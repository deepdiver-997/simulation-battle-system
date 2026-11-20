// K460 · 消除/失效窗两族 —— 2355×5 + 156×6（2026-11-11，worktree
// ~/.agent-workspaces/workspaces/sbs-local/k460，分支 feat/k460，基点 3efc428d；
// 场景 test/scenario/scenario_8470_k460.cpp，号段 8470-8479）
//
// ══ 家族语义（effect_info 官方模板即权威，unity 库现查 2026-11-11）══
//   2355（args_num=3）"{0}回合内对手回合类效果被消除后{1}{2}回合"
//     ——{1}{2} 为**连读切分**：{1}=异常状态码（官方渲染成绿色关键词），{2}=该状态
//     持续回合数（模板字面 "{1}[/color]{2}回合"，{2} 紧贴"回合"）。定案双证：
//     ①官方全文——亮银之靴（辛德拉缇 4843，bwiki 精灵:4843/4399 技能表）：
//       "3回合内对手回合类效果被消除后**沉默3回合**"，切片 [3,30,3] ↔ 30=沉默
//       （AbnormalStatusId::Silence）、3 回合，逐位咬合；
//     ②BWIKI 效果库独立佐证"回合类效果被消除后 → 混乱3回合"（浩域森罗切片
//       [4,10,3] ↔ 10=混乱 Confusion）。连读=**码值+回合数**两参各表一义，
//       不是数字拼接（拼接档 82/303/103/153/232 回合显然荒谬，否证记档）。
//   156（args_num=1）"{0}回合内使得对手所有能力增强效果失效"
//     ——读面失效不真写（看板口径，同 1804 家族"视为"读面手法）：
//       ①不 emit 消除/吸取事件（EVENT_STAT_REMOVED 不发、消除类响应不触发）；
//       ②不改本体存储（ctx->ability_levels 原值保留，窗过期强化自动回来）；
//       ③只压**对手正等级的读面**（ws.view_levels 正槽归零——伤害/命中/速度公式
//         的读取源，battleFsm.cpp:559 每回合从本体重基；负槽不碰="能力增强失效"
//         只失效增强、弱化照常）。
//
// ══ 载体切片（unity 库 SQL 现查 + side_effect.arg_count 顺序切分零剩余，
//   2026-11-11 本 worktree 现查；实现读行参不硬编码，场景 T8470-pre 以实际
//   加载器 load_skill_effects 逐载体锁定）══
//   2355 [{0}=窗回合,{1}=异常码,{2}=异常回合]（5 载体）：
//     29170 飞白渲襟 [191,2312,1469,2355,843] → **[3,8,2]**（月照星魂：被消除后
//       睡眠2回合，官方 info"对手受到的禁锢"；8=Sleep）、
//     29201 亮银之靴 [1924,1449,2355] → **[3,30,3]**（辛德拉缇：沉默3回合，官方
//       全文互证；30=Silence）、
//     29211 浩域森罗 [2190,1856,2068,1695,2355,619] → **[4,10,3]**（10=Confusion）、
//     29362 叛逆简论 [191,469,2007,2355] → **[4,15,3]**（15=Icebound 冰封）、
//     38228 赤绘 [1849,2068,2073,2355,2391] → **[3,23,2]**（23=Curse 诅咒）。
//   156 [{0}=回合]（6 载体）：16770 混沌创世决 [521,156]→**[4]**、
//     21425 禁制领域 [156]→**[5]**（官方 info"5回合内，能力增强效果失效了"互证）、
//     28105 地之禁锢 [156]→**[5]**、28576 天脉轮转 [156,1102]→**[3]**、
//     28883 时之弦 [433,156]→**[3]**、29498 四象帷 [191,463,156,1670]→**[3]**。
//
// ══ 实现口径 ══
//   2355（默认 SKILL_EFFECT 时点，无路由/无内核改动）：
//   1. 窗 = EVENT_BREAK watcher（duration={0}，once=false）：检测通道 = K447 已落地
//      "回合类效果消除"事件面——break_round_effects **成功路径** emit
//      （battle_primitives.cpp:961；NO_EFFECTS/IMMUNE 早退不发；自然过期/切换作废
//      不 emit →"被消除才触发"）。watcher 挂法直系 = K455 1786
//      （resources/moves_lib/k455_hit_invalid_on_purge.cpp：ON_STAGE+valid_id、
//      InRounds delta<rounds、once=false、重施放先拆旧）；与 1786 唯二差异=
//      过滤面（ev.target==**opp**：只认"对手回合类效果被消除"，模板主语在对手，
//      1786 是"自身…被消除"）+ 触发后果（施加异常 vs ③层票）。
//      施动者不限（文本被动语态无施动者限定——1786 同口径：对手断自己、第三方断
//      都算，凡 ev.target==opp 即触发）。
//      ⚠️ **无锚**（窗本体不注册进回合类可清集合，1786/1886 同款 watcher-only）。
//   2. 触发后果 = g_core->apply_anomaly(ctx, opp, {1}, {2}, self)（core_api.h:41
//      公开入口；免疫/反弹/转化/抗性全在 core 侧按序处理——对手免异常/抗性抵挡
//      是施加通道的正常档位，结果码落审计槽 bailuo_skills.cpp:72 同款 5 参调用）。
//      **确定性施加**：模板无 % 行参 → 不走 apply_anomaly_checked 申报制
//      （chance 闸门/掷骰不介入；K452 checked 版仅"申报概率"族用）。
//      同种异常重触发 = 内核既定回合覆盖（battle_primitives.cpp:389 更长才覆盖，
//      否则 SUCCESS 保持）——重复触发语义由通道自带，插件不另记。
//   3. {1} 行参守卫 = is_valid_abnormal_status_id（abnormal-types.h:74）——
//      越界码响亮拒（EFFECT_ARG_GUARD），不静默施加。
//
//   156（默认 SKILL_EFFECT 时点 + NextRounds ROUND_START tick，无内核改动）：
//   1. 首跳（施放当回合立即生效，J271 1427/K456 首跳口径）：对手 6 槽**正**读面
//      归零（view_levels[opp][i]>0 → 0；负槽/零槽不碰）。掩码落审计槽。
//   2. 2..{0} 回合 = register_next_rounds_effect（ROUND_START，owner=self，
//      duration={0}-1，行参 [self,opp]）重跑归零——回合初 resetWorkspace+sync 重基
//      （battleFsm.cpp:1748）**先于** ROUND_START 桶（:1753）→ tick 归零后视图
//      本回合保持；窗过期回合无 tick → 重基恢复本体值 = "效果结束恢复语义"
//      （本体从未被写，恢复零成本）。锚语义（K456 同款）：duration>0 → isRoundEffect
//      入回合类可清集合、断回合可清、ON_STAGE 换宠 epoch 作废；重施放同
//      (source_id,effect_id) 键覆盖旧锚刷新不叠加。
//   3. ⚠️ 窗内对手**当回合后手**新获强化（stat_change 同步视图）在本 tick 之后
//      落进读面——本实现不拦截（插件侧无 stat_change 写钩子；窗内每回合首归零、
//      次回合首再归零）。官方对"失效窗内新获强化是否当回合即失效"无实测证据，
//      按插件可观测面口径记档（1767 v1"口径记待拍板"同款纪律）。载体侧实证：
//      21425 官方 info"5回合内，能力增强效果失效了"= 窗开始时已有强化失效，
//      与首跳+tick 归零逐位对应。
//
// ══ 生命周期 ══
//   - 2355 窗：watcher InRounds {0}（register_round 起算 current-register<rounds，
//     R1 施放 {0}=N → R1..RN 活，RN+1 过期，event_center.h:451/:390 过期跳过+移除）；
//     scope=ON_STAGE + watcher_valid_id[self]（自身换宠 epoch 作废——离场清窗、
//     回场需重施放再武装，K455 2026-11-10 定案口径）。异常本体 duration={2}：
//     end_round=trigger_round+{2}（abnormal_status_end_round 权威源，回合覆盖档
//     内核自带）。
//   - 156 窗：首跳（当回合）+ NextRounds tick ×({0}-1)；ON_STAGE 挂施放方（换宠
//     作废）；对手换宠不清窗（对手侧新宠等级本就从 0 起，窗继续有效——"对手所有
//     能力增强效果失效"作用在对手侧窗口期）。
//   - 重施放：2355 拆旧 watcher（防双 watcher 双响应，K454/K455 纪律）；156 同键
//     覆盖（timed_bucket 同源去重，窗口刷新不叠加）。
//
// ══ plugin_storage 审计键（10^9 + id×10^4 + owner×16 + slot，K452/K456 键式；
//   1023550/1001560 段全库 grep 零占用 2026-11-11；同基址内 owner×16 步长 > 槽位
//   上限 → owner 间无交叠；跨文件撞键由 tools/check_storage_keys.py 门禁静态扫）══
//   2355 基 1023550000：slot0={0}回显、1={1}码回显、2={2}回显、3=末次触发轮
//     （roundCount）、4=累计触发数、5=末次 apply_anomaly 结果码（ApplyAnomalyResult
//     static_cast<int>）、9=跳过原因（1=缺 apply_anomaly 原语槽）
//   2355 交错键（K454 纪律：多基址各自 +2*side）：1023550101+2*side=活跃 watcher id、
//     1023550102+2*side=累计触发数（与 slot4 同值双账，键面/槽面互证）
//   156 基 1001560000：slot0={0}回显、1=末次归零掩码（bit i=槽 i 被归零）、
//     2=末次归零轮、3=累计归零操作数、9=跳过原因（预留）
//
// ══ 动工前置三查（2026-11-11，k460 worktree + 主库 + 兄弟 worktree 现查）══
//   ①内核 grep 2355/156（裸 id 与 SKILL_EFFECT(156,/SOUL_PROGRAM(156, 精确式）
//     src/ include/ 零命中——内核无同号硬编码；②effect_info 2355 args_num=3 /
//     156 args_num=1，同文模板全表唯一（近亲 1400/1786/1886/2238/2375 均不同文），
//     11 载体顺序切分零剩余、param=[1,1,1]/[]；③registry moves 无两 id、
//     resources/ + 主检出 + k457/k458/k459 兄弟 worktree SKILL_EFFECT/SOUL_PROGRAM/
//     kMark 精确注册零命中——真缺口。
//   拆分/退役核查：custom_effect_overrides 两 id 待场景前 SQL 复核（K455 同项）；
//     无 900000 拆分迁移与故意退役记录（docs_local 全文 grep 无）。
// 先例：K455 1786（EVENT_BREAK watcher 直系）、1886（j_break_effect_reduce_window
//   同触发文家族）、K456 1369/1196（NextRounds 窗口键式+审计槽布局直系）、
//   J271 1427（首跳口径）、1767（batch_c_skills.cpp:1551 视图改写读面手法）、
//   1219/1083（真实断回合技能，场景驱动）、31973/28830（K455 同款驱动技）。
// 场景回归：test/scenario/scenario_8470_k460.cpp（号段 8470-8479）。

#include "common.h"

namespace {

// ── 审计键基址（见文件头「plugin_storage 审计键」节）──
constexpr int k2355AuditBase = 1023550000;   // 10^9 + 2355×10^4
constexpr int k2355WatchBase = 1023550101;   // +2*side：活跃 watcher id
constexpr int k2355TrigBase  = 1023550102;   // +2*side：累计触发数
constexpr int k156AuditBase  = 1001560000;   // 10^9 + 156×10^4

inline int audit_key_of(int base, int owner, int slot) { return base + owner * 16 + slot; }

int storage_int_local(BattleContext* ctx, int key, int def = 0) {
    const auto it = ctx->plugin_storage.find(key);
    if (it == ctx->plugin_storage.end()) {
        return def;
    }
    return any_int_or(it->second, def);
}

void set_storage_int_local(BattleContext* ctx, int key, int value) {
    ctx->plugin_storage[key] = value;
}

// ── 2355 —— {0}回合内对手回合类效果被消除后（令对手）{1}状态{2}回合 ──
// int_args = [owner, target, {0}=窗回合, {1}=异常码, {2}=异常回合]
//（args_num=3，loader 注入 owner/target 前缀 + 行参原序）。无概率行参——确定性施加。
EffectResult effect_2355_anomaly_on_opp_purge(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2355, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 2355, args,
                     "2355 布局=[owner,target,窗回合,异常码,异常回合]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int anomaly = args.int_args[3];
    const int dur = args.int_args[4];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp,
                     2355, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0, 2355, args, "需要 窗回合 > 0");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 2355, args,
                     "需要 {1} 为合法异常状态码");
    EFFECT_ARG_GUARD(dur > 0, 2355, args, "需要 异常回合 > 0");
    if (!g_core || !g_core->apply_anomaly) {
        set_storage_int_local(ctx, audit_key_of(k2355AuditBase, self, 9), 1);
        return EffectResult::kOk;
    }

    // 行参审计（在位即落，供场景核"装载面真实切片"）。
    set_storage_int_local(ctx, audit_key_of(k2355AuditBase, self, 0), rounds);
    set_storage_int_local(ctx, audit_key_of(k2355AuditBase, self, 1), anomaly);
    set_storage_int_local(ctx, audit_key_of(k2355AuditBase, self, 2), dur);

    // 重施放刷新：先拆旧 watcher（摘不到 = 已过期/已作废/从未注册，安全 no-op；
    // 防双 watcher 双响应——K454/K455 纪律）。
    const int old_watcher = storage_int_local(ctx, k2355WatchBase + 2 * self);
    if (old_watcher > 0) {
        ctx->event_center_.remove_watcher(old_watcher);
    }

    // 触发回调：只认"对手回合类效果被消除"（ev.target==opp；施动者不限——
    // 1786"文本无施动者限定"同口径）。后果 = 对手陷入 {1} 状态 {2} 回合
    //（确定性施加，免疫/抗性/转化/反弹档位由施加通道自理，结果码落审计槽）。
    const auto on_purge = [self, opp, anomaly, dur](BattleContext* c, const BattleEvent& ev) {
        if (!c || ev.target != opp) {
            return;
        }
        const int tkey = k2355TrigBase + 2 * self;
        set_storage_int_local(c, tkey, storage_int_local(c, tkey) + 1);
        set_storage_int_local(c, audit_key_of(k2355AuditBase, self, 4),
                              storage_int_local(c, audit_key_of(k2355AuditBase, self, 4)) + 1);
        set_storage_int_local(c, audit_key_of(k2355AuditBase, self, 3), c->roundCount);
        const ApplyAnomalyResult r =
            g_core->apply_anomaly(c, /*target=*/opp, /*anomaly_id=*/anomaly,
                                  /*duration_rounds=*/dur, /*actor=*/self);
        set_storage_int_local(c, audit_key_of(k2355AuditBase, self, 5),
                              static_cast<int>(r));
    };

    // 窗 watcher：ON_STAGE + 当前 valid_id（自身换宠作废——离场清窗、回场需重施放）；
    // duration={0}（InRounds 口径：R1..RN 活，RN+1 过期跳过+移除）；once=false
    //（窗内可重复触发，同种异常由施加通道回合覆盖档自理）。
    const int watcher_id = ctx->event_center_.register_watcher(
        EventType::EVENT_BREAK, self, ctx->roundCount, /*duration_rounds=*/rounds,
        /*once=*/false, on_purge, WatcherScope::ON_STAGE, ctx->watcher_valid_id[self]);
    set_storage_int_local(ctx, k2355WatchBase + 2 * self, watcher_id);
    return EffectResult::kOk;
}

// ── 156 归零一跳：对手 6 槽正读面归零（本体不动、无事件、负槽不碰）──
// 返回归零掩码（bit i = 槽 i 本次被归零）。
int k156_zero_boost_view(BattleContext* ctx, int self, int opp) {
    int mask = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ws.view_levels[opp][i] > 0) {
            ctx->ws.view_levels[opp][i] = 0;   // 只压读面；本体 ability_levels 不动
            mask |= (1 << i);
        }
    }
    set_storage_int_local(ctx, audit_key_of(k156AuditBase, self, 1), mask);
    set_storage_int_local(ctx, audit_key_of(k156AuditBase, self, 2), ctx->roundCount);
    set_storage_int_local(ctx, audit_key_of(k156AuditBase, self, 3),
                          storage_int_local(ctx, audit_key_of(k156AuditBase, self, 3)) + 1);
    return mask;
}

EffectResult k156_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    // tick 行参 = [self, opp]（cast 时打包，无 owner/target 前缀）
    k156_zero_boost_view(ctx, args.int_args[0], args.int_args[1]);
    return EffectResult::kOk;
}

// ── 156 —— {0}回合内使得对手所有能力增强效果失效 ──
// int_args = [owner, target, {0}=回合]（args_num=1 + 前缀）。
EffectResult effect_156_boost_invalidate_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 156, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 156, args,
                     "156 布局=[owner,target,回合]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp,
                     156, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0, 156, args, "需要 回合 > 0");

    set_storage_int_local(ctx, audit_key_of(k156AuditBase, self, 0), rounds);
    k156_zero_boost_view(ctx, self, opp);   // 首跳：施放当回合立即失效
    if (rounds > 1) {
        // 2..{0} 回合走 NextRounds 的 ROUND_START tick（同源键覆盖=重施放刷新不叠加；
        // ON_STAGE 换宠 epoch 作废；断回合可清——K456 锚语义）。
        register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, self, /*effect_id=*/156,
                                    rounds - 1, {self, opp}, &k156_tick);
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表；registry moves +2）──
SKILL_EFFECT(2355, effect_2355_anomaly_on_opp_purge)
SKILL_EFFECT(156, effect_156_boost_invalidate_window)
