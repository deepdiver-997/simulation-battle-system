// K399 体力门复合输出族首批 —— effect 1020/1264/1470/1530（4 宏 / 8 行，
// 双行档位差；号段 7530-7539，worktree ../agents/k399，a01-knight3 2026-11-02）
//
// 行清单（tools/soul_effects_dump.py 1020 1264 1470 1530，2026-11-02）：
//   1020 圣灵之魂迈尔斯 3421 row732 args[25,2,50,2,4,1]（kind[3,5,14]）：
//     「攻击技能命中后附加自身最大体力值25%的百分比伤害；回合开始时，若自身体力高于
//     最大体力的1/2，则当回合受到的攻击伤害减少50%；战斗阶段结束时，若自身体力低于
//     最大体力的1/2，则恢复自身最大体力的1/4并造成等量百分比伤害（boss无效）」
//     row1031 伙伴行 args[30,2,60,2,3,1]：25→30 / 50→60 / 1/4→1/3。
//   1264 孙权 3685 row967 args[2,1,2,4,50,3,1]（kind[1,5,9]）：
//     「回合开始时若自身体力高于最大体力的1/2则当回合所有技能先制+1，若自身体力低于
//     最大体力的1/2则当回合自身攻击技能{吸取}对手最大{体力}的1/4；回合开始时若自身
//     处于能力提升状态则当回合自身造成的攻击伤害提升50%，若自身不处于能力提升状态则
//     当回合自身攻击技能减少对手最大体力的1/3（boss无效）」
//     row973 伙伴行 args[2,1,2,4,50,3,1,1]：前六参同、尾段多 db[7]=1，且行文末子句
//     "减少"→"{吸取}"（verb 升级，见下解码）。4399✓3685 旁证同构（4399 时点写作
//     "回合结束时…下回合"，tips 权威取"回合开始时…当回合"，记档）。
//   1470 暗夜·诺伊尔顿 3883 row1152 args[50,1,50,50,1,50,1]（kind[10,13,14]）：
//     「回合开始时，若自身满体力则当回合对手使用属性技能则50%无效，若自身体力未满则
//     当回合自身所有技能先制+1且附加自身已损失体力50%的固定伤害；|自身受到攻击时50%
//     令对手害怕，未触发则令对手全属性-1且下回合自身攻击50%令对手诅咒（boss无效）」
//     row1448 伙伴行 args[100,1,50,50,1,50,1]：50%→100% 无效。
//     ⚠️ effect 1470 = soul 空间（本宏）；moves 空间另有同号 1470（天赋威力）——
//     双注册空间常态（1004 先例），互不相干。
//   1530 极鳞魔兽 3935 row1199 args[50,50,3,3,3,1]（kind[1,3,5]）：
//     「战斗阶段结束时，若自身体力等于回合开始时则自身下回合受到的攻击伤害减少50%，
//     若自身体力高于回合开始时则自身自身下回合造成的攻击伤害提升50%，若自身体力低于
//     回合开始时则自身下回合攻击{吸取}对手最大{体力}的1/3，若当前体力低于最大体力的
//     1/3则下回合攻击先制+3（boss无效）」
//     row1270 伙伴行 args[60,60,3,3,3,1]：50→60（减/增两档）。
//
// ⚠️ 行≠模板：effect_info 四 id 全为技能空间异文模板（1020 未查得魂印模板/异文、
//    1264/1470/1530 同号异文）——tips 权威（K377 1354/K391 1497/K397 同款记档）。
//
// 行参解码（db_at 原序读 owned_int_args，两行互锚逐位与 tips 自洽）：
//   1020：db[0]=命中附加自身maxHP%（25/30）、db[1]=高体门分母（2→高于1/2）、
//         db[2]=受攻伤减少%（50/60）、db[3]=低体门分母（2→低于1/2）、
//         db[4]=战阶末回复分母（4/3→1/4、1/3）、db[5]=boss旗。
//   1264：db[0]=高体门分母（2）、db[1]=先制+1、db[2]=低体门分母（2）、
//         db[3]=吸取分母（1/4）、db[4]=有强化增伤%（50）、db[5]=无强化 1/N 分母（3）、
//         db[6]=boss旗、db[7]=无强化子句动词旗（仅 row973：1=吸取=deal+heal；
//         缺省=row967 仅"减少"=deal 不 heal——伙伴行净增强 majority 口径，记档）。
//   1470：db[0]=满体属技无效概率%（50/100）、db[1]=先制+1、db[2]=附加自身已损%（50）、
//         db[3]=受击害怕%（50）、db[4]=全属性降幅（1）、db[5]=下回合诅咒%（50）、
//         db[6]=boss旗。
//   1530：db[0]=等于档受攻伤减少%（50/60）、db[1]=高于档增伤%（50/60）、
//         db[2]=低于档吸取分母（1/3）、db[3]=低体门分母（1/3）、db[4]=先制+3、
//         db[5]=boss旗。
//
// 落法（通道=板行处方逐子句；K377/K391 体力门变档直系、K186 DETECT 附粉、
//       K394 ③/K395 881 pending 先制窗、K311/K343 封技通道、K229 快照账本）：
//   1020  ①命中附加：DETECT 票（enter early 臂，K186 1880② 同位）attackerId==own +
//         isRed + final>0 → deal_pink_damage(对手, 自身max×db[0]/100, PERCENT_VALUE)
//         （"百分比伤害"字面→PERCENT_VALUE，K242 max 分数档同 kind）。
//         ②高体减伤：ROUND_START hp>max/db[1]（严格档）→ ws.damage_reduce_add[own][0]
//         += db[2]（hundun 2411 自减伤槽直系，默认 REDUCE_PCT/MITIGATE 安装面消费、
//         只红伤；ws 每回合 reset → 条件持续每回合重写，K397 1139 同款）。
//         ③战阶末低体：ROUND_END（当次判，K190 口径）hp<max/db[3] → amount=max/db[4]
//         → heal_amount(own, amount) + deal_pink_damage(对手, amount, PERCENT_VALUE)
//         （"恢复…并造成等量…"语序：先回后打，K225 吸取双段值形）。
//   1264  ROUND_START 三门采样→账本（K229 显式写/清）：high=hp>max/db[0]（严格）、
//         low=hp<max/db[2]（严格）、boost=六槽任一正等级（K248 判据）。
//         ①先制：FMR 桶（memset 后写，K394 ⑤ 直系）high 旗 → ws.preemptive_level
//         [own] += db[1]，消费即清旗。
//         ②低体吸取：DETECT 票 low 旗（伤害时点读账本，回合内不重估）→
//         drain_hp(own→对手, db[3])（"吸取对手最大体力1/N"原语直系，deal FIXED+heal）。
//         ③有强化：ROUND_START 直接 ws.damage_add_pct[own] += db[4]（K397 1139 增伤
//         通道，仅红伤面）。
//         ④无强化：DETECT 票 !boost 旗 → db[7]==1 ? drain_hp(db[5]) :
//         deal_pink_damage(对手, 对手max/db[5], PERCENT_VALUE)。
//   1470  ①满体属技无效：ROUND_START hp>=max → seal_skill(attribute=true, count=0
//         不限次数, duration=1 每回合窗, 固定句柄刷新, chance_pct=db[0])——chance
//         语义=内核每次响应时掷（battle_primitives seal_skill：概率封属"每回合都有
//         机会封住"），100% 档 condition 恒真、50% 档逐次掷；"无效"= SEAL_ATTRIBUTE
//         非 hit_invalid（K311/K397-897 口径）。
//         ②未满先制+附加：ROUND_START hp<max → 账本旗；FMR 桶 → preemptive += db[1]；
//         DETECT 票（旗开）→ deal_pink_damage(对手, 自身已损×db[2]/100, FIXED)
//         （"固定伤害"字面→FIXED，K186 同 wording 同 kind；已损取 DETECT 现值）。
//         ③受击害怕：EVENT_TAKE_DAMAGE watcher（K199/K211 同门 + E12 NORMAL 红伤过滤
//         —— "受到攻击时"只认攻击伤害）→ apply_anomaly_checked(害怕6, db[3]%)，
//         ROLL_FAILED（内核单掷回执，K172/K242 口径）→ 全属性-db[4]（六槽含命中，
//         k242/k893 stat_drop 循环）+ 下回合诅咒旗（until=roundCount+1）。
//         ④下回合诅咒：EVENT_SKILL_USED watcher（own+攻击技）旗 until==roundCount →
//         apply_anomaly_checked(诅咒23, db[5]%)——逐次掷（"下回合自身攻击"每次攻击
//         各判，K164 每次攻击同款）。
//   1530  ROUND_START 双职责：先消费下回合戳（K391 ①' until==roundCount 精确窗形）：
//         reduce 戳→ws.damage_reduce_add[own][0]+=db[0]、amp 戳→ws.damage_add_pct
//         [own]+=db[1]，消费即清账（K7 教训）；再快照本回合 rsHp/rsRound（K229 账本）。
//         ROUND_END（当次判）三分档（快照 vs 现值，快照缺位/跨回合=不判）：
//         等于→reduce 戳 / 高于→amp 戳 / 低于→drain 戳（until=roundCount+1；
//         三档互斥）；独立第四句：hp<max/db[3] → pl 戳（amount=db[4], until+1，
//         与三分档可叠加）。
//         下回合：DETECT 票 drain 戳开 → drain_hp(own→对手, db[2])；FMR 桶 pl 戳 →
//         preemptive += db[4]。
//   boss 口径：四行 tips 尾注（boss无效）挂整串 → 全节点 boss_invalid=true（K395
//         K193 惯例）；enter 早挂的管线票/事件 watcher 不随节点走 → 票内手动
//         is_boss_challenge 门（K391 双保险）。
//   时点记档：1530 "回合开始时快照 vs 战阶末现值"照 K190「当次判」——快照只在
//         ROUND_START 采样、票/分支在消费点现读账本，回合内不重估。
//
// 三查记录（a01-knight3 2026-11-02 本 worktree 复核）：
//   ①内核 grep \b1020\b/\b1264\b/\b1470\b/\b1530\b 零命中（无魂印行为硬编码）；
//   ②effect_icon kind 已核（dump kind[3,5,14]/[1,5,9]/[10,13,14]/[1,3,5]）；effect_info
//     四 id 异文、tips 权威；
//   ③registry soul 基线 351 无四 id、soul_lib 全库无 SOUL_PROGRAM/SOUL_MARK 四 id、
//     无未合并分支实现（K397/K398 为并行族批、不涉四 id）——真缺口。4399 旁证：
//     3421/3685/3883/3935 四行文本与 tips 同构（1264 时点措辞差异记档如上）。
// 先例：k397_hp_compare_dual（体力门 ROUND_START 定分支+严格档+ws 写入）、
//     k391_order_fixed_seal_1497（下回合戳 until==roundCount 精确窗+票内 boss 门）、
//     k_dual_domain（FMR 桶 pending 先制 memset 后写+drain_hp）、k186（DETECT 附粉
//     相位/已损现值）、k229_hpcmp_mitigate_drain（快照账本显式写/清）、k311/k343
//     （SEAL_ATTRIBUTE 属性技无效每回合窗）、k242（ROLL_FAILED 补偿+全属性六槽
//     stat_drop 循环+吸取双段）、k172（申报制单掷+补偿）、k199/k211（TAKE_DAMAGE
//     watcher 现值口径）、hundun_diwang_2411（ws.damage_reduce_add 自减伤槽）、
//     k248（六槽正等级强化判据）。
// 场景回归：test/scenario/scenario_7530_hp_gate_output_family.cpp（号段 7530-7539）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>
#include <primitives/battle_primitives.h>

namespace {

constexpr int kMark1020 = 1020;
constexpr int kMark1264 = 1264;
constexpr int kMark1470 = 1470;
constexpr int kMark1530 = 1530;

constexpr int k1020WiredBase = 990399000;   // +owner：1020 DETECT 票接线旗
constexpr int k1264WiredBase = 990399010;   // +owner：1264 DETECT 票接线旗
constexpr int k1470WiredBase = 990399020;   // +owner：1470 票/watcher 接线旗
constexpr int k1530WiredBase = 990399030;   // +owner：1530 DETECT 票接线旗

// 1264 账本（ROUND_START 写，票/FMR 读；每回合显式重写）
constexpr int k1264HighBase = 990399100;    // +owner：高体旗（先制支）
constexpr int k1264LowBase = 990399110;     // +owner：低体旗（吸取支）
constexpr int k1264BoostBase = 990399120;   // +owner：有强化旗（增伤支/减少支取反）

// 1470 账本
constexpr int k1470NotFullBase = 990399200;  // +owner：未满旗（先制+附加支）
constexpr int k1470CurseUntilBase = 990399210;  // +owner：下回合诅咒窗（until）

// 1530 账本
constexpr int k1530RsHpBase = 990399300;     // +owner：回合开始体力快照
constexpr int k1530RsRoundBase = 990399310;  // +owner：快照所属回合（跨回合作废）
constexpr int k1530ReducePctBase = 990399320;   // +owner：等于档减伤戳 %
constexpr int k1530ReduceUntilBase = 990399330; // +owner：减伤戳生效回合
constexpr int k1530AmpPctBase = 990399340;      // +owner：高于档增伤戳 %
constexpr int k1530AmpUntilBase = 990399350;    // +owner：增伤戳生效回合
constexpr int k1530DrainUntilBase = 990399360;  // +owner：低于档吸取窗（until）
constexpr int k1530PlAmountBase = 990399370;    // +owner：先制+3 戳量
constexpr int k1530PlUntilBase = 990399380;     // +owner：先制戳生效回合

constexpr int k1470SealHandle = 147001;     // 固定句柄：每回合重授=刷新非追加（K311）
constexpr int kFearCode = static_cast<int>(AbnormalStatusId::Fear);     // 6
constexpr int kCurseCode = static_cast<int>(AbnormalStatusId::Curse);   // 23
constexpr int kCombatStatCount = 6;         // 六槽含命中（k242/k893 口径）

// ── 宿主行参数（db_at 原序读 owned_int_args，K364/K377/K397 HostRow 同款）──────
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

int flag_of(BattleContext* ctx, int key) {
    const auto it = ctx->plugin_storage.find(key);
    return it == ctx->plugin_storage.end() ? 0 : any_int_or(it->second, 0);
}

void set_flag(BattleContext* ctx, int key, int value) {
    ctx->plugin_storage[key] = value;
}

void clear_flag(BattleContext* ctx, int key) {
    ctx->plugin_storage.erase(key);
}

// 自身 maxHP 门（1020/1264 用）：strictly_above/below = 严格档（等号无分支，K377 口径）。
// 返回 1=高于 / 2=低于 / 0=等号或不可判。
int hp_gate(BattleContext* ctx, int owner, int denom_high, int denom_low) {
    const ElfPet& self = ctx->getPet(owner);
    const int max_hp = resolve_pet_max_hp(self);
    if (self.hp <= 0 || max_hp <= 0) {
        return 0;
    }
    if (denom_high > 0 && max_hp / denom_high > 0 && self.hp > max_hp / denom_high) {
        return 1;
    }
    if (denom_low > 0 && self.hp < max_hp / denom_low) {
        return 2;
    }
    return 0;
}

// 六槽任一正等级 = "处于能力提升状态"（K248 判据）。
bool side_has_any_boost(BattleContext* ctx, int side) {
    for (int i = 0; i < kCombatStatCount; ++i) {
        if (ctx->ability_levels[side][i] > 0) {
            return true;
        }
    }
    return false;
}

// 全属性-db[4]（六槽含命中；stat_drop 自查免弱，k242/k893 形）。
void drop_all_stats(BattleContext* ctx, int owner, int target, int amount) {
    if (!g_core || !g_core->stat_drop || amount <= 0) {
        return;
    }
    for (int i = 0; i < kCombatStatCount; ++i) {
        (void)g_core->stat_drop(ctx, target, i, amount, owner);
    }
}

// ════════════════════ 1020 圣灵之魂迈尔斯 ════════════════════

// enter(early)：接线 ① 命中附加票（DETECT 相位，K186 1880② 同位）。
EffectResult k399_1020_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const int key = k1020WiredBase + owner;
    if (ctx->plugin_storage.count(key)) {
        return EffectResult::kOk;   // early 双触发防重（K391 同款）
    }
    ctx->plugin_storage.emplace(key, 1);
    const HostRow row = host_row(ctx, a, kMark1020);
    const int attach_pct = db_at(row, 0, 25);
    if (attach_pct <= 0) {
        return EffectResult::kOk;
    }
    ctx->register_default_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner, attach_pct](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner || !g_core || !g_core->deal_pink_damage) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || !d.isRed || d.final <= 0) {
                return;   // 只随自己的攻击红伤附加（"命中后"）
            }
            if (!onstage_has_soul_mark(c, owner, kMark1020) || c->is_boss_challenge) {
                return;
            }
            const int max_hp = resolve_pet_max_hp(c->getPet(owner));
            const int attach = max_hp * attach_pct / 100;
            if (attach > 0) {
                g_core->deal_pink_damage(c, c->opponent(owner), attach,
                                         DamageKind::PERCENT_VALUE, owner);
            }
        });
    return EffectResult::kOk;
}

// ② ROUND_START：高体（>max/db[1] 严格）→ 当回合受攻伤减少 db[2]%。
EffectResult k399_1020_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    if (!onstage_has_soul_mark(ctx, a.owner, kMark1020)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMark1020);
    const int gate_denom = db_at(row, 1, 2);
    const int reduce_pct = db_at(row, 2, 50);
    if (hp_gate(ctx, a.owner, gate_denom, 0) == 1 && reduce_pct > 0) {
        ctx->ws.damage_reduce_add[a.owner][0] += reduce_pct;
    }
    return EffectResult::kOk;
}

// ③ ROUND_END：低体（<max/db[3] 严格，当次判）→ 回复 1/db[4] + 等量粉伤。
EffectResult k399_1020_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMark1020)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMark1020);
    const int gate_denom = db_at(row, 3, 2);
    const int heal_denom = db_at(row, 4, 4);
    if (hp_gate(ctx, owner, 0, gate_denom) != 2) {
        return EffectResult::kOk;
    }
    const int max_hp = resolve_pet_max_hp(ctx->getPet(owner));
    const int amount = heal_denom > 0 ? max_hp / heal_denom : 0;
    if (amount <= 0) {
        return EffectResult::kOk;
    }
    if (g_core->heal_amount) {
        (void)g_core->heal_amount(ctx, owner, amount);
    }
    (void)g_core->deal_pink_damage(ctx, ctx->opponent(owner), amount,
                                   DamageKind::PERCENT_VALUE, owner);
    return EffectResult::kOk;
}

// ════════════════════ 1264 孙权 ════════════════════

// ROUND_START：三门采样 → 账本（显式写/清，K229 口径）+ ③ 有强化增伤直写 ws。
EffectResult k399_1264_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    if (!onstage_has_soul_mark(ctx, a.owner, kMark1264)) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const HostRow row = host_row(ctx, a, kMark1264);
    const int high_denom = db_at(row, 0, 2);
    const int low_denom = db_at(row, 2, 2);
    const int boost_amp = db_at(row, 4, 50);
    const int gate = hp_gate(ctx, owner, high_denom, low_denom);
    set_flag(ctx, k1264HighBase + owner, gate == 1 ? 1 : 0);
    set_flag(ctx, k1264LowBase + owner, gate == 2 ? 1 : 0);
    const bool boost = side_has_any_boost(ctx, owner);
    set_flag(ctx, k1264BoostBase + owner, boost ? 1 : 0);
    if (boost && boost_amp > 0) {
        ctx->ws.damage_add_pct[owner] += static_cast<float>(boost_amp);
    }
    return EffectResult::kOk;
}

// FMR：高体旗 → 当回合全技先制 +db[1]（memset 后写，K394 ⑤ 直系），消费即清旗。
EffectResult k399_1264_fmr(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    if (!onstage_has_soul_mark(ctx, a.owner, kMark1264)) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (flag_of(ctx, k1264HighBase + owner) == 1) {
        const HostRow row = host_row(ctx, a, kMark1264);
        const int pl = db_at(row, 1, 1);
        if (pl > 0) {
            ctx->ws.preemptive_level[owner] += pl;   // "所有技能"——无攻击技门
        }
    }
    clear_flag(ctx, k1264HighBase + owner);
    return EffectResult::kOk;
}

// enter(early)：接线 ②④ 攻击吸取/减少票（DETECT，旗在伤害时点读账本）。
EffectResult k399_1264_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const int key = k1264WiredBase + owner;
    if (ctx->plugin_storage.count(key)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.emplace(key, 1);
    const HostRow row = host_row(ctx, a, kMark1264);
    const int drain_denom = db_at(row, 3, 4);
    const int reduce_denom = db_at(row, 5, 3);
    const int drain_verb = db_at(row, 7, 0);   // row973=1 吸取 / row967 缺省=仅减少
    ctx->register_default_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner, drain_denom, reduce_denom, drain_verb](BattleContext* c,
                                                       int bucket_owner) {
            if (!c || bucket_owner != owner || !g_core) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || !d.isRed || d.final <= 0) {
                return;   // 只随自己的攻击红伤附加
            }
            if (!onstage_has_soul_mark(c, owner, kMark1264) || c->is_boss_challenge) {
                return;
            }
            const int opp = c->opponent(owner);
            // ② 低体旗 → 吸取对手最大体力 1/db[3]（drain_hp 原语：deal FIXED+heal）
            if (flag_of(c, k1264LowBase + owner) == 1 && drain_denom > 0
                && g_core->drain_hp) {
                (void)g_core->drain_hp(c, owner, opp, drain_denom);
            }
            // ④ 无强化旗 → 减少/吸取对手最大体力 1/db[5]
            if (flag_of(c, k1264BoostBase + owner) != 1 && reduce_denom > 0) {
                const int foe_max = resolve_pet_max_hp(c->getPet(opp));
                const int amount = foe_max / reduce_denom;
                if (amount > 0) {
                    if (drain_verb == 1 && g_core->drain_hp) {
                        (void)g_core->drain_hp(c, owner, opp, reduce_denom);
                    } else if (g_core->deal_pink_damage) {
                        (void)g_core->deal_pink_damage(c, opp, amount,
                                                       DamageKind::PERCENT_VALUE,
                                                       owner);
                    }
                }
            }
        });
    return EffectResult::kOk;
}

// ════════════════════ 1470 暗夜·诺伊尔顿 ════════════════════

// enter(early)：接线 ② 附加票（DETECT）+ ③ 受击害怕 watcher + ④ 诅咒 watcher。
EffectResult k399_1470_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const int key = k1470WiredBase + owner;
    if (ctx->plugin_storage.count(key)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.emplace(key, 1);
    const HostRow row = host_row(ctx, a, kMark1470);
    const int attach_pct = db_at(row, 2, 50);
    const int fear_pct = db_at(row, 3, 50);
    const int drop_amount = db_at(row, 4, 1);
    const int curse_pct = db_at(row, 5, 50);

    // ② 未满旗开 → 附加自身已损×db[2]% 固定伤害（DETECT 现值，K186 同款）
    if (attach_pct > 0) {
        ctx->register_default_damage_effect(
            DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
            [owner, attach_pct](BattleContext* c, int bucket_owner) {
                if (!c || bucket_owner != owner || !g_core
                    || !g_core->deal_pink_damage) {
                    return;
                }
                const DamageSnapshot& d = c->resolvedDamage;
                if (d.attackerId != owner || !d.isRed || d.final <= 0) {
                    return;
                }
                if (!onstage_has_soul_mark(c, owner, kMark1470)
                    || c->is_boss_challenge) {
                    return;
                }
                if (flag_of(c, k1470NotFullBase + owner) != 1) {
                    return;   // 满体回合无附加
                }
                const int max_hp = resolve_pet_max_hp(c->getPet(owner));
                const int lost = max_hp - c->getPet(owner).hp;
                const int attach = lost * attach_pct / 100;
                if (attach > 0) {
                    g_core->deal_pink_damage(c, c->opponent(owner), attach,
                                             DamageKind::FIXED, owner);
                }
            });
    }
    // ③ 受击害怕（"受到攻击时"= E12 NORMAL 红伤过滤）+ ROLL_FAILED 补偿
    if (fear_pct > 0) {
        ctx->event_center_.register_watcher(
            EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount,
            /*duration_rounds=*/0, /*once=*/false,
            [owner, fear_pct, drop_amount](BattleContext* c, const BattleEvent& ev) {
                if (!c || !g_core || !g_core->apply_anomaly_checked || !g_core->stat_drop) {
                    return;
                }
                if (ev.target != owner || ev.actor == owner || ev.amount <= 0) {
                    return;   // 仅对手造成的实际伤害（K199/K211 现值口径）
                }
                if (ev.damage_kind != static_cast<int>(DamageKind::NORMAL)) {
                    return;   // "受到攻击"只认攻击伤害（E12 反伤族过滤口径）
                }
                if (!onstage_has_soul_mark(c, owner, kMark1470)
                    || c->is_boss_challenge) {
                    return;
                }
                const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
                    c, c->opponent(owner), kFearCode, random_anomaly_duration(),
                    owner, fear_pct, ChanceSource::SoulMark);
                if (r != ApplyAnomalyResult::ROLL_FAILED) {
                    return;
                }
                // 未触发：全属性-db[4] + 下回合诅咒旗
                drop_all_stats(c, owner, c->opponent(owner), drop_amount);
                set_flag(c, k1470CurseUntilBase + owner, c->roundCount + 1);
            },
            WatcherScope::TEAM);
    }
    // ④ 下回合自身攻击 db[5]% 令对手诅咒（逐次掷）
    if (curse_pct > 0) {
        ctx->event_center_.register_watcher(
            EventType::EVENT_SKILL_USED, owner, ctx->roundCount,
            /*duration_rounds=*/0, /*once=*/false,
            [owner, curse_pct](BattleContext* c, const BattleEvent& ev) {
                if (!c || !g_core || !g_core->apply_anomaly_checked) {
                    return;
                }
                if (ev.actor != owner || ev.used_attribute) {
                    return;   // own + 攻击技（K164 116 同门；miss 也算使用）
                }
                if (!onstage_has_soul_mark(c, owner, kMark1470)
                    || c->is_boss_challenge) {
                    return;
                }
                if (flag_of(c, k1470CurseUntilBase + owner) != c->roundCount) {
                    return;   // 只在"下回合"窗内
                }
                (void)g_core->apply_anomaly_checked(
                    c, c->opponent(owner), kCurseCode, random_anomaly_duration(),
                    owner, curse_pct, ChanceSource::SoulMark);
            },
            WatcherScope::TEAM);
    }
    return EffectResult::kOk;
}

// ① ROUND_START：满体 → 属技无效封属窗（chance_pct 逐次响应掷）；未满 → 账本旗。
EffectResult k399_1470_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMark1470)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMark1470);
    const int seal_pct = db_at(row, 0, 50);
    // 诅咒窗过期清账（until < roundCount = 窗已过；== 保持当回合生效）
    const int curse_until = flag_of(ctx, k1470CurseUntilBase + owner);
    if (curse_until != 0 && curse_until < ctx->roundCount) {
        clear_flag(ctx, k1470CurseUntilBase + owner);
    }
    const ElfPet& self = ctx->getPet(owner);
    const int max_hp = resolve_pet_max_hp(self);
    const bool full = self.hp > 0 && max_hp > 0 && self.hp >= max_hp;
    if (full) {
        // 属性技"无效"= SEAL_ATTRIBUTE（K311 口径）；count=0 不限次数、duration=1
        // 每回合窗、固定句柄重复授予=刷新；chance_pct<100 → 内核每次响应时掷。
        if (seal_pct > 0 && g_core->seal_skill) {
            (void)g_core->seal_skill(ctx, /*source=*/owner, /*target=*/1 - owner,
                                     /*effect_id=*/k1470SealHandle,
                                     /*attribute=*/true, /*attack=*/false,
                                     /*count=*/0, /*duration_rounds=*/1,
                                     /*penetrable=*/true, /*source_slot=*/-1,
                                     EffectScope::ON_STAGE, /*hit_invalid=*/false,
                                     /*chance_pct=*/seal_pct,
                                     /*consumed_when_pierced=*/false);
        }
        clear_flag(ctx, k1470NotFullBase + owner);
    } else {
        set_flag(ctx, k1470NotFullBase + owner, 1);
    }
    return EffectResult::kOk;
}

// FMR：未满旗 → 当回合全技先制 +db[1]（memset 后写）。⚠️ 旗不清——同一旗还
// gating 本回合 DETECT 附加票（FMR 先于攻击相），下回合 ROUND_START 重写。
EffectResult k399_1470_fmr(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    if (!onstage_has_soul_mark(ctx, a.owner, kMark1470)) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (flag_of(ctx, k1470NotFullBase + owner) == 1) {
        const HostRow row = host_row(ctx, a, kMark1470);
        const int pl = db_at(row, 1, 1);
        if (pl > 0) {
            ctx->ws.preemptive_level[owner] += pl;
        }
    }
    return EffectResult::kOk;
}

// ════════════════════ 1530 极鳞魔兽 ════════════════════

// ROUND_START：先消费下回合戳（reduce→ws 减伤槽 / amp→ws 增伤槽），再快照本回合。
EffectResult k399_1530_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    if (!onstage_has_soul_mark(ctx, a.owner, kMark1530)) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    // 吸取窗过期清账（until < roundCount = 窗已过；== 保持当回合生效）
    const int drain_until_stale = flag_of(ctx, k1530DrainUntilBase + owner);
    if (drain_until_stale != 0 && drain_until_stale < ctx->roundCount) {
        clear_flag(ctx, k1530DrainUntilBase + owner);
    }
    // ── 消费下回合戳（until==roundCount 精确窗，K391 ①' 口径；消费即清账）──
    const int reduce_pct = flag_of(ctx, k1530ReducePctBase + owner);
    const int reduce_until = flag_of(ctx, k1530ReduceUntilBase + owner);
    if (reduce_pct > 0 && reduce_until == ctx->roundCount) {
        ctx->ws.damage_reduce_add[owner][0] += reduce_pct;
    }
    clear_flag(ctx, k1530ReducePctBase + owner);
    clear_flag(ctx, k1530ReduceUntilBase + owner);
    const int amp_pct = flag_of(ctx, k1530AmpPctBase + owner);
    const int amp_until = flag_of(ctx, k1530AmpUntilBase + owner);
    if (amp_pct > 0 && amp_until == ctx->roundCount) {
        ctx->ws.damage_add_pct[owner] += static_cast<float>(amp_pct);
    }
    clear_flag(ctx, k1530AmpPctBase + owner);
    clear_flag(ctx, k1530AmpUntilBase + owner);
    // ── 本回合快照（K229 账本；回合号随写，跨回合作废）──
    set_flag(ctx, k1530RsHpBase + owner, ctx->getPet(owner).hp);
    set_flag(ctx, k1530RsRoundBase + owner, ctx->roundCount);
    return EffectResult::kOk;
}

// ROUND_END：快照 vs 现值三分档（互斥）+ 独立低体先制句；戳 until=下回合。
EffectResult k399_1530_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMark1530)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMark1530);
    const int reduce_pct = db_at(row, 0, 50);
    const int amp_pct = db_at(row, 1, 50);
    const int drain_denom = db_at(row, 2, 3);
    const int low_denom = db_at(row, 3, 3);
    const int pl_amount = db_at(row, 4, 3);
    const ElfPet& self = ctx->getPet(owner);
    if (self.hp <= 0) {
        return EffectResult::kOk;
    }
    // 三分档：快照属于本回合才判（快照缺位/跨回合=不判，时点记档 K190 当次判）
    if (flag_of(ctx, k1530RsRoundBase + owner) == ctx->roundCount) {
        const int rs_hp = flag_of(ctx, k1530RsHpBase + owner);
        const int until = ctx->roundCount + 1;
        if (self.hp == rs_hp) {
            if (reduce_pct > 0) {
                set_flag(ctx, k1530ReducePctBase + owner, reduce_pct);
                set_flag(ctx, k1530ReduceUntilBase + owner, until);
            }
        } else if (self.hp > rs_hp) {
            if (amp_pct > 0) {
                set_flag(ctx, k1530AmpPctBase + owner, amp_pct);
                set_flag(ctx, k1530AmpUntilBase + owner, until);
            }
        } else {
            if (drain_denom > 0) {
                set_flag(ctx, k1530DrainUntilBase + owner, until);
            }
        }
    }
    clear_flag(ctx, k1530RsRoundBase + owner);   // 快照消费即清（K7 显式清账）
    // 独立第四句：当前体力 < max/db[3] → 下回合攻击先制 +db[4]
    const int max_hp = resolve_pet_max_hp(self);
    if (low_denom > 0 && max_hp > 0 && self.hp < max_hp / low_denom && pl_amount > 0) {
        set_flag(ctx, k1530PlAmountBase + owner, pl_amount);
        set_flag(ctx, k1530PlUntilBase + owner, ctx->roundCount + 1);
    }
    return EffectResult::kOk;
}

// FMR：pl 戳 → 当回合全技先制 +db[4]（memset 后写），消费即清。
EffectResult k399_1530_fmr(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    if (!onstage_has_soul_mark(ctx, a.owner, kMark1530)) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const int pl_until = flag_of(ctx, k1530PlUntilBase + owner);
    if (pl_until == ctx->roundCount) {
        const int amount = flag_of(ctx, k1530PlAmountBase + owner);
        if (amount > 0) {
            ctx->ws.preemptive_level[owner] += amount;
        }
    }
    clear_flag(ctx, k1530PlAmountBase + owner);
    clear_flag(ctx, k1530PlUntilBase + owner);
    return EffectResult::kOk;
}

// enter(early)：接线低于档吸取票（DETECT，drain 戳在下回合窗内逐次判）。
EffectResult k399_1530_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const int key = k1530WiredBase + owner;
    if (ctx->plugin_storage.count(key)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.emplace(key, 1);
    const HostRow row = host_row(ctx, a, kMark1530);
    const int drain_denom = db_at(row, 2, 3);
    ctx->register_default_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner, drain_denom](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner || !g_core || !g_core->drain_hp) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || !d.isRed || d.final <= 0) {
                return;   // 只随自己的攻击红伤附加
            }
            if (!onstage_has_soul_mark(c, owner, kMark1530) || c->is_boss_challenge) {
                return;
            }
            if (flag_of(c, k1530DrainUntilBase + owner) != c->roundCount) {
                return;   // 只在"下回合"窗内
            }
            (void)g_core->drain_hp(c, owner, c->opponent(owner), drain_denom);
        });
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +4（1020/1264/1470/1530 入账，
//    只增不改）────────────────────────────────────────────────────────────────────
SOUL_PROGRAM(kMark1020, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k399_1020_arm,
                               /*once=*/false, /*early=*/true, SoulScope::STAGE,
                               /*boss_invalid=*/true),
                         SOUL_NODE(State::BATTLE_ROUND_START, &k399_1020_round_start,
                               /*once=*/false, /*early=*/false, SoulScope::STAGE,
                               /*boss_invalid=*/true),
                         SOUL_NODE(State::BATTLE_ROUND_END, &k399_1020_round_end,
                               /*once=*/false, /*early=*/false, SoulScope::STAGE,
                               /*boss_invalid=*/true)});
SOUL_PROGRAM(kMark1264, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k399_1264_arm,
                               /*once=*/false, /*early=*/true, SoulScope::STAGE,
                               /*boss_invalid=*/true),
                         SOUL_NODE(State::BATTLE_ROUND_START, &k399_1264_round_start,
                               /*once=*/false, /*early=*/false, SoulScope::STAGE,
                               /*boss_invalid=*/true),
                         SOUL_NODE(State::BATTLE_FIRST_MOVE_RIGHT, &k399_1264_fmr,
                               /*once=*/false, /*early=*/false, SoulScope::STAGE,
                               /*boss_invalid=*/true)});
SOUL_PROGRAM(kMark1470, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k399_1470_arm,
                               /*once=*/false, /*early=*/true, SoulScope::STAGE,
                               /*boss_invalid=*/true),
                         SOUL_NODE(State::BATTLE_ROUND_START, &k399_1470_round_start,
                               /*once=*/false, /*early=*/false, SoulScope::STAGE,
                               /*boss_invalid=*/true),
                         SOUL_NODE(State::BATTLE_FIRST_MOVE_RIGHT, &k399_1470_fmr,
                               /*once=*/false, /*early=*/false, SoulScope::STAGE,
                               /*boss_invalid=*/true)});
SOUL_PROGRAM(kMark1530, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k399_1530_arm,
                               /*once=*/false, /*early=*/true, SoulScope::STAGE,
                               /*boss_invalid=*/true),
                         SOUL_NODE(State::BATTLE_ROUND_START, &k399_1530_round_start,
                               /*once=*/false, /*early=*/false, SoulScope::STAGE,
                               /*boss_invalid=*/true),
                         SOUL_NODE(State::BATTLE_ROUND_END, &k399_1530_round_end,
                               /*once=*/false, /*early=*/false, SoulScope::STAGE,
                               /*boss_invalid=*/true),
                         SOUL_NODE(State::BATTLE_FIRST_MOVE_RIGHT, &k399_1530_fmr,
                               /*once=*/false, /*early=*/false, SoulScope::STAGE,
                               /*boss_invalid=*/true)});
