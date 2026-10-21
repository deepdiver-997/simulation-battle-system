// examples/public/demo_scenario.cpp —— 公开示例：一个完整可跑的最小闭环
//
// 场景：自制魂印「登场时给自己加 10 点护盾」（见 demo_mark.h）
//   R1：持有示例魂印的精灵登场 → 护盾 10 落地；
//   R2：对手打一刀 → 伤害被护盾吸收 → 本体体力零变化；
//   R3：无魂印对照 → 同样一刀 → 本体体力真的掉。
//
// 整个场景**不查官方数据库**（技能/面板手工装配、魂印程序手动注册），
// 公开检出 clone 下来即可编可跑。
// 断言用 examples/public/public_harness.h 的 check_eq / check_true。
#include <iostream>

#include "bare_pet.h"
#include "demo_mark.h"
#include "public_harness.h"

using namespace examples;

namespace {

ElfPet pet_with_demo_mark(int id, const std::string& name) {
    std::array<Skills, 5> skills{
        skill_bare(10001, "撞击", /*power=*/100),
        skill_bare(10001, "撞击", 100),
        skill_bare(10001, "撞击", 100),
        skill_bare(10001, "撞击", 100),
        skill_bare(10001, "撞击", 100),
    };
    SoulMark mark(kDemoMarkId, "示例魂印", "演示：登场获得 10 点护盾");
    return pet_bare(id, name, skills, /*atk=*/100, /*hp=*/1000, /*speed=*/200, std::move(mark));
}

ElfPet plain_pet(int id, const std::string& name) {
    std::array<Skills, 5> skills{
        skill_bare(10001, "撞击", 100), skill_bare(10001, "撞击", 100),
        skill_bare(10001, "撞击", 100), skill_bare(10001, "撞击", 100),
        skill_bare(10001, "撞击", 100),
    };
    return pet_bare(id, name, skills, /*atk=*/100, /*hp=*/1000, /*speed=*/100);
}

}  // namespace

int main() {
    // 注册示例魂印程序（公开 API，不依赖插件动态库/数据库）。
    register_demo_mark();

    // ── 第一段：带示例魂印的精灵，护盾吸收对手一击 ──────────────────────────
    {
        Battle b(party_of(pet_with_demo_mark(1, "护盾方")),
                 party_of(plain_pet(2, "攻击方")));
        b.start();
        // R1：登场时点护盾已落地（early 节点在选择操作前执行）。
        check_eq(b.ctx().getPet(0).shield_bank_.total(), 10,
                 "示例魂印：登场获得 10 点护盾");
        // R2：对手出撞击（伤害 111）→ 护盾吸 10 → 剩 101 落地 → 体力掉 101。
        const int hp_before = b.ctx().getPet(0).hp;
        b.submit(0, BattleFsm::ActionType::NONE, -1);
        b.submit(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        check_eq(b.ctx().getPet(0).hp, hp_before - 101,
                 "示例魂印：护盾吸收 10 点，其余 101 点落地");
        check_eq(b.ctx().getPet(0).shield_bank_.total(), 0,
                 "示例魂印：护盾被这一刀打空");
    }

    // ── 第二段：无魂印对照，同样一刀 → 体力真的掉 ─────────────────────────
    {
        Battle b(party_of(plain_pet(1, "无印方")), party_of(plain_pet(2, "攻击方")));
        b.start();
        const int hp_before = b.ctx().getPet(0).hp;
        b.submit(0, BattleFsm::ActionType::NONE, -1);
        b.submit(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        check_true(b.ctx().getPet(0).hp < hp_before, "对照：无魂印时体力确实下降");
    }

    std::cout << "examples/public demo PASS（断言 " << check_count() << " 条）" << std::endl;
    return 0;
}
