#include <utils/sbs_rand.h>
// 场景 947 —— K2 攻击附带（effect 265）：艾妮双生"开场2回合窗" + 耀世之魂无窗行
//
// 机制口径：直接攻击（used_attribute=false）使用即掷；艾妮行限 roundCount<=2；
// 耀世之魂 db[1]/db[2] = pct/异常（db[0]=1000 语义不明不消费）；格雷姆流血行记档跳过。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>

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

// 0 方带魂印攻 rounds 回合（第 attack_round 回合出手，其余 NONE），查 1 方 status。
bool probe_after_rounds(const SoulMark& mark, int rounds, int attack_round, int status_id) {
    ElfPet holder = scenario::make_pet(1, "艾妮", {kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                       /*speed=*/300, /*atk=*/5000, /*hp=*/100000, mark);
    ScenarioBattle b(scenario::make_party(holder),
                     scenario::make_party(scenario::make_pet(
                         2, "沙包", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                         /*speed=*/100, /*atk=*/10, /*hp=*/1000000)));
    BattleContext& ctx = b.ctx();
    sbs_srand(20260916);
    b.start();
    for (int r = 1; r <= rounds; ++r) {
        b.submit_operation(0, r == attack_round ? BattleFsm::ActionType::SELECT_SKILL
                                                : BattleFsm::ActionType::NONE,
                           r == attack_round ? 0 : -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
    }
    return ctx.has_active_abnormal_status(1, status_id);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ A 艾妮行（无参→30% 冻伤+2回合窗；db[0]=100 覆盖为必中）：第 1 回合出手 → 冻伤
    CHECK(probe_after_rounds(make_mark(265, {100}, 2458), 1, 1,
                             static_cast<int>(AbnormalStatusId::Frostbite)),
          "A 开场 2 回合窗内攻击 → 冻伤");
    // ══ B 窗外：第 3 回合才出手 → 不冻（roundCount>2 门）════════════════════
    CHECK(!probe_after_rounds(make_mark(265, {100}, 2458), 3, 3,
                              static_cast<int>(AbnormalStatusId::Frostbite)),
          "B 第 3 回合攻击 → 窗已过不冻");
    // ══ C 耀世之魂 db[1000,100,2]：无窗 + 100% 烧伤（db[1]/db[2] 参数位）════
    CHECK(probe_after_rounds(make_mark(265, {1000, 100, 2}, 2591), 3, 3,
                             static_cast<int>(AbnormalStatusId::Burn)),
          "C 耀世之魂无窗行第 3 回合仍烧（参数位 db[1]/db[2]）");
    // ══ D 格雷姆流血行（无参、monster=2562）：不施加（记档）═════════════════
    CHECK(!probe_after_rounds(make_mark(265, {}, 2562), 1, 1,
                              static_cast<int>(AbnormalStatusId::Bleed)),
          "D 流血(16) 记档行不施加");

    std::cout << "scenario_947 PASS (K2 265 攻击附带：2回合窗 + 无窗行 + 记档)" << std::endl;
    return 0;
}
