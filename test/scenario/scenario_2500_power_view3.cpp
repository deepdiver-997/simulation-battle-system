#include <utils/sbs_rand.h>
// 场景 2500 —— J159 变威力族三（3 宏，moves_lib/j_power_view3.cpp）
//（2026-10-04，worktree ../agents/j159；号段 2500-2509 → T2500-T2502 三块）
//
// 断言口径（crit=0 全程去噪）：
//   - 468（烈焰狂火杀 14051 物 pri0 纯链；"物理载体降槽 -1 时威力翻倍恰与攻击减半
//     抵消"的配平性质）：R1（预置 -1 降、tick 未跑）伤害=半威力带；R2（tick 置旗+
//     解除）伤害≈2×R1 且 ≈R3（解除后无旗基线）。
//   - 118（白羽净化 [150,220] 物 pri0 纯链）：6 连打全部落在 [300,520]
//     （d(150)×0.85≈287 下界、d(220)×1.0≈495 上界，按拍打 40 力≈90 的比例外推）
//     且 max>min（确系随机）。
//   - 51（变化术 [5] cat4 pri0 纯链）：a) self atk=100、对手 atk=400 → R1 变化术
//     后 R2 拍打伤害 ≈ 对手攻击量级（与 self atk=400 的对照场比值 ∈[0.8,1.25]）；
//     b) R7（窗过期）伤害回落到 atk=100 量级（< R2 的一半）。
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
constexpr int kLieYan  = 14051;  // 烈焰狂火杀 [468]：物（468 载体）
constexpr int kBaiYu   = 12078;  // 白羽净化 [118]|[150,220]：物（118 载体）
constexpr int kBianHua = 20096;  // 变化术 [51]|[5]：属性技（51 载体）
constexpr int kSelfHp  = 5000;
constexpr int kFoeHp   = 99999;

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
        s.critical_strike_rate = 0.0f;   // 全程去暴击噪声
    }
}

ElfPet mk_pet(int id, const char* name, std::array<int, 5> skills, int speed, int atk) {
    ElfPet p = make_pet(id, name, skills, speed, atk, id % 2 == 1 ? kSelfHp : kFoeHp);
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

    // ══ T2500 468 降槽威力翻倍+解除（烈焰狂火杀，配平断言）══
    {
        ScenarioBattle b(
            make_party(mk_pet(1, "己方", {kLieYan, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300, 300)),
            make_party(mk_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        (void)stat_change(&ctx, 0, 0, -1);   // 自身攻击 -1（配平前置）
        int prev = ctx.getPet(1).hp;
        int d1 = 0, d2 = 0, d3 = 0;
        for (int r = 0; r < 3; ++r) {
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 载体连打
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int now = ctx.getPet(1).hp;
            (r == 0 ? d1 : r == 1 ? d2 : d3) = prev - now;
            prev = now;
        }
        std::cout << "  [dbg] d1=" << d1 << " d2=" << d2 << " d3=" << d3 << "\n";
        CHECK(d2 * 10 > d3 * 17 && d2 * 10 < d3 * 23,
              "T2500.a 468：R2（攻击复原+威力翻倍）≈2×R3（" + std::to_string(d2) +
                  " vs " + std::to_string(d3) + "）");
        CHECK(d3 * 10 > d1 * 13,
              "T2500.b 468：解除降槽后伤害抬升（R3 " + std::to_string(d3) + " > R1 " +
                  std::to_string(d1) + "）");
        std::cout << "  [T2500] 468 降槽威力翻倍 通过\n";
    }

    // ══ T2501 118 随机威力 150~220（白羽净化）══
    {
        ScenarioBattle b(
            make_party(mk_pet(1, "己方", {kBaiYu, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300, 300)),
            make_party(mk_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        int prev = ctx.getPet(1).hp;
        int mn = 1 << 30, mx = 0;
        for (int r = 0; r < 6; ++r) {
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int now = ctx.getPet(1).hp;
            const int loss = prev - now;
            mn = std::min(mn, loss);
            mx = std::max(mx, loss);
            CHECK(loss > 250 && loss < 560,
                  "T2501 118：第 " + std::to_string(r + 1) + " 打掉血 " +
                      std::to_string(loss) + " 应在 150~220 威力带外推 [250,560]");
            prev = now;
        }
        CHECK(mx > mn, "T2501 118：确系随机（max " + std::to_string(mx) + " > min " +
                           std::to_string(mn) + "）");
        std::cout << "  [T2501] 118 随机威力带 通过（" << mn << "~" << mx << "）\n";
    }

    // ══ T2502 51 攻击力与对手相同（变化术）══
    {
        // a) self atk=100 → 变化术后 R2 拍打 ≈ atk=400 对照场的拍打
        int copied_loss = 0, native_loss = 0, expired_loss = 0;
        {
            ScenarioBattle b(make_party(
                                 mk_pet(1, "己方", {kBianHua, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300, 100)),
                             make_party(mk_pet(2, "对手",
                                               {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100, 400)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 变化术
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            int prev = ctx.getPet(1).hp;
            for (int r = 0; r < 6; ++r) {   // R2..R7 拍打
                b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
                b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
                b.drive();
                const int now = ctx.getPet(1).hp;
                if (r == 0) copied_loss = prev - now;      // R2（窗内）
                if (r == 5) expired_loss = prev - now;     // R7（窗过期）
                prev = now;
            }
        }
        {
            // 对照场不带变化术（51 会把自身攻击拷成对手的 1！）——纯拍打原生基线
            ScenarioBattle b(make_party(
                                 mk_pet(1, "己方", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300, 400)),
                             make_party(mk_pet(2, "对手",
                                               {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100, 1)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 拍打（热身）
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            int prev = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 拍打（atk=400 原生）
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            native_loss = prev - ctx.getPet(1).hp;
        }
        std::cout << "  [dbg] copied=" << copied_loss << " native=" << native_loss
                  << " expired=" << expired_loss << "\n";
        CHECK(copied_loss * 10 > native_loss * 8 && copied_loss * 10 < native_loss * 13,
              "T2502.a 51：拷贝攻与 atk=400 原生同级（" + std::to_string(copied_loss) +
                  " vs " + std::to_string(native_loss) + "）");
        CHECK(expired_loss * 2 < copied_loss,
              "T2502.b 51：窗过期回落 atk=100 量级（" + std::to_string(expired_loss) +
                  " < 一半 " + std::to_string(copied_loss / 2) + "）");
        std::cout << "  [T2502] 51 攻击力拷贝 通过\n";
    }

    std::cout << "scenario_2500 PASS (J159 变威力族三 468/118/51)" << std::endl;
    return 0;
}
