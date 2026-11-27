// K230 · 异常护盾/PP归零+受击门恢复族 —— effect 1562（1 宏 2 行）
//                                                            （2026-10-08，夜跑 k-prod-6，worktree ../agents/k230）
//
// 行清单（tools/soul_effects_dump.py 1562，2026-10-08）：
//   1562 row1227（极岳魔兽 3962）args[3,1,3,3,1]
//     「回合开始时，若自身处于异常状态则为自身附加等同于自身最大体力1/3的护盾，若自身
//     不处于异常状态则回合结束使对手随机1个技能的PP值归零；自身受到的攻击伤害低于自身
//     最大体力的1/3则战斗阶段结束时恢复自身最大体力的1/3（boss无效）」
//   1562 row1272（同载体 3962，魔界凶兽伙伴效果）args[3,1,2,3,1]
//     同文；db[2]=受击门分母 3/2（行间互锚唯一变量）。
//   → db[0]=护盾分母（max/db[0]）；db[1]=PP归零个数；db[2]=受击门分母
//     （受到攻击伤害 < max/db[2]）；db[3]=恢复分母（max/db[3]）；
//     db[4] 不消费（K2 纪律：解析不了宁可不用，不猜）。tips 为权威文（SOP §六.1）。
//
// 实现口径：
//   - 分支 = **回合开始时快照**（官方文案锚"回合开始时"，K229 同款账本）：ROUND_START
//     查 has_any_anomaly（k227/crawford 全池扫，"异常状态"不过滤子类）——有异常 →
//     立即 shield_bank_.add(kSoulShieldPriority, max/db[0])；无异常 → 账本记 PP 归零
//     分支，ROUND_END 执行 pp_zero_slot（core_api"随机{0}项技能 PP 归零"族专用原语，
//     比 LB 直写 skills[].pp 多发 EVENT_PP_REDUCED——k227 直写系旧例不效仿）；
//   - 受击累计 = EVENT_TAKE_DAMAGE watcher（canglan 2343"当回合受到了攻击伤害"同窗）
//     只累计**红伤**（ev.damage_kind==NORMAL——粉/真/属性直伤不发攻击伤害语义），
//     amount=实扣 HP（护盾后真值，管线 emit 口径）；ROUND_START 清零、ROUND_END 读后清，
//     双保险防跨回合残留；
//   - 恢复 = ROUND_END（=官方"战斗阶段结束时"，K226 五处同锚）：累计 < max/db[2] →
//     heal_amount(max/db[3])（自身体力>0 守卫；门值进审计槽供场景断言 db[2] 消费）；
//   - （boss无效）：K190 口径 boss 旗忽略不置位（记档）。
//
// 动工前置三查（2026-10-08）：①内核 grep 1562 无硬编码（0 命中）；②effect_icon
//   kind=[2,5,15]、tips 为权威文（dump 原文已核）；③soul_lib 无 1562 注册、
//   sibling 分支 feat/k226-k229 亦无（K167 撞车教训，全分支 grep）。
// 先例：k227 2011（has_any_anomaly 全池扫+异常门+ROUND_END PP 形）、canglan 2343
//   （EVENT_TAKE_DAMAGE 红旗 watcher）、K1/K229（账本快照+审计槽）、K10 637
//   （ROUND heal）、e_block/j_prob（shield_bank_.add 挂盾形）。
// 场景回归：test/scenario/scenario_3740_anomaly_shield_pp_1562.cpp（号段 3740-3749）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMark = 1562;
constexpr int kWiredKeyBase = 991423000;   // +owner：watcher 已挂旗标（防 early 双触发）
constexpr int kBranchKeyBase = 991424000;  // +owner：回合分支账本（1=PP归零，0/缺=无）
constexpr int kDmgSumBase = 991425000;     // +owner：本回合受击红伤累计（审计槽，只读）
constexpr int kGateBase = 991426000;       // +owner：本回合受击门值 max/db[2]（审计槽，只读）

// ── 宿主行参数（"行≠宏"：全量 DB args + monster_id，K226/K227 同款私副本）──────

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

// "处于异常状态" = 任一官方异常槽活跃（k227/crawford 全池扫同款，不过滤子类）。
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

// ── early：挂一次受击累计 watcher（红伤时点过滤）────────────────────────────

EffectResult soulmark_1562_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || watcher_wired(ctx, owner)) {
        return EffectResult::kOk;
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount,
        /*duration_rounds=*/0, /*once=*/false,
        [owner](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || ev.target != owner || ev.amount <= 0) {
                return;
            }
            if (ev.damage_kind != static_cast<int>(DamageKind::NORMAL)) {
                return;   // "受到的攻击伤害"=红伤实扣（粉/真/属性直伤不计）
            }
            if (!onstage_has_soul_mark(wctx, owner, kMark)) {
                return;
            }
            auto& sum = wctx->plugin_storage[kDmgSumBase + owner];
            sum = any_int_or(sum, 0) + ev.amount;
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── ROUND_START：清累计 + 异常快照分支（有异常→挂盾；无异常→PP归零账本）──────

EffectResult soulmark_1562_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMark)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage[kDmgSumBase + owner] = 0;   // 每回合清（读后清双保险的另一半）
    const HostRow row = host_row(ctx, args, kMark);
    const int shield_denom = db_at(row, 0, -1);
    const int pp_count = db_at(row, 1, -1);
    if (shield_denom <= 0 || pp_count <= 0) {
        return EffectResult::kOk;   // 无参行不武装（记档）
    }
    if (has_any_anomaly(ctx, owner)) {
        const ElfPet& self = ctx->getPet(owner);
        const int max_hp = resolve_pet_max_hp(self);
        if (max_hp > 0 && self.hp > 0) {
            ctx->getPet(owner).shield_bank_.add(ShieldBank::kSoulShieldPriority,
                                                max_hp / shield_denom, kMark, /*duration=*/0);
        }
        ctx->plugin_storage[kBranchKeyBase + owner] = 0;
    } else {
        ctx->plugin_storage[kBranchKeyBase + owner] = 1;
    }
    return EffectResult::kOk;
}

// ── ROUND_END：分支执行 PP 归零 + 受击门恢复 ────────────────────────────────

EffectResult soulmark_1562_round_end(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMark)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, kMark);
    const int gate_denom = db_at(row, 2, -1);
    const int heal_denom = db_at(row, 3, -1);
    if (gate_denom <= 0 || heal_denom <= 0) {
        return EffectResult::kOk;
    }
    const ElfPet& self = ctx->getPet(owner);
    const int max_hp = resolve_pet_max_hp(self);
    if (max_hp <= 0) {
        return EffectResult::kOk;
    }
    // ① 分支账本：无异常快照 → 对手随机 db[1] 个技能 PP 归零（读后清）
    int branch = 0;
    const auto bit = ctx->plugin_storage.find(kBranchKeyBase + owner);
    if (bit != ctx->plugin_storage.end()) {
        branch = any_int_or(bit->second, 0);
        ctx->plugin_storage.erase(bit);
    }
    if (branch == 1 && g_core && g_core->pp_zero_slot) {
        const int opp = 1 - owner;
        const int pp_count = db_at(row, 1, 1);
        int done = 0;
        int guard = 0;
        while (done < pp_count && guard < 100) {
            ++guard;
            const int slot = ctx->rng().next() % 5;   // 5 技能槽随机（无限 PP 槽由原语拒并跳过计数外重试）
            if (g_core->pp_zero_slot(ctx, opp, slot, owner) ==
                PpReduceResult::SUCCESS) {
                ++done;
            }
        }
    }
    // ② 受击门：本回合受到攻击伤害 < max/db[2] → 回 max/db[3]（门值/累计进审计槽，
    //    保留供场景事后读——防残留由 ROUND_START 清零承担）
    int dmg_sum = 0;
    const auto dit = ctx->plugin_storage.find(kDmgSumBase + owner);
    if (dit != ctx->plugin_storage.end()) {
        dmg_sum = any_int_or(dit->second, 0);
    }
    const int gate_value = max_hp / gate_denom;
    ctx->plugin_storage[kGateBase + owner] = gate_value;
    if (self.hp > 0 && dmg_sum < gate_value && g_core && g_core->heal_amount) {
        (void)g_core->heal_amount(ctx, owner, max_hp / heal_denom);
    }
    return EffectResult::kOk;
}

}  // namespace

// 注册（+1 宏：1562）。early=受击累计 watcher 一次挂；ROUND_START=清累计+异常快照
// 分支（挂盾/PP 账本）；ROUND_END=PP 归零执行+受击门恢复（更新器每回合重注册，SOP §一.1）。
SOUL_PROGRAM(1562, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_1562_arm,
                    /*once=*/false, /*early=*/true),
                   SoulMarkNodeRef(State::BATTLE_ROUND_START, &soulmark_1562_round_start,
                    /*once=*/false, /*early=*/false),
                   SoulMarkNodeRef(State::BATTLE_ROUND_END, &soulmark_1562_round_end,
                    /*once=*/false, /*early=*/false)});
