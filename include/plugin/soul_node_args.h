#ifndef SOUL_NODE_ARGS_H
#define SOUL_NODE_ARGS_H

// 类型化魂印行参（T4，2026-10-05 决策清单 K5 用户裁定）。
//
// 旧行参协议：bind_soulmark_args 丢弃行参前两位、合并 [own,opp,slot]+行参[2:]，
// 实现侧从绑定后 args[3:] 读——索引错位是运行期事故（K165 实测：{3,100} 直接传丢参
// 走 fallback）。类型化后节点函数签名用 SoulNodeFn，行参一律 a.row_or(i, fallback)
// （row(0)=官方行第 1 位），错位在编译期就写不出来。
//
// 为什么独立成头：shim 需要完整 EffectArgs（解 int_args/int_count），而
// effect.h 反向包含 plugin_interface.h——类型化面只能放在 effect.h 之下。
// 引入方式：resources/soul_lib/common.h 统一 #include，插件无需自己引。
//
// 用法：
//   EffectResult k165_303_arm(BattleContext*, const SoulNodeArgs& a);   // 行参 a.row_or(0, 2)
//   SOUL_PROGRAM(303, {soul_node(State::OPERATION_ENTER_EXIT_STAGE, &k165_303_arm,
//                      /*once=*/false, /*early=*/true)});
//   SOUL_MARK_TYPED(1004, soulmark_canglan_apply_eternal_tears);

#include <cstddef>
#include <vector>

#include <effects/effect.h>          // EffectArgs 全型（EffectFn 同款签名面）
#include <fsm/state.h>
#include <plugin/plugin_interface.h> // SoulMarkNodeRef / SoulScope / plugin_reg

struct SoulNodeArgs {
    int owner = -1;        // 绑定后 args[0]（bind 恒填，见 soul_mark_manager.cpp:351）
    int opp = -1;          // args[1]
    int host_slot = -1;    // args[2]
    std::vector<int> row;  // 官方行参原序（绑定前行参[2:]）

    // 官方行第 i+1 位；缺失返回 fallback。
    int row_or(size_t i, int fallback) const {
        return i < row.size() ? row[i] : fallback;
    }
    bool has_row(size_t i) const { return i < row.size(); }
    size_t row_count() const { return row.size(); }
};

using SoulNodeFn = EffectResult (*)(BattleContext*, const SoulNodeArgs&);

// SoulNodeFn → EffectFn 适配（函数指针作模板实参：注册点即绑定，无运行期分发）。
// shim 在注册边界把行参拷进自有向量（每次几个 int），bind 协议保持不变。
// ⚠️ **a.row 是偏移 2 位的坑位口径**（2026-10-21 方案 B 定论）：bind_soulmark_args
//    丢弃 owned_int_args 前两位 → a.row = DB args[2:]，真实对局常为空/错位。
//    **新代码禁止读 a.row**——行参一律 db_at 读 owned_int_args 原序（套路目录 §一）；
//    仅存的合法用户是两个手写魂印 id 豁免文件（legacy_guards / canglan_tears，
//    DB 无对应行，见《魂印行参读取路径缺陷-2026-10-21》§豁免）。
template <SoulNodeFn Fn>
EffectResult soul_node_shim(BattleContext* ctx, const EffectArgs& bound) {
    SoulNodeArgs a;
    if (bound.int_args != nullptr && bound.int_count >= 3) {
        a.owner = bound.int_args[0];
        a.opp = bound.int_args[1];
        a.host_slot = bound.int_args[2];
        a.row.assign(bound.int_args + 3, bound.int_args + bound.int_count);
    }
    return Fn(ctx, a);
}

// 类型化节点构造（给 SOUL_PROGRAM 节点列表用）。⚠️ 必须是宏：fn 要作模板实参在
// 编译期绑定 shim，运行期函数指针做不到（EffectFn 是裸函数指针，带不动闭包）。
//   SOUL_PROGRAM(303, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k165_303_arm,
//                      /*once=*/false, /*early=*/true), ...});
#define SOUL_NODE(trigger, fn, ...)                                                        \
    SoulMarkNodeRef(trigger, &::soul_node_shim<fn>, ##__VA_ARGS__)

// 类型化单效果魂印（给 SOUL_MARK 用）。
#define SOUL_MARK_TYPED(id, fn)                                                            \
    namespace {                                                                            \
    EffectResult soul_mark_typed_shim_##fn(BattleContext* ctx, const EffectArgs& bound) {  \
        return ::soul_node_shim<fn>(ctx, bound);                                           \
    }                                                                                      \
    [[maybe_unused]] const bool kSoulMarkTypedReg_##fn =                                   \
        ::plugin_reg::add_soul_mark((id), &soul_mark_typed_shim_##fn);                     \
    }

#endif  // SOUL_NODE_ARGS_H
