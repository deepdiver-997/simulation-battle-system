#include <utils/sbs_rand.h>
// 场景 951 —— K3 定额反弹（effect 328 / 1057）
//
// 328 瞳神·耶里梅斯 db[60,0]"每次受到伤害反弹 60 点固定伤害"——**任意伤害**（措辞无
// "攻击"限定，粉伤也弹）；1057 赤渊之蛇 db[250,70,1]"每次受到攻击伤害后反弹 250 点"
// ——只认攻击伤害时点（闪避/必暴子句按工单记档）。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kYanFan = 10037;   // 燕返：威力 60 必中
constexpr int kGaoSu = 20017;    // 高速移动：属性技

SoulMark make_mark(int effect_id, std::vector<int> db_args, int monster_id) {
    SoulMark sm(effect_id, "K" + std::to_string(effect_id), "test",
                EffectArgs(std::move(db_args)));
    sm.monster_id = monster_id;
    return sm;
}

// 守方（0 方）带魂印挨一击，返回攻方总损失。
int probe_attacker_loss(const SoulMark& mark) {
    ScenarioBattle b(scenario::make_party(scenario::make_pet(
                         1, "受击方", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                         /*speed=*/100, /*atk=*/10, /*hp=*/1000000, mark)),
                     scenario::make_party(scenario::make_pet(
                         2, "进攻方", {kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                         /*speed=*/300, /*atk=*/5000, /*hp=*/1000000)));
    BattleContext& ctx = b.ctx();
    sbs_srand(20260916);
    b.start();
    const int hp0 = ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp;
    const int hp1 = ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp;
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    const int def_loss = hp0 - ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp;
    CHECK(def_loss > 1000, "基线落血量级");
    return hp1 - ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ A 328 精确断言：对照局（无魂印）取落血 D，带魂印局攻方损 = D + 60 ════
    {
        const int base = probe_attacker_loss(SoulMark{});
        const int marked = probe_attacker_loss(make_mark(328, {60, 0}, 2672));
        CHECK_EQ(marked, base + 60, "A 攻方总损 = 基线落血 + 60（srand 对齐）");
    }
    // ══ B 328 粉伤也弹（"受到伤害"无攻击限定）══════════════════════════════
    {
        ScenarioBattle b(scenario::make_party(scenario::make_pet(
                             1, "受击方", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                             /*speed=*/100, /*atk=*/10, /*hp=*/1000000,
                             make_mark(328, {60, 0}, 2672))),
                         scenario::make_party(scenario::make_pet(
                             2, "进攻方", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                             /*speed=*/300, /*atk=*/5000, /*hp=*/1000000)));
        BattleContext& ctx = b.ctx();
        sbs_srand(20260916);
        b.start();
        const int hp1 = ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp;
        deal_pink_damage(&ctx, 0, 100, DamageKind::FIXED, 1);
        ctx.event_center_.drain(&ctx, ctx.roundCount);   // 事件晚派发：引擎在回合内 drain，场景显式对齐
        CHECK_EQ(hp1 - ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp, 60,
                 "B 受到粉伤 100 → 反弹 60 给进攻方");
    }
    // ══ C 1057：受到攻击伤害 → 反弹 250（闪避/必暴子句记档不测）══════════════
    {
        const int base = probe_attacker_loss(SoulMark{});
        const int marked = probe_attacker_loss(make_mark(1057, {250, 70, 1}, 3458));
        CHECK_EQ(marked, base + 250, "C 攻方总损 = 基线落血 + 250");
    }

    std::cout << "scenario_951 PASS (K3 328 任意伤害弹 / 1057 攻击伤弹)" << std::endl;
    return 0;
}
