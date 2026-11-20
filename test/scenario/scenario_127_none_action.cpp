#include <utils/sbs_rand.h>
// 场景 127 —— ActionType::NONE（空操作）语义锁定（2026-09-25）
//
// A. NONE 基础：不出手（对手 hp 不变）、自身 PP 不扣、对手攻击照常落地（与嗑药同路：
//    is_skill_action=false → 跳主流程，回合类/异常后续照走）
// B. NONE 计"未使用技能"：1287 自然祝福回合末"用技 -1 / 未用 +1"在 NONE 回合走 +1
//    （回合类效果在空操作回合照常触发）
#include <any>
#include <entities/pet_factory.h>
#include <plugin/core_api.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPlain = 10001;
constexpr int kMoni = 3711;
constexpr int kLegacy = 1287;

int pub_get(BattleContext& x, int key) {
    auto it = x.plugin_storage.find(key);
    if (it == x.plugin_storage.end()) {
        return 0;
    }
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : 0;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }
    sbs_srand(7);

    // ═══ A. NONE 基础语义 ══════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> own = {
            make_pet(1, "坦克", {kPlain, kPlain, kPlain, kPlain, kPlain},
                     /*speed=*/100, /*atk=*/100, 100000),
            make_pet(2, "队友", {kPlain, kPlain, kPlain, kPlain, kPlain}, 100, 10, 4000),
            make_pet(3, "队友", {kPlain, kPlain, kPlain, kPlain, kPlain}, 100, 10, 4000),
            make_pet(4, "队友", {kPlain, kPlain, kPlain, kPlain, kPlain}, 100, 10, 4000),
            make_pet(5, "队友", {kPlain, kPlain, kPlain, kPlain, kPlain}, 100, 10, 4000),
            make_pet(6, "队友", {kPlain, kPlain, kPlain, kPlain, kPlain}, 100, 10, 4000)};
        ScenarioBattle b(own, scenario::make_party(
            make_pet(9, "对手", {kPlain, kPlain, kPlain, kPlain, kPlain}, 200, 100, 100000)));
        b.start();
        BattleContext& x = b.ctx();

        // R1：空操作——对手攻击照常落地，自己不出手、不扣 PP
        const int foe_hp_before = x.getPet(1).hp;
        const int my_hp_before = x.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        // ⚠️ 选择记录只在选择期可读：提交 side 1 的操作会同步跑完整个回合并重置
        const int choice0 = x.roundChoice[0][0];
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(choice0, static_cast<int>(BattleFsm::ActionType::NONE),
                 "A4 选择记录 = NONE");
        CHECK_EQ(x.getPet(1).hp, foe_hp_before, "A1 空操作：自己未出手（对手 hp 不变）");
        CHECK(x.getPet(0).hp < my_hp_before, "A2 对手攻击照常落地（空操作不挡攻击）");
        CHECK_EQ(x.seerRobot[0].elfPets[x.on_stage[0]].skills[0].pp,
                 x.seerRobot[0].elfPets[x.on_stage[0]].skills[0].maxPP,
                 "A3 空操作不扣 PP");
    }

    // ═══ B. NONE 计"未使用技能"（回合类效果照走）═══════════════════════════
    {
        std::array<ElfPet, 6> own = {
            make_pet(kMoni, "圣光莫妮卡", {kPlain, kPlain, kPlain, kPlain, kPlain},
                     /*speed=*/500, /*atk=*/10, 6000,
                     SoulMark(kLegacy, "神", "测试桩：未神谕 1287 自然祝福")),
            make_pet(2, "队友", {kPlain, kPlain, kPlain, kPlain, kPlain}, 100, 10, 4000),
            make_pet(3, "队友", {kPlain, kPlain, kPlain, kPlain, kPlain}, 100, 10, 4000),
            make_pet(4, "队友", {kPlain, kPlain, kPlain, kPlain, kPlain}, 100, 10, 4000),
            make_pet(5, "队友", {kPlain, kPlain, kPlain, kPlain, kPlain}, 100, 10, 4000),
            make_pet(6, "队友", {kPlain, kPlain, kPlain, kPlain, kPlain}, 100, 10, 4000)};
        ScenarioBattle b(own, scenario::make_party(
            make_pet(9, "对手", {kPlain, kPlain, kPlain, kPlain, kPlain}, 100, 10, 100000)));
        b.start();
        BattleContext& x = b.ctx();
        CHECK_EQ(pub_get(x, 4693300), 4, "B1 前置：首次登场 +4 自然祝福");

        // R1：用技（祝福 4→3）；R2：空操作（回合末走"未使用 → +1"：3→4）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(pub_get(x, 4693300), 3, "B2 用技 → 消耗 1 层（4→3）");
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(pub_get(x, 4693300), 4, "B3 空操作 = 未使用技能 → 回合末 +1（3→4）");
    }

    std::cout << "scenario_127 全部通过" << std::endl;
    return 0;
}
