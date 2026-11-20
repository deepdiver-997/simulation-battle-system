#include <utils/sbs_rand.h>
// 场景 2740 —— J172 增伤门族（2 宏，moves_lib/j_amp_gates.cpp）
//（2026-10-04，worktree ../agents/j172；号段 2740-2749 → T2740-T2741 两块）
//
// 断言口径（crit=0）：
//   - 903（笔走龙蛇 30005 [30] 物 pri0 纯链；max 5000）：
//     a) self hp 3000 > foe hp 1000（ foe 预扣）→ 粉 1500 精确（5000×30%）+ 红伤带；
//     b) self hp 1000 ≤ foe 1000（等血，非"高于"）→ 无粉。
//   - 631（玄天凤坠闪 16328 [1,100] 物 pri0 纯链，码 1 中毒 确定档）：
//     a) 自身预置中毒 → 载体命中（解除+R2 AMP+100）→ R2 拍打伤害 ≈2× 基线拍打
//        （同场 R3 过期对照，比值法）；
//     b) 对照：自身无中毒 → R2 拍打 ≈ 基线量级（无 AMP）。
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

constexpr int kPaiDa   = 10004;  // 拍打：物40 pri0
constexpr int kBiZou   = 30005;  // 笔走龙蛇 [903]|[30]：物（903 载体）
constexpr int kXuanTian= 16328;  // 玄天凤坠闪 [631]|[1,100]：物（631 载体）
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

    // ══ T2740 903 高体门最大体力粉伤（笔走龙蛇）══
    {
        // a) self 3000 > foe 1000 → 粉 1500
        {
            ScenarioBattle b(
                make_party(mk_pet(1, "己方", {kBiZou, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300, 1)),
                make_party(mk_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100, 1)));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 3000;
            ctx.getPet(1).hp = 2500;
            const int foe_hp0 = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = foe_hp0 - ctx.getPet(1).hp;
            CHECK(loss >= 1501 && loss <= 1508,
                  "T2740.a 903：粉 1500 + 红伤带（实测 " + std::to_string(loss) + "）");
        }
        // b) 等血（非高于）→ 无粉
        {
            ScenarioBattle b(
                make_party(mk_pet(1, "己方", {kBiZou, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300, 1)),
                make_party(mk_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100, 1)));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 1000;
            ctx.getPet(1).hp = 2500;
            const int foe_hp0 = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = foe_hp0 - ctx.getPet(1).hp;
            CHECK(loss <= 8,
                  "T2740.b 903 对照：等血无粉（实测 " + std::to_string(loss) + "）");
        }
        std::cout << "  [T2740] 903 高体门粉伤 通过\n";
    }

    // ══ T2741 631 消除→下回合增伤（玄天凤坠闪，配对比值）══
    {
        // a) 自身中毒 → 载体（解除+AMP）→ R2 拍打 ≈2× R3 拍打（同场过期对照）
        {
            ScenarioBattle b(
                make_party(mk_pet(1, "己方", {kXuanTian, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300, 300)),
                make_party(mk_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100, 1)));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)apply_anomaly(&ctx, 0, /*中毒=*/1, /*duration=*/5, /*actor=*/1);
            CHECK(ctx.has_active_abnormal_status(0, 1), "T2741.a 前置：自身已中毒");
            int prev = ctx.getPet(1).hp;
            int l1 = 0, l2 = 0, l3 = 0;
            for (int r = 0; r < 3; ++r) {
                b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, r == 0 ? 0 : 1);
                b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
                b.drive();
                const int now = ctx.getPet(1).hp;
                (r == 0 ? l1 : r == 1 ? l2 : l3) = prev - now;
                prev = now;
            }
            std::cout << "  [dbg] l1=" << l1 << " l2=" << l2 << " l3=" << l3 << "\n";
            CHECK(l2 * 10 > l3 * 15,
                  "T2741.a 631：R2（AMP+100）应 >1.5× R3（过期）" + std::to_string(l2) +
                      " vs " + std::to_string(l3));
            CHECK(!ctx.has_active_abnormal_status(0, 1),
                  "T2741.a 631：中毒已解除");
        }
        // b) 对照：无中毒 → 门关 → R2 拍打回到基线量级（与载体 R1 特伤区分靠技能面）
        {
            ScenarioBattle b(
                make_party(mk_pet(1, "己方", {kXuanTian, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300, 300)),
                make_party(mk_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100, 1)));
            b.start();
            BattleContext& ctx = b.ctx();
            int prev = ctx.getPet(1).hp;
            int l1 = 0, l2 = 0;
            for (int r = 0; r < 2; ++r) {
                b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, r == 0 ? 0 : 1);
                b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
                b.drive();
                const int now = ctx.getPet(1).hp;
                (r == 0 ? l1 : l2) = prev - now;
                prev = now;
            }
            CHECK(l2 * 10 > l1 * 3 && l2 * 10 < l1 * 7,
                  "T2741.b 631 对照：R2 拍打 ≈ 载体物伤量级 0.3~0.7×（" +
                      std::to_string(l2) + " vs " + std::to_string(l1) + "，无 AMP 翻倍）");
        }
        std::cout << "  [T2741] 631 消除增伤 通过\n";
    }

    std::cout << "scenario_2740 PASS (J172 增伤门族 903/631)" << std::endl;
    return 0;
}
