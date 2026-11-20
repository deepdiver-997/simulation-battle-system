// J 组 J256 · 条件自强化族 —— 2 宏/26 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威）：
//   763（args_num=6）"自身不处于能力提升状态时则{0}"
//   1425（args_num=7）"造成的伤害低于{0}则自身{1}"
//
// 参数解码（2026-10-22 全载体切片核对）：两条的"后缀"都是**六槽能力增量**，顺序为
// **官方码序 攻/防/特攻/特防/速/命中**，需经 {0,2,1,3,4,5} 映射到引擎槽
// （0攻/1特攻/2防/3特防/4速/5命中；防与特攻互换，见 477/501 先例与常识速查
//  "属性排列双序"坑——直接按引擎序读会把防/特攻读反）：
//   763  args[0..5]  → 官方序六槽增量
//   1425 args[0]=伤害阈值，args[1..6] → 官方序六槽增量
//   例：28536 午夜帷幕 763 [1,1,2,1,2,0] = 攻+1/防+1/特攻+2/特防+1/速+2
//       → 引擎槽 [1,2,1,1,2,0]；
//       32147 剑·万千凛锋 [1,1,1,1,1,1] = 全属性+1；
//       33757 裂魂践踏 1425 [200,1,0,0,0,1,0] = 伤害<200 → 攻+1/速+1。
//   佐证：特攻向载体 18184 [0,0,2,0,0,0] / 32571 [0,0,2,0,2,0] 的合理读法是
//   "特攻+2（可带速+2）"而非"防御+2"；与 477（args 语义序）映射一致。
//
// 实现口径：
//   - 763 = SKILL_EFFECT 判"六槽无正等级"（count_boost_kinds 同判据；负等级不算提升）
//     → 逐槽 stat_change(self, slot, delta)（自身增益走 stat_change，不吃免弱）。
//     审计槽 990763100+owner = 应用槽位位掩码 / -1 门闭。
//   - 1425 = DETECT 相位回调（J15 729 直系）读 resolvedDamage：attackerId==owner、
//     isRed、final>0 且 **final < {0}**（严格小于）→ 逐槽 stat_change(self)。
//     注册时以 `skill_use_seq[owner]` 作 token 写守卫槽，回调消费后置 -1
//     （旧回调不跨回合误触发、同一发伤害只结算一次）。
//     审计槽 991425100+owner = 触发 final / -1 门闭；守卫 991425200+owner。
//
// 载体切片（side_effect 链按 effect_info.args_num 顺序切分，2026-10-22 脚本核算）：
//   763 [d0..d5]：28536 午夜帷幕（纯链，属性）[1,1,2,1,2,0]、30753 远古余响（纯链）
//     [1,0,0,0,1,0]、18184 汹涌兽心怒 [0,0,2,0,0,0]、30970 拥抱黑暗 [0,1,0,1,0,0]、
//     32147 剑·万千凛锋 [1,1,1,1,1,1]、35393 诛魔镇魂刃 [1,1,1,1,1,1]、
//     36147 辉·沧天逆流拳 [1,1,1,1,1,1]、38435 寒霜烈风 [1,1,1,1,1,1]。
//   1425 [阈值,d0..d5]：33757 裂魂践踏 [200,1,0,0,0,1,0]、34397 净尘法球
//     [250,0,1,0,1,0,0]、34945 劳逸结合 [300,0,0,1,0,1,0]、32829 巨龙钢爪碎
//     [300,2,0,0,0,2,0]、33151 呆萌白板 [300,2,0,0,0,2,0]、33742 暗霜裂天穹
//     [300,0,0,2,0,2,0] 等 13 链。
//
// 动工前置三查（2026-10-22）：①内核 grep 763/1425 无行为硬编码 ②effect_info 模板唯一、
//   custom_effect_overrides/programs 零行 ③全库 moves_lib/soul_lib 并集（含各 worktree）
//   无 SKILL_EFFECT(763/1425) 注册——真缺口。
//
// 场景回归：test/scenario/scenario_5870_cond_self_boost.cpp（号段 5870-5879）。

#include "common.h"

#include <array>

namespace {

constexpr int k763AuditBase = 990763100;
constexpr int k1425AuditBase = 991425100;
constexpr int k1425GuardBase = 991425200;

// 官方码序（0攻/1防/2特攻/3特防/4速/5命中）→ 引擎能力槽
// （0攻/1特攻/2防/3特防/4速/5命中）。防与特攻互换是本映射的全部意义。
constexpr int kOfficialStatToSlot[6] = {0, 2, 1, 3, 4, 5};

// 六槽是否全部**非正**（"不处于能力提升状态"；J256/J51 count_boost 同判据）。
bool j256_no_positive_level(const BattleContext* ctx, int owner) {
    if (!ctx || owner < 0 || owner > 1) {
        return false;
    }
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[owner][i] > 0) {
            return false;
        }
    }
    return true;
}

// 逐槽应用六槽增量（官方序输入 → 引擎槽，只处理非 0），返回**官方序参数位**掩码。
int j256_apply_six(BattleContext* ctx, int owner, const int* deltas) {
    if (!ctx || !g_core || !g_core->stat_change || owner < 0 || owner > 1 || !deltas) {
        return 0;
    }
    int mask = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (deltas[i] != 0) {
            (void)g_core->stat_change(ctx, owner, kOfficialStatToSlot[i], deltas[i]);
            mask |= (1 << i);
        }
    }
    return mask;
}

int j256_storage_int(BattleContext* ctx, int key, int fallback = -999) {
    if (!ctx) {
        return fallback;
    }
    const auto it = ctx->plugin_storage.find(key);
    return it == ctx->plugin_storage.end() ? fallback : any_int_or(it->second, fallback);
}

// ── 763 —— 自身不处于能力提升状态时则{0} ──
// int_args = [owner, target, d0..d5]（args_num=6）。
EffectResult effect_763_no_boost_self_stats(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 763, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 8, 763, args,
                     "763 布局=[owner,target,d0..d5]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     763, args, "需要 owner/target ∈ {0,1}");
    if (!j256_no_positive_level(ctx, owner)) {
        ctx->plugin_storage[k763AuditBase + owner] = -1;   // 门闭：已有正等级
        return EffectResult::kOk;
    }
    const int mask = j256_apply_six(ctx, owner, args.int_args + 2);
    ctx->plugin_storage[k763AuditBase + owner] = mask;
    return EffectResult::kOk;
}

// ── 1425 —— 造成的伤害低于{0}则自身{1} ──
// int_args = [owner, target, 阈值, d0..d5]（args_num=7）。
EffectResult effect_1425_low_damage_self_stats(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1425, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 9, 1425, args,
                     "1425 布局=[owner,target,阈值,d0..d5]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int threshold = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1425, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(threshold >= 0, 1425, args, "需要 阈值 >= 0");

    const int token = ctx->skill_use_seq[owner];
    ctx->plugin_storage[k1425GuardBase + owner] = token;
    ctx->plugin_storage[k1425AuditBase + owner] = -1;   // 默认门闭
    ctx->register_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner, target, threshold, token, deltas = std::array<int, 6>{
             args.int_args[3], args.int_args[4], args.int_args[5],
             args.int_args[6], args.int_args[7], args.int_args[8]}](BattleContext* c,
                                                                   int bucket_owner) {
            if (!c || bucket_owner != owner) {
                return;
            }
            if (j256_storage_int(c, k1425GuardBase + owner) != token) {
                return;   // 不是本次施放武装的回调
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || d.defenderId != target) {
                return;
            }
            if (!d.isRed || d.final <= 0 || d.final >= threshold) {
                return;   // 未命中/被完全挡下/未低于阈值
            }
            c->plugin_storage[k1425GuardBase + owner] = -1;   // 消费本次武装
            c->plugin_storage[k1425AuditBase + owner] = d.final;
            (void)j256_apply_six(c, owner, deltas.data());
        });
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(763, effect_763_no_boost_self_stats)
SKILL_EFFECT(1425, effect_1425_low_damage_self_stats)
