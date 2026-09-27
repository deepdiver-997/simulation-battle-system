#ifndef SUIT_H
#define SUIT_H

#include <string>
#include <vector>

#include <entities/suit_manager.h>
#include <fsm/battleContext.h>

// 套装 = "程序"（多时点效果序列），模型对等 SoulMark（include/entities/soul_mark.h）：
//   程序链路：SuitManager 里注册的 SuitNodeRef（复用 SoulMarkNodeRef）序列，
//     每节点注册到自己的时点桶（**套装桶**，执行序魂印 → 套装 → 技能）。
//   单效果链路：EffectFn 回退（ROUND_START 注册，函数内自行判断时机）。
//
// 与魂印的本质区别在**生命周期**（用户 2026-09-19 口径）：
//   生效范围**全队**（装备穿在赛尔身上，官方 desc 口径"背包内精灵…"），
//   **无法被任何效果抹除或者屏蔽** —— 节点一律展开成 TEAM 常驻条目：
//   不随换宠作废（epoch 体系碰不到 TEAM）、不被断回合（非回合类效果）、
//   不吃 damage_suppress_mask（注册类别由插件自己选非抑制敏感的）。
//   scope_ 字段不消费 SoulScope——套装没有"登场/离场"语义，恒 TEAM。
class Suit {
public:
    Suit() = default;

    Suit(int suit_id, std::string suit_name, std::string suit_description)
        : id(suit_id)
        , name(std::move(suit_name))
        , description(std::move(suit_description)) {
        SuitManager& mgr = SuitManager::getInstance();
        if (const std::vector<SoulMarkNodeRef>* program = mgr.getSuitProgram(id)) {
            program_ = *program;
            has_program_ = true;
            effect = nullptr;
        } else {
            effect = mgr.getSuitFunc(id);
        }
    }

    int id = 0;
    std::string name;
    std::string description;
    // 程序链路：多时点节点（trigger_state/once/early 复用 SoulMarkNodeRef 语义；
    // early 对套装 = 激活时立即执行一次——activate_suits 在战斗开始前跑，等价就绪）。
    std::vector<SoulMarkNodeRef> program_;
    bool has_program_ = false;
    // 单效果链路（旧/简）：EffectFn。
    EffectFn effect = nullptr;

    // 把套装效果链注册进 owner 方套装桶（activate_suits 对双方各调一次）。
    // 幂等：桶 key = (source_id << 32) | effect_id，同源重复注册覆盖不追加。
    // early 节点语义对齐 SoulMark::activate_soul_mark：注册之外**立即执行一次**
    //（信号类，须在选择前就绪——圣芒佑界 476 的 install 节点靠它把异常持续上限/
    // 监听器提前到 init_battle，覆盖"战斗开始时点的魂印早于套装施加异常"的时序）。
    // 安装类节点自带 plugin_storage 幂等守卫，ROUND_START 桶里再次执行无害。
    void register_suit_effect(BattleContext* context, int owner) const {
        if (!context || owner < 0 || owner > 1) {
            return;
        }
        if (has_program_) {
            for (std::size_t node_idx = 0; node_idx < program_.size(); ++node_idx) {
                const SoulMarkNodeRef& node = program_[node_idx];
                if (!node.effect_fn) {
                    continue;
                }
                // ⚠️ 逐节点独立 effect_id（同魂印 register_soul_effect 的 id*100+n 约定）：
                //    同 State 多节点各占一条，重复激活命中同一 key（幂等）。
                const int node_effect_id = id * 100 + static_cast<int>(node_idx);
                Effect wrapper(node_effect_id, 0, owner, /*left_round=*/-1,
                               bind_suit_args(owner), node.effect_fn);
                auto ce = std::make_unique<ContinuousEffect>(
                    wrapper, node.trigger_state, owner, /*duration=*/-1, context->roundCount
                );
                ce->source_id_ = id;
                ce->scope_ = EffectScope::TEAM;   // 全队、整场、不可抹除（类注释）
                ce->once_ = node.once;
                context->register_suit_effect(node.trigger_state, owner, std::move(ce));
            }
            for (const SoulMarkNodeRef& node : program_) {
                if (node.early && node.effect_fn) {
                    node.effect_fn(context, bind_suit_args(owner));
                }
            }
            return;
        }
        if (!effect) {
            return;
        }
        Effect wrapper(id, 0, owner, /*left_round=*/-1, bind_suit_args(owner), effect);
        auto ce = std::make_unique<ContinuousEffect>(
            wrapper, State::BATTLE_ROUND_START, owner, /*duration=*/-1, context->roundCount
        );
        ce->source_id_ = id;
        ce->scope_ = EffectScope::TEAM;
        context->register_suit_effect(State::BATTLE_ROUND_START, owner, std::move(ce));
    }

private:
    // 套装效果参数绑定：args[0]=owner，args[1]=1-owner（与魂印 bind_soulmark_args 同约定）。
    static EffectArgs bind_suit_args(int owner) {
        return EffectArgs(std::vector<int>{owner, 1 - owner});
    }
};

#endif  // SUIT_H
