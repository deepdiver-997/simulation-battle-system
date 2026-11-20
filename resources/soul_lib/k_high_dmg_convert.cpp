// K404 高伤转化/受击恢复族 —— effect 1409/1638/2494（3 宏 / 6 行，双行档位差）
//（看板 K404「高伤转化/受击恢复族 2026-11-02 编排挖掘」，a01-knight8，
//  worktree ../agents/k404；号段 7610-7619；通道=粉伤管线 CAP 快照（canglan_pink
//  PinkDamagePhase 先例）+ 红伤 CAP/DETECT 直改（k_damage_cap2 928/K400 1474 形）+
//  heal_amount 吃封回血 + ws.must_crit 窗（K7 1639 闪避窗同构账本）+
//  EVENT_TAKE_DAMAGE watcher（K3 thorn 同款）+ CAP 比例回调内实时读对手
//  preemptive_level（K1 766 凭证穿透链同款消费））
//
// 行清单（tools/soul_effects_dump.py 1409 1638 2494，2026-11-02）：
//   1409 小霸王·孙策 3823 两行（row1098 本体 args[2,1,1,300,300,1,100,1] /
//   row1307 伙伴「天作之合」args[2,1,1,300,350,1,120,1]，kind 同 [1,9,14]）：
//     「战斗阶段结束时，若自身存活且体力低于最大体力的1/2则解除自身异常状态，
//       恢复自身全部体力并造成等量固伤，同时本场战斗结束前自身所有技能先制+1
//       （每场战斗仅触发1次）；|自身造成的攻击伤害高于300则{吸取}对手300点{体力}，
//       自身造成的伤害低于300则下回合攻击技能先制+1且造成的伤害提升100%（boss无效）」
//     → db[0]=低体分母（2→1/2）、db[1]=每场触发次数（1）、db[2]=本场先制+1量（1）、
//       db[3]=伤害阈值（300）、db[4]=吸取量（300/350 档位差）、db[5]=下回合先制量（1）、
//       db[6]=下回合增伤%（100/120 档位差）、db[7]=1 旗不消费。
//   1638 幻境界神 4048 两行（row1298 本体 args[150,3,20,5,35,20,5,35] / row1542
//   伙伴「六界神谕」args[150,3,25,5,40,25,5,40]，kind 同 [1,5,6]）：
//     「自身受到的固定伤害或百分比伤害≥自身当前体力值时触发群星逆转：免疫本次
//       受到的伤害并将其150%转化为自身体力值，下3回合起自身致命率提升至100%
//       （每场战斗仅触发1次）；|自身在本场战斗中攻击技能命中后{吸取}对手最大{体力}
//       的20%，每次叠加5%，最高35%；自身在本场战斗中属性技能命中后下回合受到的
//       攻击伤害减少20%，每次叠加5%，最高35%（boss无效）」
//     → db[0]=转化%（150）、db[1]=致命窗回合数（3）、db[2]=吸取初始%（20/25）、
//       db[3]=吸取叠加步%（5）、db[4]=吸取上限%（35/40）、db[5]=减伤初始%（20/25）、
//       db[6]=减伤叠加步%（5）、db[7]=减伤上限%（35/40）。
//   2494 托勒斯 4887 两行（row2074 本体 args[100,15,200,5,500,1] / row2103 伙伴
//   「黄金圣徒」args[80,15,200,5,500,1]，kind 同 [5,14]；4399 原文分号四子句已核）：
//     「自身先制等级不高于0；自身每次受到的攻击伤害不超过自身最大体力的100%，
//       对手每有1级先制等级该比例降低15%，自身受到攻击后恢复200×（200%-该比例）；
//       自身每次造成的攻击伤害不低于对手最大体力的40%，对手每有1级先制等级该比例
//       提高5%，自身造成攻击伤害后附加500*该比例的固定伤害；自身被击败时令对手
//       1回合内先制等级归0（boss无效）」
//     → db[0]=受伤帽比例%（100/80 档位差）、db[1]=对手每级先制降幅%（15）、
//       db[2]=恢复基数（200）、db[3]=对手每级先制升幅%（5）、db[4]=附加固伤基数（500）、
//       db[5]=1 旗不消费；地板基座 40% 两行同值、非 args 载 → 模板常量（记档②）。
//
// ⚠️ 行判别（K400 1237「args 档位差」/K402 同判）：三宏两行均同宠同 kind 同参数个数，
//   唯一运行期差异=行参数值（300/350、100/120、20/25、35/40、100/80）——纯 args
//   驱动，无需行别通道；回调内一律 live 读宿主 soulMark.args.owned_int_args
//   （零 arm 期快照，双行切换不 stale），实现零硬编码行值。
//
// ⚠️ 口径记档（majority reading，均按先例）：
//   ①【1409 "造成等量固伤"】等量 = **实际恢复量**（heal_amount 落地后读
//     ws.last_heal_amount，封回血/恢复修正% 自然一致——恢复 0 则固伤 0）；
//     固伤走 deal_pink_damage(FIXED)（K197 DETECT 附粉同原语）。
//   ②【1409/2494 "吸取 N 点体力"】= 双段值形（deal_pink_damage(FIXED) +
//     heal_amount 同值，K225 341 吸取同款）；heal 按计算值非实落值（K225 口径记档）。
//   ③【1409 阈值严格不等号】"高于300/低于300" 恰 300 双分支都不结（K402 ⑦ 同门）；
//     攻击须实际落地（d.final>0——miss/被挡无"造成的伤害"，818/893 命中口径族）。
//   ④【1409 clause1 时序】"战斗阶段结束时"= BATTLE_ROUND_END 桶；先解异常→再恢复→
//     后固伤（行文序）；"本场战斗结束前先制+1（每场1次）"= plugin_storage 永久账
//     （离场保留）+ FMR 每回合重写 ws.preemptive_level（K402 1561 永久累计同构）。
//   ⑤【1638 群星逆转检测位】= 粉伤管线 **CAP 相位**（canglan 2343 同位，pre-hood）：
//     判 r.final（免减后、护罩前）≥ 当前体力（判定时点血未扣——管线跑完才落地）；
//     免疫本次 = r.final 归零（CAP 位清零 → HOOD 无可消费、无 TAKE_PINK 事件）；
//     转化基数 = CAP 位现值（免减后的 final，非 raw）。护罩会吃光的段也照触发
//     （CAP 位口径，post-hood 判定无生产者先例，记档）。逐段判定（箫澈 2099 同款）。
//   ⑥【1638 "下3回合起致命率100%"】= ws.must_crit 窗（板行处方 K395 pending 形），
//     窗界账本显式记 [start,end)（K7 1639 闪避窗同构——ROUND_START 桶每回合显式
//     写/清，窗外 must_crit 显式置 false）；窗=触发回合+1 起、不含触发当回合。
//   ⑦【1638 2a/2b 叠加账】"本场战斗中…每次叠加" = 战斗级 plugin_storage 计数，
//     第 n 次命中档位 = min(初始 + 步×(n−1), 上限)；2a 命中即结（EVENT_HIT watcher，
//     K7 1673 同窗同过滤——攻击技/属性技按 roundChoice 静态 type 分流）；
//   ⑧【1638 2b "下回合受到的攻击伤害减少"】= ROUND_START 快照账：当日命中计入
//     **次回合**生效（快照=回合开始时账本现值，当日新增不回溯本回合），
//     消费=红伤 REDUCE_PCT 票（MITIGATE 类，不可被挡伤失效无视）。
//   ⑨【2494 首句门槛】"自身先制等级不高于0"（4399 分号四子句之首）：majority
//     reading=约束第 2/3 子句（受攻帽/受攻恢复、攻击地板/附加固伤——板行明示第 2
//     子句门槛、第 3 子句对称同理），判据=回调内实时读 ws.preemptive_level[owner]≤0
//     （伤害时点现值，含技能基值先制——FMR memset 后定值）；第 4 子句（被击败时）
//     自含触发不设门槛（死亡时点读先制等级无机制意义，板行未述，记档）。
//   ⑩【2494 帽/地板比例动态算】回调内实时读 ws.preemptive_level[opp]（K1 766 凭证
//     穿透链同款消费、板行"回调内实时读对手 preemptive_level"处方）；算术直读无钳制
//     （对手负先制抬升帽/降地板，2532 priority_nullified 同款偏差记档）；
//     帽=红伤 CAP 相位（k_damage_cap2 928 同位、MITIGATE 类、697「无视伤害限制」
//     凭证可穿——K1 消费链同款）；地板=DETECT 直改 final=max(final, 地板值)
//     （K400 1474 DETECT 直改同位）+ 附加固伤 DETECT 附粉（K197 直系）。
//   ⑪【2494 "攻击伤害不低于40%"】= 伤害**地板**（boost-to-floor）非条件门：
//     与第 2 子句"不超过…%"（帽）句式镜像对称，majority reading 记档。
//   ⑫【2494 死亡归零】"自身被击败时令对手1回合内先制等级归0" = 死亡双桶（K402
//     1475/K188 同构，epoch 防同尸）置次回合戳 + **ROSTER scope** ROUND_START 消费
//     （ROSTER 恒注册/离场不死——soul_mark_manager.cpp:532 注册口径，宿主死后
//     消费不断链）→ ws.priority_nullified[opp]=true（2532/2492 同通道：内核 FMR
//     异常修正点统一清零，先于先制比较、晚于一切先制来源；负先制被抬到 0 的
//     清零近似偏差同 2532 记档）。
//   ⑬【boss 尾注】三行 (boss无效) 均为 tips 末位全句尾注 → 全节点 boss_invalid=true
//     （K400 ⑨/K399/K402 ⑤ 口径）；1409/1638 行中"（每场战斗仅触发1次）"为子句尾注、
//     仅次数限（plugin_storage once 旗），非 boss 语义。damage/事件回调一律加
//     is_boss_challenge 门（K402 1630 同款纵深防御）。
//
// ⚠️ 三查记录（a01-knight8 2026-11-02，worktree 内复核）：
//   ①内核 grep -w 1409/1638/2494 零命中（无魂印行为硬编码）；
//   ②effect_icon kind 已核（[1,9,14]/[1,5,6]/[5,14]）；effect_info 三 id 均为技能
//     空间异文模板（威力+24/未受击降属/附加系伤害——魂印语义权威=effect_icon tips，
//     SOP §六.1 双空间常态）；2494 已对 4399 pets_full.jsonl 原文（分号四子句一致）；
//   ③soul_lib 全库无 SOUL_PROGRAM/SOUL_MARK(1409/1638/2494)、registry 无三 id、
//     无未合并分支实现——真缺口。
// 存储键段：10^9 + effect_id×10^4（1409→1014090000 / 1638→1016380000 /
//   2494→1024940000）+ owner*16 + 槽，grep 全库无撞；plugin_storage 键 int 空间。
// 注册：SOUL_PROGRAM ×3（只增不改），soul 369→372。
// 场景回归：test/scenario/scenario_7610_sun_ce_1409.cpp 等（号段 7610-7619）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMark1409 = 1409;
constexpr int kMark1638 = 1638;
constexpr int kMark2494 = 2494;

constexpr int kOwnerStride = 16;   // 每 owner 键段步长（槽 0..15，K402 同款）

// ── 存储键段（base + owner*16 + 槽；全库 grep 无撞）─────────────────────────
constexpr int k1409Base = 1014090000;
constexpr int k1409OnceUsed = 0;   // clause1 每场1次已触发旗
constexpr int k1409PlLedger = 1;   // 本场结束前先制+1 永久账（1/0）
constexpr int k1409LowStamp = 2;   // 低伤支 pending 戳（=授予回合+1；0=无）
constexpr int k1409Wired = 3;      // DETECT 回调已挂旗

constexpr int k1638Base = 1016380000;
constexpr int k1638OnceUsed = 0;   // 群星逆转每场1次已触发旗
constexpr int k1638WinStart = 1;   // 致命窗起点回合（0=无窗）
constexpr int k1638WinEnd = 2;     // 致命窗终点回合（排他）
constexpr int k1638DrainHits = 3;  // 2a 攻击技命中计数（战斗级）
constexpr int k1638AttrHits = 4;   // 2b 属性技命中计数（战斗级）
constexpr int k1638EffReduce = 5;  // 2b 本回合生效减伤%（ROUND_START 快照）
constexpr int k1638Wired = 6;      // CAP/HIT/REDUCE 已挂旗

constexpr int k2494Base = 1024940000;
constexpr int k2494ZeroStamp = 0;  // 死亡归零戳（=死亡回合+1；0=无）
constexpr int k2494Wired = 1;      // CAP/DETECT/TAKE_DAMAGE 已挂旗
constexpr int k2494DeathEpoch = 2; // 死亡 epoch（同尸防重，k188 口径）

// 2494 地板基座：tips「不低于对手最大体力的40%」两行同值、非 args 载 → 模板常量。
constexpr int k2494FloorBasePct = 40;

// ── 共用小件（K402 定例：行参 live 读 owned_int_args 原序）──────────────────

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

// 宿主在场的行参 live 读（回调内调用；不在场/无mark返回 nullptr）。
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

int db_at(const std::vector<int>* db, int idx, int fallback) {
    if (db && idx >= 0 && idx < static_cast<int>(db->size())) {
        return (*db)[static_cast<std::size_t>(idx)];
    }
    return fallback;
}

// 攻击伤害事件时点（K400/K395/K402 同款私副本）。
bool attack_damage_event(const BattleEvent& ev) {
    return ev.state == static_cast<int>(State::BATTLE_FIRST_ATTACK_DAMAGE)
        || ev.state == static_cast<int>(State::BATTLE_SECOND_ATTACK_DAMAGE);
}

// 697「无视伤害限制」凭证（K1 766 首消费同款读法，k_damage_cap2 同款）。
bool attacker_pierces_damage_limit(const BattleContext* c, int attacker_id) {
    return attacker_id >= 0 && attacker_id <= 1
        && c->ws.attack_credential[attacker_id].ignore_damage_limit;
}

// 本回合选中的技能类型（roundChoice 静态面板，K7 1673 / canglan 2343 同款读法）。
// 返回：1=攻击技 / 0=属性技 / -1=非选技操作。
int selected_skill_kind(const BattleContext* c, int side) {
    constexpr int kActionSelectSkill = 1;
    if (!c || side < 0 || side > 1 || c->roundChoice[side][0] != kActionSelectSkill) {
        return -1;
    }
    const int idx = c->roundChoice[side][1];
    const auto& skills = c->seerRobot[side].elfPets[c->on_stage[side]].skills;
    if (idx < 0 || idx >= static_cast<int>(skills.size())) {
        return -1;
    }
    return skills[static_cast<std::size_t>(idx)].type == SkillType::Attribute ? 0 : 1;
}

// ═════════════════════════════════════════════════════════════════════════════
// effect 1409 小霸王·孙策：战阶末低体→解异常+回满+等量固伤+本场先制+1（每场1次）；
//   攻击伤害>300→吸取对手 db[4] 点；<300→下回合攻击先制+db[5] 且增伤 db[6]%
// ═════════════════════════════════════════════════════════════════════════════

// enter（early）：arm DETECT 阈值分支回调（TEAM 一次，回调 live 读行参）。
EffectResult k404_1409_enter(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const int wired_key = k1409Base + owner * kOwnerStride + k1409Wired;
    if (read_int(ctx, wired_key) == 1) {
        return EffectResult::kOk;
    }
    write_int(ctx, wired_key, 1);
    ctx->register_default_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner || c->is_boss_challenge) {
                return;
            }
            DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || d.defenderId != 1 - owner || !d.isRed
                || d.final <= 0) {
                return;   // 自身造成的攻击红伤（落地值，记档③）
            }
            if (!onstage_has_soul_mark(c, owner, kMark1409)) {
                return;
            }
            const std::vector<int>* db = live_row_db(c, owner, kMark1409);
            const int threshold = db_at(db, 3, 300);
            const int foe = 1 - owner;
            if (d.final > threshold) {
                // 高于阈值：{吸取}对手 db[4] 点体力（双段值形，记档②）。
                const int drain = db_at(db, 4, 300);
                if (drain > 0 && g_core && g_core->deal_pink_damage) {
                    (void)g_core->deal_pink_damage(c, foe, drain, DamageKind::FIXED,
                                                   owner);
                    if (g_core->heal_amount) {
                        (void)g_core->heal_amount(c, owner, drain);
                    }
                }
            } else if (d.final < threshold) {
                // 低于阈值：下回合攻击先制+db[5] 且增伤 db[6]%（pending 戳）。
                write_int(c, k1409Base + owner * kOwnerStride + k1409LowStamp,
                          c->roundCount + 1);
            }
            // 恰等于阈值：双分支都不结（记档③）。
        });
    return EffectResult::kOk;
}

// FMR（memset 后写）：消费低伤支 pending（K394 T2 stamp>0 守卫）+ 永久先制账重写。
EffectResult k404_1409_fmr(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !onstage_has_soul_mark(ctx, a.owner, kMark1409)) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const int base = k1409Base + owner * kOwnerStride;
    const std::vector<int>* db = live_row_db(ctx, owner, kMark1409);
    // 低伤支 pending：戳=授予回合+1，须 >0 且 ==当回合（缺失键 0 守卫）。
    const int stamp = read_int(ctx, base + k1409LowStamp);
    if (stamp > 0 && stamp == ctx->roundCount) {
        erase_int(ctx, base + k1409LowStamp);
        const int pl = db_at(db, 5, 1);
        const int amp = db_at(db, 6, 100);
        if (pl > 0) {
            ctx->ws.preemptive_level[owner] += pl;   // "攻击技能先制"——side 级 pl 槽（K402 ⑨ 通道）
        }
        if (amp > 0) {
            ctx->ws.damage_add_pct[owner] += static_cast<float>(amp);   // ws 钦定通道
        }
    }
    // clause1 永久账：本场结束前所有技能先制+db[2]（离场保留，FMR 每回合重写）。
    if (read_int(ctx, base + k1409PlLedger) == 1) {
        const int pl1 = db_at(db, 2, 1);
        if (pl1 > 0) {
            ctx->ws.preemptive_level[owner] += pl1;
        }
    }
    return EffectResult::kOk;
}

// ROUND_END：存活+低体（每场1次）→ 解异常 → 回满 → 等量固伤 → 置永久先制账。
EffectResult k404_1409_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !g_core || !g_core->heal_amount
        || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMark1409)) {
        return EffectResult::kOk;
    }
    const int base = k1409Base + owner * kOwnerStride;
    if (read_int(ctx, base + k1409OnceUsed) == 1) {
        return EffectResult::kOk;   // 每场战斗仅触发1次（子句尾注，记档⑬）
    }
    ElfPet& me = ctx->getPet(owner);
    const int max_hp = resolve_pet_max_hp(me);
    const int denom = db_at(live_row_db(ctx, owner, kMark1409), 0, 2);
    // 存活 + 体力低于最大体力的 1/db[0]（严格低于）。
    if (me.hp <= 0 || max_hp <= 0 || denom <= 0 || me.hp * denom >= max_hp) {
        return EffectResult::kOk;
    }
    write_int(ctx, base + k1409OnceUsed, 1);
    // ① 解除自身异常状态。
    if (g_core->dispel_active_anomalies) {
        (void)g_core->dispel_active_anomalies(ctx, owner);
    }
    // ② 恢复自身全部体力 → ③ 造成**等量**固伤（实际恢复量读数，记档①）。
    const int missing = max_hp - me.hp;
    if (missing > 0) {
        (void)g_core->heal_amount(ctx, owner, missing);
        const int healed = ctx->ws.last_heal_amount[owner];
        if (healed > 0) {
            (void)g_core->deal_pink_damage(ctx, 1 - owner, healed, DamageKind::FIXED,
                                           owner);
        }
    }
    // ④ 本场战斗结束前所有技能先制+db[2]（永久账，FMR 重写）。
    write_int(ctx, base + k1409PlLedger, 1);
    return EffectResult::kOk;
}

// ═════════════════════════════════════════════════════════════════════════════
// effect 1638 幻境界神：固/粉伤≥当前体力→群星逆转（免疫+150%转化+3回合必暴，
//   每场1次）；攻击技命中→叠加吸取对手 maxHP%；属性技命中→叠加下回合减伤%
// ═════════════════════════════════════════════════════════════════════════════

// enter（early）：arm 粉伤 CAP + EVENT_HIT watcher + 红伤 REDUCE_PCT（各 TEAM 一次）。
EffectResult k404_1638_enter(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !g_core || !g_core->heal_amount
        || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const int base = k1638Base + owner * kOwnerStride;
    if (read_int(ctx, base + k1638Wired) == 1) {
        return EffectResult::kOk;
    }
    write_int(ctx, base + k1638Wired, 1);
    const int foe = 1 - owner;
    // ① 群星逆转：粉伤 CAP 相位（canglan 2343 同位，记档⑤）。
    ctx->register_pink_damage_effect(
        PinkDamagePhase::CAP, owner,
        [owner](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner || c->is_boss_challenge) {
                return;
            }
            if (!onstage_has_soul_mark(c, owner, kMark1638)) {
                return;
            }
            const int base2 = k1638Base + owner * kOwnerStride;
            if (read_int(c, base2 + k1638OnceUsed) == 1) {
                return;   // 每场战斗仅触发1次
            }
            PinkDamageResolved& r = c->resolvedPink;
            if (r.target != owner || r.final <= 0) {
                return;
            }
            // 判定：本段（免减后）≥ 自身当前体力（判定时点血未扣，记档⑤）。
            const int cur_hp = c->getPet(owner).hp;
            if (r.final < cur_hp || cur_hp <= 0) {
                return;
            }
            write_int(c, base2 + k1638OnceUsed, 1);
            const std::vector<int>* db = live_row_db(c, owner, kMark1638);
            const int convert = db_at(db, 0, 150);   // 转化%
            const int rounds = db_at(db, 1, 3);      // 下3回合
            const int dmg = r.final;
            // 免疫本次：整段归零（HOOD 无可消费、无 TAKE_PINK 事件）。
            r.final = 0;
            // 其 150% 转化为自身体力值（heal 原语，吃封回血）。
            if (convert > 0) {
                (void)g_core->heal_amount(c, owner, dmg * convert / 100);
            }
            // 下3回合起致命率提升至100%：窗界账本（K7 1639 闪避窗同构，记档⑥）。
            if (rounds > 0) {
                const int win_start = c->roundCount + 1;
                write_int(c, base2 + k1638WinStart, win_start);
                write_int(c, base2 + k1638WinEnd, win_start + rounds);
            }
        },
        EffectScope::TEAM);
    // ② 2a/2b 命中账 watcher（EVENT_HIT，K7 1673 同窗；攻/属按 roundChoice 分流）。
    ctx->event_center_.register_watcher(
        EventType::EVENT_HIT, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, foe](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.actor != owner || c->is_boss_challenge) {
                return;
            }
            if (!onstage_has_soul_mark(c, owner, kMark1638)) {
                return;
            }
            const int kind = selected_skill_kind(c, owner);
            const int base2 = k1638Base + owner * kOwnerStride;
            const std::vector<int>* db = live_row_db(c, owner, kMark1638);
            if (kind == 1) {
                // 2a 攻击技命中后{吸取}对手最大体力 pct%（双段值形，记档②⑦）。
                const int init_pct = db_at(db, 2, 20);
                const int step = db_at(db, 3, 5);
                const int cap = db_at(db, 4, 35);
                if (init_pct <= 0) {
                    return;
                }
                const int hits = read_int(c, base2 + k1638DrainHits) + 1;
                write_int(c, base2 + k1638DrainHits, hits);
                int pct = init_pct + (hits - 1) * step;
                if (cap > 0) {
                    pct = std::min(pct, cap);
                }
                const int amount =
                    resolve_pet_max_hp(c->getPet(foe)) * pct / 100;
                if (amount > 0) {
                    (void)g_core->deal_pink_damage(c, foe, amount,
                                                   DamageKind::PERCENT_VALUE, owner);
                    (void)g_core->heal_amount(c, owner, amount);
                }
            } else if (kind == 0) {
                // 2b 属性技命中：只记账（次回合 ROUND_START 快照生效，记档⑧）。
                const int init_pct = db_at(db, 5, 20);
                if (init_pct > 0) {
                    write_int(c, base2 + k1638AttrHits,
                              read_int(c, base2 + k1638AttrHits) + 1);
                }
            }
        },
        WatcherScope::TEAM);
    // ③ 2b 消费票：红伤 REDUCE_PCT（MITIGATE 类，快照档减伤，记档⑧）。
    ctx->register_default_damage_effect(
        DamagePhase::REDUCE_PCT, owner, DamageEffectCategory::MITIGATE,
        [owner](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner || c->is_boss_challenge) {
                return;
            }
            DamageSnapshot& d = c->resolvedDamage;
            if (d.defenderId != owner || !d.isRed || d.final <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(c, owner, kMark1638)) {
                return;
            }
            const int reduce =
                read_int(c, k1638Base + owner * kOwnerStride + k1638EffReduce);
            if (reduce <= 0) {
                return;
            }
            d.final = d.final * (100 - reduce) / 100;
        });
    return EffectResult::kOk;
}

// ROUND_START：致命窗显式写/清（K7 窗界教训：窗外显式置 false）+ 2b 快照账。
EffectResult k404_1638_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !onstage_has_soul_mark(ctx, a.owner, kMark1638)) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const int base = k1638Base + owner * kOwnerStride;
    // ① 致命窗：窗内 must_crit=true，窗外显式 false（ws reset 兜底+显式清双保险）。
    const int win_start = read_int(ctx, base + k1638WinStart);
    const int win_end = read_int(ctx, base + k1638WinEnd);
    if (win_start > 0 && ctx->roundCount >= win_start && ctx->roundCount < win_end) {
        ctx->ws.must_crit[owner] = true;
    } else {
        ctx->ws.must_crit[owner] = false;
    }
    // ② 2b 快照：本回合生效减伤 = 回合开始时属性命中账现值（"下回合"口径，记档⑧）。
    const int hits = read_int(ctx, base + k1638AttrHits);
    int reduce = 0;
    if (hits > 0) {
        const std::vector<int>* db = live_row_db(ctx, owner, kMark1638);
        const int init_pct = db_at(db, 5, 20);
        const int step = db_at(db, 6, 5);
        const int cap = db_at(db, 7, 35);
        reduce = init_pct + (hits - 1) * step;
        if (cap > 0) {
            reduce = std::min(reduce, cap);
        }
    }
    write_int(ctx, base + k1638EffReduce, reduce);
    return EffectResult::kOk;
}

// ═════════════════════════════════════════════════════════════════════════════
// effect 2494 托勒斯：先制≤0 门槛下 受攻帽（对手每级先制-db[1]%）+受攻恢复
//   db[2]×(200%-帽)；攻击地板（对手每级先制+db[3]%）+附加 db[4]×比例 固伤；
//   被击败→对手1回合内先制归0
// ═════════════════════════════════════════════════════════════════════════════

// enter（early）：arm 红伤 CAP 帽 + DETECT 地板/附加 + TAKE_DAMAGE 恢复 watcher。
EffectResult k404_2494_enter(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !g_core || !g_core->heal_amount
        || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const int base = k2494Base + owner * kOwnerStride;
    if (read_int(ctx, base + k2494Wired) == 1) {
        return EffectResult::kOk;
    }
    write_int(ctx, base + k2494Wired, 1);
    const int foe = 1 - owner;
    // ① 受攻帽：红伤 CAP 相位（928 同位、MITIGATE 类、697 凭证可穿，记档⑩）。
    ctx->register_default_damage_effect(
        DamagePhase::CAP, owner, DamageEffectCategory::MITIGATE,
        [owner](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner || c->is_boss_challenge) {
                return;
            }
            DamageSnapshot& d = c->resolvedDamage;
            if (d.defenderId != owner || !d.isRed || d.final <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(c, owner, kMark2494)) {
                return;
            }
            if (c->ws.preemptive_level[owner] > 0) {
                return;   // 门槛：自身先制等级不高于0（伤害时点现值，记档⑨）
            }
            const std::vector<int>* db = live_row_db(c, owner, kMark2494);
            const int cap_pct = db_at(db, 0, 100);
            const int per_lv = db_at(db, 1, 15);
            const int opp_pl = d.attackerId >= 0 && d.attackerId <= 1
                                   ? c->ws.preemptive_level[d.attackerId]
                                   : 0;   // 对手每级先制该比例-db[1]%（实时读，记档⑩）
            const int cap = cap_pct - per_lv * opp_pl;
            if (cap <= 0) {
                return;   // 比例非正：帽失效（不归零，记档⑩无钳制口径的退化面）
            }
            if (attacker_pierces_damage_limit(c, d.attackerId)) {
                return;   // 697「无视伤害限制」穿帽（K1 消费链）
            }
            const int cap_value = resolve_pet_max_hp(c->getPet(owner)) * cap / 100;
            if (d.final > cap_value) {
                d.final = cap_value;
            }
        });
    // ② 攻击地板 + 附加固伤：DETECT 直改 + 附粉（K400 1474/K197 同位，记档⑪⑩）。
    ctx->register_default_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner, foe](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner || c->is_boss_challenge) {
                return;
            }
            DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || d.defenderId != foe || !d.isRed
                || d.final <= 0) {
                return;   // 自身造成的攻击红伤（落地值）
            }
            if (!onstage_has_soul_mark(c, owner, kMark2494)) {
                return;
            }
            if (c->ws.preemptive_level[owner] > 0) {
                return;   // 门槛（记档⑨）
            }
            const std::vector<int>* db = live_row_db(c, owner, kMark2494);
            const int per_lv = db_at(db, 3, 5);
            const int append_base = db_at(db, 4, 500);
            const int opp_pl = c->ws.preemptive_level[foe];
            const int floor_pct = k2494FloorBasePct + per_lv * opp_pl;   // 每级+db[3]%
            if (floor_pct > 0) {
                const int floor_value =
                    resolve_pet_max_hp(c->getPet(foe)) * floor_pct / 100;
                if (d.final < floor_value) {
                    d.final = floor_value;   // 地板：boost-to-floor（记档⑪）
                }
            }
            if (append_base > 0 && floor_pct > 0) {
                // 附加 500×该比例 固伤（比例=地板比例，分数口径 500×40% = 200）。
                (void)g_core->deal_pink_damage(c, foe, append_base * floor_pct / 100,
                                               DamageKind::FIXED, owner);
            }
        });
    // ③ 受攻恢复：EVENT_TAKE_DAMAGE watcher（K3 thorn 同款；门槛现读，记档⑨）。
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, foe](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != owner || ev.actor == owner || ev.amount <= 0
                || !attack_damage_event(ev) || c->is_boss_challenge) {
                return;   // 只认被对手攻击命中（K400 ④/K241 同门）
            }
            if (!onstage_has_soul_mark(c, owner, kMark2494)) {
                return;
            }
            if (c->getPet(owner).hp <= 0) {
                return;   // 致死击不恢复（箫澈 2099「体力为0不触发」同守卫：事件晚
                          // drain 时点 hp 已落地 0，此时回血会在死亡登记前复活宿主）
            }
            if (c->ws.preemptive_level[owner] > 0) {
                return;   // 门槛（记档⑨）
            }
            const std::vector<int>* db = live_row_db(c, owner, kMark2494);
            const int cap_pct = db_at(db, 0, 100);
            const int per_lv = db_at(db, 1, 15);
            const int heal_base = db_at(db, 2, 200);
            const int opp_pl = c->ws.preemptive_level[foe];
            const int ratio = cap_pct - per_lv * opp_pl;   // 现行帽比例%
            const int heal = heal_base * (200 - ratio) / 100;   // 200×（200%-该比例）
            if (heal > 0) {
                (void)g_core->heal_amount(c, owner, heal);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// 死亡双桶（K402 1475/K188 同构，epoch 防同尸）：置次回合归零戳。
EffectResult k404_2494_death(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !onstage_has_soul_mark(ctx, a.owner, kMark2494)) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const int slot = ctx->on_stage[owner];
    if (slot < 0 || slot > 5 || ctx->seerRobot[owner].elfPets[slot].hp > 0) {
        return EffectResult::kOk;   // 宿主未倒（死亡时点桶）
    }
    const int epoch = ctx->pipeline_valid_id[owner];
    const int epoch_key = k2494Base + owner * kOwnerStride + k2494DeathEpoch;
    if (read_int(ctx, epoch_key) == epoch) {
        return EffectResult::kOk;   // 同尸门
    }
    write_int(ctx, epoch_key, epoch);
    write_int(ctx, k2494Base + owner * kOwnerStride + k2494ZeroStamp,
              ctx->roundCount + 1);
    return EffectResult::kOk;
}

// ROUND_START（ROSTER scope——恒注册/离场不死，记档⑫）：消费归零戳。
EffectResult k404_2494_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const int key = k2494Base + owner * kOwnerStride + k2494ZeroStamp;
    const int stamp = read_int(ctx, key);
    if (stamp <= 0 || stamp != ctx->roundCount) {
        return EffectResult::kOk;
    }
    erase_int(ctx, key);
    ctx->ws.priority_nullified[1 - owner] = true;   // 对手1回合内先制归0（2532 通道，记档⑫）
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +3（1409/1638/2494 入账，只增不改）──
// boss 尾注：三行 (boss无效) 全为 tips 末位全句尾注 → 全节点 boss_invalid=true
//（K400 ⑨/K399/K402 ⑤ 口径，记档⑬）。2494 归零消费节点用 ROSTER scope
//（宿主死后消费不断链，soul_mark_manager ROSTER=恒注册口径，记档⑫）。
SOUL_PROGRAM(kMark1409, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k404_1409_enter,
                               /*once=*/false, /*early=*/true, SoulScope::STAGE,
                               /*boss_invalid=*/true),
                         SOUL_NODE(State::BATTLE_FIRST_MOVE_RIGHT, &k404_1409_fmr,
                                   /*once=*/false, /*early=*/false, SoulScope::STAGE,
                                   /*boss_invalid=*/true),
                         SOUL_NODE(State::BATTLE_ROUND_END, &k404_1409_round_end,
                                   /*once=*/false, /*early=*/false, SoulScope::STAGE,
                                   /*boss_invalid=*/true)});

SOUL_PROGRAM(kMark1638, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k404_1638_enter,
                                   /*once=*/false, /*early=*/true, SoulScope::STAGE,
                                   /*boss_invalid=*/true),
                         SOUL_NODE(State::BATTLE_ROUND_START, &k404_1638_round_start,
                                   /*once=*/false, /*early=*/false, SoulScope::STAGE,
                                   /*boss_invalid=*/true)});

SOUL_PROGRAM(kMark2494, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k404_2494_enter,
                                   /*once=*/false, /*early=*/true, SoulScope::STAGE,
                                   /*boss_invalid=*/true),
                         SOUL_NODE(State::BATTLE_ROUND_START, &k404_2494_round_start,
                                   /*once=*/false, /*early=*/false, SoulScope::ROSTER,
                                   /*boss_invalid=*/true),
                         SOUL_NODE(State::BATTLE_FIRST_MOVER_DEATH, &k404_2494_death,
                                   /*once=*/false, /*early=*/false, SoulScope::STAGE,
                                   /*boss_invalid=*/true),
                         SOUL_NODE(State::BATTLE_SECOND_MOVER_DEATH, &k404_2494_death,
                                   /*once=*/false, /*early=*/false, SoulScope::STAGE,
                                   /*boss_invalid=*/true)});
