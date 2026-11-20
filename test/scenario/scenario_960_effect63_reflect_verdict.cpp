// 场景 960 —— effect 63 反馈语义裁定回归（2026-10-01 人工裁定，交接档"裁定批量落档"§3）
//（worktree=hub main-local；载体：相位光波 10859 [63]，沿用场景 150 T4 的成功路径）
//
// 裁定：反馈 = 两步（①解除自身下降；②同等级镜像附加给对手），整体成功 = 两步都成功。
//   · 自身下降被锁（STAT_DROP_LOCK，1684 通道）→ 整体不触发、自身不清；
//   · 对手免弱（STAT_DROP）→ 整体不触发、自身不清（原子性——不再"清了自己却镜像失败"）；
//   · 对手某槽已 -6（stat_drop 返 AT_FLOOR）→ 仍算成功，独立结果信号区分。
// 结果信号（F9 审计槽惯例）：plugin_storage[63000063+owner] = 1 成功（有落地）/
//   2 成功但全 AT_FLOOR / 3 失败。供"反馈成功则…"类链消费（crawford 1876 同源语义）。
//
// 断言组：
//   T1 成功主路径：自身攻-2/速-1 → 反馈 → 对手攻-2 速-1、自身清零、outcome=1。
//   T2 免弱原子失败：对手挂 STAT_DROP 票 → 反馈 → 自身下降保留（原子）、对手不动、
//      outcome=3（修复点：旧实现会清自己但镜像失败）。
//   T3 自身被锁：自身挂 STAT_DROP_LOCK → 反馈 → 整体不触发、outcome=3。
//   T4 全 AT_FLOOR：对手攻先压到 -6 → 反馈 → 对手保持 -6、自身清零、outcome=2
//      （-6 无免弱也算成功，独立信号）。
//   T5 无下降对照：不触发、不写 outcome。
#include <array>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kXiangWei = 10859;   // 相位光波：[63]（纯反馈载体）
constexpr int kPaDa = 10004;       // 拍打 filler
constexpr int kOutcomeKey = 63000063;   // +owner（镜像 batch_b_skills.cpp 审计槽）

ElfPet self_pet(const std::array<int, 5>& skills, int speed, int atk, int hp) {
    ElfPet p = make_pet(1, "己方", skills, speed, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet(const std::array<int, 5>& skills, int speed, int atk, int hp) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

void play_round_foe_none(ScenarioBattle& b, int self_slot) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, self_slot);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

int outcome_of(BattleContext& ctx, int owner) {
    const auto it = ctx.plugin_storage.find(kOutcomeKey + owner);
    return it == ctx.plugin_storage.end() ? 0 : std::any_cast<int>(it->second);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ════════════════════════════════════════════════════════════════
    // T1 成功主路径 + outcome=1
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kXiangWei, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/300, /*atk=*/1, /*hp=*/5000)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*speed=*/100, /*atk=*/1, /*hp=*/100000)));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(stat_drop(&ctx, 0, /*攻击=*/0, 2, 1) == StatDropResult::SUCCESS);
        REQUIRE(stat_drop(&ctx, 0, /*速度=*/4, 1, 1) == StatDropResult::SUCCESS);
        play_round_foe_none(b, 0);
        CHECK_EQ(ctx.ability_level(1, 0), -2, "T1 反馈：对手攻击 -2");
        CHECK_EQ(ctx.ability_level(1, 4), -1, "T1 反馈：对手速度 -1");
        CHECK_EQ(ctx.ability_level(0, 0), 0, "T1 自身攻击下降解除");
        CHECK_EQ(outcome_of(ctx, 0), 1, "T1 结果信号 = 1（成功，有实际落地）");
        std::cout << "  [T1] 63 成功主路径 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // T2 免弱原子失败：自身不清（修复点——旧实现清自己但镜像失败）
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kXiangWei, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/300, /*atk=*/1, /*hp=*/5000)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*speed=*/100, /*atk=*/1, /*hp=*/100000)));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(stat_drop(&ctx, 0, 0, 2, 1) == StatDropResult::SUCCESS);
        ctx.grant_immunity(/*owner=*/1, ImmunityType::STAT_DROP, coverage_all());
        play_round_foe_none(b, 0);
        CHECK_EQ(ctx.ability_level(0, 0), -2,
                 "T2 核心：对手免弱 → 整体不触发，自身下降保留（原子性）");
        CHECK_EQ(ctx.ability_level(1, 0), 0, "T2 对照：对手未被降（免弱）");
        CHECK_EQ(outcome_of(ctx, 0), 3, "T2 结果信号 = 3（失败）");
        std::cout << "  [T2] 63 免弱原子失败 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // T3 自身被锁（STAT_DROP_LOCK）→ 整体不触发
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kXiangWei, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/300, /*atk=*/1, /*hp=*/5000)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*speed=*/100, /*atk=*/1, /*hp=*/100000)));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(stat_drop(&ctx, 0, 0, 2, 1) == StatDropResult::SUCCESS);
        ctx.grant_immunity(/*owner=*/0, ImmunityType::STAT_DROP_LOCK, coverage_all());
        play_round_foe_none(b, 0);
        CHECK_EQ(ctx.ability_level(0, 0), -2, "T3 核心：自身下降被锁 → 不可解除、保留");
        CHECK_EQ(ctx.ability_level(1, 0), 0, "T3 对照：无可反馈 → 对手不动");
        CHECK_EQ(outcome_of(ctx, 0), 3, "T3 结果信号 = 3（失败）");
        std::cout << "  [T3] 63 自身被锁不触发 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // T4 全 AT_FLOOR：仍算成功、独立信号 = 2
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kXiangWei, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/300, /*atk=*/1, /*hp=*/5000)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*speed=*/100, /*atk=*/1, /*hp=*/100000)));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(stat_drop(&ctx, 0, 0, 2, 1) == StatDropResult::SUCCESS);
        // 对手攻先压到 -6（穿透版避开免弱；无免弱时普通 stat_drop 亦可，取省事）
        REQUIRE(stat_drop_piercing(&ctx, 1, 0, 6) == StatDropResult::SUCCESS);
        play_round_foe_none(b, 0);
        CHECK_EQ(ctx.ability_level(1, 0), -6,
                 "T4 核心：对手已 -6 → AT_FLOOR 不再下降（非免疫，仍算成功）");
        CHECK_EQ(ctx.ability_level(0, 0), 0, "T4 核心：自身下降解除（AT_FLOOR 不阻塞成功）");
        CHECK_EQ(outcome_of(ctx, 0), 2, "T4 结果信号 = 2（成功但全 AT_FLOOR，未实际下降）");
        std::cout << "  [T4] 63 全 AT_FLOOR 独立信号 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // T5 无下降对照：不触发、不写 outcome
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kXiangWei, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/300, /*atk=*/1, /*hp=*/5000)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*speed=*/100, /*atk=*/1, /*hp=*/100000)));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round_foe_none(b, 0);
        CHECK_EQ(outcome_of(ctx, 0), 0, "T5 对照：无下降可反馈 → 不触发、不写结果信号");
        std::cout << "  [T5] 63 无下降对照 通过\n";
    }

    std::cout << "scenario_960 PASS (effect 63 反馈语义裁定回归)" << std::endl;
    return 0;
}
