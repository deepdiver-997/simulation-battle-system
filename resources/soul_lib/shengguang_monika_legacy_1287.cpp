#include <utils/sbs_rand.h>
// 圣光莫妮卡（未神谕 3711）魂印 1287 —— 自然祝福（精灵王线 2026-09-25）
//
// 官方全文（effect_icon 963 / effect 1287，boss 有效）：
//   首次登场时获得4层自然祝福，之后每次登场时获得2层自然祝福；战斗阶段结束时，
//   若当回合自身使用技能则消耗1层自然祝福，若当回合自身未使用技能则获得1层自然祝福
//   （自然祝福下场后清零，最多可获得4层，boss有效）
//   | 自然祝福：25%概率免疫受到的异常状态、攻击技能先制+1、受到的攻击伤害减少8%
//
// 资源 = kMoniPubNz（**未神谕旧资源**，cap 4）——与神谕版宁界之律（kMoniPubLv）互不相通
//（用户 2026-09-25 口径：旧自然祝福不能和新的律混用）。神谕开关 OFF 时工厂选本行
//（K1 effect_icon 低版本行），SOUL_PROGRAM(1287) 随 soulMark.id 自动生效。
#include "common.h"
#include "../monika_runtime.h"

namespace {

constexpr int kMarkLegacy = 1287;
constexpr int kLegacyNzCap = 4;             // "最多可获得4层"
constexpr int kLegacyImmuneSrcId = 12871;   // 25% 免控固定句柄（ROUND_START 重授刷新）

inline bool onstage_1287(BattleContext* ctx, int owner) {
    return ctx->on_stage[owner] >= 0 && ctx->on_stage[owner] < 6
        && ctx->getPet(owner).soulMark.id == kMarkLegacy;
}

inline int nz_get(BattleContext* ctx, int owner) {
    return moni_pub_get(ctx->plugin_storage, kMoniPubNz + owner);
}

// ── 本地助手（moves_lib/common.h 的同名逻辑；soul 侧不共享那个头，就地复刻，同
//    crawford_deleermo_4644 的做法）──
bool chose_skill_this_round(const BattleContext* ctx, int side) {
    return ctx && side >= 0 && side <= 1 && ctx->roundChoice[side][0] == 1;
}

bool selected_skill_is_attack(BattleContext* ctx, int side) {
    if (!chose_skill_this_round(ctx, side)) {
        return false;
    }
    const int idx = ctx->roundChoice[side][1];
    if (idx < 0 || idx >= static_cast<int>(ctx->getPet(side).skills.size())) {
        return false;
    }
    return ctx->getPet(side).skills[static_cast<std::size_t>(idx)].type
        != SkillType::Attribute;
}

// 登场（战斗开始 + 每次切换上场）：清零重计 +4/+2（钩子由 activate_soul_mark 显式调用）。
void monika_legacy_on_enter(BattleContext* ctx, int owner) {
    if (!ctx || owner < 0 || owner > 1 || !onstage_1287(ctx, owner)) {
        return;
    }
    auto& oss = ctx->getPet(owner).on_stage_storage;
    if (oss.count(kMarkLegacy)) {
        return;   // 每次上场（on_stage_storage 生命周期）只执行一次
    }
    oss[kMarkLegacy] = true;
    // "自然祝福下场后清零"：以再登场时清零重计表达（离场期间无人读它，观察等价）。
    // "首次/之后"：soulmark_storage 跨上场期持久（2432 记账同槽）。
    ElfPet& me = ctx->getPet(owner);
    const bool first = !me.soulmark_storage.count(2492101);
    me.soulmark_storage[2492101] = 1;
    const int gain = first ? 4 : 2;
    moni_pub_set(ctx->plugin_storage, kMoniPubNz + owner,
                 gain > kLegacyNzCap ? kLegacyNzCap : gain);
}

EffectResult monika_legacy_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_1287(ctx, owner)) {
        return EffectResult::kOk;
    }
    const int nz = nz_get(ctx, owner);
    if (nz <= 0) {
        return EffectResult::kOk;
    }
    // ① 25% 概率免疫受到的异常状态：回合窗口票（固定句柄重授），掷骰在 condition
    //    现场做（每次施加掷一次；"自然祝福"=层数>0 即可，不按层数叠概率——与神谕版
    //    "每层25%"口径不同，见 tips 第二段字面）。古早施加只查 Ancient 层，天然不响应。
    ctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(),
                        /*anomaly_mask=*/0, /*duration_rounds=*/1, kLegacyImmuneSrcId,
                        /*soul_immunity=*/false, EffectScope::ON_STAGE,
                        /*counts=*/0, /*source_effect_id=*/1287, ImmunityTier::Modern,
                        [](const BattleContext*, int) {
                            return (sbs_rand() % 100) < 25;
                        });
    // ② 受到的攻击伤害减少 8%（MITIGATE 加算减伤槽；ws 每回合 reset → 此处重写；
    //    混沌魔君索伦森 1011 同款"受到的攻击伤害减少"通道）。
    int(&slots)[4] = ctx->ws.damage_reduce_add[owner];
    for (int i = 0; i < 4; ++i) {
        if (slots[i] == 0) {
            slots[i] = 8;
            break;
        }
    }
    return EffectResult::kOk;
}

// 攻击技能先制 +1（有祝福时）：先手权结算时点判所选技能类别（481 同款攻击限定）。
EffectResult monika_legacy_move_right(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_1287(ctx, owner)) {
        return EffectResult::kOk;
    }
    if (nz_get(ctx, owner) > 0 && selected_skill_is_attack(ctx, owner)) {
        ctx->ws.preemptive_level[owner] += 1;
    }
    return EffectResult::kOk;
}

// 战斗阶段结束：用技 → -1；未用 → +1（cap 4）。"使用技能"口径同 2492：
// 选了技且流程真正走到技能结算（skill_pp_cost_consumed 落账）。
EffectResult monika_legacy_round_end(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_1287(ctx, owner)) {
        return EffectResult::kOk;
    }
    const bool selected = ctx->roundChoice[owner][0] == 1 /*SELECT_SKILL*/;
    const bool used = selected && ctx->ws.skill_pp_cost_consumed[owner];
    int nz = nz_get(ctx, owner);
    nz = used ? (nz > 0 ? nz - 1 : 0)
              : (nz < kLegacyNzCap ? nz + 1 : kLegacyNzCap);
    moni_pub_set(ctx->plugin_storage, kMoniPubNz + owner, nz);
    return EffectResult::kOk;
}

}  // namespace

SOUL_PROGRAM(1287, {SoulMarkNodeRef(State::BATTLE_ROUND_START, &monika_legacy_round_start),
                    SoulMarkNodeRef(State::BATTLE_FIRST_MOVE_RIGHT, &monika_legacy_move_right),
                    SoulMarkNodeRef(State::BATTLE_ROUND_END, &monika_legacy_round_end)});
SOUL_MARK_HOOKS(1287, &monika_legacy_on_enter, nullptr);
