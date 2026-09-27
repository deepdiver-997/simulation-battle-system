#ifndef SUIT_MANAGER_H
#define SUIT_MANAGER_H

#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <effects/effect.h>
#include <plugin/plugin_interface.h>
#include <plugin/suit_reg.h>
#include <utils/dynamic_library.h>

// 套装管理器 —— 从动态库（resources/suit_lib）加载套装效果（2026-09-19 套装线）。
//
// 与 SoulMarkManager 同构（单例 + 目录扫描 dlopen + 注册表缓存），但**不实现
// IEffectRegistry**：套装走 suit_reg.h 的独立 API（SuitPluginApi），入口符号
// suit_plugin_register。独立的原因见 suit_reg.h 头注释。
//
// 套装效果数据面：程序（多时点节点，优先）或单效果函数（回退）。
// 查询方：Suit（include/entities/suit.h，构造时取程序）与
//         BattleContext::activate_suits（注册进套装桶）。
class SuitManager {
public:
    // 单例。首次调用传动态库目录（PetFactory::initialize_runtime_data 传
    // "resources/suit_lib"）；目录不存在/为空 = 零注册，不报错（场景测试可
    // 手动 registerSuitProgram 注入测试套装）。
    static SuitManager& getInstance(const std::string& lib_dir = "");

    // 注册（启动期 / 测试手动注入）。同 id 重复注册后到者为准。
    void registerSuit(int suit_id, EffectFn fn);
    void registerSuitProgram(int suit_id, const std::vector<SoulMarkNodeRef>& nodes);

    // 查询。程序未注册返回 nullptr（调用方回退单效果链路）；函数未注册返回 noop。
    const std::vector<SoulMarkNodeRef>* getSuitProgram(int suit_id) const;
    EffectFn getSuitFunc(int suit_id) const;

    bool isInitialized() const { return initialized_; }
    size_t getLoadedLibraryCount() const { return loaded_libraries_.size(); }

    // 已注册套装 id 全集（单效果 ∪ 程序），升序去重。覆盖率对账用。
    std::vector<int> registered_suit_ids() const;

private:
    SuitManager() = default;
    ~SuitManager() = default;

    void ensureInitialized(const std::string& lib_dir);
    void loadFromDynamicLibraries(const std::string& lib_dir);

    static EffectResult noop_effect(BattleContext* context, const EffectArgs& args) {
        (void)context;
        (void)args;
        return EffectResult::kOk;
    }

    std::unordered_map<int, EffectFn> effect_cache_;
    std::unordered_map<int, std::vector<SoulMarkNodeRef>> program_cache_;
    mutable std::shared_mutex cache_mutex_;

    // 保持加载状态（防函数指针失效）。
    std::vector<DynamicLibrary> loaded_libraries_;
    bool initialized_ = false;

    SuitManager(const SuitManager&) = delete;
    SuitManager& operator=(const SuitManager&) = delete;
};

#endif  // SUIT_MANAGER_H
