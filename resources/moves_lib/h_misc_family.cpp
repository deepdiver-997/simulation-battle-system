#include <utils/sbs_rand.h>
// H 组 H10 · misc 散族 —— 491×29 / 446×20 / 516×21 / 472×21 / 154×21 / 555×21
//（2026-09-30，worktree ../agents/h10；场景 scenario_870_misc_family.cpp，段 870-879
//  ——看板行无自带段、v2 分配器未接管，自取 870-879（H8 用 860-869，全库无冲突））
//
// 效果与载体（DB 双表切片 + 4399 双源核；六效果链上伙伴 arg_count/args_num 全一致）：
//   491（args_num=2）"{0}回合内对手造成的伤害降低{1}%"：
//     触手强击 14987 [2,50]→"2回合内对手造成的伤害降低50%"；连岳绝牙袭 [3,50]、
//     遮天巨叶 [3,50]、绝灭利刃 [2,50]、纵横宇宙 [450,491]|[100,200,3,50]→切片[3,50]。
//   446（args_num=0）"随机自身3项属性+1"：妖·火舞红莲 17388 [33,446]、萃之绝命破
//     [180,446]、六芒神剑 [85,446]、分魂破世击 [446,456]|[300]——全链 0 参。
//   516（args_num=7）"{0}..{5},体力值低于1/{6}时强化效果翻倍"（{0}..{5}=六槽**语义序**
//     旗标表）：不屈意志 22555 [1,1,0,1,0,0,2]→4399"攻击、防御、特防+1，体力值低于
//     1/2时强化效果翻倍"——旗标位 0,1,3 = 攻,防,特防 ✓；雷神之怒 [1,1,0,1,1,0,2]→
//     "攻击防御特防速度+1" ✓；红莲业火 [1,1,0,0,1,0,2]→"攻击+1、速度+1、防御+1" ✓；
//     枯木发荣 [1,1,0,1,1,0,2]→"攻速、双防+1" ✓；屠龙领域 [1,1,0,1,1,1,2]→"攻击+1、
//     双防+1、速度+1、命中+1" ✓——五载体语义序交叉验证全吻合（H2 158/H6 477 同序）。
//   472（args_num=1）"若对手处于{0}状态则每次攻击造成的伤害都将恢复自身体力"：
//     嗜火梵天绝 14091 [472]|[2]→"若对手处于烧伤状态则每次攻击…"（2=烧伤）；紫电红雷爆
//     [6]→害怕。
//   154（args_num=2）"若对手{0}，则对对方造成伤害的1/{1}恢复自身体力"：
//     精神侵蚀 12744 [8,2]→"若对手睡眠，则对对方造成伤害的1/2恢复自身体力"；
//     魔藤之种 [1,2]→中毒 1/2；天火炎龙波 [2,3]→烧伤 1/3；深渊之爪 [1,4]→中毒 1/4。
//   555（args_num=1）"若打出致命一击则自身{0}回合内免疫异常状态"：苍月无极闪 15310
//     [555]|[2]→"若打出致命一击则自身2回合内免疫异常状态"；抹杀 [5]→5回合。
//
// 判据与先例：
//   - 491 = 525 同构（round_window.cpp）：REDUCE_PCT 阶段一次性注册管线回调 +
//     start/expire 半开区间窗口判（条目常驻、窗口外自失效——525 记档的"回调不随断回合
//     消失"边界同承）；MITIGATE 类别（减伤不被"挡伤失效"无视，速查 §三）；窗口起点走
//     round_effect_start_round（InRounds 后手顺延官方口径）。多来源并列时各自乘算
//     （REDUCE_PCT 阶段"乘算槽连乘"语义；本单全库切片均 50%，单来源场景不受影响，记档）。
//   - 446 随机 3 项：partial Fisher-Yates + sbs_rand（1244 随机档同源）；六槽=引擎槽
//     0攻..5命中（"3项属性"取自六项能力，速查 §三）；走 stat_change 真原语（自身增益
//     不吃免弱，本体/视图同步）。
//   - 516 强化 = 旗标表 stat_change（语义序→引擎槽映射 {0,2,1,3,4,5}，477 同款）；
//     "体力值低于1/{6}" = 当前 hp < max_hp/{6} 严格小于；"强化效果翻倍"按 F5 1344 口径
//     自限为**本效果的强化翻倍**（+1 → +2）。
//   - 472/154 吸血 = **EVENT_TAKE_DAMAGE watcher duration=1**（本技能出手当回合的
//     "每次攻击"逐笔生效——多段攻每段各吸；811 的 TAKE_DAMAGE 累计同通道）：
//     actor==self、target==opp、damage_kind==NORMAL（只吸攻击红伤，E12 反伤族同过滤）
//     → 对手仍处于 {0}（实时查 has_active_anomaly）→ heal_amount（吃封回血）。
//     472 吸全额、154 吸 1/{1}（整型除法）。⚠️ 作用域记档：模板无回合参数，按"本技能
//     使用当回合"落地（4399 载体全是单体攻击技、无跨回合文本佐证，保守取态）。
//   - 555 = crit_happened[owner] 判据（query_usage ①.5 已置位，e_crit_trigger/wing
//     1117 同读取面）→ grant_immunity(ANOMALY, duration={0})（Modern 默认层，
//     471/48/1440 同款绝对窗口）。
//
// 前置三查（2026-09-30）：①内核 grep 六 id 零硬编码 ②custom_effect_overrides 无行
//   ③moves_lib 无既有注册（命中 1545 是别的效果）——真缺口。
//
// 场景回归：test/scenario/scenario_870_misc_family.cpp（870-879 号段，一文件覆盖
//   六效果；446 用种子扫描钉"恰好 3 项"、555 用 crit_rate=16 必暴口径造致命一击）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

// 语义序（攻,防,特攻,特防,速,命中）→ 引擎槽（477/H2 158 同款映射）。
constexpr int kSemanticToSlot[6] = {0, 2, 1, 3, 4, 5};

// ── 491 —— {0}回合内对手造成的伤害降低{1}% ──
// int_args = [owner, target, rounds{0}, pct{1}]。525 同构（见文件头）。
EffectResult effect_491_damage_down_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 491, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 491, args,
                     "491 布局=[owner,target,rounds,pct]（int_count>=4）");
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    const int pct = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 491, args, "owner 越界");
    EFFECT_ARG_GUARD(rounds > 0 && pct > 0, 491, args, "需要 rounds>0 且 pct>0");
    const int start = ctx->round_effect_start_round(self, rounds, EffectWindowKind::InRounds);
    const int expire = start + rounds;
    ctx->register_damage_effect(
        DamagePhase::REDUCE_PCT, self, DamageEffectCategory::MITIGATE,
        [self, start, expire, pct](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != self) {
                return;
            }
            if (c->roundCount < start || c->roundCount >= expire) {
                return;   // 窗口外（含已过期）——条目仍在，但不再产生作用（525 同口径）
            }
            DamageSnapshot& d = c->resolvedDamage;
            if (d.defenderId != self || !d.isRed || d.final <= 0) {
                return;   // 只削"对手造成的攻击伤害"（红伤）
            }
            d.final = std::max(0, d.final * (100 - pct) / 100);
        });
    return EffectResult::kOk;
}

// ── 446 —— 随机自身3项属性+1 ──
// int_args = [owner, target]（0 参模板）。partial Fisher-Yates 取 3 个不重复槽。
EffectResult effect_446_random_three_up(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 446, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 446, args,
                     "446 布局=[owner,target]");
    const int owner = args.int_args[0];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 446, args, "owner 越界");
    EFFECT_ARG_GUARD(g_core && g_core->stat_change != nullptr, 446, args,
                     "需要 stat_change 原语");
    int slots[6] = {0, 1, 2, 3, 4, 5};
    for (int i = 0; i < 3; ++i) {
        const int j = choose_skill_range(*ctx, 446, 0, owner, owner, i, 5);
        const int tmp = slots[i];
        slots[i] = slots[j];
        slots[j] = tmp;
        (void)g_core->stat_change(ctx, owner, slots[i], 1);
    }
    return EffectResult::kOk;
}

// ── 472/154 共用体 —— 异常吸血窗口：本回合每次攻击红伤逐笔按对手异常状态吸血 ──
// args = [owner, target, anomaly, denom]（denom=1 → 全额，472；154 传 {1}）。
EffectResult effect_anomaly_drain_window(BattleContext* ctx, const EffectArgs& args, int effect_id) {
    EFFECT_ARG_GUARD(ctx != nullptr, effect_id, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, effect_id, args,
                     "布局=[owner,target,anomaly,denom]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int anomaly = args.int_args[2];
    const int denom = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && opp != self, effect_id,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(denom > 0 && is_valid_abnormal_status_id(anomaly), effect_id, args,
                     "需要 denom>0 且 {0}=合法异常码");
    if (!g_core || !g_core->heal_amount || !g_core->has_active_anomaly) {
        return EffectResult::kOk;
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, self, ctx->roundCount, /*duration_rounds=*/1,
        /*once=*/false,
        [self, opp, anomaly, denom, effect_id](BattleContext* c, const BattleEvent& ev) {
            if (!c || !g_core || !g_core->heal_amount || !g_core->has_active_anomaly) {
                return;
            }
            // 只认"自己打对手"的攻击红伤（E12 反伤族同过滤；粉伤/真伤不吸）
            if (ev.actor != self || ev.target != opp || ev.amount <= 0
                || ev.damage_kind != static_cast<int>(DamageKind::NORMAL)) {
                return;
            }
            if (!g_core->has_active_anomaly(c, opp, anomaly)) {
                return;   // 对手不处于{0}：本笔不吸
            }
            const int heal = denom == 1 ? ev.amount : ev.amount / denom;
            if (heal > 0) {
                (void)g_core->heal_amount(c, self, heal);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

EffectResult effect_472_anomaly_drain(BattleContext* ctx, const EffectArgs& args) {
    // int_args = [owner, target, {0}=异常码]（472 吸全额；补 denom=1 复用 154 通道）。
    EFFECT_ARG_GUARD(ctx != nullptr, 472, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 472, args,
                     "472 布局=[owner,target,anomaly]");
    std::vector<int> ints(args.int_args, args.int_args + args.int_count);
    if (args.int_count < 4) {
        ints.push_back(1);   // denom=1 → 全额
    }
    return effect_anomaly_drain_window(ctx, EffectArgs(ints), 472);
}

EffectResult effect_154_anomaly_drain(BattleContext* ctx, const EffectArgs& args) {
    // int_args = [owner, target, {0}=异常码, {1}=分母]。
    return effect_anomaly_drain_window(ctx, args, 154);
}

// ── 555 —— 若打出致命一击则自身{0}回合内免疫异常状态 ──
// int_args = [owner, target, rounds]。crit_happened 在 query_usage ①.5 已置位，
// SKILL_EFFECT 时点可读（wing 1117 同面）。
EffectResult effect_555_crit_anomaly_immune(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 555, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 555, args,
                     "555 布局=[owner,target,rounds]");
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 555, args, "owner 越界");
    EFFECT_ARG_GUARD(rounds > 0, 555, args, "需要 rounds>0");
    if (!ctx->crit_happened[owner]) {
        return EffectResult::kOk;   // 未打出致命一击：不授票
    }
    // Ancient 层（48 同形老系纯免："两种施加都挡"——与 H2 471 的 Modern 取态按载体
    // 年代分档：555 载体 15xxx 老代、471 载体 33xxx+ 新代；古早施加只被 Ancient 挡）。
    ctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(),
                        /*anomaly_mask=*/0, /*duration_rounds=*/rounds,
                        /*source_id=*/0, /*soul_immunity=*/false, EffectScope::ON_STAGE,
                        /*counts=*/0, /*source_effect_id=*/555,
                        /*tier=*/ImmunityTier::Ancient);
    return EffectResult::kOk;
}

// ── 516 —— {0}..{5},体力值低于1/{6}时强化效果翻倍 ──
// int_args = [owner, target, f0..f5（语义序旗标）, denom]（int_count=9）。
EffectResult effect_516_low_hp_double_boost(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 516, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 9, 516, args,
                     "516 布局=[owner,target,六槽旗标(语义序),denom]");
    const int owner = args.int_args[0];
    const int denom = args.int_args[8];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 516, args, "owner 越界");
    EFFECT_ARG_GUARD(denom > 0, 516, args, "需要 分母>0");
    EFFECT_ARG_GUARD(g_core && g_core->stat_change != nullptr, 516, args,
                     "需要 stat_change 原语");
    // "体力值低于1/{6}"=当前 hp < max/{denom}（严格），低体 → 本效果强化翻倍（1344 口径
    // 自限：只翻本效果）。boost 基准 = 旗标值（全库切片 0/1）。
    const bool low_hp = ctx->getPet(owner).hp < max_hp_of(ctx, owner) / denom;
    for (int i = 0; i < 6; ++i) {
        const int flag = args.int_args[2 + i];
        if (flag > 0) {
            (void)g_core->stat_change(ctx, owner, kSemanticToSlot[i], flag * (low_hp ? 2 : 1));
        }
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(491, effect_491_damage_down_window)
SKILL_EFFECT(446, effect_446_random_three_up)
SKILL_EFFECT(472, effect_472_anomaly_drain)
SKILL_EFFECT(154, effect_154_anomaly_drain)
SKILL_EFFECT(555, effect_555_crit_anomaly_immune)
SKILL_EFFECT(516, effect_516_low_hp_double_boost)
