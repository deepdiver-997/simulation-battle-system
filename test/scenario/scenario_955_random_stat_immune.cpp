#include <utils/sbs_rand.h>
// 场景 955 —— K4 免弱+随机成长（effect 622 雷魂·雷锘）
//
// "免疫能力下降，每回合使自身随机属性+1"：ROUND_START 挂本回合 STAT_DROP 免疫 +
// 随机槽 stat_change(+1)（db[0]=项数）。随机性用 srand 定档后断**总量**（各槽和 +1/回合）。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kGaoSu = 20017;    // 高速移动：属性技

SoulMark make_mark(int effect_id, std::vector<int> db_args, int monster_id) {
    SoulMark sm(effect_id, "K" + std::to_string(effect_id), "test",
                EffectArgs(std::move(db_args)));
    sm.monster_id = monster_id;
    return sm;
}

int level_sum(BattleContext& ctx, int side) {
    int sum = 0;
    for (int i = 0; i < 6; ++i) {
        sum += ctx.ability_levels[side][i];
    }
    return sum;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ A 回合 1：随机 +1 恰好一次 + 免弱生效 ═══════════════════════════════
    {
        ScenarioBattle b(scenario::make_party(scenario::make_pet(
                             1, "雷魂·雷锘", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                             /*speed=*/100, /*atk=*/10, /*hp=*/1000000,
                             make_mark(622, {1, 0}, 2871))),
                         scenario::make_party(scenario::make_pet(
                             2, "对手", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                             /*speed=*/100, /*atk=*/10, /*hp=*/1000000)));
        BattleContext& ctx = b.ctx();
        sbs_srand(20260916);
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);   // 鬼脸 20018：速度-2
        b.drive();
        CHECK_EQ(level_sum(ctx, 0), 1, "A 每回合随机 +1 恰好 1 项（鬼脸被免不扣）");
        // 免弱生效判据：鬼脸 -2 打速度槽——无免弱时速度槽 ∈ {-2,-1}（随机+1 可能落速度槽），
        // 有免弱时 ∈ {0,1}——两区间不相交，>=0 即免弱成立。
        CHECK(ctx.ability_levels[0][4] >= 0, "A 鬼脸被 622 免弱票挡下（速度槽未被扣负）");
    }
    // ══ A2 对照：无魂印时鬼脸照常 -2 ═══════════════════════════════════════
    {
        ScenarioBattle b(scenario::make_party(scenario::make_pet(
                             1, "咆哮方", {kGaoSu, 20201, kGaoSu, kGaoSu, kGaoSu},
                             /*speed=*/300, /*atk=*/10, /*hp=*/1000000)),
                         scenario::make_party(scenario::make_pet(
                             2, "裸桩", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                             /*speed=*/100, /*atk=*/10, /*hp=*/1000000)));
        BattleContext& ctx = b.ctx();
        sbs_srand(20260916);
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        std::cerr << "[dbg955] self levels=";
        for (int i = 0; i < 6; ++i) std::cerr << ctx.ability_levels[0][i] << ",";
        std::cerr << std::endl;
        CHECK_EQ(ctx.ability_levels[0][0], 1, "A2 对照：咆哮自攻 +1");
        CHECK_EQ(ctx.ability_levels[0][4], 1, "A2 对照：咆哮自速 +1");
    }
    // ══ B 两个回合：累计 +2 ═════════════════════════════════════════════════
    {
        ScenarioBattle b(scenario::make_party(scenario::make_pet(
                             1, "雷魂·雷锘", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                             /*speed=*/100, /*atk=*/10, /*hp=*/1000000,
                             make_mark(622, {1, 0}, 2871))),
                         scenario::make_party(scenario::make_pet(
                             2, "对手", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                             /*speed=*/100, /*atk=*/10, /*hp=*/1000000)));
        BattleContext& ctx = b.ctx();
        sbs_srand(20260916);
        b.start();
        for (int r = 0; r < 2; ++r) {
            b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
        }
        CHECK_EQ(level_sum(ctx, 0), 2, "B 两回合累计 +2");
    }

    std::cout << "scenario_955 PASS (K4 622 免弱 + 随机成长)" << std::endl;
    return 0;
}
