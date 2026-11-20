#include <utils/sbs_rand.h>
// 场景 2942 —— K14 effect 2442（鲨鱼辣椒 4826 + 超级 4830）
//
// 断言口径：
//   A1 红伤 -25%（对手无流血；对照=无印记裸红伤；-100 为回合末无流血吸取）。
//   A2 类计数翻倍：debug_step 停泊在 OLD_ROUND_END_1（红伤与吸取已结算、rc 未进位），
//     依次打固定粉（第二类 -25%）、百分比粉（满 2 类 → 第三类翻倍 -50%）。
//   B 流血门：对手有流血 → 红减关（损失 = 裸红伤 - 吸取600）；控制类免疫（诅咒 23 被拒）。
//   C 4830 登场：对手满体 → 3 段流血。
//   D 4830 回合末吸取：100 + 流血余 3 回合×100 = 400，等量回血。
//   ⚠️ 4830_enter 在 init 按对手满体自动挂流血 → 各探针开局统一 dispel 后自控门态。
#include <cstdlib>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kPaiDa = 10004;
constexpr int kBleedId = static_cast<int>(AbnormalStatusId::Bleed);

SoulMark make_mark(int effect_id, std::vector<int> db_args, int monster_id) {
    SoulMark sm(effect_id, "K" + std::to_string(effect_id), "test",
                EffectArgs(std::move(db_args)));
    sm.monster_id = monster_id;
    return sm;
}

ElfPet holder_pet(int monster_id) {
    return scenario::make_pet(1, "持有方", {10005, 10005, 10005, 10005, 10005},
                              /*speed=*/100, /*atk=*/10, /*hp=*/1000000,
                              make_mark(2442, {0, 25, 0, 0, 1}, monster_id));
}
ElfPet plain_holder_pet() {
    return scenario::make_pet(1, "持有方", {10005, 10005, 10005, 10005, 10005},
                              /*speed=*/100, /*atk=*/10, /*hp=*/1000000);
}
ElfPet attacker_pet() {
    ElfPet p = scenario::make_pet(2, "攻方", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                  /*speed=*/300, /*atk=*/5000, /*hp=*/1000000);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// 红伤探针：对手门态可控（先 dispel 再按需挂流血）。
int probe_red_loss(bool with_mark, int monster_id, bool opp_bleeding) {
    ScenarioBattle b(scenario::make_party(
                         with_mark ? holder_pet(monster_id) : plain_holder_pet()),
                     scenario::make_party(attacker_pet()));
    BattleContext& ctx = b.ctx();
    sbs_srand(20261004);
    b.start();
    dispel_active_anomalies(&ctx, 1);
    if (opp_bleeding) {
        apply_anomaly(&ctx, 1, kBleedId, 5, 0);
    }
    const int hp0 = ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp;
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    return hp0 - ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // A1 红伤 -25%（吸取 100，无流血）
    {
        const int raw = probe_red_loss(false, 0, false);
        CHECK(raw > 1000, "裸红伤基线量级合理");
        const int marked = probe_red_loss(true, 4826, false);
        CHECK_EQ(marked, raw * 75 / 100 - 100,
                 "A1 无流血 → 红伤 -25% + 回合末吸取 100");
    }
    // A2 类计数翻倍
    {
        ScenarioBattle b(scenario::make_party(holder_pet(4826)),
                         scenario::make_party(attacker_pet()));
        BattleContext& ctx = b.ctx();
        sbs_srand(20261004);
        b.start();
        dispel_active_anomalies(&ctx, 1);
        ctx.debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        ctx.debug_step_mode = false;
        b.step_past(State::BATTLE_OLD_ROUND_END_1);   // 红伤+吸取已结算，rc 未进位
        const int h1 = ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp;
        deal_pink_damage(&ctx, 0, 200, DamageKind::FIXED, 1);   // 第二类 -25%
        const int h2 = ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp;
        deal_pink_damage(&ctx, 0, 200, DamageKind::PERCENT_VALUE, 1); // 第三类 → 满 2 类翻倍
        const int h3 = ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp;
        b.drive();
        CHECK_EQ(h1 - h2, 150, "A2a 第二类（固定粉）-25%（200→150）");
        CHECK_EQ(h2 - h3, 100, "A2b 满 2 类后第三类翻倍（200→100）");
    }
    // B 流血门 + 控制免疫
    {
        const int raw = probe_red_loss(false, 0, true);
        const int marked = probe_red_loss(true, 4826, true);
        CHECK_EQ(marked, raw - 600,
                 "B1 对手有流血 → 红减关；回合末吸取 100+流血余5回合×500");
        ScenarioBattle b(scenario::make_party(holder_pet(4826)),
                         scenario::make_party(attacker_pet()));
        BattleContext& ctx = b.ctx();
        sbs_srand(20261004);
        b.start();
        apply_anomaly(&ctx, 1, kBleedId, 5, 0);
        ctx.debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        ctx.debug_step_mode = false;
        b.step_past(State::BATTLE_ROUND_START);   // 免疫票在本回合内（duration 1）
        apply_anomaly(&ctx, 0, /*诅咒(控制类)=*/23, 3, 1);
        CHECK(!ctx.has_active_abnormal_status(0, 23), "B2 对手有流血 → 控制类免疫（诅咒被拒）");
        b.drive();
    }
    // C 4830 登场流血段
    {
        ScenarioBattle b(scenario::make_party(holder_pet(4830)),
                         scenario::make_party(attacker_pet()));
        BattleContext& ctx = b.ctx();
        sbs_srand(20261004);
        b.start();
        CHECK(ctx.has_active_abnormal_status(1, kBleedId), "C 对手满体 → 3 段流血挂上");
    }
    // D 4830 回合末吸取（流血 3 回合 → 100+300=400；对手自身还有流血 tick）
    {
        // 先量 tick：无印记局手动挂同款流血（3 回合），双方 NONE
        int tick = 0;
        {
            ScenarioBattle t(scenario::make_party(plain_holder_pet()),
                             scenario::make_party(attacker_pet()));
            BattleContext& tctx = t.ctx();
            sbs_srand(20261004);
            t.start();
            apply_anomaly(&tctx, 1, kBleedId, 3, 0);
            const int hp0 = tctx.seerRobot[1].elfPets[tctx.on_stage[1]].hp;
            t.submit_operation(0, BattleFsm::ActionType::NONE, -1);
            t.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            t.drive();
            tick = hp0 - tctx.seerRobot[1].elfPets[tctx.on_stage[1]].hp;
        }
        CHECK(tick > 0, "流血 tick 量测为正");
        ScenarioBattle b(scenario::make_party(holder_pet(4830)),
                         scenario::make_party(attacker_pet()));
        BattleContext& ctx = b.ctx();
        sbs_srand(20261004);
        b.start();
        const int opp_hp0 = ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp;
        ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp = 999000;   // 留出回血空间（满血 heal 封顶）
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        const int opp_loss = opp_hp0 - ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp;
        const int self_gain = ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp - 999000;
        CHECK_EQ(opp_loss, 400 + tick, "D 回合末吸取 400 + 流血 tick");
        CHECK_EQ(self_gain, 400, "D2 等量回血（tick 不回吸）");
    }

    std::cout << "scenario_2942 PASS (K14 effect2442 家族)" << std::endl;
    return 0;
}
