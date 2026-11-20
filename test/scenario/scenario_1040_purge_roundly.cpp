// 场景 1040 —— J19 消强/吸反族三（759/978，moves_lib/j_purge_roundly.cpp）
//（2026-10-02，worktree ../agents/j19；号段 1040-1049 → T1040-T1041 两块）
//
// 断言口径：
//   - 759 累加型弱化：等级槽**精确等式**逐回合断言（-2/回合无随机）；消强门两半
//     （有提升→触发 / 无提升→不触发）；速度-2×3 回合铺满整窗（含后手顺延不双落）；
//   - 978 吸取并反转：对手正等级反转成负 + 自身获得等量正（六槽精确等式）；
//     无提升 → 反转失败无事发生（1061 同路径，978 无消除兜底——记档口径）；
//   - 我方先手（speed 300 vs 100）固定行动序。
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
constexpr int kPaiDa   = 10004;   // 拍打：物40（填充）
constexpr int kShiFen  = 35106;   // 时分重合 [759]|[3,4,-2] 物85（759：3回合速度-2）
constexpr int kLeiMing = 17977;   // 雷鸣审判 [978]        物80（978 纯载体）

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300, int atk = 1, int hp = 99999) {
    ElfPet p = make_pet(1, "消强者", skills, speed, atk, hp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100, int atk = 1, int hp = 99999) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, hp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

int level_at(BattleContext& ctx, int side, int slot) {
    return ctx.ability_levels[side][slot];
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1040 759 消强→每回合速度-2×3（35106 时分重合 [3,4,-2]）══
    {
        {   // (a) 对手有提升 → 消强成功 → R1 -2 / R2 -2 / R3 -2（累进 -6）
            ScenarioBattle b(make_party(self_pet({kShiFen, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            REQUIRE(stat_change(&ctx, 1, 0, +2) == StatChangeResult::SUCCESS);   // 对手攻+2（消强料）
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);       // R1 时分重合
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(level_at(ctx, 1, 0), 0, "T1040.a 759：R1 消强落地（攻+2 清零）");
            CHECK_EQ(level_at(ctx, 1, 4), -2, "T1040.a 759：R1 立即 -2（窗口含当回合）");
            for (int r = 2; r <= 3; ++r) {
                b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
                b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
                b.drive();
                CHECK_EQ(level_at(ctx, 1, 4), -2 * r,
                         "T1040.a 759：R" + std::to_string(r) + " tick 累进 -2");
            }
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(level_at(ctx, 1, 4), -6,
                     "T1040.a 759：R4 窗口过期（3 回合恰 -6，不双落）");
        }
        {   // (b) 对手无提升 → 消强失败 → 不触发
            ScenarioBattle b(make_party(self_pet({kShiFen, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(level_at(ctx, 1, 4), 0, "T1040.b 759 门槛：无提升消强失败不触发");
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(level_at(ctx, 1, 4), 0, "T1040.b 759 门槛：R2 仍无 tick");
        }
        std::cout << "  [T1040] 759 消强+每回合弱化窗 通过\n";
    }

    // ══ T1041 978 吸取并反转（17977 雷鸣审判）══
    {
        {   // (a) 对手 攻+2/速+1 → 反转成 攻-2/速-1，自身获得 +2/+1
            ScenarioBattle b(make_party(self_pet({kLeiMing, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            REQUIRE(stat_change(&ctx, 1, 0, +2) == StatChangeResult::SUCCESS);
            REQUIRE(stat_change(&ctx, 1, 4, +1) == StatChangeResult::SUCCESS);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 17977
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(level_at(ctx, 1, 0), -2, "T1041.a 978：对手攻 +2 反转成 -2");
            CHECK_EQ(level_at(ctx, 1, 4), -1, "T1041.a 978：对手速 +1 反转成 -1");
            CHECK_EQ(level_at(ctx, 0, 0), +2, "T1041.a 978：自身吸取 攻+2");
            CHECK_EQ(level_at(ctx, 0, 4), +1, "T1041.a 978：自身吸取 速+1");
        }
        {   // (b) 对手无提升 → 反转失败无事发生（978 无消除兜底）
            ScenarioBattle b(make_party(self_pet({kLeiMing, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            int sum_foe = 0;
            int sum_me = 0;
            for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
                sum_foe += level_at(ctx, 1, i);
                sum_me += level_at(ctx, 0, i);
            }
            CHECK_EQ(sum_foe, 0, "T1041.b 978：对手无提升 → 六槽不动");
            CHECK_EQ(sum_me, 0, "T1041.b 978：自身无吸取");
        }
        std::cout << "  [T1041] 978 吸取并反转 通过\n";
    }

    std::cout << "scenario_1040 PASS (J19 消强/吸反族三)" << std::endl;
    return 0;
}
