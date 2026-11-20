// 场景 710 —— G1 消回合成功族三批（990 / 1179 / 1083，moves_lib/g_break_round3.cpp）
//（2026-09-29，worktree ../agents/g1；场景段 710-719 一文件覆盖三效果）
//
// 家族门槛 = break_round_effects 返回 SUCCESS（真的清掉了东西）才执行子句。
// 造"对手有回合类效果"：开战后直调 seal_skill 给**对手名下**挂一条回合型封属票
//（610/470 同款注入——47 免弱票是 IMMUNE 纯查询类，不计入 has_round_type，不可作断料）。
//
// 断言组（三效果各"主路径+门槛对照"）：
//   T1 990（鸣世斩天诀 19756，切片=[300] 即发固伤）：己方先手消除 SUCCESS →
//      R1 对手掉血 = 红伤（atk=1、威力160 基线 3）+ 固伤 300（即发版，F1 1036 计次版的更简同构）→
//      R2 只剩红伤（一次性附加、无窗口）；对照：对手无票 → R1 只剩红伤（[2,5] 界）。
//   T2 1179（王·擎天之光 30809，切片=[2,100] 下2次攻击额外+100%）：消除 SUCCESS →
//      R2/R3 己方攻击伤害 ≈ R4 的 2 倍（计次在**伤害管线 AMP_EXTRA 内**消费、乘算；
//      "下2次"不含当次——R1 自身伤害不吃）→ R4 计次耗尽恢复；对照：无票 →
//      R2:R4 比例落正常区间 [0.8, 1.25]。⚠️ 红伤 85%~100% 浮动 → 比例断言（坑表口径）。
//   T3 1083（恐惧黑洞 32193，切片=[1083] 纯消除）：**结构断言** has_round_type——
//      己方慢速后手 → R1 消除触发 → 对手回合类效果消失；对照：己方快速先手 →
//      不触发 → 对手回合类效果仍在。
//      ⚠️ 场景载体选 32193（[1083] 单效果、PP 正常）：板载名件 痴愚 13739 / 谄诳 13740
//      moves.max_pp=0（pp=0 不可选，skills.cpp query_selectable）且切片含 5 个未实现
//      伴生效果——28 个载体切片全为无参一致，换载体不损失覆盖（g_break_round3.cpp 记档）。
//
// 随机性口径：三效果均无概率；红伤浮动用上下界/比例断言。对手技能不设必中保持
// 命中链干净（610 同款——本批无 MISS 断言，保持同构以防干扰）。
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kZhuangJi = 10001;   // 撞击：无效果 priority 0
constexpr int kMingshi = 19756;    // 鸣世斩天诀：物攻160 [990,300]|[991,1,100]（991 未实现 no-op）
constexpr int kQingtian = 30809;   // 王·擎天之光：特攻160 [760]|[1179,2,100]|[812,6]（760 不微弱中性局 no-op、812 未实现）
constexpr int kKongju = 32193;     // 恐惧黑洞：特攻130 [1083]（单效果纯消除）
constexpr int kFillerAttr = 22638; // 虚空之遗：属性技 filler（610 同款备用）

ElfPet self_pet(std::array<int, 5> skills, int speed = 300, int atk = 1, int hp = 5000) {
    ElfPet p = make_pet(1, "己方", skills, speed, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;     // 己方必中：消除类效果必须命中才进 SKILL_EFFECT 桶
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100, int atk = 1, int hp = 100000) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;     // 不设必中：保持命中链干净（610 同款）
        s.accuracy = 100;
    }
    return p;
}

void play_round(ScenarioBattle& b, int self_slot, int foe_slot) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, self_slot);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_slot);
    b.drive();
}

// 开战后给**对手名下**（source=1）挂一条回合型封属票——己方可断的"回合类效果"
//（610 同款注入；封属性技能不影响全攻击技能的测试动作；3 回合窗口覆盖全部断言）。
void grant_foe_round_ticket(ScenarioBattle& b) {
    seal_skill(&b.ctx(), /*source=*/1, /*target=*/0, /*effect_id=*/70610,
               /*attribute=*/true, /*attack=*/false, /*count=*/0,
               /*duration_rounds=*/3, /*penetrable=*/true, /*source_slot=*/-1,
               EffectScope::ON_STAGE, /*hit_invalid=*/false, /*chance_pct=*/100,
               /*consumed_when_pierced=*/false);
    CHECK(b.ctx().rule_center_.has_round_type(1), "前提：对手持有回合类效果");
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ════════════════════════════════════════════════════════════════
    // T1a 990：先手消除成功 → R1 红伤 1~2 + 固伤 300；R2 计时点已过、只剩红伤
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kMingshi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                             /*speed=*/300, /*atk=*/1)),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                            /*speed=*/100, /*atk=*/1)));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_foe_round_ticket(b);
        play_round(b, 0, 0);   // R1：鸣世斩天诀消除 SUCCESS → 即发 300 固伤
        const int loss1 = 100000 - ctx.getPet(1).hp;
        CHECK(loss1 >= 302 && loss1 <= 305,
              "T1a 核心：消除成功附加300点固定伤害（红伤 atk=1 → 1~2 浮动）");
        play_round(b, 1, 1);   // R2：撞击 → 990 是即发一次性，无残留
        const int hp_after_r1 = ctx.getPet(1).hp;
        play_round(b, 1, 1);   // R3：再撞一回合
        const int loss3 = hp_after_r1 - ctx.getPet(1).hp;
        CHECK(loss3 >= 1 && loss3 <= 2,
              "T1a 核心：固伤只在消除当次附加（无窗口残留，红伤 1~2）");
        std::cout << "  [T1a] 990 消除成功→即发300固伤 通过\n";
    }
    // ════════════════════════════════════════════════════════════════
    // T1b 990 对照：对手无票 → 消除失败 → 无固伤
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kMingshi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                             /*speed=*/300, /*atk=*/1)),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                            /*speed=*/100, /*atk=*/1)));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, 0, 0);
        const int loss1 = 100000 - ctx.getPet(1).hp;
        CHECK(loss1 >= 2 && loss1 <= 5,
              "T1b 对照：无回合类效果可消 → 不附加固伤（只剩红伤 1~2）");
        std::cout << "  [T1b] 990 门槛对照 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // T2a 1179：先手消除成功 → R2/R3 己方攻击 ≈ 2 倍（下2次、不含当次）、R4 耗尽
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kQingtian, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                             /*speed=*/300, /*atk=*/100)),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                            /*speed=*/100, /*atk=*/1)));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_foe_round_ticket(b);
        play_round(b, 0, 0);   // R1：擎天之光消除 SUCCESS、武装 2 次（自身本次伤害不吃）
        const int hp_after_r1 = ctx.getPet(1).hp;
        play_round(b, 1, 1);   // R2：撞击 → 计次 1（+100%）
        const int hp_after_r2 = ctx.getPet(1).hp;
        play_round(b, 1, 1);   // R3：撞击 → 计次 2（+100%，用尽）
        const int hp_after_r3 = ctx.getPet(1).hp;
        play_round(b, 1, 1);   // R4：撞击 → 计次耗尽 → 正常
        const int d2 = hp_after_r1 - hp_after_r2;
        const int d3 = hp_after_r2 - hp_after_r3;
        const int d4 = hp_after_r3 - ctx.getPet(1).hp;
        CHECK(d4 > 0, "T2a 前置：R4 正常伤害为正（atk=100 vs def=100）");
        // d2 = 2·u2·base、d4 = u4·base（u∈[0.85,1]）→ 比例 ∈ [1.7, 2.3529]
        CHECK(d2 * 20 >= 34 * d4 && d2 * 17 <= 40 * d4,
              "T2a 核心：下2次攻击额外+100%（第1次，d2:d4 ≈ 2，比例界）");
        CHECK(d3 * 20 >= 34 * d4 && d3 * 17 <= 40 * d4,
              "T2a 核心：下2次攻击额外+100%（第2次，d3:d4 ≈ 2，比例界）");
        std::cout << "  [T2a] 1179 消除成功→下2次攻击额外+100% 通过\n";
    }
    // ════════════════════════════════════════════════════════════════
    // T2b 1179 对照：无票 → 消除失败 → R2:R4 正常比例（无武装）
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kQingtian, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                             /*speed=*/300, /*atk=*/100)),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                            /*speed=*/100, /*atk=*/1)));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, 0, 0);   // R1
        const int hp_after_r1 = ctx.getPet(1).hp;
        play_round(b, 1, 1);   // R2
        const int hp_after_r2 = ctx.getPet(1).hp;
        play_round(b, 1, 1);   // R3
        const int hp_after_r3 = ctx.getPet(1).hp;
        play_round(b, 1, 1);   // R4
        const int d2 = hp_after_r1 - hp_after_r2;
        const int d4 = hp_after_r3 - ctx.getPet(1).hp;
        CHECK(d4 > 0, "T2b 前置：R4 正常伤害为正");
        // 比例 u2/u4 ∈ [0.85, 1.176] → 界 [0.8, 1.25]（610 T2b 同界）
        CHECK(d2 * 100 >= 80 * d4 && d2 * 100 <= 125 * d4,
              "T2b 对照：消除失败 → 无增伤（R2:R4 落正常浮动区间）");
        std::cout << "  [T2b] 1179 门槛对照 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // T3a 1083：己方后手 → 消除触发 → 对手回合类效果消失（结构断言）
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kKongju, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                             /*speed=*/100, /*atk=*/1)),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                            /*speed=*/300, /*atk=*/1)));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_foe_round_ticket(b);
        play_round(b, 0, 0);   // R1：己方慢速后手用恐惧黑洞 → "若后出手"成立 → 消除
        CHECK(!ctx.rule_center_.has_round_type(1),
              "T3a 核心：后出手消除对手回合类效果（票已断）");
        std::cout << "  [T3a] 1083 后手消除 通过\n";
    }
    // ════════════════════════════════════════════════════════════════
    // T3b 1083 对照：己方先手 → 不触发 → 对手回合类效果仍在
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kKongju, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                             /*speed=*/300, /*atk=*/1)),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                            /*speed=*/100, /*atk=*/1)));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_foe_round_ticket(b);
        play_round(b, 0, 0);   // R1：己方先手 → "若后出手"不成立
        CHECK(ctx.rule_center_.has_round_type(1),
              "T3b 对照：先手不触发消除（对手票仍在）");
        std::cout << "  [T3b] 1083 先手对照 通过\n";
    }

    std::cout << "scenario_710 PASS (G1 消回合成功族三批 990/1179/1083)" << std::endl;
    return 0;
}
