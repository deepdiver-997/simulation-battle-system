// 场景 5910 —— J260 天敌控制兜底闪避窗族（1489，moves_lib/j_predator_dodge_fallback.cpp）
//（2026-10-22，worktree feat/jbatch；号段 5910-5919）
//
// 断言口径（审计槽 + 精确命中率乘区 + 同类型门闭对照）：
//   - 1489（28235 雾弥抚骨 [1489]|[100,28,2,70]）：天敌关系 → 100% 申报施加束缚(28)
//     → 落地审计 1、hit_rate_mod[对手] 保持 1.0（主句落地不开闪避窗）。
//   - 非天敌 → 天敌门审计 0，主句/兜底都不动作（无异常、无乘区）；**且对手攻击命中**
//     （证明 A 局"没有可观测闪避"不是因为对手没出手）。
//   - 天敌 + 对手异常免疫票 → 申报 100% 被挡（落地审计 0）→ 闪避窗：当回合
//     hit_rate_mod[对手] = 0.3（70% 档），R2 tick 重写 0.3，R3 过期回到 1.0
//     （{2}=2 回合窗精确：R1+R2）。
//   - 36569 悼 [1489,842]|[50,7,1,100,75]：兜底 100% 闪避档 —— 用免疫票强制未触发，
//     R1 对手攻击**必 miss**（自身 HP 不变）；R2 窗过期（{2}=1）后攻击落地（HP 下降）。
//     842（有强化才增伤）在无强化时为空转，不干扰。
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <fsm/state.h>
#include <plugin/core_api.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;        // 拍打：物攻 40、accuracy 100、非必中
constexpr int kCarrierA = 28235;     // 雾弥抚骨 [1489]|[100,28,2,70]
constexpr int kCarrierB = 36569;     // 悼 [1489,842]|[50,7,1,100,75] → 1489=[50,7,1,100]
constexpr int kMaxHp = 99999;
constexpr int kBind = 28;            // 束缚
constexpr int kImmuneSource = 7901;  // 场景私有免疫票句柄（非 registry 号段）

constexpr int kPredatorAuditBase = 996489100;   // +owner
constexpr int kLandedAuditBase = 996489200;     // +owner

struct PredatorPair {
    int self_type = -1;
    int foe_type = -1;
};

PredatorPair find_predator_pair() {
    PredatorPair p;
    for (int a = 0; a <= 30 && p.self_type < 0; ++a) {
        for (int d = 0; d <= 30; ++d) {
            const int self_pair[2] = {a, 0};
            const int foe_pair[2] = {d, 0};
            if (core_api().restraint_multiplier(foe_pair, self_pair) > 1.0) {
                p.self_type = a;
                p.foe_type = d;
                return p;
            }
        }
    }
    return p;
}

void set_accuracy(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = must_hit ? 1 : 0;
    }
}

ElfPet mk_self(int carrier, std::array<int, 2> types) {
    ElfPet p = make_pet(1, "己方", {carrier, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, /*hp=*/kMaxHp);
    p.elementalAttributes = types;
    set_accuracy(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe(std::array<int, 2> types) {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/300, /*hp=*/kMaxHp);
    p.elementalAttributes = types;
    set_accuracy(p, /*must_hit=*/false);   // 非必中：闪避乘区才能观测到 miss
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

bool near_val(float a, float b) {
    return std::fabs(a - b) < 1e-4f;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    const PredatorPair pair = find_predator_pair();
    CHECK(pair.self_type >= 0 && pair.foe_type >= 0, "找到一对天敌克制组合");
    std::cout << "  [setup] 天敌组合 self=" << pair.self_type << " foe=" << pair.foe_type
              << std::endl;

    const std::array<int, 2> self_types{pair.self_type, 0};
    const std::array<int, 2> foe_types{pair.foe_type, 0};

    // ══ A：天敌 + 100% 申报 → 主句落地，不开闪避窗 ══
    {
        ScenarioBattle b(make_party(mk_self(kCarrierA, self_types)),
                         make_party(mk_foe(foe_types)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();

        CHECK_EQ(audit(ctx, kPredatorAuditBase + 0), 1, "A1 天敌门审计=1");
        CHECK_EQ(audit(ctx, kLandedAuditBase + 0), 1, "A2 主句落地审计=1（100% 档）");
        CHECK(ctx.has_active_abnormal_status(1, kBind), "A3 对手身上有束缚(28)");
        CHECK(near_val(ctx.ws.hit_rate_mod[1], 1.0f), "A4 主句落地 → 命中率乘区保持 1.0（无闪避窗）");
        std::cout << "  [A] 主句落地：束缚挂上、乘区 1.0\n";
    }

    // ══ B：天敌门闭（无克制）→ 整条不动作 ══
    {
        const std::array<int, 2> neutral{0, 0};
        ScenarioBattle b(make_party(mk_self(kCarrierA, neutral)),
                         make_party(mk_foe(neutral)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int hp0 = ctx.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();

        CHECK_EQ(audit(ctx, kPredatorAuditBase + 0), 0, "B1 非天敌 → 门审计=0");
        CHECK_EQ(audit(ctx, kLandedAuditBase + 0), -999, "B2 门闭时主句路径未执行（无落地审计）");
        CHECK(!ctx.has_active_abnormal_status(1, kBind), "B3 门闭 → 不给对手上束缚");
        CHECK(near_val(ctx.ws.hit_rate_mod[1], 1.0f), "B4 门闭 → 命中率乘区 1.0");
        CHECK(ctx.getPet(0).hp < hp0, "B5 对手攻击命中（证明门闭局确实有攻击可观测）");
        std::cout << "  [B] 天敌门闭：无异常、无乘区、对手正常命中\n";
    }

    // ══ C：天敌 + 异常免疫票（强制未触发）→ 70% 闪避窗 {2}=2 回合 ══
    {
        ScenarioBattle b(make_party(mk_self(kCarrierA, self_types)),
                         make_party(mk_foe(foe_types)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.grant_immunity(/*owner=*/1, ImmunityType::ANOMALY, coverage_all(),
                           /*anomaly_mask=*/0, /*duration_rounds=*/0,
                           /*source_id=*/kImmuneSource, /*soul_immunity=*/true);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();

        CHECK_EQ(audit(ctx, kPredatorAuditBase + 0), 1, "C1 天敌门审计=1");
        CHECK_EQ(audit(ctx, kLandedAuditBase + 0), 0, "C2 免疫票挡下 → 落地审计=0");
        CHECK(!ctx.has_active_abnormal_status(1, kBind), "C3 束缚被免疫，未挂上");
        CHECK(near_val(ctx.ws.hit_rate_mod[1], 0.3f), "C4 兜底窗当回合：对手命中率乘区=0.3（70% 闪避）");

        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(near_val(ctx.ws.hit_rate_mod[1], 0.3f), "C5 R2 窗内 tick 重写 0.3");

        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(near_val(ctx.ws.hit_rate_mod[1], 1.0f), "C6 R3 窗过期（{2}=2 回合）乘区回到 1.0");
        std::cout << "  [C] 强制未触发：R1/R2 乘区 0.3、R3 归 1.0\n";
    }

    // ══ D：100% 闪避档（36569 {2}=1）→ R1 必 miss，R2 窗过期后命中 ══
    {
        ScenarioBattle b(make_party(mk_self(kCarrierB, self_types)),
                         make_party(mk_foe(foe_types)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.grant_immunity(/*owner=*/1, ImmunityType::ANOMALY, coverage_all(),
                           /*anomaly_mask=*/0, /*duration_rounds=*/0,
                           /*source_id=*/kImmuneSource, /*soul_immunity=*/true);
        const int hp0 = ctx.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();

        CHECK_EQ(audit(ctx, kLandedAuditBase + 0), 0, "D1 免疫票挡下 → 兜底生效");
        CHECK(near_val(ctx.ws.hit_rate_mod[1], 0.0f), "D2 100% 闪避档 → 乘区 0.0");
        CHECK_EQ(ctx.getPet(0).hp, hp0, "D3 R1 对手攻击必 miss（自身 HP 不变）");

        const int hp1 = ctx.getPet(0).hp;   // 提交操作即推进 FSM，须在 submit 前拍快照
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(near_val(ctx.ws.hit_rate_mod[1], 1.0f), "D4 R2 窗过期（{2}=1 回合）乘区回到 1.0");
        CHECK(ctx.getPet(0).hp < hp1, "D5 R2 对手攻击恢复命中（HP 下降）");
        std::cout << "  [D] 100% 闪避：R1 miss 不变、R2 恢复命中\n";
    }

    std::cout << "scenario_5910 PASS (J260 天敌控制兜底闪避窗族)" << std::endl;
    return 0;
}
