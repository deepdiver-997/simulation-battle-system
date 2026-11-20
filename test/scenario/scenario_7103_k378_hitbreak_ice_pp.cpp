#include <utils/sbs_rand.h>
// 场景 7103 —— K378 消回合类+冰封/PP归零/先制解降族
//（1355，soul_lib/k_hitbreak_ice_pp_preempt.cpp；号段 7103，2026-10-24 worktree ../agents/k378）
//
// 断言口径（概率走审计槽精确计数；先制走 K220 3620 debug_step FMR 停泊直读）：
//   - a 消回合类+50% 冰封（row1057）：对手名下挂回合类封属票（seal_skill 空类别
//     直调——仅作"有无可断物"存在性，不拦技能）后我方攻击命中 → break SUCCESS →
//     50% 冰封申报：20 回合审计落地 ∈ [3,17]（p=0.5 期望 10）+ 冰封状态至少观测
//     一次（Icebound=15 已建模）；
//   - b 无可断物不掷点：不挂票 → 命中后 break=NO_EFFECTS → 审计键缺席（未申报）；
//   - c PP 门（row1650）：对手置负等级 → 次回合攻击命中 → 对手恰 1 个技能槽 PP=0
//     （db[3]=1，K230 循环 SUCCESS 计数）；门关对照 0 槽；
//   - d 先制+2 + 解除（row1650 db[2]=2）：自身置负等级 → 次回合 FMR 停泊读
//     preemptive_level==2 + 战后自身负等级全部复位到 0；
//   - e row1057 先制+1 互锚：同 d 形状读数 ==1。
//   ⚠️ 概率断言跑前 srand 定种子。时点模型实测：ROUND_START/FMR 桶在**每次
//   drive() 开头**触发（battle start 不触发当回合 ROUND_START）——降属置位要在
//   drive 之前写、采样发生在下一次 drive（T7103.d 首版时点勘误）。
#include <any>
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

constexpr int kPaiDa = 10004;   // 拍打：物理攻击 filler
constexpr int kIce = static_cast<int>(AbnormalStatusId::Icebound);   // 15
constexpr int kMaxHp = 800000;
constexpr int kSlots = BattleContext::kAbilityLevelSlotCount;
constexpr int kAuditPctKey = 990378800;      // +owner*2（owner=0）

SoulMark make_mark(const std::vector<int>& db_args) {
    SoulMark sm(/*effect_id=*/1355, "K1355 魂印", "test",
                EffectArgs(std::vector<int>(db_args)));
    sm.monster_id = 3778;
    return sm;
}

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 0;
    }
}

ElfPet mark_pet(const std::vector<int>& db_args) {
    ElfPet p = make_pet(1, "印主", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/300, kMaxHp, make_mark(db_args));
    accuracy_100(p);
    return p;
}

ElfPet foe_pet(int atk) {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, atk, kMaxHp);
    accuracy_100(p);
    return p;
}

// 印主攻击、对手 NONE；返回当刀伤害
int holder_strike(ScenarioBattle& b) {
    BattleContext& ctx = b.ctx();
    const int hp_before = ctx.getPet(1).hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
    return hp_before - ctx.getPet(1).hp;
}

int audit_int(BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    return it == ctx.plugin_storage.end() ? -1 : std::any_cast<int>(it->second);
}

bool anomaly_landed(BattleContext& ctx) {
    const int res = audit_int(ctx, kAuditPctKey + 1);
    return res == static_cast<int>(ApplyAnomalyResult::SUCCESS)
        || res == static_cast<int>(ApplyAnomalyResult::DURATION_EXTENDED)
        || res == static_cast<int>(ApplyAnomalyResult::REPLACED_EXISTING);
}

int zero_pp_slots(BattleContext& ctx) {
    int zero = 0;
    for (const Skills& s : ctx.getPet(1).skills) {
        if (s.maxPP > 0 && s.pp == 0) {
            ++zero;
        }
    }
    return zero;
}

bool all_zero(BattleContext& ctx, int side) {
    for (int i = 0; i < kSlots; ++i) {
        if (ctx.ability_levels[side][i] != 0) {
            return false;
        }
    }
    return true;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7103.a 消回合类成功 → 50% 冰封（审计槽精确计数）═════════════════════
    {
        sbs_srand(20261024);
        ScenarioBattle b(make_party(mark_pet({50, 1, 1, 1})),
                         make_party(foe_pet(/*atk=*/1)));
        b.start();
        BattleContext& ctx = b.ctx();
        int landed = 0;
        bool ice_seen = false;
        for (int r = 0; r < 20; ++r) {
            // 对手自封（source=target=1）：票绑定施放方=对手 → has_round_type(1)
            // 成立（可断物）；封的是对手自己的技能——对手 NONE 不受影响，我方攻击
            // 不被拦。空类别（attack=attribute=false）被 seal_skill 卫语句拒收
            //（battle_primitives.cpp:1324，首版实测）。
            seal_skill(&ctx, /*source=*/1, /*target=*/1, /*effect_id=*/70001,
                       /*attribute=*/true, /*attack=*/true, /*count=*/1,
                       /*duration_rounds=*/2, /*penetrable=*/false);
            holder_strike(b);
            if (anomaly_landed(ctx)) {
                ++landed;
            }
            if (ctx.has_active_abnormal_status(1, kIce)) {
                ice_seen = true;
            }
        }
        CHECK(audit_int(ctx, kAuditPctKey) == 50,
              "T7103.a 审计槽申报概率应=50（实测 " << audit_int(ctx, kAuditPctKey)
                  << "）");
        CHECK(landed >= 3 && landed <= 17,
              "T7103.a 20 次命中 50% 冰封落地 ∈ [3,17]（期望 10，实测 "
                  + std::to_string(landed) + "）");
        CHECK(ice_seen, "T7103.a 冰封状态至少观测一次（Icebound=15 已建模）");
        std::cout << "  [T7103.a] 1355 消回合类+50%冰封（审计计数 " << landed
                  << "/20）通过\n";
    }

    // ══ T7103.b 无可断物不掷点 ═══════════════════════════════════════════════
    {
        sbs_srand(20261025);
        ScenarioBattle b(make_party(mark_pet({50, 1, 1, 1})),
                         make_party(foe_pet(/*atk=*/1)));
        b.start();
        const int dmg = holder_strike(b);
        CHECK(dmg > 0, "T7103.b 命中前提不成立：dmg=" << dmg);
        CHECK(b.ctx().plugin_storage.find(kAuditPctKey)
                      == b.ctx().plugin_storage.end(),
              "T7103.b break=NO_EFFECTS 不应申报冰封（无审计写入）");
        CHECK(!b.ctx().has_active_abnormal_status(1, kIce),
              "T7103.b 无可断物对手不应冰封");
        std::cout << "  [T7103.b] 1355 NO_EFFECTS 不掷点 通过\n";
    }

    // ══ T7103.c PP 门（row1650）：对手降属 → 攻击命中 PP 归零 1 槽 ════════════
    {
        sbs_srand(20261026);
        ScenarioBattle b(make_party(mark_pet({80, 1, 2, 1})),
                         make_party(foe_pet(/*atk=*/1)));
        b.start();
        BattleContext& ctx = b.ctx();
        holder_strike(b);   // r1：门关（对手无降属）——0 槽归零
        CHECK_EQ(zero_pp_slots(ctx), 0, "T7103.c r1 门关不应归零 PP");
        ctx.ability_levels[1][0] = -1;   // 对手置负等级（持久，次回合采样）
        holder_strike(b);   // r2：ROUND_START 采样门开 → 攻击命中 → PP 归零
        CHECK_EQ(zero_pp_slots(ctx), 1,
                 "T7103.c 门开应恰归零 1 槽（db[3]=1，实测 "
                     << zero_pp_slots(ctx) << "）");
        std::cout << "  [T7103.c] 1355 PP 门归零 1 槽 通过\n";
    }

    // ══ T7103.d 先制+2 + 解除自身能力下降（row1650 db[2]=2）══════════════════
    {
        sbs_srand(20261027);
        ScenarioBattle b(make_party(mark_pet({80, 1, 2, 1})),
                         make_party(foe_pet(/*atk=*/1)));
        b.start();
        BattleContext& ctx = b.ctx();
        holder_strike(b);   // r1：门关（自身无降属）
        for (int i = 0; i < kSlots; ++i) {
            ctx.ability_levels[0][i] = -1;   // 自身置负等级（ROUND_START 逐 drive
                                             // 触发——次回合采样，首版时点勘误）
        }
        // r2：debug_step 停泊 FMR 读先制
        ctx.debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        ctx.debug_step_mode = false;
        b.step_past(State::BATTLE_FIRST_MOVE_RIGHT);
        const int pre = ctx.ws.preemptive_level[0];
        b.drive();
        CHECK_EQ(pre, 2, "T7103.d FMR 先制应=2（实测 " << pre << "）");
        CHECK(all_zero(ctx, 0), "T7103.d 自身负等级应全部复位（实测 "
                  + std::to_string(ctx.ability_levels[0][0]) + "...）");
        std::cout << "  [T7103.d] 1355 先制+2+解除降属 通过\n";
    }

    // ══ T7103.e row1057 先制+1 互锚 ══════════════════════════════════════════
    {
        sbs_srand(20261028);
        ScenarioBattle b(make_party(mark_pet({50, 1, 1, 1})),
                         make_party(foe_pet(/*atk=*/1)));
        b.start();
        BattleContext& ctx = b.ctx();
        holder_strike(b);   // r1：门关
        for (int i = 0; i < kSlots; ++i) {
            ctx.ability_levels[0][i] = -1;
        }
        ctx.debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        ctx.debug_step_mode = false;
        b.step_past(State::BATTLE_FIRST_MOVE_RIGHT);
        const int pre = ctx.ws.preemptive_level[0];
        b.drive();
        CHECK_EQ(pre, 1, "T7103.e FMR 先制应=1（实测 " << pre << "）");
        CHECK(all_zero(ctx, 0), "T7103.e 自身负等级应全部复位");
        std::cout << "  [T7103.e] 1355 row1057 先制+1 互锚 通过\n";
    }

    std::cout << "scenario_7103 PASS (K378 消回合类+冰封/PP归零/先制解降族)"
              << std::endl;
    return 0;
}
