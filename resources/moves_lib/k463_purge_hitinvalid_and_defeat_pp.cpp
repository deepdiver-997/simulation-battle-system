// K463 · 解除→免疫票 + 击败后场下PP降 两族 —— 2226×6 + 2027×6（2026-11-12，
// worktree ~/.agent-workspaces/workspaces/sbs-local/k463，分支 feat/k463，基点 31fe17d7；
// 场景 test/scenario/scenario_8500_k463.cpp，号段 8500-8509）
//
// ══ 家族语义（effect_info 官方模板即权威，unity 库现查 2026-11-12）══
//   2226（args_num=1）"解除双方所处的异常状态，解除成功则免疫下{0}次对手的攻击"
//   2027（args_num=1）"击败对手则对方下只精灵出战时降低所有技能{0}点PP值"
//
// ══ 载体切片（unity 库 side_effect.arg_count 优先顺序切分，12 载体全链零剩余；
//   场景 T8500-pre 以实际加载器 load_skill_effects 逐载体锁定）══
//   2226 [{0}=次数]（6 载体，{0} 全=1——实现读行参不硬编码）：
//     28866 沉云浸野 [911(1),1687(1),1785(3),2226(1)]=[1|4|4,3,120|1]、
//     28911 E·联合阵线 [48(1),1731(3),2226(1),1211(1)]=[4|4,300,1|1|300]、
//     29421 无畏印 [191(1),2226(1),1135(1),1540(2),444(0)]=[4|1|2|2,350|]、
//     38093 申驳复议 [1441(1),2226(1)]=[2|1]（⚠️ 兄弟 1441=消强→随机PP归零，
//       污染 PP/rand 面——本单精确断言不选，只进加载器切片核验）、
//     38406 一心永驻 [2226(1)]=[1]（**纯链单效果**、cat1 攻击技、must_hit=1
//       ——主载体）、
//     38450 临世圣歌 [2226(1),1688(3)]=[1|3,100,11]（prio=3，先制污染，不选）。
//   2027 [{0}=点数]（6 载体，{0}∈{2,3}）：
//     36670 灵梦芳华逝 [1837(3),973(2),897(2),2027(1)]=[100,2,1|100,22|1,50|2]
//       （1837 概率异常链、897 on-hit 50%——概率污染，不选）、
//     36872 华灵绯天咒 [1114(1),2024(1),2033(1),2027(1)]=[2|20|2|3]
//       （1114/2033 都会扣 PP——污染观测面，不选）、
//     37081 截光画册 [1120(2),1632(3),2027(1)]=[300,300|300,2,150|3]
//       （1632 pp_gate_extra_drain 扣 PP——不选）、
//     37165 魔荼蔓延 [700(1),1522(2),2027(1)]=[2|220,300|3]
//       （700 先出手降对手所有 PP——不选）、
//     37527 影中艳 [2117(2),431(0),2027(1)]=[2,22||2]（**主载体**：2117 异常转化
//       [2→22，无异常良性 no-op]、431 降门威力翻倍 [无下降良性 no-op]，链上零 rand
//       零 PP 面、monster 4671 learnable）、
//     37712 风土震荡 [928(1),806(2),526(2),2027(1)]=[1|2,2|2,300|2]
//       （928 消回合门 [无可断 no-op]、806 窗族、526 属性技粉伤窗 [对手不出属性技
//       良性]——次载体，2027=[2]）。
//
// ══ 2226 实现口径 ══
//   · 解除 = dispel_active_anomalies 原语（"效果解除路径"：直接清槽、无到期伤害、
//     无 EVENT_ANOMALY_EXPIRED，battle_primitives.h:285）**双方两侧独立调用独立
//     计数**（self/opp 各自返回解除条数；审计槽分记）。
//   · "解除成功"门 = **至少一侧有异常可解**（cured_self + cured_opp > 0；两侧都
//     无可解 → 不授票，负测 T8500.d）。631/2158 的 cured<=0 早退同门形。
//   · 免疫票 = **防御方 consume_hit_invalid 直系**（看板行指定通道；K454/K455
//     先例）：g_core->hit_effect_invalid(target=**self**, HitInvalidMode::kFullNull,
//     count={0}, is_attribute_skill=false, source_id=2226)。
//     ⚠️ 通道甄别（同文三通道对照，记档）：本仓"免疫下N次对手的攻击"已有两建模——
//     570（DAMAGE 次数免伤票：只挡伤害、效果照落）与 1293（SEAL 次数盔：拦下
//     +SKILL_INVALID 补偿，为"免疫成功则…"后句要盔结算信号才选）。本单看板行
//     指定 ③层 consume_hit_invalid 直系，且派发须验的"真 miss 趟 vs 无效趟 vs
//     免疫触发"差分只有 ③层 能做（HIT_INVALID 不 emit EVENT_SKILL_INVALID、
//     不注册无效补偿分支——skills.cpp:1275；盔/miss 趟都发 SKILL_INVALID）。
//     mode = **kFullNull**（"免疫攻击"= 白板：效果失效+伤害归 0；kEffectsOnly
//     伤害照落不成其为"免疫"——738 首用档位语义）。is_attribute_skill=false：
//     文本"攻击"不含属性技（K454 1838"属性技能命中失效"对照面）。
//   · 消费差分三腿（沿 K455 1794/1115 范式，T8500.e）：真 miss 趟（accuracy=0 且
//     must_hit=0——必中跳过整条命中率链，坑集 K7）不消费 ③层（miss 早退于
//     skills.cpp ②.5）；免疫触发趟消费 1 次（consume_hit_invalid 命中即扣）；
//     对手属性技趟不消费也不被免（consume 按 skill_kind 分流，HIT_INVALID_ATTACK
//     只响应攻击技）。
//   · 生命周期：grant_hit_invalid 挂 self（bearer/防御方）、scope=ON_STAGE +
//     source_slot=-1 → 自身换宠整方清（clear_on_stage 对 -1 全槽匹配，1637/K455
//     同款）；非回合类（remaining_rounds=0）→ 断回合不清、不随 tick 递减——
//     "下{0}次"无回合界，消费即耗（consume 后 counts=0 当场注销，K455 同款）；
//     覆盖键 (category, source_owner=self, 2226, kFullNull) 刷新不叠加（重复施放
//     刷新剩余次数为 {0}，K455 T8423 同款）。
//
// ══ 2027 实现口径 ══
//   · 击败检测 = **AFTER_ACTION 桥**（K459 2075 同款：击败与否须伤害落地后可知 →
//     SKILL_EFFECT 时点只挂 AFTER_ACTION 桥、双注册 FIRST/SECOND、行动方那条跑
//     ——execute_registered_actions(robotId) 只跑本方桶，每回合恰一条执行；CE
//     duration=1 + Effect lr=0 = 一次性动作在途节点，isActionOneShot 不计回合类、
//     不被断回合）。桥内判据 = 对手在场宠 hp<=0（582/1049/2075 同判据）→ 击败。
//   · 自身存活门 = 桥内 armed.hp<=0 早退（官方「击败」=未死亡精灵可触发的时点，
//     K453 2456 T8413/T8410 同款口径；击败对手但同波被反噬击倒 → 不立票）。
//     ⚠️ 可达性记档（2026-11-13 续作探针实测，场景 T8500.o 双态覆盖）：反馈类
//    致死（855 挂 TAKE_DAMAGE）在攻击伤害状态结束的统一 drain 投递——**早于
//     AFTER_ACTION 桶**。反馈量 ≥ 施放者当前血 → 桥时点已 0 血 → 门拦下不立票
//     （T8500.o）；反馈量 < 当前血 → 带伤存活 → 票照立、下只出战照常消费
//     （T8500.j/.n 存活档）。TAKE_DAMAGE 载荷 amount=实际伤害（超杀钳到目标
//     剩余血），构造反馈量时按此折算。
//   · 后果 = **待命票**（per-pet on_stage_storage，K453 2456 per-pet 票先例）：
//     pending={0} + 武装槽快照写入施放宠 on_stage_storage——下场即清
//     （battleFsm.cpp:607 on_stage_storage.clear()，"普通效果下场失效"统一建模，
//     K453 2026-11-10 定案口径；死亡下场同清，官方无"下场保留"证据）。
//   · 消费 = **EVENT_ENTER_STAGE watcher**（perform_switch 统一漏斗：主动中切与
//     死亡换宠都 emit，battleFsm.cpp:630——1779/855 同款挂法；ev.actor==opp 过滤
//     = 对手侧登场）：pending>0 且施放宠仍在场（槽绑定 + hp>0 双保险，K453 同款）
//     → g_core->pp_reduce(ctx, opp, {0})（"降低所有技能{0}点PP值"全槽原语：
//     clamp≥0 钳零、pp==-1 无限槽不参与、每实际变化槽 emit EVENT_PP_REDUCED，
//     battle_primitives.cpp:1876——清 PP 必走原语，直写绕过事件=私造通道禁）→
//     pending 清零（一次性消费）。
//   · watcher 重施放先摘旧（on_stage_storage 存 id，防双 watcher 双扣——K454/K455
//     纪律）；scope=ON_STAGE + 当前 epoch（施放宠换宠作废；死亡下场后待命票随
//     on_stage_storage 清、回调槽绑定/hp 门双保险兜住）。
//   · {0}=0 = 自然 no-op（"降 0 点"不武装不报故障，K458 slots<=0 同款守卫）；
//     真实载体 {0}∈{2,3}，{0} 变档腿走 EffectParamOverride（装配期官方覆盖基建，
//     K453/K458 同款）。
//
// ══ 动工前置三查（2026-11-12，k463 worktree 现查）══
//   ① 内核 grep 2226/2027（src/ include/ 全量）零命中——无路由无硬编码；
//   ② effect_info 2226/2027 行即模板（args_num=1 与 side_effect.arg_count=1 全等）；
//     六载体 json 切片零剩余（上方清单）；boss_invalid_effects/拆分迁移：
//     grep tools/db_patches 2226/2027 零命中（无 900000 拆分与退役记录）；
//   ③ registry moves（基线）无两 id；resources/test/examples + 全部兄弟 worktree
//     （k461/k462/k464/endgame-walk/agents/web-replay）SKILL_EFFECT(2226/2027)/
//     SOUL_PROGRAM(2226/2027)/kMark2226/kMark2027 零命中——真缺口。
//   审计键段全库 grep 无撞（2026-11-12）：2226 基 1022260000、2027 基 1020270000
//   （10^9+id×10^4 口径）、on_stage_storage 私键 990202701-003（99+效果号系，
//   K453 9902456xx 同式）。
//   ⚠️ 续作独立复验（2026-11-13，a01-k463 续作会话）：①内核 grep 2226/2027 零命中
//   复跑 rc=1；②effect_info/side_effect 两行 args_num=arg_count=1 复查、12 载体
//   side_effect/side_effect_arg 切片逐位复查零剩余（38406=[2226]|[1]、37527=
//   [2117,431,2027]|[2,22,2] 等）；③本 worktree+兄弟 worktree（k462/k464/
//   endgame-walk/agents/web-replay）注册 grep 零命中复跑、审计键段 grep 零撞复跑、
//   tools/db_patches 无拆分/退役复跑——三查全部独立成立。
//
// ══ 先例 ══
//   K455 1786（③层票消费差分范式/生命周期）、K454 1838（命中失效语义/通道对照）、
//   K459 2075（AFTER_ACTION 桥击败门）、K453 2456（per-pet 票/存活门/官方击败
//   时点佐证）、1779（ENTER_STAGE watcher 挂法）、1655（pp_reduce 原语先手）、
//   K458（pp 原语族/审计键式/钳零无限槽庇保留面）、1293/570（同文异通道对照）。
// 场景回归：test/scenario/scenario_8500_k463.cpp（号段 8500-8509）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

// ── 审计键段（战斗级 plugin_storage；10^9 + id×10^4 效果号系，全库 grep 无撞）──
constexpr int k2226AuditBase = 1022260000;  // + owner*16 + slot
constexpr int k2027AuditBase = 1020270000;  // + owner*16 + slot

// 2027 per-pet on_stage_storage 私键（K453 9902456xx 同式：99+效果号系；
// 下场即清——容器生命周期即效果生命周期）。
constexpr int k2027PendingKey = 990202701;  // 待命票点数（>0 = 已武装）
constexpr int k2027SlotKey    = 990202702;  // 武装时在场槽位（槽绑定双保险）
constexpr int k2027WatchKey   = 990202703;  // 活跃 ENTER watcher id（重施放先摘旧）

inline int audit_key_of(int base, int owner, int slot) { return base + owner * 16 + slot; }

int stor_int_local(BattleContext* ctx, int key, int def = 0) {
    const auto it = ctx->plugin_storage.find(key);
    if (it == ctx->plugin_storage.end()) {
        return def;
    }
    return any_int_or(it->second, def);
}

void set_stor_int_local(BattleContext* ctx, int key, int value) {
    ctx->plugin_storage[key] = value;
}

// ════════════════ 2226 ════════════════

// ── 2226 —— 解除双方所处的异常状态，解除成功则免疫下{0}次对手的攻击 ──
// int_args = [owner, target, {0}=次数]（args_num=1，loader 注入 owner/target 前缀）。
// 无概率行参、无掷点（确定性效果，审计槽即可精确断言）。
EffectResult effect_2226_purge_both_grant_hit_invalid(BattleContext* ctx,
                                                      const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2226, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 2226, args,
                     "2226 布局=[owner,target,次数]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int count = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp,
                     2226, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(count > 0, 2226, args, "需要 次数 > 0");
    if (!g_core || !g_core->dispel_active_anomalies || !g_core->hit_effect_invalid) {
        set_stor_int_local(ctx, audit_key_of(k2226AuditBase, self, 9), 1);  // 跳过：缺原语槽
        return EffectResult::kOk;
    }

    // 解除双方异常（两侧独立调用、独立计数——"解除双方所处的异常状态"）。
    const int cured_self = g_core->dispel_active_anomalies(ctx, self);
    const int cured_opp = g_core->dispel_active_anomalies(ctx, opp);
    set_stor_int_local(ctx, audit_key_of(k2226AuditBase, self, 0), cured_self);
    set_stor_int_local(ctx, audit_key_of(k2226AuditBase, self, 1), cured_opp);
    set_stor_int_local(ctx, audit_key_of(k2226AuditBase, self, 3), count);

    // "解除成功"门：至少一侧有异常可解。两侧都无可解 → 不授票（负测腿）。
    if (cured_self + cured_opp <= 0) {
        set_stor_int_local(ctx, audit_key_of(k2226AuditBase, self, 2), 0);
        return EffectResult::kOk;
    }
    set_stor_int_local(ctx, audit_key_of(k2226AuditBase, self, 2), 1);
    set_stor_int_local(ctx, audit_key_of(k2226AuditBase, self, 6), ctx->roundCount);

    // 免疫票：挂 self（防御方）——对手打来的攻击在 skills.cpp ②.5 consume_hit_invalid
    // 消费；kFullNull 白板档（免疫=效果失效+伤害归 0）；攻击技类（文本"攻击"不含
    // 属性技）。覆盖键 (HIT_INVALID_ATTACK, self, 2226, kFullNull) 刷新不叠加。
    g_core->hit_effect_invalid(ctx, /*target=*/self, HitInvalidMode::kFullNull,
                               /*count=*/count, /*is_attribute_skill=*/false,
                               /*source_id=*/2226);
    set_stor_int_local(ctx, audit_key_of(k2226AuditBase, self, 5),
                       stor_int_local(ctx, audit_key_of(k2226AuditBase, self, 5)) + 1);
    set_stor_int_local(ctx, audit_key_of(k2226AuditBase, self, 4),
                       ctx->rule_center_.hit_invalid_remaining(self, /*skill_kind=*/0));
    return EffectResult::kOk;
}

// ════════════════ 2027 ════════════════

// ── 2027 ENTER 消费回调：对手侧登场 → 待命票在 → 全槽 PP -{0} ──
// 过滤 ev.actor==opp（perform_switch 统一漏斗，1779 同款）；消费门：待命票 >0 +
// 施放宠槽绑定 + 施放宠 hp>0（死亡下场/亡语同波不消费——K453 官方击败时点口径）。
void k463_2027_enter_consume(BattleContext* c, const BattleEvent& ev, int self, int opp) {
    ElfPet& armed = c->getPet(self);
    const auto pend_it = armed.on_stage_storage.find(k2027PendingKey);
    if (pend_it == armed.on_stage_storage.end() || any_int_or(pend_it->second) <= 0) {
        return;   // 无待命票（watcher 空转无害）
    }
    const int amount = any_int_or(pend_it->second);
    // 槽绑定 + 存活双保险（K453 同款）：武装宠被换下/被击倒 → 待命票已随
    // on_stage_storage 清（下场失效），此处再兜一层（亡语同波窗口未清形态）。
    const auto slot_it = armed.on_stage_storage.find(k2027SlotKey);
    const int armed_slot = slot_it == armed.on_stage_storage.end() ? -1
                                                                  : any_int_or(slot_it->second);
    if (armed_slot < 0 || armed_slot > 5 || c->on_stage[self] != armed_slot || armed.hp <= 0) {
        set_stor_int_local(c, audit_key_of(k2027AuditBase, self, 9), 2);  // 跳过：门闭
        return;
    }
    // 全槽 PP -{0}（原语：钳零 / 无限槽不参与 / 实际变化槽 emit EVENT_PP_REDUCED）。
    int rc = 0;
    if (g_core && g_core->pp_reduce) {
        rc = static_cast<int>(g_core->pp_reduce(c, opp, amount));
    } else {
        set_stor_int_local(c, audit_key_of(k2027AuditBase, self, 9), 1);  // 跳过：缺原语
        return;
    }
    // 一次性消费。
    armed.on_stage_storage[k2027PendingKey] = 0;
    set_stor_int_local(c, audit_key_of(k2027AuditBase, self, 2),
                       stor_int_local(c, audit_key_of(k2027AuditBase, self, 2)) + 1);
    set_stor_int_local(c, audit_key_of(k2027AuditBase, self, 5), c->roundCount);
    set_stor_int_local(c, audit_key_of(k2027AuditBase, self, 6), rc);
    set_stor_int_local(c, audit_key_of(k2027AuditBase, self, 7), amount);
}

// ── 2027 AFTER_ACTION 桥：伤害已落地，"击败与否"在此可知（K459 2075 同款）──
// 行参 [self, opp, amount]。双注册 FIRST/SECOND、行动方那条跑；duration=1 + lr=0
// = 一次性动作在途节点（isActionOneShot，不计回合类、不被断回合）。
EffectResult k463_2027_defeat_bridge(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int amount = args.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || self == opp || amount <= 0) {
        return EffectResult::kOk;
    }
    set_stor_int_local(ctx, audit_key_of(k2027AuditBase, self, 3), amount);
    // 击败门：对手在场宠 hp<=0（582/1049/2075 同判据——被本次攻击击败）。
    if (ctx->getPet(opp).hp > 0) {
        set_stor_int_local(ctx, audit_key_of(k2027AuditBase, self, 8), 0);  // 未击败
        return EffectResult::kOk;
    }
    // 自身存活门：官方「击败」=未死亡精灵可触发的时点（K453 同款）——自爆/亡语
    // 同波击倒施放者 → 不立票（per-pet 下场失效建模）。
    ElfPet& armed = ctx->getPet(self);
    if (armed.hp <= 0) {
        set_stor_int_local(ctx, audit_key_of(k2027AuditBase, self, 8), 3);  // 击败但自亡
        return EffectResult::kOk;
    }
    // 立待命票（per-pet on_stage_storage）：下场即清 = 离场清理结构性成立。
    armed.on_stage_storage[k2027PendingKey] = amount;
    armed.on_stage_storage[k2027SlotKey] = ctx->on_stage[self];
    set_stor_int_local(ctx, audit_key_of(k2027AuditBase, self, 0), amount);
    set_stor_int_local(ctx, audit_key_of(k2027AuditBase, self, 1),
                       stor_int_local(ctx, audit_key_of(k2027AuditBase, self, 1)) + 1);
    set_stor_int_local(ctx, audit_key_of(k2027AuditBase, self, 4), ctx->roundCount);
    set_stor_int_local(ctx, audit_key_of(k2027AuditBase, self, 8), 1);  // 击败立票
    return EffectResult::kOk;
}

// ── 2027 —— 击败对手则对方下只精灵出战时降低所有技能{0}点PP值 ──
// int_args = [owner, target, {0}=点数]（args_num=1）。
// 施放 = 重武装：ENTER watcher 先摘旧再注册（防双 watcher 双扣）；AFTER_ACTION
// 双桥（同 source_id 去重键覆盖——timed_bucket 同源去重，重施放刷新）。
EffectResult effect_2027_defeat_next_enter_pp_reduce(BattleContext* ctx,
                                                     const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2027, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 2027, args,
                     "2027 布局=[owner,target,点数]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int amount = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp,
                     2027, args, "需要 owner/target ∈ {0,1}");
    set_stor_int_local(ctx, audit_key_of(k2027AuditBase, self, 3), amount);
    if (amount <= 0) {
        return EffectResult::kOk;   // "降 0 点"=自然 no-op（不武装不报故障，K458 同款）
    }
    if (!g_core || !g_core->pp_reduce) {
        set_stor_int_local(ctx, audit_key_of(k2027AuditBase, self, 9), 1);
        return EffectResult::kOk;
    }

    // ENTER watcher：重施放先摘旧（摘不到 = 已随换宠 epoch 作废/从未注册，安全 no-op）。
    ElfPet& pet = ctx->getPet(self);
    const int old_watcher = any_int_or(pet.on_stage_storage[k2027WatchKey]);
    if (old_watcher > 0) {
        ctx->event_center_.remove_watcher(old_watcher);
    }
    const int watcher_id = ctx->event_center_.register_watcher(
        EventType::EVENT_ENTER_STAGE, self, ctx->roundCount,
        /*duration_rounds=*/0, /*once=*/false,
        [self, opp](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.actor != opp) {
                return;   // 只认"对手侧有精灵登场"（1779 同判据）
            }
            k463_2027_enter_consume(c, ev, self, opp);
        },
        WatcherScope::ON_STAGE,
        /*valid_id=*/ctx->watcher_valid_id[self]);   // 必传当前 epoch（K453 同款——缺省 0
                                                     // 永不匹配，首轮扣减即被清）
    pet.on_stage_storage[k2027WatchKey] = watcher_id;

    // AFTER_ACTION 双桥（K459 2075 同款：击败与否伤害落地后才可知；行动方那条跑）。
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect e(2027, 0, self, /*left_round=*/0,
                 EffectArgs(std::vector<int>{self, opp, amount}), &k463_2027_defeat_bridge);
        auto ce = std::make_unique<ContinuousEffect>(e, st, self, /*duration=*/1,
                                                     ctx->roundCount);
        ce->source_id_ = 2027;
        ctx->register_skill_effect(st, self, std::move(ce));
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表；registry moves +2）──
SKILL_EFFECT(2226, effect_2226_purge_both_grant_hit_invalid)
SKILL_EFFECT(2027, effect_2027_defeat_next_enter_pp_reduce)
