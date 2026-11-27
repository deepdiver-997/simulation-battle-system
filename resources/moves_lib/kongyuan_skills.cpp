// 空元行者 4586 —— 技能组效果（含它引用的通用模板族）
//
// 建档 2026-09-20（feat/kongyuan 线）。4586 的可学技能 = 16 把 + 1 把隐藏（37005 遗典·妄世律裁），
// 逐把的官方文本（4399 图鉴 `reference/scrape_4399/out/pets_full.jsonl`，与 `effect_info` 逐字同源）：
//
//   28530 镌戒        必中；下次攻击忽略对手双防值的30%                                  → 886
//   28531 宣读        必中；消除对手回合类效果；100%自身特攻+2；100%自身速度+2            → 180/4/4
//   28532 无边消亡    必中；4回合内，每回合对手攻/防/特攻/特防/速度/命中 -1                → 448
//   28533 烬灭书章    必中；3回合内每回合50%令对手害怕，未触发则自身下1次受攻伤-300点      → 1162
//   28534 敕界归元    必中；5回合免疫并反弹异常；100%令对手害怕，未触发则下1回合攻击100%使对手害怕；
//                    3回合内若对手用攻击技能则其后下2回合属性技能命中失效；3回合内对手用属性技能后
//                    自身回最大体力1/3并造等量百分比伤害；空元之诗·镀                        → 191/1035/1165/2099/2087
//   28535 告命诗途    必中；全属性+1并将诗章补至与先制等级相等，无诗章额外书写1篇，拥有时强化翻倍；
//                    4回合内使用技能吸取对手最大体力1/3，自身体力低于1/2翻倍，吸取后若对手体力未减少
//                    则2回合内对手无法恢复体力；下2回合自身所有技能先制+2，自身先制等级>0时提升为+3；
//                    空元之诗·柱                                                          → 2092/2093/2098/2088
//   36995 攫噬        吸取对手最大体力的1/4                                              → 1925
//   36996 浩博        无视对手攻击免疫效果                                                → 699（穿透凭证，core 已接）
//   36997 铭刻        令对手下1次使用的技能PP值归零                                        → 1775
//   36998 阅世        先制+3；若对手处于回合类效果则伤害提升100%并消除回合类效果；吸取对手能力提升
//                    状态，成功则2回合内令对手属性技能无效；空元之诗·渍                     → 1808/872/2086
//   36999 释魂咒喻    当回合击败对手则对手下只出场精灵随机进入2种异常状态                    → 826（已实现）
//   37000 逆流龙叹    50%伤害翻倍，若后出手则几率翻倍；出手时本回合若受到伤害则反馈给对手      → 1701/818
//   37001 谬法无端    附加对手当前体力40%的伤害                                           → 751
//   37002 荒宙无度    释放技能时自身每损失10%体力则此技能威力+20点                          → 1652
//   37003 道命诠魂    附加已损失体力值40%的固定伤害；附加对手已损失生命值40%的固定伤害        → 436/514
//   37004 神忏福音之章 必中；自身处于能力下降则先制+3；解除自身能力下降，成功则对手下1次攻击技能无效；
//                    未击败对手则下2回合自身先制+2；空元之诗·烙                          → 881/1115/1218/2089
//   37005 遗典·妄世律裁 必中；每有1篇诗章造成攻击伤害+20%，诗章数>2则每有1篇额外附加50点真伤；
//                    满体力时100%令对手束缚，未触发则下2回合对手所有技能先制-1；不为满体力时
//                    2回合内对手属性技能无效；空元之诗·均                                → 2094/2095/2097/2090
//
// 文件范围：**4586 技能组引用而尚未实现的那批**。其中 2092/2093/2094/2095/2097/2098 是
// 空元行者独占（或近乎独占，全库 1~5 把技能）；其余（1925/436/514/751/886/1162/1165/1218/
// 1701/1775/1808/818/872/1652/2099/448/1035）是**通用模板族**，本文件按"先有精灵需求"落地
// ——先例：德莱赫默线的 `crawford_skills.cpp` 同样在本线文件里实现了 21 条通用模板
//（见 `docs_local/agents/README.md` 克雷弗德行）。将来若要归拢到家族文件，按 id grep 即可
//（注册表按 id 全局唯一，文件位置纯属可读性问题）。
//
// ⚠️ 参数切片：`args.int_args[0]` = 施放方、`[1]` = 对手、`[2..]` = 本效果自己的那一段
//   （见 `src/db/official_data_repository.cpp` 的 build_skill_effects + `skills.cpp` 的
//   build_effect_args_for_skill）。逐条的 arg 切片已写在各自的注释里。
#include "common.h"
#include "../kongyuan_runtime.h"

namespace {

constexpr int kKongYuanMarkId = 2175;

// ── 公共小件 ──

// "现代施加的落地"判定：SUCCESS / 替换控场 / 同种延长都算触发；CONVERTED（被转成别的异常）
// 与各挡下属"未触发"。⚠️ 插件之间不互相链接 → 各 dylib/各文件各持一份同语义实现
// （同源先例：burn_trio_skills.cpp 的 anomaly_landed）。
bool anomaly_landed(ApplyAnomalyResult r) {
    return r == ApplyAnomalyResult::SUCCESS
        || r == ApplyAnomalyResult::REPLACED_EXISTING
        || r == ApplyAnomalyResult::DURATION_EXTENDED;
}

// 挡伤/弹伤是否已被抑制（bit = DamageEffectCategory 掩码）。**不在管线里**的时点桶效果
// 必须自检（管线的 walk 会按类别跳过，桶里的效果它管不到）。同源：damage_modifier.cpp 同名函数。
bool damage_guard_suppressed(BattleContext* ctx, int owner) {
    if (!ctx || owner < 0 || owner > 1) {
        return true;
    }
    constexpr int kBits = (1 << static_cast<int>(DamageEffectCategory::BLOCK))
                        | (1 << static_cast<int>(DamageEffectCategory::DETECT));
    return (ctx->getPet(owner).damage_suppress_mask & kBits) != 0;
}

// 往**技能桶**里装一条"永久"节点（duration = -1，每到此状态就执行一次）。
// 用途：效果要跨回合盯着一个 pending（如 886 的"下次攻击"、2099 的对手出手观测），
// 而技能分支节点只活本回合 → 必须由插件自建常驻节点。
// ⚠️ source_id = id → 同 id 反复注册只占一条（桶按 (source_id<<32|effect_id) 去重）。
void register_permanent_skill_node(BattleContext* ctx, State state, int owner, int id,
                                   std::vector<int> ints, EffectFn fn) {
    if (!ctx || owner < 0 || owner > 1 || !fn) {
        return;
    }
    Effect e(id, 0, owner, /*left_round=*/-1, EffectArgs(std::move(ints)), fn);
    auto ce = std::make_unique<ContinuousEffect>(e, state, owner, /*duration=*/-1,
                                                 ctx->roundCount);
    ce->source_id_ = id;
    ctx->register_skill_effect(state, owner, std::move(ce));
}

// ⚠️ **先制窗口要排到 MOVE_RIGHT 桶尾**：基值先制是**选择期**由 `on_selected` 注册的一条
//    同桶节点（`ce->source_id_ = 技能 id`，见 skills.cpp），桶内按 key 升序遍历
//    （key = source_id<<32 | lo）→ 技能 id 大的（如阅世 36998）会排在窗口节点之后，
//    于是"读当前先制等级"的窗口会读到**还没加基值**的 0（场景 104 的 E2 实测踩到）。
//    用这个**大 source_id** 作窗口节点的键 → 排在所有技能 id 之后 → 读到的等级已含基值。
//    （桶内顺序不可控是既有事实，FSM 注释也这么写；此处用键值把顺序**变成契约**。）
constexpr int kPriorityWindowTailSource = 1 << 30;

void register_priority_window(BattleContext* ctx, int owner, int id, int rounds,
                              std::vector<int> ints, EffectFn fn) {
    if (!ctx || owner < 0 || owner > 1 || rounds <= 0 || !fn) {
        return;
    }
    Effect e(id, 0, owner, rounds, EffectArgs(std::move(ints)), fn);
    const int start = ctx->round_effect_start_round(owner, rounds, EffectWindowKind::NextRounds);
    auto ce = std::make_unique<ContinuousEffect>(e, State::BATTLE_FIRST_MOVE_RIGHT, owner, rounds,
                                                 start);
    ce->source_id_ = kPriorityWindowTailSource;
    ctx->register_skill_effect(State::BATTLE_FIRST_MOVE_RIGHT, owner, std::move(ce));
}

// ── 空元行者专属：2092/2093/2094/2095/2097/2098 的公共小件 ──

// 本回合结算出来的**先制等级**（`ws.preemptive_level[owner]`）。
// 官方"自身先制等级"= 累积的先制等级（《名词解释》同类句"每有1级先制"是**可计数**的）
// → 落点就是 MOVE_RIGHT 结算后 ws 里的那个值。⚠️ 2092/2098 共用此读法，改一处即可。
int kongyuan_priority_level(BattleContext* ctx, int owner) {
    if (!ctx || owner < 0 || owner > 1) {
        return 0;
    }
    return std::max(0, ctx->ws.preemptive_level[owner]);
}

// 空妄诗章篇数（**只读**镜像；权威在魂印侧，见 kongyuan_runtime.h）
int kongyuan_poems(BattleContext* ctx, int owner) {
    return kongyuan::kongyuan_pub_poems(ctx->plugin_storage, owner);
}

}  // namespace

// ════════════════════════════════════════════════════════════════════════════
// 一、空元行者专属（2092 / 2093 / 2094 / 2095 / 2097 / 2098）
// ════════════════════════════════════════════════════════════════════════════

namespace {

// ── 2092 告命诗途：「全属性+{0}并将空妄诗章补充至与自身先制等级相等，
//      自身没有空妄诗章时额外书写{1}篇，拥有时强化效果翻倍」──
// Args（切片）= [1, 1]：{0}=1 全属性等级、{1}=1 额外书写篇数。
// 判定顺序（文本顺序，逐条对齐）：
//   ① 记 `had = 诗章 > 0`（"拥有时强化效果翻倍"与"没有诗章时额外书写"是**同一状态**的两面
//      → 必须用**同一个入口态**，否则两条口径会打架）；
//   ② 全属性 + {0} × (had ? 2 : 1)（六项含命中，走 stat_change 真原语）；
//   ③ "补充至与自身先制等级相等" = 补充至**至少**先制等级（不回落，故取 max）；
//   ④ had == false → 额外书写 {1} 篇。
// ⚠️ **跨 dylib**：诗章的写只能经发布槽下命令（`kongyuan_runtime.h`），由魂印侧消费
//    （ROUND_START + 之诗声明前各一次）——本函数**不**直接改权威状态。
// ⚠️ "自身先制等级"取值 = 本回合 MOVE_RIGHT 结算出来的 `ws.preemptive_level`
//    （见 kongyuan_priority_level 注释）；⚠️ 口径未实测（若官方是"技能自身先制值"，
//    改那一处即可）。
EffectResult effect_2092_gaoming_shitu(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int add_each = args.int_args[2];
    const int add_extra = args.int_args[3];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const bool had_poems = kongyuan_poems(ctx, owner) > 0;

    const int delta = had_poems ? add_each * 2 : add_each;   // "拥有时强化效果翻倍"
    all_stats_up(ctx, owner, delta);

    // ③ "并将空妄诗章补充至与自身先制等级相等" —— **无条件**子句（"补充"= 只补不回退；
    //    魂印侧按 target 取 max，不回落）
    kongyuan::kongyuan_cmd_poems_target(ctx->plugin_storage, owner,
                                       kongyuan_priority_level(ctx, owner));
    // ④ "自身没有空妄诗章时额外书写{1}篇" —— 只有这一条挂在"没有诗章"上
    if (!had_poems) {
        kongyuan::kongyuan_cmd_poems_add(ctx->plugin_storage, owner, add_extra);
    }
    return EffectResult::kOk;
}

// ── 2093 告命诗途：「{0}回合内使用技能吸取对手最大体力的1/{1}，自身体力低于1/{2}时效果翻倍，
//      吸取后若对手体力未减少则{3}回合内对手{4}」──
// Args（切片）= [4, 3, 2, 2, 100]：轮数=4、分母=3、低体力分母=2、封印轮数=2、{4}=100。
// ⚠️ {4} 在 effect_info 里是 **100**、不是异常 id —— 4399 同句写的是「2回合内对手**无法恢复体力**」
//    → 这不是异常，是**封回血票**（`ImmunityType::HEAL_BLOCK`，heal 入口查询）。官方模板把
//    "封回血"也编成了占位符（值 100 疑为"100% 封锁"档），故此处**不**把 {4} 当异常施加。
// 逐条：
//   ① 窗口 {0} 回合、注册在**本方 AFTER_ACTION**（"使用技能"→ 每回合自己出手后结算）；
//   ② 基础吸取 = 对手**最大**体力 / {1}（官方写死"最大体力"；⚠️ 引擎的 `drain_hp` 原语按
//      **当前**体力折算（597 家族 201 技在用）→ 本函数不复用它，改粉伤·固定档 + 按**实际减少量**
//      回血，与 894/1892 同款，口径与官文一致）；
//   ③ "自身体力低于1/{2}时效果翻倍" = hp < max/{2} → 吸取量 ×2；
//   ④ "吸取后若对手体力未减少则…" = 《名词解释》「以」：**因果关联词**，前件不成立则后件不结算
//      → 只有"对手体力一点没掉"（被封回血/被免粉/护罩全吸收等）才封回血。
EffectResult effect_2093_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int denom = args.int_args[2] > 0 ? args.int_args[2] : 3;
    const int low_denom = args.int_args[3] > 0 ? args.int_args[3] : 2;
    const int seal_rounds = args.int_args[4] > 0 ? args.int_args[4] : 2;
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || !g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    if (!used_skill_this_round(ctx, owner)) {
        return EffectResult::kOk;   // "使用技能"才结算（嗑药/被控不出手不算）
    }
    int amount = max_hp_of(ctx, opp) / denom;
    if (ctx->getPet(owner).hp * low_denom < max_hp_of(ctx, owner)) {
        amount *= 2;   // "自身体力低于1/{2}时效果翻倍"
    }
    const int before = ctx->getPet(opp).hp;
    g_core->deal_pink_damage(ctx, opp, amount, DamageKind::FIXED, owner);
    const int received = before - ctx->getPet(opp).hp;
    if (received > 0) {
        if (g_core->heal_amount) {
            g_core->heal_amount(ctx, owner, received);   // 吸取 = 按实际减少量回血
        }
        return EffectResult::kOk;   // 对手体力减少了 → 后件不触发（「以」）
    }
    // ④ 对手体力**未减少** → {3} 回合内对手无法恢复体力
    ctx->grant_immunity(opp, ImmunityType::HEAL_BLOCK, coverage_all(), /*anomaly_mask=*/0,
                        /*duration_rounds=*/seal_rounds, /*source_id=*/2093,
                        /*soul_immunity=*/false, EffectScope::ON_STAGE,
                        /*counts=*/0, /*source_effect_id=*/2093);
    return EffectResult::kOk;
}

EffectResult effect_2093_gaoming_drain_window(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 7) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    // 窗口 tick 的 args：[owner, opp, denom, low_denom, seal_rounds]
    register_round_effect(ctx, State::BATTLE_FIRST_AFTER_ACTION, owner, 2093, rounds,
                          {owner, 1 - owner, args.int_args[3], args.int_args[4], args.int_args[5]},
                          &effect_2093_tick);
    register_round_effect(ctx, State::BATTLE_SECOND_AFTER_ACTION, owner, 2093, rounds,
                          {owner, 1 - owner, args.int_args[3], args.int_args[4], args.int_args[5]},
                          &effect_2093_tick);
    return EffectResult::kOk;
}

// ── 2094 遗典·妄世律裁：「自身每有1篇空妄诗章造成的攻击伤害提升{0}%，
//      若空妄诗章的篇数高于{1}则每有1篇额外附加{2}点真实伤害」──
// Args（切片）= [20, 2, 50]：每篇 +20%、阈值 2 篇、超阈每篇 50 点真伤。
// ① 增伤走**通用增伤·加算**通道（`ws.damage_add_pct`，AMP 阶段）——文本无"额外"二字
//    （官方 L352 判据：通用增伤加法、非通用增伤乘法）；
// ② "高于{1}则每有1篇额外附加{2}点真实伤害" = 篇数 > 阈值 时，按**全部**篇数附加（不是超出部分）
//    ——文本是"每有1篇"，主语是篇数本身；⚠️ 若实测口径是"超出部分"，改这一处。
EffectResult effect_2094_poem_damage(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int per_pct = args.int_args[2];
    const int threshold = args.int_args[3];
    const int per_true = args.int_args[4];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    const int poems = kongyuan_poems(ctx, owner);
    if (poems <= 0) {
        return EffectResult::kOk;
    }
    ctx->ws.damage_add_pct[owner] += static_cast<float>(per_pct * poems);
    if (poems > threshold && per_true > 0 && g_core && g_core->deal_true_damage) {
        g_core->deal_true_damage(ctx, opp, per_true * poems, owner);
    }
    return EffectResult::kOk;
}

// ── 2095 遗典·妄世律裁：「自身为满体力时{0}%令对手{1}，
//      未触发则下{2}回合对手所有技能先制-{3}」──
// Args（切片）= [100, 28, 2, 1]：{0}=100%、{1}=28（束缚）、{2}=2 回合、{3}=1 级先制。
// "未触发"按施加原语的返回值判（免疫/弹控/转化/抗性都算没触发）。
// "下{2}回合对手所有技能先制-{3}" → NextRounds 窗口 + MOVE_RIGHT 时点写**对手**的
// preemptive_level（同族先例：burn_trio_skills.cpp 的 619）。
EffectResult effect_2095_tick_down_priority(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int opp = args.int_args[0];
    const int down = args.int_args[2];
    if (opp < 0 || opp > 1 || down <= 0) {
        return EffectResult::kOk;
    }
    ctx->ws.preemptive_level[opp] -= down;
    return EffectResult::kOk;
}

EffectResult effect_2095_full_hp_fear(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 6 || !g_core || !g_core->apply_anomaly) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int chance = args.int_args[2];
    const int status = args.int_args[3];
    const int rounds = args.int_args[4];
    const int down = args.int_args[5];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(owner).hp < max_hp_of(ctx, owner)) {
        return EffectResult::kOk;   // "自身为满体力时"
    }
    bool landed = false;
    if (roll_skill_percent(*ctx, 2095, 0, owner, opp, chance)) {
        landed = anomaly_landed(
            g_core->apply_anomaly(ctx, opp, status, random_anomaly_duration(), owner));
    }
    if (!landed && rounds > 0 && down > 0) {
        register_next_rounds_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, owner, 2095, rounds,
                                    {opp, 0, down}, &effect_2095_tick_down_priority);
    }
    return EffectResult::kOk;
}

// ── 2097 遗典·妄世律裁：「自身不为满体力时{0}回合内对手属性技能无效」──
// Args（切片）= [2]。走 `seal_skill(attribute=true, attack=false, count=0, duration=rounds)`
// ——回合窗口型封属（非"下N次"），同族先例 round_window.cpp 的 478。
EffectResult effect_2097_low_hp_seal(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->seal_skill) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(owner).hp >= max_hp_of(ctx, owner)) {
        return EffectResult::kOk;   // "自身不为满体力时"
    }
    g_core->seal_skill(ctx, owner, opp, /*effect_id=*/2097,
                       /*attribute=*/true, /*attack=*/false,
                       /*count=*/0, /*duration_rounds=*/rounds, /*penetrable=*/true,
                       /*source_slot=*/-1, EffectScope::ON_STAGE, /*hit_invalid=*/false,
                       /*chance_pct=*/100, /*consumed_when_pierced=*/false);
    return EffectResult::kOk;
}

// ── 2098 告命诗途：「下{0}回合自身所有技能先制+{1}，自身先制等级大于{2}时提升为+{3}」──
// Args（切片）= [2, 2, 0, 3]：{0}=2 回合、{1}=2、{2}=阈值 0、{3}=3。
// tick 在 MOVE_RIGHT 时点读**当时**的先制等级（此时基值先制已由 `on_selected` 写入——
// 它的桶键 source_id=0 更小、map 升序遍历先跑）→ "先制等级 > 0 时提升为 +3"。
// ⚠️ 阈值比较读的是**自身**的等级（自方桶，值已完整）；不要读对手的（两侧桶先后执行，
//    对手侧可能还没写完）。
EffectResult effect_2098_tick_priority(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    // tick args = {owner, 0, bonus, threshold, bonus_high}
    const int owner = args.int_args[0];
    const int bonus = args.int_args[2];
    const int threshold = args.int_args[3];
    const int bonus_high = args.int_args[4];
    if (owner < 0 || owner > 1 || bonus <= 0) {
        return EffectResult::kOk;
    }
    const int level = kongyuan_priority_level(ctx, owner);
    ctx->ws.preemptive_level[owner] += (level > threshold) ? bonus_high : bonus;
    return EffectResult::kOk;
}

EffectResult effect_2098_priority_window(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 6) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    // ⚠️ 用**排尾**注册（见 kPriorityWindowTailSource）：条件读的就是"当前先制等级"，
    //    必须等基值先制与其它先制来源都入账之后再判。
    register_priority_window(ctx, owner, 2098, rounds,
                             {owner, 0, args.int_args[3], args.int_args[4], args.int_args[5]},
                             &effect_2098_tick_priority);
    return EffectResult::kOk;
}

}  // namespace

// ════════════════════════════════════════════════════════════════════════════
// 二、通用模板族（4586 技能组引用）
// ════════════════════════════════════════════════════════════════════════════

namespace {

// ── 1925 攫噬：「吸取对手最大体力的1/{0}」Args=[4] ──
// ⚠️ 官方写的是**最大**体力；引擎的 `drain_hp` 原语按**当前**体力折算（597 家族 201 技在用，
//    属于引擎层的历史近似）→ 本函数不复用它：粉伤·固定档（family 口径的档位）+ 按实际
//    减少量回血（`heal_amount`，吃封回血/恢复修正）。
//    ⚠️ 台账：597 家族应当同步改到"最大体力"口径，属"通用模板族"另一条线。
EffectResult effect_1925_drain_max_hp_fraction(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int denom = args.int_args[2];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || denom <= 0) {
        return EffectResult::kOk;
    }
    const int before = ctx->getPet(opp).hp;
    g_core->deal_pink_damage(ctx, opp, max_hp_of(ctx, opp) / denom, DamageKind::FIXED, owner);
    const int received = before - ctx->getPet(opp).hp;
    if (received > 0 && g_core->heal_amount) {
        g_core->heal_amount(ctx, owner, received);
    }
    return EffectResult::kOk;
}

// ── 436 「附加已损失体力值{0}%的百分比伤害」Args=[40]（道命诠魂）──
EffectResult effect_436_pink_own_lost(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int pct = args.int_args[2];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || pct <= 0) {
        return EffectResult::kOk;
    }
    const int lost = max_hp_of(ctx, owner) - ctx->getPet(owner).hp;
    if (lost <= 0) {
        return EffectResult::kOk;
    }
    // ⚠️ 档位：文本明确写"**百分比**伤害"→ PERCENT_VALUE（值已算出，引擎不再按 max 换算）
    g_core->deal_pink_damage(ctx, opp, lost * pct / 100, DamageKind::PERCENT_VALUE, owner);
    return EffectResult::kOk;
}

// ── 514 「附加对手已损失生命值{0}%的百分比伤害」Args=[40]（道命诠魂）──
EffectResult effect_514_pink_opp_lost(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int pct = args.int_args[2];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || pct <= 0) {
        return EffectResult::kOk;
    }
    const int lost = max_hp_of(ctx, opp) - ctx->getPet(opp).hp;
    if (lost <= 0) {
        return EffectResult::kOk;
    }
    g_core->deal_pink_damage(ctx, opp, lost * pct / 100, DamageKind::PERCENT_VALUE, owner);
    return EffectResult::kOk;
}

// ── 751 「附加对手当前体力{0}%的伤害」Args=[40]（谬法无端）──
// ⚠️ 分母是**当前**体力 → 必须 PERCENT_VALUE 自己算（`PERCENT` 档是"占目标**最大**体力的
//    百分比"，引擎按 max 换算，用它就错了）。
EffectResult effect_751_pink_opp_current(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int pct = args.int_args[2];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || pct <= 0) {
        return EffectResult::kOk;
    }
    const int value = ctx->getPet(opp).hp * pct / 100;
    if (value <= 0) {
        return EffectResult::kOk;
    }
    g_core->deal_pink_damage(ctx, opp, value, DamageKind::PERCENT_VALUE, owner);
    return EffectResult::kOk;
}

// ── 1808 「若对手处于回合类效果则造成伤害提升{0}%并消除对手回合类效果」Args=[100]（阅世）──
// 顺序是承重的：**先判在不在**（两本账都要查：时点桶里的回合类 + RuleCenter 的回合型票）
// → 再断 → 再增伤。同款先例：burn_trio_skills.cpp 的 1959。
EffectResult effect_1808_round_effect_damage_up(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int pct = args.int_args[2];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || pct <= 0) {
        return EffectResult::kOk;
    }
    const bool had_round = ctx->has_round_effects(opp)
        || ctx->rule_center_.has_round_type(opp);
    if (!had_round) {
        return EffectResult::kOk;   // "若对手处于回合类效果"
    }
    if (g_core && g_core->break_round_effects) {
        g_core->break_round_effects(ctx, opp);
    }
    ctx->ws.damage_add_pct[owner] += static_cast<float>(pct);   // 通用增伤·加算
    return EffectResult::kOk;
}

// ── 872 「吸取对手能力提升状态，吸取成功则{0}回合内令对手使用的属性技能无效」Args=[2]（阅世）──
// 走 `transfer_stat_boosts` 真原语（= 转化：对手正等级整体搬到自己、对手清零；对手有
// 免消除强化 STAT_CLEAR 时返回 0 → 整条不成立）。
EffectResult effect_872_drain_boost_seal(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || rounds <= 0
        || !g_core->transfer_stat_boosts || !g_core->seal_skill) {
        return EffectResult::kOk;
    }
    if (g_core->transfer_stat_boosts(ctx, /*from=*/opp, /*to=*/owner) <= 0) {
        return EffectResult::kOk;   // 吸取失败 → 后半条不结算（"吸取成功则…"）
    }
    g_core->seal_skill(ctx, owner, opp, /*effect_id=*/872,
                       /*attribute=*/true, /*attack=*/false,
                       /*count=*/0, /*duration_rounds=*/rounds, /*penetrable=*/true,
                       /*source_slot=*/-1, EffectScope::ON_STAGE, /*hit_invalid=*/false,
                       /*chance_pct=*/100, /*consumed_when_pierced=*/false);
    return EffectResult::kOk;
}

// ── 1701 「{0}%的概率造成的伤害翻倍，若后出手则概率翻倍」Args=[50]（逆流龙叹）──
// "概率翻倍" = **概率值**翻倍（50%→100%），不是伤害倍数翻倍（用户 2026-09-13 同族口径，
// 见 chaos_sorensen_skills.cpp 的 1129）。"伤害翻倍"落通用增伤·加算 +100（文本无"额外"）。
EffectResult effect_1701_chance_double(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int chance_arg = args.int_args[2];
    if (owner < 0 || owner > 1 || chance_arg <= 0) {
        return EffectResult::kOk;
    }
    int chance = chance_arg;
    if (ctx->is_second_mover(owner)) {
        chance *= 2;   // "若后出手则概率翻倍"
    }
    if (!roll_skill_percent(*ctx, 1701, 0, owner, args.int_args[1], chance)) {
        return EffectResult::kOk;
    }
    ctx->ws.damage_add_pct[owner] += 100.0f;
    return EffectResult::kOk;
}

// ── 818 「出手时本回合若受到伤害，则将伤害反馈给对手」（逆流龙叹，Args=[]）──
// "出手时" = 自己的技能效果结算时点 → 此刻能读到的"本回合受到的伤害"就是**已经落地**的
// 上一笔（先出手时本回合还没挨打 → 弹 0，与 effect 34「将所受的伤害{0}倍反馈给对手」的
// 既有实现同口径，见 damage_modifier.cpp）。
// ⚠️ 弹伤**直写 hp**（不走伤害系统）：与 effect 34 / legacy_guards 同款——弹伤本身是
//    "把已结算过的伤害原样返回"，再进一次管线会被增/减伤二次加工。
EffectResult effect_818_reflect_taken(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const DamageSnapshot& d = ctx->resolvedDamage;
    if (d.defenderId != owner || d.attackerId < 0 || d.attackerId > 1
        || !d.isRed || d.final <= 0) {
        return EffectResult::kOk;   // 先出手 / 本回合没被打 → 没东西可弹
    }
    if (damage_guard_suppressed(ctx, owner)) {
        return EffectResult::kOk;   // 挡伤失效期：检测/反馈类一并失效（同 34 的守卫）
    }
    ctx->seerRobot[d.attackerId].elfPets[ctx->on_stage[d.attackerId]].hp -= d.final;
    return EffectResult::kOk;
}

// ── 1652 「释放技能时自身每损失{0}%的体力值则此技能威力提升{1}点」Args=[10, 20]（荒宙无度）──
// 写 `ws.skill_power_view`（`SkillView::operator+=` 自动打"变威力"标记 → 走二次重算）。
// 点数 = floor(已损失% / {0}) × {1}，即 lost×100/(max×{0}) × {1}。
EffectResult effect_1652_power_by_lost_hp(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int step_pct = args.int_args[2];
    const int per_step = args.int_args[3];
    if (owner < 0 || owner > 1 || step_pct <= 0 || per_step <= 0) {
        return EffectResult::kOk;
    }
    const int max_hp = max_hp_of(ctx, owner);
    const int lost = max_hp - ctx->getPet(owner).hp;
    if (lost <= 0) {
        return EffectResult::kOk;
    }
    const int steps = lost * 100 / (max_hp * step_pct);
    if (steps <= 0) {
        return EffectResult::kOk;
    }
    ctx->ws.skill_power_view[owner] += steps * per_step;
    return EffectResult::kOk;
}

// ── 1218 「未击败对手则下{0}回合自身先制+{1}」Args=[2, 2]（神忏福音之章）──
// "未击败"在**伤害落地之后**判 → 时点 = AFTER_ACTION（1307「未击败对手则…」的既有归位，
// 见 skills.cpp 的 effect_register_state；官方 idx=401 第 6 条把"未击败"收纳在
// "未击败对手/击败对手时"，本引擎对应 AFTER_ACTION 的 hp>0 判据，与 2105/933 同款）。
EffectResult effect_1218_not_defeated_priority(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int bonus = args.int_args[3];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || rounds <= 0 || bonus <= 0) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(opp).hp <= 0) {
        return EffectResult::kOk;   // 已击败对手 → 不算"未击败"
    }
    register_next_rounds_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, owner, 1218, rounds,
                                {owner, 0, bonus}, &effect_move_right_priority_bonus);
    return EffectResult::kOk;
}

// ── 448 「{0}回合内每回合对手{1}…」Args=[rounds, d0..d5]（无边消亡等 56 技）──
// 解码（2026-09-20，用 4399 逐技文本反查确认）：**后 6 个参数是六个能力槽的每回合等级变化量**
// ——槽序 = 引擎口径 `0攻击 1防御 2特攻 3特防 4速度 5命中`（见 battleContext.h 的能力槽注释；
// 反查样例：灵羽打击 [3,0,0,0,0,-1,0] = "3回合，每回合对手**速度**-1"✓、
// 光虎王百裂拳 [2,0,-1,0,0,0,0] = "每回合防御-1"✓、暗虎王百裂掌 [2,0,0,0,-1,0,0] =
// "每回合特防-1"✓、灵火毁灭 [3,0,-1,0,0,-1,0] = "防御-1、速度-1"✓、无边消亡 6 个 -1 =
// "全属性-1"✓）。
// 负值 = 弱化（走 `stat_drop`，查免弱/ATTACH_BAN）、正值 = 提升（走 `stat_change`，查 STAT_BOOST）、
// 0 = 不动。窗口 tick 挂 ROUND_START（"每回合"，同族先例 mistike_skills.cpp 的 1428）。
EffectResult effect_448_roundly_stats(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 8 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        const int delta = args.int_args[2 + i];
        if (delta == 0) {
            continue;
        }
        if (delta < 0) {
            if (g_core->stat_drop) {
                g_core->stat_drop(ctx, opp, i, -delta, owner);
            }
        } else if (g_core->stat_change) {
            g_core->stat_change(ctx, opp, i, delta);
        }
    }
    return EffectResult::kOk;
}

EffectResult effect_448_window(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 9) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    std::vector<int> tick{owner, 1 - owner};
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        tick.push_back(args.int_args[3 + i]);
    }
    register_round_effect(ctx, State::BATTLE_ROUND_START, owner, 448, rounds, std::move(tick),
                          &effect_448_roundly_stats);
    return EffectResult::kOk;
}

// ── 1162 「{0}回合内自身技能额外{1}%概率令对手{2}，未触发{3}则自身下{4}次受到的攻击伤害减少{5}点」──
// Args（切片）= [3, 50, 6, 6, 1, 300]（烬灭书章）：轮数=3、概率=50%、异常=6（害怕）、
// {3}=6（兜底判据用的同一异常 id）、次数=1、点数=300。
// ⚠️ 本条**原先只被通用 parser 接了一半**（parser 只识"令对手{状态}"，兜底子句丢弃）——
//    本实现接手全部两个子句（注册 SKILL_EFFECT 后 parser 那条路不再走）。
// 窗口 tick 挂 AFTER_ACTION（"每回合"自己的行动后），每轮独立掷点；未触发（免疫/弹控/抗性
// 都算没触发）→ 给自身挂一条**次数型点数减伤**（REDUCE_FLAT，同族先例 round_window.cpp 的 408）。
EffectResult effect_1162_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core || !g_core->apply_anomaly) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int chance = args.int_args[1];
    const int status = args.int_args[2];
    const int times = args.int_args[3];
    const int points = args.int_args[4];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    bool landed = false;
    if (roll_skill_percent(*ctx, 1162, 0, owner, 1 - owner, chance)) {
        landed = anomaly_landed(
            g_core->apply_anomaly(ctx, 1 - owner, status, random_anomaly_duration(), owner));
    }
    if (landed || times <= 0 || points <= 0) {
        return EffectResult::kOk;
    }
    auto remaining = std::make_shared<int>(times);
    ctx->register_damage_effect(
        DamagePhase::REDUCE_FLAT, owner, DamageEffectCategory::MITIGATE,
        [remaining, points, owner](BattleContext* c, int) {
            if (*remaining <= 0) {
                return;
            }
            DamageSnapshot& d = c->resolvedDamage;
            if (d.defenderId != owner || !d.isRed || d.final <= 0) {
                return;
            }
            d.final = std::max(0, d.final - points);
            --*remaining;
        });
    return EffectResult::kOk;
}

EffectResult effect_1162_window(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 8) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    // tick args = {owner, chance, status, times, points}（{3} 那个参数是兜底判据的同 id，跳过）
    const std::vector<int> tick{owner, args.int_args[3], args.int_args[4],
                                args.int_args[6], args.int_args[7]};
    register_round_effect(ctx, State::BATTLE_FIRST_AFTER_ACTION, owner, 1162, rounds, tick,
                          &effect_1162_tick);
    register_round_effect(ctx, State::BATTLE_SECOND_AFTER_ACTION, owner, 1162, rounds, tick,
                          &effect_1162_tick);
    return EffectResult::kOk;
}

// ── 1035 「命中后{0}%使对手{1}，未触发则下{2}回合攻击有{3}%概率使对手{4}」──
// Args（切片）= [100, 6, 1, 100, 6]（敕界归元）：即时 100% → 6（害怕）；未触发则下 1 回合
// 自身的**攻击**有 100% 概率令对手 6（害怕）。
// ⚠️ 同样是从"parser 只接一半"接手（parser 只识即时那句）。
// 兜底窗口 tick：每轮自己的行动后，若本回合用的是**攻击技能**则按 {3}% 再掷一次。
EffectResult effect_1035_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core || !g_core->apply_anomaly) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int chance = args.int_args[1];
    const int status = args.int_args[2];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (!executed_skill_this_round(ctx, owner) || !executed_skill_is_attack(ctx, owner)) {
        return EffectResult::kOk;   // "下{N}回合**攻击**有…"
    }
    if (roll_skill_percent(*ctx, 1035, 1, owner, 1 - owner, chance)) {
        g_core->apply_anomaly(ctx, 1 - owner, status, random_anomaly_duration(), owner);
    }
    return EffectResult::kOk;
}

EffectResult effect_1035_hit_fear(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 7 || !g_core || !g_core->apply_anomaly) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int chance = args.int_args[2];
    const int status = args.int_args[3];
    const int rounds = args.int_args[4];
    const int fallback_chance = args.int_args[5];
    const int fallback_status = args.int_args[6];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    bool landed = false;
    if (roll_skill_percent(*ctx, 1035, 0, owner, 1 - owner, chance)) {
        landed = anomaly_landed(
            g_core->apply_anomaly(ctx, 1 - owner, status, random_anomaly_duration(), owner));
    }
    if (landed || rounds <= 0) {
        return EffectResult::kOk;
    }
    const std::vector<int> tick{owner, fallback_chance, fallback_status};
    register_round_effect(ctx, State::BATTLE_FIRST_AFTER_ACTION, owner, 1035, rounds, tick,
                          &effect_1035_tick);
    register_round_effect(ctx, State::BATTLE_SECOND_AFTER_ACTION, owner, 1035, rounds, tick,
                          &effect_1035_tick);
    return EffectResult::kOk;
}

// ── 1165 「{0}回合内若对手使用攻击技能，则使用攻击技能后的下{1}回合属性技能命中效果失效」──
// Args（切片）= [3, 2]（敕界归元）：观测窗口 3 回合、封印窗口 2 回合。
// 两段式（同族"检测 → 应用"惯用分法）：
//   ① **检测**：窗口 tick 挂 ROUND_END（此刻双方都已出手，`roundChoice` 仍是本回合的选择）
//      → 对手本回合用了**攻击**技能就把 pending 置成 {1}（覆盖刷新）；
//   ② **应用**：ROUND_START tick 读 pending，>0 则给对手挂 1 回合 `SEAL_ATTRIBUTE_HIT`
//      （属性技能**命中效果失效**——照常命中、只是效果不注册，且**不**走 SKILL_INVALID 补偿），
//      并递减 pending。
// ⚠️ 为什么应用要挪到 ROUND_START：`seal_skill` 的窗口以 `ctx->roundCount` 为锚，"使用攻击
//    技能后的**下**N回合"要在**下一回合**生效——当回合就授的话整个当回合都会连带失效。
EffectResult effect_1165_detect_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int seal_rounds = args.int_args[2];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int opp = 1 - owner;
    if (executed_skill_this_round(ctx, opp) && executed_skill_is_attack(ctx, opp)) {
        kongyuan::kongyuan_slot(ctx->plugin_storage, 1165000 + owner) = std::max(1, seal_rounds);
    }
    return EffectResult::kOk;
}

EffectResult effect_1165_apply_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core || !g_core->seal_skill) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    int& pending = kongyuan::kongyuan_slot(ctx->plugin_storage, 1165000 + owner);
    if (pending <= 0) {
        return EffectResult::kOk;
    }
    --pending;
    const int opp = 1 - owner;
    g_core->seal_skill(ctx, owner, opp, /*effect_id=*/1165,
                       /*attribute=*/true, /*attack=*/false,
                       /*count=*/0, /*duration_rounds=*/1, /*penetrable=*/true,
                       /*source_slot=*/-1, EffectScope::ON_STAGE, /*hit_invalid=*/true,
                       /*chance_pct=*/100, /*consumed_when_pierced=*/false);
    return EffectResult::kOk;
}

EffectResult effect_1165_window(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    const int seal_rounds = args.int_args[3];
    if (owner < 0 || owner > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    register_round_effect(ctx, State::BATTLE_ROUND_END, owner, 1165, rounds,
                          {owner, 0, seal_rounds}, &effect_1165_detect_tick);
    // 应用窗口要罩住"检测窗口 + 封印窗口"（最后一次检测之后的 {1} 回合还要生效）
    register_round_effect(ctx, State::BATTLE_ROUND_START, owner, 1165, rounds + seal_rounds,
                          {owner, 0}, &effect_1165_apply_tick);
    return EffectResult::kOk;
}

// ── 2099 「{0}回合内对手使用属性技能后，自身恢复最大体力的1/{1}并造成等量百分比伤害」──
// Args（切片）= [3, 3]（敕界归元）：窗口 3 回合、分母 3。
// 触发 = 对手**使用属性技能后** → tick 注册在**对手桶**的 AFTER_ACTION（对手出手后即时结算，
// 与"使用属性技能后"同时点；插件把 CE 注册到对手 owner 的桶是既有能力，
// `register_skill_effect` 不校验归属）。
// "并造成等量百分比伤害"：等量 = 与**实际恢复量**相等（走 heal 原语拿实际值，吃封回血/恢复%修正）；
// 伤害走 PERCENT_VALUE（值已算好）。
EffectResult effect_2099_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core || !g_core->heal_amount
        || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];       // 受益方（= 属性技能的**承受方**）
    const int denom = args.int_args[1] > 0 ? args.int_args[1] : 3;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int opp = 1 - owner;
    // 这个 tick 挂在**对手桶**里跑 → 判"对手本回合是不是用了属性技能"
    if (!executed_skill_this_round(ctx, opp) || executed_skill_is_attack(ctx, opp)) {
        return EffectResult::kOk;
    }
    const int before = ctx->getPet(owner).hp;
    g_core->heal_amount(ctx, owner, max_hp_of(ctx, owner) / denom);
    const int healed = ctx->getPet(owner).hp - before;
    if (healed > 0) {
        g_core->deal_pink_damage(ctx, opp, healed, DamageKind::PERCENT_VALUE, owner);
    }
    return EffectResult::kOk;
}

EffectResult effect_2099_window(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    const int denom = args.int_args[3];
    if (owner < 0 || owner > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    const std::vector<int> tick{owner, denom};
    register_round_effect(ctx, State::BATTLE_FIRST_AFTER_ACTION, /*owner=*/1 - owner, 2099,
                          rounds, tick, &effect_2099_tick);
    register_round_effect(ctx, State::BATTLE_SECOND_AFTER_ACTION, /*owner=*/1 - owner, 2099,
                          rounds, tick, &effect_2099_tick);
    return EffectResult::kOk;
}

// ── 886 「下次攻击忽略对手双防值的{0}%」Args=[30]（镌戒）──
// 落点 = 公式侧的 `ws.defense_ignore_pct[**受击方**]`（calculation.h 在读防御值之前按它折减；
// 索引是**防御方** → 写对手的槽位只影响"打在对手身上"的攻击，即自己的下一次攻击）。
// "下次攻击"= 跨回合的一次性承诺 → pending + **常驻节点**（技能分支节点只活本回合）：
//   · ROUND_START：pending 期间重写槽位（ws 每回合 reset → 必须重写）；
//   · AFTER_ACTION：本回合**用了攻击技能**（= 这次攻击已结算完）→ 消费 pending 并清槽；
//   · EVENT_SWAP：本人下场即撤（官方《名词解释》idx=127「之后」：未提结束点时默持续至下场）。
EffectResult effect_886_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int pct = kongyuan::kongyuan_slot(ctx->plugin_storage, 886000 + owner);
    if (pct > 0) {
        ctx->ws.defense_ignore_pct[1 - owner] = pct;
    }
    return EffectResult::kOk;
}

EffectResult effect_886_after_action(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    int& pct = kongyuan::kongyuan_slot(ctx->plugin_storage, 886000 + owner);
    if (pct <= 0) {
        return EffectResult::kOk;
    }
    if (executed_skill_this_round(ctx, owner) && executed_skill_is_attack(ctx, owner)) {
        pct = 0;   // "下次攻击"已用掉（本回合这一击在 ATTACK_DAMAGE 已吃到折减）
        ctx->ws.defense_ignore_pct[1 - owner] = 0;
    }
    return EffectResult::kOk;
}

EffectResult effect_886_next_attack_ignore_def(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int pct = args.int_args[2];
    if (owner < 0 || owner > 1 || pct <= 0) {
        return EffectResult::kOk;
    }
    kongyuan::kongyuan_slot(ctx->plugin_storage, 886000 + owner) = pct;
    ctx->ws.defense_ignore_pct[1 - owner] = pct;   // 当回合先写（同回合不可能再攻击，安全）
    if (kongyuan::kongyuan_slot(ctx->plugin_storage, 886900 + owner)) {
        return EffectResult::kOk;   // 常驻节点与监听器只装一次
    }
    kongyuan::kongyuan_slot(ctx->plugin_storage, 886900 + owner) = 1;
    const std::vector<int> self{owner};
    register_permanent_skill_node(ctx, State::BATTLE_ROUND_START, owner, 886, self,
                                  &effect_886_round_start);
    register_permanent_skill_node(ctx, State::BATTLE_FIRST_AFTER_ACTION, owner, 886, self,
                                  &effect_886_after_action);
    register_permanent_skill_node(ctx, State::BATTLE_SECOND_AFTER_ACTION, owner, 886, self,
                                  &effect_886_after_action);
    ctx->event_center_.register_watcher(
        EventType::EVENT_SWAP, owner, ctx->roundCount, /*duration_rounds=*/0, /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (c && ev.actor == owner) {
                kongyuan::kongyuan_slot(c->plugin_storage, 886000 + owner) = 0;
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 1775 「令对手下{0}次使用的技能PP值归零」Args=[1]（铭刻）──
// 没有现成票据（RuleCenter 无 PP 类目）→ 用"计数器 + 使用事件"：
// 计数存 `plugin_storage`（战斗级，跨切换保留——"下N次使用"是挂在**对手**身上的承诺）；
// 每次对手**使用**技能（命中 EVENT_HIT / 无效 EVENT_SKILL_INVALID，两者合起来恰好覆盖
// "用了一次"；被封印（SEALED）不算"使用"，不消费）把他的**该技能**PP 归零并递减。
// ⚠️ 用哪个技能：读 `roundChoice[opp][1]`（事件 drain 时仍是本回合的选择）。
// ⚠️ "下{0}次"叠加 = 累加（每次使用再叠 {0} 次），不做覆盖。
EffectResult effect_1775_pp_zero_next(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int times = args.int_args[2];
    if (owner < 0 || owner > 1 || times <= 0) {
        return EffectResult::kOk;
    }
    kongyuan::kongyuan_slot(ctx->plugin_storage, 1775000 + owner) += times;
    if (kongyuan::kongyuan_slot(ctx->plugin_storage, 1775900 + owner)) {
        return EffectResult::kOk;   // 接线只装一次
    }
    kongyuan::kongyuan_slot(ctx->plugin_storage, 1775900 + owner) = 1;
    auto on_use = [owner](BattleContext* c, const BattleEvent& ev) {
        if (!c || ev.actor != 1 - owner) {
            return;
        }
        int& pending = kongyuan::kongyuan_slot(c->plugin_storage, 1775000 + owner);
        if (pending <= 0) {
            return;
        }
        const int opp = 1 - owner;
        constexpr int kActionSelectSkill = 1;
        if (c->roundChoice[opp][0] != kActionSelectSkill) {
            return;
        }
        const int slot = c->roundChoice[opp][1];
        auto& skills = c->seerRobot[opp].elfPets[c->on_stage[opp]].skills;
        if (slot < 0 || slot >= static_cast<int>(skills.size())) {
            return;
        }
        --pending;
        if (skills[static_cast<std::size_t>(slot)].pp > 0) {
            skills[static_cast<std::size_t>(slot)].pp = 0;   // "使用的技能PP值归零"
        }
    };
    for (const EventType type : {EventType::EVENT_HIT, EventType::EVENT_SKILL_INVALID}) {
        ctx->event_center_.register_watcher(type, owner, ctx->roundCount,
                                           /*duration_rounds=*/0, /*once=*/false, on_use,
                                           WatcherScope::TEAM);
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表）──
// 时点/分支由 core 表决定（`skills.cpp` 的 effect_register_state / default_branch_for_effect）：
// ⚠️ 下面几条**必须**在 core 表里改时点/分支，本文件只提供"执行什么"：
//   1218 → AFTER_ACTION（"未击败"要读伤害落地后的 hp）
//   886  → ROUND_START + AFTER_ACTION 各一条（同一 id 的两条**不同时点**节点）
//   1165 → ROUND_END（检测）+ ROUND_START（应用）
//   2093/1162/1035/2099 → 各自"登记窗口"的入口在 SKILL_EFFECT，tick 走窗口注册
SKILL_EFFECT(1925, effect_1925_drain_max_hp_fraction);
SKILL_EFFECT(436, effect_436_pink_own_lost);
SKILL_EFFECT(514, effect_514_pink_opp_lost);
SKILL_EFFECT(751, effect_751_pink_opp_current);
SKILL_EFFECT(872, effect_872_drain_boost_seal);
SKILL_EFFECT(886, effect_886_next_attack_ignore_def);
SKILL_EFFECT(1035, effect_1035_hit_fear);
SKILL_EFFECT(1162, effect_1162_window);
SKILL_EFFECT(1165, effect_1165_window);
// ⚠️ 窗口 tick（effect_*_tick 一族）**不**在注册表里：它们是 `register_round_effect` /
//    `register_permanent_skill_node` 直接按函数指针挂进桶的，不走 id 派发。
SKILL_EFFECT(1218, effect_1218_not_defeated_priority);
SKILL_EFFECT(1701, effect_1701_chance_double);
SKILL_EFFECT(1775, effect_1775_pp_zero_next);
SKILL_EFFECT(1808, effect_1808_round_effect_damage_up);
SKILL_EFFECT(818, effect_818_reflect_taken);
SKILL_EFFECT(1652, effect_1652_power_by_lost_hp);
SKILL_EFFECT(448, effect_448_window);
SKILL_EFFECT(2092, effect_2092_gaoming_shitu);
SKILL_EFFECT(2093, effect_2093_gaoming_drain_window);
SKILL_EFFECT(2094, effect_2094_poem_damage);
SKILL_EFFECT(2095, effect_2095_full_hp_fear);
SKILL_EFFECT(2097, effect_2097_low_hp_seal);
SKILL_EFFECT(2098, effect_2098_priority_window);
SKILL_EFFECT(2099, effect_2099_window);
