#include <utils/sbs_rand.h>
// 场景 5900 —— J259 未满体力下回合减伤族（1584，moves_lib/j_wounded_next_round_reduce.cpp）
//（2026-10-22，worktree feat/jbatch；号段 5900-5909）
//
// 断言口径（ws 加算减伤槽精确值 + 同种子红伤差分）：
//   - 1584（冥界之钟 35168 纯链 [1584]|[50]）：出手时未满体力 → R2 ROUND_START 写
//     ws.damage_reduce_add[0][首空槽] = 50（正值=减伤 50%）。
//   - a) 未满体力（5000/10000）R1 施放 → 审计 1；R2 首槽恰为 50、其余 0。
//   - b) 满血（10000/10000）R1 施放 → 审计 0；R2 全槽 0。**且本回合随后挨打**
//     （门在"出手时"判，不在下一回合开始时判）——这条同时锁门时点。
//   - c) 同种子双局差分：R1 当回合伤害 A==B（不含施放当回合）；R2 A==B/2
//     （下回合 50% 减伤）；R3 A==B（窗过期）。
//   - d) 槽位纪律：只写首空槽，R3 归零（ws 每回合 reset）。
#include <array>
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;   // 拍打：物攻 40（对手攻击载体）
constexpr int kCarrier = 35168; // 冥界之钟 [1584]|[50]（纯链）
constexpr int kMaxHp = 10000;
constexpr int kPct = 50;
constexpr int kAuditBase = 996584100;
constexpr unsigned kSeed = 20261022;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_self() {
    ElfPet p = make_pet(1, "己方", {kCarrier, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, /*hp=*/kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/300, /*hp=*/99999);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

int audit(BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return -999;
    }
    if (const int* p = std::any_cast<int>(&it->second)) {
        return *p;
    }
    return -999;
}

int count_pct_slots_from(const int (&slots)[4]) {
    int n = 0;
    for (int i = 0; i < 4; ++i) {
        if (slots[i] == kPct) {
            ++n;
        }
    }
    return n;
}

struct RoundDamage {
    int r1 = 0;
    int r2 = 0;
    int r3 = 0;
    int slot_after_r2[4] = {0, 0, 0, 0};
    int slot_after_r3[4] = {0, 0, 0, 0};
    int audit = -999;
};

// 三回合：R1 己方施放 35168、对手拍打；R2/R3 己方 NONE、对手拍打。
RoundDamage run_battle(bool wounded_at_cast) {
    sbs_srand(kSeed);
    ScenarioBattle b(make_party(mk_self()), make_party(mk_foe()));
    b.start();
    BattleContext& ctx = b.ctx();
    if (wounded_at_cast) {
        ctx.getPet(0).hp = kMaxHp / 2;   // 只压当前体力，最大体力不变
    }

    RoundDamage d;
    int hp0 = ctx.getPet(0).hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    d.r1 = hp0 - ctx.getPet(0).hp;
    d.audit = audit(ctx, kAuditBase + 0);

    hp0 = ctx.getPet(0).hp;
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    d.r2 = hp0 - ctx.getPet(0).hp;
    for (int i = 0; i < 4; ++i) {
        d.slot_after_r2[i] = ctx.ws.damage_reduce_add[0][i];
    }

    hp0 = ctx.getPet(0).hp;
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    d.r3 = hp0 - ctx.getPet(0).hp;
    for (int i = 0; i < 4; ++i) {
        d.slot_after_r3[i] = ctx.ws.damage_reduce_add[0][i];
    }
    return d;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5900.a/b：未满体力门 + 满血门 + 门时点（出手时判） ══
    {
        const RoundDamage wounded = run_battle(/*wounded_at_cast=*/true);
        const RoundDamage full = run_battle(/*wounded_at_cast=*/false);

        // a) 未满体力：审计 1；R2 首槽 50、其余 0。
        CHECK_EQ(wounded.audit, 1, "a1 未满体力门过（审计=1）");
        CHECK_EQ(wounded.slot_after_r2[0], kPct, "a2 R2 首空槽=50（精确槽位）");
        CHECK_EQ(wounded.slot_after_r2[1], 0, "a3 R2 第二槽=0");
        CHECK_EQ(wounded.slot_after_r2[2], 0, "a4 R2 第三槽=0");
        CHECK_EQ(wounded.slot_after_r2[3], 0, "a5 R2 第四槽=0");
        CHECK_EQ(count_pct_slots_from(wounded.slot_after_r2), 1, "a6 仅一处写入 50");

        // b) 满血：审计 0；R2 全槽 0。R1 随后挨打也不补门（门在出手时判）。
        CHECK_EQ(full.audit, 0, "b1 满血门闭（审计=0）");
        CHECK_EQ(full.slot_after_r2[0], 0, "b2 满血 R2 首槽=0");
        CHECK_EQ(count_pct_slots_from(full.slot_after_r2), 0, "b3 满血 R2 无减伤槽");

        // c) 同种子差分：不含当回合、R2 减半、R3 过期。
        CHECK(wounded.r1 > 0 && full.r1 > 0, "c0 R1 对手攻击实际命中（双局）");
        CHECK_EQ(wounded.r1, full.r1, "c1 施放当回合不减免（A.R1==B.R1）");
        CHECK_EQ(wounded.r2, full.r2 / 2, "c2 下回合减伤 50%（A.R2==B.R2/2）");
        CHECK(wounded.r2 < full.r2, "c3 下回合伤害确实下降");
        CHECK_EQ(wounded.r3, full.r3, "c4 R3 窗过期（A.R3==B.R3）");
        CHECK(wounded.r3 > wounded.r2, "c5 R3 伤害回升到无减伤档");

        // d) 槽位纪律：R3 归零（窗长恰好 1 回合，ws 每回合 reset）。
        CHECK_EQ(wounded.slot_after_r3[0], 0, "d1 R3 首槽已清 0");
        CHECK_EQ(count_pct_slots_from(wounded.slot_after_r3), 0, "d2 R3 无残留减伤槽");

        std::cout << "  [a/b] 门：未满体力 1 / 满血 0；R2 精确槽 50\n"
                  << "  [c] 双局差分 R1 " << wounded.r1 << "==" << full.r1
                  << "，R2 " << wounded.r2 << "==" << full.r2 << "/2，R3 " << wounded.r3
                  << "==" << full.r3 << "\n";
    }

    std::cout << "scenario_5900 PASS (J259 未满体力下回合减伤族)" << std::endl;
    return 0;
}
