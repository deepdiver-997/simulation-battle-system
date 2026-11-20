// 场景 5590 —— J228 低体全恢复族（530，moves_lib/j_low_hp_full_heal.cpp）
//（2026-10-22，worktree feat/jbatch；号段 5590-5599）
//
// 断言口径（自身体力绝对值；门用严格小于，边界必须不触发）：
//   - 530 模板：使用时若自身体力低于1/{0}则恢复全部体力值。
//   - a0) 对照：拍打无 530 → 2999 不回血（证明回血不是别处来的）。
//   - a) 15467 绝影剑 [3]：自身 2999 < 9000/3=3000 → 恢复满 9000。
//   - b) 15467 边界：自身恰 3000（等于 1/3）→ "低于"不成立 → 保持 3000。
//   - c) 34125 荆棘斑斓 [2]：自身 4499 < 4500 → 恢复满 9000。
//   - d) 34125 边界：自身恰 4500 → 保持 4500。
//   - e) 17387 天火渡世劫 [4]：自身 2249 < 2250 → 恢复满 9000。
//   - f) 24420 铁血龙盔 [5]：自身 1799 < 1800 → 恢复满 9000。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa   = 10004;  // 拍打：物 40（填充/对照刀）
constexpr int kJueYing = 15467;  // 绝影剑 [530]|[3]（物 80，优先 2）
constexpr int kJingJi  = 34125;  // 荆棘斑斓 [530]|[2]（特 130）
constexpr int kTianHuo = 17387;  // 天火渡世劫 [530]|[4]（特 150）
constexpr int kLongKui = 24420;  // 铁血龙盔 [48,127,530]|[5]（属性技；48/127 无 HP 影响）
constexpr int kSelfMax = 9000;
constexpr int kFoeMax  = 99999;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_self(int skill0) {
    ElfPet p = make_pet(1, "己方", {skill0, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, /*hp=*/kSelfMax);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/kFoeMax);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

// 跑一回合：自身用 skill0，对手 NONE；返回结算后的自身 HP。
int run_round(int skill0, int self_hp) {
    ScenarioBattle b(make_party(mk_self(skill0)), make_party(mk_foe()));
    b.start();
    BattleContext& ctx = b.ctx();
    ctx.getPet(0).hp = self_hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
    return ctx.getPet(0).hp;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5590.a0 对照：拍打无 530 → 不回血 ══
    {
        const int hp = run_round(kPaiDa, 2999);
        CHECK_EQ(hp, 2999, "T5590.a0 对照：拍打不回血（实测 " + std::to_string(hp) + "）");
        std::cout << "  [T5590.a0] 对照组通过\n";
    }

    // ══ T5590.a 15467 [3]：2999 < 3000 → 满血 9000 ══
    {
        const int hp = run_round(kJueYing, 2999);
        CHECK_EQ(hp, kSelfMax, "T5590.a 15467 低体 → 恢复满（实测 " + std::to_string(hp) + "）");
        std::cout << "  [T5590.a] 1/3 低体全恢复通过\n";
    }

    // ══ T5590.b 15467 边界：恰 3000 → 门闭 ══
    {
        const int hp = run_round(kJueYing, 3000);
        CHECK_EQ(hp, 3000, "T5590.b 15467 边界（等于 1/3）不触发（实测 " + std::to_string(hp) + "）");
        std::cout << "  [T5590.b] 1/3 边界门闭通过\n";
    }

    // ══ T5590.c 34125 [2]：4499 < 4500 → 满血 9000 ══
    {
        const int hp = run_round(kJingJi, 4499);
        CHECK_EQ(hp, kSelfMax, "T5590.c 34125 低体 → 恢复满（实测 " + std::to_string(hp) + "）");
        std::cout << "  [T5590.c] 1/2 低体全恢复通过\n";
    }

    // ══ T5590.d 34125 边界：恰 4500 → 门闭 ══
    {
        const int hp = run_round(kJingJi, 4500);
        CHECK_EQ(hp, 4500, "T5590.d 34125 边界（等于 1/2）不触发（实测 " + std::to_string(hp) + "）");
        std::cout << "  [T5590.d] 1/2 边界门闭通过\n";
    }

    // ══ T5590.e 17387 [4]：2249 < 2250 → 满血 9000 ══
    {
        const int hp = run_round(kTianHuo, 2249);
        CHECK_EQ(hp, kSelfMax, "T5590.e 17387 低体 → 恢复满（实测 " + std::to_string(hp) + "）");
        std::cout << "  [T5590.e] 1/4 低体全恢复通过\n";
    }

    // ══ T5590.f 24420 [5]：1799 < 1800 → 满血 9000 ══
    {
        const int hp = run_round(kLongKui, 1799);
        CHECK_EQ(hp, kSelfMax, "T5590.f 24420 低体 → 恢复满（实测 " + std::to_string(hp) + "）");
        std::cout << "  [T5590.f] 1/5 低体全恢复通过\n";
    }

    std::cout << "scenario_5590 PASS (J228 低体全恢复族 530)" << std::endl;
    return 0;
}
