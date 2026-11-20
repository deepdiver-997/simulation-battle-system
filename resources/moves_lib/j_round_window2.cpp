// J 组 J24 · 回合窗响应族二 —— 3 宏/56 载体（2026-10-02，worktree ../agents/j24）
//
// 家族语义（effect_info 模板即权威）：
//   535 [1] {0}回合内若受到攻击则自身防御+1 特防+1
//   153 [2] {0}回合内每回合对对方造成伤害的1/{1}恢复自身体力
//   169 [3] {0}回合内每回合额外附加{1}%概率令对手{2}
//
// 实现口径：
//   - 535 = EVENT_TAKE_DAMAGE watcher（817/462 通道：TEAM scope、InRounds {0} 含当回合；
//     "受攻击"= 红伤 NORMAL 判据、ev.amount>0、自伤不算）→ stat_change(self, 2/3 槽, +1)
//     ——自身增益走 stat_change 真原语（不吃免弱，本体/视图同步，common.h 注）。
//     每次受击都触发（模板无"每回合一次"限定）。
//   - 153 = EVENT_TAKE_DAMAGE watcher（ev.actor==self && ev.target==opp && NORMAL）把当回合
//     对敌红伤累进 plugin_storage（键 kBase153+round，回合隔离）；ROUND_END InRounds 同窗
//     tick（1655 注册面）读当回合累计 heal_amount(acc/{1})（吃封回血/恢复修正）。
//   - 169 = 592 的 **InRounds** 变体（"{0}回合内"含当回合——首回合施放即计"用技"）；
//     "每回合用技"不限攻击技（模板无"攻击"字样，只判 used_skill_this_round）；
//     AFTER_ACTION 双侧注册（桶只跑行动方，52 教训）+ J3 申报制 apply_anomaly_checked。
//
// 载体切片（unity 库 json_each 精确匹配 20+20+17=57→去重 56；side_effect.arg_count 切分
// 全载体自洽，脚本核算 2026-10-02；23705 阵列含两次 169=官方数据原样）：
//   535  [rounds]：22736 神子之威 [3]（+两枚 effect5 伴技，测防/特防不受扰）、33213 暴龙撕咬 [2]
//   153  [rounds,denom]：21414 喋血之魂 [2,2]（纯载体）、17157 聚邪咒 [3,4]、28505 幽镰引魂 [4,1]
//   169  [rounds,chance,异常码]：22075 火之奥义 [48,169]|[5,3,100,2]（chance=100 测试载体）、
//        28624 刺耳噪音 [3,30,0]、29448 圣光之月 [2,100,43]
//
// 动工前置三查（2026-10-02）：①内核 grep 3 id 无硬编码 ②custom_effect_programs/overrides
// 无声明 ③moves_lib 无既有注册——真缺口。
// 场景回归：test/scenario/scenario_1090_round_window2.cpp（号段 1090-1099）。

#include "common.h"

namespace {

// 153 的当回合对敌红伤累计键（kBase153 + roundCount；"bool 撞 int 坑"纪律：int 段）。
constexpr int kBase153 = 990153000;

// storage 整数读（j_purge_success2 同款；plugin_storage 是 map<int,std::any>）。
int j24_storage_int(const BattleContext* ctx, int key) {
    const auto it = ctx->plugin_storage.find(key);
    return it == ctx->plugin_storage.end() ? 0 : any_int_or(it->second);
}

// ── 535 —— {0}回合内若受到攻击则自身防御+1 特防+1 ──
// int_args = [owner, target, rounds]（args_num=1）。
EffectResult effect_535_onhit_def_spdef_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 535, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 535, args,
                     "535 布局=[owner,target,回合]");
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && args.int_args[1] >= 0 && args.int_args[1] <= 1,
                     535, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0, 535, args, "需要 回合 > 0");
    if (!g_core || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, self, ctx->roundCount, rounds, /*once=*/false,
        [self](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != self || !g_core || !g_core->stat_change) {
                return;
            }
            // "受攻击" = 红伤（817 同判据；自伤不算）。
            if (ev.damage_kind != static_cast<int>(DamageKind::NORMAL) || ev.amount <= 0) {
                return;
            }
            if (ev.actor < 0 || ev.actor > 1 || ev.actor == self) {
                return;
            }
            (void)g_core->stat_change(c, self, 2, +1);   // 防御 +1
            (void)g_core->stat_change(c, self, 3, +1);   // 特防 +1
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 153 的 ROUND_END tick —— payload={owner, opp, denom}；读当回合累计，heal 1/{1}。
EffectResult j24_153_round_end_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int denom = args.int_args[2];
    if (owner < 0 || owner > 1 || denom <= 0) {
        return EffectResult::kOk;
    }
    const int acc = j24_storage_int(ctx, kBase153 + ctx->roundCount);
    if (acc > 0) {
        (void)g_core->heal_amount(ctx, owner, acc / denom);
    }
    return EffectResult::kOk;
}

// ── 153 —— {0}回合内每回合对对方造成伤害的1/{1}恢复自身体力 ──
// int_args = [owner, target, rounds, denom]（args_num=2）。
EffectResult effect_153_dealt_heal_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 153, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 153, args,
                     "153 布局=[owner,target,回合,分母]");
    const int self = args.int_args[0];
    const int target = args.int_args[1];
    const int rounds = args.int_args[2];
    const int denom = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && target >= 0 && target <= 1 && self != target,
                     153, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && denom > 0, 153, args, "需要 回合/分母 > 0");
    // 累计 watcher：我打对手的红伤（对敌口径；窗口 InRounds 含当回合）。
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, self, ctx->roundCount, rounds, /*once=*/false,
        [self, target](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.actor != self || ev.target != target) {
                return;
            }
            if (ev.damage_kind != static_cast<int>(DamageKind::NORMAL) || ev.amount <= 0) {
                return;   // "造成伤害" = 红伤
            }
            const int key = kBase153 + c->roundCount;
            const auto it = c->plugin_storage.find(key);
            const int cur = it == c->plugin_storage.end()
                                ? 0
                                : any_int_or(it->second);
            c->plugin_storage[key] = cur + ev.amount;
        },
        WatcherScope::TEAM);
    // ROUND_END 结算：每窗回合末 heal 1/{1}（1655 注册面，InRounds 同窗）。
    register_round_effect(ctx, State::BATTLE_ROUND_END, self, 153, rounds,
                          {self, target, denom}, &j24_153_round_end_tick);
    return EffectResult::kOk;
}

// ── 169 的 AFTER_ACTION tick —— payload={owner, target, chance, anomaly}。
EffectResult j24_169_round_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int chance = args.int_args[2];
    const int anomaly = args.int_args[3];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    // "每回合用技"：不限攻击技（模板无"攻击"字样）。
    if (!used_skill_this_round(ctx, owner)) {
        return EffectResult::kOk;
    }
    if (g_core && g_core->apply_anomaly_checked) {
        (void)g_core->apply_anomaly_checked(ctx, target, anomaly, random_anomaly_duration(),
                                            owner, chance, ChanceSource::Skill);
    }
    return EffectResult::kOk;
}

// ── 169 —— {0}回合内每回合额外附加{1}%概率令对手{2} ──
// int_args = [owner, target, rounds, chance, anomaly]（args_num=3）。
EffectResult effect_169_skill_window_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 169, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 169, args,
                     "169 布局=[owner,target,回合,概率,异常码]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int rounds = args.int_args[2];
    const int chance = args.int_args[3];
    const int anomaly = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     169, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && chance > 0, 169, args, "需要 回合/概率 > 0");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 169, args, "需要 异常码 ∈ [0,43]");
    // "{0}回合内" = InRounds 含当回合（施放回合的"用技"即计）。
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_round_effect(ctx, st, owner, 169, rounds, {owner, target, chance, anomaly},
                              &j24_169_round_tick);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(535, effect_535_onhit_def_spdef_window)
SKILL_EFFECT(153, effect_153_dealt_heal_window)
SKILL_EFFECT(169, effect_169_skill_window_anomaly)
