#include <utils/sbs_rand.h>
// 场景 280：1152 自身不处于能力提升状态则造成的伤害提升{0}%
//         ／ 1386 当回合击败对手则回合结束时损失{0}层自然祝福
// （2026-09-28，night-3 / D12 圣光莫妮卡 3711；实现 resources/moves_lib/d5_D12.cpp）
//
// 载体：王·再见桃花源 32519（真身技能，[760,842,1152,1386]/[50,75,1]）。
// T1 无提升：1152 激活（damage_add_pct=75，通用加法通道）；842（正条件）不触发；
//     对手存活 → 1386 不扣层。
// T2 有提升（本体注入 攻+2）：1152 休眠、842 激活（AMP_EXTRA 槽=50）——两支互斥完备。
// T3 击败扣层：预置 3 层自然祝福 + 对手 hp=1 → R1 击杀 → 回合结束时 3→2。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/state.h>

#include "../../resources/monika_runtime.h"

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kHit = 10004;      // 拍打（物攻 40）
constexpr int kTaoHuaYuan = 32519;  // 王·再见桃花源：[760,842,1152,1386] [50,75,1]

ElfPet monika() {
    ElfPet p = scenario::make_pet(3711, "圣光莫妮卡", {kTaoHuaYuan, kHit, kHit, kHit, kHit},
                                  /*speed=*/200, /*atk=*/20, 3000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = scenario::make_pet(1, "守方", {kHit, kHit, kHit, kHit, kHit},
                                  /*speed=*/100, /*atk=*/20, 100000);
    for (Skills& s : p.skills) {
        s.accuracy = 100;
    }
    return p;
}

int nz_of(BattleContext& ctx) {
    return moni_pub_get(ctx.plugin_storage, kMoniPubNz + 0);
}

void one_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
    b.drive();
}

}  // namespace

int main() {
    sbs_srand(20260928);
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== T1 无提升：1152 激活（通用增伤+75）、842 休眠、1386 不扣层 ==========
    {
        ScenarioBattle b(scenario::make_party(monika()),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.plugin_storage[kMoniPubNz + 0] = 3;   // 预置 3 层自然祝福
        one_round(b);
        CHECK_EQ(static_cast<int>(ctx.ws.damage_add_pct[0]), 75,
                 "T1 无提升：1152 通用增伤 +75%");
        CHECK_EQ(ctx.ws.damage_add_extra_mul[0][0], 0,
                 "T1 无提升：842 乘法通道不给（互斥）");
        CHECK_EQ(nz_of(ctx), 3, "T1 未击败：1386 不扣层");
        std::cout << "  [T1] 无提升 → 1152 激活、1386 不扣 通过\n";
    }

    //========== T2 有提升：1152 休眠、842 激活（互补完备） ==========
    {
        ScenarioBattle b(scenario::make_party(monika()),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[0][0] = 2;   // 注入攻击+2（处于能力提升状态）
        one_round(b);
        CHECK_EQ(static_cast<int>(ctx.ws.damage_add_pct[0]), 0,
                 "T2 有提升：1152 不给");
        CHECK_EQ(ctx.ws.damage_add_extra_mul[0][0], 50,
                 "T2 有提升：842 威力+50% 走乘法通道");
        std::cout << "  [T2] 有提升 → 1152/842 互补 通过\n";
    }

    //========== T3 击败对手：回合结束时自然祝福 3→2 ==========
    {
        ScenarioBattle b(scenario::make_party(monika()),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.plugin_storage[kMoniPubNz + 0] = 3;
        ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp = 1;   // 下一击必杀
        one_round(b);
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);   // 守方替补
        CHECK_EQ(nz_of(ctx), 2, "T3 击败对手：回合结束时损失 1 层（3→2）");
        std::cout << "  [T3] 击败 → 回合末扣 1 层 通过（余 " << nz_of(ctx) << "）\n";
    }

    std::cout << "scenario_280 all passed" << std::endl;
    return 0;
}
