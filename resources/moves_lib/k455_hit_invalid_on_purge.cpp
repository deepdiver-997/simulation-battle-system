// K455 · 回合类被消除转命中失效窗 —— 1786×6（2026-11-11，worktree ~/.agent-workspaces/workspaces/sbs-local/k455）
//
// 家族语义（effect_info 官方模板即权威）：
//   1786（args_num=2）"{0}回合内若自身回合类效果被消除则对手下{1}次使用的攻击技能
//   命中效果失效"
//
// 载体切片（unity 库 json_each 精确匹配 + side_effect.arg_count 顺序切分零剩余，
//   2026-11-11 本 worktree SQL 现查，与 11-10 只读预检逐位咬合）：
//   1786 [{0}=回合,{1}=次数]：27923 濛若沙尘[191,1235,1475,1786]=[3,1]、
//     28350 鲛鲨行狩[191,861,1786,776]=[4,1]、28435 阑珊百转[881,191,1115,1400,1786]=[4,1]、
//     29449 固化石躯[191,1259,1786,679]=[3,1]、37439 化·十方哑唤[2020,800,1786,1794]=[2,1]、
//     38446 崩灭极突刺[945,2161,1786]=[1,1]（**N=1**）。{1} 六载体恒 1——实现读行参，
//     不硬编码（1838 的 38281=[1] 首报纠错同纪律）。
//
// 实现（默认 SKILL_EFFECT 时点，无路由/无内核改动）：
//   1. 窗 = EVENT_BREAK watcher（duration={0}，once=false）：
//      检测通道 = K447 已落地"回合类效果消除"事件面——break_round_effects **成功路径**
//      emit（battle_primitives.cpp:961；NO_EFFECTS/IMMUNE 早退不发，:949/:955），
//      自然过期/切换作废不 emit（invalidate_all_round_effects 无 emit）→"被消除才触发"。
//      watcher 挂法直系 = **1886**（j_break_effect_reduce_window.cpp:104，同触发文家族
//      "{0}回合内若自身回合类效果被消除则…"：owner=self、filter ev.target==self、
//      duration={0}、once=false）；自身四清（1688 双向断回合打自己）同样命中判据
//      （文本无施动者限定，与 1389 "kind=0/1 都吃"同口径）。
//      ⚠️ **无锚**（窗本体不注册进回合类可清集合，1886 同款 watcher-only）：
//      触发面检测的是**既有**回合类效果的移除事件；给窗本体另立锚会让对手断回合的
//      SUCCESS/NO_EFFECTS 判定面多一个常驻可断物，1886 先例未做、本单不做。
//   2. 触发后果 = ③层命中效果失效票：g_core->hit_effect_invalid(target=**self**,
//      kEffectsOnly, count={1}, is_attribute_skill=false)。
//      ⚠️ **方向关键**：③层票挂"被失效方（防御方）"一侧（rule_center.h grant_hit_invalid
//      "③层挂被失效方(防御方)一侧作 source 锚"；消费点 skills.cpp:1093
//      consume_hit_invalid(1-owner)——owner=攻击方 → 查防御方票）→ 票挂 self =
//      "对手打进来的攻击命中效果失效"，恰为本模板语义；挂 opp 会变成"自己打对手被剥离"。
//      kEffectsOnly：效果失效、伤害照常（kFullNull 是 1090 族"无法造成伤害"另一档）。
//   3. 为什么不走 K454 1838 的 seal_skill（SEAL_ATTRIBUTE_HIT）：该 kind 只响应
//      **属性技能**（responds_to，rule_center.h:413）；攻击技能的命中失效无 SEAL 变体
//      ——seal_skill(attack=true, hit_invalid=true) 落 SEAL_ATTACK=技能无效+SKILL_INVALID
//      补偿（语义相反）。攻击技能命中失效的一等公民 = ③层 HIT_INVALID_ATTACK
//      （skills.cpp ②.5 用户口径"命中失效不是属性技能专用"；落地直系 1637
//      j_attr_cond_window.cpp:278）。
//   4. 补偿差分（非技能无效实证）：HIT_INVALID 执行路径（skills.cpp:1275）逐节点
//      nullify 过滤、**不 emit EVENT_SKILL_INVALID、不注册无效补偿分支**。对照载体
//      38378 [1794(2,1,50),2175,2203]：1794 = EVENT_SKILL_INVALID watcher
//      （j_invalid_react_priobserve.cpp，"被打断/miss 都算"）——真 miss/无效趟 1794
//      落地回血；1786 剥离趟效果节点整支被过滤，1794 连武装都不发生（场景 T8425）。
//
// 生命周期：
//   - 窗（watcher）：InRounds {0}——register_round 起算 current-register<rounds（R1 施放
//     {0}=N → R1..RN 活，RN+1 过期），与回合类"N回合内传 rounds 本身不+1"（2009/2321/2097
//     定档）同口径；deliver 内过期跳过 + cleanup 移除（event_center.h:451/:390）。
//     custom_effect_overrides 1786=0 行（本单 SQL 现查）→ 默认 InRounds 档。
//   - 窗 scope=ON_STAGE + watcher_valid_id[self]：自身换宠 epoch 作废（离场清窗、回场
//     需重施放再武装）——本仓普通效果下场失效建模（K454 1389 ② 2026-11-10 定案口径）。
//     ⚠️ 与 1886 的 WatcherScope::TEAM **刻意不同**（1886 落地早于定案，存量不强制迁移）。
//   - ③层票：挂 self（bearer），scope=ON_STAGE + source_slot=-1（1637 同款）→ 自身换宠清
//     （clear_on_stage 对 source_slot=-1 全槽匹配）；对手换宠不影响；**非回合类**
//     （remaining_rounds=0）→ 断回合不清（clear_round_type is_round_type 门）、不随 tick 递减
//     ——"下{1}次"无回合界，消费即耗（consume 后 counts=0 当场注销）。
//   - 覆盖键刷新：grant_hit_invalid 同 (target,1786,HIT_INVALID_ATTACK,kEffectsOnly) 刷新
//     剩余次数为 {1}、不叠加（rule_center.h:797 刷新路径）——重复触发按通道约定刷新
//     （1637/seal_skill"覆盖键刷新不叠加"同款；官方 {1} 恒 1，刷新 vs 叠加仅"未消费
//     再触发"可分辨，场景 T8423 锁刷新档）。
//   - 重施放刷新：先拆旧 watcher（kWatch 键）再注册（防双 watcher 双响应，K454 纪律）。
//
// 动工前置三查（2026-11-11 本 worktree + 主库现查）：
//   ①内核 grep 1786 零命中（src/ include/ rc=1）②effect_info 1786 args_num=2 =
//   side_effect.arg_count=2（SQL JOIN 全等）、custom_effect_overrides/programs/
//   combo_rules/selection_routes 四表 0 行、boss_invalid_effects.json 含 1786（:844
//   ——官方尾注"BOSS无效"数据面，装配期整条摘除，effect_param.h 口径无需插件特例，
//   1688 同款）、六载体 json_each 精确 + 顺序切分零剩余 ③registry moves 无 1786、
//   resources/test/examples + 全部兄弟 worktree（agents/endgame-walk/k448/k449/k456/
//   web-replay）SKILL_EFFECT(1786)/SOUL_PROGRAM(1786)/kMark1786/effect_1786 零命中
//   ——真缺口。
// 先例：1886（EVENT_BREAK 检测直系，同触发文家族）、1637（③层 grant 直系）、
//   K454 1838（命中失效语义/补偿差分方法）、1389（watcher ON_STAGE/重施放拆旧/
//   交错键纪律）、1219/31973（真实断回合技能）、36897（1974 消强 + 1155 粉伤，可剥离
//   观测面）、30916（1115 真实"下1次攻击无效"对照通道）、21944（433 回合窗重武装）。
// 场景回归：test/scenario/scenario_8420_k455.cpp（号段 8420-8429）。

#include "common.h"

namespace {

// ── 审计键段（战斗级；10^9 + 1786×10^4 效果号系，新增前全库 grep 无撞）──
// K447 布局：audit = 基址 + owner×16 + slot（owner0 占 0000..000F、owner1 占 0010..001F）。
constexpr int k1786AuditBase = 1017860000;
// 交错键（K454 纪律：多基址各自 +2*side，不得再 +1——基址相差 1 时补偿 +1 会重新同值；
//   本两基址落在审计段 0..31 之外（0101+），与 K447 布局互不侵占）：
constexpr int k1786WatchBase = 1017860101;  // +2*side：活跃 watcher id（001/003）
constexpr int k1786TrigBase  = 1017860102;  // +2*side：累计触发次数（002/004）

inline int audit_key(int owner, int slot) { return k1786AuditBase + owner * 16 + slot; }

int k1786_stor(const BattleContext* ctx, int key) {
    const auto it = ctx->plugin_storage.find(key);
    return it == ctx->plugin_storage.end() ? 0 : any_int_or(it->second);
}

// ── 1786 —— {0}回合内若自身回合类效果被消除则对手下{1}次使用的攻击技能命中效果失效 ──
// int_args = [owner, target, {0}=回合, {1}=次数]（args_num=2，loader 注入 owner/target
// 前缀 + 行参原序）。无概率行参、无掷点（确定性效果，审计槽即可精确断言）。
EffectResult effect_1786_hit_invalid_on_purge(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1786, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1786, args,
                     "1786 布局=[owner,target,回合,次数]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int count = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp,
                     1786, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0, 1786, args, "需要 回合 > 0");
    // {1} 次数：官方六载体恒 1；非正行参响亮拒（失配行参报错而非静默 0——K447 口径）。
    EFFECT_ARG_GUARD(count > 0, 1786, args, "需要 次数 > 0");
    if (!g_core || !g_core->hit_effect_invalid) {
        ctx->plugin_storage[audit_key(self, 9)] = 1;   // 跳过：缺原语槽
        return EffectResult::kOk;
    }

    // 行参审计（在位即落，供场景核"装载面真实切片"）。
    ctx->plugin_storage[audit_key(self, 0)] = rounds;
    ctx->plugin_storage[audit_key(self, 1)] = count;

    // 重施放刷新：先拆旧 watcher（摘不到 = 已过期/已作废/从未注册，安全 no-op；
    // 防双 watcher 双响应——K454 1389 同款纪律）。
    const int old_watcher = k1786_stor(ctx, k1786WatchBase + 2 * self);
    if (old_watcher > 0) {
        ctx->event_center_.remove_watcher(old_watcher);
    }

    // ③层授予（触发回调内）：方向=挂 self（防御方），对手打来的攻击在 ②.5 消费。
    // 覆盖键 (self,1786,HIT_INVALID_ATTACK,kEffectsOnly) 刷新不叠加（通道内建）。
    const auto grant_charge = [self, count](BattleContext* c) {
        if (!c || !g_core || !g_core->hit_effect_invalid) {
            return;
        }
        g_core->hit_effect_invalid(c, /*target=*/self, HitInvalidMode::kEffectsOnly,
                                   /*count=*/count, /*is_attribute_skill=*/false,
                                   /*source_id=*/1786);
        c->plugin_storage[audit_key(self, 2)] = c->roundCount;          // 最近触发轮次
        c->plugin_storage[audit_key(self, 3)] =
            c->rule_center_.hit_invalid_remaining(self, /*skill_kind=*/0);  // 授予面实证
    };

    // 窗 watcher：ON_STAGE + 当前 valid_id（自身换宠作废——离场清窗、回场需重施放）；
    // duration={0}（InRounds 口径）；once=false（窗内可重复触发，③层覆盖键自行刷新）。
    const int watcher_id = ctx->event_center_.register_watcher(
        EventType::EVENT_BREAK, self, ctx->roundCount, /*duration_rounds=*/rounds,
        /*once=*/false,
        [self, grant_charge](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != self) {
                return;   // 只认"自身回合类效果被消除"（1886 同判据）
            }
            const int tkey = k1786TrigBase + 2 * self;
            c->plugin_storage[tkey] = any_int_or(c->plugin_storage[tkey]) + 1;
            grant_charge(c);
        },
        WatcherScope::ON_STAGE, ctx->watcher_valid_id[self]);
    ctx->plugin_storage[k1786WatchBase + 2 * self] = watcher_id;
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1786, effect_1786_hit_invalid_on_purge)
