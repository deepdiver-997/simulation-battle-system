// K417 逐宠定制池二批 —— 德布比特 4499 回体Ⅰ/失体Ⅰ标记族（6 宏落地）
// （看板 K417，a01-k417 2026-11-06，worktree ~/.agent-workspaces/workspaces/sbs-local/k417，
//  分支 feat/k417；场景号段 7950-7959，场景 test/scenario/scenario_7950_debubite_pool.cpp。
//  选载体依据：coverage_recompute 2026-11-06 03:13 快照（链级可用率 92.81% 非空链口径，
//  registry moves=1422）逐宠聚合（monster_learnable_moves join moves.side_effect）：
//  4499 德布比特 16 技 / 非空链 13 / 链级可用 9 = 69.2%，缺口恰好是连号段 1966-1971
//  六连号（无其他缺号）且为该宠专属定制资源族（回体Ⅰ/失体Ⅰ标记），未被 J/K 批产
//  覆盖。排除对照：普鲁修托 4489 缺口 1940-1945（6 连号，备选）；始皇帝·嬴政 4468
//  缺口 1899-1905 中 1899/1904 是条件先制族（选择期路由内核门槛，K416 1634 同款记档
//  负担）；霍光 4515 缺 2011-2015 非纯连号簇。4321 及 K416 已实现 8 id
//  （519/1720/1476/1757/1758/1759/1760/1762）全部避开——4321 复算链级 15/16（K416
//  落地后回升，56.25% 是 K416 实现前口径）。德布比特 monsters.raw_json 的
//  sp_extra_moves 带 37021 缘结·浮世诵（[699,1568,2132,2133,837]），其 2132/2133
//  同引回体/失体标记——本单不实现（2133"自身持有回体时先制+1"是选择期路由内核
//  门槛，J66 口径 src/ 本单冻结），留续单；本文件标记槽键段即其消费端对接面。）
//
// 行清单（moves 切片 side_effect.arg_count 优先，与 effect_info.args_num 6/6 一致；
// 切片逐位核对）：
//   1966 [2] "消除对手能力提升状态，若消除成功且自身持有回体Ⅰ则下回合造成的攻击伤害
//        提升{0}%，若消除成功且对手持有失体Ⅰ则下回合造成的固定伤害、百分比伤害提升{1}%"
//        载体：归一 36548[1138,1966][100,7,100,6,100,50]（1966 切片 [100,50]；4499 专属）
//   1967 [1] "若自身持有回体标记则{0}回合内自身回合类效果无法被消除（后出手则延续1回合）"
//        载体：诸行无常 28285[1967,1968,191,971][2,3,3,4,4,3,2]（1967 切片 [2]；专属）
//   1968 [2] "若对手未持有失体Ⅰ则为对手附加持续{0}回合的失体Ⅰ，若对手已持有失体Ⅰ则
//        将回合数刷新至{1}回合"
//        载体：诸行无常 28285（1968 切片 [3,3]；专属）
//   1969 [1] "若对手持有失体标记则{0}回合内自身的能力提升状态无法被消除或吸取
//        （后出手则延续1回合）"
//        载体：诸法无我 28286[1969,1970,1283,843][2,3,3,1,2,2]（1969 切片 [2]；专属）
//   1970 [2] "若自身未持有回体Ⅰ则为自身附加持续{0}回合的回体Ⅰ，若自身已持有回体Ⅰ则
//        将回合数刷新至{1}回合"
//        载体：诸法无我 28286（1970 切片 [3,3]；专属）
//   1971 [0] "未击败对手则解放自身当前持有的回体Ⅰ和对手当前持有的失体Ⅰ，解放成功
//        任意一项则令对手下回合无法主动切换精灵且先制效果失效"
//        载体：缘起·苍生聚 36552[881,1221,1971,1156][1]（1971 零参；专属）
//
// 动工前置四查（2026-11-06）：
//   ① 内核 grep 1966-1971：唯一命中 include/effects/future_link.h:30 存储键常量
//     990219701（9902 前缀 + 19701 子串巧合），零行为硬编码；
//   ② effect_info 6/6 在列且 args_num == side_effect.arg_count（1966|2、1967|1、
//     1968|2、1969|1、1970|2、1971|0 逐位 OK）；custom_effect_programs 0 命中、
//     custom_effect_overrides 0 命中；四载体切片自洽（见行清单）；
//   ③ 全库并集无注册：registry moves=1422 / soul=410 均无 6 id；boss_invalid_effects
//     .json 命中 1966/1969/1970/1971 四条——来自**魂印空间** effect_icon 同号行
//     （4385/4387/4388/4389 的 tips，文本与本族完全不同文），SKILL_EFFECT 与
//     SOUL_MARK 两注册空间互不相干（K416 头注先例口径）；feat 分支（windows-net/
//     windows-port）与兄弟 worktree（agents/night-fuzz）grep 零注册；
//   ④ 废弃 parser 覆盖面（EffectUnit 兜底，2026-11-05 冻结）：六条模板逐一比对
//     parse_status_inflict_primary 四 pattern（令/使+对手/自身+{）零命中——1966 主句
//     "消除对手能力提升状态"、1971 尾句"令对手下回合无法主动切换精灵"均无"令对手{"
//     直随花括号形态；第二刀 179/700/1083/1257/456 条件模板也零命中 → 全 6 id =
//     NONE 安全跳过（无 K416 1762 型寄生）。本单注册后 clone_effect 先命中、parser
//     通道对该 6 id 关闭（skills.cpp 未注册分支）。
//
// 口径记档：
//   - 标记资源建模（§六 资源计数，K416 圣剑/处决印记顺延 4695 段）：
//     plugin_storage[4695000+side]=回体Ⅰ到期回合、[4695010+side]=失体Ⅰ到期回合
//     （0/缺=未持有）。"持续{0}回合"= 附加回合在内 [R, R+{0}-1]（与 1683 grant_
//     immunity"授予回合起算含当回合"口径对齐）；刷新 = 从当前回合重算 {1}；到期
//     残留条目不判持有（查询带 roundCount 下界）。标记为 side 级槽：本族生产/消费
//     均在场上交互，官方模板无跨场保留文案；下场不清（无依据即不私加清理），
//     与 K416 处决印记槽同款"实现细节，后续任务以本段为准对接"记档。
//   - 1967/1969 后出手延续：is_first_mover 读 preemptive_right 解析结果
//     （tianqi_dijun 821 同款 inline，J2 勘误口径），延续 = rounds+1。
//   - 1966 双窗："下回合" = NextRounds 起点 R+1 单回合窗：攻击增伤走
//     register_next_rounds_effect(ROUND_START)+effect_set_damage_amp（693/827 直系，
//     ⚠️ InRounds N=1 中途授予=死窗，j_purge_success2 827 教训 → 必须 NextRounds）；
//     固伤/百分比增伤走 register_pink_damage_effect(AMP)+窗口 [R+1, R+2)（1469 直系
//     改窗起点）。消强门槛 = clear_stat_boosts(opp)>0（2227/1217 直系，STAT_CLEAR
//     被挡返回 0）。
//   - 1971 未击败判定：官方"未击败"须伤害落地后判——1307/1308 用内核
//     effect_register_state case 路由 AFTER_ACTION，本单 src/ 冻结 → 插件侧等价
//     改造：SKILL_EFFECT 时点经 register_round_effect 把判定节点延挂 AFTER_ACTION
//     双桶（57 形，恰好一桶/回合，K416 519 同款），判定读对手 hp<=0（1307 同判据）。
//   - 1971 解放/锁切/先制失效：解放 = 持有才擦（到期残留不算持有）；任意一项解放
//     成功 → grant_switch_lock（1994 直系，NextRounds 起点、rounds=1 → 窗恰 [R+1,
//     R+1]，window_expired 判定 current-register>=rounds）+ 先制失效走
//     register_next_rounds_effect(MOVE_RIGHT)+ws.priority_nullified tick（1994
//     nullify_tick 直系；battleFsm 先制结算点统一清零，非异常条件区 K9 线）。
//   - 缘起·苍生聚链上 881（条件先制 MOVE_RIGHT 路由，已注册）/1221（反转+次免，
//     已注册）/1156（击败回满，已注册）不在本单范围；场景低血 KO 分支会触发 1156
//     回满属预期。
//   - 链上 1138（消除回合类→100% 令对手 7 疲惫 / 未触发→100% 令对手 6 害怕）已注册，
//     场景 1966 组的对手害怕来自 1138 未触发支（对手无回合类效果），非本宏产物。
//
// 存储键段（新增前已 grep 全库无撞；tools/check_storage_keys.py 门禁兜底）：
//   - 审计槽 10<id>000x 段（7690/K416 同款）：1019660001(消强清除数) / 1019660002
//     (攻击增伤%已授予) / 1019660003(粉伤增幅%已授予) / 1019660004(AMP 实际放大额) /
//     1019670001(BREAK 窗授予回合数) / 1019680001(失体Ⅰ 1=附加2=刷新) /
//     1019680002(失体Ⅰ到期回合) / 1019690001(强化保护窗授予回合数) /
//     1019700001(回体Ⅰ 1=附加2=刷新) / 1019700002(回体Ⅰ到期回合) /
//     1019710001(解放位掩码 bit0=回体 bit1=失体) / 1019710002(锁切+先制失效已授予) /
//     1019710003(先制失效 tick 执行回合戳)
//   - plugin_storage 4695xxx 段：回体Ⅰ 4695000+side；失体Ⅰ 4695010+side（值=到期回合）
//
// 遗留记档（不产码）：
//   - 2132/2133（缘结·浮世诵 37021，4499 sp_extra_moves）：2133"自身持有回体时先制+1"
//     是条件先制族，正解 = custom_skill_selection_routes DB 行（J327 起 DB 驱动）+
//     本文件标记槽查询；本单 DB 零改纪律 → 留续单。
//     【J331 已落地 2026-11-06】2133 实交 = 路由表扩行（29→32，j4/
//     selection_routes_expansion_3.sql）+ resources/moves_lib/j331_marker_cond_prio.cpp
//     （门条件读回体槽 4695000+side，判持有与 1970 附加口径同式）。
//     ⚠️ 勘误：上方引文只是模板前半句——effect_info 2133 全文 =
//     "自身持有回体时先制+1，**否则为自身附加3回合的回体并令对手疲惫**"，
//     否则支（附加+疲惫）已由 J331 一并落地（2132 仍留续单）。
//   - 回体Ⅰ/失体Ⅰ 自身被动（若官方在特性/魂印层有逐回合收益）不在 effect_info
//     家族内，属 K 系魂印/特性任务空间，本单不产码。

#include "common.h"

#include <functional>
#include <vector>

namespace {

// ── 审计槽 ──────────────────────────────────────────────────────────────
constexpr int k1966ClearAudit   = 1019660001;
constexpr int k1966AtkAudit     = 1019660002;
constexpr int k1966PinkAudit    = 1019660003;
constexpr int k1966PinkApplied  = 1019660004;
constexpr int k1967Audit        = 1019670001;
constexpr int k1968ModeAudit    = 1019680001;
constexpr int k1968ExpiryAudit  = 1019680002;
constexpr int k1969Audit        = 1019690001;
constexpr int k1970ModeAudit    = 1019700001;
constexpr int k1970ExpiryAudit  = 1019700002;
constexpr int k1971ReleaseAudit = 1019710001;
constexpr int k1971LockAudit    = 1019710002;
constexpr int k1971NullifyAudit = 1019710003;   // 先制失效 tick 实际执行标记

// ── 功能槽（键段声明见文件头）───────────────────────────────────────────
constexpr int kHuiTiBase = 4695000;   // +side：回体Ⅰ到期回合（0/缺=未持有）
constexpr int kShiTiBase = 4695010;   // +side：失体Ⅰ到期回合（0/缺=未持有）

// 先/后出手判定（tianqi_dijun_skills.cpp 821 同款 inline；J2 勘误：读 preemptive_right
// 解析结果，不读桶位）。
bool k417_is_first_mover(const BattleContext* ctx, int self) {
    if (!ctx || self < 0 || self > 1) {
        return false;
    }
    switch (ctx->preemptive_right) {
        case PreemptiveRight::SEER_ROBOT_1: return self == 0;
        case PreemptiveRight::SEER_ROBOT_2: return self == 1;
        default: return false;
    }
}

// 标记到期回合（0 = 未持有/未写入）。
int k417_marker_expiry(const BattleContext* ctx, int base, int side) {
    if (!ctx || side < 0 || side > 1) {
        return 0;
    }
    const auto it = ctx->plugin_storage.find(base + side);
    return it == ctx->plugin_storage.end() ? 0 : any_int_or(it->second, 0);
}

// 标记持有判定：写入过且未到期（到期残留条目不判持有）。
bool k417_marker_held(const BattleContext* ctx, int base, int side) {
    if (!ctx) {
        return false;
    }
    const int expiry = k417_marker_expiry(ctx, base, side);
    return expiry > 0 && expiry >= ctx->roundCount;
}

// 附加/刷新（返回 1=附加 2=刷新；"持续 N 回合"= 附加回合在内，到期回合 = R+N-1）。
int k417_marker_attach_or_refresh(BattleContext* ctx, int base, int side,
                                  int attach_rounds, int refresh_rounds) {
    const int now = ctx->roundCount;
    const bool held = k417_marker_held(ctx, base, side);
    const int expiry = now + (held ? refresh_rounds : attach_rounds) - 1;
    ctx->plugin_storage[base + side] = expiry;
    return held ? 2 : 1;
}

// 解放：持有才擦（返回是否真的解放了一项）。
bool k417_marker_release(BattleContext* ctx, int base, int side) {
    if (!k417_marker_held(ctx, base, side)) {
        return false;
    }
    ctx->plugin_storage.erase(base + side);
    return true;
}

// ═══════════════ 1966 消强 + 双增伤下回合窗 ═════════════════════════
EffectResult effect_1966_clear_marker_damage_window(BattleContext* ctx,
                                                    const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1966, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1966, args,
                     "1966 布局=[owner,target,攻击增伤%,固伤百分比增伤%]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int atk_pct = args.int_args[2];
    const int pink_pct = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && target != owner,
                     1966, args, "需要 owner/target ∈ {0,1} 且不同侧");
    EFFECT_ARG_GUARD(atk_pct > 0 && pink_pct > 0, 1966, args, "需要 两档增伤%>0");
    // 消强门槛：clear_stat_boosts 返回清除数，被 STAT_CLEAR 免消除挡下时为 0（2227 直系）。
    int cleared = 0;
    if (g_core && g_core->clear_stat_boosts) {
        cleared = g_core->clear_stat_boosts(ctx, target);
    }
    ctx->plugin_storage[k1966ClearAudit] = cleared;
    if (cleared <= 0) {
        return EffectResult::kOk;   // "若消除成功且…"——双条件前置门
    }
    // "下回合" = NextRounds 起点（本回合不算，先/后出手都从下一回合起算）。
    const int start =
        ctx->round_effect_start_round(owner, 1, EffectWindowKind::NextRounds);
    if (k417_marker_held(ctx, kHuiTiBase, owner)) {
        // 攻击增伤：ROUND_START tick 写 ws.damage_add_pct（693 直系；ws 每回合 reset
        // 后重写，窗恰 [R+1, R+1]）。
        register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, owner, 1966, 1,
                                    {owner, target, atk_pct}, &effect_set_damage_amp);
        ctx->plugin_storage[k1966AtkAudit] = atk_pct;
    }
    if (k417_marker_held(ctx, kShiTiBase, target)) {
        // 固定伤害、百分比伤害增伤：粉伤管线 AMP 节点（1469 直系，窗 [R+1, R+2)）。
        const int expire = start + 1;
        ctx->register_pink_damage_effect(
            PinkDamagePhase::AMP, owner,
            [owner, pink_pct, start, expire](BattleContext* c, int bucket_owner) {
                if (!c || bucket_owner != owner) {
                    return;
                }
                PinkDamageResolved& p = c->resolvedPink;
                if (p.actor != owner || p.final <= 0) {
                    return;
                }
                if (c->roundCount < start || c->roundCount >= expire) {
                    return;
                }
                const int added = p.final * pink_pct / 100;
                if (added <= 0) {
                    return;
                }
                p.final += added;
                c->plugin_storage[k1966PinkApplied] = added;
            });
        ctx->plugin_storage[k1966PinkAudit] = pink_pct;
    }
    return EffectResult::kOk;
}

// ═══════════════ 1967 回体标记 → 回合类免消除窗 ═════════════════════
EffectResult effect_1967_huiti_break_immune(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1967, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1967, args,
                     "1967 布局=[owner,target,回合]");
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && rounds > 0, 1967, args,
                     "需要 owner∈{0,1} 且 回合>0");
    if (!k417_marker_held(ctx, kHuiTiBase, owner)) {
        return EffectResult::kOk;   // "若自身持有回体标记则"——无标记整体不触发
    }
    const int extra = k417_is_first_mover(ctx, owner) ? 0 : 1;   // "后出手则延续1回合"
    ctx->grant_immunity(owner, ImmunityType::BREAK, coverage_all(), /*anomaly_mask=*/0,
                        /*duration_rounds=*/rounds + extra, /*source_id=*/1967,
                        /*soul_immunity=*/false, EffectScope::ON_STAGE,
                        /*counts=*/0, /*source_effect_id=*/1967);
    ctx->plugin_storage[k1967Audit] = rounds + extra;
    return EffectResult::kOk;
}

// ═══════════════ 1968 失体Ⅰ 附加/刷新 ═══════════════════════════════
EffectResult effect_1968_shiti_attach_refresh(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1968, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1968, args,
                     "1968 布局=[owner,target,附加回合,刷新回合]");
    const int target = args.int_args[1];
    const int attach_rounds = args.int_args[2];
    const int refresh_rounds = args.int_args[3];
    EFFECT_ARG_GUARD(target >= 0 && target <= 1 && attach_rounds > 0 && refresh_rounds > 0,
                     1968, args, "需要 target∈{0,1} 且 两档回合>0");
    const int mode = k417_marker_attach_or_refresh(ctx, kShiTiBase, target,
                                                   attach_rounds, refresh_rounds);
    ctx->plugin_storage[k1968ModeAudit] = mode;
    ctx->plugin_storage[k1968ExpiryAudit] =
        k417_marker_expiry(ctx, kShiTiBase, target);
    return EffectResult::kOk;
}

// ═══════════════ 1969 失体标记 → 强化保护窗 ═════════════════════════
EffectResult effect_1969_shiti_boost_protect(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1969, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1969, args,
                     "1969 布局=[owner,target,回合]");
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && rounds > 0, 1969, args,
                     "需要 owner∈{0,1} 且 回合>0");
    if (!k417_marker_held(ctx, kShiTiBase, 1 - owner)) {
        return EffectResult::kOk;   // "若对手持有失体标记则"——无标记整体不触发
    }
    const int extra = k417_is_first_mover(ctx, owner) ? 0 : 1;   // "后出手则延续1回合"
    // 双票：STAT_CLEAR 挡消除、STAT_BOOST 挡吸取/搬走（1683 直系），窗口型 counts=0。
    ctx->grant_immunity(owner, ImmunityType::STAT_CLEAR, coverage_all(), /*anomaly_mask=*/0,
                        /*duration_rounds=*/rounds + extra, /*source_id=*/1969,
                        /*soul_immunity=*/false, EffectScope::ON_STAGE,
                        /*counts=*/0, /*source_effect_id=*/1969);
    ctx->grant_immunity(owner, ImmunityType::STAT_BOOST, coverage_all(), /*anomaly_mask=*/0,
                        /*duration_rounds=*/rounds + extra, /*source_id=*/1969,
                        /*soul_immunity=*/false, EffectScope::ON_STAGE,
                        /*counts=*/0, /*source_effect_id=*/1969);
    ctx->plugin_storage[k1969Audit] = rounds + extra;
    return EffectResult::kOk;
}

// ═══════════════ 1970 回体Ⅰ 附加/刷新 ═══════════════════════════════
EffectResult effect_1970_huiti_attach_refresh(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1970, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1970, args,
                     "1970 布局=[owner,target,附加回合,刷新回合]");
    const int owner = args.int_args[0];
    const int attach_rounds = args.int_args[2];
    const int refresh_rounds = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && attach_rounds > 0 && refresh_rounds > 0,
                     1970, args, "需要 owner∈{0,1} 且 两档回合>0");
    const int mode = k417_marker_attach_or_refresh(ctx, kHuiTiBase, owner,
                                                   attach_rounds, refresh_rounds);
    ctx->plugin_storage[k1970ModeAudit] = mode;
    ctx->plugin_storage[k1970ExpiryAudit] =
        k417_marker_expiry(ctx, kHuiTiBase, owner);
    return EffectResult::kOk;
}

// ═══════════════ 1971 未击败 → 解放双标记 + 锁切 + 先制失效 ══════════
// 先制失效 tick（1994 nullify_tick 直系）：窗内每回合 FMR 时点重写 ws。
EffectResult k417_1971_nullify_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int target = args.int_args[0];
    if (target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    ctx->ws.priority_nullified[target] = true;
    ctx->plugin_storage[k1971NullifyAudit] = ctx->roundCount;
    return EffectResult::kOk;
}

// AFTER_ACTION 延迟判定（1307 判据："击败与否在伤害落地后可知"；hp<=0 = 击败）。
EffectResult k417_1971_deferred(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1971, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 1971, args,
                     "1971 延迟节点布局=[owner,target]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && target != owner,
                     1971, args, "需要 owner/target ∈ {0,1} 且不同侧");
    if (ctx->getPet(target).hp <= 0) {
        return EffectResult::kOk;   // 击败了对手 → 不解放
    }
    int mask = 0;
    if (k417_marker_release(ctx, kHuiTiBase, owner)) {
        mask |= 1;   // 解放自身回体Ⅰ
    }
    if (k417_marker_release(ctx, kShiTiBase, target)) {
        mask |= 2;   // 解放对手失体Ⅰ
    }
    ctx->plugin_storage[k1971ReleaseAudit] = mask;
    if (mask == 0) {
        return EffectResult::kOk;   // "解放成功任意一项则"——双失败无后缀
    }
    // 锁切：下回合单回合窗（1994 直系；NextRounds 起点 → 窗恰 [R+1, R+1]）。
    const int lock_start =
        ctx->round_effect_start_round(owner, 1, EffectWindowKind::NextRounds);
    ctx->rule_center_.grant_switch_lock(owner, /*source_effect_id=*/1971, target,
                                        /*rounds=*/1, lock_start, EffectScope::ON_STAGE,
                                        ctx->round_effect_valid_id[owner]);
    // 先制失效：下回合 MOVE_RIGHT tick（NextRounds 单回合窗）。
    register_next_rounds_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, owner, 1971, 1,
                                {target}, &k417_1971_nullify_tick);
    ctx->plugin_storage[k1971LockAudit] = 1;
    return EffectResult::kOk;
}

EffectResult effect_1971_release_lock_nullify(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1971, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 1971, args,
                     "1971 布局=[owner,target]（零参载体，引擎注入双方）");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && target != owner,
                     1971, args, "需要 owner/target ∈ {0,1} 且不同侧");
    // "未击败"须伤害落地后判 → 延挂 AFTER_ACTION 双桶（57 形：恰好一桶/回合，
    // K416 519 同款；1307 的内核 case 路由等价物，src/ 冻结下的插件侧改造）。
    for (State st : {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_round_effect(ctx, st, owner, 1971, 1, {owner, target},
                              &k417_1971_deferred);
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表）──
SKILL_EFFECT(1966, effect_1966_clear_marker_damage_window)
SKILL_EFFECT(1967, effect_1967_huiti_break_immune)
SKILL_EFFECT(1968, effect_1968_shiti_attach_refresh)
SKILL_EFFECT(1969, effect_1969_shiti_boost_protect)
SKILL_EFFECT(1970, effect_1970_huiti_attach_refresh)
SKILL_EFFECT(1971, effect_1971_release_lock_nullify)
