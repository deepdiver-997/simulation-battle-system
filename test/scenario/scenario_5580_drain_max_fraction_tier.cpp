// 场景 5580 —— J227 吸取最大体力分档族（1263，moves_lib/j_drain_max_fraction_tier.cpp）
//（2026-10-22，worktree feat/jbatch；号段 5580-5589）
//
// 断言口径（绝对体力；自身回血精确量做主断言，攻击载体的红伤不参与）：
//   - 1263 模板：吸取对手最大体力的1/{0}，自身体力低于对手时变为1/{1}。
//   - a0) 对照：拍打无 1263 → 自身回血 0（证明回血不是别处来的）。
//   - a) 25770 邪能构装 [3,2]，自身 5000 < 对手 10000 → 低体档 2 → 吸 10000/2=5000。
//   - b) 25770，自身 15000 > 对手 12000 → 常规档 3 → 吸 12000/3=4000。
//   - c) 25770，自身 10000 == 对手 10000 → 严格小于不成立 → 常规档 3 → 吸 3333。
//   - d) 31575 幻梦风灵破 [3,2]（攻击技），自身 5000 < 对手 10000 → 低体档 2 →
//        自身精确 +5000（红伤不回血，不污染回血断言）。
//   - e) 31575，自身 15000 > 对手 12000 → 常规档 3 → 自身精确 +4000。
//   - 链兄弟：25770 的 962/843/58/471 与 31575 的 1260 均不改变体力（1260 需自身负等级）。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa    = 10004;  // 拍打：物 40（填充/对照刀）
constexpr int kXieNeng  = 25770;  // 邪能构装 [962,843,58,1263,471]|[1,1,1,3,2,1]（属性技）
constexpr int kHuanMeng = 31575;  // 幻梦风灵破 [1260,1263]|[1,3,2]（特攻 150）
constexpr int kSelfMax  = 20000;
constexpr int kFoeMaxA  = 10000;
constexpr int kFoeMaxB  = 12000;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_self(int skill0, int max_hp) {
    ElfPet p = make_pet(1, "己方", {skill0, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, /*hp=*/max_hp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe(int max_hp) {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/max_hp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5580.a0 对照：拍打无 1263 → 自身不回血，对手只吃红伤 ══
    {
        ScenarioBattle b(make_party(mk_self(kPaiDa, kSelfMax)), make_party(mk_foe(kFoeMaxA)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 5000;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(0).hp, 5000,
                 "T5580.a0 对照：拍打不回血（self hp 实测 " +
                     std::to_string(ctx.getPet(0).hp) + "，应 5000）");
        CHECK(ctx.getPet(1).hp < kFoeMaxA,
              "T5580.a0 对照：红伤确实落地（foe hp 实测 " +
                  std::to_string(ctx.getPet(1).hp) + "）");
        std::cout << "  [T5580.a0] 对照组通过\n";
    }

    // ══ T5580.a 25770 [3,2]：自身 5000 < 对手 10000 → 低体档 2 → 吸 5000 ══
    {
        ScenarioBattle b(make_party(mk_self(kXieNeng, kSelfMax)), make_party(mk_foe(kFoeMaxA)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 5000;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(0).hp, 10000,
                 "T5580.a 低体档 2：自身 5000+5000（实测 " +
                     std::to_string(ctx.getPet(0).hp) + "）");
        CHECK_EQ(ctx.getPet(1).hp, kFoeMaxA - 5000,
                 "T5580.a 低体档 2：对手 10000-5000（实测 " +
                     std::to_string(ctx.getPet(1).hp) + "）");
        std::cout << "  [T5580.a] 低体档 2 通过\n";
    }

    // ══ T5580.b 25770 [3,2]：自身 15000 > 对手 12000 → 常规档 3 → 吸 4000 ══
    {
        ScenarioBattle b(make_party(mk_self(kXieNeng, kSelfMax)), make_party(mk_foe(kFoeMaxB)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 15000;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(0).hp, 19000,
                 "T5580.b 常规档 3：自身 15000+4000（实测 " +
                     std::to_string(ctx.getPet(0).hp) + "）");
        CHECK_EQ(ctx.getPet(1).hp, kFoeMaxB - 4000,
                 "T5580.b 常规档 3：对手 12000-4000（实测 " +
                     std::to_string(ctx.getPet(1).hp) + "）");
        std::cout << "  [T5580.b] 常规档 3 通过\n";
    }

    // ══ T5580.c 25770：双方 10000 相等 → 严格小于不成立 → 常规档 3 → 3333 ══
    {
        ScenarioBattle b(make_party(mk_self(kXieNeng, kSelfMax)), make_party(mk_foe(kFoeMaxA)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 10000;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(0).hp, 10000 + 3333,
                 "T5580.c 相等走常规档 3：自身 +3333（实测 " +
                     std::to_string(ctx.getPet(0).hp) + "）");
        CHECK_EQ(ctx.getPet(1).hp, kFoeMaxA - 3333,
                 "T5580.c 相等走常规档 3：对手 -3333（实测 " +
                     std::to_string(ctx.getPet(1).hp) + "）");
        std::cout << "  [T5580.c] 相等边界通过\n";
    }

    // ══ T5580.d 31575 [3,2] 攻击载体：自身 5000 < 对手 10000 → 低体档 2 → 回血 5000 ══
    {
        ScenarioBattle b(make_party(mk_self(kHuanMeng, kSelfMax)), make_party(mk_foe(kFoeMaxA)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 5000;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(0).hp, 10000,
                 "T5580.d 攻击载体低体档 2：自身精确 +5000（实测 " +
                     std::to_string(ctx.getPet(0).hp) + "）");
        CHECK(ctx.getPet(1).hp <= kFoeMaxA - 5000,
              "T5580.d 攻击载体低体档 2：对手至少 -5000（实测 " +
                  std::to_string(ctx.getPet(1).hp) + "）");
        std::cout << "  [T5580.d] 攻击载体低体档通过\n";
    }

    // ══ T5580.e 31575 [3,2]：自身 15000 > 对手 12000 → 常规档 3 → 回血 4000 ══
    {
        ScenarioBattle b(make_party(mk_self(kHuanMeng, kSelfMax)), make_party(mk_foe(kFoeMaxB)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 15000;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(0).hp, 19000,
                 "T5580.e 攻击载体常规档 3：自身精确 +4000（实测 " +
                     std::to_string(ctx.getPet(0).hp) + "）");
        CHECK(ctx.getPet(1).hp <= kFoeMaxB - 4000,
              "T5580.e 攻击载体常规档 3：对手至少 -4000（实测 " +
                  std::to_string(ctx.getPet(1).hp) + "）");
        std::cout << "  [T5580.e] 攻击载体常规档通过\n";
    }

    std::cout << "scenario_5580 PASS (J227 吸取最大体力分档族 1263)" << std::endl;
    return 0;
}
