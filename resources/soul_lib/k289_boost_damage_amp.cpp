// K289 —— effect 659 唯一注册（原 K283 重复注册已移除）。
// 穆恩斯达 2930，DB args 原序 [2,20]：
// 「自身处于能力提升状态,则自身伤害提升20%」。只消费 db[1]，db[0]=2 不猜。
//
// 能力提升是持续态：每笔攻击伤害在 GUARD_DETECT 现场读任一 ability_levels > 0，
// 临时加到 ws.damage_add_pct，DETECT 无条件撤回本笔写入量（K410/1764 同形）。
// 因此与其他通用增伤加算；不取回合首/用技快照，也不因重复出手累积成 40%/60%。
// install_default_damage_amp 只在 DamagePipeline::AMP 消费该槽；独立粉伤管线不读它，
// 属性直伤也不跑 AMP。这里沿用真实既有通道，不能按旧头注声称粉伤/真伤同样增幅。
//
// 接线用 ON_STAGE（不是 register_default_damage_effect 的 TEAM），pipeline_valid_id
// 防 early 重触发，下场自动失效。私有 990289000 段仅保存接线 epoch 与同笔临时量，
// 存在 pet.on_stage_storage，下场清；双方/同方不同宿主不共享。
// DETECT 的撤回不查 final/提升门：BLOCK 归零、途中清强化也必须配平；零伤/技能无效
// 未写则不撤。管线会遍历全部相位，即使伤害已为 0，故不留跨回合 reset 的残账。
// 场景：4390（真实技能伤害/参数/连击）、4450（实时门/重复结算/生命周期/类型边界）。

#include "common.h"

namespace {

constexpr int kMarkId = 659;
constexpr int kAmpPctFallback = 20;
constexpr int k659RuntimeKey = 990289000;

struct AmpRuntime {
    int armed_epoch = -1;
    int written = 0;  // 仅 GUARD_DETECT -> DETECT 间非零，绝非回合常驻增伤
};

AmpRuntime* runtime(BattleContext* ctx, int owner) {
    auto& storage = ctx->getPet(owner).on_stage_storage;
    const auto it = storage.find(k659RuntimeKey);
    return it == storage.end() ? nullptr : std::any_cast<AmpRuntime>(&it->second);
}

int live_amp_pct(BattleContext* ctx, int owner) {
    const auto& db = ctx->getPet(owner).soulMark.args.owned_int_args;
    // 在位即消费，包括显式 0；不读会丢掉前两项的 a.row。
    return db.size() > 1 ? db[1] : kAmpPctFallback;
}

bool self_has_boost(const BattleContext* ctx, int owner) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[owner][i] > 0) {
            return true;
        }
    }
    return false;
}

EffectResult k289_659_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (ctx->on_stage[owner] < 0 || ctx->on_stage[owner] > 5
        || a.host_slot != ctx->on_stage[owner]
        || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    auto& storage = ctx->getPet(owner).on_stage_storage;
    const auto [it, inserted] = storage.try_emplace(k659RuntimeKey, AmpRuntime{});
    (void)inserted;
    auto* state = std::any_cast<AmpRuntime>(&it->second);
    if (!state || state->armed_epoch == ctx->pipeline_valid_id[owner]) {
        return EffectResult::kOk;  // 类型不符按缺失/失效处理，不抛 any_cast
    }
    *state = AmpRuntime{ctx->pipeline_valid_id[owner], 0};

    ctx->register_damage_effect(
        DamagePhase::GUARD_DETECT, owner, DamageEffectCategory::AMP,
        [owner](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner) {
                return;
            }
            const DamageSnapshot& damage = c->resolvedDamage;
            if (damage.attackerId != owner || !damage.isRed || damage.final <= 0
                || !onstage_has_soul_mark(c, owner, kMarkId)
                || !self_has_boost(c, owner)) {
                return;
            }
            auto* state = runtime(c, owner);
            const int pct = live_amp_pct(c, owner);
            if (!state || pct <= 0) {
                return;
            }
            c->ws.damage_add_pct[owner] += static_cast<float>(pct);
            state->written = pct;
        }, EffectScope::ON_STAGE);
    ctx->register_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::AMP,
        [owner](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner || c->resolvedDamage.attackerId != owner) {
                return;
            }
            auto* state = runtime(c, owner);
            if (state && state->written != 0) {
                c->ws.damage_add_pct[owner] -= static_cast<float>(state->written);
                state->written = 0;
            }
        }, EffectScope::ON_STAGE);
    return EffectResult::kOk;
}

}  // namespace

SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k289_659_arm,
                        /*once=*/false, /*early=*/true, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
