#include <utils/sbs_rand.h>
// 场景 946 —— K2 受击施加（effect 262 贝露）+ 每回合施加（effect 306 邪影之尊）
//
// 262：受到攻击（两攻击伤害时点）20% 使**对手**（=进攻方）中毒——Modern 通道全链。
// 306：每回合开始 15% 令对手害怕（ROUND_START 节点）。
// 断言口径：db 100/0 档确定性；262 无参行走 db[0] 覆盖（行=20% 只是常数不同）。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kYanFan = 10037;   // 燕返：威力 60 必中
constexpr int kGaoSu = 20017;    // 高速移动：属性技

SoulMark make_mark(int effect_id, std::vector<int> db_args, int monster_id = 0) {
    SoulMark sm(effect_id, "K" + std::to_string(effect_id), "test",
                EffectArgs(std::move(db_args)));
    sm.monster_id = monster_id;
    return sm;
}

bool run_and_check(const SoulMark& holder_mark, int status_id, int side_to_check) {
    ScenarioBattle b(
        scenario::make_party(scenario::make_pet(1, "贝露", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                                /*speed=*/100, /*atk=*/10, /*hp=*/1000000,
                                                holder_mark)),
        scenario::make_party(scenario::make_pet(2, "进攻方", {kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                                /*speed=*/300, /*atk=*/5000, /*hp=*/100000)));
    BattleContext& ctx = b.ctx();
    sbs_srand(20260916);
    b.start();
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    return ctx.has_active_abnormal_status(side_to_check, status_id);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ A effect 262：被打 100% → 进攻方中毒 ════════════════════════════════
    CHECK(run_and_check(make_mark(262, {100}, 2439), static_cast<int>(AbnormalStatusId::Poison), 1),
          "A 受到攻击 → 进攻方中毒（db[0]=100 档）");
    // ══ B effect 262：0% → 不中毒 ═══════════════════════════════════════════
    CHECK(!run_and_check(make_mark(262, {0}, 2439), static_cast<int>(AbnormalStatusId::Poison), 1),
          "B 0% 档 → 不中毒");
    // ══ C effect 306：每回合 100% 害怕（回合开始施加，1 方挂上）═════════════
    CHECK(run_and_check(make_mark(306, {100, 6}, 2612), static_cast<int>(AbnormalStatusId::Fear), 1),
          "C 306 回合开始 100% → 对手害怕");
    // ══ D effect 306：0% → 不挂 ═════════════════════════════════════════════
    CHECK(!run_and_check(make_mark(306, {0, 6}, 2612), static_cast<int>(AbnormalStatusId::Fear), 1),
          "D 306 0% 档 → 不害怕");

    std::cout << "scenario_946 PASS (K2 262 受击施加 + 306 每回合施加)" << std::endl;
    return 0;
}
