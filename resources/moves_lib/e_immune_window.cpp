// E 组批产 · E4 减伤/免疫窗口族（2026-09-29；工单：docs_local/docs/05-任务清单/E组批量效果工单.md）
//
//   5 效果一个家族模板、各一个 SKILL_EFFECT：
//     44  {0}回合自己受到特殊攻击伤害减半       （切片 [rounds]，如 水流冲刷 11854）
//     50  {0}回合自身受到物理攻击伤害减半       （切片 [rounds]，如 雷电冲撞 11268）
//     127 {0}%的概率，{1}回合内受到的伤害减半    （切片 [prob,rounds]，如 光羽断空闪 [100,2]）
//     576 {0}回合内免疫低于{1}的攻击伤害         （切片 [rounds,threshold]，如 圣甲地裂崩 [2,200]）
//     741 {0}回合内免疫大于{1}的攻击伤害         （切片 [rounds,threshold]，如 荧光守护 [2,280]）
//   动工三查（工单协议②）：内核 switch 表 / effect_meta 均无 44/50/127/576/741 硬编码；
//   moves_lib 无同名注册——真缺口。
//
// 实现——两条家族通道：
//   · 44/50/127 → batch_b 599 同款：register_round_effect(FIRST_MOVE_RIGHT) 逐回合把
//     50 写进 ws.damage_reduce_mul 首空槽（REDUCE_PCT 乘算连乘，×0.5=减半）；
//     44/50 按**对方本回合选技**的 SkillType 过滤（选择期在 MOVE_RIGHT 前已定 → 精确
//     生效），首出手方立即写一次（本回合对方攻击的减伤要在其伤害管线前就位；后出手方
//     本回合对方已打完，写了无消费点、ws 随下回合 reset 无害——599 原注释）；
//     127 的 {0}% 概率门在技能使用时 roll_percent 一次（受管 RNG），命中开"全体减半"
//     窗（category=2 不过滤）。
//   · 576/741 → batch_a 1011 同款：伤害管线 BLOCK 阶段插件条目（挡伤，可被"挡伤失效"
//     抑制；ON_STAGE scope 携带者下场随 epoch 作废），窗口 round_effect_start_round
//     半开区间 [start, start+rounds)；只判**红伤且非属性直伤**（"攻击伤害"口径同
//     1011"攻击技能造成的伤害"）；阈值按 BLOCK 时点**即将落地的 d.final**（减伤后）
//     现场比对——576: final < X 免疫、741: final > X 免疫（裸伤口径待实测推翻再改）；
//     命中整段快照归零（1011 形状，防下游读幻影伤害）。

#include "common.h"

#include <vector>

namespace {

constexpr int kReduceHalfPct = 50;   // 减半 = 乘算减伤槽写 50（applyDamageReduction ×(1-50/100)）

// 对方本回合选中的技能下标（-1 = 没选技能）——batch_b_selected_skill_index 同件。
// roundChoice 在选择期写入、跨 ROUND_START 保留。
int e4_selected_skill_index(const BattleContext* ctx, int side) {
    if (!ctx || side < 0 || side > 1) {
        return -1;
    }
    if (ctx->roundChoice[side][0] != 1 /*SELECT_SKILL*/) {
        return -1;
    }
    const int idx = ctx->roundChoice[side][1];
    const int count = static_cast<int>(
        ctx->seerRobot[side].elfPets[ctx->on_stage[side]].skills.size());
    return (idx >= 0 && idx < count) ? idx : -1;
}

// 599 同款 tick：本回合把减半写进 damage_reduce_mul 首空槽。
// category：0=只对物理攻击 / 1=只对特殊攻击 / 2=全部（127 用）。
EffectResult e4_category_reduce_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int category = args.int_args[3];
    const int pct = args.int_args[4];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || pct <= 0) {
        return EffectResult::kOk;
    }
    if (category == 0 || category == 1) {
        const int idx = e4_selected_skill_index(ctx, opp);
        if (idx < 0) {
            return EffectResult::kOk;   // 对方没选技能：本回合无可判类别
        }
        const SkillType type = ctx->getPet(opp).skills[static_cast<std::size_t>(idx)].type;
        const bool physical = (type == SkillType::Physical);
        if ((category == 0) != physical) {
            return EffectResult::kOk;   // 类别不匹配：本回合不减伤
        }
    }
    int(&slots)[2][4] = ctx->ws.damage_reduce_mul;
    for (int i = 0; i < 4; ++i) {
        if (slots[owner][i] == 0) {
            slots[owner][i] = pct;
            break;
        }
    }
    return EffectResult::kOk;
}

// 44/50 共用入口：N 回合受物理(category=0)/特殊(category=1)攻击伤害减半。
// Args: [owner, 占位, rounds]。
EffectResult e4_attack_reduce_half(BattleContext* ctx, const EffectArgs& args,
                                   int effect_id, int category) {
    EFFECT_ARG_GUARD(ctx != nullptr, effect_id, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_count >= 3, effect_id, args, "需要 [owner,占位,rounds]");
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, effect_id, args, "owner 越界");
    EFFECT_ARG_GUARD(rounds > 0, effect_id, args, "rounds 应为正");
    // 首出手方立即查一次（本回合对方攻击的减伤要在其伤害管线前就位）。
    if (!ctx->is_second_mover(owner)) {
        e4_category_reduce_tick(ctx, EffectArgs(std::vector<int>{
            owner, 1 - owner, rounds, category, kReduceHalfPct}));
    }
    register_round_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, owner, effect_id, rounds,
                          {owner, 1 - owner, rounds, category, kReduceHalfPct},
                          &e4_category_reduce_tick);
    return EffectResult::kOk;
}

EffectResult effect_50_physical_reduce_half(BattleContext* ctx, const EffectArgs& args) {
    return e4_attack_reduce_half(ctx, args, 50, /*category=*/0);
}

EffectResult effect_44_special_reduce_half(BattleContext* ctx, const EffectArgs& args) {
    return e4_attack_reduce_half(ctx, args, 44, /*category=*/1);
}

// ── 127 {0}%概率，{1}回合受伤减半 ─────────────────────────────────────────────
// 概率门在**使用时掷一次**（受管 roll_percent，场景 103 管辖）；命中开全体减半窗
// （category=2 不过滤对方技能类别）。Args: [owner, 占位, prob, rounds]。
EffectResult effect_127_prob_reduce_half(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 127, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_count >= 4, 127, args, "需要 [owner,占位,prob,rounds]");
    const int owner = args.int_args[0];
    const int prob = args.int_args[2];
    const int rounds = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 127, args, "owner 越界");
    EFFECT_ARG_GUARD(rounds > 0, 127, args, "rounds 应为正");
    if (!roll_percent(ctx->rng(), prob)) {
        return EffectResult::kOk;   // 概率未命中：不开窗
    }
    if (!ctx->is_second_mover(owner)) {
        e4_category_reduce_tick(ctx, EffectArgs(std::vector<int>{
            owner, 1 - owner, rounds, /*category=*/2, kReduceHalfPct}));
    }
    register_round_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, owner, 127, rounds,
                          {owner, 1 - owner, rounds, /*category=*/2, kReduceHalfPct},
                          &e4_category_reduce_tick);
    return EffectResult::kOk;
}

// ── 576/741 阈值免疫窗（1011 同款 BLOCK 条目） ────────────────────────────────
// Args: [owner, 占位, rounds, threshold]。below=true → 576（免疫低于阈值的攻击伤害）；
// below=false → 741（免疫大于阈值的）。
EffectResult e4_threshold_immune_window(BattleContext* ctx, const EffectArgs& args,
                                        int effect_id, bool below) {
    EFFECT_ARG_GUARD(ctx != nullptr, effect_id, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_count >= 4, effect_id, args, "需要 [owner,占位,rounds,threshold]");
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    const int threshold = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, effect_id, args, "owner 越界");
    EFFECT_ARG_GUARD(rounds > 0, effect_id, args, "rounds 应为正");
    const int start = ctx->round_effect_start_round(self, rounds, EffectWindowKind::InRounds);
    const int expire = start + rounds;

    ctx->register_damage_effect(
        DamagePhase::BLOCK, self, DamageEffectCategory::BLOCK,
        [self, threshold, below, start, expire](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != self) {
                return;
            }
            DamageSnapshot& d = c->resolvedDamage;
            if (d.defenderId != self) {
                return;   // 只管打我的
            }
            if (!d.isRed || d.isDirect || d.final <= 0) {
                return;   // "攻击伤害"：红伤且非属性直伤（同 1011 口径）
            }
            if (c->roundCount < start || c->roundCount >= expire) {
                return;   // 窗口外
            }
            const bool qualify = below ? (d.final < threshold) : (d.final > threshold);
            if (!qualify) {
                return;   // 阈值外：照常受伤
            }
            // 免疫：整段快照归零（1011 形状，防下游读幻影伤害）。
            d.base = 0;
            d.afterAdd = 0;
            d.afterMul = 0;
            d.final = 0;
            d.addPct = 0;
            d.mulCoef = 0.0;
        });
    return EffectResult::kOk;
}

EffectResult effect_576_immune_below(BattleContext* ctx, const EffectArgs& args) {
    return e4_threshold_immune_window(ctx, args, 576, /*below=*/true);
}

EffectResult effect_741_immune_above(BattleContext* ctx, const EffectArgs& args) {
    return e4_threshold_immune_window(ctx, args, 741, /*below=*/false);
}

}  // namespace

SKILL_EFFECT(44, effect_44_special_reduce_half);
SKILL_EFFECT(50, effect_50_physical_reduce_half);
SKILL_EFFECT(127, effect_127_prob_reduce_half);
SKILL_EFFECT(576, effect_576_immune_below);
SKILL_EFFECT(741, effect_741_immune_above);
