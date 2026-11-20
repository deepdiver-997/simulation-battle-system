#include <utils/sbs_rand.h>
// 场景 6060 —— J273 异常门固伤/异常窗族（1143，moves_lib/j_anomaly_gate_fixed_window.cpp）
//（2026-10-22，worktree feat/jbatch；号段 6060-6069 → T6060.a-c 三块）
//
// 断言口径：
//   - 纯链载体 日月轮·重天 37430 [1143]|[300,1,2]（物140，atk=1000/def=100）。
//   - 门开/门闭同种子对拍：门开局对手预置害怕(6)，门闭局无异常；固定在
//     SKILL_EFFECT 时点落地、早于 ATTACK_DAMAGE → 停泊到 FIRST_ATTACK_DAMAGE 读
//     对手 hp，差值恰为 {0}=300（攻击伤害同种子同值，窗 tick 尚未跑）。
//   - 门闭窗：AFTER_ACTION tick 末审计 991143100=1 次、991143200 mask 恰 2 位；
//     抽中码逐位核对宿主异常（若两码同为控场 15/22，控场替换只会留一个 → 至少一位）。
//   - 窗长=1：R2 改用拍打（不再施放 1143），tick 计数保持 1、mask 不变。
#include <any>
#include <iostream>
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

constexpr int kRiYue  = 37430;   // 日月轮·重天：物140 纯链 [1143]|[300,1,2]
constexpr int kPaiDa  = 10004;   // 拍打：filler
constexpr int kMaxHp  = 99999;
constexpr int kSeed   = 20266060;

constexpr int kGateBase = 991143000;   // +owner：1=门开 / 0=门闭
constexpr int kTickBase = 991143100;   // +owner：窗内 tick 次数
constexpr int kMaskBase = 991143200;   // +owner：最近抽取 bitmask（池下标位）

constexpr int kPool[4] = {2, 5, 15, 22};   // 烧伤/冻伤/冰封/焚烬

int audit_value(BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return -1;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : -1;
}

int popcount4(int mask) {
    return ((mask >> 0) & 1) + ((mask >> 1) & 1) + ((mask >> 2) & 1) + ((mask >> 3) & 1);
}

void always_hit(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
}

ElfPet self_pet() {
    ElfPet p = make_pet(1, "己方", {kRiYue, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1000, kMaxHp);
    always_hit(p);
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, kMaxHp);
    always_hit(p);
    return p;
}

// 门开/门闭同种子对拍：停泊在 FIRST_ATTACK_DAMAGE 读对手 hp（窗 tick 尚未执行）。
int foe_hp_after_attack(bool with_anomaly, int& gate) {
    ScenarioBattle b(make_party(self_pet()), make_party(foe_pet()));
    b.start();
    BattleContext& ctx = b.ctx();
    if (with_anomaly) {
        REQUIRE(apply_anomaly(&ctx, 1, 6, 3, 0) == ApplyAnomalyResult::SUCCESS);
    }
    sbs_srand(kSeed);
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    ctx.debug_step_mode = true;
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.step_past(State::BATTLE_FIRST_ATTACK_DAMAGE);
    const int hp = ctx.getPet(1).hp;
    ctx.debug_step_mode = false;
    b.drive();   // 跑完本回合（门开局也不应有窗 tick）
    gate = audit_value(ctx, kGateBase + 0);
    if (!with_anomaly) {
        CHECK_EQ(audit_value(ctx, kTickBase + 0), 1,
                 "T6060.a 1143：门闭局窗 tick 恰 1 次");
    } else {
        CHECK_EQ(audit_value(ctx, kTickBase + 0), -1,
                 "T6060.a 1143：门开局不注册窗（无 tick 计数）");
    }
    return hp;
}

bool anomaly_active(BattleContext& ctx, int side, int code) {
    return core_api().has_active_anomaly(&ctx, side, code);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T6060.a 门开固伤：同种子差值恰 300（停泊在攻击伤害后）══
    {
        int gate_open = -1;
        int gate_closed = -1;
        const int hp_open = foe_hp_after_attack(true, gate_open);
        const int hp_closed = foe_hp_after_attack(false, gate_closed);
        CHECK_EQ(gate_open, 1, "T6060.a 1143：对手有异常 → 门开审计 1");
        CHECK_EQ(gate_closed, 0, "T6060.a 1143：对手无异常 → 门闭审计 0");
        CHECK_EQ(hp_closed - hp_open, 300,
                 "T6060.a 1143：门开附加固定伤害 300（hp " + std::to_string(hp_open) +
                     " vs " + std::to_string(hp_closed) + "）");
        std::cout << "  [T6060.a] 1143 异常门固伤 通过\n";
    }

    // ══ T6060.b 门闭窗：随机 2 种异常 + mask 精确 2 位 ══
    {
        ScenarioBattle b(make_party(self_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        sbs_srand(kSeed);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(audit_value(ctx, kGateBase + 0), 0, "T6060.b 1143：门闭审计 0");
        CHECK_EQ(audit_value(ctx, kTickBase + 0), 1, "T6060.b 1143：窗 tick 1 次");
        const int mask = audit_value(ctx, kMaskBase + 0);
        CHECK_EQ(popcount4(mask), 2, "T6060.b 1143：抽取恰 2 种（mask=" +
                                         std::to_string(mask) + "）");
        const bool both_control =
            ((mask & (1 << 2)) != 0) && ((mask & (1 << 3)) != 0);
        int active = 0;
        for (int i = 0; i < 4; ++i) {
            if ((mask >> i) & 1) {
                active += anomaly_active(ctx, 1, kPool[i]) ? 1 : 0;
            }
        }
        if (both_control) {
            CHECK(active >= 1, "T6060.b 1143：双控场抽取 → 至少 1 个落地（替换语义）");
        } else {
            CHECK_EQ(active, 2, "T6060.b 1143：抽中的 2 种均落地");
        }
        std::cout << "  [T6060.b] 1143 门闭异常窗 通过 (mask=" << mask
                  << " active=" << active << ")\n";
    }

    // ══ T6060.c 窗长=1：R2 不施放 1143 → tick 不再增加、mask 不变 ══
    {
        ScenarioBattle b(make_party(self_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        sbs_srand(kSeed);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        const int mask_r1 = audit_value(ctx, kMaskBase + 0);
        CHECK_EQ(audit_value(ctx, kTickBase + 0), 1, "T6060.c 1143：R1 tick 1 次");
        // R2：改用拍打（不重新施放 1143），窗应已过期。
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(audit_value(ctx, kTickBase + 0), 1, "T6060.c 1143：R2 窗过期 → tick 仍 1");
        CHECK_EQ(audit_value(ctx, kMaskBase + 0), mask_r1, "T6060.c 1143：mask 不再变化");
        std::cout << "  [T6060.c] 1143 窗长 1 回合 通过\n";
    }

    std::cout << "scenario_6060 PASS (J273 异常门固伤/异常窗族)" << std::endl;
    return 0;
}
