#include <utils/sbs_rand.h>
// 场景 952 —— K3 免疫+反弹异常窗 & 受伤双防（effect 386）
//
// 2800 帝龙圣尊 args[2,1]：①首次上场 2 回合免疫并反弹异常（每场一次，喵史 4872 同款
// grant_immunity(ANOMALY, Modern) + grant_reflect）；②每次受到伤害双防+1。
// 2913 玛努诺 args[0,1]：无异常窗（db[0]=0），只有双防子句。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kYanFan = 10037;   // 燕返：威力 60 必中
constexpr int kGaoSu = 20017;    // 高速移动：属性技
constexpr int kPoison = static_cast<int>(AbnormalStatusId::Poison);

SoulMark make_mark(int effect_id, std::vector<int> db_args, int monster_id) {
    SoulMark sm(effect_id, "K" + std::to_string(effect_id), "test",
                EffectArgs(std::move(db_args)));
    sm.monster_id = monster_id;
    return sm;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ A 帝龙窗内：被施加中毒 → 免疫 + 反弹给施加方 ════════════════════════
    {
        ScenarioBattle b(scenario::make_party(scenario::make_pet(
                             1, "帝龙圣尊", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                             /*speed=*/100, /*atk=*/10, /*hp=*/1000000,
                             make_mark(386, {2, 1}, 2800))),
                         scenario::make_party(scenario::make_pet(
                             2, "投毒方", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                             /*speed=*/300, /*atk=*/5000, /*hp=*/1000000)));
        BattleContext& ctx = b.ctx();
        sbs_srand(20260916);
        b.start();
        apply_anomaly(&ctx, 0, kPoison, /*duration_rounds=*/2, /*actor=*/1);
        CHECK(!ctx.has_active_abnormal_status(0, kPoison), "A 帝龙免疫中毒");
        CHECK(ctx.has_active_abnormal_status(1, kPoison), "A 中毒反弹给投毒方");
    }
    // ══ B 帝龙：挨一击 → 双防各 +1 ═════════════════════════════════════════
    {
        ScenarioBattle b(scenario::make_party(scenario::make_pet(
                             1, "帝龙圣尊", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                             /*speed=*/100, /*atk=*/10, /*hp=*/1000000,
                             make_mark(386, {2, 1}, 2800))),
                         scenario::make_party(scenario::make_pet(
                             2, "进攻方", {kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                             /*speed=*/300, /*atk=*/5000, /*hp=*/1000000)));
        BattleContext& ctx = b.ctx();
        sbs_srand(20260916);
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.ability_levels[0][2], 1, "B 防御槽 +1");
        CHECK_EQ(ctx.ability_levels[0][3], 1, "B 特防槽 +1");
    }
    // ══ C 帝龙：挨两击 → 双防各 +2（"每次"）════════════════════════════════
    {
        ScenarioBattle b(scenario::make_party(scenario::make_pet(
                             1, "帝龙圣尊", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                             /*speed=*/100, /*atk=*/10, /*hp=*/1000000,
                             make_mark(386, {2, 1}, 2800))),
                         scenario::make_party(scenario::make_pet(
                             2, "进攻方", {kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                             /*speed=*/300, /*atk=*/5000, /*hp=*/1000000)));
        BattleContext& ctx = b.ctx();
        sbs_srand(20260916);
        b.start();
        for (int r = 0; r < 2; ++r) {
            b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
        }
        CHECK_EQ(ctx.ability_levels[0][2], 2, "C 两击 → 防御 +2");
        CHECK_EQ(ctx.ability_levels[0][3], 2, "C 两击 → 特防 +2");
    }
    // ══ D 玛努诺 db[0,1]：无异常窗 → 中毒照常落地、不反弹 ══════════════════
    {
        ScenarioBattle b(scenario::make_party(scenario::make_pet(
                             1, "玛努诺", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                             /*speed=*/100, /*atk=*/10, /*hp=*/1000000,
                             make_mark(386, {0, 1}, 2913))),
                         scenario::make_party(scenario::make_pet(
                             2, "投毒方", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                             /*speed=*/300, /*atk=*/5000, /*hp=*/1000000)));
        BattleContext& ctx = b.ctx();
        sbs_srand(20260916);
        b.start();
        apply_anomaly(&ctx, 0, kPoison, /*duration_rounds=*/2, /*actor=*/1);
        CHECK(ctx.has_active_abnormal_status(0, kPoison), "D 玛努诺无窗 → 中毒落地");
        CHECK(!ctx.has_active_abnormal_status(1, kPoison), "D 不反弹");
    }

    std::cout << "scenario_952 PASS (K3 386 异常免疫反弹窗 + 受伤双防)" << std::endl;
    return 0;
}
