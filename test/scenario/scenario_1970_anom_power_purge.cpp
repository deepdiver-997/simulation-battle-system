// 场景 1970 —— J104 异常状态威力族（1 宏，moves_lib/j_anom_power_purge.cpp）
//（2026-10-03，worktree ../agents/j104；号段 1970-1979 → T1970 一块）
//
// 断言口径：
//   - 变威力：自身预置中毒 → 载体伤害 ≈ 2× 无异常对照（比例带 [1.7, 2.35]，
//     双方各吃 85%-100% 浮动）；
//   - 解除：施放后自身烧伤/中毒/冻伤全清（reduce 压尽即刻失活）；
//   - 对照：无异常 → 无翻倍（与 a 同级或更低，且无解除面）；
//   - 载体 千翼幻光闪 [513] 特150 纯链。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa  = 10004;  // 拍打：物40
constexpr int kQianYi = 14695;  // 千翼幻光闪 [513]：特150（纯链载体）
constexpr int kMaxHp  = 5000;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_self(std::array<int, 5> skills) {
    ElfPet p = make_pet(1, "己方", skills, 300, /*atk=*/100, kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe(std::array<int, 5> skills) {
    ElfPet p = make_pet(2, "对手", skills, 100, /*atk=*/1, 99999);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

int self_anom_count(const BattleContext& ctx) {
    int cnt = 0;
    for (int id : {1, 2, 5}) {
        if (ctx.has_active_abnormal_status(0, id)) {
            ++cnt;
        }
    }
    return cnt;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1970 513 异常威力翻倍+命中解除（千翼幻光闪 [513]）══
    {
        // a) 自身中毒 → 威力 ×2 + 命中后三异常解除
        int d_poison = 0;
        {
            ScenarioBattle b(
                make_party(mk_self({kQianYi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            // ⚠️ 只上中毒：烧伤自带威力减半（materialize 时 is_burn_effective），
            // 与 ×2 相互抵消——官方语义为"烧伤补倍"，毒则纯净 ×2。
            (void)apply_anomaly(&ctx, 0, static_cast<int>(AbnormalStatusId::Poison),
                                /*duration=*/5, /*actor=*/1);
            CHECK(ctx.has_active_abnormal_status(0, 1),
                  "T1970.a 前置：自身已中毒");
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            d_poison = 99999 - ctx.getPet(1).hp;
            CHECK_EQ(self_anom_count(ctx), 0,
                     "T1970.a 513：命中后烧伤/中毒/冻伤全解除（残留 " +
                         std::to_string(self_anom_count(ctx)) + "）");
        }
        // b) 无异常对照：无翻倍
        int d_base = 0;
        {
            ScenarioBattle b(
                make_party(mk_self({kQianYi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            d_base = 99999 - b.ctx().getPet(1).hp;
        }
        CHECK(d_base > 0, "T1970 前置：基础伤 > 0（实测 " + std::to_string(d_base) + "）");
        CHECK(d_poison * 100 >= d_base * 170 && d_poison * 100 <= d_base * 235,
              "T1970.a 513：异常威力 ×2（毒 " + std::to_string(d_poison) + " / 基 " +
                  std::to_string(d_base) + "）");
        std::cout << "  [T1970] 513 异常威力+解除 通过\n";
    }

    std::cout << "scenario_1970 PASS (J104 异常状态威力族 513)" << std::endl;
    return 0;
}
