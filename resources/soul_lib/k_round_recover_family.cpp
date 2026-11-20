// K10 回合恢复/反弹/双节点族 —— effect 637/693/227/217（4 宏 6 行）
//
// 行清单（tools/soul_effects_dump.py 637 693 227 217，2026-10-04）：
//   637 2 行 args[回量,反比分母,?]：2892 希尔妮妮[150,3,1]「每回合恢复150HP;反弹1/3
//       对手造成的伤害」/ 2905 九霄雷魂[150,0,1]（无反弹子句）
//       → ROUND_START heal（622/636 同款桶）+ TAKE_DAMAGE 比例反弹 watcher
//       （K3 thorn 同款：ev.actor 范围守卫——ev.actor 可能 -1（UBSan fuzz 实录），
//       is_attack_damage_event 门）
//   693 3 行 args[攻附flag,已损%,回血flag(1=用攻回/2=用属回),回量]：
//       2992 圣诞神使[1,50,2,200]「攻击技附加自身已损50%固伤;属性技恢复200HP」/
//       3144 克拉罗[0,0,1,150]「使用攻击技能时立刻恢复150点体力」/ 3219 铸炼师[1,60]
//       「攻击附加自身已损60%的固定伤害」（两参半行——回血段缺省=无该子句）
//       → SKILL_EFFECT 时点（9 宏同款 roundChoice 读所选技）：攻击技→粉伤/heal，
//       属性技→heal（db[2]==2）
//   227 1 行（1909/1910 卢恩斯/卢卡斯 args空）「登场后每三回合，必定获得一次致命一击」
//       → 登场计数器（ENTER_EXIT 重置）+ ROUND_START ++；count%3==0 → ws.must_crit
//       （每回合 reset → 命中时点消费，battleWorkspace.h must_crit 套路）
//   217 1 行（1731/1732 哈迪尔/哈迪斯 args空）「出战时，若首回合受到致命伤害则保留
//       1点体力」→ EVENT_TAKE_DAMAGE watcher + hp==0 → hp=1（undying_1067 同族，
//       首回合门 roundCount==0 + 每场一次 consumed 旗标）
//
// 三查记录（2026-10-04）：①内核无四 id 硬编码；②effect_info 同 id 为技能空间模板；
// ③registry 无四 id，未合并分支无实现。
#include "common.h"

namespace {

// ── 宿主行参数视图（各文件自治同款）────────────────────────────────────────

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

bool armed_once(BattleContext* ctx, int owner, int key) {
    if (ctx->plugin_storage.count(key)) {
        return true;
    }
    ctx->plugin_storage.emplace(key, 1);
    return false;
}

// arm 幂等守卫（K5/K8 int 槽版，先 find 后 insert）。
bool armed_this_epoch(BattleContext* ctx, int owner, int storage_key) {
    ElfPet& pet = ctx->getPet(owner);
    auto it = pet.soulmark_storage.find(storage_key);
    if (it == pet.soulmark_storage.end()) {
        it = pet.soulmark_storage.emplace(storage_key, int{-1}).first;
    }
    int* armed_epoch = std::any_cast<int>(&it->second);
    if (!armed_epoch) {
        return true;
    }
    if (*armed_epoch == ctx->pipeline_valid_id[owner]) {
        return true;
    }
    *armed_epoch = ctx->pipeline_valid_id[owner];
    return false;
}

bool is_attack_damage_event(const BattleEvent& ev) {
    return ev.state == static_cast<int>(State::BATTLE_FIRST_ATTACK_DAMAGE)
        || ev.state == static_cast<int>(State::BATTLE_SECOND_ATTACK_DAMAGE);
}

// ── 637：每回合回血 + 比例反弹 ─────────────────────────────────────────────

EffectResult soulmark_637_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 637)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, 637);
    const int heal = db_at(row, 0, 150);
    if (heal > 0 && g_core && g_core->heal_amount) {
        g_core->heal_amount(ctx, owner, heal);
    }
    return EffectResult::kOk;
}

EffectResult soulmark_637_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (armed_this_epoch(ctx, owner, 637 * 100 + 9)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, 637);
    const int denom = db_at(row, 1, row.monster_id == 2892 ? 3 : 0);
    if (denom <= 0) {
        return EffectResult::kOk;   // 2905 无反弹子句
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, denom](BattleContext* wctx, const BattleEvent& ev) {
            // ⚠️ ev.actor 可能 -1（无行动方事件）——范围守卫必须带（K3 thorn 同款）。
            if (!wctx || ev.target != owner || ev.actor < 0 || ev.actor > 1
                || ev.actor == owner || ev.amount <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(wctx, owner, 637) || !is_attack_damage_event(ev)) {
                return;
            }
            if (g_core && g_core->deal_pink_damage) {
                g_core->deal_pink_damage(wctx, ev.actor, ev.amount / denom,
                                         DamageKind::FIXED, owner);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 693：攻击技附粉（已损%）+ 攻/属回血（双节点程序）────────────────────────

EffectResult soulmark_693_skill_effect(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 693)) {
        return EffectResult::kOk;
    }
    if (ctx->roundChoice[owner][0] != 1 /*SELECT_SKILL*/) {
        return EffectResult::kOk;
    }
    const int idx = ctx->roundChoice[owner][1];
    if (idx < 0 || idx >= 5) {
        return EffectResult::kOk;
    }
    const bool is_attribute =
        ctx->getPet(owner).skills[idx].type == SkillType::Attribute;
    const HostRow row = host_row(ctx, args, 693);
    const int attack_fixed_flag = db_at(row, 0, -1);
    const int lost_pct = db_at(row, 1, -1);
    int heal_flag = db_at(row, 2, -1);
    int heal_amount = db_at(row, 3, -1);
    // 半行兜底（3219 [1,60]：回血段缺省；无参行按 monster 查表）。
    if (heal_flag < 0) {
        if (row.monster_id == 2992) {
            heal_flag = 2;
            heal_amount = 200;
        } else if (row.monster_id == 3144) {
            heal_flag = 1;
            heal_amount = 150;
        }
    }
    if (attack_fixed_flag < 0 && heal_flag < 0) {
        return EffectResult::kOk;
    }
    if (!is_attribute) {
        if (attack_fixed_flag == 1 && lost_pct > 0 && g_core && g_core->deal_pink_damage) {
            const ElfPet& self = ctx->getPet(owner);
            const int max_hp = resolve_pet_max_hp(self);
            const int lost = max_hp > 0 ? max_hp - self.hp : 0;
            if (lost > 0) {
                g_core->deal_pink_damage(ctx, ctx->opponent(owner),
                                         lost * lost_pct / 100, DamageKind::FIXED, owner);
            }
        }
        if (heal_flag == 1 && heal_amount > 0 && g_core && g_core->heal_amount) {
            g_core->heal_amount(ctx, owner, heal_amount);
        }
    } else if (heal_flag == 2 && heal_amount > 0 && g_core && g_core->heal_amount) {
        g_core->heal_amount(ctx, owner, heal_amount);
    }
    return EffectResult::kOk;
}

// ── 227：登场后每三回合必爆 ────────────────────────────────────────────────

constexpr int k227CountBase = 990227000;   // 登场计数器（pet.soulmark_storage）

EffectResult soulmark_227_enter(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    // 每次登场重置计数（"登场后每三回合"——下场清零）。
    ctx->getPet(owner).soulmark_storage[k227CountBase] = int{0};
    return EffectResult::kOk;
}

EffectResult soulmark_227_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 227)) {
        return EffectResult::kOk;
    }
    ElfPet& pet = ctx->getPet(owner);
    auto it = pet.soulmark_storage.find(k227CountBase);
    if (it == pet.soulmark_storage.end()) {
        it = pet.soulmark_storage.emplace(k227CountBase, int{0}).first;
    }
    int* count = std::any_cast<int>(&it->second);
    if (!count) {
        return EffectResult::kOk;
    }
    ++*count;
    if (*count % 3 == 0) {
        ctx->ws.must_crit[owner] = true;
    }
    return EffectResult::kOk;
}

// ── 217：首回合受致命伤保留 1 点体力 ───────────────────────────────────────

struct FirstRoundGuard {
    bool consumed = false;
};

EffectResult soulmark_217_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (armed_once(ctx, owner, 217 * 100 + 7)) {
        return EffectResult::kOk;   // watcher 只 arm 一次（TEAM 永久，1067 同款）
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || ev.target != owner || ev.actor < 0 || ev.actor > 1) {
                return;
            }
            ElfPet& self = wctx->getPet(owner);
            auto& storage = self.soulmark_storage;
            auto it = storage.find(217 * 100 + 1);
            if (it == storage.end()) {
                it = storage.emplace(217 * 100 + 1, FirstRoundGuard{}).first;
            }
            FirstRoundGuard* guard = std::any_cast<FirstRoundGuard>(&it->second);
            if (!guard || guard->consumed) {
                return;
            }
            // "首回合"：round 1 的 roundCount==0（回合完成时才递增）。
            if (wctx->roundCount != 0 || self.hp > 0 || !is_attack_damage_event(ev)) {
                return;
            }
            guard->consumed = true;
            self.hp = 1;
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +4（只增不改）────────────────
SOUL_PROGRAM(637, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_637_arm,
                   /*once=*/false, /*early=*/true),
                   SoulMarkNodeRef(State::BATTLE_ROUND_START, &soulmark_637_round_start)});
SOUL_PROGRAM(693, {SoulMarkNodeRef(State::BATTLE_FIRST_SKILL_EFFECT, &soulmark_693_skill_effect),
                   SoulMarkNodeRef(State::BATTLE_SECOND_SKILL_EFFECT, &soulmark_693_skill_effect)});
SOUL_PROGRAM(227, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_227_enter,
                   /*once=*/false, /*early=*/true),
                   SoulMarkNodeRef(State::BATTLE_ROUND_START, &soulmark_227_round_start)});
SOUL_PROGRAM(217, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_217_arm,
                   /*once=*/false, /*early=*/true)});
