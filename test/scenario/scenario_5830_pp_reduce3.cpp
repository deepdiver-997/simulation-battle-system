// 场景 5830 —— J252 PP 削减族三（2033/2081/1621，moves_lib/j_pp_reduce3.cpp）
//（2026-10-22，worktree feat/jbatch；号段 5830-5839）
//
// 断言口径（精确 PP 差值 + 门闭绝对值 + 审计槽）：
//   - PP 操作一律走原语（pp_reduce / pp_zero_slot）；场景按"逐槽 PP 前后快照"断言
//     精确差值，不用"等级和"式的模糊口径。
//   - 2033（37754 折钢棱锋，链 [999,558,2033] args=[1,20,300,2]）：未击败 → 对手
//     全部技能 PP 精确 -2；击败（对手 hp=1）→ 门闭，PP 逐槽不变（绝对值），审计 -1。
//     ⚠️ 击杀局从 seerRobot[1].elfPets[0] 读倒下精灵的 PP（getPet 会随死亡切宠）。
//   - 2081（37627 虚实温度，链 [188,2081,190]）：对手无异常 → 恰 1 个槽 PP 归零、
//     其余槽不变（逐局计数）；对手带中毒 → 门闭，PP 逐槽不变（绝对值），审计 -1。
//   - 1621（36833 涛光灿灿，纯链 [1621] args=[100,1]）：满体 → 概率 100% 且点数翻倍
//     为 2（全部技能 -2）；半体 → 点数 1（全部技能 -1）；审计槽 1100 与点数 2/1 逐局吻合。
//     另测下限：对手 PP=1 时满体档 -2 钳到 0（不出现负 PP）。
#include <iostream>
#include <any>
#include <string>
#include <vector>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <plugin/core_api.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;   // 拍打：物 40（填充）
constexpr int k2033   = 37754;  // 折钢棱锋 [999,558,2033]|[1,20,300,2]
constexpr int k2081   = 37627;  // 虚实温度 [188,2081,190]|[1,3]
constexpr int k1621   = 36833;  // 涛光灿灿 [1621]|[100,1]（纯链）

constexpr int k2033Audit = 202033100;
constexpr int k2081Audit = 202081100;
constexpr int k1621Audit = 201621100;

constexpr int kPoison = 1;

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
}

ElfPet mk_owner(int carrier) {
    ElfPet p = make_pet(1, "己方", {carrier, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/300, /*hp=*/99999);
    accuracy_100(p);
    return p;
}

ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/99999);
    accuracy_100(p);
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

// 目标方第 0 只精灵（倒下时 getPet 会切宠）的逐槽 PP 快照。
std::vector<int> pp_snapshot(BattleContext& ctx, int side) {
    std::vector<int> out;
    for (const Skills& s : ctx.seerRobot[side].elfPets[0].skills) {
        out.push_back(s.pp);
    }
    return out;
}

struct Trial {
    std::vector<int> pp_before;
    std::vector<int> pp_after;
    bool foe_alive = true;
    int audit = -999;
    int amount_audit = -999;
};

// mode: 0=普通 / 1=对手 hp=1（2033 击杀门） / 2=对手中毒（2081 门闭）
//       3=自身半血（1621） / 4=对手 PP 全压到 1（1621 下限）
Trial run_trial(int carrier, int mode) {
    ScenarioBattle b(make_party(mk_owner(carrier)), make_party(mk_foe()));
    b.start();
    BattleContext& ctx = b.ctx();
    if (mode == 1) {
        ctx.getPet(1).hp = 1;   // 保证本击必杀（当前体力，max 仍 99999）
    }
    if (mode == 2) {
        (void)apply_anomaly(&ctx, 1, kPoison, /*duration_rounds=*/5, /*actor=*/0);
    }
    if (mode == 3) {
        ctx.getPet(0).hp = 100;   // 半体以下（max 99999）
    }
    if (mode == 4) {
        for (Skills& s : ctx.seerRobot[1].elfPets[0].skills) {
            s.pp = 1;
        }
    }
    Trial t;
    t.pp_before = pp_snapshot(ctx, 1);
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
    t.pp_after = pp_snapshot(ctx, 1);
    t.foe_alive = ctx.seerRobot[1].elfPets[0].hp > 0;
    t.audit = audit(ctx, (carrier == k2033 ? k2033Audit
                         : carrier == k2081 ? k2081Audit
                                            : k1621Audit));
    if (carrier == k1621) {
        t.amount_audit = audit(ctx, k1621Audit + 1);
    }
    return t;
}

std::vector<int> deltas(const Trial& t) {
    std::vector<int> d;
    for (std::size_t i = 0; i < t.pp_before.size() && i < t.pp_after.size(); ++i) {
        d.push_back(t.pp_before[i] - t.pp_after[i]);
    }
    return d;
}

bool all_delta(const Trial& t, int want) {
    for (const int d : deltas(t)) {
        if (d != want) {
            return false;
        }
    }
    return true;
}

int zero_count(const Trial& t) {
    int n = 0;
    for (const int pp : t.pp_after) {
        n += (pp == 0) ? 1 : 0;
    }
    return n;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5830.a 2033：未击败 → 全槽 PP-2；击败 → 门闭 PP 不变 ══
    {
        constexpr int N = 15;
        int hit_ok = 0, kill_ok = 0, audit_ok = 0;
        for (int i = 0; i < N; ++i) {
            const Trial t = run_trial(k2033, /*mode=*/0);
            hit_ok += (t.foe_alive && all_delta(t, 2)) ? 1 : 0;
            audit_ok += (t.audit == 1) ? 1 : 0;
        }
        for (int i = 0; i < N; ++i) {
            const Trial t = run_trial(k2033, /*mode=*/1);
            kill_ok += (!t.foe_alive && all_delta(t, 0)) ? 1 : 0;
            audit_ok += (t.audit == -1) ? 1 : 0;
        }
        std::cout << "  [T5830.a] 2033 survive_reduce_ok=" << hit_ok << "/" << N
                  << " kill_gate_ok=" << kill_ok << "/" << N
                  << " audit_ok=" << audit_ok << "/" << (2 * N) << std::endl;
        CHECK(hit_ok == N, "T5830.a 未击败 → 对手全槽 PP 精确 -2（实测 " +
                               std::to_string(hit_ok) + "/" + std::to_string(N) + "）");
        CHECK(kill_ok == N, "T5830.a 击败 → 门闭，全槽 PP 不变（实测 " +
                                std::to_string(kill_ok) + "/" + std::to_string(N) + "）");
        CHECK(audit_ok == 2 * N, "T5830.a 审计槽逐局吻合（实测 " + std::to_string(audit_ok) +
                                     "/" + std::to_string(2 * N) + "）");
    }

    // ══ T5830.b 2081：对手无异常 → 恰 1 槽归零；对手中毒 → 门闭全槽不变 ══
    {
        constexpr int N = 20;
        int one_zero = 0, other_untouched = 0, gate_closed = 0, audit_ok = 0;
        for (int i = 0; i < N; ++i) {
            const Trial t = run_trial(k2081, /*mode=*/0);
            one_zero += (zero_count(t) == 1) ? 1 : 0;
            int non_zero_delta = 0;
            for (std::size_t k = 0; k < t.pp_after.size(); ++k) {
                if (t.pp_after[k] != 0) {
                    non_zero_delta += (t.pp_after[k] == t.pp_before[k]) ? 0 : 1;
                }
            }
            other_untouched += (non_zero_delta == 0) ? 1 : 0;
            audit_ok += (t.audit == 1001) ? 1 : 0;
        }
        for (int i = 0; i < N; ++i) {
            const Trial t = run_trial(k2081, /*mode=*/2);
            gate_closed += all_delta(t, 0) ? 1 : 0;
            audit_ok += (t.audit == -1) ? 1 : 0;
        }
        std::cout << "  [T5830.b] 2081 one_zero=" << one_zero << "/" << N
                  << " other_untouched=" << other_untouched << "/" << N
                  << " anomaly_gate_closed=" << gate_closed << "/" << N
                  << " audit_ok=" << audit_ok << "/" << (2 * N) << std::endl;
        CHECK(one_zero == N, "T5830.b 无异常 → 恰 1 个槽 PP 归零（实测 " +
                                 std::to_string(one_zero) + "/" + std::to_string(N) + "）");
        CHECK(other_untouched == N, "T5830.b 非零槽逐槽不变（实测 " +
                                        std::to_string(other_untouched) + "/" +
                                        std::to_string(N) + "）");
        CHECK(gate_closed == N, "T5830.b 对手中毒 → 门闭，PP 全槽不变（实测 " +
                                    std::to_string(gate_closed) + "/" + std::to_string(N) + "）");
        CHECK(audit_ok == 2 * N, "T5830.b 审计槽逐局吻合（实测 " + std::to_string(audit_ok) +
                                     "/" + std::to_string(2 * N) + "）");
    }

    // ══ T5830.c 1621：满体 → -2；半体 → -1；PP=1 下限钳 0 ══
    {
        constexpr int N = 10;
        int full_ok = 0, half_ok = 0, clamp_ok = 0, audit_ok = 0, amount_ok = 0;
        for (int i = 0; i < N; ++i) {
            const Trial t = run_trial(k1621, /*mode=*/0);
            full_ok += all_delta(t, 2) ? 1 : 0;
            audit_ok += (t.audit == 1100) ? 1 : 0;
            amount_ok += (t.amount_audit == 2) ? 1 : 0;
        }
        for (int i = 0; i < N; ++i) {
            const Trial t = run_trial(k1621, /*mode=*/3);
            half_ok += all_delta(t, 1) ? 1 : 0;
            audit_ok += (t.audit == 1100) ? 1 : 0;
            amount_ok += (t.amount_audit == 1) ? 1 : 0;
        }
        for (int i = 0; i < N; ++i) {
            const Trial t = run_trial(k1621, /*mode=*/4);
            clamp_ok += all_delta(t, 1) ? 1 : 0;   // 1-1=0（满体档点数 2 被 1 起点钳住）
            int negatives = 0;
            for (const int pp : t.pp_after) {
                negatives += (pp < 0) ? 1 : 0;
            }
            clamp_ok -= (negatives > 0) ? 1 : 0;
        }
        std::cout << "  [T5830.c] 1621 full(-2)=" << full_ok << "/" << N
                  << " half(-1)=" << half_ok << "/" << N
                  << " clamp=" << clamp_ok << "/" << N
                  << " audit_ok=" << audit_ok << "/" << (2 * N)
                  << " amount_ok=" << amount_ok << "/" << (2 * N) << std::endl;
        CHECK(full_ok == N, "T5830.c 满体 → 点数翻倍为 2，全槽 -2（实测 " +
                                std::to_string(full_ok) + "/" + std::to_string(N) + "）");
        CHECK(half_ok == N, "T5830.c 半体 → 点数 1，全槽 -1（实测 " +
                                std::to_string(half_ok) + "/" + std::to_string(N) + "）");
        CHECK(clamp_ok == N, "T5830.c PP=1 起点：满体档 -2 钳到 0 且无负值（实测 " +
                                 std::to_string(clamp_ok) + "/" + std::to_string(N) + "）");
        CHECK(audit_ok == 2 * N, "T5830.c 审计概率槽逐局吻合（实测 " +
                                     std::to_string(audit_ok) + "/" + std::to_string(2 * N) + "）");
        CHECK(amount_ok == 2 * N, "T5830.c 审计点数槽 2/1 逐局吻合（实测 " +
                                      std::to_string(amount_ok) + "/" + std::to_string(2 * N) + "）");
    }

    std::cout << "scenario_5830 PASS (J252 PP 削减族三)" << std::endl;
    return 0;
}
