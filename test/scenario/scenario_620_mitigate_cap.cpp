// 场景 620 —— F2 减伤封顶族集成（766 CAP 锁伤 / 811 下次减 n / 1034 消回合免疫窗）
//（2026-09-29，worktree ../agents/f2；moves_lib/f_mitigate_cap.cpp）
//
// 机制：766 = 消强成功 → CAP 阶段锁伤窗（含 697 凭证穿透——该凭证的首个消费者）；
//       811 = 本回合自身造伤 watcher 累计 → REDUCE_FLAT 一次性扣减；
//       1034 = 消回合 SUCCESS → DAMAGE 票回合窗（794 直系同款）。
//
// 断言组：
//   T1 766（移山填海 17357，[3,200]）：R1 消强+挂窗 → R1/R2/R3 挨打恰 200（窗口两端），
//      R4 挨打 > 200（过期）；对照口径：伤害随机浮动 85%~100% 但 base ≫ 200 → 钳制精确。
//   T2 ★ 697 穿 CAP：同 T1 挂窗，R2 对手用狂龙爆裂袭 16915（[697]）→ 掉血 > 200
//      （凭证穿透，窗口期内）；R3 对手改用撞击 → 恰 200（窗口未过、穿透是凭证特异）。
//   T3 811（X·圣盾 32332，无参）同回合口径：己方先手打 n → 对手当回合反打被减
//      （drop1 < drop2，drop2=R2 未消费的完整伤）→ "下次"一次性；
//   T4 811 跨回合口径：己方后手，R1 对手先打（全额 f1）→ 己方后打（n 冻结在 R1）→
//      R2 挨打被减（f2 < f3=R3 全额）→ 冻结累计 + 一次性；
//   T5 1034（破碎铁拳冲 31285，[1]）：对手有回合票 → 消除成功（has_round_type 翻转）
//      → R1 挨打 0（窗口含当回合，794 同款口径）→ R2 挨打 > 0（过期）；
//   T6 1034 门槛对照：对手无回合票 → 消除失败 → R1 挨打 > 0（无窗）。
//
// 随机性口径：伤害 85%~100% 浮动——精确断言只用在"钳制/归零/票据"这类确定性出口上，
// 未钳路径全部用严格不等式（margin ≥ 3 倍攻差，浮动与类型倍数均吃不掉）。
// 场景坑对表：威力 0 保底伤（无威力 0 技能登场）、跨回合 watcher TEAM（实现侧 811）、
// filler 用 ActionType::NONE、先制残留（766 载体固有 priority=2，但测试无先制对照断言，
// 且后续回合己方 NONE 不参与先手比较——无污染面）。
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kZhuangJi = 10001;   // 撞击：无效果 priority 0
constexpr int kYishan = 17357;     // 移山填海：物理 85 [766]|[3,200]（priority 2 固有先制）
constexpr int kShengdun = 32332;   // X·圣盾：物理 130 [811]|[]（811 最净载体）
constexpr int kPozhui = 31285;     // 破碎铁拳冲：物理 160 [1034,132]|[1]（132 条件威力翻倍，
                                   //   本场景己方 HP 恒高于对手 → 不触发，无噪声）
constexpr int kKuanglong = 16915;  // 狂龙爆裂袭：物理 150 [697]|[]（穿透凭证载体）

ElfPet self_pet(std::array<int, 5> skills, int speed, int atk) {
    ElfPet p = make_pet(1, "己方", skills, speed, atk, /*hp=*/1000000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed, int atk) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, /*hp=*/100000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

void play_round(ScenarioBattle& b, int self_slot, int foe_slot) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, self_slot);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_slot);
    b.drive();
}

// 己方 NONE filler 回合（只让对手出手）。
void foe_only_round(ScenarioBattle& b, int foe_slot) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_slot);
    b.drive();
}

// 766 挂窗前置：对手带攻击提升（消强门槛，ability_levels+view_levels 双写）。
void grant_cap_window(ScenarioBattle& b) {
    BattleContext& ctx = b.ctx();
    ctx.ability_levels[1][0] = 2;
    ctx.ws.view_levels[1][0] = 2;
    play_round(b, 0, 0);   // R1 己方移山填海（priority 2 先手）：消强 + 挂窗
    CHECK_EQ(ctx.ability_levels[1][0], 0, "前置：对手提升被消除（门槛成立）");
}

// 1034 挂窗前置：给对手名下挂一条回合型封属票（470 同款 330 注入——IMMUNE 纯查询类
// 不计入 has_round_type，不可作断料）。
void grant_foe_round_ticket(ScenarioBattle& b) {
    seal_skill(&b.ctx(), /*source=*/1, /*target=*/0, /*effect_id=*/70471,
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
    // T1 766：消强 → 锁伤窗 [R1,R3] 恰 200，R4 过期
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kYishan, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                             /*speed=*/300, /*atk=*/1)),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                            /*speed=*/100, /*atk=*/2000)));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_cap_window(b);
        const int hp_r1 = ctx.getPet(0).hp;
        foe_only_round(b, 0);   // R2
        const int drop2 = hp_r1 - ctx.getPet(0).hp;
        CHECK_EQ(drop2, 200, "T1 核心：窗内 R2 挨打恰 200（CAP 钳 final）");
        const int hp_r2 = ctx.getPet(0).hp;
        foe_only_round(b, 0);   // R3（窗口最后一回合）
        const int drop3 = hp_r2 - ctx.getPet(0).hp;
        CHECK_EQ(drop3, 200, "T1 窗口末端：R3 仍恰 200");
        const int hp_r3 = ctx.getPet(0).hp;
        foe_only_round(b, 0);   // R4（过期）
        const int drop4 = hp_r3 - ctx.getPet(0).hp;
        CHECK(drop4 > 200, "T1 过期：R4 挨打超过 200（窗口右端）");
        std::cout << "  [T1] 766 锁伤窗 通过 (R2=" << drop2 << " R3=" << drop3
                  << " R4=" << drop4 << ")\n";
    }

    // ════════════════════════════════════════════════════════════════
    // T2 ★ 697 穿 CAP：窗内 [697] 技能穿透锁伤，普通技能仍被钳
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kYishan, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                             /*speed=*/300, /*atk=*/1)),
                         make_party(foe_pet({kKuanglong, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                            /*speed=*/100, /*atk=*/2000)));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_cap_window(b);
        const int hp_r1 = ctx.getPet(0).hp;
        foe_only_round(b, 0);   // R2：对手用 16915（[697]）
        const int drop697 = hp_r1 - ctx.getPet(0).hp;
        CHECK(drop697 > 200,
              "T2 核心：697「无视伤害限制」穿透 CAP——窗内掉血不被钳 200（死凭证转活）");
        const int hp_r2 = ctx.getPet(0).hp;
        foe_only_round(b, 1);   // R3：对手改用撞击（窗口还在 [R1,R3]）
        const int drop_normal = hp_r2 - ctx.getPet(0).hp;
        CHECK_EQ(drop_normal, 200,
                 "T2 对照：R3 窗口未过、无凭证技能仍恰 200（穿透是凭证特异而非过期）");
        std::cout << "  [T2] 697 穿 CAP 通过 (drop697=" << drop697
                  << " drop_normal=" << drop_normal << ")\n";
    }

    // ════════════════════════════════════════════════════════════════
    // T3 811 同回合：先手打 n → 对手当回合反打被减 → R2 全额（一次性）
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kShengdun, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                             /*speed=*/300, /*atk=*/2000)),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                            /*speed=*/100, /*atk=*/600)));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, 0, 0);   // R1：己方先手 X·圣盾（n=对手掉血），对手反打被减
        const int n = 100000 - ctx.getPet(1).hp;
        CHECK(n > 0, "T3 前置：己方当回合造成伤害 n>0（watcher 累计口径的输入）");
        const int hp_r1 = ctx.getPet(0).hp;
        foe_only_round(b, 0);   // R2：对手攻击（减伤已消费 → 全额）
        const int drop1 = 1000000 - hp_r1;   // R1 反打掉血
        const int drop2 = hp_r1 - ctx.getPet(0).hp;
        CHECK(drop1 < drop2,
              "T3 核心：当回合反打被减（drop1<drop2；n>0.15×base 的 3 倍攻差 margin 下"
              " 随机浮动不可逆）→ 下次一次性消费");
        CHECK(drop2 > 0, "T3 R2 全额落地");
        std::cout << "  [T3] 811 同回合 通过 (n=" << n << " drop1=" << drop1
                  << " drop2=" << drop2 << ")\n";
    }

    // ════════════════════════════════════════════════════════════════
    // T4 811 跨回合：后手 R1 冻结 n → R2 挨打被减 → R3 全额
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kShengdun, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                             /*speed=*/100, /*atk=*/2000)),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                            /*speed=*/300, /*atk=*/600)));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, 0, 0);   // R1：对手先打（全额 f1），己方后打（n 冻结在 R1）
        const int f1 = 1000000 - ctx.getPet(0).hp;
        CHECK(f1 > 0, "T4 前置：R1 挂窗前挨打全额（效果尚不存在）");
        const int hp_r1 = ctx.getPet(0).hp;
        foe_only_round(b, 0);   // R2：挨打被减
        const int f2 = hp_r1 - ctx.getPet(0).hp;
        const int hp_r2 = ctx.getPet(0).hp;
        foe_only_round(b, 0);   // R3：已消费 → 全额
        const int f3 = hp_r2 - ctx.getPet(0).hp;
        CHECK(f2 < f3, "T4 核心：R2 挨打被上回合冻结的 n 削减、R3 恢复全额"
                       "（跨回合冻结累计 + 一次性）");
        CHECK(f2 < f1, "T4 辅证：R2 低于挂窗前同面板全额样本");
        std::cout << "  [T4] 811 跨回合 通过 (f1=" << f1 << " f2=" << f2
                  << " f3=" << f3 << ")\n";
    }

    // ════════════════════════════════════════════════════════════════
    // T5 1034：消回合成功 → 免疫窗 R1（含当回合，794 同款）→ R2 过期
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kPozhui, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                             /*speed=*/300, /*atk=*/1)),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                            /*speed=*/100, /*atk=*/2000)));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_foe_round_ticket(b);
        play_round(b, 0, 0);   // R1：破碎铁拳冲消回合 + 挂免疫窗
        CHECK(!ctx.rule_center_.has_round_type(1), "T5 前置：对手回合票被消除（SUCCESS）");
        const int hp_r1 = ctx.getPet(0).hp;
        CHECK_EQ(hp_r1, 1000000, "T5 核心：窗内（含当回合）挨打 0 伤害（DAMAGE 票归零）");
        foe_only_round(b, 0);   // R2：窗口过期
        const int drop2 = hp_r1 - ctx.getPet(0).hp;
        CHECK(drop2 > 0, "T5 过期：R2 挨打落地（窗口右端）");
        std::cout << "  [T5] 1034 免疫窗 通过 (R2=" << drop2 << ")\n";
    }

    // ════════════════════════════════════════════════════════════════
    // T6 1034 门槛对照：对手无回合票 → 消除失败 → 无免疫窗
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kPozhui, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                             /*speed=*/300, /*atk=*/1)),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                            /*speed=*/100, /*atk=*/2000)));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, 0, 0);   // R1：消回合失败（对手无票）
        CHECK(!ctx.rule_center_.has_round_type(0), "前提对照 sanity：己方也无票可断");
        const int drop1 = 1000000 - ctx.getPet(0).hp;
        CHECK(drop1 > 0, "T6 核心：消除失败 → 无免疫窗 → R1 挨打落地（门槛分支）");
        std::cout << "  [T6] 1034 门槛对照 通过 (drop1=" << drop1 << ")\n";
    }

    std::cout << "scenario_620 PASS (减伤封顶族 766 CAP/811 下次减n/1034 免疫窗 + 697 穿透)"
              << std::endl;
    return 0;
}
