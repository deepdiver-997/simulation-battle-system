#include <utils/sbs_rand.h>
// 场景 2860 —— K9 effect 293：对手处于 X 异常 → 攻击附加 N 点固伤
//
// 载体：九霄雷影 2611 行 args[0,100,150]（麻痹门/100%/150 点）——选麻痹是刻意的：
// 麻痹/睡眠都不在"行动开始异常扣血"六条表（中毒/烧伤/冻伤/寄生/流血/混乱），
// 探针零 tick 噪声（烧伤/冻伤行各 tick 100，会污染对齐断言——首跑实证）。
//
// ⚠️ 记档（睡眠行 2697 [8,100,100] 不实现）：内核 FIRST_ON_SKILL_HIT「命中即醒」
// （实测：受击方睡眠 end_round 在命中时点清零，先于 ATTACK_DAMAGE）——攻击永远先
// 打醒对手，"对手处于睡眠状态"条件在本引擎时点序下恒假，与官方"打醒"口径一致，
// 该行无行为可断言，跳过。
//
// 断言口径（srand 对齐 rand 序 → 整数精确；pct=100 在 roll_percent 短路不耗 rand；
// 293 走 DETECT 相位同步查询——TAKE_DAMAGE watcher 交付推迟到 AFTER_ACTION，实测
// 届时查询面已过状态有效期，见 feat/k9 实现头注）：
//   A 对手麻痹 → 红伤 +150；B 对手睡眠（命中即醒→DETECT 时门已假，且无 tick）→ 纯红伤。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kPaiDa = 10004;

SoulMark make_mark(int effect_id, std::vector<int> db_args, int monster_id) {
    SoulMark sm(effect_id, "K" + std::to_string(effect_id), "test",
                EffectArgs(std::move(db_args)));
    sm.monster_id = monster_id;
    return sm;
}

ElfPet holder_pet() {
    ElfPet p = scenario::make_pet(1, "持有方", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                  /*speed=*/300, /*atk=*/5000, /*hp=*/100000,
                                  make_mark(293, {0, 100, 150}, 2611));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}
ElfPet dummy_pet() {
    return scenario::make_pet(2, "木桩", {10005, 10005, 10005, 10005, 10005},
                              /*speed=*/50, /*atk=*/10, /*hp=*/1000000);
}

int probe_loss(int anomaly_id) {
    ScenarioBattle b(scenario::make_party(holder_pet()),
                     scenario::make_party(dummy_pet()));
    BattleContext& ctx = b.ctx();
    ctx.seed_rng(20261004);
    b.start();
    if (anomaly_id >= 0) {
        apply_anomaly(&ctx, /*target=*/1, anomaly_id, /*duration_rounds=*/5, /*actor=*/0);
        CHECK(ctx.has_active_abnormal_status(1, anomaly_id), "探针前置：异常施加成功");
    }
    const int hp0 = ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
    return hp0 - ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    const int base = probe_loss(-1);
    CHECK(base > 1000, "基线红伤量级合理（atk=5000 vs def=100）");
    CHECK_EQ(probe_loss(0), base + 150, "A 对手麻痹 → 红伤 +150 固伤");
    CHECK_EQ(probe_loss(8), base, "B 对手睡眠（命中即醒、门已假）→ 无附加");

    std::cout << "scenario_2860 PASS (K9 effect293 异常门附加固伤)" << std::endl;
    return 0;
}
