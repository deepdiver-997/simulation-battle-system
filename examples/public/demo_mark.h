// examples/public/demo_mark.h —— 教学用示例魂印（**非官方机制**，可直接链接）
//
// 这是本包唯一的"效果实现"，刻意做到最简、直观、**不含任何官方机制的逆向判断**：
//   「登场时给自己加 10 点护盾」——护盾是内核的一等资源（ShieldBank），
//   加法在公开 API 里是现成的。
//
// 它存在的意义是演示**效果程序的形状**：
//   1. 效果函数签名固定为 EffectResult(BattleContext*, const SoulNodeArgs&)；
//   2. 用 SOUL_NODE 声明节点（时点 + 效果函数 + once + early + 作用域）；
//   3. 走公开的内核 API（护盾资源 / CoreApi 槽），不直接改引擎内部状态；
//   4. 注册用 `SoulMarkManager::registerSoulMarkProgram`（公开 API）——
//      **不依赖插件动态库**，所以免数据库的公开检出也能跑（见 demo_scenario.cpp）。
//
// 想实现官方机制时，请参考本仓内核文档（docs/02-效果系统/）与官方效果文本，
// 自行按族实现——**本仓的官方效果实现层暂未公开**（见 README「开放路线」）。
#ifndef EXAMPLES_PUBLIC_DEMO_MARK_H
#define EXAMPLES_PUBLIC_DEMO_MARK_H

#include <plugin/soul_node_args.h>

#include <effects/effect.h>
#include <entities/shield_bank.h>
#include <entities/soul_mark_manager.h>
#include <fsm/battleContext.h>
#include <iostream>

namespace examples {

// 示例魂印 id：官方 effect_id 空间在 1~3000 段，示例用 9xxxx 段避免撞号。
constexpr int kDemoMarkId = 90001;
constexpr int kDemoShield = 10;

inline EffectResult demo_mark_enter(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    // 直接操作承伤方的护盾资源（公开 API：ElfPet::shield_bank_）。
    // 真实插件更推荐走 CoreApi 槽（grant_shield）——它带护盾优先级与来源 id，
    // 便于后续被破盾/查询时归因。
    ctx->getPet(a.owner).shield_bank_.add(
        ShieldBank::kSoulShieldPriority, kDemoShield,
        /*source_id=*/kDemoMarkId, /*duration=*/0);
    return EffectResult::kOk;
}

// 注册一次即可（重复注册幂等：同 id 覆盖同内容）。
// ⚠️ 节点的效果函数走 **SOUL_NODE 宏的模板 shim**——它把引擎的 EffectArgs
//    （绑定后 args[0..2]=owner/opp/host_slot）适配成 SoulNodeArgs，
//    免去手写 (BattleContext*, const EffectArgs&) 的拆包样板。
//
// ⚠️ **early 标志别乱用**：early 节点会在「登场激活」时**立即执行一次**、同时又被
//    注册进该时点的桶再执行一次 → 同登场加两次效果（本示例初版护盾加了 20 不是 10）。
//    语义 = "信号类效果，必须在选择技能之前就位"（如封技/必中旗标）。
//    「登场时做一次」用 **once=true + 非 early** 即可（桶里只跑一次）。
inline void register_demo_mark() {
    SoulMarkManager::getInstance().registerSoulMarkProgram(
        kDemoMarkId,
        {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &demo_mark_enter,
                   /*once=*/true, /*early=*/false, SoulScope::STAGE)});
}

}  // namespace examples

#endif  // EXAMPLES_PUBLIC_DEMO_MARK_H
