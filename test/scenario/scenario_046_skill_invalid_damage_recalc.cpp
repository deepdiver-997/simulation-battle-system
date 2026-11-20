// 场景 046：技能无效出口的"变威力"伤害结算 —— effect 2501「技能无效时，重新进行伤害结算」。
//
// 官方口径（reference/scrape/out/all_posts_final.jsonl）：
//   · L95《机制解析—圣光莫妮卡（神谕）》：「"乐园之初诞"中技能无效效果，**相当于打盔会重新计算
//     攻击伤害，类似于变威力技能的重新计算**，但是**技能效果还是无效，只有攻击伤害**」
//   · L390《机制解析—索杰德尔》：「**由于变威力结算过于靠后，因此可以隔着盔打出攻击伤害**」
//
// 数据：技能 38421 乐园之初诞（薇尔诗）`side_effect=[2490, 946, 1892, 843, 2501]`，2501 原文
//   「技能无效时，重新进行伤害结算且自身每有260点特攻值威力翻倍1次」（args_num=0 → 260 写死在文案里）。
//
// 引擎链路（★ 2026-09-14 补上断掉的一环）：
//   query_usage ② 门判定被盔拦下 → 返回 SKILL_INVALID（`allowAttackDamagePipeline=false`）
//     → **SKILL_INVALID 分支照常注册**（`registerSkillEffects` 仍为 true）
//     → `BATTLE_FIRST_SKILL_EFFECT` 时点跑 2501 → 改写 `ws.skill_power_view`（写入即置"变威力"标记）
//     → `handle_BattleFirstAttackDamage` 的**早退出口**调
//       `run_attack_damage_from_invalid_skill` → 变威力检查点成立 → 重算 + 管线 + 落地。
//   ⚠️ 以前这里是**断的**：早退出口在 `apply_variable_power_recalc` 之前就 return，
//      视图被改了也没人重算。
//
// 验证点：
//   V1 ★ 打盔 + 2501：技能被判无效（SKILL_INVALID），但**照常打出红伤**（隔着盔）
//   V2 ★ "技能效果还是无效"：只有 SKILL_INVALID 分支注册了效果（HIT 分支的 843 不注册）
//   V3 特攻每满 260 翻一倍：520 特攻 → 威力 50→200（翻 2 次）；< 260 → 不翻、零伤害
//   V4 对照：同一顶盔拦下一个**没有**变威力词条的技能 → 伤害为 0（这条路不会白给伤害）
#include <cassert>
#include <iostream>

#include <effects/effect.h>
#include <entities/pet_factory.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

constexpr int kLeYuan = 38421;    // 乐园之初诞：特殊、power 50、side_effect 含 2501
constexpr int kZhuangJi = 10001;  // 撞击：物理，无变威力词条（对照）
constexpr int kGaoSu = 20017;     // 高速移动：属性技能（不还手）

constexpr int kArmorEffectId = 2270;   // 盔模板代理 id

// 探针：在 AFTER_ACTION 读视图威力（此时技能无效出口的改写已完成，且回合还没结束）。
int g_power_view_after = -999;
EffectResult probe_power_view(BattleContext* ctx, const EffectArgs&) {
    if (ctx) {
        g_power_view_after = static_cast<int>(ctx->ws.skill_power_view[0]);
    }
    return EffectResult::kOk;
}

// 把 restraint_view 钉成 1.0：被测技能系别是 20，与守方系别 0 的克制倍率取决于 DB 表，
// 本场景测的是"变威力重算"这条链路，不希望伤害被克制倍数搅乱。
// ⚠️ 必须在 SKILL_EFFECT 时点写（回合开始 ws.reset() 是 memset 会冲掉）。
EffectResult pin_restraint(BattleContext* ctx, const EffectArgs&) {
    if (ctx) {
        ctx->ws.restraint_view[0] = 1.0;
    }
    return EffectResult::kOk;
}

scenario::ScenarioBattle make_battle(int attacker_skill, int sp_atk) {
    auto p0 = scenario::make_pet(0, "攻方", {attacker_skill, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                 /*speed=*/300, sp_atk, /*hp=*/3000);
    auto p1 = scenario::make_pet(1, "守方", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                 /*speed=*/100, /*atk=*/100, /*hp=*/3000);
    p0.skills[0].accuracy = 100;
    p0.skills[0].must_hit = true;
    return scenario::ScenarioBattle(scenario::make_party(p0), scenario::make_party(p1));
}

// 给守方(1)挂一个可穿盔挂在攻方(0)身上（模拟"对手给我挂盔"）：source=1、target=0。
void grant_armor(BattleContext& ctx) {
    seal_skill(&ctx, /*source=*/1, /*target=*/0, kArmorEffectId,
               /*attribute=*/false, /*attack=*/true, /*count=*/1,
               /*duration_rounds=*/0, /*penetrable=*/true,
               /*source_slot=*/-1, EffectScope::ON_STAGE, /*hit_invalid=*/false,
               /*chance_pct=*/100, /*consumed_when_pierced=*/true);
}

void register_effect(BattleContext& ctx, int owner, State state, EffectFn fn, int effect_id) {
    Effect e(effect_id, /*priority=*/0, owner, /*left_round=*/1,
             EffectArgs(std::vector<int>{owner, 1 - owner}), fn);
    auto ce = std::make_unique<ContinuousEffect>(e, state, owner, /*duration=*/1, ctx.roundCount);
    ce->source_id_ = effect_id;
    ctx.register_skill_effect(state, owner, std::move(ce));
}

void strike(scenario::ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
    b.drive();
}

// 单段伤害的理论区间（restraint 钉成 1.0、无暴击、无增伤减伤）：
//   (0.84 × 特攻/特防 × 威力 + 2) × 浮动[217,255]/255
void expect_range(int sp_atk, int power, int* lo, int* hi) {
    const double raw = 0.84 * sp_atk / 100.0 * power + 2.0;
    *lo = static_cast<int>(raw * 217.0 / 255.0) - 2;
    *hi = static_cast<int>(raw) + 2;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== V1/V2/V3 打盔 + 2501：技能无效但照常打出红伤 ==========
    {
        scenario::ScenarioBattle b = make_battle(kLeYuan, /*sp_atk=*/520);
        b.start();
        BattleContext& ctx = b.ctx();
        grant_armor(ctx);
        register_effect(ctx, 0, State::BATTLE_FIRST_SKILL_EFFECT, &pin_restraint, 999046);
        g_power_view_after = -999;
        register_effect(ctx, 0, State::BATTLE_FIRST_AFTER_ACTION, &probe_power_view, 999047);

        const int hp_before = ctx.getPet(1).hp;
        strike(b);
        const int dmg = hp_before - ctx.getPet(1).hp;

        CHECK_EQ(ctx.ws.skill_exec_result[0], SkillExecResult::SKILL_INVALID,
                 "V1) 技能被判无效（盔拦下）");
        CHECK_EQ(ctx.ws.skill_resolution_flags[0].allowAttackDamagePipeline, false,
                 "V2) allowAttackDamagePipeline=false —— 正常伤害路径整段早退");
        CHECK_EQ(ctx.ws.skill_resolution_flags[0].registerSkillEffects, true,
                 "V2) ★ 但 registerSkillEffects 仍为 true → SKILL_INVALID 分支照常注册"
                 "（这就是「技能效果无效 ≠ 本效果不跑」的机制）");
        CHECK_EQ(g_power_view_after, 200,
                 "V3) ★ 2501 按特攻翻倍：520/260 = 2 次 → 视图威力 50→200");
        int lo = 0, hi = 0;
        expect_range(/*sp_atk=*/520, /*power=*/200, &lo, &hi);
        CHECK(dmg >= lo && dmg <= hi,
              "V1) ★ 隔着盔打出红伤，且落在「威力 200 单段」的区间内");
        std::cout << "  [V1] 打盔 + 2501: 技能无效但伤害=" << dmg
                  << "（威力 50→200，理论区间 " << lo << "~" << hi << "）\n";
        std::cout << "  [V3] 视图威力 50→200（520 特攻 / 260 = 翻 2 次）\n";
    }

    //========== V3b 特攻不足 260：不翻倍 → 不产生变威力标记 → 打盔零伤害 ==========
    {
        scenario::ScenarioBattle b = make_battle(kLeYuan, /*sp_atk=*/200);
        b.start();
        BattleContext& ctx = b.ctx();
        grant_armor(ctx);
        register_effect(ctx, 0, State::BATTLE_FIRST_SKILL_EFFECT, &pin_restraint, 999046);
        g_power_view_after = -999;
        register_effect(ctx, 0, State::BATTLE_FIRST_AFTER_ACTION, &probe_power_view, 999047);

        const int hp_before = ctx.getPet(1).hp;
        strike(b);
        const int dmg = hp_before - ctx.getPet(1).hp;
        CHECK_EQ(ctx.ws.skill_exec_result[0], SkillExecResult::SKILL_INVALID, "V3b) 技能仍无效");
        CHECK_EQ(g_power_view_after, 50, "V3b) 特攻 200 < 260 → 一次都不翻，视图威力停在 50");
        CHECK_EQ(dmg, 0, "V3b) 没有「变威力」改写 → 打盔就是零伤害（这条路不会白给伤害）");
        std::cout << "  [V3b] 特攻不足 260: 不翻倍 → 打盔伤害=" << dmg << "（零）\n";
    }

    //========== V4 对照：没有变威力词条的技能被同一顶盔拦下 → 零伤害 ==========
    {
        scenario::ScenarioBattle b = make_battle(kZhuangJi, /*sp_atk=*/520);
        b.start();
        BattleContext& ctx = b.ctx();
        grant_armor(ctx);
        register_effect(ctx, 0, State::BATTLE_FIRST_SKILL_EFFECT, &pin_restraint, 999046);

        const int hp_before = ctx.getPet(1).hp;
        strike(b);
        const int dmg = hp_before - ctx.getPet(1).hp;
        CHECK_EQ(ctx.ws.skill_exec_result[0], SkillExecResult::SKILL_INVALID, "V4) 技能无效");
        CHECK_EQ(dmg, 0, "V4) 对照：撞击无变威力词条 → 被盔拦下就是零伤害");
        std::cout << "  [V4] 对照（无变威力词条）: 打盔伤害=" << dmg << "（零）\n";
    }

    //========== V5 分支归属：技能**命中**时 2501 不跑 ==========
    // 2501 只登记在 SKILL_INVALID 分支（`default_branch_for_effect`），所以技能正常命中时它
    // **不执行** → 视图威力保持 50。这条锁"分支归属正确"——错放到 HIT 分支就等于给全技能白送翻倍。
    {
        scenario::ScenarioBattle b = make_battle(kLeYuan, /*sp_atk=*/520);
        b.start();
        BattleContext& ctx = b.ctx();
        register_effect(ctx, 0, State::BATTLE_FIRST_SKILL_EFFECT, &pin_restraint, 999046);
        g_power_view_after = -999;
        register_effect(ctx, 0, State::BATTLE_FIRST_AFTER_ACTION, &probe_power_view, 999047);

        const int hp_before = ctx.getPet(1).hp;
        strike(b);
        const int dmg = hp_before - ctx.getPet(1).hp;
        CHECK_EQ(ctx.ws.skill_exec_result[0], SkillExecResult::HIT, "V5) 无盔 → 技能正常命中");
        CHECK_EQ(g_power_view_after, 50, "V5) ★ 技能命中时 2501 **不跑**（只在 SKILL_INVALID 分支）");
        int lo = 0, hi = 0;
        expect_range(520, 50, &lo, &hi);
        // 2026-09-17 批 2 起 effect 946（全属性+N）生效——乐园之初诞词条之一：HIT 时
        // 攻方全属性+1 → 特攻 ×1.5。打盔的 V1 走 SKILL_INVALID 分支、946 不跑 → 区间不变。
        lo = lo * 3 / 2;
        hi = hi * 3 / 2;
        // 2026-09-20 起 effect 1892（乐园之初诞词条之一）实现生效：HIT 时挂
        // "4回合内每回合使用技能吸取300"窗口，本回合 AFTER_ACTION 即吸 300 → dmg +300。
        // （V1 打盔走 SKILL_INVALID 分支、1892 不注册 → 无吸取，区间不变。）
        lo += 300;
        hi += 300;
        CHECK(dmg >= lo && dmg <= hi, "V5) 伤害按原始威力 50 算（含 946 特攻+1 的 ×1.5、1892 吸取 +300）");
        std::cout << "  [V5] 命中时 2501 不跑: 视图威力=" << g_power_view_after
                  << "、伤害=" << dmg << "\n";
    }

    std::cout << "\n场景 046 通过：技能无效出口也接上了「变威力」检查点——SKILL_INVALID 分支的效果在\n"
                 "SKILL_EFFECT 时点改写视图威力（2501 按特攻翻倍）→ 隔着盔重算并打出红伤，\n"
                 "而技能效果本身仍然无效。\n";
    return 0;
}
