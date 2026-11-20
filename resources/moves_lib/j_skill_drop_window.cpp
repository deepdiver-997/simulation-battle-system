// J 组 J302 · 用技双降/粉伤兜底窗族 —— 1393×8（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威）：
//   1393（args_num=4）"{0}回合内每回合使用技能则造成伤害前令对手防御-{1}、速度-{2}，
//   未触发则附加对手最大体力1/{3}的百分比伤害"
//
// 实现口径（决策树：①同形套路 → 1462/849 + 150/1531）：
//   - 窗口族 = **InRounds**（"{0}回合内"；custom_effect_overrides 无 1393 声明 →
//     EffectMetaCatalog 默认 InRounds）→ 生效 [R, R+{0}-1]，**施放当回合含在内**。
//     与 1462（同模板"回合内每回合使用技能"）同形：当回合在 SKILL_EFFECT 内联首跳，
//     后续 {0}-1 回合由窗 tick 补；tick 用 register_next_rounds_effect 显式取
//     start=R+1（1462 同款，后手不顺延——触发点是"自己使用技能"，不是回合开始）。
//   - 时点：施放当回合首跳在技能效果函数内（SKILL_EFFECT 阶段，ATTACK_DAMAGE 之前 →
//     降级对**本次攻击**的伤害生效）；后续回合 tick 挂 FIRST/SECOND **BEFORE_SKILL_HIT**
//     双桶（849 同模板"造成伤害前"同款）——桶只在 mover 侧执行 → "每回合使用技能"
//     天然成立（NONE/被控/嗑药 → 桶不跑，不结算）。
//     ⚠️ 实测勘误（2026-10-22，T6350.e/f）：普通 miss（accuracy=0）与被盔
//     （SEAL_ATTACK）回合，SKILL_EFFECT 桶**仍会执行**（registerSkillEffects 未被
//     这两条路径关掉）→ 两时点在本引擎行为等价；选 BEFORE_SKILL_HIT 仅因与 849
//     同形、且不依赖 registerSkillEffects 门（未来该门收紧时仍算"使用技能"）。
//     施放当回合首跳仍走 side_effect 通道（849/1531 同限）。
//   - 结算 = stat_drop(对手, 防御=2, {1}) + stat_drop(对手, 速度=4, {2})
//     （查免弱/附加禁令/钳 -6）。"造成伤害前"是时点，不额外要求载体有伤害。
//   - "未触发" = 有尝试的槽**全部未落地**（SUCCESS 才算落地；AT_FLOOR 钳 -6 与 IMMUNE
//     免弱都算未触发——2026-10-25 口径修订，官方实测灼眼焰华 2161 同族：对手 -6 也补；
//     INVALID_PARAM 是装配问题，同样不补）→ deal_pink_damage(对手最大体力/{3},
//     PERCENT_VALUE)（1255/774 同通道）。两槽行参都显式 0 = 双降关闸，无尝试 →
//     不补粉（"在位即消费"，不用默认值兜底）。
//   - 行参在位即消费：{1}/{2} 显式 0 = 该槽不降；{3} 显式 0 = 不给兜底。
//   - 审计槽：991393100+owner = 本次落地槽数；991393200+owner = 本次兜底粉伤；
//     991393300+owner = 累计应用次数（内联首跳 + 每次 tick）。
//
// 载体切片（side_effect.arg_count 优先、args_num 兜底，2026-10-22 脚本核算 8/8 自洽）：
//   **m27394 零度压迫 [1204,525,1393]|[1,3,30,3,1,1,3] → 1393=[3,1,1,3]（属性技，
//   场景载体；1204 给下次攻击必暴、525 给自侧 3回合30% 免攻伤——均不在本场景观测面）** /
//   m28421 灼光绮丽 → [3,1,1,3] / m32374 粉碎灭魂爪 → [3,2,2,3] /
//   m32587 无尽混沌 → [3,1,1,3] / m32673 魔·暴虐无道 → [3,1,1,3] /
//   m32924 妖兽狂魂破 → [3,1,1,3] / m33439 强磁暴干扰 → [3,1,1,3] /
//   m36212 丽影龙岐冲 → [3,1,1,3]。
//   4399 文案交叉验证：27394 "每回合使对手防御-1、速度-1"（"每回合"含施放回合；
//   与 InRounds 定版口径一致）。
//
// 动工前置三查（2026-10-22）：①内核 grep 1393 无行为硬编码（唯一命中
//   j_defense_windows 注释"1393 未实现哑"）②effect_info 仅本条模板、custom_effect_overrides/
//   programs 零行 ③moves_lib/soul_lib 并集（jbatch/kbatch 各 worktree + hub）无
//   SKILL_EFFECT(1393)——真缺口。
//
// 场景回归：test/scenario/scenario_6350_skill_drop_window.cpp（号段 6350-6359）。

#include "common.h"

namespace {

constexpr int kLandAuditBase = 991393100;    // +owner：本次落地槽数
constexpr int kPinkAuditBase = 991393200;    // +owner：本次兜底粉伤
constexpr int kApplyAuditBase = 991393300;   // +owner：累计应用次数

void j302_set_storage(BattleContext* ctx, int key, int value) {
    ctx->plugin_storage[key] = value;
}

int j302_storage(const BattleContext* ctx, int key) {
    const auto it = ctx->plugin_storage.find(key);
    if (it == ctx->plugin_storage.end()) {
        return 0;
    }
    return any_int_or(it->second, 0);
}

// 弱化是否"落地"：2026-10-25 口径——只有 SUCCESS（含压到 -6 为止的部分生效）算落地；
// AT_FLOOR（已 -6 钳底）与 IMMUNE（免弱挡）都算未落地 → 触发兜底。
// ⚠️ 混合档（一槽落地一槽钳底）按"任一槽落地即算触发"处理，官方未单独实测（记着）。
// INVALID_PARAM 是装配问题不补。
bool j302_drop_landed(StatDropResult r) {
    return r == StatDropResult::SUCCESS;
}

// ── 1393 一次结算：双降 + 未触发兜底粉伤（内联首跳与窗 tick 共用）──
void j302_apply_once(BattleContext* ctx, int owner, int opp, int def_down, int spd_down,
                     int denom) {
    if (!ctx || !g_core) {
        return;
    }
    int attempted = 0;
    int landed = 0;
    if (def_down > 0 && g_core->stat_drop) {
        ++attempted;
        if (j302_drop_landed(g_core->stat_drop(
                ctx, opp, static_cast<int>(NumericalPropertyIndex::DEFENSE), def_down, owner))) {
            ++landed;
        }
    }
    if (spd_down > 0 && g_core->stat_drop) {
        ++attempted;
        if (j302_drop_landed(g_core->stat_drop(
                ctx, opp, static_cast<int>(NumericalPropertyIndex::SPEED), spd_down, owner))) {
            ++landed;
        }
    }
    j302_set_storage(ctx, kLandAuditBase + owner, landed);

    int pink = 0;
    if (attempted > 0 && landed == 0 && denom > 0 && g_core->deal_pink_damage) {
        pink = max_hp_of(ctx, opp) / denom;
        if (pink > 0) {
            (void)g_core->deal_pink_damage(ctx, opp, pink, DamageKind::PERCENT_VALUE, owner);
        }
    }
    j302_set_storage(ctx, kPinkAuditBase + owner, pink);

    j302_set_storage(ctx, kApplyAuditBase + owner,
                     j302_storage(ctx, kApplyAuditBase + owner) + 1);
}

// ── 1393 窗 tick —— 使用技能时（BEFORE_SKILL_HIT，miss 也算）：双降 + 兜底粉伤 ──
// payload = [owner, opp, def_down, spd_down, denom, start]。
// start=R+1 显式卡窗起点（防御性守卫：晚到的桶条目也不会在施放当回合重复结算）。
EffectResult j302_1393_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 6) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int def_down = args.int_args[2];
    const int spd_down = args.int_args[3];
    const int denom = args.int_args[4];
    const int start = args.int_args[5];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || owner == opp) {
        return EffectResult::kOk;
    }
    if (ctx->roundCount < start) {
        return EffectResult::kOk;   // 注册当回合的桶已拾取新条目 → 首跳已在效果函数内联
    }
    j302_apply_once(ctx, owner, opp, def_down, spd_down, denom);
    return EffectResult::kOk;
}

}  // namespace

// ── 1393 —— {0}回合内每回合使用技能：对手防-{1}、速-{2}；未触发则 maxHP/{3} 粉伤 ──
// int_args = [owner, target, rounds, def_down, spd_down, denom]（args_num=4）。
EffectResult effect_1393_skill_drop_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1393, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 1393, args,
                     "1393 布局=[owner,target,回合,防降,速降,分母]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int def_down = args.int_args[3];
    const int spd_down = args.int_args[4];
    const int denom = args.int_args[5];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner, 1393,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0, 1393, args, "需要 回合 > 0");
    // 行参在位即消费：{1}/{2}/{3} 显式 0 合法（不降该槽 / 不给兜底）。
    EFFECT_ARG_GUARD(def_down >= 0 && spd_down >= 0 && denom >= 0, 1393, args,
                     "需要 防降/速降/分母 ≥ 0");

    // 施放当回合首跳（InRounds 含当回合；施放本身即"使用技能"，SKILL_EFFECT 在伤害前）。
    j302_apply_once(ctx, owner, opp, def_down, spd_down, denom);

    // 后续 {0}-1 回合 tick：窗 [R+1, R+{0}-1]（N=1 时无 tick）。
    // 挂 BEFORE_SKILL_HIT（849 同款；实测与 SKILL_EFFECT 在 miss/被盔路径等价）。
    if (rounds > 1) {
        const std::vector<int> payload{owner, opp, def_down, spd_down, denom,
                                       ctx->roundCount + 1};
        for (const State st :
             {State::BATTLE_FIRST_BEFORE_SKILL_HIT, State::BATTLE_SECOND_BEFORE_SKILL_HIT}) {
            (void)register_next_rounds_effect(ctx, st, owner, 1393, rounds - 1, payload,
                                              &j302_1393_tick);
        }
    }
    return EffectResult::kOk;
}

SKILL_EFFECT(1393, effect_1393_skill_drop_window)
