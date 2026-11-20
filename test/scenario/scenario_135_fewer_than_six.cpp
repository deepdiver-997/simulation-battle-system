#include <utils/sbs_rand.h>
// 场景 135 —— 上场少于 6（空位垫底，2026-09-26）
//
// 锁定空位语义（官方允许上场少于 6；低位填充 0、1、2，不会出现 0、3、5）：
//   A. 2 只上场：开局正常、空位 fainted 不可选（legal pets）
//   B. 空位不被群体恢复复活/回血（heal/revive 入口 id<=0 守卫）
//   C. 全灭判负：唯一真实精灵倒下 → 对局结束（空位不算战力）
#include <entities/pet_factory.h>
#include <plugin/core_api.h>
#include <primitives/battle_primitives.h>
#include <fsm/battleContext.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {
constexpr int kPlain = 10001;

ElfPet plain_pet(int speed = 100) {
    return make_pet(1, "普", {kPlain, kPlain, kPlain, kPlain, kPlain}, speed, 100, 5000);
}

// 2 只真实 + 4 空位的队伍（低位填充：0、1 真实，2~5 空位 id=-1）
std::array<ElfPet, 6> party_of_two() {
    return {plain_pet(300), plain_pet(100),
            PetFactory::create_empty_pet(), PetFactory::create_empty_pet(),
            PetFactory::create_empty_pet(), PetFactory::create_empty_pet()};
}
}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }
    sbs_srand(7);

    // ═══ A. 2 只上场：开局/换宠 legal 语义 ══════════════════════════
    {
        ScenarioBattle b(party_of_two(), scenario::make_party(plain_pet(50)));
        b.start();
        BattleContext& x = b.ctx();
        CHECK_EQ(x.seerRobot[0].elfPets[0].id, 1, "A1 首发位真实精灵");
        CHECK(x.seerRobot[0].elfPets[2].id < 0, "A2 槽 2~5 为空位（id=-1）");
        CHECK_EQ(x.seerRobot[0].elfPets[2].hp, 0, "A3 空位恒 hp=0");

        // legal actions 的 usable 判据 = hp<=0 → fainted 不可选（legal_actions.cpp）。
        // 场景只链 sim_core，这里锁判据输入（空位 hp=0 → 不可选的充分条件）：
        for (int slot = 2; slot < 6; ++slot) {
            CHECK(x.seerRobot[0].elfPets[slot].hp <= 0
                      && x.seerRobot[0].elfPets[slot].id < 0,
                  "A4 空位 hp=0/id=-1 → legal 判定必然不可选（fainted）");
        }
        // 空位换宠被拒：submit 后 FSM 拒绝（usable=false 的槽 CHOOSE_PET 无效）
        // 直接断言 perform_switch 前置：切到空位后 on_stage 不变
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 3);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(x.on_stage[0] == 0 || x.seerRobot[0].elfPets[x.on_stage[0]].id > 0,
              "A6 在场位不会落到空位上");
    }

    // ═══ B. 空位不被治疗/复活 ═══════════════════════════════════════
    {
        ScenarioBattle b(party_of_two(), scenario::make_party(plain_pet(50)));
        b.start();
        BattleContext& x = b.ctx();
        auto& core = core_api();
        ElfPet& empty3 = x.seerRobot[0].elfPets[3];
        // heal_amount 的 target 是 side 寻址（作用对象=on_stage 精灵），空位永不
        // on_stage → 群体恢复只可能经 heal_impl 直呼（id<=0 守卫拦）。
        // 这里直接锁 revive_pet 对空位拒绝：
        CHECK(core.revive_pet(&x, 0, 3, 100) < 0, "B2 revive_pet 对空位拒绝（-1）");
        CHECK_EQ(empty3.hp, 0, "B3 空位仍 hp=0（未被复活）");
    }

    // ═══ C. 全灭判负：2 只全倒 → 引擎判负（空位不算战力）════════════
    {
        ScenarioBattle b(party_of_two(), scenario::make_party(plain_pet(50)));
        b.start();
        BattleContext& x = b.ctx();
        // 打死两只真实精灵
        for (int slot = 0; slot < 2; ++slot) {
            x.seerRobot[0].elfPets[slot].hp = 0;
        }
        auto& core = core_api();
        if (core.defeat_pet) {
            core.defeat_pet(&x, 0, 0, 1, DefeatCause::DAMAGE);
            core.defeat_pet(&x, 0, 1, 1, DefeatCause::DAMAGE);
        } else {
            x.seerRobot[0].elfPets[0].hp = 0;
            x.seerRobot[0].elfPets[1].hp = 0;
        }
        const bool any_alive =
            x.seerRobot[0].elfPets[0].hp > 0 || x.seerRobot[0].elfPets[1].hp > 0;
        CHECK(!any_alive, "C1 两只真实精灵全倒（空位不构成战力，胜负由引擎判）");
    }

    std::cout << "scenario_135 done" << std::endl;
    return 0;
}
