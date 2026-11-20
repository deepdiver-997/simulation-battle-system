// 场景 962 —— S1-P0 命中/闪避钩子收尾：ws.hit_rate_mod_attack 攻击技门控乘区
//（2026-10-01 接入；消费点 = skills.cpp query_usage 命中判定 !is_attribute 分段）
//
// 背景：1314/1576/1673/1843 族魂印"对手使用攻击技能命中率降低X%"需要**带技能类型
// 维度**的乘区——hit_rate_mod 无类型维度（H5 记档：78 物理限定曾被迫特判）。
// 新槽 hit_rate_mod_attack 只在攻击技命中判定中乘算；属性技不受影响。
//
// 断言组（用 0.0 = 必 miss 的确定性极值做门控证明）：
//   T1 门控：对手攻击侧压 0.0 → 本回合对手攻击必 miss（命中率 roll 对 0 必败，确定性）。
//   T2 默认对照：1.0 → 攻击照常命中（零行为）。
//
// ⚠️ 注入时机（首跑踩坑记档）：resetWorkspace 在每回合 ROUND_START 状态里执行——
// drive 前预写的 0.0 会在回合开始的 reset 中被冲掉。正确姿势 = 双操作提交后
// debug_step 停泊、step_past(BATTLE_ROUND_START)（reset 已跑完）再写槽、继续 drive。
// ⚠️ PP：大地战栗 max_pp=5，20 回合压测会 PP 耗尽停手——开局把对手 slot0 PP 顶满。
#include <array>
#include <functional>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kDaZhanLi = 12223;   // 大地战栗：特攻140 命中95（攻击技载体）
constexpr int kPaDa = 10004;       // 拍打 filler

ElfPet self_pet(const std::array<int, 5>& skills, int speed, int atk, int hp) {
    ElfPet p = make_pet(1, "己方", skills, speed, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;            // ⚠️ 本场景测命中率，必关必中
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet(const std::array<int, 5>& skills, int speed, int atk, int hp) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// 提交双方操作；对手侧停泊（debug_step）→ step 过 ROUND_START（reset 已跑完）→
// 由调用方注入 ws → 继续 drive。
void round_with_injection(ScenarioBattle& b, int self_slot, int foe_slot,
                          const std::function<void(BattleContext&)>& inject) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.ctx().debug_step_mode = true;
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_slot);
    b.ctx().debug_step_mode = false;
    b.step_past(State::BATTLE_ROUND_START);
    inject(b.ctx());
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ════════════════════════════════════════════════════════════════
    // T1 门控：对手攻击侧压 0.0 → 本回合攻击必 miss
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/100, /*atk=*/1, /*hp=*/5000)),
                         make_party(foe_pet({kDaZhanLi, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*speed=*/300, /*atk=*/10, /*hp=*/100000)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(1).skills[0].pp = 99;   // 防 PP 耗尽停手
        const int hp_before = ctx.getPet(0).hp;
        round_with_injection(b, 0, 0, [](BattleContext& c) {
            c.ws.hit_rate_mod_attack[1] = 0.0f;   // 压 0：命中率 roll 对 0 必败
        });
        CHECK_EQ(ctx.getPet(0).hp, hp_before, "T1 核心：攻击技命中率乘 0 → 必 miss（零掉血）");
        std::cout << "  [T1] 攻击技门控必 miss 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // T2 默认对照：1.0 → 攻击照常命中（零行为）
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/100, /*atk=*/1, /*hp=*/5000)),
                         make_party(foe_pet({kDaZhanLi, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*speed=*/300, /*atk=*/10, /*hp=*/100000)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(1).skills[0].pp = 99;
        CHECK_EQ(ctx.ws.hit_rate_mod_attack[1], 1.0f, "T2 前提：默认 1.0（无行为）");
        const int hp_before = ctx.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(ctx.getPet(0).hp < hp_before, "T2 对照：默认 1.0 → 攻击照常命中");
        std::cout << "  [T2] 默认零行为对照 通过\n";
    }

    std::cout << "scenario_962 PASS (S1-P0 攻击技门控命中率乘区)" << std::endl;
    return 0;
}
