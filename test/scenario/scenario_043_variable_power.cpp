// 场景 043：变威力 —— 推翻第一次伤害计算、以当前状态重算第二次、第二次覆盖第一次。
//
// 官方口径（reference/scrape/out/all_posts_final.jsonl）：
//   · 定义（L128《官方名词解释—1》）：官方对"威力"的定义**本身就是**变威力规则——
//     「当命中效果对技能威力值进行修改时，**会以当前状态重新进行伤害结算覆盖原本的伤害值**」。
//   · 触发条件是"威力被**改过**"，不是"威力变了"：
//     L453「**提升0点威力也属于变威力**」、L173「就算是0连击，他也是变威力效果」。
//   · 重算的内容（L453）：敌我双方的能力等级（双攻/双防，**包含暴击破防**）、属性克制系数、技能威力。
//   · 唯一肉眼可见的后果（L418）：「暴击重置比较早，在技能特效生效之前就已经把对应的双防
//     给重置为0，然后再进行变威力计算」→ **只有变威力技能能无视对手的双防提升**；
//     非变威力技能本次伤害在破防之前就算完了，享受不到（用户 2026-09-13 同口径）。
//   · 靠后性（L390）：变威力"结算过于靠后，因此可以隔着盔打出攻击伤害"。
//
// 引擎落点：`apply_variable_power_recalc`（src/fsm/battleFsm.cpp）——在 ATTACK_DAMAGE 时点桶
// **之后**、伤害管线**之前**，若 `ws.skill_power_view[owner].rewritten` 被效果置位，就再跑一次
// `stage_simple_attack_damage`（开头清快照 + 末尾整体赋值 → 天然"覆盖"而非"叠加"）。
// 触发标记由 `SkillView::operator=`/`operator+=` 自动置位（效果侧零纪律成本）。
//
// ⚠️ **伤害管线仍然只跑一遍**（重算在管线之前）——所以不碰"跑两遍就错"的那些效果：
//    次数免伤 `consume_immune` 扣次数、概率挡伤 effect_525 重掷、effect_1236 再打一次粉伤、
//    `resolvedDamage.addPct` 累加。这是"重算快照、不重跑管线"这个落点的全部意义。
//
// 验证点：
//   V1 对照·**非变威力**技能暴击：破防照常发生，但本次伤害在破防之前已算完 → 不享受
//   V2 ★ 变威力技能暴击（威力 **+= 0**）：第二次按归零后的双防结算 → 伤害 ≈ V1 的 2 倍
//      （同时锁住"提升 0 点威力也算变威力"）
//   V3 变威力改威力：威力 120 → 220，伤害按新威力算
//   V4 强制执行打盔 + 变威力：视图被置 0 → 效果改回来 → **隔着盔打出红伤**
//   V5 标记生命周期：桶跑的时候标记还在（重算未发生）→ 重算消费后立即清零
#include <cassert>
#include <iostream>

#include <effects/effect.h>
#include <entities/pet_factory.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

constexpr int kQuanLi = 10033;   // 全力一击：物理、power 120、无 side_effect、crit_rate=0
constexpr int kGaoSu = 20017;    // 高速移动：属性技能（不还手、不掷暴击）

constexpr int kDefenseStat = 2;  // levels[2] = 防御（物理攻击走它）

scenario::ScenarioBattle make_battle() {
    auto p0 = scenario::make_pet(0, "攻方", {kQuanLi, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                 /*speed=*/300, /*atk=*/400, /*hp=*/3000);
    auto p1 = scenario::make_pet(1, "守方", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                 /*speed=*/100, /*atk=*/400, /*hp=*/3000);
    p0.skills[0].accuracy = 100;
    p0.skills[0].must_hit = true;   // 排除 miss 干扰（miss 会阻止暴击）
    return scenario::ScenarioBattle(scenario::make_party(p0), scenario::make_party(p1));
}

// 打一回合：0 用技能 0 打 1，1 用属性技能不还手。
void strike(scenario::ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
    b.drive();
}

//========== 变威力效果的探针 ==========
// 模拟技能自己的"威力提升"效果（如无相谛 179「若属性相同则技能威力提升170」）：
// 在 SKILL_EFFECT 时点改 `ws.skill_power_view`。**写 0 也算变威力**，所以 delta=0 也要注册。
int g_power_delta = 0;
int g_runs = 0;
EffectResult power_boost_effect(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    ctx->ws.skill_power_view[owner] += g_power_delta;   // `+=` 自动置"变威力"标记
    ++g_runs;
    return EffectResult::kOk;
}

// ATTACK_DAMAGE 桶里的探针：记录此刻"变威力"标记是否还挂着（重算发生在桶之后 → 应仍为 true）。
int g_flag_in_attack_bucket = -1;
EffectResult probe_flag_effect(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    g_flag_in_attack_bucket = ctx->ws.skill_power_view[args.int_args[0]].rewritten ? 1 : 0;
    return EffectResult::kOk;
}

// 在指定时点为 owner 注册一个一次性效果。
void register_effect(BattleContext& ctx, int owner, State state, EffectFn fn, int effect_id) {
    Effect e(effect_id, /*priority=*/0, owner, /*left_round=*/1,
             EffectArgs(std::vector<int>{owner, 1 - owner}), fn);
    auto ce = std::make_unique<ContinuousEffect>(e, state, owner, /*duration=*/1, ctx.roundCount);
    ce->source_id_ = effect_id;
    ctx.register_skill_effect(state, owner, std::move(ce));
}

// 变威力效果（SKILL_EFFECT 时点，默认时点）。
void register_power_boost(BattleContext& ctx, int owner, int delta) {
    g_power_delta = delta;
    register_effect(ctx, owner, State::BATTLE_FIRST_SKILL_EFFECT, &power_boost_effect, 999043);
}

// 给守方(1)挂一个可穿盔（模拟"对手给我挂盔"）：source=1、target=0。
void grant_armor(BattleContext& ctx, int effect_id) {
    seal_skill(&ctx, /*source=*/1, /*target=*/0, effect_id,
               /*attribute=*/false, /*attack=*/true, /*count=*/1,
               /*duration_rounds=*/0, /*penetrable=*/true,
               /*source_slot=*/-1, EffectScope::ON_STAGE, /*hit_invalid=*/false,
               /*chance_pct=*/100, /*consumed_when_pierced=*/true);
}

// 给守方摆上 +2 防御（本体 + 视图都要写：伤害公式读 ws.view_levels，破防写两边）。
void give_defense_boost(BattleContext& ctx, int level) {
    ctx.ability_levels[1][kDefenseStat] = level;
    ctx.ws.view_levels[1][kDefenseStat] = level;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    int dmg_no_vp = 0;   // V1 对照组（非变威力）

    //========== V1 对照：**非变威力**技能暴击 —— 破防发生，但本次伤害享受不到 ==========
    // 官方时点：破防在"第一次伤害公式之后"。非变威力技能没有第二次结算 → 本次伤害按
    // **未归零的 +2 防御**算（防御 100 → 200）。
    {
        scenario::ScenarioBattle b = make_battle();
        scenario::set_crit_sixteenths(b.ctx().getPet(0), 16);   // 必定暴击
        b.start();
        BattleContext& ctx = b.ctx();
        give_defense_boost(ctx, 2);

        const int hp_before = ctx.getPet(1).hp;
        strike(b);
        dmg_no_vp = hp_before - ctx.getPet(1).hp;

        CHECK_EQ(ctx.ability_levels[1][kDefenseStat], 0,
                 "V1) 破防照常发生（+2 防御正等级归 0）");
        CHECK_EQ(ctx.ws.skill_power_view[0].rewritten, false,
                 "V1) 没有效果改过威力 → 标记不置位（不触发重算）");
        std::cout << "  [V1] 非变威力暴击: 伤害=" << dmg_no_vp
                  << "（按 +2 防御算——破防在公式之后，本次已算完）\n";
    }

    //========== V2 ★ 变威力技能暴击：威力 += 0 也触发第二次结算 ==========
    // 唯一差别：注册了一个"只改 0 点威力"的效果。若引擎按"威力变了"判触发，这里不会重算，
    // 伤害应与 V1 相同；实际应 ≈ 2 倍（第二次按归零后的双防算）。一并锁住官方
    // 「提升0点威力也属于变威力」这条口径。
    {
        scenario::ScenarioBattle b = make_battle();
        scenario::set_crit_sixteenths(b.ctx().getPet(0), 16);
        b.start();
        BattleContext& ctx = b.ctx();
        give_defense_boost(ctx, 2);
        register_power_boost(ctx, 0, /*delta=*/0);   // ★ 只改 0 点威力

        const int hp_before = ctx.getPet(1).hp;
        strike(b);
        const int dmg_vp = hp_before - ctx.getPet(1).hp;

        CHECK_EQ(ctx.ability_levels[1][kDefenseStat], 0, "V2) 破防照常发生");
        CHECK(dmg_vp > dmg_no_vp * 3 / 2,
              "V2) ★ 变威力技能暴击 → 第二次结算按**归零后的双防**算，伤害显著高于对照组");
        std::cout << "  [V2] 变威力(威力+0)暴击: 伤害=" << dmg_vp << " vs 对照=" << dmg_no_vp
                  << "（比值 " << (dmg_no_vp > 0 ? dmg_vp * 100 / dmg_no_vp : 0) << "%）\n";
    }

    //========== V3 变威力改威力：伤害按**新威力**算 ==========
    {
        scenario::ScenarioBattle ctrl = make_battle();
        ctrl.start();
        const int hp_c = ctrl.ctx().getPet(1).hp;
        strike(ctrl);
        const int dmg_ctrl = hp_c - ctrl.ctx().getPet(1).hp;

        scenario::ScenarioBattle b = make_battle();
        b.start();
        register_power_boost(b.ctx(), 0, /*delta=*/100);   // 120 → 220
        const int hp_b = b.ctx().getPet(1).hp;
        strike(b);
        const int dmg_vp = hp_b - b.ctx().getPet(1).hp;

        CHECK_EQ(g_runs > 0, true, "V3) 威力效果确实跑过");
        // 阈值取 4/3：理论比值区间是 [1.56, 2.15]（两次结算各自掷 217~255 浮动），留足余量。
        CHECK(dmg_vp > dmg_ctrl * 4 / 3,
              "V3) 威力 120→220 → 伤害按新威力算（显著高于不改威力的对照）");
        std::cout << "  [V3] 变威力改威力: 伤害=" << dmg_vp << " vs 对照=" << dmg_ctrl << "\n";
    }

    //========== V4 强制执行打盔 + 变威力：隔着盔打出红伤 ==========
    // 强制执行把视图威力置 0（"有特效、无红伤"）；技能自己的变威力效果在 SKILL_EFFECT 改回来
    // → L390「变威力结算过于靠后，可以隔着盔打出攻击伤害」。
    {
        scenario::ScenarioBattle b = make_battle();
        b.start();
        BattleContext& ctx = b.ctx();
        grant_armor(ctx, /*effect_id=*/2270);
        ctx.getPet(0).skills[0].penetration_flags.force_execute = true;
        register_power_boost(ctx, 0, /*delta=*/120);   // 视图 0 → 120

        const int hp_before = ctx.getPet(1).hp;
        strike(b);
        const int dmg_vp = hp_before - ctx.getPet(1).hp;
        CHECK_EQ(ctx.ws.skill_exec_result[0], SkillExecResult::HIT,
                 "V4) 强制执行 → 盔不拦截、技能按 HIT 结算（效果照常注册）");
        CHECK(dmg_vp > 100, "V4) ★ 变威力把视图改回来 → 隔着盔打出正常红伤");
        std::cout << "  [V4] 强制执行打盔+变威力: 伤害=" << dmg_vp << "（红伤照常）\n";

        // 对照：同样打盔 + 强制执行，但**没有**变威力效果 → 视图停在 0 → 只剩公式常数项
        scenario::ScenarioBattle bc = make_battle();
        bc.start();
        BattleContext& c2 = bc.ctx();
        grant_armor(c2, 2270);
        c2.getPet(0).skills[0].penetration_flags.force_execute = true;
        const int hp_c = c2.getPet(1).hp;
        strike(bc);
        const int dmg_ctrl = hp_c - c2.getPet(1).hp;
        CHECK(dmg_ctrl <= 5, "V4) 对照：没有变威力 → 视图威力 0 → 只有常数项那一点点");
        std::cout << "  [V4] 对照（无变威力）: 伤害=" << dmg_ctrl << "（视图威力 0）\n";
    }

    //========== V5 标记生命周期：桶里还挂着 → 重算消费后立即清零 ==========
    // 桶跑在重算**之前**（标记应仍为 true）；`apply_variable_power_recalc` 消费后必须清零
    // （否则下个技能会白重算一次）。这里用 debug 单步卡在 ATTACK_DAMAGE 之后看。
    {
        scenario::ScenarioBattle b = make_battle();
        b.start();
        BattleContext& ctx = b.ctx();
        register_power_boost(ctx, 0, /*delta=*/0);
        g_flag_in_attack_bucket = -1;
        register_effect(ctx, 0, State::BATTLE_FIRST_ATTACK_DAMAGE, &probe_flag_effect, 999044);

        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.step_past(State::BATTLE_FIRST_ATTACK_DAMAGE);

        CHECK_EQ(g_flag_in_attack_bucket, 1,
                 "V5) ATTACK_DAMAGE 桶跑的时候标记还在（重算尚未发生）");
        CHECK_EQ(ctx.ws.skill_power_view[0].rewritten, false,
                 "V5) 重算消费后标记立即清零（不会泄漏到下一个技能/回合）");
        std::cout << "  [V5] 标记生命周期: 桶内=" << g_flag_in_attack_bucket
                  << "、重算后=" << ctx.ws.skill_power_view[0].rewritten << "（已清零）\n";
    }

    std::cout << "\n场景 043 通过：变威力 = 推翻第一次伤害计算、以当前状态（含暴击破防后的双防 / "
                 "当前能力等级 / 当前克制系数 / 当前威力）重算第二次、第二次覆盖第一次；"
                 "触发条件是\"威力被改过\"（提升 0 点也算）；伤害管线仍只跑一遍。\n";
    return 0;
}
