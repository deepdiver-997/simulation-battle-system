// 场景 1410 —— J55 必暴窗族（3 宏，moves_lib/j_must_crit_windows.cpp）
//（2026-10-03，worktree ../agents/j55；号段 1410-1419 → T1410-T1412 三块）
//
// 断言口径：
//   - 160 造 miss = 对手技能 accuracy 压 1（1270 同款，无可闪避修正）；必暴观察
//     = crit_happened 旗（make_pet 默认 crit_rate=0，"旗为 true"= 真必暴，不靠
//     伤害倍率猜，1030 口径）；
//   - 652 门槛 = 对手预置提升（stat_change）→ 载体命中后对手等级清零 + 下回合
//     必暴；对照 = 对手无提升（clear 返回 0，门槛不成立）→ 无必暴；
//   - 702 = 自身预置半血 + set_crit_sixteenths(16)：R1 挂窗（当回合不回——
//     R1 末仍半血）、R2 暴击 → 精确回满 1000；对照 = crit_rate 0 → 不回。
//   - 载体全纯链（16183 [160]|[2] / 16172 [652]|[2] / 28040 [702]|[2]）——
//     无兄弟链激活风险（速查 §五并行批次防线）。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa   = 10004;  // 拍打：物40，crit_rate=0（探针攻击）
constexpr int kShengJia = 16183; // 圣甲幻瞳术 [160]|[2]：特160（miss 武装载体）
constexpr int kKaLi    = 16172;  // EX咖喱棒 [652]|[2]：物50（消强→必暴载体）
constexpr int kPaoZhi  = 28040;  // 抛掷蓄力 [702]|[2]：属（暴击回满载体）
constexpr int kMaxHp   = 1000;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300) {
    ElfPet p = make_pet(1, "己方", skills, speed, /*atk=*/1, kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100) {
    ElfPet p = make_pet(2, "对手", skills, speed, /*atk=*/1, 99999);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1410 160 对手 MISS → 下回合必暴（圣甲幻瞳术 [160]|[2]）══
    {
        // a) R1 对手攻击 miss → R2 我方 crit_rate=0 仍必暴（旗置位）
        {
            ScenarioBattle b(
                make_party(self_pet({kShengJia, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            for (Skills& s : ctx.getPet(1).skills) {
                s.accuracy = 1;   // 制造 miss（1270 同款）
            }
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 圣甲幻瞳术（开窗）
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 对手攻击（miss）
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, kMaxHp,
                     "T1410.a 前置：对手 miss 未伤我");
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 拍打
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(ctx.crit_happened[0],
                  "T1410.a 160：R1 对手 miss → R2 必暴（crit_rate=0 下旗仍置位）");
        }
        // b) 对照：R1 对手命中 → R2 不暴（旗 false）
        {
            ScenarioBattle b(
                make_party(self_pet({kShengJia, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 命中
            b.drive();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(!ctx.crit_happened[0],
                  "T1410.b 160 对照：对手命中不武装 → R2 无必暴");
        }
        std::cout << "  [T1410] 160 miss→必暴 通过\n";
    }

    // ══ T1411 652 消强成功 → 下{0}回合必暴（EX咖喱棒 [652]|[2]）══
    {
        // a) 对手有提升：R1 消除成功（等级清零）→ R2 必暴
        {
            ScenarioBattle b(
                make_party(self_pet({kKaLi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)stat_change(&ctx, 1, 0, 2);   // 对手攻击 +2（可被消除的提升）
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // EX咖喱棒
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK_EQ(ctx.ability_levels[1][0], 0,
                     "T1411.a 652 前置：对手提升被消除");
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 拍打
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(ctx.crit_happened[0],
                  "T1411.a 652：消除成功 → R2 必暴（crit_rate=0 下旗仍置位）");
        }
        // b) 对照：对手无提升（消除失败）→ 无必暴窗
        {
            ScenarioBattle b(
                make_party(self_pet({kKaLi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(!ctx.crit_happened[0],
                  "T1411.b 652 对照：无提升可消 → 门槛不成立 → 无必暴");
        }
        std::cout << "  [T1411] 652 消强→必暴 通过\n";
    }

    // ══ T1412 702 下{0}回合暴击 → 回满（抛掷蓄力 [702]|[2]）══
    {
        // a) 半血挂窗：R1 末仍半血（"下"回合起算）→ R2 暴击精确回满 1000
        {
            ScenarioBattle b(
                make_party(self_pet({kPaoZhi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            set_crit_sixteenths(ctx.getPet(0), 16);   // 之后我方攻击必暴
            ctx.getPet(0).hp = kMaxHp / 2;            // 预置半血缺口
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 抛掷蓄力（挂窗）
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);           // 对手不动，隔离血量
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, kMaxHp / 2,
                     "T1412.a 702：R1（挂窗当回合）不回血——'下{0}回合'起算");
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 拍打（必暴）
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(ctx.crit_happened[0], "T1412.a 前置：R2 打出致命一击");
            CHECK_EQ(ctx.getPet(0).hp, kMaxHp,
                     "T1412.a 702：暴击后回满（实测 " +
                         std::to_string(ctx.getPet(0).hp) + "）");
        }
        // b) 对照：窗内攻击未暴（crit_rate=0）→ 不回
        {
            ScenarioBattle b(
                make_party(self_pet({kPaoZhi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = kMaxHp / 2;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 拍打（不暴）
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(!ctx.crit_happened[0], "T1412.b 前置：R2 未暴");
            CHECK_EQ(ctx.getPet(0).hp, kMaxHp / 2,
                     "T1412.b 702 对照：未暴不回（实测 " +
                         std::to_string(ctx.getPet(0).hp) + "）");
        }
        std::cout << "  [T1412] 702 暴击回满 通过\n";
    }

    std::cout << "scenario_1410 PASS (J55 必暴窗族 160/652/702)" << std::endl;
    return 0;
}
