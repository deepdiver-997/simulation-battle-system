// 场景 4120 —— K256 对手攻击miss→冻伤族（346，soul_lib/k256_miss_frostbite.cpp）
//（2026-10-21，worktree ../agents/k256；号段 4120-4129 → T4120-T4123 四块）
//
// 断言口径（346「若对手攻击miss则对手冻伤」）：
//   - 门开：对手（1 方，攻方）攻击技被印主（0 方，守方）闪避 → 必 miss
//     → 对手被冻伤（has_active_abnormal_status(1, Frostbite=5)）。
//   - 门闭：对手攻击**命中** → 不冻伤。
//   - 属性技 miss 不触发：官方文本是"对手**攻击**miss"——对手用属性技被打闪
//     → 事件仍发，但属性技门应把它滤掉 → 不冻伤。
//   - 自己 miss 不触发：印主自己攻击 miss → 对手不被冻伤（只认 ev.actor==对手）。
//
// 造 miss 手法：守方 ws.dodge_rate[0] = 1.0（闪避 100%）+ 攻方 accuracy=1
//   → 命中率 = 1 - 100 ≤ 0 → 必 miss（场景 040 C7 / 055 K4 同款确定性手法）。
//   dodge_rate 在对手行动前写入（ws 每回合 reset 后不再清）。
#include <iostream>
#include <vector>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kJi = 10001;     // 撞击：物理攻击技载体
constexpr int kSuoTou = 20001; // 缩头：属性技载体
constexpr int kFrostbite = static_cast<int>(AbnormalStatusId::Frostbite);  // 5

ElfPet mark_pet() {
    // 行参原序 [1,5]（实现侧不消费，仅记档）。
    std::vector<int> args{1, 5};
    ElfPet p = make_pet(1, "印主", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/300, /*atk=*/1, /*hp=*/99999,
                        /*soul_mark=*/SoulMark(346, "K256 魂印", "",
                                               EffectArgs(std::move(args))));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// 攻方：accuracy=1（1% 命中）→ 守方闪避 100% 时必 miss。
// accuracy=100（100% 命中）→ 对照组"对手攻击必命中"用（门闭用例）。
ElfPet foe_pet(int skill_id, int accuracy) {
    ElfPet p = make_pet(2, "对手", {skill_id, kJi, kJi, kJi, kJi},
                        /*speed=*/100, /*atk=*/1, /*hp=*/99999);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = accuracy;
    }
    return p;
}

// 推进到对手行动开始，然后给守方写 100% 闪避 → 对手这一击必 miss。
void arm_dodge(ScenarioBattle& b) {
    BattleContext& ctx = b.ctx();
    ctx.debug_step_mode = true;
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    ctx.debug_step_mode = false;
    b.step_until(State::BATTLE_SECOND_ACTION_START);
    ctx.ws.dodge_rate[0] = 1.0f;   // 守方（印主）闪避 100%
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T4120 门开：对手攻击 miss → 对手被冻伤 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet(kJi, 1)));
        b.start();
        arm_dodge(b);
        BattleContext& ctx = b.ctx();
        CHECK(ctx.has_active_abnormal_status(1, kFrostbite),
              "T4120 346：对手攻击 miss → 对手被冻伤");
        std::cout << "  [T4120] 门开冻伤 通过\n";
    }

    // ══ T4121 门闭：对手攻击命中 → 不冻伤 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet(kJi, 100)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        ctx.debug_step_mode = false;
        b.drive();
        CHECK(!ctx.has_active_abnormal_status(1, kFrostbite),
              "T4121 346：对手攻击命中 → 不冻伤（门闭对照）");
        std::cout << "  [T4121] 门闭对照 通过\n";
    }

    // ══ T4122 对手属性技 miss → 不冻伤（属性技门） ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet(kSuoTou, 1)));
        b.start();
        arm_dodge(b);
        BattleContext& ctx = b.ctx();
        CHECK(!ctx.has_active_abnormal_status(1, kFrostbite),
              "T4122 346：对手属性技 miss → 不冻伤（只认攻击技）");
        std::cout << "  [T4122] 属性技门对照 通过\n";
    }

    // ══ T4123 印主自己 miss → 对手不被冻伤 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet(kJi, 100)));
        b.start();
        BattleContext& ctx = b.ctx();
        // 印主改用 1% 命中（大概率 miss），对手 100% 命中（必命中）→
        // 印主的未命中事件 actor==0，应被滤掉 → 对手不被冻伤。
        ctx.getPet(0).skills[0].accuracy = 1;
        ctx.debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        ctx.debug_step_mode = false;
        b.drive();
        CHECK(!ctx.has_active_abnormal_status(1, kFrostbite),
              "T4123 346：印主自己行动 → 对手不被冻伤（只认 ev.actor==对手）");
        std::cout << "  [T4123] 攻击方门对照 通过\n";
    }

    std::cout << "scenario_4120 PASS (K256 对手攻击miss→冻伤族)" << std::endl;
    return 0;
}
