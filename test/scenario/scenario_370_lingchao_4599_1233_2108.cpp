#include <utils/sbs_rand.h>
// 场景 370：灵巢之主·索杰德尔 4599 第五技「往生虚冥叹 37088」缺失效果 1233、2108
//（moves_lib/d5_D21.cpp，夜跑 night-2，2026-09-28）
//
// 断言组：
//   TC 1233 递增吸取：连续 3 次使用 → 吸 20%/25%/30% 对手最大体力（9999 → 1999/2499/2999），
//      对手每回合掉血 = 裸伤 + 吸取量（等式）。
//   TD 2108 交换 + 骑乘：命起1/魂落3 → 使用后交换为 3/1；未击败 → 下一攻击技
//      威力 +40×3（裸伤对比对照局 ≥3×）且先制 +1×1（慢者先动）；池一次清空。
//   TE 2108 反例：击败对手 → 不武装（下次攻击无先制）。
//
// 确定性：37088/拍打 crit_rate=0、必中；无概率效果；伤害浮动含在裸伤台账内；
// 命起/魂落层走共享层 lingchao_layers.h（场景直写 soulmark_storage 键）。
#include <array>
#include <cstdlib>
#include <iostream>

#include <effects/lingchao_layers.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kWangSheng = 37088;  // 往生虚冥叹：电龙特攻 160、必中、effect [797,1233,2108]
constexpr int kPaDa = 10004;       // 拍打：物攻 40、无效果

// 灵巢之主（魂印 2193 载体）：make_pet 挂 SoulMark，层走共享头键。
ElfPet lingchao(int speed) {
    SoulMark sm(2193, "灵巢之主", "测试桩：灵巢之主·索杰德尔 2193");
    ElfPet p = scenario::make_pet(4599, "灵巢之主·索杰德尔", {kWangSheng, kPaDa, kPaDa, kPaDa, kPaDa},
                                  speed, /*atk=*/150, /*hp=*/9999, sm);
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

    //========== TC 1233 递增吸取：20% → 25% → 30% ==========
    {
        sbs_srand(20260928);
        ScenarioBattle b(scenario::make_party(lingchao(/*speed=*/200)),
                         scenario::make_party(foe(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        for (int r = 1; r <= 3; ++r) {
            const int before = ctx.getPet(1).hp;
            drive_round(b, 0, 0);
            const int raw = ctx.ws.raw_attack_damage[0];
            const int drain = 9999 * (15 + 5 * r) / 100;   // 20/25/30%（整数截断）
            const int loss = before - ctx.getPet(1).hp;
            CHECK(loss == raw + drain,
                  "TC 核心：第 " << r << " 次吸取 " << drain << "（裸伤+吸体等式）");
        }
        std::cout << "  [TC] 递增吸取 1999/2499/2999 通过\n";
    }

    //========== TD 2108 交换 + 骑乘：威力 +40×3、先制 +1，一次清池 ==========
    {
        sbs_srand(20260928);
        const ElfPet m = lingchao(/*speed=*/50);   // 慢者：先制骑乘效果可观察
        std::array<ElfPet, 6> team{m, m, m, m, m, m};
        const ElfPet f = foe(/*speed=*/100);
        ScenarioBattle b(team, scenario::make_party(f));
        b.start();
        BattleContext& ctx = b.ctx();
        const int slot0 = ctx.on_stage[0];
        lingchao_set_layers(&ctx, 0, slot0, /*mingqi=*/1, /*hunluo=*/3);
        drive_round(b, 0, 0);   // R1：使用往生虚冥叹 → 交换 → 命起3/魂落1；未击败 → 武装
        CHECK_EQ(lingchao_layers(&ctx, 0, ctx.on_stage[0], kLingchaoMingqiKey), 3,
                 "TD 核心：交换后命起 = 3（原魂落）");
        CHECK_EQ(lingchao_layers(&ctx, 0, ctx.on_stage[0], kLingchaoHunluoKey), 1,
                 "TD 核心：交换后魂落 = 1（原命起）");
        // 对照局：同种子、无层的拍打裸伤（R2 威力骑乘倍率基线）
        sbs_srand(20260928);
        ScenarioBattle bc(scenario::make_party(lingchao(/*speed=*/50)),
                          scenario::make_party(foe(/*speed=*/100)));
        bc.start();
        drive_round(bc, 1, 0);
        const int raw_ctrl = bc.ctx().ws.raw_attack_damage[0];
        // R2：有骑乘局的同回合拍打 → 威力 +40×3=120（威力 40→160）+ 先制 +1
        drive_round(b, 1, 0);
        const int raw_rider = ctx.ws.raw_attack_damage[0];
        CHECK(ctx.ws.preemptive_level[0] == 1, "TD 核心：先制 +1 已置位（魂落 1 层）");
        CHECK(!ctx.is_second_mover(0), "TD 核心：慢者因先制先动");
        CHECK(raw_rider >= 3 * raw_ctrl,
              "TD 核心：威力骑乘后裸伤 ≥ 3× 对照（威力 40→160 应约 4×，含浮动）");
        // R3：池已清 → 无先制
        drive_round(b, 1, 0);
        CHECK(ctx.is_second_mover(0), "TD 核心：R3 池清空后恢复后手");
        std::cout << "  [TD] 交换 + 威力/先制骑乘 通过（rider 裸伤 " << raw_rider
                  << " vs 对照 " << raw_ctrl << "）\n";
    }

    //========== TE 2108 反例：击败对手 → 不武装 ==========
    {
        sbs_srand(20260928);
        const ElfPet m = lingchao(/*speed=*/50);
        std::array<ElfPet, 6> team{m, m, m, m, m, m};
        const ElfPet weak = foe(/*speed=*/100, /*atk=*/10, /*hp=*/1);
        ScenarioBattle b(team, scenario::make_party(weak));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b, 0, 0);   // 击杀对手
        CHECK(ctx.getPet(1).hp <= 0, "TE 前提：对手已被击败");
        // R2：拍打（无骑乘）→ 无先制、正常后手
        drive_round(b, 1, 0);
        CHECK(ctx.ws.preemptive_level[0] == 0, "TE 核心：击败 → 不武装先制骑乘");
        CHECK(ctx.is_second_mover(0), "TE 核心：慢者恢复后手");
        std::cout << "  [TE] 击败不武装 通过\n";
    }

    std::cout << "scenario_370 all passed" << std::endl;
    return 0;
}
