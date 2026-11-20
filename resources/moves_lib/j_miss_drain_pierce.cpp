// J 组 J42 · miss吸取/无视限制/低伤固伤族 —— 3 宏/57 载体（2026-10-02，worktree ../agents/j42）
//
// 家族语义（effect_info 模板即权威）：
//   1320 [2] {0}回合内若对手攻击MISS则回合结束时吸取对手最大体力的1/{1}
//   790  [1] {0}回合内自身所有攻击无视伤害限制效果
//   989  [2] 造成的伤害低于{0}则附加{1}点固定伤害，自身体力低于对手时固定伤害翻倍
//
// 实现口径：
//   - 1320 = EVENT_SKILL_INVALID watcher（emit {actor=使用方, target=对面}，miss/被封属
//     都算——"攻击MISS"字面取命中失败路径；攻击技过滤=读 roundChoice 槽位技能类型）置
//     当回合旗（plugin_storage 键 kFlag+roundCount）+ ROUND_END InRounds {0} tick：
//     有旗 → drain_hp(1/{1}) 清旗。窗口 InRounds 含当回合。
//   - 790 = grant_penetration(owner, level=1, ignore_attack_immunity=false,
//     ignore_damage_limit=true, counts=0, rounds={0}, PENETRATE_ATTACK, ON_STAGE)
//     （沧岚/无极窗口票同通道；RuleCenter 窗口含当回合）。
//     ⚠️ 本次攻击的 credential 已在 query_usage 物化（早于本 SKILL_EFFECT）→
//     同时直写 ws.attack_credential[self].ignore_damage_limit 兜当前刀
//     （ws 每回合 reset、下刀 materialize 重读 RuleCenter，双保险即时+窗口）。
//   ⛔ 790 记档跳过（2026-10-02 夜跑）：SKILL_EFFECT 未产码——效果函数未被调用
//     （纯载体 26434 [790]|[3] 亦然；无 custom_effect_programs/overrides、无内核引用、
//     无 disabledEffects）。疑 effect 链解析（parsed_units_ 条件路由）对该 id 的处理
//     有缺口，需白天专项排查后补（R 单候选）。1320/989 正常交付。
//   - 989 = DETECT 攻方桶（729/J40-1522 形）：本次红伤 final **严格 <** {0} →
//     fixed_damage(target, {1} × (我 hp < 对手 hp ? 2 : 1))——hp 对比在 DETECT 时点
//     （红伤未扣血）求值。
//
// 载体切片（unity 库 json_each 精确匹配 19×3=57；side_effect.arg_count 切分全载体
// 自洽、零剩余，脚本核算 2026-10-02）：
//   1320 [rounds,分母]：29151 月使慈唱 [4,3]（属）、38223 隐迹迷踪 [2,3]
//   790  [rounds]：24164 真龙霸体 [191,576,790]|[…,3]（属，flag 测试载体）
//   989  [阈值,固伤]：19755 究影风啸天 [300,200]（物150）
//
// 动工前置三查（2026-10-02）：①内核 grep 3 id 无硬编码 ②custom_effect_programs/overrides
// 无声明 ③moves_lib 并集（全 worktree）无既有注册——真缺口（grant_penetration/
// EVENT_SKILL_INVALID 均既有设施：沧岚、E13、1036 先例）。
// 场景回归：test/scenario/scenario_1270_miss_drain_pierce.cpp（号段 1270-1279）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int k1320FlagBase = 990132000;   // +roundCount：当回合 MISS 旗

// ── 1320 的 ROUND_END tick —— payload={owner, target, denom}；有旗则吸取并清旗。
EffectResult j42_1320_round_end_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->drain_hp) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int denom = args.int_args[2];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || denom <= 0) {
        return EffectResult::kOk;
    }
    const int key = k1320FlagBase + ctx->roundCount;
    const auto it = ctx->plugin_storage.find(key);
    if (it == ctx->plugin_storage.end() || any_int_or(it->second) != 1) {
        return EffectResult::kOk;   // 本回合对手攻击没有 MISS
    }
    ctx->plugin_storage.erase(key);
    (void)g_core->drain_hp(ctx, /*actor=*/owner, /*target=*/target, denom);
    return EffectResult::kOk;
}

// ── 1320 —— {0}回合内若对手攻击MISS则回合结束时吸取对手最大体力的1/{1} ──
// int_args = [owner, target, rounds, denom]（args_num=2）。
EffectResult effect_1320_miss_drain_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1320, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1320, args,
                     "1320 布局=[owner,target,回合,分母]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int denom = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1320, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && denom > 0, 1320, args, "需要 回合/分母 > 0");
    // miss 置旗：对手侧使用技能失败（emit {actor=使用方, target=对面}）；攻击技过滤
    // = 对手当回合所选槽位的技能非属性技。
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_INVALID, owner, ctx->roundCount, rounds, /*once=*/false,
        [owner, opp](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.actor != opp) {
                return;
            }
            const int idx = c->roundChoice[opp][1];
            const auto& skills = c->getPet(opp).skills;
            if (idx < 0 || idx >= static_cast<int>(skills.size())
                || skills[static_cast<std::size_t>(idx)].type == SkillType::Attribute) {
                return;   // "攻击MISS"：属性技的无效不算
            }
            c->plugin_storage[k1320FlagBase + c->roundCount] = 1;
        },
        WatcherScope::TEAM);
    register_round_effect(ctx, State::BATTLE_ROUND_END, owner, 1320, rounds,
                          {owner, opp, denom}, &j42_1320_round_end_tick);
    return EffectResult::kOk;
}

// ── 989 —— 造成的伤害低于{0}则附加{1}点固定伤害，自身体力低于对手时固定伤害翻倍 ──
// int_args = [owner, target, threshold, fixed]（args_num=2）。
EffectResult effect_989_low_damage_fixed(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 989, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 989, args,
                     "989 布局=[owner,target,阈值,固伤]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int threshold = args.int_args[2];
    const int fixed = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     989, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(threshold > 0 && fixed > 0, 989, args, "需要 阈值/固伤 > 0");
    ctx->register_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner, target, threshold, fixed](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || d.defenderId != target || !d.isRed || d.final <= 0) {
                return;   // 只随本次对目标的红伤附加
            }
            if (d.final >= threshold) {
                return;   // "造成的伤害低于{0}"（严格小于）
            }
            const int amount = (c->getPet(owner).hp < c->getPet(target).hp) ? fixed * 2 : fixed;
            if (g_core && g_core->fixed_damage) {
                (void)g_core->fixed_damage(c, target, amount);
            }
        });
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1320, effect_1320_miss_drain_window)
SKILL_EFFECT(989, effect_989_low_damage_fixed)
