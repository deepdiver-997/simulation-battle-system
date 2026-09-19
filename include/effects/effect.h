#ifndef EFFECT_H
#define EFFECT_H

#include <algorithm>
#include <string>
#include <vector>
#include <cstddef>
#include <memory>
#include <utility>
#include <unordered_map>
#include <shared_mutex>

#include <utils/dynamic_library.h>
#include <plugin/plugin_interface.h>

// 单条 effect 的执行结果，不等同于整次技能使用结果。
enum class EffectResult : int {
    kOk = 0,
    kSkillInvalid = 1,
    kHitEffectInvalid = 2
};

// 技能层执行结果：
// - HIT：正常命中，允许注册正常技能效果
// - SKILL_INVALID：技能没有正常进入命中效果结算分支（包含 miss / 技能无效）
// - EFFECT_INVALID：命中效果失效，不注册任何技能效果，但仍可继续进入伤害链
enum class SkillExecResult {
    HIT,
    SKILL_INVALID,
    EFFECT_INVALID,
};

// 技能层对后续流程的两个核心开关：
// - registerSkillEffects：本次是否允许把"当前执行结果对应分支"的 effect 注册出去
//   HIT -> 正常技能效果
//   SKILL_INVALID -> miss / 技能无效补偿
//   EFFECT_INVALID -> 不注册任何 skill effect
// - allowAttackDamagePipeline：本次是否允许继续进入后续攻击伤害流程
struct SkillResolutionFlags {
    bool registerSkillEffects = true;
    bool allowAttackDamagePipeline = true;
};

enum class HitInvalidMode : int {
    kEffectsOnly = 0,
    kFullNull = 1
};

struct EffectArgs {
    std::vector<int> owned_int_args;
    std::vector<int> owned_vec_offsets;
    std::vector<int> owned_vec_sizes;
    const int* int_args = nullptr;
    int int_count = 0;
    const int* vec_offsets = nullptr;
    const int* vec_sizes = nullptr;
    int vec_count = 0;
    const void* extra = nullptr;

    EffectArgs() = default;
    explicit EffectArgs(std::vector<int> ints, const void* extra_ = nullptr)
        : owned_int_args(std::move(ints)), extra(extra_) {
        refresh_views();
    }

    EffectArgs(std::vector<int> ints,
               std::vector<int> offsets,
               std::vector<int> sizes,
               const void* extra_ = nullptr)
        : owned_int_args(std::move(ints))
        , owned_vec_offsets(std::move(offsets))
        , owned_vec_sizes(std::move(sizes))
        , extra(extra_) {
        refresh_views();
    }

    EffectArgs(const EffectArgs& other)
        : owned_int_args(other.owned_int_args)
        , owned_vec_offsets(other.owned_vec_offsets)
        , owned_vec_sizes(other.owned_vec_sizes)
        , extra(other.extra) {
        refresh_views();
    }

    EffectArgs(EffectArgs&& other) noexcept
        : owned_int_args(std::move(other.owned_int_args))
        , owned_vec_offsets(std::move(other.owned_vec_offsets))
        , owned_vec_sizes(std::move(other.owned_vec_sizes))
        , extra(other.extra) {
        refresh_views();
        other.refresh_views();
    }

    EffectArgs& operator=(const EffectArgs& other) {
        if (this == &other) {
            return *this;
        }
        owned_int_args = other.owned_int_args;
        owned_vec_offsets = other.owned_vec_offsets;
        owned_vec_sizes = other.owned_vec_sizes;
        extra = other.extra;
        refresh_views();
        return *this;
    }

    EffectArgs& operator=(EffectArgs&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        owned_int_args = std::move(other.owned_int_args);
        owned_vec_offsets = std::move(other.owned_vec_offsets);
        owned_vec_sizes = std::move(other.owned_vec_sizes);
        extra = other.extra;
        refresh_views();
        other.refresh_views();
        return *this;
    }

    void refresh_views() {
        int_args = owned_int_args.empty() ? nullptr : owned_int_args.data();
        int_count = static_cast<int>(owned_int_args.size());
        vec_offsets = owned_vec_offsets.empty() ? nullptr : owned_vec_offsets.data();
        vec_sizes = owned_vec_sizes.empty() ? nullptr : owned_vec_sizes.data();
        vec_count = static_cast<int>(std::min(owned_vec_offsets.size(), owned_vec_sizes.size()));
    }
};

class BattleContext;
class EffectFactory;

using EffectFn = EffectResult (*)(BattleContext*, const EffectArgs&);

class Effect {
public:
    Effect() = default;
    // 内联实现：插件动态库不链接 sim_core，构造函数需头文件可见。
    Effect(int id, int priority, int owner, int lr, EffectArgs args = {}, EffectFn logic = nullptr)
        : id(id), priority(priority), left_round(lr), owner(owner), description(),
          logic(logic), args(std::move(args)) {}

    int id;
    int priority;
    int left_round;
    int owner;
    std::string description;
    // 逻辑函数指针的可调用性依赖其来源动态库仍然处于已加载状态；
    // 但 Effect 实例自身的生命周期可由 Skills / SkillEffectNode 持有。
    EffectFn logic;
    EffectArgs args;

    bool operator<(const Effect& other) const { return priority < other.priority; }
    bool operator==(const Effect& other) const { return id == other.id; }
    bool operator!=(const Effect& other) const { return id != other.id; }
    bool operator>(const Effect& other) const { return priority > other.priority; }
    bool operator==(std::nullptr_t) const { return logic == nullptr; }
    bool operator!=(std::nullptr_t) const { return logic != nullptr; }
};

using EffectPtr = std::shared_ptr<Effect>;

class EffectFactory : public IEffectRegistry {
public:
    // 获取单例实例（线程安全）
    // 首次调用时必须传入动态库所在目录路径
    // lib_dir: 动态库目录路径，目录中应包含 .so/.dylib/.dll 文件
    static EffectFactory& getInstance(const std::string& lib_dir = "");

    void init();
    Effect getEffect(int id, EffectArgs args = {});
    EffectPtr createEffect(int id, EffectArgs args = {});
    bool no_confilict(int id1, int id2);

    // IEffectRegistry interface - for plugin use
    void registerSoulMark(int soulmark_id, EffectFn effect_fn) override;
    void registerSoulMarkProgram(int soulmark_id,
                                 const std::vector<SoulMarkNodeRef>& nodes) override;
    void registerSkillEffect(int effect_id, EffectFn effect_fn) override;
    void registerSoulMarks(
        const std::vector<std::pair<int, EffectFn>>& soulmarks) override;
    void registerSkillEffects(
        const std::vector<std::pair<int, EffectFn>>& effects) override;

    // 单元准入门（IEffectRegistry）：存进 effect_unit 的进程全局（见 UnitAdmissionFn 注释）。
    void registerUnitAdmission(UnitAdmissionFn fn) override;

    // 手动注册效果（不通过动态库）
    void registerEffect(int effectId, EffectFn effect);

    // 检查是否已初始化
    bool isInitialized() const { return initialized_; }

    // 获取已加载的动态库数量（调试用）
    size_t getLoadedLibraryCount() const;

private:
    EffectFactory();
    ~EffectFactory() = default;

    // 从动态库加载所有效果
    void loadFromDynamicLibraries(const std::string& lib_dir);

    // 初始化单例（只执行一次）
    void ensureInitialized(const std::string& lib_dir);

    static bool roll_percent(int percent);

    // 缓存映射表：效果ID -> 效果函数
    std::unordered_map<int, EffectFn> effect_cache_;
    mutable std::shared_mutex cache_mutex_;

    // 加载的动态库（保持加载状态，防止函数指针失效）
    std::vector<DynamicLibrary> loaded_libraries_;

    // 初始化标志
    bool initialized_ = false;

    // 禁止拷贝
    EffectFactory(const EffectFactory&) = delete;
    EffectFactory& operator=(const EffectFactory&) = delete;
    EffectFactory(EffectFactory&&) = delete;
    EffectFactory& operator=(EffectFactory&&) = delete;
};

#endif // EFFECT_H