// K164 · 攻击附带概率族 —— 109 / 116 / 151 / 266（4 宏 / 9 行）
//                                                                          （2026-10-05，worktree ../agents/k164）
//
// 行清单（tools/soul_effects_dump.py 109 116 151 266，2026-10-05）：
//   109 row72（2593）args[20,20]「每次攻击20%概率附加20点固定伤害」/
//       row117（2703）args[15,100]「15%概率附加100点固伤」→ db[0]=pct%、db[1]=固伤点数
//   116 row251/252（2163）args[10,2]/[15,2]「攻击时有10%/15%的概率烧伤对手」
//       → db[0]=pct%、db[1]=异常码（2=烧伤）
//   151 row248/249（2132 双行）args[20,0]/[30,0]「攻击时有2%/3%的概率秒杀对手」
//       → ⚠️ db[0]=**千分比**（db/10：20→2%、30→3%，工单行互锚口径）；db[1] 不消费
//   266 row16（2462）args 空「5%的概率使敌方攻击落空」（args 空以行文为数，K5 的 7 同款：
//       monster_table {2462:5}）｜ row58/59（2563/2761）args[8,0]「8%概率使对手攻击落空」
//       → db[0]=pct、db[1] 不消费
//
// 实现口径（工单行处方）：
//   - 109/116/151 = EVENT_SKILL_USED（own + !used_attribute，K2 358 同窗同门；miss 也算
//     "攻击"——E13 使用即触发锚点语义）→ roll_percent(db) → fixed_damage /
//     apply_anomaly_checked(烧伤 2, Modern) / force_hp_to_zero（151 概率先 db/10 折算）。
//     151 加存活门（对手 hp≤0 不重复秒）。
//   - 266 = 命中压制乘区：ws.hit_rate_mod_attack[opp] = 1 - pct/100（K6 1314 同款通道；
//     ROUND_START soul 节点每回合重写——ws 每回合 reset）。⚠️「概率使落空」落地为命中
//     压制乘区系工单行处方口径（非逐次掷骰），记档。
//   - 概率单位多行互锚：109/116=百分比、151=千分比——严禁跨宏复用换算。
//
// 动工前置三查（2026-10-05，工单行已核）：①内核 grep 四 id 无硬编码 ②effect_icon kind
//   已核 ③registry soul=75 无四 id、未合并分支无实现。
// 先例：K2 358（EVENT_SKILL_USED 攻击技窗）、J23 36（force_hp_to_zero+存活门）、
//   K5 的 7（args 空以行文为数）、K6 1314（hit_rate_mod_attack 通道）。
// 场景回归：test/scenario/scenario_2890_attack_prob.cpp（2890 号段）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

// ── 宿主行参数（"行≠宏"：全量 DB args + monster_id，K2 同款私副本）──────────
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

bool watcher_wired(BattleContext* ctx, int macro, int owner) {
    const int key = 990164000 + macro * 10 + owner;
    if (ctx->plugin_storage.count(key)) {
        return true;
    }
    ctx->plugin_storage.emplace(key, 1);
    return false;
}

// 攻击技事件判据：自家魂印在场 + 本方使用了攻击类技能（358 同门）。
bool own_attack_event(BattleContext* ctx, int owner, const BattleEvent& ev, int mark_id) {
    if (ev.actor != owner || ev.used_attribute) {
        return false;
    }
    return onstage_has_soul_mark(ctx, owner, mark_id);
}

// 攻击附带窗 arm（109/116/151 共用）：EVENT_SKILL_USED own+攻击技 → roll → 动作。
// 动作以宏区分（模板函数编译期分派，零运行时开销）。
enum class AttackRider {
    FixedDamage,   // 109：{1}% → {2} 点固伤
    Burn,          // 116：{1}% → 烧伤（db[1] 异常码）
    Kill           // 151：db/10 % → 秒杀（存活门）
};

template <AttackRider RIDER>
void rider_dispatch(BattleContext* c, int mark_id, int owner, int opp, int pct_raw, int arg2) {
    if (RIDER == AttackRider::Kill) {
        // 151：千分比 → 百分比（20→2%、30→3%，工单互锚口径）
        const int pct = pct_raw / 10;
        if (!roll_soul_percent(*c, mark_id, 0, owner, opp, pct)) {
            return;
        }
        if (c->getPet(opp).hp <= 0) {
            return;   // 存活门（J23 36 同款）
        }
        if (g_core && g_core->force_hp_to_zero) {
            (void)g_core->force_hp_to_zero(c, opp, owner);
        }
        return;
    }
    if (RIDER == AttackRider::FixedDamage) {
        if (!roll_soul_percent(*c, mark_id, 0, owner, opp, pct_raw)) {
            return;
        }
        if (g_core && g_core->fixed_damage) {
            (void)g_core->fixed_damage(c, opp, arg2);
        }
        return;
    }
    // Burn：概率申报给原语（内核单掷、亮节管辖，K2 同款）——禁本地 roll 双掷
    if (g_core && g_core->apply_anomaly_checked) {
        (void)g_core->apply_anomaly_checked(c, opp, arg2, random_anomaly_duration(),
                                            owner, pct_raw, ChanceSource::SoulMark);
    }
}

template <AttackRider RIDER>
EffectResult attack_prob_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    constexpr int MARK = (RIDER == AttackRider::FixedDamage) ? 109
        : ((RIDER == AttackRider::Burn) ? 116 : 151);
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (watcher_wired(ctx, MARK, owner)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, MARK);
    const int db0 = db_at(row, 0, -1);
    const int db1 = db_at(row, 1, -1);
    if (db0 <= 0) {
        return EffectResult::kOk;   // 无概率行不武装
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, db0, db1](BattleContext* c, const BattleEvent& ev) {
            if (!c || !own_attack_event(c, owner, ev, MARK)) {
                return;
            }
            rider_dispatch<RIDER>(c, MARK, owner, 1 - owner, db0, db1);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

EffectResult soulmark_109_arm(BattleContext* ctx, const EffectArgs& args) {
    return attack_prob_arm<AttackRider::FixedDamage>(ctx, args);
}
EffectResult soulmark_116_arm(BattleContext* ctx, const EffectArgs& args) {
    return attack_prob_arm<AttackRider::Burn>(ctx, args);
}
EffectResult soulmark_151_arm(BattleContext* ctx, const EffectArgs& args) {
    return attack_prob_arm<AttackRider::Kill>(ctx, args);
}

// ── 266：命中压制乘区（ROUND_START soul 节点每回合重写）──────────────────────

// args 空行以行文为数（K5 的 7 同款）：monster → pct。
int row_pct_266(const HostRow& row, int db0) {
    if (db0 > 0) {
        return db0;
    }
    for (const auto& [monster, value] :
         {std::pair<int, int>{2462, 5}, {2563, 8}, {2761, 8}}) {
        if (row.monster_id == monster) {
            return value;
        }
    }
    return -1;
}

EffectResult soulmark_266_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (!onstage_has_soul_mark(ctx, owner, 266)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, 266);
    const int db0 = db_at(row, 0, -1);
    const int pct = row_pct_266(row, db0);
    if (pct <= 0) {
        return EffectResult::kOk;
    }
    ctx->ws.hit_rate_mod_attack[1 - owner] = 1.0f - static_cast<float>(pct) / 100.0f;
    return EffectResult::kOk;
}

EffectResult soulmark_266_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (watcher_wired(ctx, 266, owner)) {
        return EffectResult::kOk;
    }
    return EffectResult::kOk;
}

}  // namespace

// 注册（+4 宏：109/116/151/266）。266 的 arm 仅 dedup 占位；实际重写在 ROUND_START 节点。
SOUL_PROGRAM(109, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_109_arm,
                    /*once=*/false, /*early=*/true)});
SOUL_PROGRAM(116, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_116_arm,
                    /*once=*/false, /*early=*/true)});
SOUL_PROGRAM(151, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_151_arm,
                    /*once=*/false, /*early=*/true)});
SOUL_PROGRAM(266, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_266_arm,
                    /*once=*/false, /*early=*/true),
                   SoulMarkNodeRef(State::BATTLE_ROUND_START, &soulmark_266_round_start,
                    /*once=*/false, /*early=*/false)});
