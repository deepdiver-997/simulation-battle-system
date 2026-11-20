// K361 降转升+阈值反弹族 —— effect 819（1 宏 2 行）
//
// 行清单（tools/soul_effects_dump.py 819，2026-10-24）：
//   819：3192 天香小乔 两行 args[300,1]/[250,1]（row939 伙伴进化行，come=郎才女貌伙伴效果）：
//     「战斗阶段结束时，自身所有能力下降反转为能力上升；回合开始时若自身处于能力提升
//      状态，当回合受到的伤害大于{db[0]}则免疫并反弹该伤害（BOSS无效)」
//     db[0]=阈值（300/250）、db[1]=反弹倍数（两行均 1=全额）。
//
// 落法（双节点 + BLOCK 相位门）：
//   ① BATTLE_ROUND_END 节点（chaos_sorensen 1011 同款：BATTLE_ROUND_END=官方
//      "战斗阶段结束时"）：逐槽扫 ability_levels，负槽经 g_core->stat_change
//      （插件侧等级通道，765 回退同款；内部落 write_ability_level 唯一落点）
//      反转为正（"下降反转为上升"，幅值不变，delta=-2×lv）。
//   ② 回合门：BATTLE_ROUND_START 节点判"自身任一正等级" → plugin_storage 旗置
//      1/0（每回合无脑重写，无账本）；BLOCK 回调查旗决定当回合是否生效——
//      官方条件采样在回合开始时点，回合中变化不回看。
//   ③ BLOCK 相位门（k180 608 直系：免疫+反弹同形，K15 133 反弹同位）：
//      旗在 且 d.final > db[0] → 全字段清零（免疫）+ 按**免疫前快照** d.final×db[1]
//      对攻方 FIXED 反弹（"免疫**并**反弹该伤害"同子句绑定 → 全额快照口径，
//      区别于 608 分号独立子句"被免不反弹"）。d.final ≤ 阈值 → 不免不反。
//   ⚠️ BOSS 无效旗：tips 含"（BOSS无效)"但行参无 boss 列——PvP 引擎按 S1 §七忽略
//      （608/766 同款口径）。
//
// 三查记录（2026-10-24 本 worktree 复核）：
//   ①内核 grep 819 无魂印行为硬编码；②effect_info 819 = 技能空间模板
//   （"出手时本回合若未受到攻击伤害…"异文），魂印以 effect_icon tips 为权威；
//   ③soul_lib/registry（soul 基线）无 SOUL_PROGRAM(819)——真缺口。
//   ⚠️ 技能空间 SKILL_EFFECT(819) 已有注册（j_randpower_untouched/j_untouched_gate，
//   已备案实验遗留）——两个注册空间不相干（608 同款撞号甄别）。
// 先例：k180 608（免疫+反弹 BLOCK 门、方案 B db_at 原序行参）、k189 1016（阈值门）、
//   chaos_sorensen 1011（BATTLE_ROUND_END=战斗阶段结束时）、k12 319（反弹 deal_pink_damage）。
// 场景回归：test/scenario/scenario_7060_down_invert_reflect.cpp（7060 号段）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kGateArmKey = 990819700;   // +owner：回合门旗（1=本回合带免疫反弹门）
                                         // ⚠️ 990819000 段已被技能空间 819
                                         // （j_randpower_untouched k819SnapBase）占用——
                                         // 撞键扫描实测后换 700 段，两个注册空间各自独占。

// ── ① BATTLE_ROUND_END：能力下降反转为上升 ──────────────────────────────────
EffectResult k361_819_round_end_invert(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (!onstage_has_soul_mark(ctx, owner, 819)) {
        return EffectResult::kOk;
    }
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        const int lv = ctx->ability_levels[owner][i];
        if (lv < 0 && g_core && g_core->stat_change) {
            (void)g_core->stat_change(ctx, owner, i, -2 * lv);   // -N → +N（幅值不变）
        }
    }
    return EffectResult::kOk;
}

// ── ② BATTLE_ROUND_START：判"自身任一正等级" → 置当回合门旗 ─────────────────
EffectResult k361_819_round_start_gate(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (!onstage_has_soul_mark(ctx, owner, 819)) {
        return EffectResult::kOk;
    }
    bool boosted = false;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[owner][i] > 0) {
            boosted = true;
            break;
        }
    }
    ctx->plugin_storage[kGateArmKey + owner] = boosted ? 1 : 0;   // 每回合无脑重写
    return EffectResult::kOk;
}

// ── ③ enter(early)：挂 BLOCK 相位回调（每 epoch 一次，k180 同款幂等守卫）────
EffectResult k361_819_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ElfPet& pet = ctx->getPet(owner);
    constexpr int kEpochKey = 819 * 100 + 9;
    auto it = pet.soulmark_storage.find(kEpochKey);
    if (it == pet.soulmark_storage.end()) {
        it = pet.soulmark_storage.emplace(kEpochKey, int{-1}).first;
    }
    if (int* epoch = std::any_cast<int>(&it->second)) {
        if (*epoch == ctx->pipeline_valid_id[owner]) {
            return EffectResult::kOk;
        }
        *epoch = ctx->pipeline_valid_id[owner];
    }
    // 行参方案 B：db_at 读 owned_int_args 原序（与真实对局一致）
    int slot = a.host_slot;
    if (slot < 0 || slot > 5) {
        slot = ctx->find_pet_with_soulmark(owner, 819);
    }
    if (slot < 0 || slot > 5) {
        return EffectResult::kOk;
    }
    const auto& db = ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    const int threshold = db.size() > 0 ? db[0] : 0;
    const int reflect_mult = db.size() > 1 ? db[1] : 1;
    if (threshold <= 0 || reflect_mult <= 0) {
        return EffectResult::kOk;   // 无参不挂（K1 纪律）
    }
    ctx->register_damage_effect(
        DamagePhase::BLOCK, owner, DamageEffectCategory::BLOCK,
        [owner, threshold, reflect_mult](BattleContext* c, int bucket_owner) {
            DamageSnapshot& d = c->resolvedDamage;
            if (d.defenderId != bucket_owner || !d.isRed || d.final <= 0) {
                return;
            }
            if (bucket_owner != owner) {
                return;
            }
            if (!onstage_has_soul_mark(c, bucket_owner, 819)) {
                return;
            }
            // 回合门：本回合 ROUND_START 时带正等级才生效
            auto gate = c->plugin_storage.find(kGateArmKey + owner);
            if (gate == c->plugin_storage.end() || any_int_or(gate->second, 0) != 1) {
                return;
            }
            if (d.final <= threshold) {
                return;   // 伤害 ≤ 阈值：不免不反（严格大于）
            }
            const int incoming = d.final;   // 免疫前快照（反弹按此全额）
            d.base = 0;
            d.afterAdd = 0;
            d.afterMul = 0;
            d.final = 0;
            d.addPct = 0;
            d.mulCoef = 0.0;
            // 反弹：incoming × db[1]（两行均 1=全额），133 反弹同位（FIXED 粉）
            if (d.attackerId >= 0 && d.attackerId <= 1 && g_core
                && g_core->deal_pink_damage) {
                const int reflected = incoming * reflect_mult;
                if (reflected > 0) {
                    g_core->deal_pink_damage(c, d.attackerId, reflected,
                                             DamageKind::FIXED, bucket_owner);
                }
            }
        });
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（只增不改，三节点）────────
SOUL_PROGRAM(819, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k361_819_arm,
                             /*once=*/false, /*early=*/true),
                   SOUL_NODE(State::BATTLE_ROUND_START, &k361_819_round_start_gate),
                   SOUL_NODE(State::BATTLE_ROUND_END, &k361_819_round_end_invert)});
