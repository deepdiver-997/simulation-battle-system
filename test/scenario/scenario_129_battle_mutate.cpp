// 场景 129：调试手术原语（battle_mutate，对战调试台二期，2026-09-26）——引擎通用机制。
//
// 机制（include/fsm/battle_mutate.h）：白名单的"运行时状态插入"。客户端不裸写
// context，发具名操作由 battle_mutate 走引擎存储落地（钳制/幂等/合法性校验）。
// 服务端只在 FSM 泊车时执行并持 run_mutex（Room::debug_mutate），这里白盒测原语本体。
//
// 验证点：
//   T1 set_hp：钳到 [0,maxHp]，note 带前后值
//   T2 set_pp：钳到 [0,maxPP]
//   T3 set_level：钳到 [-6,6]
//   T4 anomaly：直写 end_round（=round+rounds）、绕对抗；越界 id/rounds 拒绝
//   T5 cure：单条/全解；解没有的异常不算错
//   T6 mark：幂等（同目标二次挂不上=失败）、目标已倒拒绝、unmark 清除
//   T7 越界 side/slot 全部拒绝
#include <iostream>

#include <fsm/battle_mutate.h>

#include "scenario_harness.h"

namespace {

constexpr int kA = 12;    // 波克尔
constexpr int kSkill = 10001;  // 撞击

using namespace battle_mutate;

}  // namespace

int main() {
    ElfPet a = scenario::make_pet(kA, "波克尔", {kSkill, kSkill, kSkill, kSkill, kSkill},
                                  /*speed=*/100, /*atk=*/100, /*hp=*/500);
    ElfPet b = scenario::make_pet(kA, "波克尔", {kSkill, kSkill, kSkill, kSkill, kSkill},
                                  /*speed=*/50, /*atk=*/100, /*hp=*/400);
    scenario::ScenarioBattle battle({a, a, a, a, a, a}, {b, b, b, b, b, b});
    battle.start();
    BattleContext& ctx = battle.ctx();

    // ── T1 set_hp 钳制 ──
    auto o = set_hp(ctx, 1, 0, 1234);
    CHECK(o.ok && ctx.seerRobot[1].elfPets[0].hp == 400, "set_hp clamps to maxHp");
    o = set_hp(ctx, 1, 0, -5);
    CHECK(o.ok && ctx.seerRobot[1].elfPets[0].hp == 0, "set_hp clamps to 0");
    set_hp(ctx, 1, 0, 300);
    CHECK(!set_hp(ctx, 1, 9, 1).ok, "set_hp rejects bad slot");

    // ── T2 set_pp 钳制 ──
    const int max_pp = ctx.seerRobot[0].elfPets[0].skills[0].maxPP;
    o = set_pp(ctx, 0, 0, 0, max_pp + 7);
    CHECK(o.ok && ctx.seerRobot[0].elfPets[0].skills[0].pp == max_pp, "set_pp clamps to maxPP");
    CHECK(!set_pp(ctx, 0, 0, 5, 1).ok, "set_pp rejects skill index 5");

    // ── T3 set_level 钳制 ──
    o = set_level(ctx, 0, 4, 9);
    CHECK(o.ok && ctx.ability_levels[0][4] == 6, "set_level clamps to +6");
    o = set_level(ctx, 1, 0, -9);
    CHECK(o.ok && ctx.ability_levels[1][0] == -6, "set_level clamps to -6");

    // ── T4 anomaly 直写 ──
    o = apply_anomaly(ctx, 1, 7, 3);
    CHECK(o.ok && ctx.abnormal_status_end_round[1][7] == ctx.roundCount + 3,
          "anomaly 7 written for 3 rounds");
    CHECK(!apply_anomaly(ctx, 1, 7, 0).ok, "anomaly rejects rounds<1");
    CHECK(!apply_anomaly(ctx, 1, 99, 1).ok, "anomaly rejects out-of-range id");

    // ── T5 cure ──
    apply_anomaly(ctx, 1, 3, 2);
    o = cure_anomaly(ctx, 1, 7);
    CHECK(o.ok && ctx.abnormal_status_end_round[1][7] <= ctx.roundCount, "cure 7");
    o = cure_anomaly(ctx, 1, -1);
    CHECK(o.ok && ctx.abnormal_status_end_round[1][3] <= ctx.roundCount, "cure all");
    CHECK(cure_anomaly(ctx, 1, 7).ok, "curing absent anomaly is not an error");

    // ── T6 mark 幂等 / 已倒拒绝 / 清除 ──
    o = attach_mark(ctx, 0, 1);
    CHECK(o.ok && ctx.hp_consume_marks.size() == 1, "mark attached");
    CHECK(!attach_mark(ctx, 0, 1).ok, "duplicate mark rejected (idempotent)");
    set_hp(ctx, 1, 0, 0);           // 目标在场位倒下（裸状态）
    CHECK(!attach_mark(ctx, 0, 1).ok, "mark on downed target rejected");
    set_hp(ctx, 1, 0, 300);
    o = clear_marks(ctx, 1);
    CHECK(o.ok && ctx.hp_consume_marks.empty(), "clear_marks removes target marks");

    // ── T7 越界拒绝 ──
    CHECK(!set_hp(ctx, 2, 0, 1).ok, "side 2 rejected");
    CHECK(!set_level(ctx, 0, 6, 1).ok, "stat 6 rejected");

    std::cout << "scenario_129_battle_mutate: all checks passed\n";
    return 0;
}
