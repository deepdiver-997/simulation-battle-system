// 战前修改族（卫岳 4510："防御、特防值等于攻击、特攻值中较低的一项"）
//
// 官方原文（4399 图鉴 981682）："岳：……自身在场期间防御、特防值等于攻击、特攻值中
// 较低的一项，且令对手所有体力恢复效果减少50%；……"
// 本文件只接线**双防改写**；穹明号（拥君之盾/绝缘之罩）、回复减少50%、解放流程待做（台账）。
//
// 语义（用户 2026-09-19 拍板）：按"战斗开始时的面板修改"落笔——修改段（STAT_MODIFY）
// 执行，读到的是先执行补丁改过的面板（"比游戏后面板"族）；落笔后面板整场保持
// （后续双攻变化不回写双防——若游戏实测要求联动，把本补丁改成常驻视图即可）。
// 面板是 pet 事实：场下首发同样落笔（进场的双防也已是改写值）。
#include "common.h"

namespace {

void weiyue_4510_stat_modify(BattleContext* ctx, int side, int slot) {
    if (!ctx || side < 0 || side > 1 || slot < 0 || slot >= 6) {
        return;
    }
    ElfPet& pet = ctx->seerRobot[side].elfPets[slot];
    const int atk = pet.numericalProperties[NumericalPropertyIndex::PHYSICAL_ATTACK];
    const int spatk = pet.numericalProperties[NumericalPropertyIndex::SPECIAL_ATTACK];
    const int lower = std::min(atk, spatk);
    pet.numericalProperties[NumericalPropertyIndex::DEFENSE] = lower;
    pet.numericalProperties[NumericalPropertyIndex::SPECIAL_DEFENSE] = lower;
}

}  // namespace

// ⚠️ 注册键 = effect_icon.effect_id = 2107（装配层 SoulMark.id 按 effect_id 查程序表；
//    4510 是 monster id，只用于命名——错键会让补丁在真实对局挂不上，2026-09-27 修复）。
SOUL_PRE_BATTLE(2107, PreBattlePhase::STAT_MODIFY, weiyue_4510_stat_modify);
