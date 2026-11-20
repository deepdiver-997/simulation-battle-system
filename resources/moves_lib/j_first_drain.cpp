// J181 先手吸取族 —— effect 1362（1 宏 / 10 技）
//                                                                          （2026-10-07，worktree ../agents/j181）
//
// 效果模板（effect_info 1362，args_num=1）："先出手时吸取对手{0}点体力"。
//
// 载体切片（side_effect_arg 顺序切片，args_num 逐位锚定）：
//   28489 陨霆雷刑 [191,681,1362,1549] args[4,2, 300, 2] → 1362=[300]
//     （191=[4]反弹异常窗、681=[2]必爆必中窗、1549=[2]后手先制+2）
//   32309 瞳·天命难违 [1070,976,1362,476] args[1,300,300] → 1362=[300]
//     （1070 零参、976=[1]、476=[300]后手回血）
//   36493 丰岁百香祝 [1247,1362,1511] args[300,100] → **1362=[300]、1511=[100]**
//     （⚠️ 立单行误写"[100]"——1247 args_num=0 不占参，切片后移一位；1247=[无参]
//     先手禁回血、1511=[100] 先手免疫反弹——1511 未接（registry ✗，静默 no-op），
//     两处均以本记档对账。）
//
// 实现：SKILL_EFFECT 时点（先手权已定，f_order_family 判据）——
//   acting_side_is_first(ctx) 为假 → 短路（"先出手时"门）；
//   吸取 = **点数吸取，引擎无原语 → 837 同款直写**：drained=min(db[0], foe.hp)、
//   foe.hp -= drained、heal_amount(owner, drained)（按实际扣减回血，吃封回血钳 max）。
//   属性技无伤害阶段但本效果在 SKILL_EFFECT 时点照常结算（模板无攻击技限定）。
//
// 三查记录（2026-10-07）：①内核 grep 1362 无硬编码；②effect_info 模板即权威；
//   ③registry.moves 无 1362、moves_lib 无 1362——真缺口。
// 先例：f_order_family（acting_side_is_first 门）、burn_trio 837（点数吸取直写 hp 形）。
// 场景回归：test/scenario/scenario_3405_first_drain.cpp（T3405-T3407）。

#include "common.h"

namespace {

EffectResult effect_1362_first_mover_drain(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1362, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_count >= 3, 1362, args, "需要 [owner,target,amount]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int amount = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1, 1362, args,
                     "owner/target 越界");
    if (!g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    if (!acting_side_is_first(ctx)) {
        return EffectResult::kOk;   // "先出手时"门
    }
    ElfPet& foe = ctx->getPet(target);
    if (foe.hp <= 0 || amount <= 0) {
        return EffectResult::kOk;
    }
    const int drained = std::min(amount, foe.hp);
    foe.hp -= drained;
    g_core->heal_amount(ctx, owner, drained);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1362, effect_1362_first_mover_drain);
