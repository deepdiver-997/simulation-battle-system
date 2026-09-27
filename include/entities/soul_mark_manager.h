#ifndef SOUL_MARK_MANAGER_H
#define SOUL_MARK_MANAGER_H

#include <unordered_map>
#include <shared_mutex>
#include <string>
#include <vector>
#include <memory>

#include <effects/effect.h>
#include <plugin/plugin_interface.h>
#include <utils/dynamic_library.h>

// 魂印管理器 - 从动态库加载魂印效果函数
class SoulMarkManager : public IEffectRegistry {
public:
    // 获取单例实例（线程安全）
    // 首次调用时必须传入动态库所在目录路径
    // lib_dir: 动态库目录路径，目录中应包含 .so/.dylib/.dll 文件
    static SoulMarkManager& getInstance(const std::string& lib_dir = "");

    // 根据魂印ID获取效果函数指针（线程安全，带缓存）
    EffectFn getEffectFunc(int soulmarkId);

    // IEffectRegistry interface - for plugin use
    void registerSoulMark(int soulmark_id, EffectFn effect_fn) override;
    void registerSoulMarkProgram(int soulmark_id,
                                 const std::vector<SoulMarkNodeRef>& nodes) override;
    // 魂印级钩子（登场/离场额外动作；见 plugin_interface.h SoulMarkHooks）。
    void registerSoulMarkHooks(int soulmark_id, const SoulMarkHooks& hooks) override;
    void registerSkillEffect(int effect_id, EffectFn effect_fn) override;
    void registerSoulMarks(
        const std::vector<std::pair<int, EffectFn>>& soulmarks) override;
    void registerSkillEffects(
        const std::vector<std::pair<int, EffectFn>>& effects) override;

    // 单元准入门（IEffectRegistry）：soul_lib 走本注册表；门是进程全局（effect_unit 存）。
    void registerUnitAdmission(UnitAdmissionFn fn) override;

    // 战前魂印补丁（IEffectRegistry）：soul_lib 走本注册表（soul_plugin 的 flush 落在
    // 本管理器而非 EffectFactory——与魂印程序/钩子同一条通道）。见 PreBattlePhase 注释。
    void registerSoulMarkPreBattle(int soulmark_id, PreBattlePhase phase,
                                   PreBattlePatchFn fn) override;

    // 携带类效果（IEffectRegistry）：转发到 EffectFactory——loadSkills 装槽只查
    // EffectFactory，而 soul_plugin 的 flush 落本管理器；不转发的话插件侧
    // CARRY_EFFECT 会被默认空实现静默吞掉。
    void registerCarryEffect(int effect_id, CarryEffectFn fn) override;

    // 战前补丁扫描执行（init_battle 调；语义同 EffectFactory::run_pre_battle_patches，
    // 只是注册表换成本管理器的）。见 PreBattlePhase 注释。
    void run_pre_battle_patches(BattleContext* ctx, PreBattlePhase phase);

    // 手动注册效果（不通过动态库）
    void registerEffect(int soulmarkId, EffectFn effect);

    // 查询魂印程序（多时点节点）。未注册程序返回 nullptr（调用方回退单效果链路）。
    const std::vector<SoulMarkNodeRef>* getSoulMarkProgram(int soulmarkId) const;

    // 查询魂印钩子。未注册返回 nullptr（调用方视作无额外动作）。
    const SoulMarkHooks* getSoulMarkHooks(int soulmarkId) const;

    // 检查是否已初始化
    bool isInitialized() const { return initialized_; }

    // 获取已加载的动态库数量（调试用）
    size_t getLoadedLibraryCount() const;

    // 已注册魂印 id 全集（单效果 ∪ 程序 ∪ 钩子），升序去重。覆盖率对账用。
    std::vector<int> registered_soulmark_ids() const;

private:
    SoulMarkManager() = default;
    ~SoulMarkManager() = default;

    // 从动态库加载所有魂印效果
    void loadFromDynamicLibraries(const std::string& lib_dir);

    // 初始化单例（只执行一次）
    void ensureInitialized(const std::string& lib_dir);

    static EffectResult noop_effect(BattleContext* context, const EffectArgs& args) {
        (void)context;
        (void)args;
        return EffectResult::kOk;
    }

    // 缓存映射表：魂印ID -> 效果函数
    std::unordered_map<int, EffectFn> effect_cache_;
    // 魂印程序缓存：魂印ID -> 多时点节点（程序模型；单效果链路不在此表）。
    std::unordered_map<int, std::vector<SoulMarkNodeRef>> program_cache_;
    // 魂印钩子缓存：魂印ID -> 登场/离场钩子（只在插件显式注册时存在）。
    std::unordered_map<int, SoulMarkHooks> hooks_cache_;
    // 战前魂印补丁（魂印ID+时段 -> fn；保注册序，见 PreBattlePhase 注释）。
    std::vector<PreBattlePatchEntry> pre_battle_patches_;
    mutable std::shared_mutex cache_mutex_;

    // 加载的动态库（保持加载状态，防止函数指针失效）
    std::vector<DynamicLibrary> loaded_libraries_;

    // 初始化标志
    bool initialized_ = false;

    // 禁止拷贝
    SoulMarkManager(const SoulMarkManager&) = delete;
    SoulMarkManager& operator=(const SoulMarkManager&) = delete;
};

#endif // SOUL_MARK_MANAGER_H