#include <utils/sbs_rand.h>
// 场景 2750 —— J173 随机恢复族（2 宏，moves_lib/j_rand_heal_power.cpp）
//（2026-10-04，worktree ../agents/j173；号段 2750-2759 → T2750-T2751 两块）
//
// 断言口径（crit=0）：
//   - 1132（飞仙灵龙诀 [150,300,200,350] 特 pri0 纯链；self hp 压到 800 防溢出）：
//     a) 有提升（+2）→ 5 次治疗全部 ∈ [150,300]；
//     b) 无提升 → 5 次全部 ∈ [200,350]（区间分支逐样本断言）。
//   - 70（远古审判 cat2 pri0 纯链）：6 连打伤害全在 [280,540]（150~220 威力带外推）
//     且 max>min。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa   = 10004;  // 拍打：物40 pri0
constexpr int kFeiXian = 34609;  // 飞仙灵龙诀 [1132]|[150,300,200,350]：物（1132 载体）
constexpr int kYuanGu  = 10847;  // 远古审判 [70]：特（70 载体）
constexpr int kSelfHp  = 5000;
constexpr int kFoeHp   = 99999;

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
        s.critical_strike_rate = 0.0f;
    }
}

ElfPet mk_pet(int id, const char* name, std::array<int, 5> skills, int speed, int atk) {
    ElfPet p = make_pet(id, name, skills, speed, atk, id == 1 ? kSelfHp : kFoeHp);
    accuracy_100(p);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }
    sbs_srand(20261004);

    // ══ T2750 1132 随机恢复双区间（飞仙灵龙诀）══
    {
        // a) 有提升 → [150,300]
        {
            ScenarioBattle b(
                make_party(mk_pet(1, "己方", {kFeiXian, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300, 1)),
                make_party(mk_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100, 1)));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)stat_change(&ctx, 0, 0, 2);   // 自身有提升
            for (int r = 0; r < 5; ++r) {
                ctx.getPet(0).hp = 800;   // 压低血线防溢出（self 也会被对手拍打？对手 NONE）
                const int hp0 = ctx.getPet(0).hp;
                b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
                b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
                b.drive();
                const int healed = ctx.getPet(0).hp - hp0;
                CHECK(healed >= 150 && healed <= 300,
                      "T2750.a 1132 有提升：第 " + std::to_string(r + 1) + " 次治疗 " +
                          std::to_string(healed) + " 应 ∈ [150,300]");
            }
        }
        // b) 无提升 → [200,350]
        {
            ScenarioBattle b(
                make_party(mk_pet(1, "己方", {kFeiXian, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300, 1)),
                make_party(mk_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100, 1)));
            b.start();
            BattleContext& ctx = b.ctx();
            for (int r = 0; r < 5; ++r) {
                ctx.getPet(0).hp = 800;
                const int hp0 = ctx.getPet(0).hp;
                b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
                b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
                b.drive();
                const int healed = ctx.getPet(0).hp - hp0;
                CHECK(healed >= 200 && healed <= 350,
                      "T2750.b 1132 无提升：第 " + std::to_string(r + 1) + " 次治疗 " +
                          std::to_string(healed) + " 应 ∈ [200,350]");
            }
        }
        std::cout << "  [T2750] 1132 随机恢复双区间 通过\n";
    }

    // ══ T2751 70 威力随机 150~220（远古审判）══
    {
        ScenarioBattle b(
            make_party(mk_pet(1, "己方", {kYuanGu, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300, 300)),
            make_party(mk_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        int prev = ctx.getPet(1).hp;
        int mn = 1 << 30, mx = 0;
        for (int r = 0; r < 5; ++r) {   // 远古审判 PP=5
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int now = ctx.getPet(1).hp;
            const int loss = prev - now;
            mn = std::min(mn, loss);
            mx = std::max(mx, loss);
            CHECK(loss > 280 && loss < 560,
                  "T2751 70：第 " + std::to_string(r + 1) + " 打掉血 " +
                      std::to_string(loss) + " 应在 150~220 威力带外推 [280,560]");
            prev = now;
        }
        CHECK(mx > mn, "T2751 70：确系随机（max " + std::to_string(mx) + " > min " +
                           std::to_string(mn) + "）");
        std::cout << "  [T2751] 70 威力随机带 通过（" << mn << "~" << mx << "）\n";
    }

    std::cout << "scenario_2750 PASS (J173 随机恢复族 1132/70)" << std::endl;
    return 0;
}
