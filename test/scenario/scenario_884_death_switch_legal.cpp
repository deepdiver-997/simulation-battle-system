// 场景 884 —— 死切期合法动作集（D1，2026-10-01《终局与死切状态机改造单》）
//
// 模型：死切期合法操作 = 仅 CHOOSE_PET。后端 legal_actions 输出 canSkip=false
//（跳过不出现在合法集合——FSM 对 NONE 本就回 "Error: must choose pet"，这里从
// 源头不给选项）；技能/药剂档在死切期一律不可用；前端据 canSkip 隐藏跳过按钮。
//
// 断言组：
//   A 合法集合：死切等待中，败方 must_choose_pet + can_skip=false，5 技能全
//     must_choose_pet 不可用、药剂全不可用、场上死宠 fainted / 替补可用；
//     健方 reason=your_on_stage_pet_is_alive（该等的不是他）。
//   B NONE 被拒：跳过提交不推进（状态/等待方原样），随后 CHOOSE_PET 照常受理。
//   C 补位续跑：换上替补后对局推进到下一回合选择期，死宠保持登记。
#include <iostream>

#include <primitives/battle_primitives.h>
#include <server/legal_actions.h>

// legal_actions 是服务端源（不在 sim_core 里）——按既有惯例直接并入场景 TU
//（scenario_136/172 的 battle_init.cpp 同款）。
#include "../../src/server/legal_actions.cpp"

#include <entities/pet_factory.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kGaoSu = 20017;    // 高速移动：属性技（无伤害噪声）
constexpr int kYanFan = 10037;   // 燕返：威力 60 必中

ElfPet victim_pet(int id, const char* name) {
    return scenario::make_pet(id, name, {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                              /*speed=*/100, /*atk=*/10, /*hp=*/100);
}

// 击杀方桩：高速先手 + 大攻，燕返一击必杀。
ElfPet killer_pet(int id) {
    return scenario::make_pet(id, "击杀方", {kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                              /*speed=*/300, /*atk=*/5000, /*hp=*/100000);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    ScenarioBattle b(scenario::make_party(victim_pet(1, "败方")),
                     scenario::make_party(killer_pet(2)));
    BattleContext& ctx = b.ctx();
    b.start();
    // 败方只留两个活口：场上 slot0（将被击杀）+ 替补 slot1；2..5 槽直写归零
    //（停等期直写不触发登记；PROTECTION_1 流程保护只拉场上，不碰场下）。
    for (int i = 2; i < 6; ++i) {
        ctx.seerRobot[0].elfPets[i].hp = 0;
    }
    // 先手燕返一击杀场上 → 本回合死亡漏斗登记 → 死切等待。
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();

    // ══ A 死切期合法集合 ═════════════════════════════════════════════════════
    {
        CHECK_EQ(static_cast<int>(ctx.currentState),
                 static_cast<int>(State::CHOOSE_AFTER_DEATH), "A 停在死切等待");
        CHECK_EQ(ctx.current_player_id_, 0, "A 等的是败方");
        CHECK(ctx.pet_death_notified[0][ctx.on_stage[0]], "A 场上击杀已登记");
        CHECK(ctx.seerRobot[0].elfPets[1].hp > 0, "A 有存活替补（不触发全灭终局）");

        CHECK(server::player_must_switch_pet(ctx, 0), "A 败方必须换宠");
        CHECK(!server::player_must_switch_pet(ctx, 1), "A 健方不被要求换宠");

        const server::LegalActions legal0 = server::compute_legal_actions(ctx, 0);
        CHECK(legal0.can_act, "A 败方可操作");
        CHECK(legal0.must_choose_pet, "A must_choose_pet=true");
        CHECK(!legal0.can_skip, "A can_skip=false（D1：跳过不进合法集合）");
        CHECK_EQ(static_cast<int>(legal0.skills.size()), 5, "A 技能档 5 条");
        for (const server::LegalSkill& s : legal0.skills) {
            CHECK(!s.usable && s.reason == "must_choose_pet",
                  "A 技能一律不可用（reason=must_choose_pet）");
        }
        for (const server::LegalMedicine& m : legal0.medicines) {
            CHECK(!m.usable, "A 药剂一律不可用");
        }
        CHECK(!legal0.pets[0].usable && legal0.pets[0].reason == "fainted",
              "A 场上死宠 fainted");
        CHECK(legal0.pets[1].usable, "A 替补可用（唯一合法动作载体）");
        CHECK(server::has_any_legal_action(legal0), "A 有合法动作（不该触发服务端兜底）");

        const server::LegalActions legal1 = server::compute_legal_actions(ctx, 1);
        CHECK(!legal1.can_act, "A 健方此刻不可操作");
        CHECK(legal1.reason == "your_on_stage_pet_is_alive",
              "A 健方提示口径：场上精灵还活着");
        std::cout << "  [A] 死切合法集合：仅换宠、无跳过 ✓" << std::endl;
    }

    // ══ B NONE 被拒 ══════════════════════════════════════════════════════════
    {
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        CHECK_EQ(static_cast<int>(ctx.currentState),
                 static_cast<int>(State::CHOOSE_AFTER_DEATH), "B NONE 后仍在死切等待");
        CHECK_EQ(ctx.current_player_id_, 0, "B 仍等败方（跳过不推进）");
        std::cout << "  [B] 死切期 NONE 被拒、不推进 ✓" << std::endl;
    }

    // ══ C 补位续跑 ═══════════════════════════════════════════════════════════
    {
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.drive();
        CHECK_EQ(ctx.on_stage[0], 1, "C 替补登场");
        CHECK(ctx.seerRobot[0].elfPets[1].hp > 0, "C 替补满血登场");
        CHECK(ctx.pet_death_notified[0][0], "C 倒下的前任保持死亡登记");
        CHECK(ctx.currentState == State::OPERATION_CHOOSE_SKILL_MEDICAMENT,
              "C 对局推进到下一回合选择期");
        CHECK(!ctx.finish_flag, "C 未终局（补位成功）");
        std::cout << "  [C] 换宠受理、对局续跑 ✓" << std::endl;
    }

    std::cout << "scenario_880 PASS (死切期合法动作集 = 仅 CHOOSE_PET，无跳过选项)"
              << std::endl;
    return 0;
}
