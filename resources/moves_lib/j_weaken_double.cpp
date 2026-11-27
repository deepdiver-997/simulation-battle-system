// J 板 J287 · 弱化翻倍族 —— 590×8 / 625×8（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info）：
//   590 "{0}%弱化效果翻倍"：先按六槽增量表对对手逐槽施加弱化，本次**整段**弱化
//       按 {0}% 概率翻倍（一次掷骰，作用到本次所有负槽；退化为 1 倍时照常施加）。
//   625 "若对手处于弱化状态，弱化效果翻倍"：同样先按六槽增量表施加弱化；
//       **施放前**对手六槽（0攻..5命中）任一 < 0（已处于弱化状态）→ 本次整段翻倍。
//
// 载体切片（side_effect.arg_count 顺序切分，16/16 自洽；模板参数个数交叉验证）：
//   590（7 参 = 六槽增量 + 概率）：
//     23377 殊途同归 [-1,-1,-1,-1,-1,-1,15]   23681 光影重重 [-1,0,-1,-1,-1,-1,50]
//     23713 毁灭爆弹 [0,-1,0,0,-1,-1,50]      23726 波卷龙怒 [0,0,0,0,-1,-2,50]
//     23900 魔灵之力 [-1,0,-1,0,-1,-1,50]     24068 圣光降临 [-1,-1,-1,0,0,-1,50]
//     24507 自然神威 [-1,-1,-1,-1,-1,-1,50]   30132 不破不立 [-1,-1,-1,-1,-1,-1,50]
//   625（6 参 = 六槽增量）：
//     19312 狂魔无尽破 [0,-1,0,0,0,-1]        23484 魔灵之咒 [-1,-1,-1,-1,-1,-1]
//     23881 残酷责难 [0,0,0,-1,-1,-1]         24346 生命沦陷 [-1,0,-1,-1,0,-1]
//     25627 木偶之躯 [-1,-1,-1,-1,-1,-1]      34919 黄泉封生锁 [0,-1,0,-1,0,-1]
//     37153 审判枷锁 [-1,-1,0,0,-1,-1]        37800 试水突进 [-1,-1,0,0,0,0]
//   —— 六槽是**官方语义序**（攻,防,特攻,特防,速,命中）→ 引擎等级槽
//   （0攻 1特攻 2防 3特防 4速 5命中）映射 {0,2,1,3,4,5}（477/504 同表；4399 交叉验证：
//   光影重重"双攻、特防、速度、命中"=[-1,0,-1,-1,-1,-1]，残酷责难"特防、命中、速度"
//   =[0,0,0,-1,-1,-1]——直读引擎序会把防/特攻互换）。
//
// 实现口径：
//   - 负值走 stat_drop（对手施加的弱化：查免弱 STAT_DROP + ATTACH_BAN，钳 -6）；
//     0 = 该槽不在本次表内（跳过）。翻倍 = 每槽下降量 ×2 后交原语（封顶由原语钳制）。
//   - 590 概率门：**一次掷骰**决定整段是否翻倍；概率参数**在位即消费**（显式 0 =
//     永不翻倍、100 = 必定翻倍；roll_percent 对 0/>=100 短路且不消耗 rand）。
//   - 625 条件门：施放前读**本体** `ctx->ability_levels[target][0..5]` 任一 < 0
//     （与 f_bonus_damage 的"处于能力下降状态"同判据），不看回合视图。
//   - 审计槽 995902100+owner（590）/ 995902200+owner（625）：本次实际倍率 1/2，
//     场景对概率分支做无随机精确断言。
//
// 动工前置三查（2026-10-22）：①内核 grep 590/625 无行为硬编码 ②effect_info 模板
//   args_num 7/6 唯一、custom_effect_programs/overrides 零行 ③全 worktree moves_lib/
//   soul_lib 无 SKILL_EFFECT(590/625) 注册（含并行分支）——真缺口；运行时 parser 不
//   匹配本模板（"弱化效果翻倍"无占位符）。
// 场景回归：test/scenario/scenario_6200_weaken_double.cpp（号段 6200-6209）。

#include "common.h"

namespace {

// 官方语义序（攻,防,特攻,特防,速,命中）→ 引擎等级槽（0攻 1特攻 2防 3特防 4速 5命中）。
constexpr int kSemanticToSlot[6] = {0, 2, 1, 3, 4, 5};

constexpr int k590ScaleAuditBase = 995902100;   // +owner：本次整段倍率 1/2
constexpr int k625ScaleAuditBase = 995902200;   // +owner：本次整段倍率 1/2

// 六槽增量表施加（官方语义序；负值走弱化原语，0/正值跳过——本族模板只描述弱化）。
inline void apply_six_slot_weaken(BattleContext* ctx, int owner, int target,
                                  const int* deltas, int scale) {
    if (!g_core || !g_core->stat_drop) {
        return;
    }
    for (int i = 0; i < 6; ++i) {
        const int delta = deltas[i];
        if (delta < 0) {
            (void)g_core->stat_drop(ctx, target, kSemanticToSlot[i], -delta * scale, owner);
        }
    }
}

// 对手是否**已处于弱化状态**（六槽任一负等级；本体权威态，不看回合视图）。
inline bool target_is_weakened(const BattleContext* ctx, int target) {
    for (int i = 0; i < 6; ++i) {
        if (ctx->ability_levels[target][i] < 0) {
            return true;
        }
    }
    return false;
}

// ── 590 —— 使对手{0}，{6}%弱化效果翻倍 ──
// int_args = [owner, target, 攻, 防, 特攻, 特防, 速, 命中, 概率]（args_num=7）。
EffectResult effect_590_weaken_chance_double(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 590, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 9, 590, args,
                     "590 布局=[owner,target,六槽增量,概率]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     590, args, "需要 owner/target ∈ {0,1}");
    const int chance = args.int_args[8];
    EFFECT_ARG_GUARD(chance >= 0, 590, args, "需要 概率 >= 0（在位即消费，0=永不翻倍）");
    if (!g_core || !g_core->stat_drop) {
        return EffectResult::kOk;
    }
    // 整段一次掷骰：显式 0 走短路（不消耗 rand），>=100 必定翻倍。
    const int scale = roll_percent(ctx->rng(), chance) ? 2 : 1;
    ctx->plugin_storage[k590ScaleAuditBase + owner] = scale;
    apply_six_slot_weaken(ctx, owner, target, args.int_args + 2, scale);
    return EffectResult::kOk;
}

// ── 625 —— 使对手{0}，若对手处于弱化状态，弱化效果翻倍 ──
// int_args = [owner, target, 攻, 防, 特攻, 特防, 速, 命中]（args_num=6）。
EffectResult effect_625_weaken_cond_double(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 625, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 8, 625, args,
                     "625 布局=[owner,target,六槽增量]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     625, args, "需要 owner/target ∈ {0,1}");
    if (!g_core || !g_core->stat_drop) {
        return EffectResult::kOk;
    }
    // 判定必须在本段弱化落地**之前**：本段自己造成的下降不计入下一段的门。
    const int scale = target_is_weakened(ctx, target) ? 2 : 1;
    ctx->plugin_storage[k625ScaleAuditBase + owner] = scale;
    apply_six_slot_weaken(ctx, owner, target, args.int_args + 2, scale);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(590, effect_590_weaken_chance_double)
SKILL_EFFECT(625, effect_625_weaken_cond_double)
