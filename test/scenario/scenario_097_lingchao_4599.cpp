// 场景 097 —— 灵巢之主·索杰德尔 4599 完整版：命起/魂落标记机 + 出战免疫 + 补偿盔/魂落粉
//            + 未击败三分支 + 击败转移裸施加
//
// 官方口径（4399 pet_id 4599 + 机制解析 idx=389）：
//   A. 战斗开始 2 命起 2 魂落（和恒 4）
//   B. 对手 0 威力攻击 → 命起-1 + 睡眠；对手免疫睡眠（未触发）→ 补偿盔挂账
//      （威力阈值 = 40×层数和 = 恒 160）
//   C. 自己 0 威力攻击 → 魂落-1 + 睡眠；未触发 → 对手最大体力 1/3 粉伤
//   D. 未击败：命起≥魂落 → 回 1/4；层数相等 → 己方次免票（对手未死亡才结算）
//   E. 击败转移：**免异常也照转**（裸施加不走赋予通道）+ 能力下降转给下只
//   F. 阵亡丢层（idx=389 #1）
//
// 0 威力必中攻击 = 14967 圣盾反击（power=0, accuracy=100, must_hit=1）。

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kSkill = 10001;        // 撞击
constexpr int kZeroAtk = 14967;      // 圣盾反击：0 威力必中攻击
constexpr int kLingchao = 2193;  // 魂印注册键 = effect_id（2026-09-27 对齐；4599=monster id）
constexpr int kMingqiKey = 990459921;
constexpr int kHunluoKey = 990459922;
constexpr int kArmorRoundKey = 990459927;
constexpr int kArmorThresholdKey = 990459928;

std::array<int, 5> mix_skills() { return {kZeroAtk, kSkill, kSkill, kSkill, kSkill}; }

ElfPet lingchao_pet(int hp = 4000, int atk = 1) {
    return make_pet(kLingchao, "索杰德尔", mix_skills(), /*speed=*/300, atk, hp,
                    SoulMark(kLingchao, "索杰德尔", "命起魂落"));
}

ElfPet plain_pet(int hp = 4000, int atk = 1) {
    return make_pet(1, "普通", mix_skills(), /*speed=*/100, atk, hp);
}

int layers_of(BattleContext& x, int side, int slot, int key) {
    auto& storage = x.seerRobot[side].elfPets[slot].soulmark_storage;
    auto it = storage.find(key);
    return it == storage.end() ? 0 : std::any_cast<int>(it->second);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ════════════════════════════════════════════════════════════════
    // A. 战斗开始 2/2
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> p0 = {lingchao_pet(), plain_pet(), plain_pet(),
                                    plain_pet(), plain_pet(), plain_pet()};
        std::array<ElfPet, 6> p1 = {plain_pet(), plain_pet(), plain_pet(),
                                    plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(p0, p1);
        b.start();
        CHECK_EQ(layers_of(b.ctx(), 0, 0, kMingqiKey), 2, "A：战斗开始命起 2 层");
        CHECK_EQ(layers_of(b.ctx(), 0, 0, kHunluoKey), 2, "A：战斗开始魂落 2 层");
    }

    // ════════════════════════════════════════════════════════════════
    // B. 对手 0 威力攻击：命起-1；对手免疫睡眠 → 补偿盔（阈值 160）
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> p0 = {lingchao_pet(), plain_pet(), plain_pet(),
                                    plain_pet(), plain_pet(), plain_pet()};
        std::array<ElfPet, 6> p1 = {plain_pet(), plain_pet(), plain_pet(),
                                    plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(p0, p1);
        b.start();
        // 守方全异常免疫 → 睡眠必"未触发"。
        b.ctx().grant_immunity(1, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                               /*duration_rounds=*/3, /*source_id=*/9001,
                               /*soul_immunity=*/false, EffectScope::ON_STAGE, /*counts=*/0);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);  // 灵巢撞击（非 0 威力）
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);  // 对手 0 威力攻击
        b.drive();
        CHECK_EQ(layers_of(b.ctx(), 0, 0, kMingqiKey), 1, "B：命起-1（对手 0 威力攻击）");
        CHECK_EQ(layers_of(b.ctx(), 0, 0, kHunluoKey), 3, "B：魂落同步+1（和恒 4）");
        std::cerr << "[debug] sleep_on_foe="
                  << b.ctx().has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Sleep))
                  << " immune_query="
                  << b.ctx().is_immune(1, ImmunityType::ANOMALY, State::BATTLE_ROUND_START)
                  << " ps_keys=";
        for (auto& [k, v] : b.ctx().plugin_storage) { std::cerr << k << " "; }
        std::cerr << std::endl;
        auto& ps = b.ctx().plugin_storage;
        auto rd = ps.find(kArmorRoundKey + 0);
        CHECK(rd != ps.end(),
              "B：睡眠未触发 → 补偿盔挂账（当回合旗）");
        auto th = ps.find(kArmorThresholdKey + 0);
        CHECK(th != ps.end() && std::any_cast<int>(th->second) == 160,
              "B：威力阈值 = 40×(1+3) = 160");
        CHECK(!b.ctx().has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Sleep)),
              "B：对手免疫睡眠 → 未被睡眠");
    }

    // ════════════════════════════════════════════════════════════════
    // B2. 对手有弹控：睡眠弹给灵巢自己 → 只看对手最终状态 = 未触发 → 上盔；
    //     对手攻击打到盔上 → 灵巢被打醒（弹上去的睡眠不保留）
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> p0 = {lingchao_pet(), plain_pet(), plain_pet(),
                                    plain_pet(), plain_pet(), plain_pet()};
        std::array<ElfPet, 6> p1 = {plain_pet(), plain_pet(), plain_pet(),
                                    plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(p0, p1);
        b.start();
        // 对手挂弹控 + 免控：弹控只在"免疫命中"时把异常反弹给施放方 → 睡眠弹回灵巢自己。
        b.ctx().grant_immunity(1, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                               /*duration_rounds=*/5, /*source_id=*/9001,
                               /*soul_immunity=*/false, EffectScope::ON_STAGE, /*counts=*/0);
        b.ctx().rule_center_.grant_reflect(/*source_owner=*/1, /*source_slot=*/0,
                                           /*source_effect_id=*/9002, /*target=*/1,
                                           /*rounds=*/3);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);  // 灵巢撞击
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);  // 对手 0 威力攻击
        b.drive();
        CHECK_EQ(layers_of(b.ctx(), 0, 0, kMingqiKey), 1, "B2：命起-1");
        auto& ps = b.ctx().plugin_storage;
        CHECK(ps.count(kArmorRoundKey + 0) && ps.count(kArmorThresholdKey + 0),
              "B2：弹控=对手没睡=未触发 → 补偿盔照上");
        CHECK(!b.ctx().has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Sleep)),
              "B2：对手最终没睡");
        CHECK(!b.ctx().has_active_abnormal_status(0, static_cast<int>(AbnormalStatusId::Sleep)),
              "B2：弹到灵巢身上的睡眠随攻击打到盔上被打醒");
    }

    // ════════════════════════════════════════════════════════════════
    // C. 自己 0 威力攻击：魂落-1；对手免疫睡眠 → 1/3 最大体力粉伤
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> p0 = {lingchao_pet(), plain_pet(), plain_pet(),
                                    plain_pet(), plain_pet(), plain_pet()};
        std::array<ElfPet, 6> p1 = {plain_pet(6000), plain_pet(), plain_pet(),
                                    plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(p0, p1);
        b.start();
        b.ctx().grant_immunity(1, ImmunityType::ANOMALY, coverage_all(), 0,
                               /*duration_rounds=*/5, /*source_id=*/9001, false,
                               EffectScope::ON_STAGE, 0);
        const int foe_max = b.ctx().getPet(1).numericalBase[NumericalPropertyIndex::HP];
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);  // 灵巢 0 威力攻击
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);  // 对手撞击（威力>0 不触发）
        b.drive();
        CHECK_EQ(layers_of(b.ctx(), 0, 0, kHunluoKey), 1, "C：魂落-1（自己 0 威力攻击）");
        CHECK_EQ(layers_of(b.ctx(), 0, 0, kMingqiKey), 3, "C：命起同步+1");
        const int foe_hp = b.ctx().getPet(1).hp;
        CHECK(foe_hp <= foe_max - foe_max / 3, "C：睡眠未触发 → 对手掉 1/3 最大体力粉伤");
    }

    // ════════════════════════════════════════════════════════════════
    // D. 未击败：命起≥魂落 → 回 1/4；相等 → 次免票（对手存活时）
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> p0 = {lingchao_pet(8000), plain_pet(), plain_pet(),
                                    plain_pet(), plain_pet(), plain_pet()};
        std::array<ElfPet, 6> p1 = {plain_pet(8000), plain_pet(), plain_pet(),
                                    plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(p0, p1);
        b.start();
        // 先耗一层命起（对手 0 威力攻击），使下一回合开始时 1/3 —— 命起<魂落。
        // 再打一回合未击败：此时 魂落≥命起 → 只回 PP 不回血；然后 0 威力互打把层数拉回
        // 2/2 相等 → 次免票。此处锁"相等 → 次免 + 双恢复"主路径。
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(layers_of(b.ctx(), 0, 0, kMingqiKey), 1, "D 前置：1/3");
        // 回合 2：双方撞击（威力>0，无触发、无击杀）→ 未击败：命起(1)<魂落(3) → 只回 PP。
        // 恢复作用于**所有**技能 → 用没出过手的 3 号槽观测（出手的 1 号槽会被
        // 回合内 PP 消耗 -1 抵消掉 +1，2026-09-20 实测）。
        b.pet(0).skills[3].pp = 10;
        const int pp_before = 10;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        std::cerr << "[debug] mq=" << layers_of(b.ctx(), 0, 0, kMingqiKey)
                  << " hl=" << layers_of(b.ctx(), 0, 0, kHunluoKey)
                  << " foe_hp=" << b.ctx().getPet(1).hp
                  << " state=" << static_cast<int>(b.state()) << std::endl;
        CHECK_EQ(b.pet(0).skills[3].pp, pp_before + 1, "D：魂落≥命起 → 恢复所有技能 1 点 PP");
        CHECK(!b.ctx().is_immune(0, ImmunityType::ANOMALY, State::BATTLE_OPPONENT_DEFEAT_STATUS),
              "D：层数不等 → 无次免票");
        // 回合 3：双方 0 威力互打 → 1/3 → 0/4（对手攻-命起再-1=0，自己攻-魂落-1=2？）
        // 精确推演：对手 0 威力攻 → 命起 1→0；自己 0 威力攻 → 魂落 3→2。0/2。
        // 再回合 4 互撞：命起(0)<魂落(2) → 只回 PP。不锁，直接转到相等场景：
        // 用两回合各掉一层使 0/2 → 下一轮对手 0 威力攻无命起可扣（0 层不触发）、
        // 自己 0 威力攻 → 魂落 2→1。此时 0/1。循环到 0/0 后层数机停止——层数耗尽即闭环。
        // （相等→次免的路径由 F 组前的 E 组独立构造：开场 2/2 直接未击败回合。）
    }

    // ════════════════════════════════════════════════════════════════
    // D2. 开场直接未击败（2/2 相等）→ 回血 1/4 + PP + 次免票
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> p0 = {lingchao_pet(50000), plain_pet(), plain_pet(),
                                    plain_pet(), plain_pet(), plain_pet()};
        std::array<ElfPet, 6> p1 = {plain_pet(50000), plain_pet(), plain_pet(),
                                    plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(p0, p1);
        b.start();
        // 双方撞击互不掉层、打不死（50000 血 vs atk1）→ 未击败时 2/2 相等。
        // 先打一回合留出回血空间。
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        // 回血每回合都触发（他常驻满血）→ 先手动压低体力再观察精确恢复：
        // 30000 − 对手撞击 2 + 回 1/4(12500) = 42498。
        b.pet(0).hp = 30000;
        const int hp_before = 30000;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        CHECK_EQ(b.pet(0).hp, hp_before - 2 + 50000 / 4, "D2：命起≥魂落 → 回复最大体力 1/4");
        CHECK(b.ctx().is_immune(0, ImmunityType::ANOMALY, State::BATTLE_OPPONENT_DEFEAT_STATUS),
              "D2：层数相等 → 己方免疫下次异常（次免票在）");
    }

    // ════════════════════════════════════════════════════════════════
    // E. 击败转移：免异常的被击败者照样转（裸施加）+ 能力下降转给下只
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> p0 = {lingchao_pet(8000, /*atk=*/300), plain_pet(), plain_pet(),
                                    plain_pet(), plain_pet(), plain_pet()};
        std::array<ElfPet, 6> p1 = {plain_pet(1), plain_pet(4000), plain_pet(4000),
                                    plain_pet(4000), plain_pet(4000), plain_pet(4000)};
        ScenarioBattle b(p0, p1);
        b.start();
        // 守方在场宠：全异常免疫票 + 中毒 3 回合 + 攻击 -2。转移必须无视免疫。
        b.ctx().grant_immunity(1, ImmunityType::ANOMALY, coverage_all(), 0,
                               /*duration_rounds=*/5, /*source_id=*/9001, false,
                               EffectScope::ON_STAGE, 0);
        b.ctx().apply_abnormal_status_for_rounds(1, static_cast<int>(AbnormalStatusId::Poison), 3);
        b.ctx().ability_level(1, 0) = -2;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);  // 撞击击杀
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();   // 停在守方选替补
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.drive();
        CHECK_EQ(b.ctx().on_stage[1], 1, "E 前置：守方补位 1 号槽");
        CHECK(b.ctx().has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Poison)),
              "E：中毒转移给下只（免异常票也拦不住——转移不属于赋予）");
        CHECK_EQ(b.ctx().ability_level(1, 0), -2, "E：能力下降转移给下只");
    }

    // ════════════════════════════════════════════════════════════════
    // F. 阵亡丢层（idx=389 #1：复活后 0 层）
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> p0 = {lingchao_pet(), plain_pet(), plain_pet(),
                                    plain_pet(), plain_pet(), plain_pet()};
        std::array<ElfPet, 6> p1 = {plain_pet(), plain_pet(), plain_pet(),
                                    plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(p0, p1);
        b.start();
        CHECK_EQ(layers_of(b.ctx(), 0, 0, kMingqiKey), 2, "F 前置：2 层命起");
        b.pet(0).hp = 0;   // 漏斗只认 hp<=0（NOT_DOWN 门）
        defeat_pet(&b.ctx(), 0, 0, /*actor=*/1, DefeatCause::DAMAGE);
        b.ctx().event_center_.drain(&b.ctx(), b.ctx().roundCount);   // 死亡 watcher 在 drain 点投递
        CHECK_EQ(layers_of(b.ctx(), 0, 0, kMingqiKey), 0, "F：阵亡 → 命起清零");
        CHECK_EQ(layers_of(b.ctx(), 0, 0, kHunluoKey), 0, "F：阵亡 → 魂落清零");
    }

    std::cout << "scenario_097_lingchao_4599: all groups passed" << std::endl;
    return 0;
}
