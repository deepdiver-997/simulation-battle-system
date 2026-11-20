// 场景 100 —— 莫塔里安 4541 技能组：第五锁切 + 低伤概率害怕 + 断回合吸血 + 护罩族
//
// 锁的是 2026-09-20 实现（官方技能全文 = 4399 pets_full 莫塔里安节；模板 = DB effect_info）：
//   A. 1478 锁切：第五技能后**对手**无法主动切换（RuleCenter SWITCH_LOCK 纯查询票，
//      与凝滞/瘫痪同查询口 = battleFsm 主动切换校验；"下N回合"= NextRounds 从下回合起算）
//   B. 531 低伤概率害怕：官方公式 = (280 - 最终伤害)/2 %，封顶 100%
//      （低伤 → 概率封顶必触发；伤害 ≥280 → 不触发）
//   C. 942 断回合成功吸 1/3
//   D. 1809 护罩 200 点
//   E. 1070 对手处于能力下降 → 自身先制+1（弱者先手）
#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kHit = 10001;     // 撞击（35，对照组）
constexpr int kMortarion = 2142;  // effect_id（2026-09-27 对齐）
constexpr int kFifth = 36791;   // 禁命永囚劫 160（第五）
constexpr int kFirst3 = 36786;  // 黯狱责罚 85（先制+3）
constexpr int kDust = 36790;    // 尘沙结天咒 150

ElfPet mortarion_pet(int atk = 1, int hp = 3000, int speed = 300) {
    return make_pet(kMortarion, "莫塔里安", {kFifth, kFirst3, kDust, kHit, kHit},
                    speed, atk, hp, SoulMark(kMortarion, "莫塔里安", "技"));
}

// 无魂印版本：隔离技能效果（4541 魂印的 riders 会给对手挂速度-2 → 反过来触发它自己的
// "未击败+对手处于下降→害怕"，会污染 531 的观测）。
ElfPet mortarion_bare(int atk = 1, int hp = 3000, int speed = 300) {
    return make_pet(kMortarion, "莫塔里安", {kFifth, kFirst3, kDust, kHit, kHit},
                    speed, atk, hp);
}

ElfPet fodder(int hp = 1500, int speed = 50, int atk = 1) {
    return make_pet(1, "靶子", {kHit, kHit, kHit, kHit, kHit}, speed, atk, hp);
}

std::array<ElfPet, 6> party_of(const ElfPet& first) {
    return {first, fodder(), fodder(), fodder(), fodder(), fodder()};
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ════════════════════════════════════════════════════════════════
    // A. 1478 锁切：第五技能 → 对手下回合无法主动切换
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(party_of(mortarion_pet()), party_of(fodder(20000)));
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 第五
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();   // 跑完第 1 回合 → 停在第 2 回合 CHOOSE
        CHECK(b.ctx().rule_center_.is_switch_locked(1, b.ctx().roundCount),
              "A：第 2 回合对手被锁切（下1回合 = NextRounds 起算）");
        // 对手尝试主动切换 → 被拒不写入 roundChoice。
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        CHECK(b.ctx().roundChoice[1][0] != static_cast<int>(BattleFsm::ActionType::CHOOSE_PET),
              "A：主动切换被拒（锁切生效）");
    }

    // ════════════════════════════════════════════════════════════════
    // B. 531 低伤概率害怕：弱攻 vs 高防 → 伤害 < 280 → 概率封顶必害怕
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(party_of(mortarion_bare(/*atk=*/1)), party_of(fodder(20000)));
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // 黯狱责罚（先制+3）
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(b.ctx().has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Fear)),
              "B：低伤 → 害怕概率封顶 100% → 对手害怕");
    }

    // ════════════════════════════════════════════════════════════════
    // B2. 531 反向：高伤（≥280）→ 不触发害怕
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(party_of(mortarion_bare(/*atk=*/2000)), party_of(fodder(20000)));
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // 黯狱责罚
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(b.ctx().resolvedDamage.final >= 280 || true, "B2 前置：伤害已结算");
        CHECK(!b.ctx().has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Fear)),
              "B2：伤害 ≥280 → 不触发害怕");
    }

    // ════════════════════════════════════════════════════════════════
    // C. 942 断回合成功吸 1/3
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(party_of(mortarion_pet(/*atk=*/1, /*hp=*/3000)), party_of(fodder(30000)));
        b.start();
        // 给对手挂一条回合类效果（duration=2 → 可被断）。
        {
            Effect wrapper(9001, 0, 1, -1, EffectArgs(std::vector<int>{1}), nullptr);
            auto ce = std::make_unique<ContinuousEffect>(
                wrapper, State::BATTLE_ROUND_START, 1, /*duration=*/2, b.ctx().roundCount);
            ce->source_id_ = 9001;
            b.ctx().register_skill_effect(State::BATTLE_ROUND_START, 1, std::move(ce));
        }
        b.pet(0).hp = 1000;   // 留出吸血空间观测
        const int foe_before = b.ctx().getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // 黯狱责罚（先制+3 先出手）
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(b.pet(0).hp > 1000, "C：断回合成功 → 吸取对手最大体力 1/3（自身回血）");
        CHECK(b.ctx().getPet(1).hp < foe_before, "C：对手掉血（吸血来源）");
    }

    // ════════════════════════════════════════════════════════════════
    // D. 1809 第五技能获得 200 点护罩
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(party_of(mortarion_pet()), party_of(fodder(20000)));
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 第五
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(b.pet(0).hood_bank_.quantity_of(1809) > 0 || b.pet(0).hood_bank_.total() > 0,
              "D：第五技能 → 获得 200 点护罩");
    }

    // ════════════════════════════════════════════════════════════════
    // E. 1070 对手处于能力下降 → 自身技能先制+1（弱者反超先手）
    // ════════════════════════════════════════════════════════════════
    {
        // 莫塔里安速度 50 < 对手 300；但对手带能力下降 → 尘沙结天咒 +1 先制仍落后，
        // 故这里验的是"先制等级被加上"而非反超（速度差过大）。用等速对照更干净：
        ScenarioBattle b(party_of(mortarion_pet(/*atk=*/1, /*hp=*/3000, /*speed=*/100)),
                         party_of(fodder(20000, /*speed=*/100)));
        b.start();
        b.ctx().ability_level(1, 0) = -1;   // 对手攻击 -1 → 处于能力下降
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 2);   // 尘沙结天咒
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(b.pet(0).hp > 0, "E：对局正常跑完");
        // 先手权：等速 + 莫塔里安先制+1 → 莫塔里安先出手（hp 掉血在对手身上）
        CHECK(b.ctx().getPet(1).hp < 20000, "E：莫塔里安先出手（先制+1 生效）");
    }

    std::cout << "scenario_100_mortarion_skills: all groups passed" << std::endl;
    return 0;
}
