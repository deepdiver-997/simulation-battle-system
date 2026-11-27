// 场景 5750 —— J244 概率倍伤/未触发封属族（921，moves_lib/j_prob_mult_seal_attr.cpp）
//（2026-10-22，worktree feat/jbatch；号段 5750-5759）
//
// 断言口径（审计分支 + 行为探针 + 伤害比值）：
//   - 921「{0}%概率造成攻击伤害为{1}倍，未触发则{2}回合内令对手属性技无效」：
//     载体 36906 猫叒斩 纯链 [30,3,2]（物 140）。
//   - 每局两回合：R1 己方出手（审计槽 990921100 记 rolled）→ R2 对手用属性技
//     **精准 20138**（自身命中+1）探封印。
//     · rolled=1（倍伤分支）→ R2 精准正常 → 对手命中槽 +1；
//     · rolled=0（未触发）→ 封属窗（2 回合）→ R2 精准无效 → 命中槽保持 0。
//   - 逐局断言"行为 == 审计分支"（N=40；不写成"与对照相等"）。
//   - 伤害比值：rolled=1 局均伤 / rolled=0 局均伤 ∈ [2.5,3.5]（{1}=3 倍；85%~100%
//     浮动由均值抹平）。
#include <iostream>
#include <any>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa  = 10004;
constexpr int kMaoRuo = 36906;  // [921]|[30,3,2]（物 140）
constexpr int kZhunQue = 20138; // 精准：属性技（自身命中+1，封印探针）
constexpr int kAuditKey = 990921100;

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
}

ElfPet mk_owner() {
    ElfPet p = make_pet(1, "己方", {kMaoRuo, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/300, /*hp=*/99999);
    accuracy_100(p);
    return p;
}

ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kZhunQue, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/99999);
    accuracy_100(p);
    return p;
}

int audit(BattleContext& ctx) {
    const auto it = ctx.plugin_storage.find(kAuditKey);
    if (it == ctx.plugin_storage.end()) {
        return -1;
    }
    if (const int* p = std::any_cast<int>(&it->second)) {
        return *p;
    }
    return -1;
}

struct Trial {
    bool rolled = false;
    int damage = 0;
    int foe_hit_after_r2 = 0;
};

Trial run_trial(unsigned seed) {
    ScenarioBattle b(make_party(mk_owner()), make_party(mk_foe()));
    b.ctx().seed_rng(seed);
    b.start();
    BattleContext& ctx = b.ctx();
    const int foe_before = ctx.getPet(1).hp;
    // R1：己方用 921 载体；对手不出手。
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
    Trial t;
    t.rolled = (audit(ctx) == 1);
    t.damage = foe_before - ctx.getPet(1).hp;
    // R2：己方不出手；对手用精准探封印。
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    t.foe_hit_after_r2 = ctx.ability_levels[1][5];
    return t;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    constexpr int N = 40;
    int rolled_n = 0, seal_n = 0, bad_behavior = 0;
    long mult_dmg = 0, plain_dmg = 0;
    for (int i = 0; i < N; ++i) {
        const Trial t = run_trial(1 + i * 104729u);
        if (t.rolled) {
            ++rolled_n;
            mult_dmg += t.damage;
            if (t.foe_hit_after_r2 != 1) {
                ++bad_behavior;
            }
        } else {
            ++seal_n;
            plain_dmg += t.damage;
            if (t.foe_hit_after_r2 != 0) {
                ++bad_behavior;
            }
        }
    }
    std::cout << "  [T5750] N=" << N << " rolled=" << rolled_n << " sealed=" << seal_n
              << " bad_behavior=" << bad_behavior << std::endl;
    CHECK(rolled_n > 0 && seal_n > 0,
          "T5750 两分支均被覆盖（rolled=" + std::to_string(rolled_n) + " sealed=" +
              std::to_string(seal_n) + "）");
    CHECK(rolled_n >= 3 && rolled_n <= 25,
          "T5750 {0}=30 触发率合理（实测 " + std::to_string(rolled_n) + "/" + std::to_string(N) + "）");
    CHECK(bad_behavior == 0,
          "T5750 逐局行为 == 审计分支（倍伤→精准可用 / 未触发→封属；不符 " +
              std::to_string(bad_behavior) + " 局）");
    const double ratio = (static_cast<double>(mult_dmg) / rolled_n)
                         / (static_cast<double>(plain_dmg) / seal_n);
    std::cout << "  [T5750] mult_avg=" << (mult_dmg / rolled_n)
              << " plain_avg=" << (plain_dmg / seal_n) << " ratio=" << ratio << std::endl;
    CHECK(ratio > 2.5 && ratio < 3.5,
          "T5750 倍伤分支 ≈{1}=3×（实测 ratio=" + std::to_string(ratio) + "）");

    std::cout << "scenario_5750 PASS (J244 概率倍伤/未触发封属族)" << std::endl;
    return 0;
}
