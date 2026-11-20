// 场景 610 —— F1 消回合成功族二批（950 / 789 / 1036，moves_lib/f_break_round2.cpp）
//（2026-09-29，worktree ../agents/f1）
//
// 家族门槛 = break_round_effects 返回 SUCCESS（真的清掉了东西）才执行子句。
// 造"对手有回合类效果"：开战后直调 seal_skill 给**对手名下**挂一条回合型封属票
//（470/330 同款注入——47 免弱票是 IMMUNE 纯查询类，不计入 has_round_type，不可作断料）。
//
// 断言组（三效果各"主路径+门槛对照"）：
//   T1 950（鬼影千重阵 19626，切片=[2] 2回合MISS）：己方先手消除 SUCCESS →
//      R1 对手攻击 MISS（"{0}回合内"含当回合，立即写）→ R2 仍 MISS（ROUND_START
//      tick 补写）→ R3 窗口过期对手命中；对照：对手无票 → 消除失败 → R1 即命中。
//      ⚠️ 对手技能不设必中（must_hit 绕过 hit_rate_mod——内核命中链必中早退）。
//   T2 789（光辉之羽 17619，切片=[1] 下1回合翻倍）：消除 SUCCESS → R2 己方攻击
//      伤害 ≈ R2:R3 = 2 倍（NextRounds 本回合不算；红伤 85%~100% 浮动 → 比例断言
//      [1.7, 2.35]）；对照：无票 → R2:R3 比例落正常区间 [0.8, 1.25]。
//   T3 1036（极品乐淘淘 30320，切片=[1,300] 下1次攻击+300固伤）：消除 SUCCESS →
//      R2 己方攻击 = 300 固伤 + 红伤 1~2（主伤钳小：atk=1）→ R3 只剩红伤（计次
//      耗尽）；属性技不消费（R2 用属性 filler → 0 伤 0 消费，R3 固伤照落）；
//      对照：无票 → R2 无固伤。
//
// 随机性口径：三效果均无概率（纯确定）；红伤浮动用上下界/比例断言（坑表口径）。
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kZhuangJi = 10001;   // 撞击：无效果 priority 0
constexpr int kGuiying = 19626;    // 鬼影千重阵：暗影特攻 [950,101]|[2,50]（2回合MISS+50%吸血）
constexpr int kGuanghui = 17619;   // 光辉之羽：光飞行特攻 [789]|[1]（下1回合受伤翻倍；固有先制+3）
constexpr int kLettao = 30320;     // 极品乐淘淘：物攻 [1036]|[1,300]（下1次攻击+300固伤）
constexpr int kFillerAttr = 22638; // 虚空之遗：属性技 filler（190 窗对无提升方 no-op，164 同款）

ElfPet self_pet(std::array<int, 5> skills, int speed = 300, int atk = 1, int hp = 5000) {
    ElfPet p = make_pet(1, "己方", skills, speed, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;     // 己方必中：消除类效果必须命中才进 SKILL_EFFECT 桶
        s.accuracy = 100;
    }
    return p;
}

// ⚠️ 对手技能**不设必中**：must_hit 绕过 hit_rate_mod（内核命中链必中早退），
//    会把 950 的 MISS 断言打穿。accuracy=100 保持其它路径干净。
ElfPet foe_pet(std::array<int, 5> skills, int speed = 100, int atk = 1, int hp = 100000) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
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
//（470 同款注入；封属性技能不影响全物理技能的测试动作；3 回合窗口覆盖全部断言）。
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
    // T1a 950：先手消除成功 → R1/R2 对手攻击 MISS（R1 含当回合、R2 窗口 tick）、
    //        R3 窗口过期恢复命中
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kGuiying, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                             /*speed=*/300, /*atk=*/1)),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                            /*speed=*/100, /*atk=*/200)));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_foe_round_ticket(b);
        play_round(b, 0, 0);   // R1：己方先手鬼影千重阵消除 SUCCESS；对手攻击应 MISS
        CHECK(ctx.getPet(0).hp >= 5000,
              "T1a 核心：消除当回合对手攻击 MISS（{0}回合内含当回合；101 吸血只加不减）");
        play_round(b, 1, 1);   // R2：窗口 tick 补写 → 对手攻击仍 MISS
        CHECK(ctx.getPet(0).hp >= 5000,
              "T1a 核心：窗口第 2 回合对手攻击仍 MISS（ROUND_START tick 重写）");
        play_round(b, 2, 2);   // R3：窗口过期（2 回合已满）→ 对手命中
        CHECK(ctx.getPet(0).hp < 5000,
              "T1a 核心：窗口过期 → 对手攻击恢复命中（atk=200 打出实际伤害）");
        std::cout << "  [T1a] 950 消除成功→MISS 窗口（含当回合+tick+过期）通过\n";
    }
    // ════════════════════════════════════════════════════════════════
    // T1b 950 对照：对手无票 → 消除失败 → 不 MISS
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kGuiying, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                             /*speed=*/300, /*atk=*/1)),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                            /*speed=*/100, /*atk=*/200)));
        b.start();
        play_round(b, 0, 0);
        CHECK(b.ctx().getPet(0).hp < 5000,
              "T1b 对照：无回合类效果可消 → 子句不触发 → 对手攻击命中");
        std::cout << "  [T1b] 950 门槛对照 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // T2a 789：消除成功 → R2 己方攻击伤害翻倍（NextRounds，R3 恢复正常）
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kGuanghui, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                             /*speed=*/300, /*atk=*/100)),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                            /*speed=*/100, /*atk=*/10)));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_foe_round_ticket(b);
        play_round(b, 0, 0);   // R1：光辉之羽消除 SUCCESS（NextRounds 窗口=R2）
        const int hp_after_r1 = ctx.getPet(1).hp;
        play_round(b, 1, 1);   // R2：己方撞击 → 翻倍 d2
        const int d2 = hp_after_r1 - ctx.getPet(1).hp;
        const int hp_after_r2 = ctx.getPet(1).hp;
        play_round(b, 1, 1);   // R3：窗口过期 → 正常 d3
        const int d3 = hp_after_r2 - ctx.getPet(1).hp;
        CHECK(d3 > 0, "T2a 前置：R3 正常伤害为正（atk=100 vs def=100）");
        // d2 = 2·u2·base、d3 = u3·base（u∈[0.85,1]）→ 比例 ∈ [1.7, 2.3529]
        CHECK(d2 * 20 >= 34 * d3 && d2 * 17 <= 40 * d3,
              "T2a 核心：下1回合伤害翻倍（d2:d3 ≈ 2，红伤浮动用比例界）");
        std::cout << "  [T2a] 789 消除成功→下回合受伤翻倍 通过\n";
    }
    // ════════════════════════════════════════════════════════════════
    // T2b 789 对照：无票 → 消除失败 → R2:R3 正常比例
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kGuanghui, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                             /*speed=*/300, /*atk=*/100)),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                            /*speed=*/100, /*atk=*/10)));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, 0, 0);
        const int hp_after_r1 = ctx.getPet(1).hp;
        play_round(b, 1, 1);
        const int d2 = hp_after_r1 - ctx.getPet(1).hp;
        const int hp_after_r2 = ctx.getPet(1).hp;
        play_round(b, 1, 1);
        const int d3 = hp_after_r2 - ctx.getPet(1).hp;
        CHECK(d3 > 0, "T2b 前置：R3 正常伤害为正");
        // 比例 u2/u3 ∈ [0.85, 1.176] → 界 [0.8, 1.25]
        CHECK(d2 * 100 >= 80 * d3 && d2 * 100 <= 125 * d3,
              "T2b 对照：消除失败 → 无翻倍（R2:R3 落正常浮动区间）");
        std::cout << "  [T2b] 789 门槛对照 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // T3a 1036：消除成功 → R2 己方攻击 +300 固伤、R3 计次耗尽只剩红伤
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kLettao, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                             /*speed=*/300, /*atk=*/1)),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                            /*speed=*/100, /*atk=*/1)));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_foe_round_ticket(b);
        play_round(b, 0, 0);   // R1：极品乐淘淘消除 SUCCESS（计数武装=1）
        const int hp_after_r1 = ctx.getPet(1).hp;
        play_round(b, 1, 1);   // R2：己方撞击 → 300 固伤 + 红伤 1~2（atk=1 主伤钳小）
        const int loss2 = hp_after_r1 - ctx.getPet(1).hp;
        CHECK(loss2 >= 301 && loss2 <= 302,
              "T3a 核心：下1次攻击附加300点固定伤害（红伤 1~2 浮动）");
        const int hp_after_r2 = ctx.getPet(1).hp;
        play_round(b, 1, 1);   // R3：计数耗尽 → 只剩红伤 1~2
        const int loss3 = hp_after_r2 - ctx.getPet(1).hp;
        CHECK(loss3 >= 1 && loss3 <= 2,
              "T3a 核心：计次型——用掉 1 次后不再附加");
        std::cout << "  [T3a] 1036 消除成功→下1次攻击+300固伤 通过\n";
    }
    // ════════════════════════════════════════════════════════════════
    // T3b 1036 对照：无票 → 消除失败 → 不武装固伤
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kLettao, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                             /*speed=*/300, /*atk=*/1)),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                            /*speed=*/100, /*atk=*/1)));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, 0, 0);
        const int hp_after_r1 = ctx.getPet(1).hp;
        play_round(b, 1, 1);
        const int loss2 = hp_after_r1 - ctx.getPet(1).hp;
        CHECK(loss2 >= 1 && loss2 <= 2,
              "T3b 对照：无回合类效果可消 → 无固伤附加（只剩红伤 1~2）");
        std::cout << "  [T3b] 1036 门槛对照 通过\n";
    }
    // ════════════════════════════════════════════════════════════════
    // T3c 1036 攻击限定：属性技能命中不消费计次
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kLettao, kFillerAttr, kZhuangJi, kZhuangJi, kZhuangJi},
                                             /*speed=*/300, /*atk=*/1)),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                            /*speed=*/100, /*atk=*/1)));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_foe_round_ticket(b);
        play_round(b, 0, 0);   // R1：消除 SUCCESS（计数武装=1）
        const int hp_after_r1 = ctx.getPet(1).hp;
        play_round(b, 1, 1);   // R2：属性技（虚空之遗）→ 无红伤、不消费计次
        CHECK_EQ(hp_after_r1 - ctx.getPet(1).hp, 0,
                 "T3c 前置：属性技能不造成伤害、不附加固伤");
        play_round(b, 2, 2);   // R3：撞击 → 固伤仍在（未被属性技消耗）
        const int loss3 = hp_after_r1 - ctx.getPet(1).hp;
        CHECK(loss3 >= 301 && loss3 <= 302,
              "T3c 核心：属性技能命中不消费计次（300 固伤留给下次攻击）");
        std::cout << "  [T3c] 1036 属性技不消费 通过\n";
    }

    std::cout << "scenario_610 PASS (F1 消回合成功族二批 950/789/1036)" << std::endl;
    return 0;
}
