// K226 · 攻击瘫痪概率+低血恢复族 —— effect 869（1 宏 2 行）
//                                                                          （2026-10-08，day 专项 worktree ../agents/k226）
//
// 行清单（tools/soul_effects_dump.py 869，2026-10-08）：
//   869 row583/584（圣辉·达赫迪 3255，本体/进化同文双行）
//     args[30,2,3,1]/[35,2,3,1]「攻击时有30%的概率使对手瘫痪，自身体力不满时额外附加
//     自身已损失体力百分比的概率；战斗阶段结束时若自身体力低于1/2且自身处于能力提升
//     状态，则恢复自身最大体力的1/3（BOSS无效）」
//   → db[0]=基础概率%（30/35，行间互锚唯一变量）；db[1]=血门分母（低于 1/2）；
//     db[2]=恢复分母（1/3）；db[3]=1 不消费（K2 纪律：解析不了的字段宁可不用，不猜）。
//   ⚠ effect_info 869 模板（"附加自身速度%的百分比伤害"）为 moves 空间错位文——soul 侧
//     以 tips 为权威（SOP §六.1，K194 口径）。
//
// 实现口径：
//   - 攻击窗 = EVENT_SKILL_USED（own + !used_attribute，K2 358/662 同窗同门；miss 也算
//     "攻击"——E13 使用即触发语义）；
//   - 瘫痪 = AbnormalStatusId::Crippled(19)（k_reflect_tail_family 锚点；同族前作
//     662"攻击时20%瘫痪" K2 已建模限制类，非 0-9 段跳过口径不适用）；
//   - 概率 = db[0] +（体力不满时）已损体力百分比（(max-hp)*100/max）——662"已损体力
//     爬坡"同族；申报通道 apply_anomaly_checked（内核单掷、禁本地预掷，2026-10-07
//     kprod1 勘误 §二.1）；F9 审计槽 990869000+owner 记本次申报概率（场景无随机梯度
//     断言，782 首创口径）；
//   - 回合末 = BATTLE_ROUND_END（= 官方"战斗阶段结束时"，chaos_sorensen/k178/k186/
//     puni_1038 五处同锚）：hp*db[1] < max && 自身任一等级槽 > 0（k172 判据）→
//     heal_amount(max/db[2])；
//   - （BOSS无效）：按 K190 口径 boss 旗忽略不置位（记档）。
//
// 动工前置三查（2026-10-08）：①内核 grep 869 无硬编码（0 命中）；②effect_icon
//   kind=[5,10]、effect_info 模板系 moves 空间错位文（以 tips 为权威）；③registry
//   soul=172 无 869、moves 侧 821 命中系 skills.cpp 通用模板另一空间（eid 撞号甄别
//   SOP §六.2），soul 侧无注册。
// 先例：K2 662（EVENT_SKILL_USED 瘫痪概率 + 已损爬坡 + 审计槽）、K10 637（ROUND heal）、
//   K172（能力提升判据）、k169（BATTLE_ROUND_COMPLETION 节点形）/chaos_sorensen
//   （ROUND_END=战斗阶段结束锚）。
// 场景回归：test/scenario/scenario_3700_para_lowheal_869.cpp（号段 3700-3709）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMark = 869;
constexpr int kAuditChanceKeyBase = 990869000;   // +owner：本次申报概率（审计槽，只读）
constexpr int kWiredKeyBase = 990869100;         // +owner：watcher 已挂旗标（防 early 双触发）

// ── 宿主行参数（"行≠宏"：全量 DB args + monster_id，K2/K164 同款私副本）──────

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

// 攻击技事件判据：自家魂印在场 + 本方使用了攻击类技能（662 同门）。
bool own_attack_event(BattleContext* ctx, int owner, const BattleEvent& ev) {
    if (ev.actor != owner || ev.used_attribute) {
        return false;
    }
    return onstage_has_soul_mark(ctx, owner, kMark);
}

// 自身处于能力提升状态 = 任一等级槽 > 0（k172 对手判据同构）。
bool has_any_stat_boost(BattleContext* ctx, int side) {
    for (int i = 0; i < 6; ++i) {
        if (ctx->ability_levels[side][i] > 0) {
            return true;
        }
    }
    return false;
}

// ── 攻击窗：db[0]% + 体力不满时附加已损体力百分比 → 瘫痪(19) ─────────────────

EffectResult soulmark_869_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, kMark);
    const int base_pct = db_at(row, 0, -1);
    if (base_pct < 0) {
        return EffectResult::kOk;   // 无概率行不武装
    }
    if (watcher_wired(ctx, owner)) {
        return EffectResult::kOk;
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, base_pct](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || !own_attack_event(wctx, owner, ev)) {
                return;
            }
            const ElfPet& self = wctx->getPet(owner);
            const int max_hp = resolve_pet_max_hp(self);
            int pct = base_pct;
            if (max_hp > 0 && self.hp < max_hp) {
                // "自身体力不满时额外附加自身已损失体力百分比的概率"（662 已损爬坡同族）
                pct += (max_hp - self.hp) * 100 / max_hp;
            }
            wctx->plugin_storage[kAuditChanceKeyBase + owner] = pct;   // F9 审计槽
            if (g_core && g_core->apply_anomaly_checked) {
                (void)g_core->apply_anomaly_checked(
                    wctx, 1 - owner, static_cast<int>(AbnormalStatusId::Crippled),
                    random_anomaly_duration(), owner, pct, ChanceSource::SoulMark);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 回合末（BATTLE_ROUND_END = 战斗阶段结束时）：低于 1/db[1] 且有提升 → 回 1/db[2] ──

EffectResult soulmark_869_round_end(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMark)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, kMark);
    const int gate_denom = db_at(row, 1, 2);
    const int heal_denom = db_at(row, 2, 3);
    if (gate_denom <= 0 || heal_denom <= 0) {
        return EffectResult::kOk;
    }
    const ElfPet& self = ctx->getPet(owner);
    const int max_hp = resolve_pet_max_hp(self);
    if (self.hp <= 0 || max_hp <= 0) {
        return EffectResult::kOk;   // 阵亡不恢复
    }
    if (self.hp * gate_denom >= max_hp) {
        return EffectResult::kOk;   // 未低于 1/gate（严格小于；恰在门限不触发）
    }
    if (!has_any_stat_boost(ctx, owner)) {
        return EffectResult::kOk;   // 门：自身处于能力提升状态
    }
    if (g_core && g_core->heal_amount) {
        g_core->heal_amount(ctx, owner, max_hp / heal_denom);
    }
    return EffectResult::kOk;
}

}  // namespace

// 注册（+1 宏：869）。arm=early 挂 EVENT_SKILL_USED watcher；回合末恢复走声明节点
// （更新器每回合重注册，SOP §一.1）。
SOUL_PROGRAM(869, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_869_arm,
                    /*once=*/false, /*early=*/true),
                   SoulMarkNodeRef(State::BATTLE_ROUND_END, &soulmark_869_round_end,
                    /*once=*/false, /*early=*/false)});
