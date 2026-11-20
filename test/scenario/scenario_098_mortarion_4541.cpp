// 场景 098 —— 莫塔里安 4541 完整版：回合开始 riders + 未击败害怕
//
// 官方（4399 pet_id 4541）：
//   A. 登场 300 护罩（091 D 已锁登场/重触发，本场景只锁 riders 前置）
//   B. 护罩在 + 对手无回合类效果 → 当回合免疫异常 + 束缚对手 + 对手攻击后速度-2
//   C. 对手有回合类效果 → 自身全属性+1（无束缚）
//   D. 护罩被耗尽 → riders 整体不结算
//   E. 未击败 + 对手处于能力下降 → 害怕（对手未死亡才结算）
//
// 击败重触发/挡下次攻击在 091 D/D2 已锁，不重复。

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kSkill = 10001;
constexpr int kMortarion = 2142;  // effect_id（2026-09-27 对齐）

std::array<int, 5> plain_skills() { return {kSkill, kSkill, kSkill, kSkill, kSkill}; }

ElfPet mortarion_pet(int hp = 8000) {
    return make_pet(kMortarion, "莫塔里安", plain_skills(), /*speed=*/300, /*atk=*/1, hp,
                    SoulMark(kMortarion, "莫塔里安", "riders"));
}

ElfPet plain_pet(int hp = 8000, int atk = 1) {
    return make_pet(1, "普通", plain_skills(), /*speed=*/100, atk, hp);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ════════════════════════════════════════════════════════════════
    // B. 护罩在 + 对手无回合类效果 → 免异常票 + 束缚 + 对手攻击后速度-2
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> p0 = {mortarion_pet(), plain_pet(), plain_pet(),
                                    plain_pet(), plain_pet(), plain_pet()};
        std::array<ElfPet, 6> p1 = {plain_pet(), plain_pet(), plain_pet(),
                                    plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(p0, p1);
        b.start();
        CHECK_EQ(b.pet(0).hood_bank_.total(), 300, "B 前置：登场 300 护罩");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        // 免疫票窗口 = 本回合（InRounds 1），advanceRound 后过期 → 扫 entries 验证授予。
        bool anomaly_ticket = false;
        for (const RuleTicket& t : b.ctx().rule_center_.entries()) {
            if (t.target == 0
                && t.category == RuleCategory::IMMUNE && t.subtype == static_cast<int>(ImmunityType::ANOMALY)
                && t.source_effect_id == kMortarion) {
                anomaly_ticket = true;
            }
        }
        CHECK(anomaly_ticket, "B：当回合自身免疫异常（票已授予）");
        CHECK(b.ctx().has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Bind)),
              "B：对手不处于回合类效果 → 束缚");
        CHECK_EQ(b.ctx().ability_level(1, 4), -2, "B：对手攻击后速度-2");
    }

    // ════════════════════════════════════════════════════════════════
    // C. 对手有回合类效果 → 全属性+1（无束缚）
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> p0 = {mortarion_pet(), plain_pet(), plain_pet(),
                                    plain_pet(), plain_pet(), plain_pet()};
        std::array<ElfPet, 6> p1 = {plain_pet(), plain_pet(), plain_pet(),
                                    plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(p0, p1);
        b.start();
        // 给对手手工挂一条回合类效果（duration>0 → active_round_count>0）。
        {
            Effect wrapper(9001, 0, 1, -1, EffectArgs(std::vector<int>{1}), nullptr);
            auto ce = std::make_unique<ContinuousEffect>(
                wrapper, State::BATTLE_ROUND_START, 1, /*duration=*/2, b.ctx().roundCount);
            ce->source_id_ = 9001;
            b.ctx().register_skill_effect(State::BATTLE_ROUND_START, 1, std::move(ce));
        }
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(!b.ctx().has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Bind)),
              "C：对手有回合类效果 → 不束缚");
        CHECK_EQ(b.ctx().ability_level(0, 0), 1, "C：全属性+1（攻击）");
        CHECK_EQ(b.ctx().ability_level(0, 4), 1, "C：全属性+1（速度）");
    }

    // ════════════════════════════════════════════════════════════════
    // D. 护罩被耗尽 → riders 不结算（无免异常票、无束缚、无速度-2）
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> p0 = {mortarion_pet(), plain_pet(), plain_pet(),
                                    plain_pet(), plain_pet(), plain_pet()};
        std::array<ElfPet, 6> p1 = {plain_pet(), plain_pet(), plain_pet(),
                                    plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(p0, p1);
        b.start();
        b.pet(0).hood_bank_.clear();   // 护罩没了
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        bool anomaly_ticket_d = false;
        for (const RuleTicket& t : b.ctx().rule_center_.entries()) {
            if (t.target == 0
                && t.category == RuleCategory::IMMUNE && t.subtype == static_cast<int>(ImmunityType::ANOMALY)
                && t.source_effect_id == kMortarion) {
                anomaly_ticket_d = true;
            }
        }
        CHECK(!anomaly_ticket_d, "D：护罩不在 → 无免异常票");
        CHECK(!b.ctx().has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Bind)),
              "D：护罩不在 → 不束缚");
        CHECK_EQ(b.ctx().ability_level(1, 4), 0, "D：护罩不在 → 速度-2 不结算");
    }

    // ════════════════════════════════════════════════════════════════
    // E. 未击败 + 对手处于能力下降 → 害怕
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> p0 = {mortarion_pet(), plain_pet(), plain_pet(),
                                    plain_pet(), plain_pet(), plain_pet()};
        std::array<ElfPet, 6> p1 = {plain_pet(), plain_pet(), plain_pet(),
                                    plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(p0, p1);
        b.start();
        b.ctx().ability_level(1, 0) = -2;   // 对手处于能力下降
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(b.ctx().has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Fear)),
              "E：未击败 + 对手有降级 → 害怕");
        CHECK(b.pet(0).hp > 0 && b.ctx().getPet(1).hp > 0, "E 前置：双方都活着（未击败语义）");
    }

    std::cout << "scenario_098_mortarion_4541: all groups passed" << std::endl;
    return 0;
}
