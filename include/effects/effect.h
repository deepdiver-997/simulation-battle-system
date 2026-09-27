#ifndef EFFECT_H
#define EFFECT_H

#include <algorithm>
#include <map>
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
// - INHERENT：**固有效果**分支（空元之诗 2087~2090，用户 2026-09-20 口径）——
//   不因 miss / 被封属 / 打在盔上而失效（那些都走 SKILL_INVALID 补偿趟），也不依赖命中：
//   HIT 与 SKILL_INVALID **两趟都注册**（`Skills::execute` 的三个注册点各补一条）。
//   不是 execute() 的返回值——它只作为 `effectBranches` 的键存在。
//   唯一能压下它的是"封效果"类机制：引擎暂无独立封印种，命中效果失效趟沿用
//   逐节点 nullify 过滤（`nullify.hit_effect_invalidatable`）= 现有最接近的"效果被封"开关。
enum class SkillExecResult {
    HIT,
    SKILL_INVALID,
    EFFECT_INVALID,
    INHERENT,
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
    // ⚠️ **没有默认构造**——每个 Effect 都必须走全参构造，构造来源可枚举。
    //    历史（2026-09-19 定案，任务台账"Skills 布局 UB"）：曾有 `Effect() = default`
    //    且成员裸奔，"默认构造后只填部分字段"的路径（loadSkills 的
    //    base_priority_effect / unit_effect、grant_guaranteed_first、测试注入）读到
    //    栈/堆垃圾 —— left_round 为正时效果被误注册成 N 回合的回合类效果
    //    （isRoundEffect()=duration_rounds_>0），计入 has_round_effects 后"消回合成功"
    //    族（797/889）就误判成功，且症状随 Skills 布局漂移。默认构造已删：
    //    新代码再写 `Effect e;` 会直接编译失败，迫使构造点显式给全字段。
    //    默认值对齐 EffectFactory::getEffect 的显式口径：left_round=0（一次性，本回合有效）。
    // 内联实现：插件动态库不链接 sim_core，构造函数需头文件可见。
    Effect(int id, int priority, int owner, int lr, EffectArgs args = {}, EffectFn logic = nullptr)
        : id(id), priority(priority), left_round(lr), owner(owner), description(),
          logic(logic), args(std::move(args)) {}

    int id = 0;
    int priority = 0;
    int left_round = 0;
    int owner = 0;
    std::string description;
    // 逻辑函数指针的可调用性依赖其来源动态库仍然处于已加载状态；
    // 但 Effect 实例自身的生命周期可由 Skills / SkillEffectNode 持有。
    // NSDMI 是保险丝：ctor 恒赋值，但防将来聚合/成员初始化路径漏赋值后裸调。
    EffectFn logic = nullptr;
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

    // 携带类效果（IEffectRegistry）：effect_id → fn 注册表。loadSkills 加载技能时
    // 逐条查询、命中把 fn 装进 Skills::carryEffects 槽位（见 CarryEffectFn 注释）。
    void registerCarryEffect(int effect_id, CarryEffectFn fn) override;
    // 查询（loadSkills 用）；未注册返回 nullptr。init 前调用安全（空表）。
    CarryEffectFn find_carry_effect(int effect_id) const;

    // 已注册效果 id 全集（SKILL_EFFECT ∪ CARRY_EFFECT，含手动注册），升序去重。
    // 覆盖率对账用：数据层引用的 effect id 集合减去本集合 = "未实现"清单。
    std::vector<int> registered_effect_ids() const;

    // 战前魂印补丁（IEffectRegistry）：按 (魂印 id, 时段) 注册，见 PreBattlePhase 注释。
    void registerSoulMarkPreBattle(int soulmark_id, PreBattlePhase phase,
                                   PreBattlePatchFn fn) override;
    // 两时段扫描执行：遍历双方全部 6 槽，魂印 id 命中本时段注册表即执行 fn(ctx, side, slot)。
    // init_battle 在 activate_suits / 携带扫描之后按 PANEL_SNAPSHOT → STAT_MODIFY 各调一次。
    void run_pre_battle_patches(BattleContext* ctx, PreBattlePhase phase);

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

    // 携带类效果注册表（effect_id → fn；插件 flush 时灌入，见 CarryEffectFn 注释）
    std::map<int, CarryEffectFn> carry_effects_;

    // 战前魂印补丁注册表（插件 flush 时灌入，见 PreBattlePhase 注释；保注册序）
    std::vector<PreBattlePatchEntry> pre_battle_patches_;

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