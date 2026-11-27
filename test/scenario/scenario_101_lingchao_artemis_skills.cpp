// 场景 101 —— 灵巢之主 4599 / 阿尔忒弥斯 4745 技能组
//
// 锁的是 2026-09-20 实现（技能全文 = 4399 pets_full；模板 = DB effect_info）：
//   A. 1164（迴还）：反转对手提升；对手无提升时走补偿分支（消除提升+断回合）
//   B. 2106（命诞如虚）：命起+1 → 魂落镜像-1（层和恒 4）；命起高于魂落 → 回血+等量百分比
//   C. 2107（魂殒若梦）：魂落+1 → 命起镜像-1
//   D. 712（千魂之结）：对手攻/特攻正等级置零
//   E. 1545（抿叶扬音）：消除对手护盾并等量转移给自己
//   F. 1279（森语百兽谣）：反转自身下降 → 成功则对手技能被拦
//   G. 2278（森语百兽谣）：对手为狩杀之的 → 增伤；否则本回合攻击伤害归零
#include "../../resources/lingchao_layers.h"
#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kHit = 10001;
constexpr int kLingchao = 2193;  // effect_id（2026-09-27 对齐装配层）
constexpr int kArtemis = 2354;  // effect_id（2026-09-27 对齐装配层）
constexpr int kMingqiKey = 990459921;
constexpr int kHunluoKey = 990459922;

std::array<int, 5> plain() { return {kHit, kHit, kHit, kHit, kHit}; }

// 灵巢：带真魂印（层数由 arm 初始化 2/2）+ 指定技能在 0 号槽。
ElfPet lingchao_pet(int skill0, int hp = 3000, int atk = 1, int speed = 300) {
    return make_pet(kLingchao, "索杰德尔", {skill0, kHit, kHit, kHit, kHit}, speed, atk, hp,
                    SoulMark(kLingchao, "索杰德尔", "命起魂落"));
}

ElfPet artemis_pet(int skill0, int hp = 3000, int atk = 1, int speed = 300) {
    return make_pet(kArtemis, "阿尔忒弥斯", {skill0, kHit, kHit, kHit, kHit}, speed, atk, hp,
                    SoulMark(kArtemis, "阿尔忒弥斯", "狩杀之的"));
}

ElfPet fodder(int hp = 3000, int speed = 50) {
    return make_pet(1, "靶子", plain(), speed, /*atk=*/1, hp);
}

std::array<ElfPet, 6> party_of(const ElfPet& first) {
    return {first, fodder(), fodder(), fodder(), fodder(), fodder()};
}

int layers(BattleContext& x, int side, int slot, int key) {
    return lingchao_layers(&x, side, slot, key);
}

// 无参构造一条回合类效果（可被断）挂在 side 上。
void give_round_effect(BattleContext& x, int side, int id, int rounds) {
    Effect wrapper(id, 0, side, -1, EffectArgs(std::vector<int>{side}), nullptr);
    auto ce = std::make_unique<ContinuousEffect>(
        wrapper, State::BATTLE_ROUND_START, side, rounds, x.roundCount);
    ce->source_id_ = id;
    x.register_skill_effect(State::BATTLE_ROUND_START, side, std::move(ce));
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ════════════════════════════════════════════════════════════════
    // A. 1164 迴还：对手有提升 → 反转；对手无提升 → 补偿（消除提升+断回合）
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(party_of(lingchao_pet(37083)), party_of(fodder()));
        b.start();
        b.ctx().ability_level(1, 0) = 2;            // 对手攻击 +2（有提升）
        give_round_effect(b.ctx(), 1, 9001, 3);     // 对手一条回合类
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 迴还
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(b.ctx().ability_level(1, 0) < 0, "A：对手提升被反转成下降（1164 反转成功）");
    }

    // ════════════════════════════════════════════════════════════════
    // A2. 1164 补偿分支：对手无提升 → 消除其提升 + 断其回合类
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(party_of(lingchao_pet(37083)), party_of(fodder()));
        b.start();
        give_round_effect(b.ctx(), 1, 9002, 3);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(b.ctx().skills_effects.active_round_count(1), 0,
                 "A2：对手无提升 → 补偿分支断其回合类效果");
    }

    // ════════════════════════════════════════════════════════════════
    // B. 2106 命诞如虚：命起+1（魂落镜像-1）；命起高于魂落 → 回血+等量百分比
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(party_of(lingchao_pet(28583, /*hp=*/3000)), party_of(fodder(30000)));
        b.start();
        CHECK_EQ(layers(b.ctx(), 0, 0, kMingqiKey), 2, "B 前置：2/2");
        b.pet(0).hp = 1000;   // 留出回血空间
        const int foe_before = b.ctx().getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 命诞如虚
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(layers(b.ctx(), 0, 0, kMingqiKey), 3, "B：命起 +1 → 3 层");
        CHECK_EQ(layers(b.ctx(), 0, 0, kHunluoKey), 1, "B：魂落镜像 -1 → 1 层（和恒 4）");
        CHECK(b.pet(0).hp > 1000, "B：命起高于魂落 → 恢复最大体力 1/{0}");
        CHECK(b.ctx().getPet(1).hp < foe_before, "B：附加等量百分比伤害");
    }

    // ════════════════════════════════════════════════════════════════
    // C. 2107 魂殒若梦：魂落+1（命起镜像-1）
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(party_of(lingchao_pet(28584)), party_of(fodder(30000)));
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 魂殒若梦
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(layers(b.ctx(), 0, 0, kHunluoKey), 3, "C：魂落 +1 → 3 层");
        CHECK_EQ(layers(b.ctx(), 0, 0, kMingqiKey), 1, "C：命起镜像 -1 → 1 层");
    }

    // ════════════════════════════════════════════════════════════════
    // D. 712 千魂之结：对手攻/特攻正等级置零
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(party_of(lingchao_pet(28582)), party_of(fodder()));
        b.start();
        b.ctx().ability_level(1, 0) = 2;   // 攻击 +2
        b.ctx().ability_level(1, 2) = 3;   // 特攻 +3
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 千魂之结
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        // 712 的"置零"在每回合开始结算 → 再跑一回合让窗口载荷执行。
        b.ctx().ability_level(1, 0) = 2;   // 复原，验证下一回合被压回 0
        b.ctx().ability_level(1, 2) = 3;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(b.ctx().ability_level(1, 0), 0, "D：对手攻击被置零");
        CHECK_EQ(b.ctx().ability_level(1, 2), 0, "D：对手特攻被置零");
    }

    // ════════════════════════════════════════════════════════════════
    // E. 1545 抿叶扬音：消除对手护盾并等量转移给自己
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(party_of(artemis_pet(37861)), party_of(fodder()));
        b.start();
        b.ctx().getPet(1).shield_bank_.add(0, 500, /*source_id=*/9001, /*duration=*/0);
        // 注意：她魂印标记对手时已给被标记者挂了"等于其体力值"的护盾（1200），
        // 故转移量 = 该笔 + 本次 500 —— 断言按**转移前实际总量**取，不硬编码。
        const int foe_shield_before = b.ctx().getPet(1).shield_bank_.total();
        CHECK(foe_shield_before > 500, "E 前置：对手已有护盾（含魂印标记那笔）");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 抿叶扬音
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(b.ctx().getPet(1).shield_bank_.total(), 0, "E：对手护盾被消除");
        CHECK_EQ(b.pet(0).shield_bank_.quantity_of(1545), foe_shield_before,
                 "E：等量护盾转移给自己");
    }

    // ════════════════════════════════════════════════════════════════
    // F. 1279 森语百兽谣：反转自身下降 → 成功则对手技能被拦
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(party_of(artemis_pet(37863)), party_of(fodder()));
        b.start();
        b.ctx().ability_level(0, 0) = -2;   // 自身攻击下降（可反转）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 森语百兽谣
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(b.ctx().ability_level(0, 0), 2, "F：自身下降被反转成 +2");
        // 反转成功 → 对手侧被封技能（seal 票在拦）。
        bool sealed = false;
        for (const RuleTicket& t : b.ctx().rule_center_.entries()) {
            if (t.target == 1 && t.source_effect_id == 1279) {
                sealed = true;
            }
        }
        CHECK(sealed, "F：反转成功 → 对手技能被拦（1279 封票在）");
    }

    // ════════════════════════════════════════════════════════════════
    // G. 2278 森语百兽谣：对手为狩杀之的 → 增伤；否则归零
    // ════════════════════════════════════════════════════════════════
    {
        // G1：把对手在场宠标为狩杀之的（模拟魂印标记）→ 应增伤（不掉血为 0）。
        ScenarioBattle b(party_of(artemis_pet(37863, /*hp=*/5000, /*atk=*/200)), party_of(fodder(30000)));
        b.start();
        auto& storage = b.ctx().seerRobot[0].elfPets[0].soulmark_storage;
        storage[990474511] = 1;   // 标记 side=1
        storage[990474512] = 0;   // slot=0（对手在场）
        // 标记时已给对手挂了"等于其体力值"的护盾/护罩 → 先清掉，否则伤害全被盾吃掉。
        b.ctx().getPet(1).shield_bank_.clear();
        b.ctx().getPet(1).hood_bank_.clear();
        const int foe_before = b.ctx().getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(b.ctx().getPet(1).hp < foe_before, "G1：对手为狩杀之的 → 正常增伤打出伤害");
    }

    std::cout << "scenario_101_lingchao_artemis_skills: all groups passed" << std::endl;
    return 0;
}
