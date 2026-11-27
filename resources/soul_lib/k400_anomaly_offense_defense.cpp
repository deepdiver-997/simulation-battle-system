// K400 异常攻防复合族 —— effect 1474/1151/1237（3 宏 / 6 行，双行档位差）
//（看板 K400「异常攻防复合族 2026-11-02 编排挖掘」，a01-knight4，worktree ../agents/k400；
//  号段 7540-7549；通道=K193 827 条件焚烬直系 + J2 538 clear_stat_boosts 消强先例 + K662 审计槽 +
//  K4 回合能力族 + K239 must_crit pending + K395 1126 爬坡回落）
//
// 行清单（tools/soul_effects_dump.py 1474 1151 1237，2026-11-02）：
//   1474 伽玛·鬼雀 3887 两行（kind 互异 → 行判别锚，见下【行判别 A】）：
//     row1156 本体 args[50,50,100,3,1] kind[1,10,14]：「回合开始时若对手处于异常状态则
//       受到的伤害提升50%；自身使用攻击时有50%概率令对手焚烬，未触发则自身下回合攻击
//       必定致命一击；自身受到攻击时有100%概率令对手烧伤，未触发则战斗阶段结束时附加
//       自身最大体力的1/3的百分比伤害（boss无效）」
//     row1961 伙伴行（千机再构）args[100,70,100,3,1] kind[1,10,12]：同文但首句=
//       「当回合自身造成的伤害提升100%」、焚烬 70%。
//     → db[0]=首句幅度（50/100，**语义随行别**：本体=对手受伤+50%、伙伴=自身增伤+100%
//       ——板行明示"两行门别不同注意 tips 互锚"）；db[1]=焚烬率（50/70）；
//       db[2]=受击烧伤率（100 恒）；db[3]=补偿固伤分母（3，"1/3"互锚）；
//       db[4]=1 旗不消费（K1 纪律）。
//   1151 西楚霸王·项羽 3564 两行（kind 同为 [9,10,13]，**args 计数互异** → 行判别锚，
//   见下【行判别 B】）：
//     row864 本体 args[60,1,40,2,1]：「回合开始时，若对手处于能力提升状态则消除对手能力
//       提升状态并有60%概率使对手焚烬，若对手不处于能力提升状态则自身所有技能先制+1；
//       自身使用攻击技能造成的伤害40%概率翻倍，未触发则对手2回合内无法使用属性技能
//       （boss无效)」
//     row912 伙伴行（霸王别姬）args[70,1,40,2,1,1]：同文但焚烬 70%、
//       「伤害40%概率提升150%」（=×2.5；**幅度不在 args**——tips 字面，按行别取）。
//     → db[0]=焚烬率（60/70）；db[1]=先制量（1，tips"先制+1"互锚）；db[2]=伤害增幅
//       触发率（40 恒）；db[3]=封属回合数（2，"2回合内"互锚）；db[4]/db[5]=尾位旗
//       不消费（K1 纪律；伙伴行多出的一位即行判别锚）。
//   1237 正义圣盾·凯普特 3662 两行（同 kind [1,10,11]，**args 档位差**常规行≠宏）：
//     row948 本体 args[2,2,100,30,7,10,60,1]：「回合开始时自身攻击+2、速度+2；若对手
//       处于异常状态则自身攻击技能造成的伤害提升100%，若对手不处于异常状态则自身攻击
//       技能30%令对手疲惫，未触发则下次疲惫的概率提升10%，最高60%（boss无效）」
//     row1477 伙伴行（X之战意）args[2,2,100,40,7,10,80,1]：疲惫 40%、上限 80%。
//     → db[0]=攻+2、db[1]=速+2、db[2]=增伤幅度（100 恒）、db[3]=疲惫基础率（30/40）、
//       db[4]=疲惫码（7=Fatigue，abnormal-types.h 互锚）、db[5]=爬坡步长（10 恒）、
//       db[6]=爬坡上限（60/80）、db[7]=1 旗不消费。
//
// ⚠️ 口径记档（majority reading，均按先例）：
//   【行判别 A】1474 两行 args 计数相同（5/5）、同宠 3887，唯一运行期可见差异=
//     kind_tags（本体 14 / 伙伴 12；真实链路 pet_factory.cpp:537 从 effect_icon.kind
//     填充）。实现按 kind_tags 判别：含 14=本体（对手受伤+50%）、含 12=伙伴（自身增伤
//     +100%）、**空=按本体**（本体行 majority；场景侧显式 set kind_tags 拟真）。
//   【行判别 B】1151 两行 kind 相同、同宠 3564，唯一运行期可见差异=owned_int_args
//     计数（本体 5 / 伙伴 6）。实现按 db.size()>=6 判伙伴 → 增幅 250（"提升150%"），
//     否则本体 → 200（"翻倍"）；幅度为 tips 字面（不在 args，K238/K311 同法），
//     记档待官方参数化后再迁 args。
//   ①「对手处于异常/能力提升状态」门 = ROUND_START 同步采样（k390 1523 mode 账本形，
//     2492 每回合重写）：回合内不再翻转（"回合开始时"管辖全句，K1040 RS 置旗同口径；
//     中途上场当回合无采样 → 两分支都不启用，K390 记档延续）。
//   ② 1474 本体首句「对手受到的伤害提升50%」= 受击侧增幅：DETECT 票（own 桶 TEAM，
//     once）守 d.defenderId==opp（**不收窄 attacker==own**——行文无"攻击时"限定词，
//     比 K264 356 更贴字面；1v1 观测等价，管线双桶 walk 实证见 damage_pipeline.h:99）。
//     伙伴首句「自身造成的伤害提升100%」= ws.damage_add_pct（FMR 桶写——K395 M6/K185
//     钦定通道；FMR 在 memset 后、先制比较前，本回合增伤安全）。
//   ③ 1474 焚烬窗 = EVENT_SKILL_USED（own+非属性技；E13 使用即发，miss 也算——K396 ③
//     窗口定例）→ apply_anomaly_checked(Incinerate=22, db[1])（K193 申报通道不预掷）；
//     ROLL_FAILED（battle_primitives.h "未触发补偿"钦定档）→ must_crit pending =
//     roundCount+1，次回合 FMR 消费 ws.must_crit（K239 1899 直系；must_crit 每回合
//     reset → FMR 写覆盖整回合；暴击掷点在 query_usage ①.5 晚于 FMR，K194 记档）。
//   ④ 1474 受击烧伤 = EVENT_TAKE_DAMAGE（target==own/actor≠own/amount>0/攻击伤害时点，
//     K241/K395 1218 同门）→ apply_anomaly_checked(Burn=2, db[2])；**未落地**
//     （!anomaly_landed——100% 档 ROLL_FAILED 不可能，补偿只可能来自免烧/抗性，
//     K238/K396-1040"未落地即补偿"读法，记档）→ pending 计数++；ROUND_END（K302
//     "战斗阶段结束"定例）→ 对手侧 deal_pink_damage(自身maxHP/db[3] ×计数,
//     PERCENT_VALUE)（K201 334 百分比伤害同kind；逐次未触发逐次补偿，literal 记档）。
//   ⑤ 1151 消强+焚烬 = ROUND_START 节点直执行（K172 808 同款：过门先无条件
//     clear_stat_boosts——"则消除…并有60%概率"消强不掷骰；焚烬走申报通道，无未触发
//     子句故无补偿）；非提升支 = mode 账本 → FMR 写 ws.preemptive_level += db[1]
//     （memset 后写，K390/K395 881 形）。
//   ⑥ 1151 伤害增幅 = EVENT_SKILL_USED（own+非属性技）本地 roll_percent(db[2])
//     （增幅非异常通道，本地掷点合法——K373 856 先例）→ 成功置增幅旗（200/250 按行
//     判别 B），失败 → seal_skill(attribute=true, count=0, duration_rounds=db[3])
//     （"无法使用属性技能"=封属，K311 口径；单次授予 duration=N 靠 tick 自灭，
//     k_hit_suppress_cond 同源；同句柄刷新非追加）。旗消费 = DETECT 票（own 攻击红伤
//     ——行文"自身使用攻击技能造成的伤害"明确攻方限定）d.final×旗/100 后清旗；
//     SKILL_USED 入口先清残旗 + ROUND_END 兜底清（miss 残留防泄漏到下次攻击）；
//     多段技首击消费口径记档。
//   ⑦ 1237 回合能力 = ROUND_START stat_change(攻槽0 +db[0]) + stat_change(速槽4
//     +db[1])（K4/K196 直系；±6 钳制内核自理）；增伤支 = mode==2 → FMR
//     ws.damage_add_pct += db[2]（②同通道）；疲惫爬坡 = mode==1 时 SKILL_USED
//     （own+非属性技）→ 现行档申报 apply_anomaly_checked(Fatigue=7)（K662 审计槽：
//     掷前写槽）→ ROLL_FAILED：ledger=min(pct+db[5], db[6])；落地：ledger=db[3]
//     （"未触发则下次提升"对称读法=落地回落，K395 1126 M2 直系；ledger 跨回合保留
//     ——「下次」无回合过期，K395 M4 同口径）。
//   ⑧ 概率节点全部带 K662 式审计槽（申报概率/增幅前后值掷前写 plugin_storage，
//     场景无随机精确断言 + 统计带侧证）。1474 受击烧伤 100% 档内核短路不耗 rand
//     （K226 口径）。
//   ⑨ boss 尾注：三行全部全句尾注（boss无效）→ 全节点 boss_invalid=true（K193/K395
//     M9 惯例：boss 对局节点不注册不执行 → 票不武装）。
//
// ⚠️ 三查记录（a01-knight4 2026-11-02，worktree 内复核）：
//   ①内核 grep -w 1474/1151/1237 零魂印硬编码（唯一命中 battle_primitives.cpp:1889
//     注释引用技能空间 1237 先例，非行为）；
//   ②effect_info 三 id 均为技能空间异文模板（1151 先制/1237 PP归零/1474 消强附加伤）
//     ——effect_icon tips 为权威（SOP §六.1）；
//   ③registry soul=351 无三 id；全 feat 分支 soul_lib 并集 git grep 无
//     SOUL_PROGRAM(1474/1151/1237)——真缺口；场景号段 7540-7549 全分支无撞。
// 存储键段：99474xxxxx（1474）/ 991151xxxxx（1151）/ 991237xxxxx（1237），
//   grep 全库（soul_lib+test/scenario）无撞；plugin_storage 键 int 空间。
// 注册：SOUL_PROGRAM ×3（1474/1151/1237 入账，只增不改），soul 351→354。
// 场景回归：test/scenario/scenario_7540_anomaly_1474.cpp / scenario_7543_anomaly_1151.cpp
//   / scenario_7546_anomaly_1237.cpp（号段 7540-7549）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

#include <primitives/battle_primitives.h>

namespace {

constexpr int kIncinerate = static_cast<int>(AbnormalStatusId::Incinerate);  // 22 焚烬
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);              // 2 烧伤
constexpr int kFatigue = static_cast<int>(AbnormalStatusId::Fatigue);        // 7 疲惫

// ── 存储键段（+owner；tools/check_storage_keys.py 已核无撞）──────────────────
constexpr int k1474WireSkill = 994740000;   // SKILL_USED watcher 已挂旗
constexpr int k1474WireTake = 994740010;    // TAKE_DAMAGE watcher 已挂旗
constexpr int k1474WireDetect = 994740020;  // DETECT 受击增幅票已挂旗（一场一次）
constexpr int k1474Mode = 994740030;        // RS 采样：1=对手异常（增幅窗启用）
constexpr int k1474CritPend = 994740040;    // 下回合必暴 pending（roundCount+1）
constexpr int k1474FixCount = 994740050;    // 烧伤未落地计数（ROUND_END 消费）
constexpr int k1474AuditInc = 994740060;    // 焚烬申报概率（审计槽）
constexpr int k1474AuditBurn = 994740070;   // 烧伤申报概率（审计槽）
constexpr int k1474AmpIn = 994740080;       // 受击增幅前 final（审计）
constexpr int k1474AmpOut = 994740090;      // 受击增幅后 final（审计）
constexpr int k1474IncStat = 994740100;     // 焚烬落地计数（统计带侧证）

constexpr int k1151Mode = 991151000;        // RS 采样：2=对手无提升（先制支启用）
constexpr int k1151WireSkill = 991151010;   // SKILL_USED watcher 已挂旗
constexpr int k1151WireDetect = 991151020;  // DETECT 增幅消费票已挂旗
constexpr int k1151DblFlag = 991151030;     // 本攻击增幅旗（200/250，0=无）
constexpr int k1151AuditInc = 991151040;    // 焚烬申报概率（审计槽）
constexpr int k1151AuditDbl = 991151050;    // 增幅申报概率（审计槽，恒 db[2]）
constexpr int k1151DblIn = 991151060;       // 增幅前 final（审计）
constexpr int k1151DblOut = 991151070;      // 增幅后 final（审计）
constexpr int k1151IncStat = 991151080;     // 焚烬落地计数（统计带侧证）
constexpr int k1151DblStat = 991151090;     // 增幅成功计数（统计带侧证）

constexpr int k1237Mode = 991237000;        // RS 采样：1=疲惫支 2=增伤支
constexpr int k1237WireSkill = 991237010;   // SKILL_USED watcher 已挂旗
constexpr int k1237Ramp = 991237020;        // 疲惫现行爬坡档（跨回合账本）
constexpr int k1237Audit = 991237030;       // 疲惫申报概率（审计槽）
constexpr int k1237Land = 991237040;        // 疲惫落地计数（统计带侧证）

// 1151 封属句柄：同 source_id 重复授予 = 刷新非追加（RuleCenter grant_seal 覆盖键）。
constexpr int k1151SealHandle = 115101;

// ── 共用小件（db_at 原序读 owned_int_args = 方案 B，K172/K193 定例）──────────

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

// K238 落地集（"未触发补偿"只认这些档为已落地；免烧/抗性等非掷败档=未落地）。
bool anomaly_landed(ApplyAnomalyResult r) {
    return r == ApplyAnomalyResult::SUCCESS
        || r == ApplyAnomalyResult::DURATION_EXTENDED
        || r == ApplyAnomalyResult::REPLACED_EXISTING
        || r == ApplyAnomalyResult::CONVERTED;
}

// "处于异常状态" = 任一官方异常槽活跃（k390/k227 全池扫同款）。
bool has_any_anomaly(BattleContext* ctx, int side) {
    if (!g_core || !g_core->has_active_anomaly) {
        return false;
    }
    for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
        if (g_core->has_active_anomaly(ctx, side, id)) {
            return true;
        }
    }
    return false;
}

// 对手处于能力提升状态 = 任一等级槽正等级（K172/K185 判据，负等级不算）。
bool has_boost(BattleContext* ctx, int side) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[side][i] > 0) {
            return true;
        }
    }
    return false;
}

// 攻击伤害事件时点（K185/K395 同款私副本）。
bool attack_damage_event(const BattleEvent& ev) {
    return ev.state == static_cast<int>(State::BATTLE_FIRST_ATTACK_DAMAGE)
        || ev.state == static_cast<int>(State::BATTLE_SECOND_ATTACK_DAMAGE);
}

// ═════════════════════════════════════════════════════════════════════════════
// effect 1474 伽玛·鬼雀：对手异常→受击增幅/自身增伤（行判别 A）+ 焚烬/未触发必暴
//   + 受击烧伤/未落地战阶末补偿
// ═════════════════════════════════════════════════════════════════════════════

// 行判别 A：kind_tags 含 12 = 伙伴行（千机再构）；否则（含空）= 本体行（记档①）。
bool row1474_is_partner(BattleContext* ctx, int owner) {
    const int slot = ctx->find_pet_with_soulmark(owner, 1474);
    if (slot < 0 || slot > 5) {
        return false;
    }
    const std::vector<int>& tags = ctx->seerRobot[owner].elfPets[slot].soulMark.kind_tags;
    for (int tag : tags) {
        if (tag == 12) {
            return true;
        }
    }
    return false;
}

// enter：接线三件套（SKILL_USED 焚烬窗 / TAKE_DAMAGE 烧伤窗 / DETECT 受击增幅票）。
EffectResult k400_1474_enter(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->apply_anomaly_checked || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.erase(k1474Mode + owner);     // 跨局/换宠残留清账（K390 同款）
    ctx->plugin_storage.erase(k1474CritPend + owner);
    ctx->plugin_storage.erase(k1474FixCount + owner);
    // ① SKILL_USED 焚烬窗（own+非属性技；行参 arm 时捕获——单行族一场一宿主）。
    if (!ctx->plugin_storage.count(k1474WireSkill + owner)) {
        ctx->plugin_storage.emplace(k1474WireSkill + owner, 1);
        const HostRow row = host_row(ctx, a, 1474);
        const int inc_pct = db_at(row, 1, 50);        // db[1]，兜底本体行字面 50
        ctx->event_center_.register_watcher(
            EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [owner, inc_pct](BattleContext* c, const BattleEvent& ev) {
                if (!c || !g_core || !g_core->apply_anomaly_checked) {
                    return;
                }
                if (ev.actor != owner || ev.used_attribute) {
                    return;                            // 攻击技窗（K396 ③）
                }
                if (!onstage_has_soul_mark(c, owner, 1474)) {
                    return;
                }
                write_int(c, k1474AuditInc + owner, inc_pct);   // 审计槽：掷前写
                const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
                    c, 1 - owner, kIncinerate, random_anomaly_duration(), owner,
                    inc_pct, ChanceSource::SoulMark);
                if (anomaly_landed(r)) {
                    write_int(c, k1474IncStat + owner,
                              read_int(c, k1474IncStat + owner) + 1);   // 统计带侧证
                }
                if (r == ApplyAnomalyResult::ROLL_FAILED) {
                    // 未触发 → 下回合必暴 pending（K239 直系；同回合多次掷歪同值覆盖）。
                    write_int(c, k1474CritPend + owner, c->roundCount + 1);
                }
            },
            WatcherScope::TEAM);
    }
    // ② TAKE_DAMAGE 受击烧伤窗（target==own/actor≠own/amount>0/攻击伤害时点）。
    if (!ctx->plugin_storage.count(k1474WireTake + owner)) {
        ctx->plugin_storage.emplace(k1474WireTake + owner, 1);
        const HostRow row = host_row(ctx, a, 1474);
        const int burn_pct = db_at(row, 2, 100);      // db[2]，兜底字面 100
        ctx->event_center_.register_watcher(
            EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [owner, burn_pct](BattleContext* c, const BattleEvent& ev) {
                if (!c || !g_core || !g_core->apply_anomaly_checked) {
                    return;
                }
                if (ev.target != owner || ev.actor == owner || ev.amount <= 0
                    || !attack_damage_event(ev)) {
                    return;                            // 只认"被对手攻击命中"（K241 同门）
                }
                if (!onstage_has_soul_mark(c, owner, 1474)) {
                    return;
                }
                write_int(c, k1474AuditBurn + owner, burn_pct);   // 审计槽：掷前写
                const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
                    c, ev.actor, kBurn, random_anomaly_duration(), owner,
                    burn_pct, ChanceSource::SoulMark);
                if (!anomaly_landed(r)) {
                    // 未落地（100% 档只可能免烧/抗性）→ 战阶末补偿计数++（记档④）。
                    write_int(c, k1474FixCount + owner, read_int(c, k1474FixCount + owner) + 1);
                }
            },
            WatcherScope::TEAM);
    }
    // ③ DETECT 受击增幅票（本体行专属通道；TEAM once——回调按行判别自守）。
    if (!ctx->plugin_storage.count(k1474WireDetect + owner)) {
        ctx->plugin_storage.emplace(k1474WireDetect + owner, 1);
        ctx->register_default_damage_effect(
            DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
            [owner](BattleContext* c, int bucket_owner) {
                if (!c || bucket_owner != owner) {
                    return;
                }
                if (!onstage_has_soul_mark(c, owner, 1474)) {
                    return;
                }
                if (row1474_is_partner(c, owner)) {
                    return;                            // 伙伴行走 FMR damage_add_pct 通道
                }
                if (read_int(c, k1474Mode + owner) != 1) {
                    return;                            // RS 采样：对手非异常 → 门闭
                }
                DamageSnapshot& d = c->resolvedDamage;
                if (d.defenderId != 1 - owner || !d.isRed || d.final <= 0) {
                    return;                            // "对手受到的伤害"（记档②，不收窄攻方）
                }
                const int slot = c->find_pet_with_soulmark(owner, 1474);
                const std::vector<int>* db = slot < 0 || slot > 5
                    ? nullptr
                    : &c->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
                const int amp = (db && db->size() > 0) ? (*db)[0] : 50;   // db[0]，兜底本体字面 50
                if (amp <= 0) {
                    return;
                }
                write_int(c, k1474AmpIn + owner, d.final);        // 审计：增幅前
                d.final = d.final * (100 + amp) / 100;            // K264 同款 DETECT 直改
                write_int(c, k1474AmpOut + owner, d.final);       // 审计：增幅后
            });
    }
    return EffectResult::kOk;
}

// ROUND_START：采样对手异常态（每回合重写，2492 形）。
EffectResult k400_1474_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !onstage_has_soul_mark(ctx, a.owner, 1474)) {
        return EffectResult::kOk;
    }
    write_int(ctx, k1474Mode + a.owner, has_any_anomaly(ctx, 1 - a.owner) ? 1 : 0);
    return EffectResult::kOk;
}

// FMR：①必暴 pending 消费（K239 直系：pending==roundCount → ws.must_crit，一次清账）；
//      ②伙伴行增伤支（K395 M6：ws 钦定通道，memset 后写）。
EffectResult k400_1474_fmr(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !onstage_has_soul_mark(ctx, a.owner, 1474)) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const auto pit = ctx->plugin_storage.find(k1474CritPend + owner);
    if (pit != ctx->plugin_storage.end()
        && any_int_or(pit->second, -1) == ctx->roundCount) {
        ctx->plugin_storage.erase(pit);
        ctx->ws.must_crit[owner] = true;   // 未触发焚烬 → 本回合攻击必定致命一击
    }
    if (!row1474_is_partner(ctx, owner) || read_int(ctx, k1474Mode + owner) != 1) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, 1474);
    const int amp = db_at(row, 0, 100);              // db[0]，兜底伙伴字面 100
    if (amp > 0) {
        ctx->ws.damage_add_pct[owner] += static_cast<float>(amp);
    }
    return EffectResult::kOk;
}

// ROUND_END：烧伤未落地补偿（自身 maxHP/db[3] 的 PERCENT_VALUE ×计数，记档④）。
EffectResult k400_1474_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 1474)) {
        return EffectResult::kOk;
    }
    const int count = read_int(ctx, k1474FixCount + owner);
    ctx->plugin_storage.erase(k1474FixCount + owner);   // 旗每回合消费不残留（K1040 口径）
    if (count <= 0 || ctx->getPet(owner).hp <= 0) {
        return EffectResult::kOk;                       // 阵亡不结（K367 口径）
    }
    const HostRow row = host_row(ctx, a, 1474);
    const int denom = db_at(row, 3, 3);                 // db[3]，兜底字面 3
    if (denom <= 0) {
        return EffectResult::kOk;
    }
    const int unit = resolve_pet_max_hp(ctx->getPet(owner)) / denom;
    if (unit <= 0) {
        return EffectResult::kOk;
    }
    (void)g_core->deal_pink_damage(ctx, 1 - owner, unit * count,
                                   DamageKind::PERCENT_VALUE, owner);
    return EffectResult::kOk;
}

// ═════════════════════════════════════════════════════════════════════════════
// effect 1151 西楚霸王·项羽：对手提升→消强+焚烬 / 非提升→全技先制+1；
//   攻击伤害 40% 翻倍（本体）|提升150%（伙伴）→ 未触发封属 2 回合
// ═════════════════════════════════════════════════════════════════════════════

// 行判别 B：owned_int_args 计数 >=6 = 伙伴行（霸王别姬；记档②）。增幅= tips 字面。
int row1151_multiplier(BattleContext* ctx, int owner) {
    const int slot = ctx->find_pet_with_soulmark(owner, 1151);
    if (slot < 0 || slot > 5) {
        return 200;
    }
    const std::vector<int>& db = ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    return db.size() >= 6 ? 250 : 200;   // 伙伴"提升150%"=×2.5 / 本体"翻倍"=×2
}

// ROUND_START：对手有提升 → 无条件消强 + db[0]% 焚烬（K172 808 直系）；否则置先制支。
EffectResult k400_1151_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->clear_stat_boosts || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 1151)) {
        return EffectResult::kOk;
    }
    const int opp = a.opp;
    if (!has_boost(ctx, opp)) {
        write_int(ctx, k1151Mode + owner, 2);           // 非提升 → FMR 先制支启用
        return EffectResult::kOk;
    }
    write_int(ctx, k1151Mode + owner, 0);
    (void)g_core->clear_stat_boosts(ctx, opp);          // 消强不掷骰（官方"则消除…并有"）
    const HostRow row = host_row(ctx, a, 1151);
    const int inc_pct = db_at(row, 0, 60);              // db[0]，兜底本体字面 60
    write_int(ctx, k1151AuditInc + owner, inc_pct);     // 审计槽：掷前写（无补偿支）
    const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
        ctx, opp, kIncinerate, random_anomaly_duration(), owner, inc_pct,
        ChanceSource::SoulMark);
    if (anomaly_landed(r)) {
        write_int(ctx, k1151IncStat + owner,
                  read_int(ctx, k1151IncStat + owner) + 1);     // 统计带侧证
    }
    return EffectResult::kOk;
}

// FMR：非提升支 → 全技先制+db[1]（memset 后写，K390/K395 881 形）。
EffectResult k400_1151_fmr(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !onstage_has_soul_mark(ctx, a.owner, 1151)) {
        return EffectResult::kOk;
    }
    if (read_int(ctx, k1151Mode + a.owner) != 2) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, 1151);
    const int preempt = db_at(row, 1, 1);               // db[1]，兜底 tips 字面 +1
    if (preempt > 0) {
        ctx->ws.preemptive_level[a.owner] += preempt;
    }
    return EffectResult::kOk;
}

// enter：接线增幅窗（SKILL_USED 掷点置旗/封属 + DETECT 消费票）。
EffectResult k400_1151_enter(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->seal_skill) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.erase(k1151Mode + owner);       // 跨局/换宠残留清账
    ctx->plugin_storage.erase(k1151DblFlag + owner);
    if (ctx->plugin_storage.count(k1151WireSkill + owner)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.emplace(k1151WireSkill + owner, 1);
    const HostRow row = host_row(ctx, a, 1151);
    const int dbl_pct = db_at(row, 2, 40);              // db[2]，兜底字面 40
    const int seal_rounds = db_at(row, 3, 2);           // db[3]，兜底 tips 字面 2
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, dbl_pct, seal_rounds](BattleContext* c, const BattleEvent& ev) {
            if (!c || !g_core || !g_core->seal_skill) {
                return;
            }
            if (ev.actor != owner || ev.used_attribute) {
                return;                                  // 攻击技窗（"使用攻击技能"字面）
            }
            if (!onstage_has_soul_mark(c, owner, 1151)) {
                return;
            }
            // 入口先清残旗（上一攻击 miss 未消费 → 不泄漏到本次，记档⑥）。
            c->plugin_storage.erase(k1151DblFlag + owner);
            write_int(c, k1151AuditDbl + owner, dbl_pct);           // 审计槽：掷前写
            if (roll_percent(c->rng(), dbl_pct)) {
                write_int(c, k1151DblFlag + owner, row1151_multiplier(c, owner));
                write_int(c, k1151DblStat + owner,
                          read_int(c, k1151DblStat + owner) + 1);   // 统计带侧证
            } else if (seal_rounds > 0) {
                // 未触发补偿：对手 2 回合内无法使用属性技能（封属，K311 口径）。
                (void)g_core->seal_skill(c, /*source=*/owner, /*target=*/1 - owner,
                                         /*effect_id=*/k1151SealHandle,
                                         /*attribute=*/true, /*attack=*/false,
                                         /*count=*/0, /*duration_rounds=*/seal_rounds,
                                         /*penetrable=*/true, /*source_slot=*/-1,
                                         EffectScope::ON_STAGE, /*hit_invalid=*/false,
                                         /*chance_pct=*/100, /*consumed_when_pierced=*/false);
            }
        },
        WatcherScope::TEAM);
    // DETECT 消费票（own 攻击红伤：旗>0 → final×旗/100 → 清旗 + 审计前后值）。
    if (!ctx->plugin_storage.count(k1151WireDetect + owner)) {
        ctx->plugin_storage.emplace(k1151WireDetect + owner, 1);
        ctx->register_default_damage_effect(
            DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
            [owner](BattleContext* c, int bucket_owner) {
                if (!c || bucket_owner != owner) {
                    return;
                }
                if (!onstage_has_soul_mark(c, owner, 1151)) {
                    return;
                }
                const int flag = read_int(c, k1151DblFlag + owner);
                if (flag <= 0) {
                    return;
                }
                DamageSnapshot& d = c->resolvedDamage;
                if (d.attackerId != owner || d.defenderId != 1 - owner || !d.isRed
                    || d.final <= 0) {
                    return;                              // 自身攻击红伤才消费
                }
                write_int(c, k1151DblIn + owner, d.final);
                d.final = d.final * flag / 100;
                write_int(c, k1151DblOut + owner, d.final);
                c->plugin_storage.erase(k1151DblFlag + owner);   // 首击消费（记档⑥）
            });
    }
    return EffectResult::kOk;
}

// ROUND_END：增幅旗兜底清（本攻击 miss 未消费 → 不跨回合残留，记档⑥）。
EffectResult k400_1151_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.erase(k1151DblFlag + a.owner);
    return EffectResult::kOk;
}

// ═════════════════════════════════════════════════════════════════════════════
// effect 1237 正义圣盾·凯普特：回合开始攻+2速+2；对手异常→攻击伤害+100% /
//   非异常→攻击 30%/40% 疲惫（未触发+10% 爬坡，封顶 60%/80%，落地回落）
// ═════════════════════════════════════════════════════════════════════════════

// ROUND_START：自身攻/速自升（K4 直系）+ 采样对手异常定分支（记档①）。
EffectResult k400_1237_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 1237)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, 1237);
    const int atk_up = db_at(row, 0, 2);                // db[0]，兜底字面 +2
    const int spd_up = db_at(row, 1, 2);                // db[1]，兜底字面 +2
    if (atk_up != 0) {
        (void)g_core->stat_change(ctx, owner, kAbilityLevelIndexAttack, atk_up);
    }
    if (spd_up != 0) {
        (void)g_core->stat_change(ctx, owner, kAbilityLevelIndexSpeed, spd_up);
    }
    // 分支采样：对手异常 → 增伤支（mode=2）；否则疲惫支（mode=1）。
    write_int(ctx, k1237Mode + owner, has_any_anomaly(ctx, 1 - owner) ? 2 : 1);
    return EffectResult::kOk;
}

// FMR：增伤支 → 攻击技能伤害+db[2]%（ws 钦定通道，memset 后写，K395 M6）。
EffectResult k400_1237_fmr(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !onstage_has_soul_mark(ctx, a.owner, 1237)) {
        return EffectResult::kOk;
    }
    if (read_int(ctx, k1237Mode + a.owner) != 2) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, 1237);
    const int amp = db_at(row, 2, 100);                 // db[2]，兜底字面 100
    if (amp > 0) {
        ctx->ws.damage_add_pct[a.owner] += static_cast<float>(amp);
    }
    return EffectResult::kOk;
}

// enter：接线疲惫爬坡窗（SKILL_USED own+非属性技，mode==1 才申报）。
EffectResult k400_1237_enter(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.erase(k1237Mode + owner);       // 跨局/换宠残留清账
    if (ctx->plugin_storage.count(k1237WireSkill + owner)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.emplace(k1237WireSkill + owner, 1);
    const HostRow row = host_row(ctx, a, 1237);
    const int base = db_at(row, 3, 30);                 // db[3]，兜底本体字面 30
    const int code = db_at(row, 4, kFatigue);           // db[4]=7 疲惫码互锚
    const int step = db_at(row, 5, 10);                 // db[5]，兜底字面 10
    const int cap = db_at(row, 6, 60);                  // db[6]，兜底本体字面 60
    if (base <= 0 || code < 0) {
        return EffectResult::kOk;
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, base, code, step, cap](BattleContext* c, const BattleEvent& ev) {
            if (!c || !g_core || !g_core->apply_anomaly_checked) {
                return;
            }
            if (ev.actor != owner || ev.used_attribute) {
                return;                                  // 攻击技窗
            }
            if (!onstage_has_soul_mark(c, owner, 1237)) {
                return;
            }
            if (read_int(c, k1237Mode + owner) != 1) {
                return;                                  // 对手异常支 → 疲惫句不启用
            }
            int pct = read_int(c, k1237Ramp + owner);
            if (pct <= 0) {
                pct = base;                              // 无账本 = 基础档
            }
            write_int(c, k1237Audit + owner, pct);       // 审计槽：掷前写现行档
            const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
                c, 1 - owner, code, random_anomaly_duration(), owner, pct,
                ChanceSource::SoulMark);
            if (r == ApplyAnomalyResult::ROLL_FAILED) {
                write_int(c, k1237Ramp + owner,
                          step > 0 ? std::min(pct + step, cap) : pct);   // 未触发爬坡封顶
            } else if (anomaly_landed(r)) {
                write_int(c, k1237Ramp + owner, base);   // 落地回落基础档（K395 1126 M2）
                write_int(c, k1237Land + owner, read_int(c, k1237Land + owner) + 1);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +3（1474/1151/1237，只增不改）────
// 三行 tips 尾注全部（boss无效）→ 全节点 boss_invalid=true（K193/K395 M9 惯例）。
SOUL_PROGRAM(1474, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k400_1474_enter,
                              /*once=*/false, /*early=*/true, SoulScope::STAGE,
                              /*boss_invalid=*/true),
                    SOUL_NODE(State::BATTLE_ROUND_START, &k400_1474_round_start,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE,
                              /*boss_invalid=*/true),
                    SOUL_NODE(State::BATTLE_FIRST_MOVE_RIGHT, &k400_1474_fmr,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE,
                              /*boss_invalid=*/true),
                    SOUL_NODE(State::BATTLE_ROUND_END, &k400_1474_round_end,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE,
                              /*boss_invalid=*/true)});

SOUL_PROGRAM(1151, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k400_1151_enter,
                              /*once=*/false, /*early=*/true, SoulScope::STAGE,
                              /*boss_invalid=*/true),
                    SOUL_NODE(State::BATTLE_ROUND_START, &k400_1151_round_start,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE,
                              /*boss_invalid=*/true),
                    SOUL_NODE(State::BATTLE_FIRST_MOVE_RIGHT, &k400_1151_fmr,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE,
                              /*boss_invalid=*/true),
                    SOUL_NODE(State::BATTLE_ROUND_END, &k400_1151_round_end,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE,
                              /*boss_invalid=*/true)});

SOUL_PROGRAM(1237, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k400_1237_enter,
                              /*once=*/false, /*early=*/true, SoulScope::STAGE,
                              /*boss_invalid=*/true),
                    SOUL_NODE(State::BATTLE_ROUND_START, &k400_1237_round_start,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE,
                              /*boss_invalid=*/true),
                    SOUL_NODE(State::BATTLE_FIRST_MOVE_RIGHT, &k400_1237_fmr,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE,
                              /*boss_invalid=*/true)});
