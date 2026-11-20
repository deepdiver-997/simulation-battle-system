// E7 · 先制族批产——828「若自身处于能力提升状态则先制+1」×44 /
//      1080「连续使用时先制+1」×43 / 523「（若当回合未击败对手）攻击结束时自身{位图}+1」×45
//（2026-09-29，E 组按家族批产，一个文件一家族）
//
// 前置三查（2026-09-29）：①内核无 828/1080/523 硬编码（skills.cpp switch 表与
// penetration_flags 均无——防 697 式假阳性）；②effect_meta 无分类标记；
// ③全库无 SKILL_EFFECT 注册（828/1080/523 均真缺口）。
//
// 828（0 参）"若自身处于能力提升状态则先制+1"：
//   - 条件先制族 = 1782/1002 同构：武装节点（SKILL_EFFECT 命中趟一次性）注册
//     MOVE_RIGHT 常驻 tick（999 回合），tick 判"所选技能携带本效果"→ 自身任一
//     能力等级 >0 → preemptive_level += 1。
//   - 多载体判定不走硬编码技能 id（828 载体一批：泰坦盾击 30099/迅捷之月 18369/
//     邪皇血符 24389…），改查 **Skills::rawEffectRecords**（公开成员，战斗期随拷贝
//     携带）——selected 技能的记录里含 effect_id==828 即载体，天然覆盖全部载体。
//   - ⚠️ 首用结构性偏差记档（1002/1782 同根源）：先手权结算先于 SKILL_EFFECT
//     武装 → 每场第 1 次使用不加先制（官方应加），第 2 次起正常。
// 1080（0 参）"连续使用时先制+1"：
//   - 同款 MOVE_RIGHT tick + 载体判定；连用判定 per-owner storage
//     （key=1080000+owner）记 last_used_round：last >= roundCount-1（上回合或本
//     回合早先已用过载体技）→ +1。**只在选载体技时更新 last**——中间穿插别的技能
//     则 last 停留旧值，自然中断（1155 连用计数的 per-pet storage 先例）。
//   - 无首用偏差：首用本就不算"连续"（天然正确）。
//   - 同回合额外行动再次出手（last==roundCount）按连续处理（字面口径记档）。
// 523（6 参）"攻击结束时自身{位图}"：
//   - 4399 实证位图序 = [攻,防,特攻,特防,速,命中]（断裂星空闪 14843 [1,0,0,0,1,0]
//     →"攻击、速度+1"；弑神冥火爆 15939 [0,0,1,0,0,0] →"特攻+1"；炽天使之光
//     15426/万月千星箭 16492 同验证），模板 info 截断了条件子句——4399 各载体
//     一律为"（若）当回合未击败对手"。
//   - AFTER_ACTION 桥（1962 同款双桶 FIRST/SECOND + rounds=1）：命中趟武装，
//     本回合行动后 tick——对手存活（未击败）→ 按位图非零位 stat_change(+1)。
//   - 提升走 stat_change 原语（+1 非下降，不涉 STAT_DROP 免疫面）。
//
// Args 约定（E 组协议）：int_args 前置 [owner, target]，模板参数从 [2] 起
//（523：[owner, target, 攻,防,特攻,特防,速,命中] 共 8 位）。
//
// 场景回归：test/scenario/scenario_520_priority_family.cpp。

#include "common.h"

namespace {

constexpr int kActionSelectSkill = 1;        // roundChoice[side][0] 的 SELECT_SKILL
constexpr int k828ArmedKeyBase = 990082800;  // 828 常驻节点已注册旗标（owner 侧）
constexpr int k1080ArmedKeyBase = 990108000; // 1080 常驻节点已注册旗标（owner 侧）
constexpr int k1080LastRoundBase = 1080000;  // 1080 连用计数：last_used_round（owner 侧）

// 本回合 owner 所选技能的 rawEffectRecords 是否携带 effect_id（多载体通用判定）。
bool selected_skill_has_effect(BattleContext* ctx, int owner, int effect_id) {
    if (!ctx || owner < 0 || owner > 1 || ctx->roundChoice[owner][0] != kActionSelectSkill) {
        return false;
    }
    const int slot = ctx->roundChoice[owner][1];
    const auto& skills = ctx->getPet(owner).skills;
    if (slot < 0 || slot >= static_cast<int>(skills.size())) {
        return false;
    }
    for (const official_data::SkillEffectRecord& rec :
         skills[static_cast<std::size_t>(slot)].rawEffectRecords) {
        if (rec.effect_id == effect_id) {
            return true;
        }
    }
    return false;
}

bool has_any_positive_boost(BattleContext* ctx, int owner) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[owner][i] > 0) {
            return true;
        }
    }
    return false;
}

int storage_int(BattleContext* ctx, int key) {
    const auto it = ctx->plugin_storage.find(key);
    return it == ctx->plugin_storage.end() ? 0 : any_int_or(it->second);
}

// ── 828 的 MOVE_RIGHT tick：载体技 + 自身有提升 → 先制+1 ──
EffectResult effect_828_priority_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1 || !selected_skill_has_effect(ctx, owner, 828)) {
        return EffectResult::kOk;
    }
    if (has_any_positive_boost(ctx, owner)) {
        ctx->ws.preemptive_level[owner] += 1;
    }
    return EffectResult::kOk;
}

// ── 828 —— 若自身处于能力提升状态则先制+1（武装节点）──
EffectResult effect_828_boost_priority(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int key = k828ArmedKeyBase + owner;
    if (ctx->plugin_storage.count(key)) {
        return EffectResult::kOk;   // 常驻节点只注册一次
    }
    ctx->plugin_storage[key] = 1;
    register_round_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, owner, 828,
                          /*rounds=*/999, {owner}, &effect_828_priority_tick);
    return EffectResult::kOk;
}

// ── 1080 的 MOVE_RIGHT tick：载体技 + 连用 → 先制+1 ──
EffectResult effect_1080_priority_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1 || !selected_skill_has_effect(ctx, owner, 1080)) {
        return EffectResult::kOk;   // 非载体技：不判连用、不更新 last（自然中断）
    }
    const int key = k1080LastRoundBase + owner;
    const int last = storage_int(ctx, key);
    if (last >= ctx->roundCount - 1) {   // 上回合或本回合早先已用 → 连用
        ctx->ws.preemptive_level[owner] += 1;
    }
    ctx->plugin_storage[key] = ctx->roundCount;
    return EffectResult::kOk;
}

// ── 1080 —— 连续使用时先制+1（武装节点；命中趟每次使用都补记 last，
// 否则首场首次使用（tick 尚未注册）的"用过"不被计数，连用判定会晚一回合）──
EffectResult effect_1080_consecutive_priority(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage[k1080LastRoundBase + owner] = ctx->roundCount;   // 命中趟 = 真的使用了
    const int key = k1080ArmedKeyBase + owner;
    if (ctx->plugin_storage.count(key)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage[key] = 1;
    register_round_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, owner, 1080,
                          /*rounds=*/999, {owner}, &effect_1080_priority_tick);
    return EffectResult::kOk;
}

// ── 523 的 AFTER_ACTION tick：未击败对手 → 按位图自身提升 ──
EffectResult effect_523_bitmap_boost_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 8 || !g_core || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(target).hp <= 0) {
        return EffectResult::kOk;   // 已击败对手：不提升
    }
    for (int i = 0; i < 6; ++i) {   // 位图序 = [攻,防,特攻,特防,速,命中]
        const int steps = args.int_args[2 + i];
        if (steps > 0) {
            g_core->stat_change(ctx, owner, i, steps);
        }
    }
    return EffectResult::kOk;
}

// ── 523 —— （若当回合未击败对手）攻击结束时自身{位图}（AFTER_ACTION 桥武装）──
EffectResult effect_523_after_attack_boost(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 8) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    for (State st : {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_round_effect(ctx, st, owner, 523, /*rounds=*/1,
                              {owner, target, args.int_args[2], args.int_args[3],
                               args.int_args[4], args.int_args[5], args.int_args[6],
                               args.int_args[7]},
                              &effect_523_bitmap_boost_tick);
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表）──
SKILL_EFFECT(828, effect_828_boost_priority);
SKILL_EFFECT(1080, effect_1080_consecutive_priority);
SKILL_EFFECT(523, effect_523_after_attack_boost);
