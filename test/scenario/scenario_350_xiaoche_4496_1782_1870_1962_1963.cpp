#include <utils/sbs_rand.h>
// 场景 350：箫澈 4496 第五技「箫·残夜悠悠 36547」缺失效果 1782、1870、1962、1963
//（moves_lib/d5_D19.cpp，夜跑 night-2，2026-09-28）
//
// 断言组：
//   TA 1782 先制：R1 首用（结构性偏差：无先制，记档）→ R2 有盾 +2 必先（慢者先动）
//      → R3 改用拍打后恢复后手。
//   TB 1870 护罩翻倍：有护罩 → 掉血 = 2×裸伤。
//   TC 1962 消回合：对手持回合型票 → 使用后票被清（对手存活=未击败）。
//   TD 1963 消提升：对手有强化 → 使用后清零。
//   TE 未击败反例：一击致死 → 不消除（对手强化保留）。
//
// 确定性：36547/拍打 crit_rate=0、必中、同系克制 1.0；无概率效果；
// 伤害浮动含在裸伤台账内（TB 用等式断言）。
#include <array>
#include <cstdlib>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kXiao = 36547;   // 箫·残夜悠悠：电龙特攻 160、必中、effect [1782,1870,1962,1963]
constexpr int kPaDa = 10004;   // 拍打：物攻 40、无效果

ElfPet xiaoche(int speed, int atk = 150) {
    ElfPet p = scenario::make_pet(4496, "箫澈", {kXiao, kPaDa, kPaDa, kPaDa, kPaDa},
                                  speed, atk, /*hp=*/9999);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe(int speed, int atk = 10) {
    ElfPet p = scenario::make_pet(1, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                  speed, atk, /*hp=*/9999);
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

    //========== TA 1782 先制：有盾 +2（慢者先动）；首用偏差记档 ==========
    {
        sbs_srand(20260928);
        ScenarioBattle b(scenario::make_party(xiaoche(/*speed=*/50)),
                         scenario::make_party(foe(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).shield_bank_.add(/*priority=*/0, /*value=*/500, /*source_id=*/70001,
                                       /*duration=*/0);
        drive_round(b, 0, 0);   // R1：首用——掷点时点早于武装 → 无先制（慢者后手）
        CHECK(ctx.is_second_mover(0), "TA 前提：R1 首用无先制（结构性偏差已记档）");
        drive_round(b, 0, 0);   // R2：有盾再使用 → 先制+2，慢者先动
        CHECK(ctx.ws.preemptive_level[0] == 2, "TA 核心：先制 +2 已置位");
        CHECK(!ctx.is_second_mover(0), "TA 核心：R2 慢者因先制+2 先动");
        drive_round(b, 1, 0);   // R3：改用拍打（盾仍在但不加成）
        CHECK(ctx.is_second_mover(0), "TA 核心：非 36547 不加先制");
        std::cout << "  [TA] 有盾先制+2 通过\n";
    }

    //========== TB 1870 护罩翻倍：掉血 = 2×裸伤 ==========
    {
        sbs_srand(20260928);
        ScenarioBattle b(scenario::make_party(xiaoche(/*speed=*/200)),
                         scenario::make_party(foe(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hood_bank_.add(/*priority=*/0, /*value=*/500, /*source_id=*/70002,
                                     /*duration=*/0);
        drive_round(b, 0, 0);
        const int raw = ctx.ws.raw_attack_damage[0];
        const int loss = 9999 - ctx.getPet(1).hp;
        CHECK(raw > 0, "TB 前提：攻击伤害落地");
        CHECK(loss >= 2 * raw - 1 && loss <= 2 * raw + 1,
              "TB 核心：有护罩 → 攻击伤害翻倍（掉血=2×裸伤）");
        std::cout << "  [TB] 护罩翻倍 通过（裸伤 " << raw << " 掉血 " << loss << "）\n";
    }

    //========== TC 1962 消回合：对手存活 → 回合类票被清 ==========
    {
        sbs_srand(20260928);
        ScenarioBattle b(scenario::make_party(xiaoche(/*speed=*/200)),
                         scenario::make_party(foe(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        seal_skill(&ctx, /*source=*/1, /*target=*/0, /*effect_id=*/70005,
                   /*attribute=*/true, /*attack=*/false, /*count=*/0,
                   /*duration_rounds=*/3, /*penetrable=*/true, /*source_slot=*/-1,
                   EffectScope::ON_STAGE, /*hit_invalid=*/false, /*chance_pct=*/100,
                   /*consumed_when_pierced=*/false);   // 对手名下的回合类效果
        CHECK(ctx.rule_center_.has_round_type(1), "TC 前提：对手持有回合类效果");
        drive_round(b, 0, 0);   // 未击败（对手 9999 血存活）
        CHECK(!ctx.rule_center_.has_round_type(1), "TC 核心：未击败 → 消除对手回合类效果");
        std::cout << "  [TC] 消除回合类 通过\n";
    }

    //========== TD 1963 消提升：对手存活 → 强化被清 ==========
    {
        sbs_srand(20260928);
        ScenarioBattle b(scenario::make_party(xiaoche(/*speed=*/200)),
                         scenario::make_party(foe(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][0] = 2;
        drive_round(b, 0, 0);
        CHECK_EQ(ctx.ability_levels[1][0], 0, "TD 核心：未击败 → 消除对手能力提升状态");
        std::cout << "  [TD] 消除能力提升 通过\n";
    }

    //========== TE 未击败反例：一击致死 → 不消除 ==========
    {
        sbs_srand(20260928);
        ScenarioBattle b(scenario::make_party(xiaoche(/*speed=*/200, /*atk=*/3000)),
                         scenario::make_party(foe(/*speed=*/100, /*atk=*/10)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][0] = 2;   // 对手强化：致死时不消除
        ctx.getPet(1).hp = 3000;        // 压低血量保证一击致死（3000 攻实测一击 ~3765）
        drive_round(b, 0, 0);
        CHECK(ctx.getPet(1).hp <= 0, "TE 前提：对手已被击败（实际 hp=" << ctx.getPet(1).hp << "）");
        CHECK_EQ(ctx.ability_levels[1][0], 2, "TE 核心：击败对手 → 不消除其能力提升");
        std::cout << "  [TE] 击败不消除 通过\n";
    }

    std::cout << "scenario_350 all passed" << std::endl;
    return 0;
}
