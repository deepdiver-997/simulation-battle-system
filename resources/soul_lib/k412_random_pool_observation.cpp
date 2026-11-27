// K412 随机触发池/事象观测族 —— effect 2514/2520（2 宏 / 4 行，双行档位差，
// 看板 K412 复杂族 2026-11-04，a01-k412，worktree ../agents/k412；号段 7720-7729。
// 通道=2514 随机池选取（roll+池索引+审计槽申报 K662）+触发项数递增账本+各效果原语
//   （必暴 ws.must_crit/K394 ④封回血窗）；2520=致死拦截+复活（undying_1067/
//   K11 304 重生回满记档先例）+退出时消强消回合（K396 1040 同族=undying_1067 同式）
//   +「记录当回合」账本（k382 2501 skill_type_view/skill_use_seq 同款读法）。
//   dump 全文先行，双行纯 args 档位差。）
//
// 行清单（tools/soul_effects_dump.py 2514 2520，2026-11-04）：
//   2514 西西弗 4905 两行（row2092 本体 / row2106 伙伴「黄金圣徒伙伴效果」，kind 同
//   [1,3,5]，纯 args 档位差 db[0]/db[2]/db[3]）：
//     row2092 args[1,1,30,300,1]：「自身使用技能时，随机触发下列效果种的1项并使本次
//       在场的触发项数+1：|2回合内自身攻击必定打出致命一击|2回合内对手内无法通过自身
//       技能恢复体力|2回合内自身攻击技能无视对手能力提升状态|2回合内自身能力提升状态
//       无法被消除|2回合内受到的攻击伤害减少30%|2回合内每回合战斗阶段结束时恢复自身
//       300点体力|（boss无效）」
//     row2106 args[2,1,50,400,1]：触发 2 项、减伤 50%、回血 400（余同）。
//     → db[0]=触发项数（1/2 档位差）、db[1]=1 旗不消费（K1 纪律）、db[2]=⑤减伤%
//       （30/50）、db[3]=⑥回合末回血量（300/400）、db[4]=1 旗不消费；kind=[1,3,5]
//       备档不消费（无行文锚）。
//     池 6 项（0..5）：①必暴 ②对手封回血 ③攻击无视对手提升 ④自身提升不可被消除
//     ⑤受伤减 30|50% ⑥每回合战斗阶段结束回 300|400。
//   2520 西曼德妮 4915 两行（row2100 本体 / row2108 伙伴「黄金圣徒伙伴效果」，kind 同
//   [14,10]，纯 args 档位差 db[0]/db[2]/db[3]）：
//     row2100 args[40,1,4,0,1]／row2108 args[60,1,3,1,1]。行文三段：
//     ①「战斗开始时，自身进入事象观测；自身受到致死伤害时，退出事象观测以消除对手的
//       能力提升状态、回合类效果并复活」；
//     ②「自身登场之后，记录当回合自身首个进入的异常状态和对手所使用技能的类型，记录
//       成功则当回合结束时解除自身所处的异常状态；自身的技能命中时，若类型与自身记录
//       的技能类型相同则附加自身记录的异常状态，未触发则令自身凝滞」；
//     ③「战斗阶段结束时，若对手处于异常状态则自身特攻、速度+1且下回合使用技能时附加
//       特攻值40%（row2108:60%）的百分比伤害，若对手不处于异常状态则恢复自身最大体力
//       的1/4（row2108:1/3 且下回合技能先制+1）」。
//
// ⚠️ 划界/口径记档（majority reading，按 K409/K406/K405 板行-段位对齐先例）：
//   1.【2520 段③跳过】板行 2520 通道只列「致死拦截+复活 / 退出时消强消回合 /
//     「记录当回合」账本」三件，段③（特攻速度+1/附加特攻值%百分比伤害/回血 1|1/3/
//     先制+1）板行未引 → 记档跳过不产码（K409 1398 燃血段/K406 1463 seg3 直系先例）；
//     其行参位 db[0..4] 解码备档不消费。
//   2.【2514 窗语义】「2回合内」自触发（自身用技=必然"先出手"分支）→ InRounds 口径
//     生效 [R, R+N-1]=[R, R+1]（continuousEffect.h 回合数窗口家族引擎级通用规则）：
//     until=roundCount+1，ROUND_START 重写条件 until>0 && until>=roundCount（0 键=
//     无窗，防 K402 首回合 roundCount=0 撞车坑）；ws 两项（must_crit/damage_reduce_add）
//     每回合 reset → RS 在窗内重写（2492 形）；两项免疫票（HEAL_BLOCK/STAT_CLEAR）
//     触发当回合授 duration=2 + RS 窗内重授 duration=1（K405 坑：ROUND_START 授
//     duration=1 是安全形）。③无视提升**不走 RS 重写**：ws.boost_pierced 是
//     defender 索引+行动末清的"本次命中"旗标（e_ignore_boost.cpp E11 定稿口径，
//     battleFsm.cpp:1960/:2101 行动末清）→ 按 486 先例用 FIRST/SECOND_BEFORE_SKILL_HIT
//     双桶窗内每次出手 tick 写 [opp] 旗标（先于同行动 ATTACK_DAMAGE 消费）。
//   3.【2514 触发时点】「自身使用技能时」=EVENT_SKILL_USED（E13 锚：miss/被封也算
//     使用，K409 1006② 同窗同门）；watcher drain 在 State 桶后（emit 只入队）→ 触发
//     当回合的必暴不覆盖本刀（暴击掷点在 query_usage ①.5，先于 drain）——窗的必暴
//     实际覆盖下一整回合，场景按此断言（记档）。触发项数账本=每次触发事件 +1（两行
//     行文均「+1」字面，非 +项数），「本次在场」=每次入场重置（OPERATION_ENTER_EXIT_
//     STAGE 清账）；账本无行内消费方，落 plugin_storage 供审计（K662）。
//   4.【2514 roll】随机去重选 N 项（1314「随机 3 项-1」rand%6 去重循环同款，sbs_rand
//     唯一入口）；选中集 bitmask 落审计槽 base+2，场景按 mask 白盒断言（K662/F9 782）。
//   5.【2514 ②封回血通道】HEAL_BLOCK 免疫票挂对手（K402 1561 同款，coverage_all 闭环）
//     ——原语拦 heal_impl 全通道，字面「通过自身技能恢复体力」的窄口径（仅技能回血）
//     引擎无分通道查询点 → 按既有票通道全封（majority，记档）。
//   6.【2514 ④提升锁通道】STAT_CLEAR 免疫票挂自身（rule_center.h effect 1960 同通道：
//     只护正等级不被消除/吸取）——字面「无法被消除」。
//   7.【2520 致死拦截面】EVENT_HP_EXHAUSTED 预死亡信号（伤害通道落血≤0 即广播，回调
//     内写血自然否决死亡漏斗）；秒杀通道（EVENT_HP_TO_ZERO）与体力修正通道**不在本
//     信号面**（event_center.h 既定口径，记档）。复活血量：官方未给 → 「重生=满状态」
//     通例回满（K11 304/K384 1725 记档直系）；观测退出后死亡登记复位由 revive_pet
//     原语承担。事象观测=战斗开始时进入（enter 且 roundCount==0，字面「战斗开始时」
//     ——再登场不重入，记档）；退出即永久（obs_consumed 一次性，无「每场一次」字样
//     但观测已退出，字面自洽）。
//   8.【2520 记录账本】「当回合」=逐回合记录：自身首个异常=EVENT_ANOMALY_APPLIED
//     watcher（target=own，每回合首条，amount=落地异常 id）；对手技能类型=命中消费点
//     现读 roundChoice → skills[idx].type（选择期即定）+ skill_use_seq 增量判「本回合
//     用过技」（skill_use_seq 门=k382 私副本同款）。⚠️ **不走 ws.skill_type_view**：
//     属性技无伤害管线、视图不物化（默认 0=物理会把属性技误判同型——T7725e 首跑
//     实测，记档）。⚠️ battleFsm.cpp:265 易燃→烧伤转化路径 emit 的 APPLIED 事件缺
//     amount 载荷（默认 0=麻痹 id）——记录时 amount==0 且自身无麻痹而有烧伤 → 改记
//     烧伤（载荷补丁，记档）。
//   9.【2520 命中对账】FIRST/SECOND_ON_SKILL_HIT 双桶同 handler（攻击/属性技都落桶，
//     k382 ③同位；自身每回合只在己方动作桶命中一次）；类型相同 → 附加记录的异常
//     （100% 申报短路不耗 rand，duration=random_anomaly_duration()=2，K382 同式）；
//     未触发（无异常记录或对手本回合未用技或类型不同）→ 自身凝滞（Stasis=32，同参）。
//     自加凝滞自身也会被 APPLIED watcher 记为当回合异常 → 段②回合末解除把它一并清掉
//     （字面「自身首个进入的异常状态」含自附加，记档）。
//   10.【2520 回合末解除】「解除自身所处的异常状态」= dispel_active_anomalies(self)
//     全清（字面无「记录的那个」限定词；触发条件=当回合有异常记录成功）。
//   11.【boss 尾注】2514「（boss无效）」行尾全句尾注、2520 同 → 全节点 boss_invalid=
//     true（K409 记档3 口径）；enter 早挂的 watcher 不随节点走 → 回调内手动
//     is_boss_challenge 双保险（K391 口径）。
//
// 三查记录（a01-k412 2026-11-04，worktree 内复核）：
//   ①内核 grep -w 2514/2520 零命中（无魂印行为硬编码）；
//   ②effect_icon kind=[1,3,5]/[14,10] 已核（dump 原文）；effect_info 未查得同文魂印
//     模板，tips 权威（SOP §六.1 惯例）；
//   ③soul_lib 全库无 SOUL_PROGRAM/SOUL_MARK 二 id；存储键段 1025140000/1025200000
//     grep 全库无撞（golden 链哈希同形串非键段）——真缺口。
// 注册：SOUL_PROGRAM ×2（2514/2520 入账，只增不改）。
// 场景回归：test/scenario/scenario_7720_random_pool_observation.cpp（7720-7729）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMark2514 = 2514;
constexpr int kMark2520 = 2520;

constexpr int kOwnerStride = 16;   // 每 owner 键段步长（槽 0..15，K402/K404/K406/K409 同款）

// ── 存储键段（10^9 + id×10^4 + owner*16 + 槽；全库 grep 无撞）─────────────────
constexpr int k2514Base = 1025140000;
constexpr int k2514Wired = 0;      // SKILL_USED watcher 接线旗（防再登场双挂）
constexpr int k2514Ledger = 1;     // 触发项数账本（本次在场，入场清零）
constexpr int k2514MaskAudit = 2;  // 审计槽：最近一次触发的池项 bitmask（K662）
constexpr int k2514Until0 = 3;     // 池项 i 的窗界 until（=触发回合+1；0=无窗），i=0..5
constexpr int k2514SidHeal = 9;    // HEAL_BLOCK 票句柄（重授复用）
constexpr int k2514SidStat = 10;   // STAT_CLEAR 票句柄（重授复用）

constexpr int k2520Base = 1025200000;
constexpr int k2520Wired = 0;        // watcher 接线旗（一场一次）
constexpr int k2520Obs = 1;          // 事象观测中（1=观测中）
constexpr int k2520ObsUsed = 2;      // 观测已退出（一次性，永久）
constexpr int k2520RecAnom = 3;      // 当回合记录的异常 id +1（0=未记录；+1 避 0=麻痹歧义）
constexpr int k2520RecRound = 4;     // 记录发生回合

// 免疫票 source_id（RuleCenter 句柄复用键；9902514xx 段全库 grep 无撞）
constexpr int k2514SidHealBase = 990251410;
constexpr int k2514SidStatBase = 990251440;

// ── 共用小件（HostRow/db_at 原序读 owned_int_args，K400/K404/K406/K409 定例）──

struct HostRow {
    const std::vector<int>* db = nullptr;
    bool valid = false;
};

HostRow host_row(BattleContext* ctx, const SoulNodeArgs& a, int mark_id) {
    HostRow row;
    if (!ctx) {
        return row;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return row;
    }
    int slot = a.host_slot;
    if (slot < 0 || slot > 5) {
        slot = ctx->find_pet_with_soulmark(owner, mark_id);
    }
    if (slot < 0 || slot > 5) {
        return row;
    }
    row.db = &ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    row.valid = true;
    return row;
}

int db_at(const HostRow& row, int idx, int fallback) {
    if (row.db && idx >= 0 && idx < static_cast<int>(row.db->size())) {
        return (*row.db)[idx];
    }
    return fallback;
}

// 回调内 live 读行参（K404 零 arm 期快照口径）。
const std::vector<int>* live_row_db(BattleContext* ctx, int owner, int mark_id) {
    if (!ctx || owner < 0 || owner > 1 || ctx->on_stage[owner] < 0
        || ctx->on_stage[owner] > 5) {
        return nullptr;
    }
    const ElfPet& host = ctx->seerRobot[owner].elfPets[ctx->on_stage[owner]];
    if (host.soulMark.id != mark_id) {
        return nullptr;
    }
    return &host.soulMark.args.owned_int_args;
}

int live_db_at(const std::vector<int>* db, int idx, int fallback) {
    if (db && idx >= 0 && idx < static_cast<int>(db->size())) {
        return (*db)[static_cast<std::size_t>(idx)];
    }
    return fallback;
}

int read_int(BattleContext* ctx, int key) {
    const auto it = ctx->plugin_storage.find(key);
    return it == ctx->plugin_storage.end() ? 0 : any_int_or(it->second, 0);
}

void write_int(BattleContext* ctx, int key, int v) {
    ctx->plugin_storage[key] = v;
}

void erase_int(BattleContext* ctx, int key) {
    ctx->plugin_storage.erase(key);
}

// ════════════════════ 2514 西西弗 · 随机触发池 ═══════════════════════════════

// 池项窗界键 = base + owner*stride + k2514Until0 + item。
inline int until_key(int base_owner, int item) {
    return base_owner + k2514Until0 + item;
}

// 窗内判定（0 键=无窗；防首回合 roundCount=0 撞车，K402 戳 0 守卫同款）。
inline bool window_active(BattleContext* ctx, int base_owner, int item) {
    const int until = read_int(ctx, until_key(base_owner, item));
    return until > 0 && until >= ctx->roundCount;
}

// 池项即时应用（触发点/RS 重写共用）。③不走此函数——boost_pierced 是 defender
// 索引+行动末清的本次命中旗标，由 BEFORE_SKILL_HIT 双桶 tick 独立写（记档2）。
void apply_pool_item(BattleContext* ctx, int owner, int item, int reduce_pct,
                     int ticket_duration) {
    if (!ctx || owner < 0 || owner > 1) {
        return;
    }
    const int base = k2514Base + owner * kOwnerStride;
    switch (item) {
        case 0:   // ① 2回合内自身攻击必定打出致命一击
            ctx->ws.must_crit[owner] = true;
            break;
        case 1: {  // ② 2回合内对手无法通过自身技能恢复体力（HEAL_BLOCK 票挂对手）
            if (!g_core) {
                break;
            }
            const int sid = read_int(ctx, base + k2514SidHeal);
            const int new_sid = ctx->grant_immunity(
                /*owner=*/1 - owner, ImmunityType::HEAL_BLOCK, coverage_all(),
                /*anomaly_mask=*/0, /*duration_rounds=*/ticket_duration,
                /*source_id=*/sid, /*soul_immunity=*/false, EffectScope::TEAM,
                /*counts=*/0, /*source_effect_id=*/kMark2514, ImmunityTier::Modern);
            write_int(ctx, base + k2514SidHeal, new_sid);
            break;
        }
        case 3: {  // ④ 2回合内自身能力提升状态无法被消除（STAT_CLEAR 票挂自身）
            const int sid = read_int(ctx, base + k2514SidStat);
            const int new_sid = ctx->grant_immunity(
                /*owner=*/owner, ImmunityType::STAT_CLEAR, coverage_all(),
                /*anomaly_mask=*/0, /*duration_rounds=*/ticket_duration,
                /*source_id=*/sid, /*soul_immunity=*/false, EffectScope::TEAM,
                /*counts=*/0, /*source_effect_id=*/kMark2514, ImmunityTier::Modern);
            write_int(ctx, base + k2514SidStat, new_sid);
            break;
        }
        case 4: {  // ⑤ 2回合内受到的攻击伤害减少 N%（加算减伤槽，正值=减免）
            int(&reduce)[4] = ctx->ws.damage_reduce_add[owner];
            for (int i = 0; i < 4; ++i) {
                if (reduce[i] == 0) {
                    reduce[i] = reduce_pct;
                    break;
                }
            }
            break;
        }
        default:
            break;   // ⑥ 回合末回血在 ROUND_END 节点处理；③ 见 pierce tick
    }
}

// ③ 无视提升 tick（FIRST/SECOND_BEFORE_SKILL_HIT 双桶）：窗内每次出手写
// ws.boost_pierced[opp]=true（defender 索引，本次命中语义，486 先例）——先于同行动
// ATTACK_DAMAGE 消费，行动末随内核清。
EffectResult k412_2514_pierce_tick(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMark2514)) {
        return EffectResult::kOk;
    }
    if (!window_active(ctx, k2514Base + owner * kOwnerStride, /*item=*/2)) {
        return EffectResult::kOk;
    }
    ctx->ws.boost_pierced[1 - owner] = true;
    return EffectResult::kOk;
}

// ROUND_START：窗内池项重写（ws 三项每回合 reset；票重授 duration=1，K405 安全形）。
EffectResult k412_2514_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMark2514)) {
        return EffectResult::kOk;
    }
    const int base = k2514Base + owner * kOwnerStride;
    const int reduce_pct = live_db_at(live_row_db(ctx, owner, kMark2514), 2, 30);
    for (int item = 0; item <= 5; ++item) {
        if (window_active(ctx, base, item)) {
            apply_pool_item(ctx, owner, item, reduce_pct, /*ticket_duration=*/1);
        }
    }
    return EffectResult::kOk;
}

// ROUND_END：⑥ 每回合战斗阶段结束时恢复自身 db[3] 点体力（窗内）。
EffectResult k412_2514_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMark2514)) {
        return EffectResult::kOk;
    }
    const int base = k2514Base + owner * kOwnerStride;
    if (!window_active(ctx, base, 5)) {
        return EffectResult::kOk;
    }
    const int heal = live_db_at(live_row_db(ctx, owner, kMark2514), 3, 300);
    if (heal > 0 && ctx->getPet(owner).hp > 0) {
        (void)g_core->heal_amount(ctx, owner, heal);
    }
    return EffectResult::kOk;
}

// enter（early）：SKILL_USED watcher 接线（一场一次）+ 触发项数账本入场清零
//（「本次在场」——OPERATION_ENTER_EXIT_STAGE 入/出场都过此节点，出场清同值无害）。
EffectResult k412_2514_enter(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const int base = k2514Base + owner * kOwnerStride;
    erase_int(ctx, base + k2514Ledger);   // 触发项数账本入场清零（记档3）
    if (read_int(ctx, base + k2514Wired) == 1) {
        return EffectResult::kOk;
    }
    write_int(ctx, base + k2514Wired, 1);
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c) {
                return;
            }
            if (ev.actor != owner) {
                return;   // 自身使用技能（E13：miss/被封也算使用）
            }
            if (c->is_boss_challenge) {
                return;   // boss 无效（watcher 手动门，K391/K409 口径）
            }
            if (!onstage_has_soul_mark(c, owner, kMark2514)) {
                return;
            }
            const std::vector<int>* db = live_row_db(c, owner, kMark2514);
            const int count = live_db_at(db, 0, 1);      // db[0]：触发项数（1/2 档位差）
            const int reduce_pct = live_db_at(db, 2, 30);  // db[2]：⑤减伤%
            if (count <= 0) {
                return;
            }
            const int base = k2514Base + owner * kOwnerStride;
            // 随机去重选 N 项（1314 rand%6 去重循环同款；使用本对局 RNG）
            bool picked[6] = {false, false, false, false, false, false};
            int picked_count = 0;
            for (int tries = 0; tries < 256 && picked_count < count; ++tries) {
                const int idx = choose_soul_range(*c, kMark2514, 0, owner, owner, 0, 5);
                if (picked[idx]) {
                    continue;
                }
                picked[idx] = true;
                ++picked_count;
            }
            // 去重循环兜底（256 次不中的概率可忽略）：顺序补齐（确定性，记档）
            for (int idx = 0; picked_count < count && idx < 6; ++idx) {
                if (!picked[idx]) {
                    picked[idx] = true;
                    ++picked_count;
                }
            }
            // 窗界落账 + 即时应用（触发当回合余段生效；必暴因 drain 时点实际覆盖
            // 下一整回合——记档3）
            int mask = 0;
            for (int item = 0; item < 6; ++item) {
                if (!picked[item]) {
                    continue;
                }
                mask |= (1 << item);
                write_int(c, until_key(base, item), c->roundCount + 1);
                apply_pool_item(c, owner, item, reduce_pct, /*ticket_duration=*/2);
            }
            write_int(c, base + k2514MaskAudit, mask);   // 审计槽（K662）
            write_int(c, base + k2514Ledger,
                      read_int(c, base + k2514Ledger) + 1);   // 触发项数+1（记档3）
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ════════════════════ 2520 西曼德妮 · 事象观测 ═══════════════════════════════

// 对手本回合是否用过技（判据统一 k382 私副本——skill_use_seq 增量）。
bool foe_executed_this_round(const BattleContext* ctx, int opp) {
    if (!ctx || opp < 0 || opp > 1) {
        return false;
    }
    return ctx->skill_use_seq[opp] > ctx->ws.skill_use_seq_round_start[opp];
}

// ⚠️ 技能类型读法：roundChoice → skills[idx].type（选择期即定）——**不用**
// ws.skill_type_view：属性技无伤害管线、视图不物化（默认 0=物理会被误判同型，
// T7725e 首跑实测）。side 已用技（SELECT_SKILL）返回其类型，否则 -1。
int used_skill_type(BattleContext* ctx, int side) {
    if (!ctx || side < 0 || side > 1) {
        return -1;
    }
    if (ctx->roundChoice[side][0] != 1 /*SELECT_SKILL*/) {
        return -1;
    }
    const int idx = ctx->roundChoice[side][1];
    if (idx < 0 || idx >= 5) {
        return -1;
    }
    return static_cast<int>(ctx->getPet(side).skills[idx].type);
}

// enter（early）：双 watcher 接线（一场一次）+ 战斗开始时进入事象观测（记档7：
// 字面「战斗开始时」→ 仅 battle 起始回合的入场置位，再登场不重入）。
EffectResult k412_2520_enter(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const int base = k2520Base + owner * kOwnerStride;
    if (ctx->roundCount == 0 && read_int(ctx, base + k2520ObsUsed) == 0) {
        write_int(ctx, base + k2520Obs, 1);   // 战斗开始时进入事象观测
    }
    if (read_int(ctx, base + k2520Wired) == 1) {
        return EffectResult::kOk;
    }
    write_int(ctx, base + k2520Wired, 1);
    // ① 致死拦截 watcher：退出观测 + 消对手强/断对手回合 + 复活（回满，K11 304 通例）。
    ctx->event_center_.register_watcher(
        EventType::EVENT_HP_EXHAUSTED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c || !g_core || !g_core->revive_pet || !g_core->clear_stat_boosts
                || !g_core->break_round_effects) {
                return;
            }
            if (ev.target != owner) {
                return;
            }
            if (c->is_boss_challenge) {
                return;   // boss 无效（watcher 手动门）
            }
            if (!onstage_has_soul_mark(c, owner, kMark2520)) {
                return;
            }
            const int base = k2520Base + owner * kOwnerStride;
            if (read_int(c, base + k2520Obs) != 1) {
                return;   // 仅观测中拦截；退出后不再有观测可退（一次性）
            }
            write_int(c, base + k2520Obs, 0);       // 退出事象观测
            write_int(c, base + k2520ObsUsed, 1);
            const int opp = c->opponent(owner);
            g_core->clear_stat_boosts(c, opp);       // 消除对手的能力提升状态
            g_core->break_round_effects(c, opp);     // 断对手回合类效果（K396 1040 同式）
            const int slot = c->on_stage[owner];
            if (slot >= 0 && slot <= 5) {
                (void)g_core->revive_pet(c, owner, slot,
                                         resolve_pet_max_hp(c->getPet(owner)));
            }
        },
        WatcherScope::TEAM);
    // ② 记录账本 watcher：自身首个进入的异常（每回合首条；易燃转化缺载荷补丁，记档8）。
    ctx->event_center_.register_watcher(
        EventType::EVENT_ANOMALY_APPLIED, owner, ctx->roundCount,
        /*duration_rounds=*/0, /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c || !g_core || !g_core->has_active_anomaly) {
                return;
            }
            if (ev.target != owner) {
                return;
            }
            if (!onstage_has_soul_mark(c, owner, kMark2520)) {
                return;
            }
            const int base = k2520Base + owner * kOwnerStride;
            if (read_int(c, base + k2520RecRound) == c->roundCount
                && read_int(c, base + k2520RecAnom) > 0) {
                return;   // 本回合已记录「首个」
            }
            int anomaly = ev.amount;
            if (anomaly == 0
                && !g_core->has_active_anomaly(c, owner,
                                               static_cast<int>(AbnormalStatusId::Paralysis))
                && g_core->has_active_anomaly(c, owner,
                                              static_cast<int>(AbnormalStatusId::Burn))) {
                anomaly = static_cast<int>(AbnormalStatusId::Burn);   // 易燃转化载荷补丁
            }
            write_int(c, base + k2520RecAnom, anomaly + 1);   // +1：0=未记录
            write_int(c, base + k2520RecRound, c->roundCount);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// 命中对账（FIRST/SECOND_ON_SKILL_HIT 双桶）：类型相同 → 附加记录的异常；未触发 →
// 自身凝滞（记档9）。100% 申报短路不耗 rand（K382 同式）。
EffectResult k412_2520_on_skill_hit(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !g_core
        || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMark2520)) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(owner).hp <= 0) {
        return EffectResult::kOk;
    }
    const int base = k2520Base + owner * kOwnerStride;
    const int opp = 1 - owner;
    const int rec_plus = read_int(ctx, base + k2520RecAnom);
    const int rec_round = read_int(ctx, base + k2520RecRound);
    const bool rec_active = rec_plus > 0 && rec_round == ctx->roundCount;
    bool matched = false;
    if (rec_active && foe_executed_this_round(ctx, opp)) {
        const int rec_anomaly = rec_plus - 1;
        const int rec_type = used_skill_type(ctx, opp);
        const int own_type = used_skill_type(ctx, owner);
        if (rec_type >= 0 && own_type >= 0 && rec_type == own_type) {
            matched = true;
            (void)g_core->apply_anomaly_checked(
                ctx, opp, rec_anomaly, random_anomaly_duration(), owner,
                /*chance_pct=*/100, ChanceSource::SoulMark);
        }
    }
    if (!matched) {
        // 未触发 → 令自身凝滞（Stasis=32；自附加会被记录账本捕获、回合末解除，记档9）
        (void)g_core->apply_anomaly_checked(
            ctx, owner, static_cast<int>(AbnormalStatusId::Stasis),
            random_anomaly_duration(), owner, /*chance_pct=*/100,
            ChanceSource::SoulMark);
    }
    return EffectResult::kOk;
}

// ROUND_END：记录成功（当回合有异常记录）→ 解除自身所处的异常状态（全清，记档10）
// + 记录账本回合界复位。
EffectResult k412_2520_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !g_core
        || !g_core->dispel_active_anomalies) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMark2520)) {
        return EffectResult::kOk;
    }
    const int base = k2520Base + owner * kOwnerStride;
    const int rec_plus = read_int(ctx, base + k2520RecAnom);
    const int rec_round = read_int(ctx, base + k2520RecRound);
    if (rec_plus > 0 && rec_round == ctx->roundCount && ctx->getPet(owner).hp > 0) {
        (void)g_core->dispel_active_anomalies(ctx, owner);
    }
    erase_int(ctx, base + k2520RecAnom);    // 记录账本回合界复位
    erase_int(ctx, base + k2520RecRound);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +2（2514/2520 入账，只增不改）──────
// tips 尾注（boss无效）→ 全节点 boss_invalid=true（记档11）。
SOUL_PROGRAM(kMark2514, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k412_2514_enter,
                              /*once=*/false, /*early=*/true, SoulScope::STAGE,
                              /*boss_invalid=*/true),
                         SOUL_NODE(State::BATTLE_ROUND_START, &k412_2514_round_start,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE,
                              /*boss_invalid=*/true),
                         SOUL_NODE(State::BATTLE_ROUND_END, &k412_2514_round_end,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE,
                              /*boss_invalid=*/true),
                         SOUL_NODE(State::BATTLE_FIRST_BEFORE_SKILL_HIT, &k412_2514_pierce_tick,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE,
                              /*boss_invalid=*/true),
                         SOUL_NODE(State::BATTLE_SECOND_BEFORE_SKILL_HIT, &k412_2514_pierce_tick,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE,
                              /*boss_invalid=*/true)});
SOUL_PROGRAM(kMark2520, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k412_2520_enter,
                              /*once=*/false, /*early=*/true, SoulScope::STAGE,
                              /*boss_invalid=*/true),
                         SOUL_NODE(State::BATTLE_FIRST_ON_SKILL_HIT, &k412_2520_on_skill_hit,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE,
                              /*boss_invalid=*/true),
                         SOUL_NODE(State::BATTLE_SECOND_ON_SKILL_HIT, &k412_2520_on_skill_hit,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE,
                              /*boss_invalid=*/true),
                         SOUL_NODE(State::BATTLE_ROUND_END, &k412_2520_round_end,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE,
                              /*boss_invalid=*/true)});
