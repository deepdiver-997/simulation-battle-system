// K197 攻击附加对手体力百分比固伤族 —— effect 259（1 宏 / 1 行）
//
// 行清单（tools/soul_effects_dump.py 259，2026-10-07）：
//   259 布恩穆尼 2428 args[]（零参）：「每次攻击都附加对手当前体力值10%的固定伤害」
//
// ⚠️ 实现形态（T4 类型化行参）：DETECT 相位附粉（293 直系，epoch 防重 + ON_STAGE
//    绑定）——own 红伤落地门（d.attackerId==owner + isRed + final>0 + onstage 宿主）
//    → deal_pink_damage(对手当前体力/10, FIXED, owner)。**当前体力 = DETECT 时点
//    现值**（管线内红伤尚未扣：apply_resolved_damage 在 pipeline_.run 之后）；
//    固定伤害走 FIXED 档（吃护罩，293 同原语）。miss 无管线天然不触发。
//
// ⚠️ 三查记录（author k-prod-2 2026-10-07）：①内核无 259 魂印行为硬编码 ②effect_info
//    无 259 技能模板 ③soul_lib 并集无同 id 注册——真缺口。
// 先例：k_cond_power_family 293（DETECT 附粉全形态）、k184（EpochGuard）。
// 场景回归：test/scenario/scenario_3305_hp_pct_attach.cpp（号段 3305-3314）。

#include "common.h"

namespace {

constexpr int kMarkId = 259;

struct EpochGuard {
    int armed_epoch = -1;
};

bool already_armed_this_epoch(BattleContext* ctx, int owner) {
    ElfPet& pet = ctx->getPet(owner);
    if (!pet.soulmark_storage.count(kMarkId)) {
        pet.soulmark_storage[kMarkId] = EpochGuard{};
    }
    EpochGuard& guard = std::any_cast<EpochGuard&>(pet.soulmark_storage[kMarkId]);
    if (guard.armed_epoch == ctx->pipeline_valid_id[owner]) {
        return true;
    }
    guard.armed_epoch = ctx->pipeline_valid_id[owner];
    return false;
}

EffectResult k197_259_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || already_armed_this_epoch(ctx, a.owner)) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    ctx->register_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner](BattleContext* c, int bucket_owner) {
            if (!c || !g_core || !g_core->deal_pink_damage) {
                return;
            }
            if (bucket_owner != owner) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || !d.isRed || d.final <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            ElfPet& foe = c->getPet(c->opponent(owner));
            if (foe.hp <= 0) {
                return;
            }
            (void)g_core->deal_pink_damage(c, c->opponent(owner), foe.hp / 10,
                                           DamageKind::FIXED, owner);
        });
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（259 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k197_259_arm,
                        /*once=*/false, /*early=*/true)});
