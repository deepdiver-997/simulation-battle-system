#ifndef PLUGIN_INTERFACE_H
#define PLUGIN_INTERFACE_H

#include <map>
#include <string>
#include <utility>
#include <vector>

#include <fsm/state.h>

class BattleContext;
struct EffectArgs;
enum class EffectResult : int;

// Effect function type (same as defined in effect.h)
using EffectFn = EffectResult (*)(BattleContext*, const EffectArgs&);

// 魂印节点的注册作用域。
//   STAGE （默认）：随精灵上下场——只在宿主精灵于场上时注册；离场时由 epoch 作废。
//                   适用于"只影响自身/自身在场才成立"的魂印（如无为觉者 2260）。
//   ROSTER        ：常驻——只要宿主精灵存活于出战背包（不一定在场）就注册，跨切换保留。
//                   适用于"在场下也提供效果"的魂印（如瀚宇星皇 903 的星皇之赐/之佑、
//                   薇尔诗 2513 的场域抑制）。
// 作用域在**节点**级：一个魂印可以混用（多数魂印整体是同一作用域）。
// 判定"源此刻在场上还是场下"用 BattleContext::find_pet_with_soulmark。
enum class SoulScope {
    STAGE,
    ROSTER,
};

// 魂印程序节点（插件面）：一个时点 + 一个效果函数。
// 对等 skill 的 SkillEffectNode，但坍缩掉"执行结果分叉"维度——魂印只有"时点"。
// trigger_state：触发时点（State 枚举）。
// once：触发一次后移除（回合限一次；下回合由**更新器桶**在回合首时点重注册刷新）。
// early：战斗开始立即执行一次（信号类，如 2260 的 ignore_pp 须在选择技能前就绪）。
// effect_fn：节点效果函数（插件自写，args[0]=owner, args[1]=1-owner 由引擎绑定）。
// scope：注册作用域（见 SoulScope）。
struct SoulMarkNodeRef {
    State trigger_state = State::BATTLE_ROUND_START;
    EffectFn effect_fn = nullptr;
    bool once = false;
    bool early = false;
    SoulScope scope = SoulScope::STAGE;
    // boss 无效标注（2026-09-26 "boss 有效"线）：本节点实现的官方子句带"（boss无效）"
    // 尾注时置位——boss 挑战对局（is_boss_challenge）中该节点**不注册不执行**。
    // 标注单位 = 带尾注的子句（tips 按 | 分句；同一子句的**所有**节点必须统一置位，
    // 漏标会产生半初始化状态，如轮回只禁 arm 留着回档）。未标注 = 任何对局照常。
    bool boss_invalid = false;

    SoulMarkNodeRef() = default;
    SoulMarkNodeRef(State trigger, EffectFn fn, bool once_ = false, bool early_ = false,
                    SoulScope scope_ = SoulScope::STAGE, bool boss_invalid_ = false)
        : trigger_state(trigger), effect_fn(fn), once(once_), early(early_), scope(scope_),
          boss_invalid(boss_invalid_) {}
};

// 魂印级可选钩子（插件提供；留空则走引擎默认行为）。
//
// 与节点注册/作废的分工：
//   - 「登场注册 / 下场作废」本身由引擎按节点的 scope 自动完成（注册节点 + epoch 作废）；
//   - 钩子是**在引擎默认行为之外**的额外动作。
//
// on_enter：登场时（战斗开始 / 切换上场）在引擎重注册节点之后调用。
// on_exit ：离场 / 阵亡时在引擎 epoch 作废之外调用，用于清理**不在效果桶里**的状态。
//           典型：薇尔诗 2513 离场时要关掉它注册的全局抑制标记（该标记是 context 字段，
//           不在桶里，epoch 作废管不到它，必须显式清）。
//
// 签名与 EffectFn 一致但不取 args（免去插件自己绑 owner 的样板）；owner 由引擎传入。
using SoulMarkHookFn = void (*)(BattleContext*, int owner);

struct SoulMarkHooks {
    SoulMarkHookFn on_enter = nullptr;
    SoulMarkHookFn on_exit = nullptr;
};

// ════════════════════════════════════════════════════════════════════
// 单元准入门（注册期钩子，插件提供）
//
// 干什么：技能的**条件效果单元**（组合语法解析出的 EffectUnit）在注册到效果桶之前，
// core 逐条问一次；返回 true = 本条**跳过条件求值**、按无条件注册。
//
// 为什么需要：万相乖离"永久取消第五技能首条未取消过的效果中的触发条件"这类**进度型**
// 改写，若用"把技能对象的 condition 改成 None"表达有两个毛病——
//   ① 进度被持久写进 `Skills`（core 对象被插件改写；同一批 pet 打第二局会把进度带过去；
//      且历史实测"给 Skills 加成员"会引出依赖布局的 UB）；
//   ② 想重置就得另存一份原值台账，两份状态必须同步。
// 改成"**进度（计数器）留在魂印侧 + 注册期按进度放行**"后：技能对象只读，
// 重置 = 计数器清零，一份状态。
//
// 参数：owner = 执行方；skill_id = 正在执行的技能 id；unit_index = 单元在该技能
//       `parsed_units_` 里的下标；condition = 该单元当前条件（UnitCondition 整数值）。
// 语义：返回 false = 照常按条件求值；**不做**"强制跳过本单元不注册"（需要时加返回值，
//       而不是加一个布尔）。
// 时机：per-use（每次技能执行、注册分支时逐条问），不是加载期——加载期没有 BattleContext。
// 用 typedef：effect_unit.h 也声明同名同型（typedef 允许重复声明，using 别名不允许）。
typedef bool (*UnitAdmissionFn)(BattleContext*, int owner, int skill_id, int unit_index,
                                int condition);

// **携带类效果**（"携带此技能时，魂印/专属特性…"族，2026-09-19）：
// 不是传统运行时效果（iconHit"技能使用成功时"），而是"只要携带就生效"的固有为
// （官方 DB 用 analyze 里的 sprite 名区分：iconInherent=固有为 / iconHit=运行时）。
// 执行模型（用户 2026-09-19 拍板，同日恢复槽位）：**查库加载时识别**——loadSkills 逐条
// effect 查本注册表，命中就把函数指针装进 Skills::carryEffects 槽位；**init_battle**
// （构造期，activate_suits 之后）遍历全部精灵的全部技能执行槽位里的非空指针（授予改写
// 票等；典型载荷是 RuleCenter::ANOMALY_POOL_MOD / 魂印参数改写票，见台账"携带类技能
// 强化"行）。曾经因 Skills 布局 UB（根因=Effect 成员未初始化，已修）在扫描期查注册表
// 过渡，槽位已按原设计恢复。
// ⚠️ 每场战斗只扫一次；技能替换后的重扫待做。owner=携带方，skill_slot=0..4。
typedef void (*CarryEffectFn)(BattleContext* ctx, int owner, int skill_slot, int effect_id);

// **战前魂印补丁**（魂印的"战斗开始时"面板族，2026-09-19）：无为觉者的能力减半、
// 卫岳的双防改写、极战·阿尔斯兰的"攻击高于对手"一次性判定等都属此族——它们不是
// 运行时时点效果，而是**战斗开始前**就该执行完的面板程序。执行模型与携带类同款：
// 注册表按魂印 id 索引，init_battle 扫描全部 12 个槽位、魂印 id 命中即执行。
//
// ⚠️ 分**两个时段**（历史遗留的先后顺序，用户 2026-09-19 拍板）——同一份面板，
//    两时段读到的不一样：
//
//   PANEL_SNAPSHOT 「战前快照」：**任何魂印面板修改之前**执行。此时所有 12 只精灵的
//      面板都是"未被魂印改过"的原始值（装备/套装数值不写面板、走独立视图，同样不在）。
//      适用：历史遗留的"比的是游戏前面板"族——极战·阿尔斯兰「自身攻击值高于对手时
//      所有技能额外先制+1」在这里拿原始面板**一次性判定**，结果自存（建议
//      `pet.soulmark_storage`，下场保留），后续时点直接读，不再重比。
//
//   STAT_MODIFY 「战斗开始」：快照段全部跑完**之后**，按注册序逐个执行。先执行的
//      改写会被后执行的看到——"比的是游戏后面板"族（无为觉者的总和减半、卫岳的
//      双防=双攻较低项）在这里落笔。
//
// side=0/1，slot=0..5（战场全 roster，不按在场过滤——"战斗开始时"是面板事实，
// 与上下场无关；需要在场语义的效果体自查）。
enum class PreBattlePhase {
    PANEL_SNAPSHOT = 1,
    STAT_MODIFY = 2,
};
typedef void (*PreBattlePatchFn)(BattleContext* ctx, int side, int slot);
struct PreBattlePatchEntry {
    int soulmark_id;
    PreBattlePhase phase;
    PreBattlePatchFn fn;
};

// Plugin interface version for compatibility checking
constexpr const char* kPluginInterfaceVersion = "1.0";

// Plugin registration function signature
// Each plugin library MUST export a function with this signature:
// extern "C" void plugin_register_effects(void* registry)
using PluginRegisterFn = void (*)(void*);

// Effect registry interface - passed to plugins for registration
// This interface allows plugins to register their effects without
// needing to know the concrete manager/factory types
class IEffectRegistry {
public:
    virtual ~IEffectRegistry() = default;

    // Register a soul mark effect
    virtual void registerSoulMark(int soulmark_id, EffectFn effect_fn) = 0;

    // Register a soul mark program（多时点：一个魂印 = 多个 State→effect 节点）。
    virtual void registerSoulMarkProgram(int soulmark_id,
                                         const std::vector<SoulMarkNodeRef>& nodes) = 0;

    // 注册魂印级钩子（可选取）。默认空实现，不强制既有实现者改写。
    virtual void registerSoulMarkHooks(int soulmark_id, const SoulMarkHooks& hooks) {
        (void)soulmark_id;
        (void)hooks;
    }

    // 注册**单元准入门**（见 UnitAdmissionFn 注释）。默认空实现，不强制既有实现者改写。
    virtual void registerUnitAdmission(UnitAdmissionFn fn) { (void)fn; }

    // 注册**携带类效果**（见 CarryEffectFn 注释）。默认空实现，不强制既有实现者改写。
    // 注册表按 effect_id 索引；loadSkills 加载技能时逐条查询、命中装槽。
    virtual void registerCarryEffect(int effect_id, CarryEffectFn fn) {
        (void)effect_id;
        (void)fn;
    }

    // 注册**战前魂印补丁**（见 PreBattlePhase 注释）。默认空实现，不强制既有实现者改写。
    // 注册表按 (魂印 id, 时段) 索引；init_battle 分两时段扫描执行。
    virtual void registerSoulMarkPreBattle(int soulmark_id, PreBattlePhase phase,
                                           PreBattlePatchFn fn) {
        (void)soulmark_id;
        (void)phase;
        (void)fn;
    }

    // Register a skill/move effect
    virtual void registerSkillEffect(int effect_id, EffectFn effect_fn) = 0;

    // Register multiple soul marks at once
    virtual void registerSoulMarks(
        const std::vector<std::pair<int, EffectFn>>& soulmarks) = 0;

    // Register multiple skill effects at once
    virtual void registerSkillEffects(
        const std::vector<std::pair<int, EffectFn>>& effects) = 0;
};

// ════════════════════════════════════════════════════════════════════
// 静态自注册 —— 插件侧不再手写「中央注册清单」
//
// 用法（插件 .cpp 里，效果函数定义之后，文件作用域）：
//     SKILL_EFFECT(816, effect_816_immunity_reflect_true);
//     SOUL_MARK(1004, soulmark_canglan_apply_eternal_tears);
//     SOUL_PROGRAM(1217, SoulMarkNodeRef(State::BATTLE_ROUND_START, &fn, false, false));
//
// 宏展开成一个**静态初始化对象**，在 `dlopen` 那一刻把条目塞进本 dylib 的表；
// `plugin_register` 再一次性把整张表推给 registry。
//
// 为什么这么设计（而不是"core 侧按 `effect_<id>` 名字 dlsym"）：
//   · 表由插件**推**、id 留在**编译期**——写错 id 直接编译不过；dlsym 是按字符串拉，
//     名字打错只会让 `dlsym` 返回 null，变成**运行期静默失效**（本项目已被"静默早退"坑过两次）。
//   · 不需要中央清单：写完函数写一行宏即可，改名/删除只动一处。
//   · `extern "C"` 也省了：dlsym 路线要求每个函数都加，行数并不比现在少。
//   · 别名（一个函数挂两个 id）照样支持：写两行宏。
//
// 为什么 init 符号**去不掉**：插件需要 `CoreApi*`（core→插件的函数指针表），
// 而递"数据"只能靠一次调用。所以 `plugin_register` 必须留着，这里只是把它从
// "逐个 register" 变成"推一张表"。
//
// ⚠️ 表是**每 dylib 一份**：`inline` 函数的函数内静态在各 .so/.dylib 里各有一份实例
//    （插件之间不互相链接，正合需要）。
// ⚠️ 静态初始化在 dlopen 时执行，**早于** core 调 `plugin_register` —— 顺序天然正确。
// ⚠️ 宏参数要求是**函数名**（`&(fn)` 取地址）：传成员函数/空指针编译不过，比运行期跳过更早暴露。
// ════════════════════════════════════════════════════════════════════
namespace plugin_reg {

struct SkillEffectEntry {
    int id;
    EffectFn fn;
};
struct SoulMarkEntry {
    int id;
    EffectFn fn;
};
struct SoulProgramEntry {
    int id;
    std::vector<SoulMarkNodeRef> nodes;
};

// 函数内静态：避开跨编译单元的静态初始化顺序问题。
inline std::vector<SkillEffectEntry>& skill_effects() {
    static std::vector<SkillEffectEntry> table;
    return table;
}
inline std::vector<SoulMarkEntry>& soul_marks() {
    static std::vector<SoulMarkEntry> table;
    return table;
}
inline std::vector<SoulProgramEntry>& soul_programs() {
    static std::vector<SoulProgramEntry> table;
    return table;
}

inline bool add_skill_effect(int id, EffectFn fn) {
    skill_effects().push_back(SkillEffectEntry{id, fn});
    return true;
}
inline bool add_soul_mark(int id, EffectFn fn) {
    soul_marks().push_back(SoulMarkEntry{id, fn});
    return true;
}
inline bool add_soul_program(int id, std::vector<SoulMarkNodeRef> nodes) {
    soul_programs().push_back(SoulProgramEntry{id, std::move(nodes)});
    return true;
}

// 单元准入门：**每 dylib 至多一条**（全局钩子，不按 id 分）。多条时按注册顺序任一命中即放行。
inline std::vector<UnitAdmissionFn>& unit_admissions() {
    static std::vector<UnitAdmissionFn> table;
    return table;
}
inline bool add_unit_admission(UnitAdmissionFn fn) {
    unit_admissions().push_back(fn);
    return true;
}

// 携带类效果：按 effect_id 索引（id → fn）。同 id 重复注册以后到者为准（覆盖键语义）。
inline std::map<int, CarryEffectFn>& carry_effects() {
    static std::map<int, CarryEffectFn> table;
    return table;
}
inline bool add_carry_effect(int effect_id, CarryEffectFn fn) {
    carry_effects()[effect_id] = fn;
    return true;
}

// 战前魂印补丁：按注册序存（同魂印可多条、跨时段保序——同注册序内先快照段后修改段
// 由执行侧两次扫描保证，这里只保注册序）。
inline std::vector<PreBattlePatchEntry>& pre_battle_patches() {
    static std::vector<PreBattlePatchEntry> table;
    return table;
}
inline bool add_pre_battle_patch(int soulmark_id, PreBattlePhase phase, PreBattlePatchFn fn) {
    pre_battle_patches().push_back(PreBattlePatchEntry{soulmark_id, phase, fn});
    return true;
}

// 魂印级钩子（登场/离场额外动作）：按魂印 id 存（2026-09-20，阿尔忒弥斯 4745
// 离场清狩杀之魂首用）。见 SoulMarkHooks 注释——引擎默认行为（节点注册 + epoch 作废）
// 之外的"桶外状态"清理走这里。
struct SoulHookEntry {
    int soulmark_id;
    SoulMarkHooks hooks;
};
inline std::vector<SoulHookEntry>& soul_hooks() {
    static std::vector<SoulHookEntry> table;
    return table;
}
inline bool add_soul_hook(int soulmark_id, const SoulMarkHooks& hooks) {
    soul_hooks().push_back(SoulHookEntry{soulmark_id, hooks});
    return true;
}

// 把本 dylib 的三张表一次性推给 registry（在 `plugin_register` 里调一次）。
inline void flush(IEffectRegistry* registry) {
    if (!registry) {
        return;
    }
    for (const SkillEffectEntry& e : skill_effects()) {
        registry->registerSkillEffect(e.id, e.fn);
    }
    for (const SoulMarkEntry& e : soul_marks()) {
        registry->registerSoulMark(e.id, e.fn);
    }
    for (const SoulProgramEntry& e : soul_programs()) {
        registry->registerSoulMarkProgram(e.id, e.nodes);
    }
    for (UnitAdmissionFn fn : unit_admissions()) {
        registry->registerUnitAdmission(fn);
    }
    for (const auto& [effect_id, fn] : carry_effects()) {
        registry->registerCarryEffect(effect_id, fn);
    }
    for (const PreBattlePatchEntry& e : pre_battle_patches()) {
        registry->registerSoulMarkPreBattle(e.soulmark_id, e.phase, e.fn);
    }
    for (const SoulHookEntry& e : soul_hooks()) {
        registry->registerSoulMarkHooks(e.soulmark_id, e.hooks);
    }
}

}  // namespace plugin_reg

#define SKILL_EFFECT(id, fn)                                                        \
    namespace {                                                                     \
    [[maybe_unused]] const bool kSkillEffectReg_##fn = ::plugin_reg::add_skill_effect((id), &(fn)); \
    }

#define SOUL_MARK(id, fn)                                                           \
    namespace {                                                                     \
    [[maybe_unused]] const bool kSoulMarkReg_##fn = ::plugin_reg::add_soul_mark((id), &(fn)); \
    }

#define SOUL_PROGRAM(id, ...)                                                       \
    namespace {                                                                     \
    [[maybe_unused]] const bool kSoulProgramReg_##id =                              \
        ::plugin_reg::add_soul_program((id), {__VA_ARGS__});                        \
    }

// 单元准入门（全局一条）：UNIT_ADMISSION(fn) —— fn 签名见 UnitAdmissionFn。
#define UNIT_ADMISSION(fn)                                                          \
    namespace {                                                                     \
    [[maybe_unused]] const bool kUnitAdmissionReg_##fn =                            \
        ::plugin_reg::add_unit_admission(&(fn));                                    \
    }

// 携带类效果：CARRY_EFFECT(id, fn) —— fn 签名见 CarryEffectFn（战斗开始扫描执行）。
#define CARRY_EFFECT(id, fn)                                                        \
    namespace {                                                                     \
    [[maybe_unused]] const bool kCarryEffectReg_##id =                              \
        ::plugin_reg::add_carry_effect((id), &(fn));                                \
    }

// 战前魂印补丁：SOUL_PRE_BATTLE(id, phase, fn) —— 见 PreBattlePhase 注释。
// 例：SOUL_PRE_BATTLE(4327, PreBattlePhase::PANEL_SNAPSHOT, alslan_panel_snapshot);
#define SOUL_PRE_BATTLE(id, phase, fn)                                              \
    namespace {                                                                     \
    [[maybe_unused]] const bool kPreBattlePatchReg_##id##_##fn =                    \
        ::plugin_reg::add_pre_battle_patch((id), (phase), &(fn));                   \
    }

// 魂印级钩子：SOUL_MARK_HOOKS(id, on_enter_fn, on_exit_fn) —— 两参均可传 nullptr。
// 例：SOUL_MARK_HOOKS(4745, nullptr, &artemis_exit_clear_hunt_soul);
#define SOUL_MARK_HOOKS(id, enter_fn, exit_fn)                                      \
    namespace {                                                                     \
    [[maybe_unused]] const bool kSoulHookReg_##id =                                 \
        ::plugin_reg::add_soul_hook((id), ::SoulMarkHooks{(enter_fn), (exit_fn)});  \
    }

// Utility to convert effect ID to function name
// Format: effect_{category}_{id}
// category: "soulmark" for soul marks, "skill" for skills
inline std::string effectIdToFunctionName(const std::string& category, int id) {
    return "effect_" + category + "_" + std::to_string(id);
}

// Default plugin init function names
constexpr const char* kSoulMarkPluginInitFn = "soulmark_plugin_register";
constexpr const char* kSkillPluginInitFn = "skill_plugin_register";

#endif // PLUGIN_INTERFACE_H