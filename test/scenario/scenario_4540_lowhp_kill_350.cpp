// 场景 4540 —— K298 低体瞬杀攻击门族（350，soul_lib/k298_lowhp_kill.cpp）
//（2026-10-21，worktree ../agents/k298；号段 4540-4549 → T4540-T4543 四块）
//
// 断言口径（350「对手体力少于200时，使用攻击技能直接秒杀对手」）：
//   - T4540 **门开**：对手体力 199（< 200）→ 印主用攻击技 → 对手被秒杀（hp=0）。
//   - T4541 **边界**：对手体力**恰在** 200 → 门闭（tips 字面"少于200"是严格小于；
//     恰好 200 不触发）。
//   - T4542 **攻击技门**：对手体力 199 但印主用**属性技** → 不秒杀（tips 明写
//     "使用攻击技能"）。
//   - T4543 **门闭对照**：对手体力 201（≥ 200）用攻击技 → 不秒杀。
//
// ⚠️ 场景构造口径：按 **DB args 原样**传 {2,200,5}（不补 {0,0} 前缀）——实现侧走
//   owned_int_args 原序读（K257 起的夜跑口径，见实现头注「行参读取路径」）。
// ⚠️ **对手体力要压到 200 以下**：直接写 ctx.getPet(1).hp（真实 hp，秒杀门读的是
//   当前体力而非护盾抵扣后的值）。
// ⚠️ 秒杀 = force_hp_to_zero → 对手 hp 恰为 0（精确断言，不带浮动）。
#include <cstdlib>
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

constexpr int kJi = 10001;     // 撞击：攻击技载体
constexpr int kSuoTou = 20004;  // 瞪眼：属性技载体
constexpr int kThreshold = 200; // db[1]：绝对门限

SoulMark make_mark() {
    // DB args 原序 [2,200,5]：db[1]=200（"体力少于200"自证）；db[0]/db[2] 记档不消费。
    SoulMark sm(350, "K298 魂印", "test", EffectArgs(std::vector<int>{2, 200, 5}));
    sm.monster_id = 2723;
    return sm;
}

// 印主：技能 0 可切成攻击技/属性技。
ElfPet mark_pet(int skill_id = kJi) {
    ElfPet p = make_pet(1, "幻世帝天", {skill_id, kJi, kJi, kJi, kJi},
                        /*speed=*/300, /*atk=*/1, /*hp=*/99999, make_mark());
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_pet() {
    ElfPet p = make_pet(2, "对手", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/100, /*atk=*/1, /*hp=*/99999);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// 印主出一技；对手体力压到 foe_hp → 返回对手最终体力。
int mark_uses(ScenarioBattle& b, int foe_hp, int skill_id = 0) {
    BattleContext& ctx = b.ctx();
    ctx.getPet(1).hp = foe_hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, skill_id);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
    return ctx.getPet(1).hp;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T4540 门开：对手体力 199 < 200 → 秒杀 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        const int hp = mark_uses(b, /*foe_hp=*/kThreshold - 1);
        CHECK_EQ(hp, 0,
                 "T4540 350：对手体力 199（< 200）→ 印主用攻击技能直接秒杀对手");
        std::cout << "  [T4540] 低体门开秒杀 通过\n";
    }

    // ══ T4541 边界：对手体力恰在 200 → 门闭 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        const int hp = mark_uses(b, /*foe_hp=*/kThreshold);
        CHECK(hp > 0,
              "T4541 350：对手体力恰在 200 → 门闭（'少于200'是严格小于，不含等于），"
              "实测体力 " + std::to_string(hp));
        std::cout << "  [T4541] 边界门闭 通过\n";
    }

    // ══ T4542 攻击技门：属性技不秒杀 ══
    {
        ElfPet p = mark_pet(/*skill_id=*/kSuoTou);   // 槽 0 = 属性技
        ScenarioBattle b(make_party(p), make_party(plain_pet()));
        b.start();
        const int hp = mark_uses(b, /*foe_hp=*/kThreshold - 1);
        CHECK(hp > 0,
              "T4542 350：用属性技 → 不秒杀（tips 明写'使用攻击技能'），实测体力 "
                  + std::to_string(hp));
        std::cout << "  [T4542] 攻击技门对照 通过\n";
    }

    // ══ T4543 门闭对照：对手体力 201 ≥ 200 → 不秒杀 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        const int hp = mark_uses(b, /*foe_hp=*/kThreshold + 1);
        CHECK(hp > 0,
              "T4543 350：对手体力 201（≥ 200）→ 门闭不秒杀，实测体力 "
                  + std::to_string(hp));
        std::cout << "  [T4543] 门闭对照 通过\n";
    }

    std::cout << "scenario_4540 PASS (K298 低体瞬杀攻击门族)" << std::endl;
    return 0;
}
