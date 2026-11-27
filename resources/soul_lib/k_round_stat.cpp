// K4 回合能力变化族 —— effect 607/612/622/636/641/642（6 宏落地；390/1224 记档）
//
// 行清单与参数布局（tools/soul_effects_dump.py 390 642 1224 607 612 622 636 641，2026-10-01；
// 每宏的编码用"多行互锚/单行唯一解"逐个破解，k_allowed 布局互不通用——行≠宏本义）：
//   612 萨瑞罗斯（两行同构）args[350,100,delta,1]："受到的伤害大于350恢复全部体力
//       （boss无效）"+"每回合双防+1/+2"→ db[0]=阈值 db[1]=回复% db[2]=双防增量；
//       回满挂 TAKE_DAMAGE（两攻击时点）watcher，判定值 = 链首快照 ws.guard_detect_damage
//       （犀牛 1217 同款口径：增伤前）
//   622 雷魂·雷锘 args[1,0]："免疫能力下降，每回合使自身随机属性+1" → ROUND_START 节点：
//       每回合 grant_immunity(STAT_DROP, 本回合) + 随机槽 stat_change(+1)×1（db[0]=项数）
//   636 凯普斯特（两行同构）args[count, (slot,delta)×count, …]：ENGINE 槽 (0,1)(4,1)
//       = 攻+1速+1 / 伙伴行 (0,2)(4,2) → ROUND_START 逐对 stat_change；
//       另一子句"对手处于异常状态时自身攻击威力翻倍"：SKILL_USED(actor=own, 攻击技) 时
//       对手有任意活跃异常 → ws.skill_power_view 翻倍（1443 变威力管线同路）
//   641 爆炎莱茵 args[2, 1,0, 1,4, 1]：count + (delta,slot)×count（注意与 636 的
//       (slot,delta) 相反！唯一解锚定：tips 攻+1速+1 = (1,0)(1,4)）+ 尾参不消费
//   642 赤瞳缪斯/玄冰之魂 args[(code,delta)×2, fixed, ?]：code 表锚定两行 tips——
//       缪斯"特攻+1速+1"=(2,1)(4,1)、玄冰"(2,0)(4,0) 无变化"→ code 映射
//       {1:攻0, 2:特攻1, 3:防2, 4:速4, 5:特防3, 6:命中5}（⚠️ 仅这两行锚定，新行先验）；
//       尾部 fixed=150 = "每回合附加150点固定伤害"（ROUND_START 粉伤 FIXED 给对手）
//   607 赤瞳缪斯 args[1,150,1]："每回合附加150点固定伤害"照做；"每回合速度+1"参数缺失
//       → 记档（该子句未编码，不硬编）
//   记档跳过：390（五行参数各异——五拍参数无法自洽解码，需官方实测裁决，S1 箱三口径）；
//   1224（tips 残文"则下回合…"截断 + 失明(20)未建模，等内核微任务）。
// 三查记录（2026-10-01）：①内核无这些魂印 id 硬编码；②effect_info 均技能空间模板；
// ③全库无同 id 注册。
#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

// 642 的槽码表（锚定见头注；ENGINE 槽 0攻1特攻2防3特防4速5命中）。
int code642_to_slot(int code) {
    switch (code) {
        case 1: return 0;   // 攻
        case 2: return 1;   // 特攻
        case 3: return 2;   // 防
        case 4: return 4;   // 速
        case 5: return 3;   // 特防
        case 6: return 5;   // 命中
        default: return -1;
    }
}

struct HostRow {
    const std::vector<int>* db = nullptr;
    int monster_id = 0;
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
    return row;
}

int db_at(const HostRow& row, int idx, int fallback) {
    if (row.db && idx >= 0 && idx < static_cast<int>(row.db->size())) {
        return (*row.db)[idx];
    }
    return fallback;
}

bool watcher_wired(BattleContext* ctx, int macro, int owner) {
    const int key = 990691000 + macro * 10 + owner;
    if (ctx->plugin_storage.count(key)) {
        return true;
    }
    ctx->plugin_storage.emplace(key, 1);
    return false;
}

// ── 612：受伤>阈值回满（链首判定）+ 每回合双防 ──────────────────────────────

EffectResult soulmark_612_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, 612);
    const int threshold = db_at(row, 0, 350);
    const int heal_pct = db_at(row, 1, 100);
    const int def_delta = db_at(row, 2, 1);
    if (watcher_wired(ctx, 612, owner)) {
        return EffectResult::kOk;
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, threshold, heal_pct](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || ev.target != owner || ev.actor == owner || ev.amount <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(wctx, owner, 612)) {
                return;
            }
            if (ev.state != static_cast<int>(State::BATTLE_FIRST_ATTACK_DAMAGE)
                && ev.state != static_cast<int>(State::BATTLE_SECOND_ATTACK_DAMAGE)) {
                return;
            }
            // 判定 = 链首快照（犀牛口径："受到的伤害"增伤前）。
            if (wctx->ws.guard_detect_damage[owner] <= threshold) {
                return;
            }
            if (g_core && g_core->heal_amount) {
                const int max_hp = resolve_pet_max_hp(wctx->getPet(owner));
                g_core->heal_amount(wctx, owner, max_hp * heal_pct / 100);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

EffectResult soulmark_612_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 612)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, 612);
    const int def_delta = db_at(row, 2, 1);
    if (def_delta != 0 && g_core && g_core->stat_change) {
        g_core->stat_change(ctx, owner, /*防御槽=*/2, def_delta);
        g_core->stat_change(ctx, owner, /*特防槽=*/3, def_delta);
    }
    return EffectResult::kOk;
}

// ── 622：免弱 + 每回合随机属性 +1 ───────────────────────────────────────────

EffectResult soulmark_622_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 622)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, 622);
    const int count = db_at(row, 0, 1);
    // 免疫能力下降：每回合重挂（duration 1 = 本回合；coverage 全行动段）。
    ctx->grant_immunity(owner, ImmunityType::STAT_DROP, coverage_all(), /*anomaly_mask=*/0,
                        /*duration_rounds=*/1, /*source_id=*/622 * 10,
                        /*soul_immunity=*/true, EffectScope::ON_STAGE, /*counts=*/0,
                        /*source_effect_id=*/622);
    if (g_core && g_core->stat_change) {
        for (int i = 0; i < count; ++i) {
            g_core->stat_change(ctx, owner, ctx->rng().next() % 6, 1);   // 随机属性 +1（0..5 槽）
        }
    }
    return EffectResult::kOk;
}

// ── 636：每回合 stat 对 + 对手异常时威力翻倍 ────────────────────────────────

EffectResult soulmark_636_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 636)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, 636);
    const int count = db_at(row, 0, 2);
    if (g_core && g_core->stat_change) {
        for (int i = 0; i < count; ++i) {
            const int slot = db_at(row, 1 + i * 2, -1);
            const int delta = db_at(row, 2 + i * 2, 0);
            if (slot >= 0 && slot <= 5 && delta != 0) {
                g_core->stat_change(ctx, owner, slot, delta);
            }
        }
    }
    return EffectResult::kOk;
}

// "对手处于异常状态时自身攻击威力翻倍"：BATTLE_*_SKILL_EFFECT 时点写 skill_power_view
// （2501"按特攻翻倍"同路——SKILL_USED watcher 太早，view 尚未物化； ATTACK_DAMAGE 桶
// 收尾看到 rewritten 标记会按改写值重算）。回合门 = roundChoice（沧岚 after_action 同款）。
EffectResult soulmark_636_skill_effect(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 636)) {
        return EffectResult::kOk;
    }
    if (ctx->roundChoice[owner][0] != 1 /*SELECT_SKILL*/) {
        return EffectResult::kOk;
    }
    const int idx = ctx->roundChoice[owner][1];
    if (idx < 0 || idx >= 5 || ctx->getPet(owner).skills[idx].type == SkillType::Attribute) {
        return EffectResult::kOk;   // 只翻攻击技的威力
    }
    if (!ctx->has_any_active_abnormal_status(1 - owner)) {
        return EffectResult::kOk;
    }
    auto& view = ctx->ws.skill_power_view[owner];
    if (view.value > 0) {
        view = view.value * 2;
    }
    return EffectResult::kOk;
}

// ── 641：每回合 stat 对（(delta,slot) 布局，与 636 相反）────────────────────

EffectResult soulmark_641_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 641)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, 641);
    const int count = db_at(row, 0, 2);
    if (g_core && g_core->stat_change) {
        for (int i = 0; i < count; ++i) {
            const int delta = db_at(row, 1 + i * 2, 0);
            const int slot = db_at(row, 2 + i * 2, -1);
            if (slot >= 0 && slot <= 5 && delta != 0) {
                g_core->stat_change(ctx, owner, slot, delta);
            }
        }
    }
    return EffectResult::kOk;
}

// ── 642 / 607：每回合 code 表 stat 对 + 定额粉伤 ────────────────────────────

EffectResult soulmark_642_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 642)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, 642);
    // (code,delta) 对：读到 code 非法（=进入 fixed 段）为止。
    for (int i = 0; i + 1 < static_cast<int>(row.db->size()); i += 2) {
        const int slot = code642_to_slot((*row.db)[i]);
        const int delta = (*row.db)[i + 1];
        if (slot < 0) {
            break;
        }
        if (delta != 0 && g_core && g_core->stat_change) {
            g_core->stat_change(ctx, owner, slot, delta);
        }
    }
    // "每回合附加150点固定伤害"：数值取第一个 ≥100 的参数（缪斯/玄冰行都是 150）。
    for (int i = 0; i < static_cast<int>(row.db->size()); ++i) {
        const int fixed = (*row.db)[i];
        if (fixed >= 100 && g_core && g_core->deal_pink_damage) {
            g_core->deal_pink_damage(ctx, 1 - owner, fixed, DamageKind::FIXED, owner);
            break;
        }
    }
    return EffectResult::kOk;
}

EffectResult soulmark_607_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 607)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, 607);
    const int fixed = db_at(row, 1, 150);
    if (fixed > 0 && g_core && g_core->deal_pink_damage) {
        g_core->deal_pink_damage(ctx, 1 - owner, fixed, DamageKind::FIXED, owner);
    }
    return EffectResult::kOk;   // "每回合速度+1"参数缺失 → 记档（见头注）
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +6（390/1224 记档；只增不改）──
SOUL_PROGRAM(607, {SoulMarkNodeRef(State::BATTLE_ROUND_START, &soulmark_607_round_start)});
SOUL_PROGRAM(612, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_612_arm,
                    /*once=*/false, /*early=*/true),
                   SoulMarkNodeRef(State::BATTLE_ROUND_START, &soulmark_612_round_start)});
SOUL_PROGRAM(622, {SoulMarkNodeRef(State::BATTLE_ROUND_START, &soulmark_622_round_start)});
SOUL_PROGRAM(636, {SoulMarkNodeRef(State::BATTLE_ROUND_START, &soulmark_636_round_start),
                   SoulMarkNodeRef(State::BATTLE_FIRST_SKILL_EFFECT, &soulmark_636_skill_effect),
                   SoulMarkNodeRef(State::BATTLE_SECOND_SKILL_EFFECT, &soulmark_636_skill_effect)});
SOUL_PROGRAM(641, {SoulMarkNodeRef(State::BATTLE_ROUND_START, &soulmark_641_round_start)});
SOUL_PROGRAM(642, {SoulMarkNodeRef(State::BATTLE_ROUND_START, &soulmark_642_round_start)});
