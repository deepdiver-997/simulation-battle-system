// K395 冰封/焚烬条件触发族首批 —— 6 行主批 + 1676 同机制并做（7 宏）
//（2026-10-30 夜跑 a01-kice，worktree ../agents/k395；号段 7490-7499 板行权威）
//
// 行清单（tools/soul_effects_dump.py 698 881 1126 1218 1258 1394 1676，2026-10-30）：
//   698  row374  落樱飞雪 2995 args[20,15,10,1]
//     「攻击20%使对手冰封,对手每处于一种能力提升状态,冰封概率提升10%（boss无效）」
//     → db0=基础20% db1=冰封码15（args[1] 异常码与行文互锚，消费）db2=每提升+10%
//       db3=1 旗（不消费，记档）。
//   881  row597  寒凌 3269 args[50,1,2,1]
//     「先出手时有50%的概率使对手冰封，未触发则自身下1次技能先制+2（BOSS无效）」
//     → db0=50% db1=1 旗（不消费，记档）db2=先制+2 db3=1 旗（不消费，记档）。
//   1126 row842  风凌雪 3533 args[40,10,70,1,1]
//     「使用攻击技能40%的概率使对手冰封，未触发则下次冰封的概率提升10%，最高70%；
//       使用属性技能则自身下回合先制+1（BOSS无效）」
//     → db0=基础40% db1=递增步10 db2=上限70 db3=先制+1（与行文互锚，消费）
//       db4=1 旗（不消费，记档）。
//   1218 row929  伽楼儿 3638 args[50,22,50,50,1]
//     「自身受到攻击时50%的概率令对手焚烬，未触发则消除对手回合类效果；回合开始时
//       若自身体力低于对手则自身造成的伤害提升50%且受到的伤害减少50%（boss无效）」
//     → db0=50% db1=焚烬码22（互锚，消费）db2=增伤50% db3=减伤50% db4=1 旗（不消费）。
//   1258 row966  月玲珑 3681 args[25,25,15,1]
//     「25%的概率免疫本回合受到的攻击伤害，若自身体力低于对手则概率翻倍；自身攻击
//       技能25%的概率令对手冰封，若自身体力低于对手则概率翻倍（boss无效）」
//     → db0=免伤25% db1=冰封25% db2=冰封码15（互锚，消费）db3=1 旗（不消费）。
//   1394 row1086 狞 3811 args[25,50,1]
//     「自身攻击技能有25%的概率令对手焚烬，自身体力低于对手时概率翻倍；自身受到
//       攻击伤害有50%的概率令对手烧伤，自身体力低于对手时概率翻倍（boss无效）」
//     → db0=攻→焚烬25% db1=受击→烧伤50% db2=1 旗（不消费）。
//   1676 row1326 冰·寒凌 4084 args[50,1,2,50,1,2,1]（881 同机制异 id，板行明示
//     "可并做，防撞车误判"——同文件落地，单独注册）
//     「若当回合自身技能先出手则50%的概率令对手冰封，未触发则使自身下1次技能先制+2；
//       若当回合对手技能先出手则50%的概率令对手害怕，未触发则使对手下1次技能先制-2
//       （boss无效）」
//     → db0=己先手冰封50% db1=1 旗 db2=己先制+2 db3=敌先手害怕50% db4=1 旗
//       db5=敌先制-2 db6=1 旗（不消费）。
//
// ⚠️ 三查记录（2026-10-30，a01-kice）：
//   ①内核 grep 698/881/1126/1218/1258/1394/1676：仅 skill 空间同号（skills.cpp
//     881/1218 条件先制模板、abnormal-types.h 1676 焚烬引用注记）——魂印行为零硬编码；
//   ②effect_info 七个 id 均为技能空间异文模板（698"击败对手对方下只出战…"等），
//     魂印语义权威 = effect_icon tips（dump 原文见上）；
//   ③registry soul 基线 340 无七 id、soul_lib 全库 grep SOUL_PROGRAM 无同号——真缺口。
//
// 口径记档（歧义按先例 majority reading 落地）：
//   [M1]「未触发」补偿档 = ROLL_FAILED 单档（K172 808 / K232 834 先例）：目标免疫/
//     抗性/弹控不算"未触发"——官方补偿语义锚"概率没过门"（battle_primitives.h
//     ROLL_FAILED 注记）。K241 级联族按"未落地"是级联特例，不采。
//   [M2] 1126 爬坡回落：触发落地后概率回落基础档 db0（"未触发则下次提升"的对称读法；
//     K662 爬坡按已损体力现值重算无此问题，本行是状态型爬坡）。未触发=ROLL_FAILED
//     才爬坡（M1 同源）。
//   [M3] 881/1676「先出手时」= E13(SKILL_USED) + is_second_mover(owner)==false：
//     E13 在 execute 起点发射，先手权已结算（preemptive_right 是 FMR 入口清零后
//     本回合定值）；「使用」口径 = E13 即触发（miss/被盔也算，2026-10-03 裁定，
//     K177 同款）。1676「对手技能先出手」= ev.actor==opp && !is_second_mover(opp)。
//   [M4]「下1次技能先制±N」= pending 账（plugin_storage，"下1次"无回合过期）：
//     次回合 FMR 桶消费 ws.preemptive_level±N 一次即清（memset 后写，K394 ③
//     "受击下回合先制+1/+2 FMR 消费"/K189 直系；hamo_3809_ao 对手侧 preemptive
//     写同桶先例）。宿主不在场不消费（pending 保留到再上场用技，"下1次技能"字面）。
//   [M5] 低血翻倍 = 自身当前体力 < 对手当前体力（触发时点现值，K377 体力对照门
//     同款读法；"低于"严格不等号）。1258 免伤翻倍在 BLOCK 相位现值判。
//   [M6] 1218 ②「造成的伤害提升50%」= ws.damage_add_pct（FMR 桶写，通用增伤
//     钦定通道 battleContext.h:1604；ws 每回合 memset → ROUND_START 写会被清，
//     必须 FMR 写——K245/K391 记档）；「受到的伤害减少50%」= REDUCE_PCT+MITIGATE
//     红伤票（"受到的伤害"措辞 = K1 3/280 同款仅红伤，k_panel_family 口径），
//     回调实时读 RS 低体门旗（k390 mode 账本形）。
//   [M7] 1258 ①「25%的概率免疫本回合受到的攻击伤害」= 逐次受击 BLOCK 相位掷点
//     （K180 608 免疫形直系；"本回合受到的攻击伤害"读作免疫"本次受到的攻击伤害"，
//     概率随每次受击独立掷——majority reading，免伤族无逐回合预掷先例）。
//   [M8] 概率递增/申报全部走审计槽（F9 782/K662 口径）：各宏把本次申报概率写
//     plugin_storage（99395 段，grep 无撞），场景无随机精确断言 + 统计带侧证
//     （落地计数键，bounded for 循环，禁 while(!FINISHED)——2026-10-26 记档）。
//   [M9] boss 尾注：七行全部全句尾注（boss无效）→ 全节点 boss_invalid=true
//     （K193 惯例，boss 对局节点不注册不执行）。
//
// 先例：k193_cond_freeze 827（DETECT 申报通道/HostRow db_at 方案 B）、k241 804
//   （TAKE_DAMAGE 受击窗+审计槽）、k390 1523（焚烬=22/RS mode 账本/FMR 消费）、
//   k399（break_round_effects 消回合）、k_anomaly_chance 662（审计槽爬坡）、
//   k_dual_domain（FMR pending 消费窗）、hamo_3809_ao（对手 preemptive 写）、
//   k_lowhit_immune_reflect 608（BLOCK 免伤形）、k_guard_family（REDUCE_PCT 票）。
// 场景回归：test/scenario/scenario_7490_iceburn_698.cpp 等 7490-7496 七文件。

#include "common.h"

#include <abnormal-system/abnormal-types.h>
#include <primitives/battle_primitives.h>

namespace {

constexpr int kIcebound = static_cast<int>(AbnormalStatusId::Icebound);      // 15
constexpr int kIncinerate = static_cast<int>(AbnormalStatusId::Incinerate);  // 22
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);              // 2
constexpr int kFear = static_cast<int>(AbnormalStatusId::Fear);              // 6

// ── 审计/账本键位（99395 段：+owner；grep 全库无撞，M8）───────────────────────
constexpr int k698WireBase = 993950000;    // 698 watcher 接线旗（一场一次）
constexpr int k698AuditBase = 993951000;   // 698 本次申报概率
constexpr int k698StatBase = 993951010;    // 698 落地计数（统计带侧证）
constexpr int k881AuditBase = 993952000;   // 881 本次申报概率
constexpr int k881StatBase = 993952010;    // 881 落地计数
constexpr int k881PendBase = 993952020;    // 881 下1次先制+db2 pending（0=无）
constexpr int k881WireBase = 993952030;    // 881 watcher 接线旗
constexpr int k1126RampBase = 993953000;   // 1126 现行爬坡档（跨回合账本）
constexpr int k1126AuditBase = 993953010;  // 1126 本次申报概率
constexpr int k1126StatBase = 993953020;   // 1126 落地计数
constexpr int k1126PendBase = 993953030;   // 1126 下回合先制+db3 pending
constexpr int k1126WireBase = 993953040;   // 1126 watcher 接线旗
constexpr int k1218ModeBase = 993954000;   // 1218 RS 低体门旗（1=低体）
constexpr int k1218WireBase = 993954030;   // 1218 watcher 接线旗（与 mode 分键）
constexpr int k1218AuditBase = 993954010;  // 1218 焚烬申报概率
constexpr int k1218StatBase = 993954020;   // 1218 焚烬落地计数
constexpr int k1258DodgePctBase = 993955000;  // 1258 免伤申报概率
constexpr int k1258DodgeHitBase = 993955010;  // 1258 免伤落地旗（1/0）
constexpr int k1258DodgeStatBase = 993955020; // 1258 免伤落地计数
constexpr int k1258IceAuditBase = 993955030;  // 1258 冰封申报概率
constexpr int k1258IceStatBase = 993955040;   // 1258 冰封落地计数
constexpr int k1258WireBase = 993955050;      // 1258 watcher 接线旗
constexpr int k1394IncAuditBase = 993956000;  // 1394 焚烬申报概率
constexpr int k1394IncStatBase = 993956010;   // 1394 焚烬落地计数
constexpr int k1394BurnAuditBase = 993956020; // 1394 烧伤申报概率
constexpr int k1394BurnStatBase = 993956030;  // 1394 烧伤落地计数
constexpr int k1394Wire1Base = 993956040;     // 1394 攻击支 watcher 接线旗
constexpr int k1394Wire2Base = 993956050;     // 1394 受击支 watcher 接线旗
constexpr int k1676SelfAuditBase = 993957000; // 1676 己先手支申报概率
constexpr int k1676SelfStatBase = 993957010;  // 1676 己先手支落地计数
constexpr int k1676SelfPendBase = 993957020;  // 1676 己下1次先制+db2 pending
constexpr int k1676OppAuditBase = 993957030;  // 1676 敌先手支申报概率
constexpr int k1676OppStatBase = 993957040;   // 1676 敌先手支落地计数
constexpr int k1676OppPendBase = 993957050;   // 1676 敌下1次先制-db5 pending
constexpr int k1676WireBase = 993957060;      // 1676 watcher 接线旗

constexpr int kEpochBase = 993958000;      // 票接线 epoch 防重：+宏内偏移*100+owner

// ── 共用小件（db_at 原序读 = 方案 B，K193/K384 定例）────────────────────────

struct HostRow {
    const std::vector<int>* db = nullptr;
    int monster_id = 0;
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
    const ElfPet& host = ctx->seerRobot[owner].elfPets[slot];
    row.db = &host.soulMark.args.owned_int_args;
    row.monster_id = host.soulMark.monster_id;
    row.valid = true;
    return row;
}

int db_at(const HostRow& row, int idx, int fallback) {
    if (row.db && idx >= 0 && idx < static_cast<int>(row.db->size())) {
        return (*row.db)[idx];
    }
    return fallback;
}

void write_int(BattleContext* ctx, int key, int v) {
    ctx->plugin_storage[key] = v;
}

int read_int(BattleContext* ctx, int key) {
    const auto it = ctx->plugin_storage.find(key);
    return it == ctx->plugin_storage.end() ? 0 : any_int_or(it->second, 0);
}

// 票接线 epoch 防重（k_guard_family already_armed 形，int 键简化版）：
// OPERATION_ENTER_EXIT_STAGE early 节点开局双触发/下场重入防堆叠。
bool ticket_armed_this_epoch(BattleContext* ctx, int key, int owner) {
    const int epoch = ctx->pipeline_valid_id[owner];
    const auto it = ctx->plugin_storage.find(key);
    if (it != ctx->plugin_storage.end() && any_int_or(it->second, -1) == epoch) {
        return true;
    }
    ctx->plugin_storage[key] = epoch;
    return false;
}

// TEAM watcher 一次接线旗（k241 形：一场一次，onstage 门在回调内）。
bool watcher_wired(BattleContext* ctx, int key) {
    if (ctx->plugin_storage.count(key)) {
        return true;
    }
    ctx->plugin_storage.emplace(key, 1);
    return false;
}

// 对手是否处于能力提升状态 + 处于提升的槽种数（引擎口径 = 正等级槽计数，K258 判据）。
int boost_slot_count(BattleContext* ctx, int side) {
    int n = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[side][i] > 0) {
            ++n;
        }
    }
    return n;
}

bool attack_damage_state(const BattleEvent& ev) {
    return ev.state == static_cast<int>(State::BATTLE_FIRST_ATTACK_DAMAGE)
        || ev.state == static_cast<int>(State::BATTLE_SECOND_ATTACK_DAMAGE);
}

bool anomaly_landed(ApplyAnomalyResult r) {
    return r == ApplyAnomalyResult::SUCCESS
        || r == ApplyAnomalyResult::DURATION_EXTENDED
        || r == ApplyAnomalyResult::REPLACED_EXISTING
        || r == ApplyAnomalyResult::CONVERTED;
}

// 申报异常 + 审计槽 + 落地计数（M8 通道，各宏共用一次内联）。
ApplyAnomalyResult declare_anomaly(BattleContext* ctx, int target, int anomaly_id,
                                   int actor, int pct, int audit_key, int stat_key) {
    write_int(ctx, audit_key, pct);
    const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
        ctx, target, anomaly_id, random_anomaly_duration(), actor, pct,
        ChanceSource::SoulMark);
    if (anomaly_landed(r) && stat_key > 0) {
        write_int(ctx, stat_key, read_int(ctx, stat_key) + 1);
    }
    return r;
}

// ══ effect 698 落樱飞雪：攻击 20% 冰封 + 对手每提升一种 +10% ═══════════════════
//（E13 攻击技窗 = K164 116 "攻击时X%烧伤"同窗；提升数现值读；无上限（行文无）。）

EffectResult k395_698_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->apply_anomaly_checked || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const HostRow row = host_row(ctx, a, 698);
    const int base = db_at(row, 0, -1);
    const int code = db_at(row, 1, kIcebound);   // args[1] 异常码互锚（缺失按冰封兜底）
    const int step = db_at(row, 2, -1);
    if (base <= 0 || step < 0 || code < 0) {
        return EffectResult::kOk;
    }
    if (watcher_wired(ctx, k698WireBase + owner)) {
        return EffectResult::kOk;   // 接线去重旗（一场一次，TEAM watcher 常驻）
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, base, code, step](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || !g_core || !g_core->apply_anomaly_checked) {
                return;
            }
            if (ev.actor != owner || ev.used_attribute) {
                return;   // 攻击技窗（声明类型载荷，K2 358/K164 同款）
            }
            if (!onstage_has_soul_mark(wctx, owner, 698)) {
                return;
            }
            const int opp = 1 - owner;
            const int pct = base + step * boost_slot_count(wctx, opp);
            declare_anomaly(wctx, opp, code, owner, pct, k698AuditBase + owner,
                            k698StatBase + owner);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ══ effect 881 寒凌 / 1676 冰·寒凌 己先手支共用体 ══════════════════════════════
//（881 全行 = 1676 第一分句：己先手 50% 冰封，未触发(ROLL_FAILED)→己下1次先制+N。）
// pending 消费节点（M4）：FMR 桶（memset 后写）一次消费即清。STAGE 节点引擎按
// epoch 注册/作废——宿主离场节点不跑，pending 天然保留到再上场（"下1次技能"字面）。

void consume_preempt_pending(BattleContext* ctx, int owner, int pend_key) {
    const int amount = read_int(ctx, pend_key);
    if (amount == 0) {
        return;
    }
    ctx->plugin_storage.erase(pend_key);
    if (amount > 0) {
        ctx->ws.preemptive_level[owner] += amount;
    }
}

// ══ effect 881 寒凌 ═════════════════════════════════════════════════════════════

EffectResult k395_881_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->apply_anomaly_checked || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const HostRow row = host_row(ctx, a, 881);
    const int pct = db_at(row, 0, -1);
    const int preempt = db_at(row, 2, -1);
    if (pct <= 0 || preempt < 0) {
        return EffectResult::kOk;
    }
    if (watcher_wired(ctx, k881WireBase + owner)) {
        return EffectResult::kOk;   // 接线去重（与 pending 分键）
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, pct, preempt](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || !g_core || !g_core->apply_anomaly_checked) {
                return;
            }
            if (ev.actor != owner || wctx->is_second_mover(owner)) {
                return;   // 先出手门（M3：E13 时点先手权已结算）
            }
            if (!onstage_has_soul_mark(wctx, owner, 881)) {
                return;
            }
            const ApplyAnomalyResult r = declare_anomaly(
                wctx, 1 - owner, kIcebound, owner, pct, k881AuditBase + owner,
                k881StatBase + owner);
            if (r == ApplyAnomalyResult::ROLL_FAILED) {
                write_int(wctx, k881PendBase + owner, preempt);   // 未触发补偿（M1/M4）
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

EffectResult k395_881_fmr(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    consume_preempt_pending(ctx, a.owner, k881PendBase + a.owner);
    return EffectResult::kOk;
}

// ══ effect 1126 风凌雪 ══════════════════════════════════════════════════════════

EffectResult k395_1126_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->apply_anomaly_checked || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const HostRow row = host_row(ctx, a, 1126);
    const int base = db_at(row, 0, -1);
    const int step = db_at(row, 1, -1);
    const int cap = db_at(row, 2, -1);
    const int preempt = db_at(row, 3, 1);   // tips "先制+1" 互锚（M 记档：单行无第二锚）
    if (base <= 0 || step <= 0 || cap <= 0) {
        return EffectResult::kOk;
    }
    if (watcher_wired(ctx, k1126WireBase + owner)) {
        return EffectResult::kOk;
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, base, step, cap, preempt](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || !g_core || !g_core->apply_anomaly_checked) {
                return;
            }
            if (ev.actor != owner || !onstage_has_soul_mark(wctx, owner, 1126)) {
                return;
            }
            if (ev.used_attribute) {
                // 属性技：自身下回合先制+db3（pending，M4）
                write_int(wctx, k1126PendBase + owner, preempt);
                return;
            }
            // 攻击技：现行爬坡档申报（M2）
            int pct = read_int(wctx, k1126RampBase + owner);
            if (pct <= 0) {
                pct = base;   // 无账本 = 基础档
            }
            const ApplyAnomalyResult r = declare_anomaly(
                wctx, 1 - owner, kIcebound, owner, pct, k1126AuditBase + owner,
                k1126StatBase + owner);
            if (r == ApplyAnomalyResult::ROLL_FAILED) {
                write_int(wctx, k1126RampBase + owner,
                          std::min(pct + step, cap));   // 未触发爬坡（封顶）
            } else if (anomaly_landed(r)) {
                write_int(wctx, k1126RampBase + owner, base);   // 落地回落基础档（M2）
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

EffectResult k395_1126_fmr(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    consume_preempt_pending(ctx, a.owner, k1126PendBase + a.owner);
    return EffectResult::kOk;
}

// ══ effect 1218 伽楼儿 ══════════════════════════════════════════════════════════

EffectResult k395_1218_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->apply_anomaly_checked || !g_core->break_round_effects
        || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const HostRow row = host_row(ctx, a, 1218);
    const int pct = db_at(row, 0, -1);
    const int code = db_at(row, 1, kIncinerate);   // args[1]=22 焚烬码互锚
    const int reduce = db_at(row, 3, -1);
    if (pct <= 0 || code < 0) {
        return EffectResult::kOk;
    }
    // ① 受击焚烬/消回合（TEAM watcher，K241 形）
    if (!watcher_wired(ctx, k1218WireBase + owner)) {
        ctx->event_center_.register_watcher(
            EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [owner, pct, code](BattleContext* wctx, const BattleEvent& ev) {
                if (!wctx || !g_core || !g_core->apply_anomaly_checked
                    || !g_core->break_round_effects) {
                    return;
                }
                if (ev.target != owner || ev.amount <= 0 || !attack_damage_state(ev)) {
                    return;   // 受击窗（攻击伤害时点，K241 同款）
                }
                if (!onstage_has_soul_mark(wctx, owner, 1218)) {
                    return;
                }
                const ApplyAnomalyResult r = declare_anomaly(
                    wctx, 1 - owner, code, owner, pct, k1218AuditBase + owner,
                    k1218StatBase + owner);
                if (r == ApplyAnomalyResult::ROLL_FAILED) {
                    (void)g_core->break_round_effects(wctx, 1 - owner);   // 未触发补偿
                }
            },
            WatcherScope::TEAM);
    }
    // ② 受伤减 db3%（红伤 REDUCE_PCT+MITIGATE 票，回调实时读 RS 门旗 = M6/M10 形）
    if (reduce > 0 && !ticket_armed_this_epoch(ctx, kEpochBase + owner, owner)) {
        ctx->register_damage_effect(
            DamagePhase::REDUCE_PCT, owner, DamageEffectCategory::MITIGATE,
            [owner, reduce](BattleContext* c, int bucket_owner) {
                if (!c || bucket_owner != owner) {
                    return;
                }
                if (read_int(c, k1218ModeBase + owner) != 1) {
                    return;   // RS 低体门旗未置 → 本回合无减伤
                }
                DamageSnapshot& d = c->resolvedDamage;
                if (d.defenderId != bucket_owner || !d.isRed || d.final <= 0) {
                    return;
                }
                if (!onstage_has_soul_mark(c, owner, 1218)) {
                    return;
                }
                d.final = d.final * (100 - reduce) / 100;
            });
    }
    return EffectResult::kOk;
}

EffectResult k395_1218_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 ||
        !onstage_has_soul_mark(ctx, a.owner, 1218)) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const int low = ctx->getPet(owner).hp < ctx->getPet(1 - owner).hp ? 1 : 0;
    write_int(ctx, k1218ModeBase + owner, low);   // 每回合重写（2492 形）
    return EffectResult::kOk;
}

EffectResult k395_1218_fmr(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 ||
        !onstage_has_soul_mark(ctx, a.owner, 1218)) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (read_int(ctx, k1218ModeBase + owner) == 1) {
        const HostRow row = host_row(ctx, a, 1218);
        const int amp = db_at(row, 2, 0);
        if (amp > 0) {
            ctx->ws.damage_add_pct[owner] += amp;   // FMR 写（memset 后，M6）
        }
    }
    return EffectResult::kOk;
}

// ══ effect 1258 月玲珑 ══════════════════════════════════════════════════════════

EffectResult k395_1258_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->apply_anomaly_checked || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const HostRow row = host_row(ctx, a, 1258);
    const int dodge = db_at(row, 0, -1);
    const int ice = db_at(row, 1, -1);
    const int code = db_at(row, 2, kIcebound);   // args[2]=15 冰封码互锚
    if (dodge <= 0 || ice <= 0 || code < 0) {
        return EffectResult::kOk;
    }
    // ① 免伤票（BLOCK 相位逐次掷点，M7；低血翻倍现值判）
    if (!ticket_armed_this_epoch(ctx, kEpochBase + 100 + owner, owner)) {
        ctx->register_damage_effect(
            DamagePhase::BLOCK, owner, DamageEffectCategory::BLOCK,
            [owner, dodge](BattleContext* c, int bucket_owner) {
                if (!c || bucket_owner != owner) {
                    return;
                }
                DamageSnapshot& d = c->resolvedDamage;
                if (d.defenderId != bucket_owner || !d.isRed || d.final <= 0) {
                    return;
                }
                if (!onstage_has_soul_mark(c, owner, 1258)) {
                    return;
                }
                const int opp = 1 - owner;
                const bool low = c->getPet(owner).hp < c->getPet(opp).hp;   // M5 现值
                const int pct = low ? dodge * 2 : dodge;
                write_int(c, k1258DodgePctBase + owner, pct);
                const bool landed = roll_percent(c->rng(), pct);
                write_int(c, k1258DodgeHitBase + owner, landed ? 1 : 0);
                if (landed) {
                    write_int(c, k1258DodgeStatBase + owner,
                              read_int(c, k1258DodgeStatBase + owner) + 1);
                    d.base = 0;   // 免疫归零（k180 608 全字段清零形）
                    d.afterAdd = 0;
                    d.afterMul = 0;
                    d.final = 0;
                    d.addPct = 0;
                    d.mulCoef = 0.0;
                }
            });
    }
    // ② 攻击技冰封（E13 攻击技窗 + 低血翻倍现值判）
    if (!watcher_wired(ctx, k1258WireBase + owner)) {
        ctx->event_center_.register_watcher(
            EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [owner, ice, code](BattleContext* wctx, const BattleEvent& ev) {
                if (!wctx || !g_core || !g_core->apply_anomaly_checked) {
                    return;
                }
                if (ev.actor != owner || ev.used_attribute) {
                    return;
                }
                if (!onstage_has_soul_mark(wctx, owner, 1258)) {
                    return;
                }
                const int opp = 1 - owner;
                const bool low = wctx->getPet(owner).hp < wctx->getPet(opp).hp;
                declare_anomaly(wctx, opp, code, owner, low ? ice * 2 : ice,
                                k1258IceAuditBase + owner, k1258IceStatBase + owner);
            },
            WatcherScope::TEAM);
    }
    return EffectResult::kOk;
}

// ══ effect 1394 狞 ══════════════════════════════════════════════════════════════

EffectResult k395_1394_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->apply_anomaly_checked || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const HostRow row = host_row(ctx, a, 1394);
    const int inc = db_at(row, 0, -1);
    const int burn = db_at(row, 1, -1);
    if (inc <= 0 || burn <= 0) {
        return EffectResult::kOk;
    }
    if (!watcher_wired(ctx, k1394Wire1Base + owner)) {
        // ① 攻击技 25% 焚烬（低血翻倍现值判）
        ctx->event_center_.register_watcher(
            EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [owner, inc](BattleContext* wctx, const BattleEvent& ev) {
                if (!wctx || !g_core || !g_core->apply_anomaly_checked) {
                    return;
                }
                if (ev.actor != owner || ev.used_attribute) {
                    return;
                }
                if (!onstage_has_soul_mark(wctx, owner, 1394)) {
                    return;
                }
                const int opp = 1 - owner;
                const bool low = wctx->getPet(owner).hp < wctx->getPet(opp).hp;
                declare_anomaly(wctx, opp, kIncinerate, owner, low ? inc * 2 : inc,
                                k1394IncAuditBase + owner, k1394IncStatBase + owner);
            },
            WatcherScope::TEAM);
    }
    if (!watcher_wired(ctx, k1394Wire2Base + owner)) {
        // ② 受击 50% 烧伤攻击者（低血翻倍现值判）
        ctx->event_center_.register_watcher(
            EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [owner, burn](BattleContext* wctx, const BattleEvent& ev) {
                if (!wctx || !g_core || !g_core->apply_anomaly_checked) {
                    return;
                }
                if (ev.target != owner || ev.amount <= 0 || !attack_damage_state(ev)) {
                    return;
                }
                if (ev.actor != 1 - owner) {
                    return;   // 只认对手的攻击（1v1 受击面）
                }
                if (!onstage_has_soul_mark(wctx, owner, 1394)) {
                    return;
                }
                const int opp = 1 - owner;
                const bool low = wctx->getPet(owner).hp < wctx->getPet(opp).hp;
                declare_anomaly(wctx, opp, kBurn, owner, low ? burn * 2 : burn,
                                k1394BurnAuditBase + owner, k1394BurnStatBase + owner);
            },
            WatcherScope::TEAM);
    }
    return EffectResult::kOk;
}

// ══ effect 1676 冰·寒凌（881 同机制异 id 并做：己先手支 + 敌先手支镜像）════════

EffectResult k395_1676_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->apply_anomaly_checked || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const HostRow row = host_row(ctx, a, 1676);
    const int ice_pct = db_at(row, 0, -1);
    const int ice_preempt = db_at(row, 2, -1);
    const int fear_pct = db_at(row, 3, -1);
    const int fear_preempt = db_at(row, 5, -1);
    if (ice_pct <= 0 || ice_preempt < 0 || fear_pct <= 0 || fear_preempt < 0) {
        return EffectResult::kOk;
    }
    if (watcher_wired(ctx, k1676WireBase + owner)) {
        return EffectResult::kOk;
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, ice_pct, ice_preempt, fear_pct, fear_preempt](BattleContext* wctx,
                                                              const BattleEvent& ev) {
            if (!wctx || !g_core || !g_core->apply_anomaly_checked) {
                return;
            }
            if (!onstage_has_soul_mark(wctx, owner, 1676)) {
                return;
            }
            const int opp = 1 - owner;
            if (ev.actor == owner && !wctx->is_second_mover(owner)) {
                // 己先手支：50% 冰封，ROLL_FAILED → 己下1次先制+db2（M3/M4）
                const ApplyAnomalyResult r = declare_anomaly(
                    wctx, opp, kIcebound, owner, ice_pct, k1676SelfAuditBase + owner,
                    k1676SelfStatBase + owner);
                if (r == ApplyAnomalyResult::ROLL_FAILED) {
                    write_int(wctx, k1676SelfPendBase + owner, ice_preempt);
                }
                return;
            }
            if (ev.actor == opp && !wctx->is_second_mover(opp)) {
                // 敌先手支：50% 害怕，ROLL_FAILED → 敌下1次先制-db5
                const ApplyAnomalyResult r = declare_anomaly(
                    wctx, opp, kFear, owner, fear_pct, k1676OppAuditBase + owner,
                    k1676OppStatBase + owner);
                if (r == ApplyAnomalyResult::ROLL_FAILED) {
                    write_int(wctx, k1676OppPendBase + owner, fear_preempt);
                }
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

EffectResult k395_1676_fmr(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const int opp = 1 - owner;
    // 己支 pending：宿主在场才消费（M4）；敌支 pending：对手在场（对手用技面）。
    const int own_pend = read_int(ctx, k1676SelfPendBase + owner);
    if (own_pend != 0) {
        if (onstage_has_soul_mark(ctx, owner, 1676)) {
            ctx->plugin_storage.erase(k1676SelfPendBase + owner);
            ctx->ws.preemptive_level[owner] += own_pend;
        }
    }
    const int opp_pend = read_int(ctx, k1676OppPendBase + owner);
    if (opp_pend != 0) {
        if (ctx->getPet(opp).hp > 0 && ctx->on_stage[opp] >= 0) {
            ctx->plugin_storage.erase(k1676OppPendBase + owner);
            ctx->ws.preemptive_level[opp] -= opp_pend;
        }
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +7（698/881/1126/1218/1258/1394/1676
//    入账，只增不改）。tips 尾注全句（boss无效）→ 全节点 boss_invalid=true（M9）。

SOUL_PROGRAM(698, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k395_698_arm,
                   /*once=*/false, /*early=*/true, SoulScope::STAGE,
                   /*boss_invalid=*/true)});

SOUL_PROGRAM(881, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k395_881_arm,
                   /*once=*/false, /*early=*/true, SoulScope::STAGE,
                   /*boss_invalid=*/true),
                   SOUL_NODE(State::BATTLE_FIRST_MOVE_RIGHT, &k395_881_fmr,
                   /*once=*/false, /*early=*/false, SoulScope::STAGE,
                   /*boss_invalid=*/true)});

SOUL_PROGRAM(1126, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k395_1126_arm,
                    /*once=*/false, /*early=*/true, SoulScope::STAGE,
                    /*boss_invalid=*/true),
                    SOUL_NODE(State::BATTLE_FIRST_MOVE_RIGHT, &k395_1126_fmr,
                    /*once=*/false, /*early=*/false, SoulScope::STAGE,
                    /*boss_invalid=*/true)});

SOUL_PROGRAM(1218, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k395_1218_arm,
                    /*once=*/false, /*early=*/true, SoulScope::STAGE,
                    /*boss_invalid=*/true),
                    SOUL_NODE(State::BATTLE_ROUND_START, &k395_1218_round_start,
                    /*once=*/false, /*early=*/false, SoulScope::STAGE,
                    /*boss_invalid=*/true),
                    SOUL_NODE(State::BATTLE_FIRST_MOVE_RIGHT, &k395_1218_fmr,
                    /*once=*/false, /*early=*/false, SoulScope::STAGE,
                    /*boss_invalid=*/true)});

SOUL_PROGRAM(1258, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k395_1258_arm,
                    /*once=*/false, /*early=*/true, SoulScope::STAGE,
                    /*boss_invalid=*/true)});

SOUL_PROGRAM(1394, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k395_1394_arm,
                    /*once=*/false, /*early=*/true, SoulScope::STAGE,
                    /*boss_invalid=*/true)});

SOUL_PROGRAM(1676, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k395_1676_arm,
                    /*once=*/false, /*early=*/true, SoulScope::STAGE,
                    /*boss_invalid=*/true),
                    SOUL_NODE(State::BATTLE_FIRST_MOVE_RIGHT, &k395_1676_fmr,
                    /*once=*/false, /*early=*/false, SoulScope::STAGE,
                    /*boss_invalid=*/true)});
