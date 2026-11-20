// J 组 J310 · 伤害区间/高于门异常族 —— 748×7 + 1539×7（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 官方模板即权威）：
//   748 （args_num=3）"若打出的伤害在{0}至{1}之间则对手{2}"
//   1539（args_num=3）"造成的伤害高于{0}则{1}%令对手{2}，未触发则消除对手回合类效果"
//
// 直系先例（照抄口径，不另造机制）：
//   - **触发面** = J255 632/633/639「伤害比较器反馈族」同款：SKILL_EFFECT 时点武装一条
//     伤害管线 **DETECT 相位**回调，回调里读 `ctx->resolvedDamage`
//     （attackerId/defenderId/final/isRed）——J15 729 伤害阈值门同款。
//   - **异常面** = 申报制 `apply_anomaly_checked`（亮节闸门管辖；J3/J215 同款）。
//   - **兜底面** = "未触发" = 异常**未落地**（免疫/抗性/反弹/概率未掷中都算，batch_a 同判据）
//     → `g_core->break_round_effects(target)`（990/1083/1200 同款"消除对手回合类效果"）。
//     ⚠️ 方向口径：**"对手" = 效果持有者的另一方** = 本效果的 target（防御方）——
//        990「消除对手回合类效果，消除成功则附加固伤」同款（opp = args[1]）；
//        1200（受击侧效果）的"对手"才是攻击者，方向相反但规则一致。
//
// 口径决策（写死并记档）：
//   - 748 区间 = **闭区间** [下限, 上限]（"在{0}至{1}之间"含端点；7 载体区间
//     150~300 / 200~300 / 250~350，实测落点离端点 >20，端点取舍不影响判定）。
//   - 1539 = **严格大于**阈值（"高于{0}"）。
//   - 只认**落地红伤**（isRed）：属性技无伤害相位自然不触发；被完全挡下 final=0
//     时 748 的 0∉[下限,上限]、1539 的 0≯阈值 → 两种门都不开。
//   - 748 模板无概率 → 按 **100%** 施加（走申报制通道，免疫/抗性/反弹照常判）。
//   - 1539 的 {1}% 走申报制：概率 0 = 显式关闸档（红线7，在位即消费）→ 必未落地 → 必走兜底。
//   - 多段攻击（1141 族）一技多打：首段命中即消费授权（守卫槽置 -1），后续段不再触发。
//
// 载体切片（side_effect.arg_count 优先 / args_num 兜底，2026-10-22 脚本核算）：
//   748  [下限,上限,异常码]：17196 恶·灭世修罗 [33,748]|[200,300,6]（**场景载体**，物 160；
//        33=消除对手能力提升状态，场上无提升 → 无副作用）、
//        34945 劳逸结合 [1425,748]|[200,300,6]、36104 黯世影染决 [979,748]|[200,300,6]、
//        36343 作弊球棍 [748]|[150,300,0]（**纯链**；异常码 0=麻痹，现代通道非确定，见下）、
//        36413 脊裂 [1220,748]|[200,300,6]（**priority=3 不选**：污染先手顺序）、
//        36914 天琴凝冰意 [1199,748,698]|[250,350,15]、
//        37670 篆刻 [5,748,593]|[200,300,16]（**priority=1 不选**）
//   1539 [阈值,概率,异常码]：34797 绒羽霜漫天 [1491,1061,1539]|[300,100,15]
//        （**场景载体**，物 160；1491=遇天敌先制+1、1061=反转对手能力提升 → 无副作用）、
//        33926 耀阳灼穿 [1260,1539]|[300,100,22]、33952 王·尘芥刚雷 [836,1539,1355]|[300,100,0]、
//        34534 幽夜潜行击 [1183,986,1539]|[300,100,6]、
//        34883 灭世煞念 [494,1481,1539]|[300,100,23]、
//        36943 天烬雷魂破 [1686,1539,1425]|[300,100,0]、
//        37518 迪拉修姆光流 [1395,101,1539,793]|[300,100,22]
//   → 748 区间恒在 150~350；1539 阈值恒 300、概率恒 100。
//   ⚠️ 异常码 0（麻痹）在 apply_anomaly_checked **现代通道非确定**（100% 档实测落害怕 6，
//      机制未查——常识速查 §四记档）：36343 / 33952 / 36943 三个载体记档，场景不选。
//
// 动工前置三查（2026-10-22）：①内核 grep 748/1539 无行为硬编码 ②effect_info 两条模板
//   各仅本条（无同文其它 id）③全库 moves_lib/soul_lib 并集（jbatch/kbatch/hub/agents）
//   无 SKILL_EFFECT(748)/SKILL_EFFECT(1539) 注册——真缺口。
//
// 审计槽（9xxxx 私有段，场景精确断门；+owner*1）：
//   991748100 + owner = 748 触发那一发的 final 伤害（-999 = 未触发）
//   991748200 + owner = 748 DETECT 守卫 token（-1 = 已消费）
//   991539100 + owner = 1539 触发那一发的 final 伤害（-999 = 未触发）
//   991539101 + owner = 1539 兜底（消除对手回合类效果）触发次数（-999 = 未走兜底）
//   991539200 + owner = 1539 DETECT 守卫 token（-1 = 已消费）
//
// 场景回归：test/scenario/scenario_6430_damage_range_anomaly.cpp（号段 6430-6439）。

#include "common.h"

#include <functional>

namespace {

constexpr int k748AuditBase = 991748100;    // +owner：触发那一发的 final 伤害
constexpr int k748GuardBase = 991748200;    // +owner：DETECT 守卫 token
constexpr int k1539AuditBase = 991539100;   // +owner：触发那一发的 final 伤害
constexpr int k1539FallbackBase = 991539101;  // +owner：兜底触发次数
constexpr int k1539GuardBase = 991539200;   // +owner：DETECT 守卫 token

int j310_storage_int(const BattleContext* ctx, int key, int fallback = -999) {
    if (!ctx) {
        return fallback;
    }
    const auto it = ctx->plugin_storage.find(key);
    if (it == ctx->plugin_storage.end()) {
        return fallback;
    }
    return any_int_or(it->second, fallback);
}

// 现代施加"落地"判定（J3 申报制口径：免疫/抗性/反弹都算未落地）。
bool j310_anomaly_landed(ApplyAnomalyResult r) {
    return r == ApplyAnomalyResult::SUCCESS
        || r == ApplyAnomalyResult::REPLACED_EXISTING
        || r == ApplyAnomalyResult::DURATION_EXTENDED;
}

// 武装一条 DETECT 回调：本次施放造成的红伤 final 满足 hit(final) → on_pass(c, final)。
//
// 守卫三道（防"上一次施放留下的回调"跨回合误触发，J255 令牌守卫的加强版）：
//   ① 守卫槽 == 本次武装的 token；② skill_use_seq[owner] 仍等于 token；
//   ③ attackerId/defenderId 匹配本次施放双方。
void j310_arm_detect(BattleContext* ctx, int owner, int target, int guard_base,
                      std::function<bool(int)> hit,
                      std::function<void(BattleContext*, int)> on_pass) {
    if (!ctx || !hit || !on_pass) {
        return;
    }
    const int token = ctx->skill_use_seq[owner];
    ctx->plugin_storage[guard_base + owner] = token;
    ctx->register_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner, target, guard_base, hit, token, on_pass](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner) {
                return;
            }
            if (j310_storage_int(c, guard_base + owner) != token) {
                return;   // 不是本次武装的回调
            }
            if (c->skill_use_seq[owner] != token) {
                return;   // 期间又用过别的技能 → 陈旧回调
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || d.defenderId != target) {
                return;
            }
            if (!d.isRed) {
                return;   // 非红伤（属性技无伤害相位）
            }
            if (!hit(d.final)) {
                return;
            }
            c->plugin_storage[guard_base + owner] = -1;   // 消费本次武装（多段只触发首段）
            on_pass(c, d.final);
        });
}

// ── 748 —— 若打出的伤害在{0}至{1}之间则对手{2} ──
// int_args = [owner, target, 下限, 上限, 异常码]（args_num=3）。
EffectResult effect_748_damage_range_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 748, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 748, args,
                     "748 布局=[owner,target,下限,上限,异常码]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int lo = args.int_args[2];
    const int hi = args.int_args[3];
    const int anomaly = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     748, args, "需要 owner/target ∈ {0,1}");
    // 在位即消费：下限>上限 = 空区间 = 恒不触发（显式关闸档，不是非法参数）。
    EFFECT_ARG_GUARD(lo >= 0 && hi >= 0, 748, args, "需要 下限>=0 且 上限>=0");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 748, args, "需要 异常码 ∈ [0,43]");
    j310_arm_detect(ctx, owner, target, k748GuardBase,
                    [lo, hi](int final_damage) { return final_damage >= lo && final_damage <= hi; },
                    [owner, target, anomaly](BattleContext* c, int final_damage) {
        c->plugin_storage[k748AuditBase + owner] = final_damage;
        if (!g_core || !g_core->apply_anomaly_checked) {
            return;
        }
        // 模板无概率 → 100% 施加（仍走申报制通道：免疫/抗性/反弹照常判）。
        (void)g_core->apply_anomaly_checked(c, target, anomaly, random_anomaly_duration(), owner,
                                            /*chance=*/100, ChanceSource::Skill);
    });
    return EffectResult::kOk;
}

// ── 1539 —— 造成的伤害高于{0}则{1}%令对手{2}，未触发则消除对手回合类效果 ──
// int_args = [owner, target, 阈值, 概率, 异常码]（args_num=3）。
EffectResult effect_1539_damage_over_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1539, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1539, args,
                     "1539 布局=[owner,target,阈值,概率,异常码]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int threshold = args.int_args[2];
    const int chance = args.int_args[3];
    const int anomaly = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1539, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(threshold >= 0, 1539, args, "需要 阈值>=0");
    EFFECT_ARG_GUARD(chance >= 0 && chance <= 100, 1539, args, "需要 概率 ∈ [0,100]");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 1539, args, "需要 异常码 ∈ [0,43]");
    j310_arm_detect(ctx, owner, target, k1539GuardBase,
                    [threshold](int final_damage) { return final_damage > threshold; },
                    [owner, target, chance, anomaly](BattleContext* c, int final_damage) {
        c->plugin_storage[k1539AuditBase + owner] = final_damage;
        if (!g_core || !g_core->apply_anomaly_checked) {
            return;
        }
        const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
            c, target, anomaly, random_anomaly_duration(), owner, chance, ChanceSource::Skill);
        if (j310_anomaly_landed(r)) {
            return;   // 异常落地 → 不走兜底
        }
        // 未触发（概率未掷中 / 免疫 / 抗性 / 反弹）→ 消除对手回合类效果。
        c->plugin_storage[k1539FallbackBase + owner] =
            j310_storage_int(c, k1539FallbackBase + owner, 0) + 1;
        if (g_core->break_round_effects) {
            (void)g_core->break_round_effects(c, target);
        }
    });
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(748, effect_748_damage_range_anomaly)
SKILL_EFFECT(1539, effect_1539_damage_over_anomaly)
