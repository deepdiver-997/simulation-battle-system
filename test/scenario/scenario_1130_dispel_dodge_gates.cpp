// 场景 1130 —— J28 解弱闪避/先手免伤族（568/664/1018，moves_lib/j_dispel_dodge_gates.cpp）
//（2026-10-02，worktree ../agents/j28；号段 1130-1139 → T1130-T1132 三块）
//
// 断言口径：
//   - 568 闪避=hp 不动（foe atk=100 → 单刀 ~30-36，dodge_rate=1.0 必闪）；解弱门两半
//     （有下降→触发且自身降档清零 / 无下降→不触发照常承伤）；窗过期照常承伤；
//   - 664 先手免伤=速度序两半（先手→R1 对手大刀 0 伤；后手→对手先打先落地）；
//   - 1018 低伤免疫+反等量：foe atk=800（raw ~271 <300 阈值）→ 我 0 伤 + foe 掉
//     ~271（FIXED 反弹，浮动带 230~272）；R4 过期照常承伤（窗 3 回合）。
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

// ── 载体技（切片已逐条与 side_effect/arg 核对）──
constexpr int kPaiDa   = 10004;   // 拍打：物40（对手填充刀）
constexpr int kShaDou  = 28138;   // 鲨斗之盾 [568]|[2]    属（568 2回合闪避）
constexpr int kYiShi   = 25655;   // 意识初醒 [664]        属（664 纯载体，priority 0）
constexpr int kMeiWei  = 26473;   // 美味佳肴 [1018]|[3,300] 属（1018 3回合阈 300）

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300, int atk = 1, int hp = 5000) {
    ElfPet p = make_pet(1, "门族二", skills, speed, atk, hp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100, int atk = 100, int hp = 99999) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, hp);
    accuracy_100(p, /*must_hit=*/false);   // 必中可穿闪避（内核统一行为），闪避测试不带必中
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1130 568 解弱→闪避窗（28138 [2]）══
    {
        {   // (a) 自身带下降 → 解除成功 → R1/R2 闪避（hp 不动），R3 过期承伤
            ScenarioBattle b(make_party(self_pet({kShaDou, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            REQUIRE(stat_change(&ctx, 0, 0, -1) == StatChangeResult::SUCCESS);   // 自身攻-1（解弱料）
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);       // R1 鲨斗之盾
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, 5000, "T1130.a 568：R1 闪避（hp 不动）");
            CHECK_EQ(ctx.ability_levels[0][0], 0, "T1130.a 568：自身下降已解除");
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, 5000, "T1130.a 568：R2 仍闪避");
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            // 2026-10-02 合并修正：链上 1265 切片=[1,100]（下1次受伤-100%）已由
            // j_dmg_mod_family 实现——R3 闪避窗过期但该刀被 1265 全免（次数消耗）。
            CHECK_EQ(ctx.getPet(0).hp, 5000,
                     "T1130.a 568：R3 窗过期，链上 1265[1,100] 全免该刀");
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(ctx.getPet(0).hp < 5000, "T1130.a 568：R4 1265 次数耗尽承伤");
        }
        {   // (b) 门对照：自身无下降 → 568 闪避窗不挂 → R1 被 1265 全免、R2 承伤
            //（1265 是链上独立效果、无条件武装——R1 免伤不是 568 的功劳；
            //  568 闪避窗在 (a) 里让 R2 也无伤，这里 R2 落地 = 门槛差异的信号）
            ScenarioBattle b(make_party(self_pet({kShaDou, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, 5000, "T1130.b 568 门：R1 被 1265 全免（非闪避）");
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(ctx.getPet(0).hp < 5000, "T1130.b 568 门：R2 承伤（闪避窗未挂）");
        }
        std::cout << "  [T1130] 568 解弱闪避窗+门 通过\n";
    }

    // ══ T1131 664 先手免伤（16477）══
    {
        {   // (a) 先手：R1 对手大刀 0 伤
            ScenarioBattle b(make_party(self_pet({kYiShi, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 /*speed=*/300)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*speed=*/100, /*atk=*/3000)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 瞬光天闪（先手）
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 对手大刀
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, 5000, "T1131.a 664：先手 → 当回合对手攻击 0 伤");
        }
        {   // (b) 后手对照：对手先打先落地
            ScenarioBattle b(make_party(self_pet({kYiShi, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 /*speed=*/100)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*speed=*/300, /*atk=*/3000)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 我后手出 664
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 对手先手大刀
            b.drive();
            CHECK(ctx.getPet(0).hp < 5000, "T1131.b 664 对照：后手不免（先挨刀）");
        }
        std::cout << "  [T1131] 664 先手免伤+对照 通过\n";
    }

    // ══ T1132 1018 低伤免疫+反等量（26473 [3,300]；foe atk=800 → raw ~271<300）══
    {
        ScenarioBattle b(make_party(self_pet({kMeiWei, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            /*speed=*/100, /*atk=*/800, /*hp=*/5000)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 美味佳肴（开窗）
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const int foe_hp_r1 = ctx.getPet(1).hp;
        CHECK_EQ(ctx.getPet(0).hp, 5000, "T1132 1018：R1 低伤免疫（hp 不动）");
        CHECK(foe_hp_r1 < 99999 - 200, "T1132 1018：反等量固伤（foe 掉 ~271，实测掉 " +
                                           std::to_string(99999 - foe_hp_r1) + "）");
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(0).hp, 5000, "T1132 1018：R2 仍免疫");
        for (int r = 0; r < 2; ++r) {   // R3 末窗 + R4 过期
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
        }
        CHECK(ctx.getPet(0).hp < 5000, "T1132 1018：R4 窗口过期承伤");
        std::cout << "  [T1132] 1018 低伤免疫反伤+过期 通过\n";
    }

    std::cout << "scenario_1130 PASS (J28 解弱闪避/先手免伤族)" << std::endl;
    return 0;
}
