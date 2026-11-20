// 战前快照族（极战·阿尔斯兰 4327：战印首句"攻击高于对手 → 所有技能先制+1"）
//
// 官方原文（4399 图鉴 954832）："战：自身攻击值高于对手时自身所有技能额外先制+1；
// 回合结束时若自身处于异常状态则激发战神之魂：……每次回合结束时攻击伤害提升20%……
// 伤害低于300时额外附加攻击值30%真实伤害……"
// 本文件只接线**首句的先制子句**；战神之魂 / 回合伤害+20%累计 / <300 补真伤待做（台账）。
//
// 语义拍板（用户 2026-09-19）：这是历史遗留的"比游戏前面板"族——
// · 对比在**战前快照段**（PreBattlePhase::PANEL_SNAPSHOT）一次性完成：拿未被魂印
//   修改的原始面板，与对面 6 槽全部精灵的原始攻击值比；结果（布尔）存
//   `pet.soulmark_storage`（下场保留），整场不再重比——对局中双方面板怎么被改
//   （减半/强化/弱化）都不影响判定。
// · 运行时消费：MOVE_RIGHT 桶节点（先制 memset 之后跑，1263 同款）按存储的布尔
//   `preemptive_level[owner] += 1`——"所有技能"无条件，无攻击门。
#include "common.h"

namespace {

// ⚠️ 注册键 = effect_icon.effect_id（装配层 SoulMark.id 按 effect_id 查程序表）。
//    4327 是 monster id，只用于命名与注释——用它注册程序在真实对局永远挂不上
//    （2026-09-27 修复；effect_id 取自 unity 库 effect_icon 1553 行 [4327]→1912）。
constexpr int kAlslanMarkId = 1912;

// 快照段：一次性判定"自身攻击值高于对手（对面全部精灵）"，结果落 pet.soulmark_storage。
void alslan_4327_panel_snapshot(BattleContext* ctx, int side, int slot) {
    if (!ctx || side < 0 || side > 1 || slot < 0 || slot >= 6) {
        return;
    }
    ElfPet& self = ctx->seerRobot[side].elfPets[slot];
    const int my_atk = self.numericalProperties[NumericalPropertyIndex::PHYSICAL_ATTACK];
    bool beats_all = true;
    for (int opp_slot = 0; opp_slot < 6; ++opp_slot) {
        const ElfPet& opp = ctx->seerRobot[1 - side].elfPets[opp_slot];
        if (opp.numericalProperties[NumericalPropertyIndex::PHYSICAL_ATTACK] >= my_atk) {
            beats_all = false;  // "高于"= 严格大于：打平不算
            break;
        }
    }
    self.soulmark_storage[kAlslanMarkId] = beats_all;
}

// MOVE_RIGHT 桶（STAGE）：读快照结果 → 先制+1（在场才有先制可言）。
EffectResult alslan_4327_move_right(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ElfPet& host = ctx->getPet(owner);
    const auto it = host.soulmark_storage.find(kAlslanMarkId);
    if (it == host.soulmark_storage.end()) {
        return EffectResult::kOk;
    }
    if (const bool* beats_all = std::any_cast<bool>(&it->second); beats_all && *beats_all) {
        ctx->ws.preemptive_level[owner] += 1;
    }
    return EffectResult::kOk;
}

}  // namespace

SOUL_PRE_BATTLE(kAlslanMarkId, PreBattlePhase::PANEL_SNAPSHOT, alslan_4327_panel_snapshot);
SOUL_PROGRAM(kAlslanMarkId,
             {SoulMarkNodeRef(State::BATTLE_FIRST_MOVE_RIGHT, &alslan_4327_move_right,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE)});
