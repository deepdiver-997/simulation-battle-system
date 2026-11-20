// K398 PP 归零/压制族首批 —— effect 848/759/918/1430（4 宏 / 4 行，
// 池 122 行/118 宏首刀·仅「减量/归零/恢复」侧，消耗翻倍/上限侧在内核缺口箱不入；
// 号段 7520-7529，worktree ../agents/k398，a01-knight2 2026-11-02）
//
// 行清单（tools/soul_effects_dump.py 848 759 918 1430，2026-11-02）：
//   848 塞纳留斯 3236 row559 args[2,2]（kind[15]）：「每次登场时若自身体力大于1/2则
//     使对手所有技能PP值减少2点」
//   759 雷暴·利斯塔 3097 row444 args[50,2,1]（kind[9,15]）：「每回合开始时都有50%的
//     概率使自身先制额外+2；先出手时，使对手随机1个技能的PP值归零」
//   918 万世北溟·玄武 3308 row635 args[1,25,1]（kind[5,7,15]）：「自身不处于双防强化
//     状态时免疫异常状态；若自身先出手则使对手随机1个技能的PP值归零，若自身后出手则
//     附加自身双防能力值总和25%的固定伤害并恢复等量体力（BOSS无效）」
//   1430 丝萝 3842 row1115 args[2,1,100,400,200,100,300,1]（kind[5,10,15]）：
//     「回合开始时若自身为满体力则令对手所有技能PP值-2，若自身未满体力则令对手所有
//     技能PP值-1；战斗阶段结束时随机恢复自身100-400点体力，恢复的体力值低于200则
//     100%令对手睡眠，恢复的体力值高于300则下回合自身攻击技能命中后附加恢复量100%
//     的百分比伤害（boss无效）」
//   本批 4 宏各只此 1 行（dump 实证）——行参逐位与 tips 字面互锚（见各解码），
//   实现零硬编码行数值（fallback 常量仅为 tips 字面锚，K316 394 同纪律）。
//
// ⚠️ 行≠模板：effect_info 四 id 全为技能空间异文模板（848 降属附伤 / 759 消升态 /
//    918 伤害不足提升 / 1430 伤害低于异常）——tips 权威（K391 1497/K397 同款记档）。
//
// 行参解码（db_at 原序读 owned_int_args——方案 B 禁读 a.row，K257 起夜跑口径）：
//   848 ：db[0]=体力阈值分母（"大于1/2"的 2）、db[1]=PP 减少量（"减少2点"）——
//         tips 行文序两字面 ↔ 两参逐位锚定；单行无第二行互锚，按字面序解码记档。
//   759 ：db[0]=先制概率%（"50%"）、db[1]=先制增量（"+2"）、db[2]=随机归零技数
//         （"随机1个"）——行文序逐位锚定。
//   918 ：db[0]=先手随机归零技数（"随机1个"）、db[1]=后手固伤比%（"25%"）、
//         db[2]=1 旗不消费（记档）——tips 序：①免疫无数值 ②先手"随机1个"→1
//         ③后手"25%"→25，db[2] 无字面锚（K2 纪律）。
//   1430：db[0]=满体全技-2、db[1]=未满-1、db[2]/db[3]=恢复区间 [100,400]、
//         db[4]=睡眠门（低于 200）、db[5]=附加比%（100%）、db[6]=下回合附加门
//         （高于 300）、db[7]=1 旗不消费（记档）——8 字面 ↔ 8 参逐位互锚。
//
// 落法（板行处方：通道=K321 360 全体压制 / K316 394 后手压制 / K325 643 复合直系；
// pp_reduce/pp_zero_slot 原语=K6 1576 先例；变档=K391 1497 先例；逐子句）：
//   848    ENTER 节点（"每次登场"，OPERATION_ENTER_EXIT_STAGE；部署段首登场同桶
//          执行，K171 285 enter 时序）：hp×db[0] > max（先乘后除防截断，K370 口径；
//          "大于"= 严格）→ pp_reduce(对手全技, db[1])（原语全技循环/钳 0/emit
//          EVENT_PP_REDUCED，K13 1896 同款）。无 boss 尾注 → boss_invalid=false
//          （boss 局照生效）。
//   759①   ROUND_START：roll_percent(db[0])（本地掷——先制窗不经施加通道、无亮节
//          闸门可申报，K316 394 口径）→ 审计槽记掷点（F9 782 技巧）+ 置先制门旗。
//   759①'  FMR（BATTLE_FIRST_MOVE_RIGHT 双侧桶都跑、preemptive_level memset 后写，
//          K378 1355 形）：门旗在 → ws.preemptive_level[owner] += db[1]，旗消费。
//   759②   ROUND_END：!is_second_mover(owner)（"先出手时"，先手权 FMR 已结算——
//          K316"后手门"的对称读法）→ pp_zero_slot 随机槽循环凑 db[2] 个（K230
//          1562/K378 同款 guard 循环；无限 PP 槽原语拒、已 0 槽幂等成功）。无
//          boss 尾注 → boss_invalid=false。
//   918①   enter + ROUND_START 双节点 grant_immunity(ANOMALY, coverage_all,
//          duration=1, soul=true, Modern)（K171 285 直系）——"不处于双防强化状态"
//          门在 condition 现场判（逐次施加时点，K171 同款）："处于"= 双防都 >0
//          （ability_levels[2]>0 && [3]>0，K357 1505 双防判据）；无 boss 尾注。
//   918②   ROUND_END：!is_second_mover → pp_zero_slot ×db[0]（759② 同构）。无
//          boss 尾注 → boss_invalid=false。
//   918③   ROUND_END：is_second_mover → amount=双防面板和×db[1]/100（numericalBase
//          DEFENSE+SPECIAL_DEFENSE，K233 844 口径）→ deal_pink(对手, amount, FIXED)
//          + heal_amount(自身, amount)（"恢复等量"，原语吃封回血）。（BOSS无效）
//          尾注挂此分句 → 节点 boss_invalid=true。
//   1430①  ROUND_START：满体力（hp>=max）→ pp_reduce(对手全技, db[0])，未满 →
//          pp_reduce(db[1])（变档=K391 双档读法）。无 boss 尾注。
//   1430②  ROUND_END（"战斗阶段结束"，K178 351 同桶）：amount=db[2]+rand%
//          (db[3]-db[2]+1) → 审计槽记额 → heal_amount(自身, amount)；
//          amount<db[4] → apply_anomaly_checked(对手, 睡眠8, 申报 100%, SoulMark)
//          （"100%"为 tips 字面、args 无对应位 → 常量锚；100 档内核短路不耗
//          rand，K176 口径）；amount>db[6] → 戳 stamp=amount、until=roundCount+1
//          （"下回合"==until 精确窗，K391 形）——boss 局不写（尾注挂第三分句，
//          本节点内手动门：heal/睡眠照常、仅戳不写）。
//   1430③  DETECT 票（arm@enter early，K391 1497 同位）：own 红伤命中（attackerId
//          ==owner+isRed+final>0；"命中后"→ miss 无伤害管线不触发，属性技无伤害
//          相天然排除）→ 读戳 roundCount==until → deal_pink(对手, stamp×db[5]%,
//          PERCENT_VALUE)（"百分比伤害"指定值档：amount 已是具体值）→ 消费清戳。
//          boss_invalid=true + 票内手动 is_boss_challenge 双保险（K391 管线票口径）。
//   boss 归属（按行文子句判，板行"全池 boss 尾注→逐行 boss_invalid"在本批落为
//   子句级）：848/759①②/918①②/1430①② 无尾注不置位；918③（BOSS无效）、1430③
//   （boss无效，尾注收尾于第三分句）置位；1430② 的戳写入属第三分句条件段 → 节点内
//   手动 boss 门。
//
// 三查记录（a01-knight2 2026-11-02，worktree ../agents/k398）：
//   ①内核 grep \b848\b/\b759\b/\b918\b/\b1430\b 零命中（无魂印行为硬编码）；
//   ②effect_icon kind=[15]/[9,15]/[5,7,15]/[5,10,15]、tips 为权威文（dump 原文
//     已核；effect_info 四 id 均技能空间异文模板，见上）；③registry soul 集合
//     （build/registry.json 351 dump 复核）无 848/759/918/1430、soul_lib 全库无
//     同 id 注册——真缺口。池内 1665/1856 双行本体在内核缺口箱，本单不拉（板行记档）。
// 先例：k321_pp_suppress_all 360（全技 pp 压制/pp_reduce）、k316_second_pp_cut 394 /
//   k325_second_pp2_heal 643（后手门+本地掷+ROUND_END 桶+db_at 形）、
//   k_hitbreak_ice_pp_preempt 1355 / k230_anomaly_shield_pp 1562（pp_zero_slot
//   随机槽 guard 循环）、k_fullhp_immune_family 285（条件免疫票 condition 现场
//   判）、k233_dualdef_fixed_heal 844（双防面板和+FIXED 附伤+等量回）、
//   k391_order_fixed_seal_1497（变档/until 戳/DETECT 票双保险）、k370（先乘后除
//   体力门）、k178（"战阶结束"ROUND_END 桶）。
// 场景回归：test/scenario/scenario_7520_pp_zero_suppress_first.cpp（7520-7529）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>
#include <primitives/battle_primitives.h>

namespace {

// ── 审计/账本键段（990398xxx，grep 全库无撞）────────────────────────────────
constexpr int kWire1430Base = 990398000;  // +owner：1430③ DETECT 票接线旗（arm-once）
constexpr int kAudit759Base = 990398100;  // +owner：759① 掷点审计槽（0/1，整回合可读）
constexpr int kFlag759Base  = 990398200;  // +owner：759① 先制门旗（FMR 消费 erase）
constexpr int kAudit1430Base = 990398300; // +owner：1430② 恢复额审计槽（每 ROUND_END 覆写）
constexpr int kStamp1430Base = 990398400; // +owner：1430② 下回合附加戳（amount）
constexpr int kUntil1430Base = 990398500; // +owner：1430② 戳生效回合（==until 判）

// tips 字面锚（args 缺位时的 fallback，K316 kChanceFallback 同纪律）
constexpr int k848DenomFallback  = 2;    // "大于1/2"
constexpr int k848CutFallback    = 2;    // "减少2点"
constexpr int k759PctFallback    = 50;   // "50%"
constexpr int k759PreemptFallback = 2;   // "先制额外+2"
constexpr int k759CountFallback  = 1;    // "随机1个技能"
constexpr int k918CountFallback  = 1;    // "随机1个技能"
constexpr int k918PctFallback    = 25;   // "25%"
constexpr int k1430FullFallback  = 2;    // "PP值-2"
constexpr int k1430NotFallback   = 1;    // "PP值-1"
constexpr int k1430LoFallback    = 100;  // "100-400"
constexpr int k1430HiFallback    = 400;
constexpr int k1430SleepGateFallback = 200; // "低于200"
constexpr int k1430AttachPctFallback = 100; // "恢复量100%"
constexpr int k1430StampGateFallback = 300; // "高于300"
constexpr int k1430SleepPct      = 100;  // "100%令对手睡眠"（tips 字面，args 无对应位）

// ── 宿主行参数（方案 B：db_at 读 owned_int_args 原序，K316/K391 同款私副本）──
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

int flag_of(BattleContext* ctx, int key) {
    const auto it = ctx->plugin_storage.find(key);
    return it == ctx->plugin_storage.end() ? 0 : any_int_or(it->second, 0);
}

// 随机槽 PP 归零 ×count（K230 1562/K378 1355 同款 guard 循环：
// 无限 PP 槽原语拒并重试、已 0 槽幂等成功）。
void pp_zero_random_slots(BattleContext* ctx, int target, int actor, int count) {
    if (!ctx || !g_core || !g_core->pp_zero_slot || count <= 0) {
        return;
    }
    int done = 0;
    int guard = 0;
    while (done < count && guard < 100) {
        ++guard;
        const int slot = sbs_rand() % 5;   // 5 技能槽
        if (g_core->pp_zero_slot(ctx, target, slot, actor) ==
            PpReduceResult::SUCCESS) {
            ++done;
        }
    }
}

// 双防面板和（K233 844 口径：numericalBase 权威基线，等级增减改视图不改面板）。
int def_sum(const ElfPet& pet) {
    return pet.numericalBase[NumericalPropertyIndex::DEFENSE]
         + pet.numericalBase[NumericalPropertyIndex::SPECIAL_DEFENSE];
}

// 双防都处于提升态（K357 1505 判据：ability_levels[2]>0 && [3]>0）。
bool in_dualdef_boost(const BattleContext* ctx, int side) {
    return ctx->ability_levels[side][2] > 0 && ctx->ability_levels[side][3] > 0;
}

// ══ 848 塞纳留斯：登场体力>1/2 → 对手全技 PP-db[1] ══════════════════════════
EffectResult k848_enter_pp_cut(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->pp_reduce) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 848)) {
        return EffectResult::kOk;
    }
    const ElfPet& self = ctx->getPet(owner);
    const int max_hp = resolve_pet_max_hp(self);
    const HostRow row = host_row(ctx, a, 848);
    const int denom = db_at(row, 0, k848DenomFallback);   // db[0] = 2（1/2）
    const int cut = db_at(row, 1, k848CutFallback);       // db[1] = 2
    if (denom <= 0 || cut <= 0 || max_hp <= 0 || self.hp <= 0) {
        return EffectResult::kOk;
    }
    // "大于1/2"：hp > max/denom ⟺ hp×denom > max（先乘后除防截断，K370 口径）。
    if (self.hp * denom <= max_hp) {
        return EffectResult::kOk;   // 恰等于 1/2 → 不触发（严格大于）
    }
    (void)g_core->pp_reduce(ctx, 1 - owner, cut);
    return EffectResult::kOk;
}

// ══ 759① 雷暴·利斯塔：回合开始 roll db[0]% → 审计槽 + 先制门旗 ═════════════
EffectResult k759_round_start_roll(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 759)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, 759);
    const int pct = db_at(row, 0, k759PctFallback);       // db[0] = 50
    if (pct <= 0) {
        ctx->plugin_storage[kAudit759Base + owner] = 0;
        ctx->plugin_storage[kFlag759Base + owner] = 0;
        return EffectResult::kOk;
    }
    const bool hit = roll_percent(pct);   // 本地掷（K316 394 口径）
    ctx->plugin_storage[kAudit759Base + owner] = hit ? 1 : 0;
    ctx->plugin_storage[kFlag759Base + owner] = hit ? 1 : 0;
    return EffectResult::kOk;
}

// ══ 759①' FMR：门旗在 → preemptive_level += db[1]（memset 后写，K378 形）════
EffectResult k759_fmr_preempt(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 759)) {
        return EffectResult::kOk;
    }
    const auto it = ctx->plugin_storage.find(kFlag759Base + owner);
    if (it == ctx->plugin_storage.end() || any_int_or(it->second, 0) != 1) {
        return EffectResult::kOk;   // 本回合未掷中 → 门闭
    }
    const HostRow row = host_row(ctx, a, 759);
    const int preempt = db_at(row, 1, k759PreemptFallback);   // db[1] = 2
    if (preempt > 0) {
        ctx->ws.preemptive_level[owner] += preempt;
    }
    ctx->plugin_storage.erase(it);   // 当回合一次性（K378 FMR 同款）
    return EffectResult::kOk;
}

// ══ 759② 先出手 → 对手随机 db[2] 技 PP 归零（K316 后手门对称读法）══════════
EffectResult k759_round_end_pp_zero(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 759)) {
        return EffectResult::kOk;
    }
    if (ctx->is_second_mover(owner)) {
        return EffectResult::kOk;   // 后手 → 门闭
    }
    const HostRow row = host_row(ctx, a, 759);
    const int count = db_at(row, 2, k759CountFallback);   // db[2] = 1
    pp_zero_random_slots(ctx, 1 - owner, owner, count);
    return EffectResult::kOk;
}

// ══ 918① 玄武：非双防强化态 → 免异常票（K171 285 直系，condition 现场判）════
EffectResult k918_grant_immune(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 918)) {
        return EffectResult::kOk;
    }
    int slot = a.host_slot;
    if (slot < 0 || slot > 5) {
        slot = ctx->find_pet_with_soulmark(owner, 918);
    }
    if (slot < 0 || slot > 5) {
        return EffectResult::kOk;
    }
    // "自身不处于双防强化状态时"：condition 逐次施加时点现场判（K171 满体力门同款
    // ——不在挂票时钉死，双防提升/回落即时反映）。换下场即失去前提。
    auto not_dual_boost = [owner, slot](const BattleContext* c, int) {
        if (!c || c->on_stage[owner] != slot) {
            return false;
        }
        return !in_dualdef_boost(c, owner);
    };
    ctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(),
                        /*anomaly_mask=*/0, /*duration_rounds=*/1,
                        /*source_id=*/918100, /*soul_immunity=*/true,
                        EffectScope::ON_STAGE, /*counts=*/0,
                        /*source_effect_id=*/918, ImmunityTier::Modern,
                        not_dual_boost);
    return EffectResult::kOk;
}

// ══ 918② 先出手 → 对手随机 db[0] 技 PP 归零（759② 同构）═════════════════════
EffectResult k918_round_end_pp_zero(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 918)) {
        return EffectResult::kOk;
    }
    if (ctx->is_second_mover(owner)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, 918);
    const int count = db_at(row, 0, k918CountFallback);   // db[0] = 1
    pp_zero_random_slots(ctx, 1 - owner, owner, count);
    return EffectResult::kOk;
}

// ══ 918③ 后出手 → 双防和×db[1]% FIXED + 等量回（BOSS无效）═══════════════════
EffectResult k918_round_end_fixed_heal(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->deal_pink_damage || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 918)) {
        return EffectResult::kOk;
    }
    if (!ctx->is_second_mover(owner)) {
        return EffectResult::kOk;   // 先手 → 门闭
    }
    const HostRow row = host_row(ctx, a, 918);
    const int pct = db_at(row, 1, k918PctFallback);   // db[1] = 25
    if (pct <= 0) {
        return EffectResult::kOk;
    }
    const ElfPet& self = ctx->getPet(owner);
    const ElfPet& foe = ctx->getPet(1 - owner);
    if (self.hp <= 0 || foe.hp <= 0) {
        return EffectResult::kOk;
    }
    const int amount = def_sum(self) * pct / 100;
    if (amount <= 0) {
        return EffectResult::kOk;
    }
    (void)g_core->deal_pink_damage(ctx, 1 - owner, amount, DamageKind::FIXED,
                                   owner);
    (void)g_core->heal_amount(ctx, owner, amount);
    return EffectResult::kOk;
}

// ══ 1430① 丝萝：回合开始满体 → 全技-db[0] / 未满 → 全技-db[1]（变档）════════
EffectResult k1430_round_start_pp(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->pp_reduce) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 1430)) {
        return EffectResult::kOk;
    }
    const ElfPet& self = ctx->getPet(owner);
    const HostRow row = host_row(ctx, a, 1430);
    const int cut_full = db_at(row, 0, k1430FullFallback);    // db[0] = 2
    const int cut_part = db_at(row, 1, k1430NotFallback);     // db[1] = 1
    const int max_hp = resolve_pet_max_hp(self);
    if (max_hp <= 0 || self.hp <= 0) {
        return EffectResult::kOk;
    }
    const int cut = (self.hp >= max_hp) ? cut_full : cut_part;
    if (cut <= 0) {
        return EffectResult::kOk;
    }
    (void)g_core->pp_reduce(ctx, 1 - owner, cut);
    return EffectResult::kOk;
}

// ══ 1430② ROUND_END：随机恢复 [db[2],db[3]]；<db[4] 睡眠；>db[6] 写下回合戳 ══
EffectResult k1430_round_end_heal(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 1430)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, 1430);
    const int lo = db_at(row, 2, k1430LoFallback);        // db[2] = 100
    const int hi = db_at(row, 3, k1430HiFallback);        // db[3] = 400
    const int sleep_gate = db_at(row, 4, k1430SleepGateFallback);  // db[4] = 200
    const int stamp_gate = db_at(row, 6, k1430StampGateFallback);  // db[6] = 300
    if (lo <= 0 || hi < lo) {
        return EffectResult::kOk;
    }
    const ElfPet& self = ctx->getPet(owner);
    if (self.hp <= 0) {
        return EffectResult::kOk;
    }
    const int amount = lo + sbs_rand() % (hi - lo + 1);   // [lo, hi] 闭区间
    ctx->plugin_storage[kAudit1430Base + owner] = amount;   // 审计槽（F9 782）
    (void)g_core->heal_amount(ctx, owner, amount);
    const int opp = 1 - owner;
    if (amount < sleep_gate) {
        // "低于200则100%令对手睡眠"——申报通道 100 档内核短路不耗 rand（K176）。
        if (g_core->apply_anomaly_checked) {
            (void)g_core->apply_anomaly_checked(
                ctx, opp, static_cast<int>(AbnormalStatusId::Sleep),
                random_anomaly_duration(), owner, k1430SleepPct,
                ChanceSource::SoulMark);
        }
    }
    if (amount > stamp_gate && !ctx->is_boss_challenge) {
        // "高于300则下回合自身攻击技能命中后附加恢复量100%的百分比伤害"——尾注挂
        // 第三分句 → boss 局 heal/睡眠照常、仅戳不写（子句级归属记档）。
        ctx->plugin_storage[kStamp1430Base + owner] = amount;
        ctx->plugin_storage[kUntil1430Base + owner] = ctx->roundCount + 1;
    }
    return EffectResult::kOk;
}

// ══ 1430③ arm@enter：DETECT 票——下回合 own 攻击命中 → 附加 stamp×db[5]% 粉伤 ══
EffectResult k1430_arm_attach(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int key = kWire1430Base + owner;
    if (ctx->plugin_storage.count(key)) {
        return EffectResult::kOk;   // 双触发防重（K373 同款）
    }
    ctx->plugin_storage.emplace(key, 1);
    ctx->register_default_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner) {
                return;
            }
            DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || !d.isRed || d.final <= 0) {
                return;   // 自身攻击技命中（红伤落地）
            }
            if (!onstage_has_soul_mark(c, owner, 1430) || c->is_boss_challenge) {
                return;   // boss 双保险（K391 管线票口径）
            }
            const int stamp = flag_of(c, kStamp1430Base + owner);
            const int until = flag_of(c, kUntil1430Base + owner);
            if (stamp <= 0 || until <= 0 || c->roundCount != until) {
                return;   // ==until 精确窗（K391 形）；过期戳自然失活
            }
            // db[5]=附加比%（消费时点现读，args 驱动）。
            constexpr int kAttachPctIdx = 5;
            const int slot = c->find_pet_with_soulmark(owner, 1430);
            if (slot < 0 || slot > 5) {
                return;
            }
            const std::vector<int>& db =
                c->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
            const int pct = (kAttachPctIdx < static_cast<int>(db.size()))
                                ? db[kAttachPctIdx] : k1430AttachPctFallback;
            if (pct <= 0) {
                return;
            }
            c->plugin_storage.erase(kStamp1430Base + owner);   // 消费清戳
            c->plugin_storage.erase(kUntil1430Base + owner);
            const int dmg = stamp * pct / 100;
            if (dmg > 0 && g_core && g_core->deal_pink_damage) {
                (void)g_core->deal_pink_damage(c, 1 - owner, dmg,
                                               DamageKind::PERCENT_VALUE, owner);
            }
        });
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +4（848/759/918/1430 入账，
//    只增不改）────────────────────────────────────────────────────────────────
SOUL_PROGRAM(848, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k848_enter_pp_cut,
                             /*once=*/false, /*early=*/false, SoulScope::STAGE,
                             /*boss_invalid=*/false)});

SOUL_PROGRAM(759, {SOUL_NODE(State::BATTLE_ROUND_START, &k759_round_start_roll,
                             /*once=*/false, /*early=*/false, SoulScope::STAGE,
                             /*boss_invalid=*/false),
                   SOUL_NODE(State::BATTLE_FIRST_MOVE_RIGHT, &k759_fmr_preempt,
                             /*once=*/false, /*early=*/false, SoulScope::STAGE,
                             /*boss_invalid=*/false),
                   SOUL_NODE(State::BATTLE_ROUND_END, &k759_round_end_pp_zero,
                             /*once=*/false, /*early=*/false, SoulScope::STAGE,
                             /*boss_invalid=*/false)});

SOUL_PROGRAM(918, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k918_grant_immune,
                             /*once=*/false, /*early=*/false, SoulScope::STAGE,
                             /*boss_invalid=*/false),
                   SOUL_NODE(State::BATTLE_ROUND_START, &k918_grant_immune,
                             /*once=*/false, /*early=*/false, SoulScope::STAGE,
                             /*boss_invalid=*/false),
                   SOUL_NODE(State::BATTLE_ROUND_END, &k918_round_end_pp_zero,
                             /*once=*/false, /*early=*/false, SoulScope::STAGE,
                             /*boss_invalid=*/false),
                   SOUL_NODE(State::BATTLE_ROUND_END, &k918_round_end_fixed_heal,
                             /*once=*/false, /*early=*/false, SoulScope::STAGE,
                             /*boss_invalid=*/true)});

SOUL_PROGRAM(1430, {SOUL_NODE(State::BATTLE_ROUND_START, &k1430_round_start_pp,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE,
                              /*boss_invalid=*/false),
                    SOUL_NODE(State::BATTLE_ROUND_END, &k1430_round_end_heal,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE,
                              /*boss_invalid=*/false),
                    SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k1430_arm_attach,
                              /*once=*/false, /*early=*/true, SoulScope::STAGE,
                              /*boss_invalid=*/true)});
