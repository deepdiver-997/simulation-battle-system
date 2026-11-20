// J9 · 受击响应族二 —— 1322×23 / 1200×20 / 1594×11 / 1599×3（2026-10-01，worktree ../agents/j9）
//
// 家族语义（effect_info 官方模板，四条同构）："{0}回合内受到攻击则{1}%X，未触发则Y"——
// F6 817 直系（受击 watcher → 受击时点分支），全部纯插件可达、不改内核：
//
// 1322（arg_count=4）"{0}回合内受到攻击则{1}%令对手{2}，未触发则令对手全属性-{3}"：
//   - 受击 watcher（EVENT_TAKE_DAMAGE，817 同款注册法；"受攻击"= damage_kind==NORMAL、
//     amount>0、actor 为合法对手——粉伤 tick/真伤/属性直伤不构成"受到攻击"，E12 口径）；
//   - 每次 qualifying 受击：apply_anomaly_checked（申报制，chance={1}，916/J3 口径）
//     对**攻击者**施 {2} 号异常；未落地（含被免/被挡各档）→ 攻击者全属性-{3}
//     （stat_drop 六槽，弱化原语查免弱——1009/J4 同款循环；actor=属主如实声明）。
//   - 实例：雷霆魔剑 26450 [4,100,0,1]、末日诅咒 25990 [4,100,23,1]（全库 23 载体）。
// 1200（arg_count=3）"{0}回合内受到攻击则{1}%对手{2}，未触发则消除对手回合类效果"：
//   - 同上 watcher + 申报制；未落地 → break_round_effects(攻击者)
//     （"消除对手回合类效果"= 对手（攻击者）身上的回合类效果；remove_round_effects
//     复用 break_round_effects，J3 2068 兜底同款原语、吃免断+EVENT_BREAK）。
//   - 实例：磁暴领域 25556 [4,100,0]、迷离身姿 25713 [4,70,22]（全库 20 载体）。
// 1594（arg_count=5）"{0}回合内受到攻击则{1}%使自身全属性+{2}，未触发则{3}%使对手全属性-{4}"：
//   - 主干 roll_percent（{1}，850 同款掷点）→ all_stats_up（common.h，stat_change 六槽
//     真原语——自身增益不吃免弱）；未掷中 → 再掷 {3}% → stat_drop 对手全属性-{4}。
//   - 取态记档：主干"触发"以 roll_percent 为准（不放大利好/利空方向的二级状态，
//     850 同口径）；STAT_BOOST 被封时 roll 已消耗、不算"未触发"（全属性提升是六槽
//     独立槽位，部分槽 AT_CAP 仍算触发）。
//   - 实例：混沌血源 27229 [3,50,1,100,1]、玄铁核心 27258 [4,50,1,100,1]（全库 11 载体，
//     {1} 全为 50——场景统计断言）。
// 1599（arg_count=5）"{0}回合内受到攻击则{1}%令对手{2}，未触发则{3}%令对手{4}"：
//   - 申报制主干 + 申报制兜底（两段独立 chance）；兜底掷点失败则本回合无事发生。
//   - 实例：淬寒铸体 27244 [4,100,2,100,5]（烧伤→冻伤）、暗雷聚涌 27270 [4,100,6,100,0]
//     （害怕→麻痹；生命灌注 27444 [4,100,28,100,0] 束缚→麻痹）（全库 3 载体）。
//
// ⚠️ "未触发"口径（全族统一，916 申报制）：主干 apply_anomaly_checked 未落地
//    （IMMUNE/已处同类/附加禁令等各档都算）或 roll_percent 未掷中 → 当次受击立刻
//    接兜底；窗口 {0} 回合内每次 qualifying 受击独立判定（1676 每回合掷点+未触发
//    兜底同构；J3 全族"未触发=当次 !landed"同口径）。chance=100 的载体兜底为
//    常驻死分支（仅被免时可达）——数据如此，忠实落地。
//
// 前置三查（2026-10-01）：①内核 grep 1322/1200/1594/1599 无行为硬编码
//   ②effect_meta 无该批 id 声明（均 SKILL_EFFECT 即时型，无窗口家族牵连）
//   ③moves_lib 无既有注册——全族真缺口。载体切片抽查 20+ 条全部吻合
//   [回合,概率,(异常码|升降),…] 布局；全族 57 载体与工单一致。
// 先例：817（受击 watcher 判据/注册法，F6 直系）、916/J3（申报制"未触发"）、
//   1009/J4（全属性降六槽 stat_drop）、1293/batch_a（all_stats_up）、
//   J3 2068（break_round_effects 兜底）、burn_trio（受击 watcher 内施加异常先例）、
//   850（roll_percent 掷点口径）。
// 场景回归：test/scenario/scenario_930_onhit_family2.cpp（930-934 号段）。

#include "common.h"

namespace {

// 受击判据（817 同款）：打在我身上的红伤、量>0、来源是合法对手。
bool j9_attacked_by(const BattleEvent& ev, int self, int& actor) {
    if (ev.target != self) {
        return false;
    }
    if (ev.damage_kind != static_cast<int>(DamageKind::NORMAL) || ev.amount <= 0) {
        return false;   // 粉伤 tick/真伤/属性直伤不构成"受到攻击"（E12 口径）
    }
    actor = ev.actor;
    return actor >= 0 && actor <= 1 && actor != self;   // 自伤不算"受到攻击"
}

// "未触发"判定（916/J3 申报制口径）：未落地（含被免/被挡各档）即未触发。
bool j9_anomaly_landed(ApplyAnomalyResult r) {
    return r == ApplyAnomalyResult::SUCCESS
        || r == ApplyAnomalyResult::REPLACED_EXISTING
        || r == ApplyAnomalyResult::DURATION_EXTENDED;
}

// 概率异常申报（对手侧；chance={1}）。无效异常码按未申报处理（false=未触发）。
bool j9_try_anomaly(BattleContext* ctx, int owner, int target, int chance, int anomaly) {
    if (!g_core || !g_core->apply_anomaly_checked || chance <= 0
        || !is_valid_abnormal_status_id(anomaly)) {
        return false;
    }
    const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
        ctx, target, anomaly, random_anomaly_duration(), owner, chance, ChanceSource::Skill);
    return j9_anomaly_landed(r);
}

// 全属性-{n}（六槽 stat_drop 弱化原语，查免弱；1009/J4 同款；actor=属主如实声明）。
void j9_all_stats_down(BattleContext* ctx, int owner, int target, int n) {
    if (!g_core || !g_core->stat_drop || n <= 0) {
        return;
    }
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        (void)g_core->stat_drop(ctx, target, i, n, owner);
    }
}

// ── 1322 —— {0}回合内受到攻击则{1}%令对手{2}，未触发则令对手全属性-{3} ──
EffectResult effect_1322_onhit_anomaly_else_drop(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1322, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 1322, args,
                     "1322 布局=[owner,target,回合,概率,异常码,全属性降幅]");
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    const int chance = args.int_args[3];
    const int anomaly = args.int_args[4];
    const int drop = args.int_args[5];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && rounds > 0, 1322, args, "需要 rounds>0");
    // 受击 watcher：{0} 回合窗口（InRounds 含当回合，817 同款注册法），TEAM 作用域。
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, self, ctx->roundCount, rounds, /*once=*/false,
        [self, chance, anomaly, drop](BattleContext* c, const BattleEvent& ev) {
            int actor = -1;
            if (!c || !j9_attacked_by(ev, self, actor)) {
                return;
            }
            if (!j9_try_anomaly(c, self, actor, chance, anomaly)) {
                j9_all_stats_down(c, self, actor, drop);   // 未触发 → 对手全属性-{3}
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 1200 —— {0}回合内受到攻击则{1}%对手{2}，未触发则消除对手回合类效果 ──
EffectResult effect_1200_onhit_anomaly_else_break(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1200, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1200, args,
                     "1200 布局=[owner,target,回合,概率,异常码]");
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    const int chance = args.int_args[3];
    const int anomaly = args.int_args[4];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && rounds > 0, 1200, args, "需要 rounds>0");
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, self, ctx->roundCount, rounds, /*once=*/false,
        [self, chance, anomaly](BattleContext* c, const BattleEvent& ev) {
            int actor = -1;
            if (!c || !j9_attacked_by(ev, self, actor)) {
                return;
            }
            if (!j9_try_anomaly(c, self, actor, chance, anomaly) && g_core
                && g_core->break_round_effects) {
                (void)g_core->break_round_effects(c, actor);   // 未触发 → 消除对手回合类效果
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 1594 —— {0}回合内受到攻击则{1}%使自身全属性+{2}，未触发则{3}%使对手全属性-{4} ──
EffectResult effect_1594_onhit_boost_else_drop(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1594, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 7, 1594, args,
                     "1594 布局=[owner,target,回合,概率,提升,概率2,降幅]");
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    const int chance = args.int_args[3];
    const int up = args.int_args[4];
    const int chance2 = args.int_args[5];
    const int drop = args.int_args[6];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && rounds > 0, 1594, args, "需要 rounds>0");
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, self, ctx->roundCount, rounds, /*once=*/false,
        [self, chance, up, chance2, drop](BattleContext* c, const BattleEvent& ev) {
            int actor = -1;
            if (!c || !j9_attacked_by(ev, self, actor)) {
                return;
            }
            if (roll_percent(chance)) {
                all_stats_up(c, self, up);   // 自身增益走 stat_change 真原语（不吃免弱）
            } else if (roll_percent(chance2)) {
                j9_all_stats_down(c, self, actor, drop);   // 兜底二次掷点 → 对手全属性-{4}
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 1599 —— {0}回合内受到攻击则{1}%令对手{2}，未触发则{3}%令对手{4} ──
EffectResult effect_1599_onhit_dual_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1599, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 7, 1599, args,
                     "1599 布局=[owner,target,回合,概率,异常码,概率2,异常码2]");
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    const int chance = args.int_args[3];
    const int anomaly = args.int_args[4];
    const int chance2 = args.int_args[5];
    const int anomaly2 = args.int_args[6];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && rounds > 0, 1599, args, "需要 rounds>0");
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, self, ctx->roundCount, rounds, /*once=*/false,
        [self, chance, anomaly, chance2, anomaly2](BattleContext* c, const BattleEvent& ev) {
            int actor = -1;
            if (!c || !j9_attacked_by(ev, self, actor)) {
                return;
            }
            if (!j9_try_anomaly(c, self, actor, chance, anomaly)) {
                (void)j9_try_anomaly(c, self, actor, chance2, anomaly2);   // 兜底二次申报
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1322, effect_1322_onhit_anomaly_else_drop)
SKILL_EFFECT(1200, effect_1200_onhit_anomaly_else_break)
SKILL_EFFECT(1594, effect_1594_onhit_boost_else_drop)
SKILL_EFFECT(1599, effect_1599_onhit_dual_anomaly)
