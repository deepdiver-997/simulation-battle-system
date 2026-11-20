#include <utils/sbs_rand.h>
// 场景 948 —— K2 攻击技施加（effect 358 忍者族）："自身攻击类技能有X%概率使对手Y 2回合"
//
// 机制口径：EVENT_SKILL_USED + 属性技过滤（used_attribute）；时长固定 2 回合（tips 明示）；
// 失明行（越马 3369）记档跳过。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kYanFan = 10037;   // 燕返：威力 60 必中（攻击技）
constexpr int kGaoSu = 20017;    // 高速移动：属性技

SoulMark make_mark(int effect_id, std::vector<int> db_args, int monster_id) {
    SoulMark sm(effect_id, "K" + std::to_string(effect_id), "test",
                EffectArgs(std::move(db_args)));
    sm.monster_id = monster_id;
    return sm;
}

// 0 方带魂印用 skill_index（0=攻击技，1=属性技）出一回合，查 1 方 status。
bool probe_applied(const SoulMark& mark, int skill_index, int status_id) {
    ElfPet holder = scenario::make_pet(1, "影忍·黑段", {kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                       /*speed=*/300, /*atk=*/5000, /*hp=*/100000, mark);
    ScenarioBattle b(scenario::make_party(holder),
                     scenario::make_party(scenario::make_pet(
                         2, "沙包", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                         /*speed=*/100, /*atk=*/10, /*hp=*/1000000)));
    BattleContext& ctx = b.ctx();
    sbs_srand(20260916);
    b.start();
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, skill_index);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
    return ctx.has_active_abnormal_status(1, status_id);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    constexpr int kFatigue = static_cast<int>(AbnormalStatusId::Fatigue);
    constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);
    constexpr int kBlind = static_cast<int>(AbnormalStatusId::Blind);

    // ══ A 攻击技 100% → 疲惫落地（影忍·黑段行参数位 db[2]/db[3]）════════════
    CHECK(probe_applied(make_mark(358, {1, 1, 100, 7}, 2747), 0, kFatigue),
          "A 攻击技 100% → 对手疲惫");
    // ══ B 属性技不触发（"攻击类技能"过滤）══════════════════════════════════
    CHECK(!probe_applied(make_mark(358, {1, 1, 100, 7}, 2747), 1, kFatigue),
          "B 属性技 → 不施加（攻击类过滤）");
    // ══ C 0% → 不施加 ══════════════════════════════════════════════════════
    CHECK(!probe_applied(make_mark(358, {1, 1, 0, 7}, 2747), 0, kFatigue),
          "C 0% 档 → 不施加");
    // ══ D 烧伤参数位（火忍·浓信同款）═══════════════════════════════════════
    CHECK(probe_applied(make_mark(358, {1, 1, 100, 2}, 2753), 0, kBurn),
          "D db[3]=2 → 烧伤落地");
    // ══ E 失明行记档跳过（越马 [1,1,30,20]）════════════════════════════════
    CHECK(!probe_applied(make_mark(358, {1, 1, 100, 20}, 3369), 0, kBlind),
          "E 失明(20) 记档行不施加");

    std::cout << "scenario_948 PASS (K2 358 忍者族：攻击类过滤 + 2回合 + 记档)" << std::endl;
    return 0;
}
