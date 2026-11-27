#include <utils/sbs_rand.h>
// 场景 3060 —— K172 回合开始消强+害怕/固伤分支族（808，soul_lib/k172_round_start_clear_fear.cpp）
//（2026-10-07，worktree ../agents/k172；号段 3060-3069 → T3060-T3063 四块）
//
// 断言口径（NONE/NONE 出手隔离 ROUND_START，场面上只有 808 在动）：
//   - 恐惧掷点 sbs_srand 钉种子分岔：落怕 seed → 害怕落地、无固伤；失手 seed →
//     无害怕、fixed_damage 300/350 落地（NONE 对局无红伤污染 → hp 差账精确）。
//   - 消强无条件：过门即清正槽（对照槽 -1 不动——只消"提升"不碰"下降"）。
//   - 门闭对照：对手无提升 → 整句不触发（不清槽、不掷骰、不掉血）。
#include <array>
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaDa = 10004;   // 拍打：占位技能（对局用 NONE，不实际出招）

template <int MarkId>
ElfPet mark_pet(const std::array<int, 5>& skills, int chance, int fixed) {
    ElfPet p = make_pet(1, "印主", skills, /*speed=*/100, /*atk=*/1, /*hp=*/99999,
                        /*soul_mark=*/SoulMark(MarkId, "K172 魂印", "",
                                               EffectArgs(std::vector<int>{chance,
                                                                           fixed, 1})));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_pet(const std::array<int, 5>& skills) {
    ElfPet p = make_pet(2, "对手", skills, /*speed=*/100, /*atk=*/1, /*hp=*/99999);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

void both_none(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    const std::array<int, 5> skills{kPaDa, kPaDa, kPaDa, kPaDa, kPaDa};

    // ══ T3060 [50,300,1] 掷点落怕：消强 + 害怕落地 + 无固伤 ══
    {
        ScenarioBattle b(make_party(mark_pet<808>(skills, 50, 300)),
                         make_party(plain_pet(skills)));
        b.start();
        b.ctx().ability_levels[1][0] = 2;
        b.ctx().seed_rng(3);
        both_none(b);
        BattleContext& ctx = b.ctx();
        CHECK_EQ(ctx.ability_levels[1][0], 0, "T3060 808：过门即消强（正槽清零）");
        CHECK(ctx.has_any_active_abnormal_status(1), "T3060 808：害怕落地");
        CHECK_EQ(ctx.getPet(1).hp, 99999, "T3060 808：落怕不补固伤（hp 无损）");
        std::cout << "  [T3060] 808 落怕分支 通过\n";
    }

    // ══ T3061 [50,300,1] 掷点失手：消强 + 无害怕 + 300 固伤补偿 ══
    {
        ScenarioBattle b(make_party(mark_pet<808>(skills, 50, 300)),
                         make_party(plain_pet(skills)));
        b.start();
        b.ctx().ability_levels[1][0] = 2;
        b.ctx().seed_rng(8);
        both_none(b);
        BattleContext& ctx = b.ctx();
        CHECK_EQ(ctx.ability_levels[1][0], 0, "T3061 808：失手分支消强照常");
        CHECK(!ctx.has_any_active_abnormal_status(1), "T3061 808：未触发害怕");
        CHECK_EQ(ctx.getPet(1).hp, 99999 - 300, "T3061 808：未触发补偿 300 固伤");
        std::cout << "  [T3061] 808 补偿固伤分支 通过\n";
    }

    // ══ T3062 门闭对照：对手无提升（负槽不算）→ 整句不触发 ══
    {
        ScenarioBattle b(make_party(mark_pet<808>(skills, 50, 300)),
                         make_party(plain_pet(skills)));
        b.start();
        b.ctx().ability_levels[1][0] = -1;   // 下降不是提升：门闭（顺验消强不碰负槽）
        b.ctx().seed_rng(8);
        both_none(b);
        BattleContext& ctx = b.ctx();
        CHECK_EQ(ctx.ability_levels[1][0], -1, "T3062 808：门闭不清槽（负槽原样）");
        CHECK(!ctx.has_any_active_abnormal_status(1), "T3062 808：门闭不掷害怕");
        CHECK_EQ(ctx.getPet(1).hp, 99999, "T3062 808：门闭无固伤");
        std::cout << "  [T3062] 808 门闭对照 通过\n";
    }

    // ══ T3063 进化行 [60,350,1]：补偿档 350（行参透传互锚）══
    {
        ScenarioBattle b(make_party(mark_pet<808>(skills, 60, 350)),
                         make_party(plain_pet(skills)));
        b.start();
        b.ctx().ability_levels[1][0] = 2;
        b.ctx().seed_rng(9);
        both_none(b);
        BattleContext& ctx = b.ctx();
        CHECK_EQ(ctx.ability_levels[1][0], 0, "T3063 808 进化行：消强照常");
        CHECK(!ctx.has_any_active_abnormal_status(1), "T3063 808 进化行：未触发害怕");
        CHECK_EQ(ctx.getPet(1).hp, 99999 - 350, "T3063 808 进化行：补偿 350 固伤");
        std::cout << "  [T3063] 808 进化行互锚 通过\n";
    }

    std::cout << "scenario_3060 PASS (K172 回合开始消强+害怕/固伤分支族)" << std::endl;
    return 0;
}
