// 场景 873 —— R2 收官验证：disabledEffects.soulMarks 不误伤技能效果（行为正确关单）。
//
// F 组实测发现②原判"缺陷"：克雷弗德 4644 禁魂 2231 后，28724（隔断式·建构律
// [46,1672,2154] args=[1,300]）的 46 免伤票在 r0 回合末快照里消失。R2 复核结论：
// **引擎行为正确，非缺陷**——46 票正常授予，且当回合对手的攻击就把它正确地挡下
// 并消耗（BLOCK 事件、掉血=0），快照时票已按语义消费；带魂印的"票还在"是
// 2231 侧效果拦掉了对手攻击（污染对照）造成的假象。
//
// 断言（走 PetFactory 工厂路径 = WS/gateway 装配同构；时间线修正为
// "r0 施放、对手不出手 → r1 对手攻击被挡 → r2 落地"）：
//   A 对照（带魂印 2231）：r0 末 46 票 counts=1；
//   B 禁魂（soulMarks=[2231]）：r0 末 46 票 counts=1（挂上）→ r1 攻击掉血=0
//     （挡伤生效）→ r2 攻击落地（消费，次数语义闭合）。
#include <iostream>

#include <entities/pet_factory.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kMark = 2231;       // 克雷弗德魂印（effect_id，2026-09-27 对齐装配层）
constexpr int kBlock = 28724;     // 隔断式·建构律：属性技 [46,1672,2154] args=[1,300]
constexpr int kFill1 = 10086;     // 克雷弗德可学池（工厂路径校验可学性）
constexpr int kFill2 = 10330;
constexpr int kFill3 = 13601;
constexpr int kFill4 = 20241;
constexpr int kPlain = 10001;     // 撞击（对手用）

ElfPet crawford(bool with_mark) {
    BattlePetMessage msg;
    msg.pet_id = 4644;
    msg.chosen_skills_id = {kBlock, kFill1, kFill2, kFill3, kFill4};
    msg.numerical_base = {1, 1, 100, 100, 600, 20000};  // 物攻,特攻,防,特防,速,体
    if (!with_mark) {
        msg.disabled_effects.soul_marks = {kMark};
    }
    ElfPet pet = PetFactory::create_pet(msg);
    for (Skills& s : pet.skills) {
        s.must_hit = true;
        s.accuracy = 100;
    }
    return pet;
}

ElfPet foe_pet() {
    ElfPet pet = make_pet(1, "对手", {kPlain, kPlain, kPlain, kPlain, kPlain},
                          /*speed=*/100, /*atk=*/300, /*hp=*/20000);
    for (Skills& s : pet.skills) {
        s.must_hit = true;
        s.accuracy = 100;
    }
    return pet;
}

bool has_46_ticket(const BattleContext& ctx) {
    for (const RuleTicket& t : ctx.rule_center_.entries()) {
        if (t.category == RuleCategory::IMMUNE && t.source_effect_id == 46
            && t.remaining_counts == 1) {
            return true;
        }
    }
    return false;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ── A 对照：带魂印 ──
    {
        ScenarioBattle b(make_party(crawford(true)), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK(has_46_ticket(ctx), "A 对照：带魂印 r0 末 46 票 counts=1");
        std::cout << "  [A] 带魂印：46 票挂上 ✓\n";
    }

    // ── B：禁魂（本工单主战场）──
    {
        ScenarioBattle b(make_party(crawford(false)), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK(ctx.getPet(0).soulMark.id == 0, "B 前置：禁魂后 soulMark 为裸壳（id=0）");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK(has_46_ticket(ctx),
              "B 核心①：禁魂只禁魂——46 免伤票在 r0 末快照挂上（原观测为时点伪影）");
        // r1：对手攻击被挡（掉血=0）
        const int hp_before = ctx.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(hp_before - ctx.getPet(0).hp, 0, "B 核心②：46 票挡下攻击（掉血=0）");
        // r2：票已消费 → 攻击落地
        const int hp_before_r2 = ctx.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(hp_before_r2 - ctx.getPet(0).hp > 0,
              "B 核心③：挡 1 次已消费，r2 攻击落地（次数语义闭合）");
        std::cout << "  [B] 禁魂：票挂上 → r1 挡下 → r2 落地 ✓\n";
    }

    std::cout << "  [873] 禁魂不误伤技能效果（行为正确关单）通过\n";
    return 0;
}
