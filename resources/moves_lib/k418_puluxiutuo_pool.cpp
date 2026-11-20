// K418 逐宠定制池三批 —— 普鲁修托 4489 回能Ⅰ/失能Ⅰ标记族（4 宏落地 + 2 内核门槛记档）
// （看板 K418，a01-k418 2026-11-06，worktree ~/.agent-workspaces/workspaces/sbs-local/k418，
//  分支 feat/k418；场景号段 7960-7969，场景 test/scenario/scenario_7960_puluxiutuo_pool.cpp。
//  选载体依据：coverage_recompute 2026-11-06 05:14 快照（链级可用率 92.81% 非空链口径，
//  主库 build/registry.json 现查 moves=1422——未含 K417 六宏的旧 dump，并库链上 1428）+
//  逐宠聚合（monster_learnable_moves join moves.side_effect）：4489 普鲁修托 16 技 /
//  非空链 11 / 链级可用 7 = 63.6%，缺口 = 散号 72 + 连号段 1940-1945 六连号（无其他缺号）；
//  1940-1945 全库 moves 引用面仅守一 36503 / 天下之赜 28265 / 咒始·光影诞 36507 三技，
//  且三技学习面独占 4489（monster_learnable_moves 全表验证）——纯连号 + 载体独占 + args
//  自洽，未被 J/K 批产覆盖。排除对照（2026-11-06 全库扫描，低覆盖<85% 且缺号含 ≥4 连号
//  共 16 只）：4853 梅赫维特 40%（缺 2371-2376 六连号但伴随 131/563/1311/2022/2340/2382/
//  2384 七个散号簇，缺口面大且未四查）；4923 莫缇 50%（2534-2537）；4554 圣谕邪魇 53%
//  （2062-2065）；4800 无极圣武 53%（2344-2348）；4888 瑞亚 54%（2433-2437）——均未四查、
//  风险高于派发主候选；4499 德布比特缺口 1966-1971 已由 K417 实现（禁碰）；4321 与 K416
//  八 id（519/1720/1476/1757/1758/1759/1760/1762）全部避开。）
//
// 落地范围裁决（1940/1941 不产码）：effect_info 1940「自身持有回能标记时先制+2」/
//   1941「对手持有失能标记时先制+2」= 条件先制族——先手权在选技阶段就定，正解 =
//   custom_skill_selection_routes DB 行（J327 数据化，2026-11-06）+ 插件标记槽门；
//   但路由表目标态断言**精确 29 行**（tools/db_patches/j4/selection_routes_29.sql
//   "COUNT(*)=29"），加行必须改该补丁 = DB 改动；本单 DB 零改 + src/ 冻结双红线 →
//   与 K417 遗留 2132/2133 同因同解，留续单（本文件标记槽键段即其消费端对接面）。
//   后果：守一 36503 [1940,1941,1922,1776] 仍缺 1940/1941、无为·魂切 36506 [36,72]
//   仍缺 72（散号，段外留 J 批）→ 4489 链级可用 7→9/11 = 81.8%。
//
// 行清单（moves 切片 side_effect.arg_count 优先，与 effect_info.args_num 6/6 一致；
// 切片逐位核对；三载体 friend_side_effect 全空）：
//   1940 [0] "自身持有回能标记时先制+2"（条件先制，不产码见上）
//        载体：守一 36503[1940,1941,1922,1776][2,2]（1940 零参切片；4489 专属）
//   1941 [0] "对手持有失能标记时先制+2"（同上）
//        载体：守一 36503（1941 零参切片；专属）
//   1942 [1] "解放自身当前持有的回能Ⅰ，解放成功则随机附加麻痹、害怕、疲惫中的{0}种
//        异常状态"
//        载体：天下之赜 28265[1344,1942,1943,843][1,1,1,1,3]（1942 切片 [1]；专属）
//   1943 [1] "解放对手当前持有的失能Ⅰ，解放成功则随机附加冰封、焚烬、感染中的{0}种
//        异常状态"
//        载体：天下之赜 28265（1943 切片 [1]；专属）
//   1944 [2] "若自身未持有回能Ⅰ则为自身附加持续{0}回合的回能Ⅰ，若自身已持有回能Ⅰ则
//        将回合数刷新至{1}回合"
//        载体：咒始·光影诞 36507[1944,1945][3,3,3,3]（1944 切片 [3,3]；专属）
//   1945 [2] "若对手未持有失能Ⅰ则为对手附加持续{0}回合的失能Ⅰ，若对手已持有失能Ⅰ则
//        将回合数刷新至{1}回合"
//        载体：咒始·光影诞 36507（1945 切片 [3,3]；专属）
//
// 动工前置四查（2026-11-06）：
//   ① 内核 grep 1940-1945：src/ include/ 零命中（rc=1），零行为硬编码；
//   ② effect_info 6/6 在列且 args_num == 切片消耗（1940|0、1941|0、1942|1、1943|1、
//     1944|2、1945|2 逐位 OK；三载体切片自洽见行清单）；custom_effect_programs
//     0 命中、custom_effect_overrides 0 命中；
//   ③ 全库并集无注册：registry moves=1422 / soul=410 均无 6 id（feat/k417 相邻段
//     1966-1971 已注册——回体Ⅰ/失体Ⅰ是另一族标记，两空间互不相干，本单不碰）；
//     boss_invalid_effects.json ids 命中 1941-1945 五条——溯源 = **魂印空间**
//     effect_icon 同号行（1944/1945 tips 带"（boss无效）"尾注、1940-1943 为不同文
//     魂印行；本族三载体 moves 行文均无 boss 尾注），SKILL_EFFECT 与 SOUL_MARK 两
//     注册空间互不相干（K416/K417 头注先例口径）；feat 分支（s28/windows-net/
//     windows-port）与兄弟 worktree（agents/s28、agents/night-fuzz）grep 零注册；
//   ④ 废弃 parser 覆盖面（EffectUnit 兜底，2026-11-05 冻结）：六条模板对
//     parse_status_inflict_primary 四 pattern（令/使+对手/自身+{）零命中——全族
//     行文无"令/使"主句（1942/1943 尾句"随机附加…异常状态"亦无概率+%形态）；
//     第二刀 179/700/1083/1257/456 条件模板零命中 → 全 6 id = NONE 安全跳过（无
//     K416 1762 型寄生）。本单注册后 clone_effect 先命中、parser 通道对该族关闭。
//
// 口径记档：
//   - 标记资源建模（§六 资源计数，K416 圣剑/处决印记 4694 段、K417 回体Ⅰ/失体Ⅰ
//     4695 段顺延）：plugin_storage[4696000+side]=回能Ⅰ到期回合、[4696010+side]=
//     失能Ⅰ到期回合（0/缺=未持有）。"持续{0}回合"= 附加回合在内 [R, R+{0}-1]
//     （K417 与 1683 grant_immunity"授予回合起算含当回合"口径对齐）；刷新 = 从当前
//     回合重算 {1}；到期残留条目不判持有（查询带 roundCount 下界）。标记为 side 级
//     槽：本族生产/消费均在场上交互，官方模板无跨场保留文案；下场不清（无依据即
//     不私加清理），与 K417 记档同款"实现细节，后续任务以本段为准对接"。
//   - 1942/1943 异常落点 = **对手**：天下之赜 28265 官方 info「解放双方的回能Ⅰ与
//     失能Ⅰ标记并令对手进入异常状态」互证——1942 解放自身回能Ⅰ、1943 解放对手
//     失能Ⅰ，两宏异常均打对手（三选池全是弱化/控制类，打自身无设计意义）。
//   - 1942/1943 解放 = 持有才擦（K417 1971 release 同式；到期残留不算持有）；
//     "解放成功则"是前置门——未持有整段跳过（无掷骰无异常）。
//   - 随机{0}种 = 三选 N **不重复**（Fisher-Yates，K416 446 同式/k398 guard 循环
//     的同槽重掷幂等陷阱同源规避）；施加走 apply_anomaly_checked chance=100
//     （官方行文无 % 占位 → 恒 100 申报；K176 口径 100% 档原语短路不耗 rand）+
//     random_anomaly_duration()（常量 2，无 rand 消费）→ 审计槽掩码 + 固定 seed
//     构成无随机精确断言（纪律：概率类禁 while 步进）。
//   - 1944/1945 = K417 1970/1968 同构直系（附加/刷新/边界全同），槽段换 4696。
//   - 天下之赜链上 1344（全属性+1 后出手翻倍，已注册）/843（下{1}回合先制+{3}，
//     已注册）不在本单范围；多回合局 843 会令普鲁修托持续先手，属预期。
//
// 存储键段（新增前已 grep 全库无撞；tools/check_storage_keys.py 门禁兜底）：
//   - 审计槽 10<id>000x 段（K416/K417 同款）：1019420001(解放位 0/1) / 1019420002
//     (选中掩码 bit0=麻痹 bit6=害怕 bit7=疲惫) / 1019420003(落地掩码) /
//     1019430001(解放位 0/1) / 1019430002(选中掩码 bit15=冰封 bit22=焚烬 bit27=感染)
//     / 1019430003(落地掩码) / 1019440001(回能Ⅰ 1=附加 2=刷新) / 1019440002(回能Ⅰ
//     到期回合) / 1019450001(失能Ⅰ 1=附加 2=刷新) / 1019450002(失能Ⅰ到期回合)
//   - plugin_storage 4696xxx 段：回能Ⅰ 4696000+side；失能Ⅰ 4696010+side（值=到期回合）
//
// 遗留记档（不产码）：
//   - 1940/1941（守一 36503，条件先制族）：见落地范围裁决；续单 = j4 补丁扩行
//     （29→31 + 目标态断言同步）+ 本文件标记槽门消费。
//     【J331 已落地 2026-11-06】续单实交 = custom_skill_selection_routes 扩 3 行
//     （29→32，含 2133；tools/db_patches/j4/selection_routes_expansion_3.sql）+
//     resources/moves_lib/j331_marker_cond_prio.cpp（1940 读回能槽 4696000+side、
//     1941 读失能槽 4696010+side，判持有口径与本文件 k418_marker_held 同式）。
//   - 1946/1947（咒终·阴阳辞 36508，4489 extra_moves.adv_move lv76；
//     1946「技能威力提升{0}%，若自身未持有回能标记则提升效果翻倍」/ 1947「附加给
//     对手{0}点固定伤害，若对手未持有失能标记则附加的固定伤害翻倍」）——派发段外
//     （1940-1945）的标记消费端，本文件槽键段即其对接面，留续单。
//   - 72（无为·魂切 36506「若没有命中对手则自己失去全部体力」，零参散号）——段外，
//     留 J 批。
//   - 回能Ⅰ/失能Ⅰ 自身被动（特性/魂印层逐回合收益，若有）不在 effect_info 家族内，
//     属 K 系魂印/特性任务空间，本单不产码。

#include "common.h"

#include <vector>

namespace {

// ── 审计槽 ──────────────────────────────────────────────────────────────
constexpr int k1942ReleaseAudit = 1019420001;
constexpr int k1942PickAudit    = 1019420002;
constexpr int k1942LandAudit    = 1019420003;
constexpr int k1943ReleaseAudit = 1019430001;
constexpr int k1943PickAudit    = 1019430002;
constexpr int k1943LandAudit    = 1019430003;
constexpr int k1944ModeAudit    = 1019440001;
constexpr int k1944ExpiryAudit  = 1019440002;
constexpr int k1945ModeAudit    = 1019450001;
constexpr int k1945ExpiryAudit  = 1019450002;

// ── 功能槽（键段声明见文件头）───────────────────────────────────────────
constexpr int kHuiNengBase = 4696000;   // +side：回能Ⅰ到期回合（0/缺=未持有）
constexpr int kShiNengBase = 4696010;   // +side：失能Ⅰ到期回合（0/缺=未持有）

// 标记到期回合（0 = 未持有/未写入）。
int k418_marker_expiry(const BattleContext* ctx, int base, int side) {
    if (!ctx || side < 0 || side > 1) {
        return 0;
    }
    const auto it = ctx->plugin_storage.find(base + side);
    return it == ctx->plugin_storage.end() ? 0 : any_int_or(it->second, 0);
}

// 标记持有判定：写入过且未到期（到期残留条目不判持有）。
bool k418_marker_held(const BattleContext* ctx, int base, int side) {
    if (!ctx) {
        return false;
    }
    const int expiry = k418_marker_expiry(ctx, base, side);
    return expiry > 0 && expiry >= ctx->roundCount;
}

// 附加/刷新（返回 1=附加 2=刷新；"持续 N 回合"= 附加回合在内，到期回合 = R+N-1）。
int k418_marker_attach_or_refresh(BattleContext* ctx, int base, int side,
                                  int attach_rounds, int refresh_rounds) {
    const int now = ctx->roundCount;
    const bool held = k418_marker_held(ctx, base, side);
    const int expiry = now + (held ? refresh_rounds : attach_rounds) - 1;
    ctx->plugin_storage[base + side] = expiry;
    return held ? 2 : 1;
}

// 解放：持有才擦（返回是否真的解放了一项）。
bool k418_marker_release(BattleContext* ctx, int base, int side) {
    if (!k418_marker_held(ctx, base, side)) {
        return false;
    }
    ctx->plugin_storage.erase(base + side);
    return true;
}

// 解放成功后的异常施加（官方行文无 % 占位 → 恒 100 申报；K176 100% 档短路口径）。
// 返回落地掩码。trio 必须恰好 3 个合法异常码（本族两组官方三选池）。
int k418_release_anomalies(BattleContext* ctx, int owner, int target, int kinds,
                           const int trio[3], int pick_audit, int land_audit) {
    // 随机 {0} 种（不重复，Fisher-Yates，K416 446 同式）——选中掩码审计 +
    // 固定 seed = 无随机精确断言（场景纪律：概率类禁 while 步进）。
    int idx[3] = {0, 1, 2};
    int picked = 0;
    int chosen[3] = {-1, -1, -1};
    const int n = kinds < 3 ? kinds : 3;
    for (int i = 0; i < n; ++i) {
        const int j = i + (sbs_rand() % (3 - i));
        const int t = idx[i];
        idx[i] = idx[j];
        idx[j] = t;
        chosen[i] = trio[idx[i]];
        picked |= (1 << chosen[i]);   // 三选池最大码 27，int 位掩码安全
    }
    ctx->plugin_storage[pick_audit] = picked;
    int landed = 0;
    if (g_core && g_core->apply_anomaly_checked) {
        for (int i = 0; i < n; ++i) {
            const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
                ctx, target, chosen[i], random_anomaly_duration(), owner, 100,
                ChanceSource::Skill);
            if (r == ApplyAnomalyResult::SUCCESS
                || r == ApplyAnomalyResult::REPLACED_EXISTING
                || r == ApplyAnomalyResult::DURATION_EXTENDED) {
                landed |= (1 << chosen[i]);
            }
        }
    }
    ctx->plugin_storage[land_audit] = landed;
    return landed;
}

// ═══════════════ 1942 解放自身回能Ⅰ → 对手随机异常 ═══════════════════
EffectResult effect_1942_release_huineng_anomaly(BattleContext* ctx,
                                                 const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1942, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1942, args,
                     "1942 布局=[owner,target,随机附加种数]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int kinds = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && target != owner,
                     1942, args, "需要 owner/target ∈ {0,1} 且不同侧");
    EFFECT_ARG_GUARD(kinds >= 0 && kinds <= 3, 1942, args, "需要 种数∈[0,3]");
    const bool released = k418_marker_release(ctx, kHuiNengBase, owner);
    ctx->plugin_storage[k1942ReleaseAudit] = released ? 1 : 0;
    if (!released) {
        return EffectResult::kOk;   // "解放成功则"——未持有整段跳过
    }
    // 异常落点 = 对手（28265 官方 info「…并令对手进入异常状态」互证，见头注）。
    static const int kTrio[3] = {static_cast<int>(AbnormalStatusId::Paralysis),   // 麻痹 0
                                 static_cast<int>(AbnormalStatusId::Fear),        // 害怕 6
                                 static_cast<int>(AbnormalStatusId::Fatigue)};    // 疲惫 7
    k418_release_anomalies(ctx, owner, target, kinds, kTrio,
                           k1942PickAudit, k1942LandAudit);
    return EffectResult::kOk;
}

// ═══════════════ 1943 解放对手失能Ⅰ → 对手随机异常 ═══════════════════
EffectResult effect_1943_release_shineng_anomaly(BattleContext* ctx,
                                                 const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1943, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1943, args,
                     "1943 布局=[owner,target,随机附加种数]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int kinds = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && target != owner,
                     1943, args, "需要 owner/target ∈ {0,1} 且不同侧");
    EFFECT_ARG_GUARD(kinds >= 0 && kinds <= 3, 1943, args, "需要 种数∈[0,3]");
    const bool released = k418_marker_release(ctx, kShiNengBase, target);
    ctx->plugin_storage[k1943ReleaseAudit] = released ? 1 : 0;
    if (!released) {
        return EffectResult::kOk;   // "解放成功则"——未持有整段跳过
    }
    static const int kTrio[3] = {static_cast<int>(AbnormalStatusId::Icebound),     // 冰封 15
                                 static_cast<int>(AbnormalStatusId::Incinerate),   // 焚烬 22
                                 static_cast<int>(AbnormalStatusId::Infection)};   // 感染 27
    k418_release_anomalies(ctx, owner, target, kinds, kTrio,
                           k1943PickAudit, k1943LandAudit);
    return EffectResult::kOk;
}

// ═══════════════ 1944 回能Ⅰ 附加/刷新 ═══════════════════════════════
EffectResult effect_1944_huineng_attach_refresh(BattleContext* ctx,
                                                const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1944, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1944, args,
                     "1944 布局=[owner,target,附加回合,刷新回合]");
    const int owner = args.int_args[0];
    const int attach_rounds = args.int_args[2];
    const int refresh_rounds = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && attach_rounds > 0 && refresh_rounds > 0,
                     1944, args, "需要 owner∈{0,1} 且 两档回合>0");
    const int mode = k418_marker_attach_or_refresh(ctx, kHuiNengBase, owner,
                                                   attach_rounds, refresh_rounds);
    ctx->plugin_storage[k1944ModeAudit] = mode;
    ctx->plugin_storage[k1944ExpiryAudit] =
        k418_marker_expiry(ctx, kHuiNengBase, owner);
    return EffectResult::kOk;
}

// ═══════════════ 1945 失能Ⅰ 附加/刷新 ═══════════════════════════════
EffectResult effect_1945_shineng_attach_refresh(BattleContext* ctx,
                                                const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1945, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1945, args,
                     "1945 布局=[owner,target,附加回合,刷新回合]");
    const int target = args.int_args[1];
    const int attach_rounds = args.int_args[2];
    const int refresh_rounds = args.int_args[3];
    EFFECT_ARG_GUARD(target >= 0 && target <= 1 && attach_rounds > 0 && refresh_rounds > 0,
                     1945, args, "需要 target∈{0,1} 且 两档回合>0");
    const int mode = k418_marker_attach_or_refresh(ctx, kShiNengBase, target,
                                                   attach_rounds, refresh_rounds);
    ctx->plugin_storage[k1945ModeAudit] = mode;
    ctx->plugin_storage[k1945ExpiryAudit] =
        k418_marker_expiry(ctx, kShiNengBase, target);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表）──
SKILL_EFFECT(1942, effect_1942_release_huineng_anomaly)
SKILL_EFFECT(1943, effect_1943_release_shineng_anomaly)
SKILL_EFFECT(1944, effect_1944_huineng_attach_refresh)
SKILL_EFFECT(1945, effect_1945_shineng_attach_refresh)
