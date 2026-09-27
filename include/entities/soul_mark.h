#ifndef SOUL_MARK_H
#define SOUL_MARK_H

#include <stdexcept>
#include <string>
#include <vector>

#include <effects/effect.h>
#include <entities/soul_mark_manager.h>

// 魂印 = "程序"（多时点效果序列，对等技能运行时组装的 effectBranches）。
//
// 单效果链路（旧）：effect = 一个 EffectFn，ROUND_START 注册，函数内部自行判断时机。
// 程序链路（新）：program_ = 多个 SoulMarkNodeRef{ trigger_state, effect_fn, once, early }，
//   每个节点注册到自己的时点桶（魂印容器，先于技能容器执行）。
//   两者共用底层 Effect/ContinuousEffect/桶/执行器。
class SoulMark {
public:
    SoulMark() = default;

    SoulMark(int id, std::string name, std::string description, EffectArgs args = {})
        : id(id)
        , name(std::move(name))
        , description(std::move(description))
        , args(std::move(args)) {
        SoulMarkManager& mgr = SoulMarkManager::getInstance();
        const std::vector<SoulMarkNodeRef>* program = mgr.getSoulMarkProgram(id);
        if (program && !program->empty()) {
            program_ = *program;
            has_program_ = true;
            effect = nullptr;
        } else {
            effect = mgr.getEffectFunc(id);
        }
        if (const SoulMarkHooks* hooks = mgr.getSoulMarkHooks(id)) {
            hooks_ = *hooks;
        }
    }

    int id = 0;
    std::string name;
    std::string description;
    // 程序链路：多时点节点（trigger_state/once/early 见 plugin_interface.h SoulMarkNodeRef）。
    // 注册时按节点 trigger_state 进魂印桶；once=回合限一次（触发后移除，下回合重注册）；
    // early=战斗开始立即执行一次（信号类，须在选择技能前就绪）。
    std::vector<SoulMarkNodeRef> program_;
    bool has_program_ = false;
    // 单效果链路（旧）：effect 函数指针（1001-1006 等未迁移的魂印）。
    EffectFn effect = nullptr;
    EffectArgs args;
    // 官方 effect_icon.kind 分类标签（现代魂印链路填充；老链路为空）。
    std::vector<int> kind_tags;
    // 归属精灵 id（现代魂印链路填充；0 = 未知）。
    int monster_id = 0;
    // 魂印级钩子（登场/离场额外动作）。默认全空 = 只走引擎的节点注册 + epoch 作废。
    SoulMarkHooks hooks_;

    // 是否有需要"出战背包存活即注册"的常驻节点（scope == ROSTER）。
    bool has_roster_nodes() const {
        for (const SoulMarkNodeRef& node : program_) {
            if (node.scope == SoulScope::ROSTER) {
                return true;
            }
        }
        return false;
    }
    // 注册魂印节点到对应时点桶（更新器桶在回合首时点会重调本方法刷新 once 效果）。
    // host_slot = 宿主精灵槽（战斗内不变；绑进效果 args[2]，见 bind_soulmark_args 约定）。
    // - 程序链路：按节点 trigger_state 注册；scope 决定注册条件与作废语义：
    //     STAGE  → 仅 owner_on_stage 时注册，ContinuousEffect::scope_ = ON_STAGE（离场 epoch 作废）
    //     ROSTER → 无条件注册（只要宿主存活），ContinuousEffect::scope_ = TEAM（跨切换保留）
    // - 单效果链路：effect 包成 ContinuousEffect 注册到 BATTLE_ROUND_START（恒收）。
    void register_soul_effect(BattleContext* context, int owner, bool owner_on_stage,
                              int host_slot);

    // 魂印激活（登场：战斗开始 OPERATION_ENTER_EXIT_STAGE / 切换上场 perform_switch）：
    // 注册符合作用域的节点 + 立即执行 early 节点（信号在选择前就绪）+ 调用 on_enter 钩子。
    void activate_soul_mark(BattleContext* context, int owner, bool owner_on_stage,
                            int host_slot);

    // 重触发登场行为（**只执行、不重注册**）：early 节点（scope 门照查）+ on_enter 钩子，
    // 全部以 (owner, host_slot) 为主体绑定。供"击败对手时重新触发自身登场时效果"族
    // （莫塔里安 4541）经 CoreApi::retrigger_entrance 调用——阿尔忒弥斯式副本重触发时，
    // 主体是当前效果归属 pet，天然落到她自己的登场效果（2026-09-20 官方实测口径）。
    void retrigger_entrance(BattleContext* context, int owner, int host_slot);

    // 在更新器桶登记"每回合刷新本魂印节点"的对象（回合边界执行，刷新 once 效果）。
    // 登场时调用一次即可（条目 source_id 去重，重复调用幂等）。
    void register_updater(BattleContext* context, int owner);

    // 魂印离场（切换下场 / 阵亡）：调用 on_exit 钩子做额外清理。
    // 注意：效果桶的作废由调用方的 invalidate_on_stage_effects（epoch 递增）完成，
    //       本方法只管"不在桶里"的额外状态（如薇尔诗 2513 的全局抑制标记）。
    void deactivate_soul_mark(BattleContext* context, int owner);
};

#endif // SOUL_MARK_H
