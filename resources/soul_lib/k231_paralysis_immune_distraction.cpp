// K231 · 登场免麻痹+失神+属技无效/反击窗族 —— effect 1396（1 宏 2 行）
//                                                            （2026-10-08，夜跑 k-prod-6，worktree ../agents/k231）
//
// 行清单（tools/soul_effects_dump.py 1396，2026-10-08）：
//   1396 row1088（哀伤公爵·阿洛希尔 3813）args[50,1,50,50,20,1]
//     「阿洛希尔登场之后免疫受到的麻痹状态且受到攻击后50%使对手失神；|自身攻击技能
//     命中后对手下1次属性技能无效，自身属性技能命中后附加自身上次受到的攻击伤害50%
//     的百分比伤害且50%令对手害怕，未触发害怕则下次概率提升20%（boss无效）」
//   1396 row1130（同载体 3813）args[80,1,60,60,20,1]
//     同文；db[0]=失神 80、db[2]=反击比 60、db[3]=害怕 60（行间互锚唯一变量）。
//   → db[0]=受击失神%；db[1]="下1次属技无效"次数（=1，消费为 seal count）；
//     db[2]=反击粉伤比%；db[3]=害怕基础%；db[4]=害怕爬坡步；
//     db[5]=1 不消费（K2 纪律：解析不了宁可不用，不猜）。tips 为权威文（SOP §六.1）。
//
// 实现口径：
//   ① 登场免麻痹 = ENTER_EXIT_STAGE early → grant_immunity(ANOMALY, anomaly_mask=
//     1<<麻痹, duration 0, ON_STAGE)——离场自动失效、重登场重授（encarlos 免降/
//     K190 免弹控窗同形；mask 细分只免麻痹，其余异常照常可施加）；
//   ② 受击失神 + ④反击记忆 = EVENT_TAKE_DAMAGE watcher arm-once（K230 同窗：
//     红伤 NORMAL、target=own、amount>0）→ 记忆槽=amount（"上次受到的攻击伤害"，
//     跨回合保留）+ apply_anomaly_checked 失神 db[0]%（申报通道内核单掷）；
//   ③④ 命中节点 = BATTLE_FIRST/SECOND_ON_SKILL_HIT 声明进 SOUL_PROGRAM（K6 记档：
//     每回合节点必须声明；skill_exec_result/skill_type_view 在该时点已物化——
//     wuxiang/lingchao 记档）——exec==HIT 后按 skill_type_view 分流：
//     - 攻击技 → seal_skill(attribute=true, attack=false, count=db[1], duration=0)
//       即时插票（encarlos 封属形；"下1次"=每击一层计票，层叠读法记档）；
//     - 属性技 → deal_pink_damage(记忆×db[2]%, PERCENT_VALUE)（K225 双段吸取形）+
//       害怕爬坡：基础 db[3]，未落地 +db[4]（上限 100），落地重置基础——概率申报制
//       （审计槽记录本次申报值，F9 口径；落地集=SUCCESS/EXTENDED/REPLACED/CONVERTED）。
//   （boss无效）：K190 口径 boss 旗忽略不置位（记档）。
//
// 动工前置三查（2026-10-08）：①内核 grep 1396 无魂印行为硬编码（moves_lib/
//   j_seal_attack_window 的 1396=[4,3,2,2] 是 moves 空间另一切片、boss_invalid_effects
//   名录而已——eid 撞号甄别 SOP §六.2）；②effect_icon kind=[10,13,14]、tips 为权威文；
//   ③soul_lib 无 1396 注册、sibling 分支 feat/k226-k230 亦无（全分支 grep）。
// 先例：encarlos 1164（seal_skill 封属 count=1 即时插票 + 登场免疫票）、K190 870
//   （grant_immunity ANOMALY 形）、K230（TAKE_DAMAGE 红伤 watcher 形）、miaoshi 4872
//   （Distraction 落地集）、k_anomaly_chance 662（概率申报+审计槽爬坡）、wuxiang 2260
//   （ON_SKILL_HIT 时点 exec_result 读）。
// 场景回归：test/scenario/scenario_3750_para_immune_distraction_1396.cpp（号段 3750-3759）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>
#include <entities/skills.h>

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMark = 1396;
constexpr int kWiredKeyBase = 991427000;      // +owner：watcher 已挂旗标（防 early 双触发）
constexpr int kLastRedBase = 991428000;       // +owner：上次受到的攻击伤害（记忆兼审计）
constexpr int kFearChanceBase = 991429000;    // +owner：害怕当前概率（爬坡账本）
constexpr int kFearAuditBase = 991430000;     // +owner：本次害怕申报%（审计槽，只读）
constexpr int kDistractAuditBase = 991431000; // +owner：本次失神申报%（审计槽，只读）

bool anomaly_landed(ApplyAnomalyResult r) {
    return r == ApplyAnomalyResult::SUCCESS
        || r == ApplyAnomalyResult::DURATION_EXTENDED
        || r == ApplyAnomalyResult::REPLACED_EXISTING
        || r == ApplyAnomalyResult::CONVERTED;
}

// ── 宿主行参数（"行≠宏"：全量 DB args + monster_id，K226 同款私副本）──────────

struct HostRow {
    const std::vector<int>* db = nullptr;
    int monster_id = 0;
    bool valid = false;
};

HostRow host_row(BattleContext* ctx, const EffectArgs& args, int mark_id) {
    HostRow row;
    if (!ctx) {
        return row;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return row;
    }
    int slot = resolve_subject_slot_from_args(args);
    if (slot < 0) {
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

bool watcher_wired(BattleContext* ctx, int owner) {
    const int key = kWiredKeyBase + owner;
    if (ctx->plugin_storage.count(key)) {
        return true;
    }
    ctx->plugin_storage.emplace(key, 1);
    return false;
}

// ── ①+②early：登场免麻痹票（每次登场重授）+ 受击失神 watcher（arm-once）──────

EffectResult soulmark_1396_enter(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMark)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, kMark);
    const int distract_pct = db_at(row, 0, -1);
    if (distract_pct < 0) {
        return EffectResult::kOk;   // 无参行不武装（记档）
    }
    // ① 免疫麻痹（mask 细分；duration 0 = 常驻、ON_STAGE = 离场失效）
    ctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(),
                        /*anomaly_mask=*/1u << static_cast<int>(AbnormalStatusId::Paralysis),
                        /*duration_rounds=*/0, /*source_id=*/kMark,
                        /*soul_immunity=*/false, EffectScope::ON_STAGE, /*counts=*/0,
                        /*source_effect_id=*/kMark, ImmunityTier::Modern);
    // ② 受击失神 + 反击记忆（arm-once）
    if (!watcher_wired(ctx, owner)) {
        ctx->event_center_.register_watcher(
            EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount,
            /*duration_rounds=*/0, /*once=*/false,
            [owner, distract_pct](BattleContext* wctx, const BattleEvent& ev) {
                if (!wctx || ev.target != owner || ev.amount <= 0) {
                    return;
                }
                if (ev.damage_kind != static_cast<int>(DamageKind::NORMAL)) {
                    return;   // "受到攻击"=红伤（K230 同门）
                }
                if (!onstage_has_soul_mark(wctx, owner, kMark)) {
                    return;
                }
                wctx->plugin_storage[kLastRedBase + owner] = ev.amount;   // 反击记忆
                if (g_core && g_core->apply_anomaly_checked) {
                    wctx->plugin_storage[kDistractAuditBase + owner] = distract_pct;
                    (void)g_core->apply_anomaly_checked(
                        wctx, 1 - owner, static_cast<int>(AbnormalStatusId::Distraction),
                        random_anomaly_duration(), owner, distract_pct,
                        ChanceSource::SoulMark);
                }
            },
            WatcherScope::TEAM);
    }
    return EffectResult::kOk;
}

// ── ③④命中节点（FIRST/SECOND_ON_SKILL_HIT 共用；exec/类型视图已物化）────────

EffectResult soulmark_1396_on_hit(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMark)) {
        return EffectResult::kOk;
    }
    if (ctx->ws.skill_exec_result[owner] != SkillExecResult::HIT) {
        return EffectResult::kOk;   // "命中后"（wuxiang 同门：非 HIT 不触发）
    }
    const HostRow row = host_row(ctx, args, kMark);
    const int opp = 1 - owner;
    if (ctx->ws.skill_type_view[owner] != static_cast<int>(SkillType::Attribute)) {
        // ③ 攻击技命中 → 对手下 db[1] 次属性技能无效（count 票即时插）
        if (g_core && g_core->seal_skill) {
            (void)g_core->seal_skill(ctx, /*source=*/owner, /*target=*/opp,
                                     /*effect_id=*/kMark,
                                     /*attribute=*/true, /*attack=*/false,
                                     /*count=*/db_at(row, 1, 1),
                                     /*duration_rounds=*/0, /*penetrable=*/true,
                                     /*source_slot=*/-1, EffectScope::ON_STAGE,
                                     /*hit_invalid=*/false, /*chance_pct=*/100,
                                     /*consumed_when_pierced=*/false);
        }
        return EffectResult::kOk;
    }
    // ④ 属性技命中 → 附加"上次受到的攻击伤害"db[2]% 粉伤（记忆≤0 无从附加，跳过）
    int last_red = 0;
    const auto lit = ctx->plugin_storage.find(kLastRedBase + owner);
    if (lit != ctx->plugin_storage.end()) {
        last_red = any_int_or(lit->second, 0);
    }
    if (last_red > 0 && g_core && g_core->deal_pink_damage) {
        (void)g_core->deal_pink_damage(ctx, opp, last_red * db_at(row, 2, 50) / 100,
                                       DamageKind::PERCENT_VALUE, owner);
    }
    // ④ 害怕爬坡：基础 db[3]，未落地 +db[4]（≤100），落地重置基础
    const int base_pct = db_at(row, 3, 50);
    const int step = db_at(row, 4, 20);
    int chance = base_pct;
    const auto cit = ctx->plugin_storage.find(kFearChanceBase + owner);
    if (cit != ctx->plugin_storage.end()) {
        chance = any_int_or(cit->second, base_pct);
    }
    ctx->plugin_storage[kFearAuditBase + owner] = chance;   // F9 审计槽
    bool landed = false;
    if (g_core && g_core->apply_anomaly_checked) {
        landed = anomaly_landed(g_core->apply_anomaly_checked(
            ctx, opp, static_cast<int>(AbnormalStatusId::Fear),
            random_anomaly_duration(), owner, chance, ChanceSource::SoulMark));
    }
    ctx->plugin_storage[kFearChanceBase + owner] =
        landed ? base_pct : std::min(100, chance + step);
    return EffectResult::kOk;
}

}  // namespace

// 注册（+1 宏：1396）。early=免麻痹票（每次登场）+失神 watcher（arm-once）；
// FIRST/SECOND_ON_SKILL_HIT=③④命中节点（更新器每回合重注册，SOP §一.1）。
SOUL_PROGRAM(1396, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_1396_enter,
                    /*once=*/false, /*early=*/true),
                   SoulMarkNodeRef(State::BATTLE_FIRST_ON_SKILL_HIT, &soulmark_1396_on_hit,
                    /*once=*/false, /*early=*/false),
                   SoulMarkNodeRef(State::BATTLE_SECOND_ON_SKILL_HIT, &soulmark_1396_on_hit,
                    /*once=*/false, /*early=*/false)});
