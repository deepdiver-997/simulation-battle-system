// 场景 5400 —— J213 吸强兜底吸血族（1928，moves_lib/j_drain_boost_fallback.cpp）
//（2026-10-22，worktree feat/jbatch；号段 5400-5409）
//
// 断言口径（能力等级 + 绝对体力双面；伤害浮动不参与断言）：
//   - 1928（神州烈阳舞 36423 链 [1081,1928,795]|[3,25,100]）：{0}=3 → 吸 max_hp(对手)/3。
//   - a) 对手预置攻+2：吸取后 self slot0=+2、opp slot0=0；兜底按 1127 定版语序
//     （吸取后判"不处于提升状态"）落 → 自身 hp 2000 + 3000/3 = 3000。
//   - b) 对手无提升：无所得（self slot0=0）；兜底同样落 → hp 3000。
//   - 兄弟 1081（对手有提升则先制+1）只影响先手、795（连用增伤）只影响伤害，均不碰
//     能力等级/体力绝对值。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa   = 10004;  // 拍打：物 40（填充刀）
constexpr int kShenZhou = 36423; // [1081,1928,795]|[3,25,100]（吸 max/3）
constexpr int kSelfMax = 5000;
constexpr int kFoeMax  = 3000;
constexpr int kDrain   = kFoeMax / 3;  // 1000
constexpr int kSelfStart = 2000;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_self() {
    ElfPet p = make_pet(1, "己方", {kShenZhou, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, /*hp=*/kSelfMax);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/kFoeMax);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5400 1928 吸强 + 兜底 max/3 吸血 ══
    {
        {   // (a) 对手攻+2 → 吸走归我 + 兜底吸血
            ScenarioBattle b(make_party(mk_self()), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = kSelfStart;
            (void)stat_change(&ctx, 1, 0, 2);   // 对手攻击 +2
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.ability_levels[0][0], 2,
                     "T5400.a 1928：吸取归我（self slot0 实测 " +
                         std::to_string(ctx.ability_levels[0][0]) + "，应 2）");
            CHECK_EQ(ctx.ability_levels[1][0], 0,
                     "T5400.a 1928：对手攻等级归零（opp slot0 实测 " +
                         std::to_string(ctx.ability_levels[1][0]) + "，应 0）");
            CHECK_EQ(ctx.getPet(0).hp, kSelfStart + kDrain,
                     "T5400.a 1928：兜底吸 max/3=1000（self hp 实测 " +
                         std::to_string(ctx.getPet(0).hp) + "，应 3000）");
        }
        {   // (b) 对手无提升 → 无所得 + 兜底仍落（1127 同门语序）
            ScenarioBattle b(make_party(mk_self()), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = kSelfStart;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.ability_levels[0][0], 0,
                     "T5400.b 1928 对照：无提升不吸（self slot0 实测 " +
                         std::to_string(ctx.ability_levels[0][0]) + "，应 0）");
            CHECK_EQ(ctx.getPet(0).hp, kSelfStart + kDrain,
                     "T5400.b 1928 对照：兜底仍落（self hp 实测 " +
                         std::to_string(ctx.getPet(0).hp) + "，应 3000）");
        }
        std::cout << "  [T5400] 1928 吸强 + 兜底吸血 + 无提升对照 通过\n";
    }

    std::cout << "scenario_5400 PASS (J213 吸强兜底吸血族)" << std::endl;
    return 0;
}
