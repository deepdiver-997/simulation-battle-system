// K406 输出转化/战阶末附加族 —— effect 1460/1463/1464（3 宏 / 6 行，双行档位差，
// 2026-11-02 编排挖掘；看板 K406，a01-knight10 2026-11-03，worktree ../agents/k406；
// 号段 7640-7649。通道=解除下降=clear_stat_drops（K401 1139 同族，判返回值>0=成功门）
// ＋先制+1=FMR pending（K394 ⑤）＋增伤=damage_add_pct（K399 同构）＋「上次攻击伤害」
// =ws.raw_attack_damage 台账（板行 last_attack_damage 系描述名；grep 1256/422 先例实证
// 字段名=raw_attack_damage，battleFsm.cpp:907 每刀覆写、:1910/:2054 回合界清零）＋
// 吸取=drain_hp（K399 1264）＋低伤转化=当回合伤害快照+下回合模式切换（K404 2494
// DETECT 直改同族）＋1次机会=每场计数账本（K404 1409 每场1次旗先例）。dump 全文先行。）
//
// 行清单（tools/soul_effects_dump.py 1460 1463 1464，2026-11-03）：
//   1460 暗瞳·魔眼凯兮 3870 两行（row1142 本体 / row1449 伙伴「暗黑战联伙伴效果」，
//   kind 同 [1,10,14]，args 档位差 db[2]/db[3]）：
//     row1142 args[1,50,80,50,50,1]：「每回合开始时解除自身能力下降状态，解除成功则
//       当回合攻击先制+1且造成的伤害提升50%；|自身攻击技能命中后，若对手每处于一种
//       能力提升状态则附加80点固定伤害，若对手不处于能力提升状态则有50%的概率令对手
//       石化，未触发则下回合造成的伤害提升50%（boss无效）」
//     row1449 伙伴行 args[1,50,100,60,50,1]：80→100（每种提升附加量）、50→60（石化%）。
//     → db[0]=①先制量（1，"先制+1"互锚）、db[1]=①增伤%（50，两行同）、
//       db[2]=②每种提升附加固伤（80/100 档位差）、db[3]=②石化%（50/60 档位差）、
//       db[4]=②未触发补偿增伤%（50，两行同）、db[5]=1 旗不消费。
//   1463 暗战·艾辛格斯 3875 两行（row1145 本体 / row1446 伙伴「暗黑战联伙伴效果」，
//   kind 同 [1,5,14]；**双行差异位全部落在被跳过的第三段**——见下段划界记档）：
//     row1145 args[50,2,1,100,50,3,1,1]：「自身攻击造成伤害的50%会恢复自身体力；|每个
//       战斗阶段结束时有1次机会，若当回合开始时自身体力低于最大体力的1/2，则附加上次
//       攻击造成伤害50%的百分比伤害；|回合开始时，若敌我双方中任意一方处于能力提升
//       状态则自身当回合所有技能先制+1且造成的攻击伤害提升100%，若敌我双方皆不处于
//       能力提升状态则自身当回合攻击50%打出致命一击且当回合战斗阶段结束时自身随机3项
//       能力值+1（boss无效）」
//     row1446 伙伴行 args[50,2,2,100,100,3,1,1]：seg3 先制+1→+2（db[2]）、致命 50%→100%
//       （db[4]）；seg1/seg2 文本两行全同（50%/1/2/1次机会/50%）。
//     → 行参逐位对文：db[0]=①吸血%（50）、db[1]=②低体门分母（2→低于1/2）、
//       db[2]=③先制量（1/2）、db[3]=③增伤%（100 两行同）、db[4]=③致命%（50/100）、
//       db[5]=③随机项数（3）、db[6]/db[7]=1 旗不消费；②的「1次机会」「伤害50%」
//       两行同文且不在 args → tips 字面 fallback 常量锚（K398 1430/K405 2510 纪律）。
//   1464 伽玛·悍刀 4887→pet3876 两行（row1146 本体 kind[5,13,14] / row1960 伙伴
//   「千机再构伙伴效果」kind[14,13]；单 | 段整串，纯 args 档位差 db[1]/db[2]/db[3]）：
//     row1146 args[3,300,3,300,1]：「自身所有技能命中后则战斗阶段结束时附加对手最大
//       体力1/3的百分比伤害，若当回合攻击造成的伤害低于300则下回合转变为{吸取}对手
//       最大体力1/3的{体力}，若当回合攻击造成的伤害高于300则下回合对手属性技能无效
//       （boss无效）」
//     row1960 伙伴行 args[3,400,2,200,1]：低伤阈值 300→400、吸取分母 1/3→1/2、高伤
//       阈值 300→200（行文「1/2」「200」与 args 互锚）；两阈值区间 (200,400) 重叠 →
//       独立 if 重叠双发（K405 2510 分支重叠 majority 记档）。
//     → db[0]=①附加分母（3，"1/3"互锚）、db[1]=②低伤阈值（300/400）、
//       db[2]=②吸取分母（3/2）、db[3]=③高伤阈值（300/200）、db[4]=1 旗不消费。
//
// ⚠️ 划界记档（majority reading，按 K403/K404/K405 板行-段位对齐先例逐宏定案）：
//   【段位划界】板行列到哪个 | 段就实现到哪段（段内截断以 dump 补全，K405 2477②
//     「恢复所有技能 PP→补全吸取400递减+else必暴」先例）：1460 两段全列 → 全落地
//     （②的石化支+未触发补偿为段内截断补全）；1463 板行只列 seg1/seg2 → seg3（回合
//     开始强化门/暴击/随机3项）**板行未列 → 记档跳过不产码**（K405 2475 第二子句/
//     K403「范围外记档」先例），其行参位 db[2..5] 解码备档不消费；1464 单段整列、
//     板行截断于低伤支 → 高伤支（下回合属性技无效）按段内截断补全落地。
//   【双行判别】逐宏定案：1460/1464 纯 args 档位差（K400 1237「args 档位差」同判，
//     无需行别通道）；1463 双行差异位（db[2]/db[4]）全在被跳的 seg3 内，本实现消费位
//     db[0]/db[1] 两行同值 → 纯 args 驱动自然覆盖，无需判别。回调内一律 live 读
//     owned_int_args（K404 零 arm 期快照口径），实现零行值硬编码。
//   【boss 尾注】三行（boss无效）均为 tips 末位尾注 → 全句尾归属 → 全节点
//     boss_invalid=true（K400 ⑨/K399/K402 ⑤/K404 ⑬/K405 口径；1463 尾注位于被跳的
//     seg3 文本内，但按 tips 末位全句尾读法仍罩已实现节点——K405 2475「尾注位于被跳
//     段之后仍全节点」同形判）；enter 早挂的管线票/事件 watcher 不随节点走 → 回调内
//     手动 is_boss_challenge 双保险（K391 口径）。
//   【1460① 成功门】clear_stat_drops 返回值>0 = 解除成功（板行处方）；先制+1=FMR
//     pending（ROUND_START 写戳=roundCount+1 防首回合 0 撞车，同回合 FMR 消费——
//     K402 1630 TriggerRound 同式）；增伤50%=ROUND_START 直写 ws.damage_add_pct
//     （「当回合」语义，K399 1264③ 同位）。
//   【1460② 提升计数】「每处于一种能力提升状态」= 5 战斗槽（攻/特攻/防/特防/速）
//     正等级项数、命中槽不计（K346 877/chaos_sorensen 5 项口径，kBattleStatSlots=5
//     私副本）；附加=计数×db[2] FIXED（877 直系 DETECT 相位同步读，避免 watcher 晚
//     drain 过期）。石化=apply_anomaly_checked 申报通道（877 同款不预掷）；「未触发」
//     =ROLL_FAILED 单档（K172/K232/k_iceburn [M1] majority：目标免疫/抗性等非掷败档
//     不补偿）→ 下回合增伤 db[4]%（pending 戳=roundCount+1，次回合 FMR 消费——
//     K404 1409 低伤支同式）。有提升支走附加时不掷点、无补偿（行文「未触发」只系
//     石化掷点，K402 ⑩「触发=效果发生」同源）。
//   【1463① 吸血】K168 363 直系：DETECT 相位（attacker==own+isRed+final>0）→
//     heal_amount(final×db[0]/100)（heal 吃封回血；DETECT 时点 hp 未扣，J 侧 363 同族
//     相位）。「攻击造成伤害」只认红伤（属性技无伤害相天然排除，G5）。
//   【1463② 1次机会】每场 1 次（板行「每场/每回合」斜杠取场——按板行指名先例
//     K404 1409 每场1次旗；官方行文「有1次机会」=可消耗机会资源，战阶末为支取时点）；
//     计数账本 plugin_storage，实际附加落地（amount>0）才计数（「触发=效果发生」，
//     K402 ⑩ 口径——低体但本回合未攻击（台账 0）不消耗机会）。
//   【1463② 读数】「上次攻击造成伤害」= ws.raw_attack_damage[owner]（422/1256 族
//     裸伤台账，battleFsm.cpp:907 每刀覆写——ROUND_END 时点值=本回合最后一刀裸伤；
//     多刀回合取末刀记档，单刀回合即「上次攻击」）；「当回合开始时自身体力低于最大
//     体力的1/2」= ROUND_START 快照低体旗（严格低于，恰 1/2 不结，K377/K404 ③ 口径）；
//     附加=deal_pink_damage(台账×50/100, PERCENT_VALUE)（「百分比伤害」字面→
//     PERCENT_VALUE，K399 1020① 同 kind 记档）。
//   【1464① 命中旗】「自身所有技能命中后」含属性技 → EVENT_HIT watcher 置当回合
//     命中旗（K404 1638 2a 同窗同过滤——EVENT_HIT 攻/属皆发），ROUND_END 消费即清；
//     战阶末附加=对手 maxHP/db[0] 的 PERCENT_VALUE 粉伤（「百分比伤害」同上记档）。
//   【1464② 模式切换】「当回合攻击造成的伤害」= ROUND_END 读 ws.raw_attack_damage
//     台账快照（未攻击回合台账=0 字面落低伤档，记档）；低于 db[1]（严格）→ 次回合
//     「前述效果改为吸取」：命中旗开时 drain_hp(own→opp, db[2]) 替代附加（K399 1264②
//     原语直系）；高于 db[3]（严格）→ 次回合对手属性技无效 = ROUND_START 消费戳
//     seal_skill(attribute, duration=1 每回合窗、count=0 不限次数、固定句柄刷新、
//     chance=100)（K399 1470①/K311 口径）。恰等阈值双支都不结（K402 ⑦ 严格档）；
//     两支独立 if（row1960 重叠区间双发，K405 2510 majority）。戳=until 精确窗
//     （==roundCount 生效、过期自灭，K391/K399 1530 形）；⚠️ 实测勘误：drain_until
//     消费必须带 >0 守卫——缺失键 read 回 0 撞首回合 rc=0 误入吸取档（K402/K405
//     stamp>0 记档坑复现，T7640k R1 d=50279 实证后修复）。
//   【段位注】1464 三支同段串联（"，"衔接无 |），板行「+」覆盖前两支、第三支按段内
//     截断补全（K405 2477② 先例）；1463 三段以「；|」分段、板行只触前两段。
//
// 三查记录（a01-knight10 2026-11-03，worktree 内复核）：
//   ①内核 grep -w 1460/1463/1464 零命中（无魂印行为硬编码）；
//   ②effect_info 三 id 均为技能空间异文模板（1460 附加百分比/1463 先出手死亡恢复/
//     1464 击败转嫁下只）——effect_icon tips 为权威（SOP §六.1 双空间常态）；
//     kind=[1,10,14]/[1,5,14]/[5,13,14]（row1960 [14,13]）已核；
//   ③soul_lib 全库无 SOUL_PROGRAM/SOUL_MARK(1460/1463/1464)（grep 实证）、各分支
//     git grep 无注册、registry soul 基线无三 id（build 后落盘复核）；号段 7640-7649
//     全分支无撞。场景载体：纯链拍打 10004/缩头 20001/水枪 10013 + 场景直调原语受控。
// 存储键段：10^9 + id×10^4（1460→1014600000 / 1463→1014630000 / 1464→1014640000）
//   + owner*16 + 槽（0..15），grep 全库无撞；plugin_storage 键 int 空间。
// 注册：SOUL_PROGRAM ×3（1460/1463/1464 入账，只增不改）。
// 场景回归：test/scenario/scenario_7640_output_convert_endwave.cpp（号段 7640-7649）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMark1460 = 1460;
constexpr int kMark1463 = 1463;
constexpr int kMark1464 = 1464;

constexpr int kOwnerStride = 16;       // 每 owner 键段步长（槽 0..15，K402/K404 同款）
constexpr int kBattleStatSlots = 5;    // 0攻 1特攻 2防 3特防 4速（命中槽 5 不计，877 口径）

// ── 存储键段（base + owner*16 + 槽；全库 grep 无撞）─────────────────────────
constexpr int k1460Base = 1014600000;
constexpr int k1460ClearStamp = 0;   // ①解除成功 pending 戳（=roundCount+1；0=无）
constexpr int k1460CompStamp = 1;    // ②石化未触发补偿戳（=授予回合+1；0=无）
constexpr int k1460Wired = 2;        // ② DETECT 票接线旗
constexpr int k1460PetrifyAudit = 3; // ② 石化申报概率（审计槽，F9 782）
constexpr int k1460PetrifyLand = 4;  // ② 石化落地计数（统计带侧证）

constexpr int k1463Base = 1014630000;
constexpr int k1463Wired = 0;        // ① DETECT 吸血票接线旗
constexpr int k1463LowFlag = 1;      // ② 当回合开始低体旗（ROUND_START 写 1/0）
constexpr int k1463OnceUsed = 2;     // ② 每场已用机会数（<1 可触发）

constexpr int k1464Base = 1014640000;
constexpr int k1464Wired = 0;        // EVENT_HIT watcher 接线旗
constexpr int k1464HitFlag = 1;      // 当回合命中旗（EVENT_HIT 置、ROUND_END 消费清）
constexpr int k1464DrainUntil = 2;   // 吸取模式戳（until==roundCount 生效）
constexpr int k1464SealUntil = 3;    // 封属 pending 戳（until，ROUND_START 消费）

constexpr int k1464SealHandle = 146401;  // 封属固定句柄（重授=刷新，K311/K399 1470 口径）

// 1463 seg2 tips 字面锚（两行同文、不在 args，K398/K405 2510 纪律）
constexpr int k1463Chances = 1;      // "有1次机会"
constexpr int k1463AttachPct = 50;   // "上次攻击造成伤害50%的百分比伤害"

// ── 共用小件（HostRow/db_at 原序读 owned_int_args，K400/K402/K404 定例）──────

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

// 回调内 live 读行参（K404 零 arm 期快照口径；不在场/无 mark 返回 nullptr）。
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

// 正等级计数（5 战斗槽，命中槽不计——K346 877/chaos_sorensen 口径）。
int positive_level_count(const BattleContext* ctx, int side) {
    int n = 0;
    for (int i = 0; i < kBattleStatSlots; ++i) {
        if (ctx->ability_levels[side][i] > 0) {
            ++n;
        }
    }
    return n;
}

// ════════════════ 1460 暗瞳·魔眼凯兮 ════════════════════════════════════════

// enter(early)：接线 ② 命中附伤/石化票（DETECT；一场一次）。
EffectResult k406_1460_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const int wired_key = k1460Base + owner * kOwnerStride + k1460Wired;
    if (read_int(ctx, wired_key) == 1) {
        return EffectResult::kOk;   // early 双触发防重（K391/K399 同款）
    }
    write_int(ctx, wired_key, 1);
    ctx->register_default_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner || !g_core || !g_core->deal_pink_damage
                || !g_core->apply_anomaly_checked) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || !d.isRed || d.final <= 0) {
                return;   // 自身攻击技命中（红伤落地；"命中后"→ miss/属性技不触发）
            }
            if (!onstage_has_soul_mark(c, owner, kMark1460) || c->is_boss_challenge) {
                return;   // boss 双保险（K391 管线票口径）
            }
            const std::vector<int>* db = live_row_db(c, owner, kMark1460);
            const int per = live_db_at(db, 2, 80);      // db[2]，兜底本体字面 80
            const int petrify_pct = live_db_at(db, 3, 50);  // db[3]，兜底本体字面 50
            const int comp_pct = live_db_at(db, 4, 50);     // db[4]，兜底字面 50
            const int opp = 1 - owner;
            const int boosts = positive_level_count(c, opp);
            if (boosts > 0) {
                // 每处于一种能力提升状态 → 附加 计数×db[2] 固定伤害（877 直系）。
                const int attach = boosts * per;
                if (attach > 0) {
                    (void)g_core->deal_pink_damage(c, opp, attach, DamageKind::FIXED,
                                                   owner);
                }
                return;   // 有提升支不掷点、无补偿（记档【1460②】）
            }
            // 无提升 → db[3]% 石化（申报通道不预掷）；ROLL_FAILED → 下回合增伤补偿。
            write_int(c, k1460Base + owner * kOwnerStride + k1460PetrifyAudit,
                      petrify_pct);   // 审计槽：掷前写申报概率（F9 782）
            const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
                c, opp, static_cast<int>(AbnormalStatusId::Petrify),
                random_anomaly_duration(), owner, petrify_pct, ChanceSource::SoulMark);
            if (r == ApplyAnomalyResult::SUCCESS
                || r == ApplyAnomalyResult::DURATION_EXTENDED
                || r == ApplyAnomalyResult::REPLACED_EXISTING
                || r == ApplyAnomalyResult::CONVERTED) {
                write_int(c, k1460Base + owner * kOwnerStride + k1460PetrifyLand,
                          read_int(c, k1460Base + owner * kOwnerStride
                                       + k1460PetrifyLand)
                              + 1);   // 落地计数（统计带侧证）
                return;
            }
            if (r == ApplyAnomalyResult::ROLL_FAILED && comp_pct > 0) {
                // 未触发（单档回执，K172/K232/k_iceburn [M1] majority）→ 下回合增伤。
                write_int(c, k1460Base + owner * kOwnerStride + k1460CompStamp,
                          c->roundCount + 1);
            }
        });
    return EffectResult::kOk;
}

// ROUND_START：① 解除自身能力下降（返回值>0=成功门）→ 当回合先制+1 pending +
// 增伤 db[1]% 直写。
EffectResult k406_1460_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !g_core || !g_core->clear_stat_drops) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMark1460)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMark1460);
    const int cleared = g_core->clear_stat_drops(ctx, owner);
    if (cleared > 0) {   // 解除成功门（板行处方：判返回值>0）
        const int pl = db_at(row, 0, 1);      // db[0]，兜底字面 +1
        const int amp = db_at(row, 1, 50);    // db[1]，兜底字面 50
        if (pl > 0) {
            // 先制+1：FMR pending（ROUND_START 写戳=roundCount+1 防首回合 0 撞车，
            // 同回合 FMR 消费——K402 1630 TriggerRound 同式）。
            write_int(ctx, k1460Base + owner * kOwnerStride + k1460ClearStamp,
                      ctx->roundCount + 1);
        }
        if (amp > 0) {
            ctx->ws.damage_add_pct[owner] += static_cast<float>(amp);   // 「当回合」直写
        }
    }
    return EffectResult::kOk;
}

// FMR：消费 ①先制戳（同回合，K402 1630 形）与 ②补偿戳（次回合，K404 1409 形）。
EffectResult k406_1460_fmr(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !onstage_has_soul_mark(ctx, a.owner,
                                                                     kMark1460)) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const int base = k1460Base + owner * kOwnerStride;
    const HostRow row = host_row(ctx, a, kMark1460);
    const int clear_stamp = read_int(ctx, base + k1460ClearStamp);
    if (clear_stamp > 0 && clear_stamp == ctx->roundCount + 1) {
        erase_int(ctx, base + k1460ClearStamp);
        const int pl = db_at(row, 0, 1);
        if (pl > 0) {
            ctx->ws.preemptive_level[owner] += pl;   // memset 后写（K394 ⑤ 通道）
        }
    }
    const int comp_stamp = read_int(ctx, base + k1460CompStamp);
    if (comp_stamp > 0 && comp_stamp == ctx->roundCount) {
        erase_int(ctx, base + k1460CompStamp);
        const int comp = db_at(row, 4, 50);
        if (comp > 0) {
            ctx->ws.damage_add_pct[owner] += static_cast<float>(comp);   // ws 钦定通道
        }
    }
    return EffectResult::kOk;
}

// ════════════════ 1463 暗战·艾辛格斯 ════════════════════════════════════════

// enter(early)：接线 ① 吸血票（DETECT，K168 363 直系；一场一次）。
EffectResult k406_1463_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const int wired_key = k1463Base + owner * kOwnerStride + k1463Wired;
    if (read_int(ctx, wired_key) == 1) {
        return EffectResult::kOk;
    }
    write_int(ctx, wired_key, 1);
    ctx->register_default_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner || !g_core || !g_core->heal_amount) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || !d.isRed || d.final <= 0) {
                return;   // 只随自己的攻击红伤（K168 363 同门）
            }
            if (!onstage_has_soul_mark(c, owner, kMark1463) || c->is_boss_challenge) {
                return;
            }
            const int pct = live_db_at(live_row_db(c, owner, kMark1463), 0, 50);
            if (pct <= 0) {
                return;
            }
            (void)g_core->heal_amount(c, owner, d.final * pct / 100);   // 吃封回血
        });
    return EffectResult::kOk;
}

// ROUND_START：② 低体快照（「当回合开始时」严格低于 max/db[1]）。
EffectResult k406_1463_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMark1463)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMark1463);
    const int denom = db_at(row, 1, 2);   // db[1]，兜底字面 1/2
    const ElfPet& self = ctx->getPet(owner);
    const int max_hp = resolve_pet_max_hp(self);
    const bool low = self.hp > 0 && max_hp > 0 && denom > 0
                  && self.hp * denom < max_hp;   // 严格低于（恰 1/2 不结）
    write_int(ctx, k1463Base + owner * kOwnerStride + k1463LowFlag, low ? 1 : 0);
    return EffectResult::kOk;
}

// ROUND_END：② 每场 1 次机会 → 低体旗开且台账>0 → 附加台账×50% 百分比伤害。
EffectResult k406_1463_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMark1463)) {
        return EffectResult::kOk;
    }
    const int base = k1463Base + owner * kOwnerStride;
    const int low = read_int(ctx, base + k1463LowFlag);
    erase_int(ctx, base + k1463LowFlag);   // 显式消费/清账（K7 教训；次回合 RS 重写）
    if (low != 1) {
        return EffectResult::kOk;
    }
    if (read_int(ctx, base + k1463OnceUsed) >= k1463Chances) {
        return EffectResult::kOk;   // 每场 1 次机会已用（K404 1409 旗先例）
    }
    // 「上次攻击造成伤害」= 裸伤台账（battleFsm.cpp:907 覆写语义，记档）。
    const int raw = ctx->ws.raw_attack_damage[owner];
    const int amount = raw * k1463AttachPct / 100;
    if (amount <= 0) {
        return EffectResult::kOk;   // 本回合未攻击/0 伤 → 不触发不耗机会（触发=效果发生）
    }
    write_int(ctx, base + k1463OnceUsed, read_int(ctx, base + k1463OnceUsed) + 1);
    (void)g_core->deal_pink_damage(ctx, 1 - owner, amount, DamageKind::PERCENT_VALUE,
                                   owner);
    return EffectResult::kOk;
}

// ════════════════ 1464 伽玛·悍刀 ════════════════════════════════════════════

// enter(early)：接线命中旗 watcher（EVENT_HIT，攻/属皆发，K404 1638 同窗；一场一次）。
EffectResult k406_1464_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const int wired_key = k1464Base + owner * kOwnerStride + k1464Wired;
    if (read_int(ctx, wired_key) == 1) {
        return EffectResult::kOk;
    }
    write_int(ctx, wired_key, 1);
    ctx->event_center_.register_watcher(
        EventType::EVENT_HIT, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.actor != owner || c->is_boss_challenge) {
                return;
            }
            if (!onstage_has_soul_mark(c, owner, kMark1464)) {
                return;
            }
            write_int(c, k1464Base + owner * kOwnerStride + k1464HitFlag, 1);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ROUND_START：消费 ③ 封属戳（until==roundCount → seal duration=1 每回合窗，
// K399 1470① 同式；>0 守卫防缺失键撞首回合，K402 记档）。
EffectResult k406_1464_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !g_core || !g_core->seal_skill) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMark1464)) {
        return EffectResult::kOk;
    }
    const int seal_until = read_int(ctx, k1464Base + owner * kOwnerStride
                                            + k1464SealUntil);
    if (seal_until > 0 && seal_until <= ctx->roundCount) {
        if (seal_until == ctx->roundCount) {
            (void)g_core->seal_skill(ctx, /*source=*/owner, /*target=*/1 - owner,
                                     /*effect_id=*/k1464SealHandle,
                                     /*attribute=*/true, /*attack=*/false,
                                     /*count=*/0, /*duration_rounds=*/1,
                                     /*penetrable=*/true, /*source_slot=*/-1,
                                     EffectScope::ON_STAGE, /*hit_invalid=*/false,
                                     /*chance_pct=*/100,
                                     /*consumed_when_pierced=*/false);
        }
        erase_int(ctx, k1464Base + owner * kOwnerStride + k1464SealUntil);
    }
    return EffectResult::kOk;
}

// ROUND_END：① 命中旗开 → 附加/吸取（模式戳切换）；② 台账快照评低/高伤支 → 次回合戳。
EffectResult k406_1464_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMark1464) || ctx->is_boss_challenge) {
        return EffectResult::kOk;
    }
    const int base = k1464Base + owner * kOwnerStride;
    const HostRow row = host_row(ctx, a, kMark1464);
    const std::vector<int>* db = row.db;
    const int attach_denom = db_at(row, 0, 3);   // db[0]，兜底字面 1/3
    const int low_gate = db_at(row, 1, 300);     // db[1]，兜底本体字面 300
    const int drain_denom = db_at(row, 2, 3);    // db[2]，兜底本体字面 3
    const int high_gate = db_at(row, 3, 300);    // db[3]，兜底本体字面 300
    const int opp = 1 - owner;
    // ── ①/② 本回合模式结算（命中旗开才结）────────────────────────────────
    const int hit = read_int(ctx, base + k1464HitFlag);
    erase_int(ctx, base + k1464HitFlag);   // 消费即清（当回合语义）
    if (hit == 1) {
        const int drain_until = read_int(ctx, base + k1464DrainUntil);
        erase_int(ctx, base + k1464DrainUntil);   // 戳消费/过期自灭（K391 形）
        // ⚠️ >0 守卫：缺失键 read 回 0 撞首回合 rc=0 误入吸取档（K402/K405 记档坑，
        //   T7640k 实测 d=50279 证实——R1 直接走了 db[2]=2 吸取）。
        if (drain_until > 0 && drain_until == ctx->roundCount) {
            // ② 低伤支次回合：前述效果改为吸取 1/db[2]（drain_hp 原语直系）。
            if (drain_denom > 0 && g_core->drain_hp) {
                (void)g_core->drain_hp(ctx, owner, opp, drain_denom);
            }
        } else if (attach_denom > 0 && g_core->deal_pink_damage) {
            // ① 基线：附加对手 maxHP/db[0] 的百分比伤害（PERCENT_VALUE，K399 1020① 同 kind）。
            const int foe_max = resolve_pet_max_hp(ctx->getPet(opp));
            const int amount = foe_max / attach_denom;
            if (amount > 0) {
                (void)g_core->deal_pink_damage(ctx, opp, amount,
                                               DamageKind::PERCENT_VALUE, owner);
            }
        }
    } else {
        erase_int(ctx, base + k1464DrainUntil);   // 未命中回合戳照旧过期（显式清账）
    }
    // ── ②/③ 台账快照评档（独立 if，恰等阈值双不结，K402 ⑦；重叠双发 K405 2510）──
    const int raw = ctx->ws.raw_attack_damage[owner];   // 「当回合攻击造成的伤害」
    if (raw < low_gate && drain_denom > 0) {
        write_int(ctx, base + k1464DrainUntil, ctx->roundCount + 1);   // 次回合转吸取
    } else {
        erase_int(ctx, base + k1464DrainUntil);   // 非低伤回合显式回落基线（K7 教训）
    }
    if (raw > high_gate) {
        write_int(ctx, base + k1464SealUntil, ctx->roundCount + 1);   // 次回合封属
    } else {
        erase_int(ctx, base + k1464SealUntil);
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +3（1460/1463/1464 入账，只增不改）
// 三行（boss无效）均为 tips 末位尾注 → 全句尾归属 → 全节点 boss_invalid=true
//（记档【boss 尾注】；1463 尾注位于被跳 seg3 内仍按全句尾罩全节点）。
SOUL_PROGRAM(kMark1460, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k406_1460_arm,
                                  /*once=*/false, /*early=*/true, SoulScope::STAGE,
                                  /*boss_invalid=*/true),
                         SOUL_NODE(State::BATTLE_ROUND_START, &k406_1460_round_start,
                                   /*once=*/false, /*early=*/false, SoulScope::STAGE,
                                   /*boss_invalid=*/true),
                         SOUL_NODE(State::BATTLE_FIRST_MOVE_RIGHT, &k406_1460_fmr,
                                   /*once=*/false, /*early=*/false, SoulScope::STAGE,
                                   /*boss_invalid=*/true)});

SOUL_PROGRAM(kMark1463, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k406_1463_arm,
                                  /*once=*/false, /*early=*/true, SoulScope::STAGE,
                                  /*boss_invalid=*/true),
                         SOUL_NODE(State::BATTLE_ROUND_START, &k406_1463_round_start,
                                   /*once=*/false, /*early=*/false, SoulScope::STAGE,
                                   /*boss_invalid=*/true),
                         SOUL_NODE(State::BATTLE_ROUND_END, &k406_1463_round_end,
                                   /*once=*/false, /*early=*/false, SoulScope::STAGE,
                                   /*boss_invalid=*/true)});

SOUL_PROGRAM(kMark1464, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k406_1464_arm,
                                  /*once=*/false, /*early=*/true, SoulScope::STAGE,
                                  /*boss_invalid=*/true),
                         SOUL_NODE(State::BATTLE_ROUND_START, &k406_1464_round_start,
                                   /*once=*/false, /*early=*/false, SoulScope::STAGE,
                                   /*boss_invalid=*/true),
                         SOUL_NODE(State::BATTLE_ROUND_END, &k406_1464_round_end,
                                   /*once=*/false, /*early=*/false, SoulScope::STAGE,
                                   /*boss_invalid=*/true)});
