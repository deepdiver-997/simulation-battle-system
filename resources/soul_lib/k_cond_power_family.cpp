// K9 条件固伤/威力修正族 —— effect 293/242/733/134（4 宏 8 行）
//
// 行清单（tools/soul_effects_dump.py 293 242 733 134，2026-10-04）：
//   293 4 行 args[异常码,100,固伤]：「若对手处于X状态，则(100%)附加N点固定伤害」
//       2568 灭世战魂[5,100,100] 冻伤 / 2611 九霄雷影[0,100,150] 麻痹 /
//       2640 炎龙圣皇[2,100,100] 烧伤 / 2697 太古邪灵[8,100,100] 睡眠
//       （异常码 = 官方码 0麻痹/2烧伤/5冻伤/8睡眠，速查 §三）→ TAKE_DAMAGE watcher
//       （K1 766"攻击附加固伤"同款：ev.state 限两攻击时点、raw_attack_damage>0 门）
//   242 1 行（2204/2205 莫迪尔/莫迪西斯 args空）：「对手体力低于1/3时自身技能威力翻倍」
//       → SKILL_EFFECT 时点 skill_power_view ×2（636 对手异常翻倍同路）
//   733 2 行 args[pct,倍数]：3052 威魄战车[30,2] / 3103 青莲剑神[50,2]
//       「所有攻击技能有X%概率威力翻倍」→ SKILL_EFFECT roll + 攻击技门 → view ×db[1]
//   134 1 行（809/810/811 灵驹族 args空）：「草系技能威力提升5%」
//       → SKILL_EFFECT 草系门（element[0]==1，DB 实证 飞叶/藤鞭/叶片 type_id=1）
//       → view ×105/100
//
// 时点依据：BATTLE_FIRST/SECOND_SKILL_EFFECT 时 skill_power_view 已物化（636/2501 同路，
// SKILL_USED 太早 view=-1 未物化——k_round_stat.cpp 头注）。攻击技门 = 技能 type 非
// Attribute（636 同款 roundChoice 读所选技）。
//
// 三查记录（2026-10-04）：①内核无四 id 硬编码；②effect_info 同 id 为技能空间模板；
// ③registry 无四 id（soul=75 基线），未合并分支无实现。
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

// ── 293：对手处于 X 异常 → 攻击附加 N 点固伤 ────────────────────────────────

// arm 幂等守卫（K1 EpochGuard 同款语义，int 槽版；先 find 后 insert——operator[]
// 会插入空 any 使"已占用"判定恒真，K5 首跑事故同款）。
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

EffectResult soulmark_293_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (armed_this_epoch(ctx, owner, 293 * 100 + 9)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, 293);
    const int anomaly = db_at(row, 0, row.monster_id == 2697 ? 8 : -1);
    const int pct = db_at(row, 1, 100);
    const int fixed = db_at(row, 2, row.monster_id == 2611 ? 150 : -1);
    if (anomaly < 0 || fixed <= 0) {
        return EffectResult::kOk;   // 无参且 monster 不在表 → 记档（K1 row_pct 口径）
    }
    // ⚠️ 用管线 DETECT 相位（攻击伤害处理内同步执行），不用 TAKE_DAMAGE watcher：
    //    watcher 交付被推迟到 AFTER_ACTION（实测 end_round 已被清——睡眠在承担方行动
    //    处理时即消耗），"对手处于 X 状态"必须赶在状态还活着的时候同步查询。
    ctx->register_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner, anomaly, pct, fixed](BattleContext* c, int bucket_owner) {
            if (!c || !g_core || !g_core->deal_pink_damage || !g_core->has_active_anomaly) {
                return;
            }
            if (bucket_owner != owner) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || !d.isRed || d.final <= 0) {
                return;   // 只随自己的攻击红伤附加
            }
            if (!onstage_has_soul_mark(c, owner, 293)) {
                return;
            }
            if (!roll_percent(pct)) {
                return;
            }
            if (!g_core->has_active_anomaly(c, c->opponent(owner), anomaly)) {
                return;
            }
            g_core->deal_pink_damage(c, c->opponent(owner), fixed, DamageKind::FIXED, owner);
        });
    return EffectResult::kOk;
}

// ── 242/733/134 共用：SKILL_EFFECT 时点的威力视图改写 ──────────────────────

// 所选技（636 同款 roundChoice 读法）；返回 -1 = 非技能选择。
int selected_skill_index(BattleContext* ctx, int owner) {
    if (ctx->roundChoice[owner][0] != 1 /*SELECT_SKILL*/) {
        return -1;
    }
    const int idx = ctx->roundChoice[owner][1];
    return (idx >= 0 && idx < 5) ? idx : -1;
}

bool selected_is_attack(BattleContext* ctx, int owner, int idx) {
    return ctx->getPet(owner).skills[idx].type != SkillType::Attribute;
}

void multiply_power_view(BattleContext* ctx, int owner, int num, int den) {
    auto& view = ctx->ws.skill_power_view[owner];
    if (view.value > 0) {
        view = view.value * num / den;   // 636/2501 同款赋值形（rewritten 管线收尾重算）
    }
}

EffectResult soulmark_242_skill_effect(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 242)) {
        return EffectResult::kOk;
    }
    const int idx = selected_skill_index(ctx, owner);
    if (idx < 0 || !selected_is_attack(ctx, owner, idx)) {
        return EffectResult::kOk;
    }
    const ElfPet& foe = ctx->getPet(ctx->opponent(owner));
    const int foe_max = resolve_pet_max_hp(foe);
    if (foe_max > 0 && foe.hp > 0 && foe.hp * 3 < foe_max) {
        multiply_power_view(ctx, owner, 2, 1);   // "威力翻倍"，args 空 → 行文字面
    }
    return EffectResult::kOk;
}

EffectResult soulmark_733_skill_effect(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 733)) {
        return EffectResult::kOk;
    }
    const int idx = selected_skill_index(ctx, owner);
    if (idx < 0 || !selected_is_attack(ctx, owner, idx)) {
        return EffectResult::kOk;   // 只翻攻击技
    }
    const HostRow row = host_row(ctx, args, 733);
    const int pct = db_at(row, 0, row.monster_id == 3103 ? 50 : -1);
    const int mult = db_at(row, 1, 2);
    if (pct <= 0 || !roll_percent(pct)) {
        return EffectResult::kOk;
    }
    multiply_power_view(ctx, owner, mult, 1);
    return EffectResult::kOk;
}

EffectResult soulmark_134_skill_effect(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 134)) {
        return EffectResult::kOk;
    }
    const int idx = selected_skill_index(ctx, owner);
    if (idx < 0) {
        return EffectResult::kOk;
    }
    // 草系 type_id=1（DB 实证：飞叶风暴/藤鞭/叶片/疾风刃均 1；args 空 → 5% 行文字面）。
    if (ctx->getPet(owner).skills[idx].element[0] != 1) {
        return EffectResult::kOk;
    }
    multiply_power_view(ctx, owner, 105, 100);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +4（只增不改）────────────────
SOUL_PROGRAM(293, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_293_arm,
                   /*once=*/false, /*early=*/true)});
SOUL_PROGRAM(242, {SoulMarkNodeRef(State::BATTLE_FIRST_SKILL_EFFECT, &soulmark_242_skill_effect),
                   SoulMarkNodeRef(State::BATTLE_SECOND_SKILL_EFFECT, &soulmark_242_skill_effect)});
SOUL_PROGRAM(733, {SoulMarkNodeRef(State::BATTLE_FIRST_SKILL_EFFECT, &soulmark_733_skill_effect),
                   SoulMarkNodeRef(State::BATTLE_SECOND_SKILL_EFFECT, &soulmark_733_skill_effect)});
SOUL_PROGRAM(134, {SoulMarkNodeRef(State::BATTLE_FIRST_SKILL_EFFECT, &soulmark_134_skill_effect),
                   SoulMarkNodeRef(State::BATTLE_SECOND_SKILL_EFFECT, &soulmark_134_skill_effect)});
