#include <utils/sbs_rand.h>
// 场景 330：极战·阿尔斯兰 4327 第五技「王·天决神威 35653」缺失效果 1734、840
//（moves_lib/d5_D17.cpp，夜跑 night-2，2026-09-28）
//
// 1734：有回合类效果 → 消除 + 100% 害怕（{1}=6）；无回合类效果 → 增伤 +100%。
// 840：击败对手 → 对方下只出战精灵登场时，己方在场吸取其最大体力 1/3。
//
// 断言组：
//   TA 1734 增伤分支：对手无回合类 → 掉血 = 2×裸伤（+100%），无害怕。
//   TB 1734 消除分支：对手持有回合型封属票（自挂）→ 票被清 + 害怕落地 + 不增伤。
//   TC 840：击败对手（对方死亡位等补位）→ 对方下只出战时己方在场吸取其 1/3
//      最大体力（3333），自身等量回复（驱动流程照抄 170 场景的死亡补位段）。
//
// 确定性：35653/拍打 crit_rate=0、必中；害怕施加 100% 短路必发、
// random_anomaly_duration 常量不耗 rand；840 吸体量固定 9999/3。
#include <array>
#include <cstdlib>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kTianJue = 35653;  // 王·天决神威：战斗物攻 160、必中、effect [760,1734,933,840]
constexpr int kPaDa = 10004;     // 拍打：物攻 40、无效果
constexpr int kDrain = 3333;     // 9999/3

ElfPet arslan(int speed, int atk = 150) {
    ElfPet p = scenario::make_pet(4327, "极战·阿尔斯兰", {kTianJue, kPaDa, kPaDa, kPaDa, kPaDa},
                                  speed, atk, /*hp=*/9999);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe(int speed, int atk = 10, int hp = 9999) {
    ElfPet p = scenario::make_pet(1, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                  speed, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

void drive_round(ScenarioBattle& b, int slot0, int slot1) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, slot0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, slot1);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== TA 1734 增伤分支：对手无回合类效果 → +100% ==========
    {
        sbs_srand(20260928);
        ScenarioBattle b(scenario::make_party(arslan(/*speed=*/200)),
                         scenario::make_party(foe(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b, 0, 0);
        const int raw = ctx.ws.raw_attack_damage[0];
        const int loss = 9999 - ctx.getPet(1).hp;
        CHECK(raw > 0, "TA 前提：攻击伤害落地");
        CHECK(loss >= 2 * raw - 1 && loss <= 2 * raw + 1,
              "TA 核心：对手无回合类 → 增伤 +100%（掉血=2×裸伤）");
        CHECK(!ctx.has_any_active_abnormal_status(1), "TA：无消除目标 → 不施加害怕");
        std::cout << "  [TA] 增伤分支 通过（裸伤 " << raw << " 掉血 " << loss << "）\n";
    }

    //========== TB 1734 消除分支：对手持有回合类票 → 清票 + 害怕、不增伤 ==========
    {
        sbs_srand(20260928);
        ScenarioBattle b(scenario::make_party(arslan(/*speed=*/200)),
                         scenario::make_party(foe(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        // 对手自挂一条回合型封属票（对手名下的"回合类效果"）
        seal_skill(&ctx, /*source=*/1, /*target=*/0, /*effect_id=*/70005,
                   /*attribute=*/true, /*attack=*/false, /*count=*/0,
                   /*duration_rounds=*/3, /*penetrable=*/true, /*source_slot=*/-1,
                   EffectScope::ON_STAGE, /*hit_invalid=*/false, /*chance_pct=*/100,
                   /*consumed_when_pierced=*/false);
        CHECK(ctx.rule_center_.has_round_type(1), "TB 前提：对手持有回合类效果");
        drive_round(b, 0, 0);
        CHECK(!ctx.rule_center_.has_round_type(1), "TB 核心：消除成功（回合类票被清）");
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Fear)),
              "TB 核心：消除成功 → 100% 害怕落地");
        const int raw = ctx.ws.raw_attack_damage[0];
        const int loss = 9999 - ctx.getPet(1).hp;
        CHECK(loss == raw, "TB：消除分支不增伤（掉血=裸伤）");
        std::cout << "  [TB] 消除 + 害怕 + 不增伤 通过\n";
    }

    //========== TC 840：击败对手 → 对方下只出战时己方在场吸取其 1/3 ==========
    {
        sbs_srand(20260928);
        const ElfPet a = arslan(/*speed=*/200, /*atk=*/3000);
        std::array<ElfPet, 6> team{a, a, a, a, a, a};
        const ElfPet weak = foe(/*speed=*/100, /*atk=*/10, /*hp=*/1);     // 一击即倒
        const ElfPet next = foe(/*speed=*/100, /*atk=*/10, /*hp=*/9000);  // 补位大血（吸取=3000）
        std::array<ElfPet, 6> foes{weak, next, next, next, next, next};
        ScenarioBattle b(team, foes);
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 5000;   // 预留吸血空间（+3333 不触顶）
        drive_round(b, 0, 0);      // R1：天决神威一击击败 weak（探针置 arm）
        REQUIRE(ctx.currentState == State::CHOOSE_AFTER_DEATH);   // 对方倒地等补位
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // 拍打：避免高攻再灭补位宠
        b.drive();
        CHECK_EQ(ctx.on_stage[1], 1, "TC 前置：对方下只精灵已出战");
        const int drain_next = 9000 / 3;   // 吸取量按新宠最大体力 1/3
        CHECK_EQ(ctx.getPet(1).hp, 9000 - drain_next,
                 "TC 核心：新宠登场即被吸取其最大体力 1/3（3000）");
        CHECK_EQ(ctx.getPet(0).hp, 5000 + drain_next, "TC 核心：己方在场等量回复 3000");
        std::cout << "  [TC] 击败 → 对方下只登场吸取 通过（吸 " << kDrain << "）\n";
    }

    std::cout << "scenario_330 all passed" << std::endl;
    return 0;
}
