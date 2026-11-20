// 场景 099 —— 阿尔忒弥斯 4745 完整版：狩杀之的 + 第二条消耗判定
//
// 官方（4399 pet_id 4745 + 机制解析 idx=246）：
//   A. 登场标记存活的对手在场宠：上限 -60%（直到当回合结束）+ 盾/罩 = 当前体力值
//   B. 回合末上限归还（旧版回合结束后 1，死亡跳 ROUND_END 也恒经）
//   C. ②标记宠=在场对手 → 体力相等 → 真伤+粉增伤双开、不消耗狩杀之魂；
//      攻击命中 → 对手吃两份威力值真伤（一次对在场、一次对标记）
//   D. ②对手体力 > 标记体力（标记宠下场）→ 只开真伤分支，消耗 1 层狩杀之魂；
//      场下标记宠吃真伤致死保留 1 点体力
//   E. 标记宠两管同时在 → 她免异常（条件票）
//
// 获取框架本体在 091 B/C 已锁，不重复。

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kSkill = 10001;      // 撞击
constexpr int kArtemis = 2354;  // 魂印注册键 = effect_id（2026-09-27 对齐；4745=monster id）
constexpr int kMarkShieldSource = 474501;
constexpr int kMarkHoodSource = 474502;
constexpr int kHuntSoulKey = 990474501;

std::array<int, 5> plain_skills() { return {kSkill, kSkill, kSkill, kSkill, kSkill}; }

ElfPet artemis_pet(int hp = 8000) {
    return make_pet(kArtemis, "阿尔忒弥斯", plain_skills(), /*speed=*/300, /*atk=*/1, hp,
                    SoulMark(kArtemis, "阿尔忒弥斯", "狩杀之的"));
}

ElfPet plain_pet(int hp = 1000, int atk = 1) {
    return make_pet(1, "普通", plain_skills(), /*speed=*/100, atk, hp);
}

int max_hp_of(BattleContext& x, int side, int slot) {
    return x.seerRobot[side].elfPets[slot].numericalBase[NumericalPropertyIndex::HP];
}

int hunt_soul_of(BattleContext& x, int side, int slot) {
    auto& storage = x.seerRobot[side].elfPets[slot].soulmark_storage;
    auto it = storage.find(kHuntSoulKey);
    return it == storage.end() ? 0 : std::any_cast<int>(it->second);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ════════════════════════════════════════════════════════════════
    // A. 登场标记：上限 -60% + 盾/罩 = 当前体力值
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> p0 = {artemis_pet(), plain_pet(), plain_pet(),
                                    plain_pet(), plain_pet(), plain_pet()};
        std::array<ElfPet, 6> p1 = {plain_pet(1000), plain_pet(), plain_pet(),
                                    plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(p0, p1);
        b.start();
        // 上限 1000 → -60% → 400；当前体力同步压到 400 → 盾/罩 = 400。
        CHECK_EQ(max_hp_of(b.ctx(), 1, 0), 400, "A：被标记者上限 -60%");
        CHECK_EQ(b.ctx().getPet(1).hp, 400, "A：当前体力同步压低");
        ElfPet& marked = b.ctx().getPet(1);
        CHECK_EQ(marked.shield_bank_.quantity_of(kMarkShieldSource), 400, "A：护盾 = 当前体力值");
        CHECK_EQ(marked.hood_bank_.quantity_of(kMarkHoodSource), 400, "A：护罩 = 当前体力值");
    }

    // ════════════════════════════════════════════════════════════════
    // B. 上限归还（回合末）
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> p0 = {artemis_pet(), plain_pet(), plain_pet(),
                                    plain_pet(), plain_pet(), plain_pet()};
        std::array<ElfPet, 6> p1 = {plain_pet(1000), plain_pet(), plain_pet(),
                                    plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(p0, p1);
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        while (b.state() != State::FINISHED && b.state() != State::OPERATION_CHOOSE_SKILL_MEDICAMENT) {
            b.step();
            std::cerr << "[trace] state=" << static_cast<int>(b.state())
                      << " max=" << max_hp_of(b.ctx(), 1, 0) << std::endl;
        }
        CHECK_EQ(max_hp_of(b.ctx(), 1, 0), 1000, "B：回合末上限归还 1000");
    }

    // ════════════════════════════════════════════════════════════════
    // C. 标记宠=在场对手（体力相等）→ 双分支 + 不消耗；攻击 → 双份威力值真伤
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> p0 = {artemis_pet(), plain_pet(), plain_pet(),
                                    plain_pet(), plain_pet(), plain_pet()};
        std::array<ElfPet, 6> p1 = {plain_pet(1000), plain_pet(), plain_pet(),
                                    plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(p0, p1);
        b.start();
        // 标记后 hp=400、上限 400 → 对手体力==标记体力 → 双开不消耗。狩杀之魂给 2 层。
        b.ctx().seerRobot[0].elfPets[0].soulmark_storage[kHuntSoulKey] = 2;
        const int hp_before = b.ctx().getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const int dealt = hp_before - b.ctx().getPet(1).hp;
        // 撞击红伤（威力 35，微小）+ 两份威力值真伤（35×2=70）→ 总掉血 ≥ 70。
        CHECK(dealt >= 70, "C：双份威力值真伤（在场 + 标记同一只）");
        CHECK_EQ(hunt_soul_of(b.ctx(), 0, 0), 2,
                 "C：同时满足 → 不消耗狩杀之魂");
        // 粉增伤分支难单独观测（她没粉伤技能），锁"分支旗已开"等价物：不消耗即证双开。
    }

    // ════════════════════════════════════════════════════════════════
    // D. 标记宠下场：真伤分支单开 + 消耗 1 层；场下致死保留 1 点
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> p0 = {artemis_pet(), plain_pet(), plain_pet(),
                                    plain_pet(), plain_pet(), plain_pet()};
        std::array<ElfPet, 6> p1 = {plain_pet(1000), plain_pet(), plain_pet(),
                                    plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(p0, p1);
        b.start();
        b.ctx().seerRobot[0].elfPets[0].soulmark_storage[kHuntSoulKey] = 2;
        // 对手主动换人：标记宠（0 号）下场 → 体力停在 400（上限归还后回 1000，手动压回）。
        // ⚠️ 首回合挑战方中切缓冲到房主输入到达才应用（2026-09-22 房主先登场规则）
        //    → step mode 停在"切换已应用、回合未结束"的窗口内断言。
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.ctx().debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.step_past(State::OPERATION_PROTECTION_MECHANISM_1);
        CHECK_EQ(b.ctx().on_stage[1], 1, "D 前置：对手 1 号在场");
        CHECK_EQ(max_hp_of(b.ctx(), 1, 0), 400,
                 "D 前置：换宠发生在回合开始前，归还尚未发生（仍 -60%）");
        b.ctx().seerRobot[1].elfPets[0].hp = 1;   // 场下标记宠 1 血：真伤致死 → 保留 1 点
        // 回合开始判定：对手在场(1号)体力 > 标记宠(0号)体力 1 → 只开真伤分支 → 消耗 1 层。
        // ⚠️ 双输入已在 step 段收集完毕 → 直接一驱到底（重复提交会被拒绝）。
        b.ctx().debug_step_mode = false;
        b.drive();
        CHECK_EQ(hunt_soul_of(b.ctx(), 0, 0), 1,
                 "D：单分支 → 消耗 1 层狩杀之魂");
        CHECK_EQ(b.ctx().seerRobot[1].elfPets[0].hp, 1,
                 "D：场下标记宠吃威力值真伤致死 → 保留 1 点体力");
        CHECK_EQ(max_hp_of(b.ctx(), 1, 0), 1000, "D：回合末上限归还（下场宠也照还）");
    }

    // ════════════════════════════════════════════════════════════════
    // E. 标记宠两管同时在 → 她免异常
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> p0 = {artemis_pet(), plain_pet(), plain_pet(),
                                    plain_pet(), plain_pet(), plain_pet()};
        std::array<ElfPet, 6> p1 = {plain_pet(1000), plain_pet(), plain_pet(),
                                    plain_pet(), plain_pet(), plain_pet()};
        ScenarioBattle b(p0, p1);
        b.start();
        CHECK(b.ctx().is_immune(0, ImmunityType::ANOMALY, State::BATTLE_ROUND_START),
              "E：标记宠盾罩同时在 → 她免异常");
        // 罩被打穿后 → 条件失效 → 不再免疫（条件票实时判）。
        b.ctx().getPet(1).hood_bank_.clear();
        CHECK(!b.ctx().is_immune(0, ImmunityType::ANOMALY, State::BATTLE_ROUND_START),
              "E：罩没了 → 不再免疫（两管同时在才免）");
    }

    std::cout << "scenario_099_artemis_4745: all groups passed" << std::endl;
    return 0;
}
