// 场景 049：犀牛（不灭地威·萨瑞卡 1217）× 693 增伤 —— 端到端验证「检测早于增伤」。
//
// 官方魂印原文（`effect_icon` pet_id=[3637], effect_id=1217）：
//   「回合开始时令自身防御+1、特防+1、速度+1，令对手防御-1、特防-1、速度-1；
//     自身受到的攻击伤害**大于350则恢复自身所有体力**，**小于350则反弹给对手造成伤害50%的固定伤害**」
// 官方时点链（L345）：「**犀牛魂印—通用增伤—693增伤—保底伤害—护盾**（从先到后）」
// 官方现象（L348）：「把增伤时点挪动到后面…结果挪动到**犀牛的挡伤时点之后**，于是出现了
//   **693增伤乱穿犀牛**」；用户口径：「犀牛的魂印检测早于一些增伤效果，导致被打穿350限制并且没有回满」。
//
// 引擎落点：
//   · 判定值 = **链首快照** `ws.guard_detect_damage`（core 的 `install_default_damage_guard_detect`
//     在管线第一位 GUARD_DETECT 写）——**已含暴击与暴击抗性减免，未含任何增伤/减伤**。
//   · 回血/反弹挂 `EVENT_TAKE_DAMAGE`（伤害**落地之后**），不走 `_AFTER_ACTION`——那些时点的桶
//     只跑当回合 mover 一侧，犀牛作为非 mover 时不执行（谱尼虚无同款坑）。
//   · 693「造成攻击伤害**额外**提升X%」→ `AMP_EXTRA`（非通用增伤·乘法），**排在犀牛检测之后** ✓
//
// 验证点：
//   R1 裸伤 <350（无 693）→ 不回血，且**反弹**链首值的 50% 固定伤害给攻击方
//   R2 ★ 裸伤 <350 + 693×2 → 实际伤害被顶过 350，**犀牛仍不回满**（"693 乱穿犀牛"）
//   R3 裸伤 >350 → 回满自身所有体力
//   R4 ★ **暴击把裸伤顶过 350 → 照样触发回满**（检测含暴击）
//   R5 ★ **暴击抗性参与判定**：必暴 + 35% 暴击抗性 → 链首值被削到 <350 → 不回满
//   R6 ★ 暴击抗性是**乘整段**：100% 抗性 → 暴击伤害 **归 0**（不是"回到不暴击的值"），
//        0 伤既不触发回满、也不触发反弹（反弹分支要求 >0）
#include <cassert>
#include <iostream>

#include <effects/effect.h>
#include <entities/pet_factory.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

constexpr int kQuanLi = 10033;   // 全力一击：物理 power 120、无 side_effect
constexpr int kGaoSu = 20017;    // 高速移动：属性技能（不还手）
constexpr int kSarikaMarkId = 1217;
constexpr int kMaxHp = 3000;

// 攻方速度 600、守方 100 → p0 恒定先手，攻击发生在 BATTLE_FIRST_ATTACK_DAMAGE。
// （K393 校准，2026-10-26：1217 补「回合开始六项」后，R1 守方防+1（def 100→150）、
//  攻方速-1/守方速+1（600×2/3=400 vs 100×1.5=150，仍恒先手）——攻方 atk 统一 ×1.5
//  补偿防御档，R1~R6 断言带与原版（def 100 无等级）逐档对齐。）
scenario::ScenarioBattle make_battle(int atk, int sarika_crit_resist = 0, bool always_crit = false) {
    auto p0 = scenario::make_pet(0, "攻方", {kQuanLi, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                 /*speed=*/600, atk, /*hp=*/3000);
    auto p1 = scenario::make_pet(1, "不灭地威·萨瑞卡",
                                 {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                 /*speed=*/100, /*atk=*/100, kMaxHp,
                                 SoulMark(kSarikaMarkId, "不灭地威·萨瑞卡", "犀牛"));
    p0.skills[0].accuracy = 100;
    p0.skills[0].must_hit = true;
    if (always_crit) {
        scenario::set_crit_sixteenths(p0, 16);   // 必定暴击
    }
    p1.damage_resist.crit_pct = sarika_crit_resist;   // 回合开始 sync 进 ws.eff_crit_resist_pct
    return scenario::ScenarioBattle(scenario::make_party(p0), scenario::make_party(p1));
}

void call_effect(BattleContext& ctx, int id, std::vector<int> ints) {
    Effect e = EffectFactory::getInstance().getEffect(id, EffectArgs(std::move(ints)));
    CHECK(e.logic != nullptr, "effect 应已在插件中注册");
    e.logic(&ctx, e.args);
}

// 打一回合（p0 用技能 0 打 p1；p1 用属性技能不还手）。
void strike(scenario::ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
    b.drive();
}

struct Outcome {
    int sarika_hp = 0;      // 打完后的萨瑞卡体力
    int taken = 0;          // 萨瑞卡实际掉的血（回满则为 0）
    int attacker_hp = 0;    // 攻击方体力（看反弹）
};

// 跑一回合；`pct693 > 0` 时先挂 693（真插件效果，非通用增伤·乘法通道）。
Outcome run_round(scenario::ScenarioBattle& b, int pct693 = 0) {
    b.start();
    if (pct693 > 0) {
        call_effect(b.ctx(), 693, {0, 1, /*rounds=*/1, pct693});
    }
    strike(b);
    Outcome o;
    o.sarika_hp = b.ctx().getPet(1).hp;
    o.taken = kMaxHp - o.sarika_hp;
    o.attacker_hp = b.ctx().getPet(0).hp;
    return o;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== R1 裸伤 <350（无 693）→ 不回血 + 反弹 50% ==========
    // atk=375（K393 校准：def 100→150 ×1.5 补偿）→ 0.84×375/150×120+2 = 254；×浮动 → 约 216~254（稳在 350 以下）
    {
        scenario::ScenarioBattle b = make_battle(/*atk=*/375);
        const Outcome o = run_round(b);
        CHECK(o.taken > 0 && o.taken < 350, "R1) 裸伤落在 350 以下");
        CHECK_EQ(o.sarika_hp, kMaxHp - o.taken, "R1) <350 → 不回血（体力就是掉掉的样子）");
        CHECK(o.attacker_hp < 3000, "R1) <350 → 反弹「造成伤害 50%」的固定伤害给攻击方");
        std::cout << "  [R1] 裸伤=" << o.taken << "（<350）→ 不回血、攻击方被反弹掉 "
                  << (3000 - o.attacker_hp) << "\n";
    }

    //========== R2 ★ 裸伤 <350 + 693×2 → 乱穿犀牛 ==========
    {
        scenario::ScenarioBattle b = make_battle(/*atk=*/375);
        const Outcome o = run_round(b, /*pct693=*/100);
        // 链首（犀牛看到的）仍是 ~216~254 < 350 → 不回血；693 在 AMP_EXTRA 把实际伤害顶成 ×2
        CHECK(o.taken > 350, "R2) ★ 实际伤害被 693 顶过 350");
        CHECK_EQ(o.sarika_hp, kMaxHp - o.taken,
                 "R2) ★★ 犀牛**没有回满** —— 它检测早于 693，看不到被顶起来的伤害（\"693 乱穿犀牛\"）");
        std::cout << "  [R2] 裸伤<350 + 693×2 → 实际伤害=" << o.taken
                  << "（>350）但犀牛不回血 —— 乱穿 ✓\n";
    }

    //========== R3 裸伤 >350 → 回满自身所有体力 ==========
    // atk=750（×1.5 补偿）→ 0.84×750/150×120+2 = 506；×浮动 → 约 430~506（稳在 350 以上）
    {
        scenario::ScenarioBattle b = make_battle(/*atk=*/750);
        const Outcome o = run_round(b);
        CHECK_EQ(o.sarika_hp, kMaxHp, "R3) >350 → 恢复自身所有体力（回满）");
        CHECK_EQ(o.attacker_hp, 3000, "R3) 高伤分支不反弹（回满与反弹互斥）");
        std::cout << "  [R3] 裸伤>350 → 回满（体力 3000）\n";
    }

    //========== R4 ★ 暴击把裸伤顶过 350 → 照样触发 ==========
    // 同一个 atk=375（裸伤<350），必定暴击 ×2 → 链首值约 432~508 > 350 → 回满
    //（⚠️ 单回合 run_round 结构不可改成 while(!FINISHED) 步进：回满分支每回合都触发，
    //  战斗永远到不了 FINISHED——K393 接管轮实测挂死记档，2026-10-26。）
    {
        scenario::ScenarioBattle b = make_battle(/*atk=*/375, /*crit_resist=*/0, /*always_crit=*/true);
        const Outcome o = run_round(b);
        CHECK_EQ(o.sarika_hp, kMaxHp,
                 "R4) ★ 暴击后 >350 → 照样回满（**犀牛的伤害检测包含暴击**）");
        std::cout << "  [R4] 暴击把裸伤从 ~250 顶到 ~500 → 回满 ✓（检测含暴击）\n";
    }

    //========== R5 ★ 暴击抗性参与判定 ==========
    // 同样必暴（×2 → ~508），但萨瑞卡暴击抗性 35% → **乘整段** → ~508×0.65 ≈ 331 < 350
    // → 链首值掉到阈值下 → **不回满**。R4(无抗性) vs R5(35%抗性) 只差抗性，
    // 差别只可能来自暴击抗性 → "判定读的是已扣抗性的值"由此钉住。
    {
        scenario::ScenarioBattle b = make_battle(/*atk=*/375, /*crit_resist=*/35, /*always_crit=*/true);
        const Outcome o = run_round(b);
        CHECK(o.taken > 0 && o.taken < 350, "R5) 35% 暴击抗性把暴击伤害削到 350 以下");
        CHECK_EQ(o.sarika_hp, kMaxHp - o.taken,
                 "R5) ★ 不回满 —— 判定读的是**已扣暴击抗性**的值");
        std::cout << "  [R5] 必暴 + 35% 暴击抗性 → 链首值=" << o.taken
                  << "（<350）→ 不回满 ✓（暴击抗性参与判定）\n";
    }

    //========== R6 ★ 暴击抗性是**乘整段**：100% 抗性 → 0 伤害 ==========
    // 官方口径（用户 2026-09-14）：「100% 暴击抗性，最终伤害应该只会是 0」
    // —— 200×0 = 0，而不是"削掉加成、回到不暴击的 100"。
    {
        scenario::ScenarioBattle b = make_battle(/*atk=*/375, /*crit_resist=*/100, /*always_crit=*/true);
        const Outcome o = run_round(b);
        CHECK_EQ(o.taken, 0, "R6) ★ 100% 暴击抗性 → 暴击伤害归 0");
        CHECK_EQ(o.sarika_hp, kMaxHp, "R6) 0 伤既不触发回满（0 不大于 350）");
        CHECK_EQ(o.attacker_hp, 3000, "R6) 也不触发反弹（反弹分支要求链首值 >0）");
        std::cout << "  [R6] 必暴 + 100% 暴击抗性 → 伤害 0、不回满、不反弹 ✓（抗性乘整段）\n";
    }

    std::cout << "\n场景 049 通过：犀牛 1217 的判定取**链首快照**（含暴击与暴击抗性、增伤前）；"
                 "693 增伤落在其后的 AMP_EXTRA 阶段 → 顶过 350 时犀牛看不到 = \"693 乱穿犀牛\"；"
                 "回血/反弹挂 EVENT_TAKE_DAMAGE（伤害落地之后）。\n";
    return 0;
}
