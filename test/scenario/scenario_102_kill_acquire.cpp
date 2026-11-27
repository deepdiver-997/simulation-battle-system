// 场景 102 —— 击杀效果获取框架（原 091，与并行的 091_shenjue_mistike 撞号 → 改号）：阿尔忒弥斯 4745 / 咤克斯 2378 主体化 / 莫塔里安 4541 / 灵巢之主 4599
//
// 锁的是 2026-09-20 的击杀效果设计（口径见
// docs_local/docs/06-交接/魂印与技能机制-交接.md"击杀效果与主体绑定"节）：
//   A.  2378 击败+2 主体化：咤在场击杀 → +2 层记在咤自己槽（槽级归因 last_damage_actor）
//   A2. 咤在场下时击败+2 不触发（STAGE 节点不在桶——"自身击败"语义；旧 watcher 边级
//       口径的回归）
//   B.  阿尔忒弥斯登场扫描：场下咤的击败节点被复制注册（独立 source_id 命名空间），
//       她击杀 → +2 层落**她的** storage；狩杀之魂 +1 层（一种击败类效果）
//   C.  消逝断供：咤被消逝 → EVENT_VANISH 推送清副本 → 她击杀不再 +2
//   D.  莫塔里安自身：登场 300 护罩；击杀 → 重触发登场效果（护罩刷回 300）+ 挡下次
//       受到的攻击（DAMAGE 票 counts=1）
//   D2. 阿尔忒弥斯 + 场下莫塔里安：她击杀 → 重触发的是**她自己的**登场效果（狩杀之魂
//       重计，且她**没有**护罩）+ 挡下次攻击直接给她（无依赖效果直接转移）
//   E.  灵巢之主：击败时被击败者的异常 + 能力下降转移给对手下一只登场精灵
//
// 常量 990474501（狩杀之魂）/ 930000000（副本 source_id 基底）见 soul_lib/artemis_4745.cpp。

#include <entities/pet_factory.h>
#include "../../resources/curse_stack.h"
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kSkill = 10001;      // 撞击（低威力）
constexpr int kZha = 2378;
constexpr int kArtemis = 2354;  // effect_id（2026-09-27 对齐）
constexpr int kMortarion = 2142;  // effect_id（2026-09-27 对齐）
constexpr int kLingchao = 2193;  // effect_id（2026-09-27 对齐）
constexpr int kHuntSoulKey = 990474501;
constexpr int kAcquiredSourceBase = 930000000;

std::array<int, 5> plain_skills() { return {kSkill, kSkill, kSkill, kSkill, kSkill}; }

// 击杀者：速度 200 抢先手、攻 200 对 hp=1 的靶子必杀。
ElfPet killer_pet(int mark_id, const char* name, int hp = 2000) {
    return make_pet(mark_id, name, plain_skills(), /*speed=*/200, /*atk=*/200, hp,
                    SoulMark(mark_id, name, "kill-acquire 场景"));
}

ElfPet plain_pet(int hp) {
    return make_pet(1, "靶子", plain_skills(), /*speed=*/50, /*atk=*/1, hp,
                    SoulMark(999, "靶子", "plain"));
}

std::array<ElfPet, 6> victim_party() {
    return {plain_pet(1), plain_pet(1), plain_pet(1), plain_pet(1), plain_pet(1), plain_pet(1)};
}

int curse_of(BattleContext& x, int side, int slot) {
    return get_curse_stacks_slot(&x, side, slot);
}

int hunt_soul_of(BattleContext& x, int side, int slot) {
    const auto& storage = x.seerRobot[side].elfPets[slot].soulmark_storage;
    auto it = storage.find(kHuntSoulKey);
    return it == storage.end() ? 0 : std::any_cast<int>(it->second);
}

bool has_acquired_copy(BattleContext& x, int side) {
    for (const State st : {State::BATTLE_OPPONENT_DEFEAT_STATUS,
                           State::BATTLE_AFTER_DEFEATING_OPPONENT}) {
        for (const auto& [key, e] : x.soul_mark_effects.at(st, side)) {
            (void)key;
            if (e->source_id_ >= kAcquiredSourceBase) {
                return true;
            }
        }
    }
    return false;
}

// 一轮"攻方击杀守方在场 → 守方补位"的标准流程；返回时停在下一回合 CHOOSE。
void round_kill(ScenarioBattle& b, int dead_side_replacement_slot) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();  // 击杀 → 停在 CHOOSE_AFTER_DEATH（守方）
    CHECK_EQ(b.state(), State::CHOOSE_AFTER_DEATH, "前置：应停在守方选替补");
    b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, dead_side_replacement_slot);
    b.drive();  // 补位登场 → 打完本回合剩余时点
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ════════════════════════════════════════════════════════════════
    // A. 咤克斯在场击杀 → +2 层记自己槽
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> p0 = {killer_pet(kZha, "咤克斯"), plain_pet(2000), plain_pet(2000),
                                    plain_pet(2000), plain_pet(2000), plain_pet(2000)};
        ScenarioBattle b(p0, victim_party());
        b.start();
        CHECK_EQ(curse_of(b.ctx(), 0, 0), 0, "A 前置：战斗开始无瞬杀特性 → 0 层");
        round_kill(b, 1);
        CHECK_EQ(curse_of(b.ctx(), 0, 0), 2, "A：咤击败对手 → +2 层（击败时点节点）");
    }

    // ════════════════════════════════════════════════════════════════
    // A2. 咤在场下：击败+2 不触发（STAGE 节点不在桶）
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> p0 = {killer_pet(1, "普通攻手"), killer_pet(kZha, "咤克斯"),
                                    plain_pet(2000), plain_pet(2000), plain_pet(2000),
                                    plain_pet(2000)};
        ScenarioBattle b(p0, victim_party());
        b.start();
        round_kill(b, 1);
        CHECK_EQ(curse_of(b.ctx(), 0, 1), 0, "A2：咤场下 → 队友击败不给他层数（自身击败语义）");
    }

    // ════════════════════════════════════════════════════════════════
    // B. 阿尔忒弥斯 + 场下咤：副本注册 + +2 落她 + 狩杀之魂 +1
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> p0 = {killer_pet(kArtemis, "阿尔忒弥斯"), killer_pet(kZha, "咤克斯"),
                                    plain_pet(2000), plain_pet(2000), plain_pet(2000),
                                    plain_pet(2000)};
        ScenarioBattle b(p0, victim_party());
        b.start();
        CHECK(has_acquired_copy(b.ctx(), 0), "B：登场扫描应复制场下咤的击败节点");
        CHECK_EQ(hunt_soul_of(b.ctx(), 0, 0), 1, "B：一种击败类效果 → 狩杀之魂 1 层");
        round_kill(b, 1);
        CHECK_EQ(curse_of(b.ctx(), 0, 0), 2, "B：她的击杀 → +2 层落她自己（主体绑定）");
        CHECK_EQ(curse_of(b.ctx(), 0, 1), 0, "B：咤本体不拿层（副本以她为主体）");
    }

    // ════════════════════════════════════════════════════════════════
    // C. 消逝断供：咤被消逝 → 推送清副本 → 她的击杀不再 +2
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> p0 = {killer_pet(kArtemis, "阿尔忒弥斯"), killer_pet(kZha, "咤克斯"),
                                    plain_pet(2000), plain_pet(2000), plain_pet(2000),
                                    plain_pet(2000)};
        ScenarioBattle b(p0, victim_party());
        b.start();
        CHECK_EQ(vanish_spirit(&b.ctx(), 0, 1, /*actor=*/-1), VanishResult::VANISHED,
                 "C 前置：消逝场下咤");
        round_kill(b, 1);
        CHECK(!has_acquired_copy(b.ctx(), 0), "C：EVENT_VANISH 推送清副本");
        CHECK_EQ(curse_of(b.ctx(), 0, 0), 0, "C：消逝断供 → 她的击杀不再获得咒怨");
        CHECK_EQ(hunt_soul_of(b.ctx(), 0, 0), 1, "C：已获得的狩杀之魂不追回（登场获得口径）");
    }

    // ════════════════════════════════════════════════════════════════
    // D. 莫塔里安自身：登场护罩 → 击杀重触发（护罩刷回）+ 挡下次攻击
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> p0 = {killer_pet(kMortarion, "莫塔里安"), plain_pet(2000),
                                    plain_pet(2000), plain_pet(2000), plain_pet(2000),
                                    plain_pet(2000)};
        ScenarioBattle b(p0, victim_party());
        b.start();
        ElfPet& mor = b.pet(0);
        CHECK_EQ(mor.hood_bank_.total(), 300, "D：登场 300 点护罩");
        mor.hood_bank_.absorb(300);   // 消耗掉，验证击杀重触发会重新获得
        CHECK_EQ(mor.hood_bank_.total(), 0, "D 前置：护罩已耗尽");
        round_kill(b, 1);
        CHECK_EQ(b.pet(0).hood_bank_.total(), 300, "D：击败 → 重触发登场效果 → 护罩刷回 300");
        CHECK(b.ctx().is_immune(0, ImmunityType::DAMAGE, State::BATTLE_ROUND_START),
              "D：击败 → 挡下次受到的攻击（DAMAGE 票在）");
    }

    // ════════════════════════════════════════════════════════════════
    // D2. 阿尔忒弥斯 + 场下莫塔里安：重触发她的登场效果 + 挡伤直接给她
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> p0 = {killer_pet(kArtemis, "阿尔忒弥斯"), killer_pet(kMortarion, "莫塔里安"),
                                    plain_pet(2000), plain_pet(2000), plain_pet(2000),
                                    plain_pet(2000)};
        ScenarioBattle b(p0, victim_party());
        b.start();
        CHECK_EQ(b.pet(0).hood_bank_.total(), 0, "D2 前置：她没有登场护罩（那是莫塔里安的登场效果）");
        round_kill(b, 1);
        // 重触发"她自己的登场效果" = acquisition 节点重跑 → 狩杀之魂重计（仍 1 种）。
        CHECK_EQ(hunt_soul_of(b.ctx(), 0, 0), 1, "D2：重触发的是她的登场效果（狩杀之魂重计）");
        CHECK_EQ(b.pet(0).hood_bank_.total(), 0, "D2：她不获得莫塔里安的护罩");
        CHECK(b.ctx().is_immune(0, ImmunityType::DAMAGE, State::BATTLE_ROUND_START),
              "D2：挡下次攻击无依赖 → 直接给她");
    }

    // ════════════════════════════════════════════════════════════════
    // E. 灵巢之主：被击败者的异常 + 能力下降 → 转移给对手下一只登场
    // ════════════════════════════════════════════════════════════════
    {
        std::array<ElfPet, 6> p0 = {killer_pet(kLingchao, "索杰德尔"), plain_pet(2000),
                                    plain_pet(2000), plain_pet(2000), plain_pet(2000),
                                    plain_pet(2000)};
        ScenarioBattle b(p0, victim_party());
        b.start();
        // 给守方在场宠挂异常（3 回合）+ 攻击 -2，模拟"被击败时所处的状态"。
        b.ctx().apply_abnormal_status_for_rounds(1, /*status_id=*/1, /*rounds=*/3);
        b.ctx().ability_level(1, /*stat=*/0) = -2;
        round_kill(b, 1);
        CHECK_EQ(b.ctx().on_stage[1], 1, "E 前置：守方补位到 1 号槽");
        CHECK(b.ctx().has_active_abnormal_status(1, 1), "E：异常状态转移给下只出战精灵");
        CHECK_EQ(b.ctx().ability_level(1, 0), -2, "E：能力下降转移给下只出战精灵");
    }

    std::cout << "scenario_102_kill_acquire: all groups passed" << std::endl;
    return 0;
}
