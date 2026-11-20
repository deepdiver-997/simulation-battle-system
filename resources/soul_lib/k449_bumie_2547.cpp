// K449 · 4947 不灭战魂·盖亚 魂印 2547 —— 不武/胜之道机（effect_icon row 2147）
//（2026-11-10，worktree k449；全库首实现。args "1 0"——db[0]=1/db[1]=0 官方 tips
//  无对应子句，不消费（808 db[2] 同款纪律，记档）。
//
// tips 三段逐句映射（编号对齐下行注）：
// ① 「自身存活于场下时，对方每次触发概率不高于10%的抗性或执行概率不高于10%的
//    通用特性时，对方获得1分不武」
//    → **【阻塞：内核缺口，未落地——精确证据与最小 patch 提案见文件尾注】**。
//      需内核面 = 抗性/特性**成功当时**的 有效率+成功来源（哪段/哪路径）快照事件，
//      保持原随机顺序与短路；提案未获批准 → 本句不落地（编排队）。
//      武装形态先例已具备：j_defeat_family 349 的 SoulScope::ROSTER（场下挂监听）。
//      des 569 的获得副作用已实现为可调用函数（k2547_buwu_gain_side_effect），
//      ①落地的唯一写入点接入即闭环；**场景不手播不武冒充场下获得**（主会话 2026-11-10
//      纪律）——播种仅用于下游消费腿（2575 门 / ②胜之道减除），且断言注释明记播种。
// ② 「自身存活于场上时，自身攻击技能无视对手的能力等级提升，若攻击技能使用时
//    对手不处于能力提升则下回合攻击技能先制+2」→ 已落地：
//    - 无视提升 = E11 既有通道 ws.boost_pierced[opp]（195/494/486 族；
//      soul 侧 defender 索引先例 k412_random_pool_observation.cpp:278/
//      k414_dodge_chain_purge.cpp:461），EVENT_SKILL_USED watcher（own+攻击技）
//      施放即置位，FSM 行动结束自清=逐刀语义 ✓。
//    - 下回合先制+2 = 对手无提升时 NextRounds(1) FMR 桶——ws.preemptive_level 在
//      FMR 时点 memset，须写 FMR 桶（SOP §二.2；1123 先例 batch_a_skills.cpp:525
//      同形 register_next_rounds_effect(FMR)）✓。
// ③ 「回合开始时，若自身处于异常状态则2回合内受到的攻击伤害减少50%，若自身不处于
//    异常状态则2回合内战斗阶段结束时恢复自身最大体力的30%，若场上存在武道则自身
//    开启胜之道并减少对方1分不武」→ 已落地（BATTLE_ROUND_START STAGE 节点）：
//    - 异常门 = ctx->has_any_active_abnormal_status（battleContext.h:797，447 套装
//      先例查询式）；
//    - 减伤窗 = ws.damage_reduce_add[owner] **首空槽**写 +50 当回合直写 + NextRounds(1)
//      ROUND_START tick 重写（两回合窗；槽形先例 j_wounded_next_round_reduce.cpp:49；
//      消费面 battleContext.cpp:408 applyDamageReduction 对全 4 槽求和；ws 每回合
//      reset → 逐轮重开天然不叠）；
//    - 回血窗 = **stamp 滚动窗**（k2547HealWinBase+owner 存"覆盖到的最后 ROUND_END"
//      绝对 rc）：回合开始无异常 → stamp=roundCount+1（覆盖 本轮+下轮 两拍）；
//      BATTLE_ROUND_END 节点（④ 同节点前段）查 stamp>=roundCount → 回 30%。滚动态
//      每轮刷新 stamp 不叠加条目（稳态恰 1×30%/轮）——"每回合开始重判 + {2}回合窗"
//      字面模型若按 ContinuousEffect 逐轮重开会在重叠拍双倍回血，**刷新不叠**为
//      建模决策（3550 御固定句柄 re-grant 同款"刷新不叠加"惯例），记档；
//      heal_amount 钳 max、吃封回血；条件转异常时旧窗自然走完（stamp 不动）。
//    - 「场上存在武道」= 双方**在场**实际武道/不武（gaia_side_has_wudao 任一侧，
//      主会话 2026-11-10 口径：不扫描替补）；胜之道开启旗 kGaiaShengKey + 对方
//      不武 -1（钳 0；每回合开始条件成立即结算——原文为回合开始时点效果）；
//    - 胜之道挡伤（des 570 官方定义「自身抵挡拥有武道的对方的攻击，触发后衍化
//      武道」）= BLOCK 相位观察者（arm 每 epoch 挂一次，回调实时查旗与对手
//      拥有武道）→ 133 同位全字段清零；「触发后衍化武道」：4947 无声明轨迹 →
//      des 449「无可衍化对象→衍化关闭」→ 胜之道为常驻挡（记档）。2575「消除
//      武道」清 kGaiaShengKey → 挡停（与 2377 同消口）。
// ④ 「战斗阶段结束时，若当回合双方任意一方使用了攻击技能则记录当回合双方造成的
//    攻击伤害，自身下回合使用攻击技能时附加此记录50%的百分比伤害，此记录小于200
//    则下回合造成的攻击伤害提升100%（boss无效）」→ 已落地（boss_invalid=true）：
//    - 战斗级 EVENT_TAKE_DAMAGE watcher（两攻击伤害时点，K341 同款）逐侧记账；
//      记账只认 **NORMAL 红伤**（ev.damage_kind 过滤——「攻击伤害」读法定档：
//      附加粉伤/反弹粉伤不计，blocked 红伤 amount=0 天然不入账）；
//    - BATTLE_ROUND_END 节点：任一侧用技旗（watcher 写）→ record=双侧账快照、
//      清账；record<200 → NextRounds(1) ROUND_START tick ws.damage_add_pct[+100]；
//    - 附加 = ② 同一 SKILL_USED watcher：record>0 → deal_pink_damage(record*50%,
//      PERCENT_VALUE)（K341 上轮受伤 rider 同位）。
// ⚠️ des 569「不武：己方获得的当回合结束后消除己方在场精灵的回合类效果、能力提升
//    状态并令其害怕；不武额外视为武道；上限4分」→ 获得副作用循环
//    k2547_buwu_gain_side_effect 已实现（上限钳 4 亦在此），唯一生产者=①（阻塞）
//    → 现阶段无活生产者、函数不注册节点（不注册空转宏）——①落地即闭环。
//    免断（break_round_effects 查 IMMUNE）用于"消除己方"是否豁免未定案——函数内
//    现走 break_round_effects（免断视为不消，保守口径），记档。
//
// 键段（99011301x/020-021，绝对键直读直写——**+side 偏移复合键碰撞教训**：
// 990112972 撞 2300 句柄 side1 已实锤（T8376）；2026-11-11 续作展开审计再实锤两处：
// 原 k2547RiderWiredBase=990113011 的 +owner1 展开位 012 = k2547Ledger0（镜像对局
// owner1 接线旗写进 side0 当回合伤害账，且账本每轮清零会抹旗）→ 已改 020/021
// （T8378e 锁）。现行布局：接线旗 010 全局单份、账 012/013、用技旗 014/015、记录
// 016/017、回血窗戳 018/019、rider 接线旗 020/021——全部互不重叠；与 2377 侧旗键
// 990113001-004 间距 ≥2（+side 偏移族纪律，gaia_wudao_runtime.h 键距注）。
//
// 三查（2026-11-10）：①内核 grep 2547 无行为硬编码 ②effect_info 2547 无模板行
// （effect_icon tips 权威）③soul_lib/registry 无 2547 注册——真缺口。载体唯一：
// 4947（row 2147，pet_id=[4947]）。2574/2575（moves 侧）已另行落地于
// j_gaia_stance.cpp（T8373/T8374 验收）。
// 先例：K341 711（SKILL_USED rider + TAKE_DAMAGE 账本）、E11（boost_pierced）、
// 1123（FMR 下回合先制）、2291（NextRounds 重写窗）、K180（BLOCK 观察者 epoch arm）、
// diwang_3512（soul 侧直构 ContinuousEffect 显式 start）、des570/569/446-449（现行
// 官方定义）。
// 场景回归：scenario_8370 T8378（②③④+胜之道挡+镜像键差分）——见同号段文件。
//
// ══ ① 阻塞证据包（2026-11-10 实查行号，供主会话审 patch 提案）════════════
// A 抗性半段：
//   - 掷点本体 include/abnormal-system/resistance-system.h:119-149 isResistantTo：
//     **两段独立掷点**——全免段 complete_immunity（:126-130，低段惩罚 :121-124）
//     先掷，失败再掷单项槽位（:136-146，惩罚 :138-143）；返回仅 bool，**成功来源
//     （哪段）与掷点当时有效率不出参**——事后查询无法判别本次成功来自全免 5% 还是
//     单项 35%，也无法还原「≤10%」门需要的实际率。
//   - 成功事件 EVENT_ANOMALY_RESISTED 已存在：src/primitives/battle_primitives.cpp
//     :343（[4] 段成功出口同步 emit，语义唯一、圣芒佑界 476 在用），载荷
//     {actor,target,anomaly_id,state} **不含段来源与有效率**。
//   - 率可变：anomaly_resist_low_penalty[target][slot]（battle_primitives.cpp:335
//     掷点时读）由印记插件战时维护——静态/事后读不安全。
// B 特性半段：
//   - 声明率 trait_proc_permille（include/effects/trait_state.h:328，含 proc_override）
//   - 执行点多路径（src/effects/common_trait.cpp）：瞬杀合并掷点 :344-372（proc_forced
//     免掷 :369-371、proc_zeroed_le50 降零 :362-364、≤0 免掷 return）；行动异常特性
//     :488-497（declared→forced 100→闸门改写 rewrite_anomaly_chance→掷点→**hp<=0
//     continue 早退**→施加）；主动毒 :503-527（同型+hp>0 门）；被动毒 apply_anomaly_raw
//     全检测直通。→ 只包装若干 roll 会漏 forced/降零/闸门改写后的真实率，且「roll 成功
//     ≠ 效果执行」（hp 早退）；"执行"是否需掷中亦无语料定案。
// C 最小 patch 提案（未批不动内核）：
//   - Patch A（抗性，随机序列零移位）：isResistantTo 加默认空参出参重载
//     `bool isResistantTo(int, int penalty=0, int* out_stage=nullptr, int* out_eff=nullptr)`
//     ——旧调用点零改动、不多掷；[4] 段（battle_primitives.cpp:331-336）取回
//     stage/eff 后在 :343 emit 前填 BattleEvent 两个**末尾追加**字段
//     （resist_stage 0=全免/1=单项、resist_eff_pct）。2547 ① watcher 认
//     EVENT_ANOMALY_RESISTED + `ev.target==opp && ev.resist_eff_pct<=10` → 不武+1。
//   - Patch B（特性）：需在 common_trait.cpp 各执行路径的成功**执行**点（apply 调用
//     前后）发统一"通用特性执行"通知（载荷=实际 gated_pct+forced+kind+执行是否落地），
//     并先补"执行是否需掷中"语料裁定——面较大，建议独立内核单，不与本单捆绑。
//   - 武装形态：349 ROSTER 先例（j_defeat_family.cpp:444-446）+ EVENT_* watcher
//     战斗级一次 + 回调判 `宿主场下存活 && on_stage[owner]!=宿主槽`（"存活于场下"）。

#include "common.h"

#include "../gaia_wudao_runtime.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 2547;
// ── 键段（本文件绝对键直读直写；见头注键段纪律）──────────────────────────
constexpr int k2547LedgerWiredKey = 990113010;  // 账本 watcher 已挂（战斗级一次，全局单份）
constexpr int k2547RiderWiredBase = 990113020;  // +owner：SKILL_USED watcher 旗（单次 +owner =
                                                // 020/021。⚠️ 原 990113011 与账键
                                                // k2547Ledger0(990113012 绝对)**碰撞**——
                                                // owner1 的接线旗写进 side0 的当回合伤害账
                                                // （镜像 2547 对局账面恒 +1 幻影），且账本
                                                // 每轮清零会抹旗；2026-11-11 展开审计实锤，
                                                // T8378e 镜像差分锁）
constexpr int k2547Ledger0 = 990113012;         // 当回合 side0 攻击伤害账（绝对）
constexpr int k2547Ledger1 = 990113013;         // 当回合 side1 攻击伤害账（绝对）
constexpr int k2547AtkUsed0 = 990113014;        // 当回合 side0 用过攻击技（绝对 0/1）
constexpr int k2547AtkUsed1 = 990113015;        // 当回合 side1 用过攻击技（绝对 0/1）
constexpr int k2547RecordBase = 990113016;      // 上回合双侧伤害记录（绝对 016+owner）
constexpr int k2547HealWinBase = 990113018;     // ③回血滚动窗戳（绝对 018+owner；017/019
                                                // 留空隔位——011 退役，+owner 展开审计）
constexpr int k2547BlockEpochKey = 990112991;   // 胜之道观察者 epoch 旗（on_stage per-pet；
                                                // 与 gaia 9901129xx 段既有键不撞、审计同段）
// des 569 不武上限（官方「上限4分」）。
constexpr int kBuWuCap = 4;

// 绝对键 int 直读直写（any_cast 治理同 gaia_int_or；账本/旗键不走 +side 存取器，
// 杜绝 990112972 型偏移复合碰撞——键与 side 解耦后镜像天然各写各的绝对槽）。
inline int abs_int(const BattleContext* ctx, int key) {
    if (!ctx) {
        return 0;
    }
    const auto it = ctx->plugin_storage.find(key);
    return it == ctx->plugin_storage.end() ? 0 : gaia_int_or(it->second);
}

inline void abs_set(BattleContext* ctx, int key, int value) {
    if (ctx) {
        ctx->plugin_storage[key] = value;
    }
}

// ── NextRounds 窗注册（soul TU 本地 inline：与 moves_lib/common.h:218 同形；
//    直构 Effect+ContinuousEffect，diwang_3512.cpp:190 先例——soul_lib/common.h
//    不带 moves helper，不跨 dylib 引 moves 符号）────────────────────────────
inline int next_rounds_effect(BattleContext* ctx, State trigger, int owner, int effect_id,
                              int rounds, std::vector<int> ints, EffectFn fn) {
    if (!ctx || owner < 0 || owner > 1 || rounds <= 0 || !fn) {
        return 0;
    }
    Effect e(effect_id, 0, owner, rounds, EffectArgs(std::move(ints)), fn);
    const int start = ctx->round_effect_start_round(owner, rounds, EffectWindowKind::NextRounds);
    auto ce = std::make_unique<ContinuousEffect>(e, trigger, owner, rounds, start);
    ce->source_id_ = effect_id;
    ctx->register_skill_effect(trigger, owner, std::move(ce));
    return start;
}

// ws.damage_reduce_add[side] 首空槽写（j_wounded_next_round_reduce.cpp:49 同款；
// 消费面对全 4 槽求和，ws 每回合 reset → tick 逐轮重写占首槽）。
inline void reduce_slot_write(BattleContext* ctx, int owner, int pct) {
    int(&slots)[4] = ctx->ws.damage_reduce_add[owner];
    for (int i = 0; i < 4; ++i) {
        if (slots[i] == 0) {
            slots[i] = pct;
            return;
        }
    }
}

// ③ 异常分支 ROUND_START tick（NextRounds(1)：次轮重写 +50）。
// ⚠️ 火轮守卫（args[1]=应火轮号）：本 tick 从 ROUND_START 魂印节点注册进**同状态
// 技能桶**——同一次 execute_registered_actions 里魂印桶先跑、技能桶后跑，注册当场
// 就会被本轮技能桶扫到（isExpired 对 registered_round=下轮 的条目在本轮恒假）→
// 首个异常回合会写成 [50,50]=-100%。守卫「rc < 应火轮即跳过」消除同扫早火
// （2026-11-11 场景 T8378b 实测实锤后修正）。
EffectResult k2547_mitigate_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int fire_round = args.int_args[1];
    if (owner < 0 || owner > 1 || ctx->roundCount < fire_round) {
        return EffectResult::kOk;   // 未到应火轮（同扫早火守卫）
    }
    reduce_slot_write(ctx, owner, 50);
    return EffectResult::kOk;
}

// ③ 回血窗 beat（在 ROUND_END 节点前段调用）：stamp 覆盖本轮 → 回 30%（钳 max、
// 吃封回血）；刷新不叠见头注③。
void k2547_heal_window_beat(BattleContext* ctx, int owner) {
    if (!ctx || !g_core || !g_core->heal_amount) {
        return;
    }
    if (gaia_side_int(ctx, owner, k2547HealWinBase) < ctx->roundCount) {
        return;   // 本轮 ROUND_END 不在窗内（窗戳 = 覆盖到的最后绝对 rc）
    }
    if (ctx->getPet(owner).hp <= 0) {
        return;
    }
    const int max_hp = std::max(
        1, ctx->getPet(owner).numericalBase[NumericalPropertyIndex::HP]);
    (void)g_core->heal_amount(ctx, owner, max_hp * 30 / 100);
}

// ④「下回合造成的攻击伤害提升100%」：NextRounds(1) ROUND_START tick。
EffectResult k2547_amp100_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ctx->ws.damage_add_pct[owner] += 100;
    return EffectResult::kOk;
}

// ② 「下回合攻击技能先制+2」：FMR 桶（memset 之后执行，SOP §二.2）。
EffectResult k2547_preempt_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int bonus = args.int_args[1];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ctx->ws.preemptive_level[owner] += bonus;
    return EffectResult::kOk;
}

// ── des 569 获得副作用（可调用；唯一生产者=①【阻塞】，落地即闭环）────────
// 获得侧当回合结束后：消除己方在场精灵回合类效果（break_round_effects，免断视为
// 不消——保守口径记档）+ 清己方能力提升 + 己方在场精灵害怕（随机时长，施控族惯例）。
void k2547_buwu_gain_side_effect(BattleContext* ctx, int gained_side) {
    if (!ctx || !g_core || gained_side < 0 || gained_side > 1) {
        return;
    }
    // 上限钳 4（des 569「上限4分」）——① 写入点须先经本钳（现无写入点，函数即契约）。
    const int cur = gaia_buwu(ctx, gained_side);
    if (cur > kBuWuCap) {
        gaia_side_set(ctx, gained_side, kGaiaBuWuKey, kBuWuCap);
    }
    if (g_core->break_round_effects) {
        (void)g_core->break_round_effects(ctx, gained_side);
    }
    if (g_core->clear_stat_boosts) {
        (void)g_core->clear_stat_boosts(ctx, gained_side);
    }
    if (g_core->apply_anomaly_checked) {
        (void)g_core->apply_anomaly_checked(ctx, gained_side,
                                            static_cast<int>(AbnormalStatusId::Fear),
                                            random_anomaly_duration(), gained_side,
                                            /*chance=*/100, ChanceSource::SoulMark);
    }
}

// ③ 回合开始节点：异常双窗门 + 场上存在武道 → 胜之道 + 对方不武-1。
EffectResult k2547_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const int opp = 1 - owner;
    // ③ 异常门：异常 → 2 回合受伤 -50%（当回合直写 + 次轮 tick）；无异常 → 回血
    // 滚动窗刷新 stamp=roundCount+1（覆盖 本轮+下轮 两拍，稳态不叠，头注③）。
    if (ctx->has_any_active_abnormal_status(owner)) {
        reduce_slot_write(ctx, owner, 50);
        (void)next_rounds_effect(ctx, State::BATTLE_ROUND_START, owner, 2547, 1,
                                 {owner, ctx->roundCount + 1}, &k2547_mitigate_tick);
    } else {
        gaia_side_set(ctx, owner, k2547HealWinBase, ctx->roundCount + 1);
    }
    // ③ 场上存在武道（双方在场实际，不扫替补）→ 开胜之道 + 对方不武 -1（钳 0）。
    // ⚠️ kGaiaShengKey 是 on_stage 键（runtime 头键段注释；读取面 gaia_sheng_active=
    // gaia_pet_int、2575 消除写点 gaia_pet_set 同槽）——必须走 gaia_pet_set 写**在场宠
    // on_stage_storage**。旧稿误用 gaia_side_set（plugin+side）→ 读取面永远读不到
    // （胜之道永不开启），且 958/959 展开位与 2574 场地盾戳 959/960 **碰撞**
    // （sheng side1=959=盾 side0；2026-11-11 T8378b3 实锤后修正）。
    if (gaia_side_has_wudao(ctx, 0) || gaia_side_has_wudao(ctx, 1)) {
        gaia_pet_set(ctx, owner, kGaiaShengKey, 1);
        const int opp_buwu = gaia_buwu(ctx, opp);
        if (opp_buwu > 0) {
            gaia_side_set(ctx, opp, kGaiaBuWuKey, opp_buwu - 1);
        }
    }
    // 当回合攻击技使用旗（④ 门）在 ROUND_START 复位（账本随逐刀累计、ROUND_END 结算）。
    abs_set(ctx, k2547AtkUsed0, 0);
    abs_set(ctx, k2547AtkUsed1, 0);
    return EffectResult::kOk;
}

// ④ 回合结束结算（含 ③ 回血窗 beat 前段）：任一侧用过攻击技 → record=双侧账、
// 清账；record<200 → 下回合 +100%。
EffectResult k2547_round_end_settle(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !onstage_has_soul_mark(ctx, a.owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    // ③ 回血窗 beat（官方尾注只挂第④段——③ 无 boss 门，见头注 boss 归属注）。
    k2547_heal_window_beat(ctx, owner);
    if (ctx->is_boss_challenge) {
        return EffectResult::kOk;   // ④ boss 无效（tips 第④段尾注；节点旗双保险）
    }
    const int used = abs_int(ctx, k2547AtkUsed0) + abs_int(ctx, k2547AtkUsed1);
    const int ledger0 = abs_int(ctx, k2547Ledger0);
    const int ledger1 = abs_int(ctx, k2547Ledger1);
    // 旗复位由 ③ 的 ROUND_START 负责；账本在此清（逐轮重计）。
    abs_set(ctx, k2547Ledger0, 0);
    abs_set(ctx, k2547Ledger1, 0);
    if (used <= 0) {
        abs_set(ctx, k2547RecordBase + owner, 0);
        return EffectResult::kOk;   // 双方均未用攻击技 → 不记录（record 归零）
    }
    const int record = ledger0 + ledger1;
    abs_set(ctx, k2547RecordBase + owner, record);
    if (record > 0 && record < 200) {
        (void)next_rounds_effect(ctx, State::BATTLE_ROUND_START, owner, 2547, 1,
                                 {owner}, &k2547_amp100_tick);
    }
    return EffectResult::kOk;
}

// ── arm：观察者/watcher（early；每 battle 一次 watcher + 每 epoch 挡伤观察者）──
EffectResult k2547_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const int opp = 1 - owner;
    // ④ 账本 watcher：战斗级全局单份（镜像双 2547 不叠记——账本本就双侧共用）。
    if (!ctx->plugin_storage.count(k2547LedgerWiredKey)) {
        ctx->plugin_storage.emplace(k2547LedgerWiredKey, 1);
        ctx->event_center_.register_watcher(
            EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [](BattleContext* c, const BattleEvent& ev) {
                if (!c || ev.amount <= 0 || ev.actor < 0 || ev.actor > 1) {
                    return;
                }
                if (ev.state != static_cast<int>(State::BATTLE_FIRST_ATTACK_DAMAGE)
                    && ev.state != static_cast<int>(State::BATTLE_SECOND_ATTACK_DAMAGE)) {
                    return;   // 只认两攻击伤害时点（K341 同款）
                }
                if (ev.damage_kind != static_cast<int>(DamageKind::NORMAL)) {
                    return;   // 「攻击伤害」=红伤（附加/反弹粉伤不计，头注④读法定档）
                }
                if (c->is_boss_challenge) {
                    return;   // ④ boss 无效
                }
                abs_set(c, (ev.actor == 0) ? k2547Ledger0 : k2547Ledger1,
                        abs_int(c, (ev.actor == 0) ? k2547Ledger0 : k2547Ledger1)
                            + ev.amount);
            },
            WatcherScope::TEAM);
    }
    // ④ 用技旗 + ② pierce/先制 + ④ rider：SKILL_USED watcher（own，战斗级一次）。
    if (!ctx->plugin_storage.count(k2547RiderWiredBase + owner)) {
        ctx->plugin_storage.emplace(k2547RiderWiredBase + owner, 1);
        ctx->event_center_.register_watcher(
            EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [owner](BattleContext* c, const BattleEvent& ev) {
                if (!c || !g_core || ev.actor != owner || ev.used_attribute) {
                    return;   // 攻击技才 ②/④
                }
                if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                    return;
                }
                const int opp = 1 - owner;
                // ④ 用技旗（门：任意一方用攻击技）——绝对键直写（014/015，镜像各侧）。
                abs_set(c, (owner == 0) ? k2547AtkUsed0 : k2547AtkUsed1, 1);
                // ② 无视对手能力等级提升（E11 通道；FSM 行动结束自清=逐刀）
                c->ws.boost_pierced[opp] = true;
                // ② 对手无提升 → 下回合先制+2（FMR 桶）
                bool opp_boosted = false;
                for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
                    if (c->ability_levels[opp][i] > 0) {
                        opp_boosted = true;
                        break;
                    }
                }
                if (!opp_boosted) {
                    (void)next_rounds_effect(c, State::BATTLE_FIRST_MOVE_RIGHT, owner,
                                             2547, 1, {owner, 2}, &k2547_preempt_tick);
                }
                // ④ 附加：上回合记录 50% PERCENT_VALUE（boss 无效手动门）
                if (c->is_boss_challenge) {
                    return;
                }
                const int record = abs_int(c, k2547RecordBase + owner);
                if (record > 0) {
                    const int rider = record * 50 / 100;
                    if (rider > 0) {
                        g_core->deal_pink_damage(c, opp, rider,
                                                 DamageKind::PERCENT_VALUE, owner);
                    }
                }
            },
            WatcherScope::TEAM);
    }
    // ③ 胜之道挡伤观察者：每 epoch 一次（on_stage 槽 epoch 旗，换宠重挂）。
    ElfPet& pet = ctx->getPet(owner);
    auto it = pet.on_stage_storage.find(k2547BlockEpochKey);
    if (it == pet.on_stage_storage.end()) {
        it = pet.on_stage_storage.emplace(k2547BlockEpochKey, int{-1}).first;
    }
    if (int* epoch = std::any_cast<int>(&it->second)) {
        if (*epoch == ctx->pipeline_valid_id[owner]) {
            return EffectResult::kOk;   // 同上场期重复触发 → 不重挂
        }
        *epoch = ctx->pipeline_valid_id[owner];
    }
    const int self = owner;
    ctx->register_damage_effect(
        DamagePhase::BLOCK, self, DamageEffectCategory::BLOCK,
        [self](BattleContext* c, int bucket_owner) {
            if (!c) {
                return;
            }
            DamageSnapshot& d = c->resolvedDamage;
            if (d.defenderId != bucket_owner || !d.isRed || d.final <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(c, self, kMarkId)) {
                return;
            }
            // 胜之道旗实时查（2575 消除即停）；对手拥有武道才挡（des 570 主语门）。
            if (gaia_sheng_active(c, self) && gaia_side_has_wudao(c, 1 - self)) {
                d.base = 0;
                d.afterAdd = 0;
                d.afterMul = 0;
                d.final = 0;
                d.addPct = 0;
                d.mulCoef = 0.0;
                // 「触发后衍化武道」：4947 无声明轨迹 → des 449 关闭（记档，不推进）。
            }
        });
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（2547 入账，只增不改）────
// ⚠️ boss 尾注归属（2026-11-10 现库 row 2147 tips 实读定档）：tips 三段
//   「①场下不武+②无视提升先制 | ③回合开始双窗+胜之道 | ④战斗阶段结束记录+ rider
//   +amp100**（boss无效）**」——尾注只挂**第④段**。按"标注单位=带尾注的子句"
//   纪律（plugin_interface.h 注、j_defeat_family 头注同口径）：仅 ROUND_END 节点
//   boss_invalid=true；arm/ROUND_START 不标（②③ boss 局照常）；④ 的 watcher/
//   settle 另有回调内手动 is_boss_challenge 双保险（K391 口径）。与 2377（⑦ 尾注
//   罩全魂印 → 全节点标）不同属正常——两行尾注位置不同。
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k2547_arm,
                        /*once=*/false, /*early=*/true, SoulScope::STAGE,
                        /*boss_invalid=*/false),
              SOUL_NODE(State::BATTLE_ROUND_START, &k2547_round_start,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false),
              SOUL_NODE(State::BATTLE_ROUND_END, &k2547_round_end_settle,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true)});
