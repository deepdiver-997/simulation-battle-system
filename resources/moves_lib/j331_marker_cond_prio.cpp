// J331 · 标记门条件先制族 —— 1940/1941（回能Ⅰ/失能Ⅰ门先制+2）+ 2133（回体门
// 先制+1 / 否则支附加回体+疲惫）（3 宏落地，K417/K418 续单）
// （看板 J331，a01-j331 2026-11-06，worktree ~/.agent-workspaces/workspaces/sbs-local/j331，
//  分支 feat/j331；场景号段 7990-7999（此前已分配保留），场景
//  test/scenario/scenario_7990_marker_cond_prio.cpp。DB 侧 = j4 扩行补丁
//  tools/db_patches/j4/selection_routes_expansion_3.sql（custom_skill_selection_routes
//  29→32 行，选择期路由 plain 适配）；本文件 = 路由行的插件消费端（clone_effect
//  命中本注册，缺注册时 loadSkills fail-closed 报「clone 无实现」——部署绑定
//  「新 DB + 本 dylib」，J327 同款原子发布门）。）
//
// 行清单（官方现查 effect_info.info 全文；args_num 与载体切片逐位=0；2026-11-06）：
//   1940 [0] "自身持有回能标记时先制+2"（纯条件先制，无后缀句）
//        载体：守一 36503[1940,1941,1922,1776][2,2]（零参切片；4489 普鲁修托专属，
//        monster_learnable_moves 全表唯一）
//   1941 [0] "对手持有失能标记时先制+2"（同上）
//        载体：守一 36503（零参切片；专属）
//   2133 [0] "自身持有回体时先制+1，否则为自身附加3回合的回体并令对手疲惫"
//        载体：缘结·浮世诵 37021[699,1568,2132,2133,837][200,100,400]（零参切片；
//        4499 德布比特 sp_extra_moves learning_lv 77，全库唯一）
//        ⚠️ 勘误记档：K417 头注遗留记档只引了前半句（"2133'自身持有回体时
//        先制+1'"）——官方模板带**否则支**。本单按 effect_info 原文全量落地：
//        持回体 → 先制+1（选择期，与 J327 不变量一致：on_selected → MOVE_RIGHT
//        桶 → 先手比较前生效）；未持有 → 附加 3 回合回体 + 令对手疲惫，两支都在
//        本效果体（FIRST_MOVE_RIGHT 节点）内执行——路由后效果不再进普通分支
//        （skills.cpp push 后 continue），节点体就是全效果的唯一执行面。
//
// 动工前置四查（2026-11-06，本 worktree + 主库现查）：
//   ① 内核 grep 1940/1941/2133：src/ include/ 零命中（rc=1），零行为硬编码；
//   ② effect_info 3/3 在列且 args_num=0（与 side_effect.arg_count=0 逐位一致；
//     守一/缘结·浮世诵切片自洽见行清单）；custom_effect_programs/
//     custom_effect_overrides 零命中；
//   ③ 全库 moves_lib/soul_lib 并集无 SKILL_EFFECT(1940/1941/2133) 注册
//     （K417/K418 头注「不产码」记档与现查一致——真缺口，非撞号）；
//     boss_invalid_effects.json 的 1941 命中为魂印空间 effect_icon 同号行
//     （K418 头注已溯源，SKILL_EFFECT/SOUL_MARK 两注册空间互不相干）；
//   ④ 废弃 parser 覆盖面：三条模板无"令/使+对手/自身+{"直随花括号形态
//     （2133 否则支"令对手疲惫"无 % 与花括号占位）——本单注册后 clone_effect
//     先命中（选择期路由优先于 parser 通道）。
//
// 口径记档：
//   - 标记功能槽 = **K417/K418 建的 side 级契约，本文件是消费端**（两文件头注
//     "本文件标记槽键段即其消费端对接面"的对接落地）：
//     plugin_storage[4696000+side]=回能Ⅰ到期回合（K418 段）
//     plugin_storage[4696010+side]=失能Ⅰ到期回合（K418 段）
//     plugin_storage[4695000+side]=回体Ⅰ到期回合（K417 段）
//     判持有 = 值>0 且 >= 当前回合数（0/缺=未持有；**到期残留条目不判持有**，
//     K417/K418 同式）；本文件对回能/失能槽**只读**；对回体槽在 2133 否则支
//     **写入**（附加 3 回合 = 附加回合在内，到期回合 = R+3-1=R+2，K417 1970
//     附加口径同式；⚠️ roundCount 在选技期为本回合 **0 基**序号——R1 选取时
//     roundCount=0，到期 2 表示 roundCount 0/1/2 三次选取判持有，场景 7995-7998
//     已锁此口径）。跨 dylib 共用键已在 tools/check_storage_keys.py
//     INTENTIONAL_SHARED 备案（4695000/4696000/4696010 三基键，故意交接面）。
//   - 条件逐回合重算（不缓存首轮判定，6450 同款口径）：门条件在 FIRST_MOVE_RIGHT
//     节点体内即判即用，先制量写 ctx->ws.preemptive_level[owner]（先手比较只看
//     等级不等、相等才比速度——结构断言依据，battleFsm handle_BattleFirstMoveRight）。
//   - 先制量为模板文案定值（零参效果，无行参可消费）：1940/1941=+2、2133=+1。
//   - 2133 否则支疲惫施加 = apply_anomaly_checked 恒 100 申报（官方行文无 % 占位，
//     K176/K418 100% 档原语短路不耗 rand 口径）+ random_anomaly_duration()
//     （常量，无 rand 消费，K418 同式）→ 全效果无随机数消费，场景可做无随机
//     精确断言。
//   - 2133 否则支对**到期残留**的覆盖：未持有（含残留）→ 直接重写槽值为新到期
//     回合（R+2），残留条目无需先擦（写即覆盖，K417 attach_or_refresh 同效）。
//
// 存储键段（新增前已 grep 全库无撞；tools/check_storage_keys.py 门禁兜底）：
//   - 审计槽 10<id>000x 段（K416/K417/K418 同款）：
//     1019400001(+owner)=1940 授予的先制量（缺席=门闭）
//     1019410001(+owner)=1941 授予的先制量（缺席=门闭）
//     1021330001(+owner)=2133 分支位（1=先制支 2=否则支）
//     1021330002(+owner)=2133 否则支写入的回体到期回合
//     1021330003(+owner)=2133 否则支疲惫落地位（1=落地 0=被挡）
//   - 功能槽：见口径记档（4695/4696 段，消费 K417/K418 契约）。
//
// 先例：1956/1292（选择期条件先制体 + 审计槽，j_cond_prio3.cpp）、
//   K417 k417_marker_held / K418 k418_marker_held（槽契约源）、
//   K418 k418_release_anomalies（apply_anomaly_checked 调用式）。
// 场景回归：test/scenario/scenario_7990_marker_cond_prio.cpp（号段 7990-7999）。

#include "common.h"

namespace {

// ── 审计槽 ──────────────────────────────────────────────────────────────
constexpr int k1940Audit        = 1019400001;   // +owner：授予的先制量（缺席=门闭）
constexpr int k1941Audit        = 1019410001;   // +owner：授予的先制量（缺席=门闭）
constexpr int k2133BranchAudit  = 1021330001;   // +owner：分支位（1=先制支 2=否则支）
constexpr int k2133AttachAudit  = 1021330002;   // +owner：否则支写入的到期回合
constexpr int k2133FatigueAudit = 1021330003;   // +owner：疲惫落地位（1=落地 0=被挡）

// ── 功能槽（K417/K418 段契约，只读；回体段否则支写）────────────────────
constexpr int kHuiNengBase = 4696000;   // +side：回能Ⅰ到期回合（K418）
constexpr int kShiNengBase = 4696010;   // +side：失能Ⅰ到期回合（K418）
constexpr int kHuiTiBase   = 4695000;   // +side：回体Ⅰ到期回合（K417）

// 标记持有判定（K417/K418 同式：写入过且未到期；到期残留条目不判持有）。
bool j331_marker_held(const BattleContext* ctx, int base, int side) {
    if (!ctx || side < 0 || side > 1) {
        return false;
    }
    const auto it = ctx->plugin_storage.find(base + side);
    if (it == ctx->plugin_storage.end()) {
        return false;
    }
    const int expiry = any_int_or(it->second, 0);
    return expiry > 0 && expiry >= ctx->roundCount;
}

}  // namespace

// ── 1940 —— 自身持有回能标记时先制+2（零参；选择期路由体）──
// int_args = [owner, target]（on_selected 绑定；零参效果无行参槽）。
EffectResult effect_1940_huineng_cond_prio(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1940, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 1940, args,
                     "1940 布局=[owner,target]（on_selected 绑定）");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp,
                     1940, args, "需要 owner/target ∈ {0,1} 且互异");
    if (!j331_marker_held(ctx, kHuiNengBase, self)) {
        return EffectResult::kOk;   // 门闭：自身未持回能标记（含到期残留）→ 不加先制
    }
    ctx->ws.preemptive_level[self] += 2;
    ctx->plugin_storage[k1940Audit + self] = 2;
    return EffectResult::kOk;
}

// ── 1941 —— 对手持有失能标记时先制+2（零参；选择期路由体）──
// 条件读**对手侧**槽（kShiNengBase+target）；自身侧失能标记不构成条件。
EffectResult effect_1941_shineng_cond_prio(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1941, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 1941, args,
                     "1941 布局=[owner,target]（on_selected 绑定）");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp,
                     1941, args, "需要 owner/target ∈ {0,1} 且互异");
    if (!j331_marker_held(ctx, kShiNengBase, opp)) {
        return EffectResult::kOk;   // 门闭：对手未持失能标记 → 不加先制
    }
    ctx->ws.preemptive_level[self] += 2;
    ctx->plugin_storage[k1941Audit + self] = 2;
    return EffectResult::kOk;
}

// ── 2133 —— 自身持有回体时先制+1，否则为自身附加3回合的回体并令对手疲惫 ──
// （零参；选择期路由体=全效果唯一执行面，路由后不进普通分支。）
EffectResult effect_2133_huiti_cond_prio_or_attach(BattleContext* ctx,
                                                   const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2133, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 2133, args,
                     "2133 布局=[owner,target]（on_selected 绑定）");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp,
                     2133, args, "需要 owner/target ∈ {0,1} 且互异");
    if (j331_marker_held(ctx, kHuiTiBase, self)) {
        // 先制支：持有回体 → 先制+1（选择期，先手比较前生效）。
        ctx->ws.preemptive_level[self] += 1;
        ctx->plugin_storage[k2133BranchAudit + self] = 1;
        return EffectResult::kOk;
    }
    // 否则支：附加 3 回合回体（附加回合在内，到期回合 = R+3-1；写即覆盖到期残留）
    // + 令对手疲惫（apply_anomaly_checked 恒 100 申报，官方行文无 % 占位）。
    const int expiry = ctx->roundCount + 3 - 1;
    ctx->plugin_storage[kHuiTiBase + self] = expiry;
    ctx->plugin_storage[k2133AttachAudit + self] = expiry;
    int fatigue_landed = 0;
    if (g_core && g_core->apply_anomaly_checked) {
        const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
            ctx, opp, static_cast<int>(AbnormalStatusId::Fatigue),
            random_anomaly_duration(), self, 100, ChanceSource::Skill);
        fatigue_landed =
            (r == ApplyAnomalyResult::SUCCESS
             || r == ApplyAnomalyResult::REPLACED_EXISTING
             || r == ApplyAnomalyResult::DURATION_EXTENDED) ? 1 : 0;
    }
    ctx->plugin_storage[k2133FatigueAudit + self] = fatigue_landed;
    ctx->plugin_storage[k2133BranchAudit + self] = 2;
    return EffectResult::kOk;
}

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表）──
SKILL_EFFECT(1940, effect_1940_huineng_cond_prio)
SKILL_EFFECT(1941, effect_1941_shineng_cond_prio)
SKILL_EFFECT(2133, effect_2133_huiti_cond_prio_or_attach)
