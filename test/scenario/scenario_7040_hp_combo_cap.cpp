#include <utils/sbs_rand.h>
// 场景 7040 —— J253 体力差连击族（1593，moves_lib/j_hp_combo_cap.cpp）
//（2026-10-24，worktree ../agents/j253；号段 7040-7049 → T7040-T7041 两块）
//
// 断言口径（crit=0 去噪；连环殴打 36895 [3,7,9] 物40 pri0 纯链；atk=100/def=100 →
//   单击基数 ≈ (0.84×100/100×40+2)=35.6 × rand[0.85,1.0] ∈ [30.3,35.6]）：
//   - A（自身体力 400 < 对手）：上限改 9 → N∈[3,9]，d=基数×N ∈ [91,320]；
//     d≥250 ⇒ N≥8（N=7 上限 floor(35.6×7)=249）——证明"体力低于对手时上限提高"生效。
//   - B（自身体力 5000 > 对手 4900）：内核掷 [3,7] 照常 → d ∈ [91,249]，
//     任何 d≥250 都不该出现（无扩展）。
#include <iostream>
#include <vector>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa   = 10004;  // 拍打：物40 pri0（filler，atk=1 无伤害面）
constexpr int kWuDa    = 36895;  // 连环殴打 [1593]|[3,7,9]：物40 pri0（载体）
constexpr int kSelfHp  = 5000;
constexpr int kFoeHp   = 99999;

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
        s.critical_strike_rate = 0.0f;
    }
}

// self：{连环殴打, 拍打×4}，速度 300 先手，atk=100（伤害量级压到 91~320 便于分档）。
ElfPet mk_self(int atk) {
    ElfPet p = make_pet(1, "己方", {kWuDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300, atk, kSelfHp);
    accuracy_100(p);
    return p;
}

ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100, 1, kFoeHp);
    accuracy_100(p);
    return p;
}

// self 连打载体 rounds 轮（对手 NONE），返回对手每轮掉血。
std::vector<int> beat_down(int self_hp, int foe_hp, int rounds) {
    ScenarioBattle b(make_party(mk_self(100)), make_party(mk_foe()));
    b.start();
    BattleContext& ctx = b.ctx();
    if (self_hp != kSelfHp) {
        ctx.getPet(0).hp = self_hp;
    }
    if (foe_hp != kFoeHp) {
        ctx.getPet(1).hp = foe_hp;
    }
    ctx.getPet(0).skills[0].pp = 99;   // 灌满 PP（DB max_pp=10，撑不起 24 轮连打；-1 反被判不可用）
    std::vector<int> losses;
    int prev = ctx.getPet(1).hp;
    for (int r = 0; r < rounds; ++r) {
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 连环殴打
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        const int now = ctx.getPet(1).hp;
        losses.push_back(prev - now);
        prev = now;
    }
    return losses;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }
    sbs_srand(20261024);

    // ══ T7040 体力低于对手 → 上限提到 9（扩展生效）══
    {
        const auto a = beat_down(/*self_hp=*/400, kFoeHp, /*rounds=*/24);
        int mn = 1 << 30, mx = 0, ext = 0;
        for (const int d : a) {
            mn = std::min(mn, d);
            mx = std::max(mx, d);
            if (d >= 250) {
                ++ext;
            }
        }
        std::cout << "  [dbg] A mn=" << mn << " mx=" << mx << " ext(>=250)=" << ext << "\n";
        CHECK(mn >= 88,
              "T7040.a1 体力低：单击×N 下界（N≥3 → d≥88，实测 min " + std::to_string(mn) + "）");
        CHECK(mx <= 322,
              "T7040.a2 体力低：上限 9 → d ≤ 322（实测 max " + std::to_string(mx) + "）");
        CHECK(ext >= 1,
              "T7040.a3 体力低：出现 d≥250（N≥8，内核 [3,7] 打不出的档）" +
                  std::to_string(ext) + " 次");
        std::cout << "  [T7040] 连环殴打 体力差扩展上限 通过\n";
    }

    // ══ T7041 体力不低于对手 → 内核 [3,7] 照常（不扩展）══
    {
        const auto b = beat_down(kSelfHp, /*foe_hp=*/4900, /*rounds=*/12);
        int mn = 1 << 30, mx = 0;
        for (const int d : b) {
            mn = std::min(mn, d);
            mx = std::max(mx, d);
        }
        std::cout << "  [dbg] B mn=" << mn << " mx=" << mx << "\n";
        CHECK(mn >= 88,
              "T7041.a1 体力高：N≥3 下界（实测 min " + std::to_string(mn) + "）");
        CHECK(mx <= 249,
              "T7041.a2 体力高：N≤7 上界（实测 max " + std::to_string(mx) + "）——不得出现扩展档");
        std::cout << "  [T7041] 体力不低于对手不扩展 通过\n";
    }

    std::cout << "scenario_7040 PASS (J253 体力差连击族 1593)" << std::endl;
    return 0;
}
