// J 组 J309 · 伤害不足→下N次增伤族 —— 918×7 + 1016×7（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 官方模板即权威；两条同义，措辞不同）：
//   918 （args_num=3）"造成的伤害不足{0}则自身下{1}次造成的伤害提升{2}%"
//   1016（args_num=3）"造成的伤害不足{0}则下{1}次攻击造成的伤害提高{2}%"
//
// 直系先例（照抄口径，不另造机制）：
//   - **触发面** = J255 632/633/639「伤害比较器反馈族」同款：SKILL_EFFECT 时点武装一条
//     伤害管线 **DETECT 相位**回调，回调里读 `ctx->resolvedDamage`
//     （attackerId/defenderId/final/isRed）——J15 729 伤害阈值门同款。
//     ⚠️ DETECT 在管线里排在 BLOCK 之后、FINAL_CORRECT 之前（damage_pipeline.h）：
//     看到的是**已算完的 final**，但**尚未扣血**。
//   - **授予面** = 1037「消强成功→下{0}次攻击伤害额外提升{1}%」同款：
//     `ctx->grant_attack_boost(owner, 效果id, count, pct)`（次数型攻击增伤票）。
//     消费点在 `skills.cpp` 的 `consume_attack_boost_grants_after_attack`
//     （**成功使用攻击技后**、伤害结算之前），应用点在 `battleFsm` 伤害快照
//     （`snapshot.base = checked_pct_mul(base, boost_sum)`）。
//
// 口径决策（写死并记档）：
//   - "不足{0}" = **严格小于**阈值（final < threshold；等于阈值不触发）。
//   - 只认**真正落地的红伤**（isRed && final > 0）：miss / 被完全挡下（final=0）
//     一律**不触发**——"造成的伤害"以实际打出伤害为前提（J255 632/633/639 同判据）。
//   - 本次施放**自己那发**不吃增伤：授予发生在 DETECT（本次伤害已算完），
//     之后的下一次攻击才吃到（"下{1}次"语义）。
//   - 授 count = {1}+1：消费点在"成功使用攻击技后"且**本次施放的消费早于授予**
//     → 授 N 只命中 N-1 次，+1 补偿（1037/1850 同款）。
//   - 多段攻击（1141 族）一技多打：首段命中即消费授权（守卫槽置 -1），
//     后续段不再触发（口径记档，官方"造成的伤害"按段还是按合计未查）。
//   - 行参在位即消费（红线7）：阈值=0 → 恒不触发（显式关闸档，不是非法参数）；
//     次数=0 → 授 count=1、消费后即失效 = 不增伤；提升%=0 → 增伤票不成立，
//     引擎 grant_attack_boost 自带 pct<=0 拒收（语义等价"提升 0%"）。
//
// 载体切片（side_effect.arg_count 优先 / args_num 兜底，2026-10-22 脚本核算）：
//   918  [阈值,次数,提升%]：19605 梦之无解念 [918]|[300,1,100]（**纯链**，场景载体）、
//        30161 天火净世决 [918]|[300,1,100]（**纯链**，特攻 150）、
//        19815 暗黑灭世断 [918,1006]|[300,1,100]、19514 炼·无情影灭 [180,918]|[300,1,100]、
//        32362 极影怨念 [1008,918]|[300,1,100]、35110 午夜钟声荡 [867,968,918]|[300,1,100]、
//        34746 鳗鳗冲锋 [1141,918]|[3,6,20,1,5,30,1 | 300,1,50]（1141 吃 7 参）
//   1016 [阈值,次数,提升%]：19855 震地熔世断 [1015,1016]|[300,1,100]、
//        30185 王·联盟审判 [1064,1016]|[300,1,100]、31718 烟花易冷 [1043,1016]|[300,1,100]、
//        31459 黯然噬魂灭 [797,1016,1049]|[300,1,100]、31935 火蚁连天烧 [1138,1016]|[300,1,100]、
//        38332 初诞·始源胎动 [990,1016]|[300,1,50]、38459 沧海铭刻 [1219,2303,1016]
//        |[3 | 8,3,1 | 300,2,50]（2303 无 side_effect 行 → args_num=4 兜底切）
//   → 阈值恒 300；次数 ∈ {1,2}；提升% ∈ {50,100}。
//
// 动工前置三查（2026-10-22）：①内核 grep 918/1016 无行为硬编码 ②effect_info 两条模板
//   各仅本条（无同文其它 id）③全库 moves_lib/soul_lib 并集（jbatch/kbatch/hub/agents）
//   无 SKILL_EFFECT(918)/SKILL_EFFECT(1016) 注册——真缺口。
//   （注：918/1016 出现在 resources/boss_invalid_effects.json = "BOSS 战无效" 数据注记，
//   非实现项；同 J12/J307 口径。）
//
// 审计槽（9xxxx 私有段，场景精确断门；+owner*1）：
//   991918100 + owner = 918 触发那一发的 final 伤害（-999 = 未触发）
//   991918101 + owner = 918 授予的 count（times+1）
//   991918200 + owner = 918 DETECT 守卫 token（-1 = 已消费）
//   991016100 + owner = 1016 触发那一发的 final 伤害（-999 = 未触发）
//   991016101 + owner = 1016 授予的 count（times+1）
//   991016200 + owner = 1016 DETECT 守卫 token（-1 = 已消费）
//
// 场景回归：test/scenario/scenario_6420_low_damage_next_boost.cpp（号段 6420-6429）。

#include "common.h"

#include <functional>

namespace {

constexpr int k918AuditBase = 991918100;   // +owner：触发那一发的 final 伤害
constexpr int k918GrantBase = 991918101;   // +owner：授予的 count
constexpr int k918GuardBase = 991918200;   // +owner：DETECT 守卫 token
constexpr int k1016AuditBase = 991016100;  // +owner：触发那一发的 final 伤害
constexpr int k1016GrantBase = 991016101;  // +owner：授予的 count
constexpr int k1016GuardBase = 991016200;  // +owner：DETECT 守卫 token

int j309_storage_int(const BattleContext* ctx, int key, int fallback = -999) {
    if (!ctx) {
        return fallback;
    }
    const auto it = ctx->plugin_storage.find(key);
    if (it == ctx->plugin_storage.end()) {
        return fallback;
    }
    return any_int_or(it->second, fallback);
}

// 武装一条 DETECT 回调：本次施放造成的红伤 final **严格小于** threshold → on_pass(c, final)。
//
// 守卫三道（J255 令牌守卫的加强版，防"上一次施放留下的回调"跨回合误触发）：
//   ① 守卫槽 == 本次武装的 token（同一效果再次武装会覆盖槽位）；
//   ② `skill_use_seq[owner]` 仍等于 token（期间用过别的技能 → 陈旧回调，弃）；
//   ③ attackerId/defenderId 匹配本次施放双方。
void j309_arm_below_detect(BattleContext* ctx, int owner, int target, int guard_base, int threshold,
                            std::function<void(BattleContext*, int)> on_pass) {
    if (!ctx || !on_pass) {
        return;
    }
    const int token = ctx->skill_use_seq[owner];
    ctx->plugin_storage[guard_base + owner] = token;
    ctx->register_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner, target, guard_base, threshold, token, on_pass](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner) {
                return;
            }
            if (j309_storage_int(c, guard_base + owner) != token) {
                return;   // 不是本次武装的回调
            }
            if (c->skill_use_seq[owner] != token) {
                return;   // 期间又用过别的技能 → 陈旧回调
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || d.defenderId != target) {
                return;
            }
            if (!d.isRed || d.final <= 0) {
                return;   // miss / 被完全挡下：没有"造成伤害"
            }
            if (d.final >= threshold) {
                return;   // "不足{0}" = 严格小于阈值
            }
            c->plugin_storage[guard_base + owner] = -1;   // 消费本次武装（多段只触发首段）
            on_pass(c, d.final);
        });
}

// ── 918 —— 造成的伤害不足{0}则自身下{1}次造成的伤害提升{2}% ──
// int_args = [owner, target, 阈值, 次数, 提升%]（args_num=3）。
EffectResult effect_918_low_damage_next_boost(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 918, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 918, args,
                     "918 布局=[owner,target,阈值,次数,提升%]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int threshold = args.int_args[2];
    const int times = args.int_args[3];
    const int pct = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     918, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(threshold >= 0 && times >= 0, 918, args, "需要 阈值>=0 且 次数>=0");
    j309_arm_below_detect(ctx, owner, target, k918GuardBase, threshold,
                          [owner, times, pct](BattleContext* c, int final_damage) {
        c->plugin_storage[k918AuditBase + owner] = final_damage;
        // 授 times+1：本次施放的消费点在授予之前（DETECT 晚于消费点）→ +1 补偿。
        c->grant_attack_boost(owner, 918, /*count=*/times + 1, /*pct=*/pct);
        c->plugin_storage[k918GrantBase + owner] = times + 1;
    });
    return EffectResult::kOk;
}

// ── 1016 —— 造成的伤害不足{0}则下{1}次攻击造成的伤害提高{2}% ──
// int_args = [owner, target, 阈值, 次数, 提升%]（args_num=3）。语义同 918（措辞不同）。
EffectResult effect_1016_low_damage_next_boost(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1016, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1016, args,
                     "1016 布局=[owner,target,阈值,次数,提升%]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int threshold = args.int_args[2];
    const int times = args.int_args[3];
    const int pct = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1016, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(threshold >= 0 && times >= 0, 1016, args, "需要 阈值>=0 且 次数>=0");
    j309_arm_below_detect(ctx, owner, target, k1016GuardBase, threshold,
                          [owner, times, pct](BattleContext* c, int final_damage) {
        c->plugin_storage[k1016AuditBase + owner] = final_damage;
        c->grant_attack_boost(owner, 1016, /*count=*/times + 1, /*pct=*/pct);
        c->plugin_storage[k1016GrantBase + owner] = times + 1;
    });
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(918, effect_918_low_damage_next_boost)
SKILL_EFFECT(1016, effect_1016_low_damage_next_boost)
