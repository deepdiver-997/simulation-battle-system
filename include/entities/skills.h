#ifndef SKILLS_H
#define SKILLS_H

#include <algorithm>
#include <cstdlib>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>
#include <db/official_data_repository.h>
#include <effects/effect.h>
#include <effects/effect_unit.h>

// Forward declare BattleContext and State
class BattleContext;
enum class State;
enum class SkillType {
    Physical = 0,
    Special = 1,
    Attribute = 2,
};

// 技能可用性修饰类型：用于处理“可无视PP/不可无视PP”等动态效果。
enum class SkillUsabilityEffectType {
    IgnorePP,
    ForceRespectPP,
};

struct SkillUsabilityEffectEntry {
    int effectId = -1;
    SkillUsabilityEffectType type = SkillUsabilityEffectType::IgnorePP;
    bool active = true;
};

// 选择期：技能能否被选中（在操作确认时检查，早于先手权判定）
enum class SkillSelectionResult {
    SELECTABLE,   // 可选
    PP_EMPTY,     // PP 耗尽
    LOCKED,       // 被锁定/封印，无法选择
};

// 执行期：本次技能使用能否成功（先手权判定后、轮到出手时检查）
enum class SkillUsageResult {
    OK,      // 可用
    MISS,    // 未命中（命中率判定失败）
    SEALED,  // 被无效类拦截（盔/威/封属，已消费次数）→ SKILL_INVALID + 补偿
    HIT_INVALID,  // 命中失效类拦截（SEAL_ATTRIBUTE_HIT）→ 技能照常命中、效果失效、无补偿
};

// SkillEffectNode - 技能分支中的单个注册节点
//
// effect 做什么，由 Effect 模板本身负责；
// registerState 则属于 skill 调度层，决定“什么时候把 effect 挂出去”。
//
// ⚠️ 这里曾有一支 `usePendingTrigger / pendingObserveState` —— 把效果登记为"未来触发器"
//    （`BattleContext::pending_effects`），等某个时点再落地。已删除（2026-09-15，用户口径）：
//    它就是**高级监控**、不响应断回合，而**事件中心完全覆盖**了这个能力
//    （`EventCenter` + `register_watcher`，「当X发生时再挂效果」是它的本职）；
//    且 `resources/` 对它**零命中**——没有任何效果数据用过它，纯死重。
struct SkillEffectNode {
    Effect effect;
    State registerState;

    SkillEffectNode(Effect effect_, State registerState_)
        : effect(effect_)
        , registerState(registerState_) {}
};

class Skills {
public:
    Skills() = delete;
    Skills(int id) : id(id) {
        if (!loadSkills()) {
            throw std::runtime_error("Failed to load skill with id: " + std::to_string(id));
        }
    }
    Skills(int id, const official_data::MonsterRecord& monster);
    // 拷贝构造：parsed_units_ 深拷贝成新 vector 后，须把引用它元素的 Effect args.extra
    // 指针重定基到新 vector（否则组合语法效果的 extra 悬垂 → 执行崩溃）。
    // 移动构造保持默认（buffer 所有权转移，extra 指针仍有效）。
    Skills(const Skills& other)
        : is_locked(other.is_locked)
        , maxPP(other.maxPP)
        , pp(other.pp)
        , id(other.id)
        , name(other.name)
        , type(other.type)
        , power(other.power)
        , accuracy(other.accuracy)
        , must_hit(other.must_hit)
        , critical_strike_rate(other.critical_strike_rate)
        , combo_min(other.combo_min)
        , combo_max(other.combo_max)
        , penetration_flags(other.penetration_flags)
        , priority(other.priority)
        , element{other.element[0], other.element[1]}
        , rawEffectRecords(other.rawEffectRecords)
        , effectBranches(other.effectBranches)
        , selection_effects_(other.selection_effects_)
        , parsed_units_(other.parsed_units_)
        , usabilityEffects(other.usabilityEffects) {
        rebase_parsed_unit_pointers(other.parsed_units_);
    }
    ~Skills() = default;
    // 计划从官方 SQLite 中加载技能静态层：
    // 1. 通过 official_data::OfficialDataStore / OfficialDataRepository 查 moves
    // 2. 读取 side_effect + effect_info，拆出本技能的效果序列与参数
    // 3. 再按本地映射规则生成 effectBranches
    bool loadSkills();
    bool skill_usable(BattleContext* ctx = nullptr, int owner = -1);
    void register_usability_effect(int effectId, SkillUsabilityEffectType type, bool active = true);
    void set_usability_effect_active(int effectId, bool active);
    void remove_usability_effect(int effectId);
    void clear_usability_effects();
    Effect clone_effect(int effectId, EffectArgs args = {}) const;
    void add_effect_node(SkillExecResult result, SkillEffectNode node);
    // 万相乖离"取消触发条件"：按解析顺序找 parsed_units_ 里第一条 condition != None 的
    // 效果单元，置 condition=None（永久无条件），返回其下标；没有可取消的返回 -1。
    // 执行器每次执行时从 args.extra 解引用 → 读到修改后的 condition，自然生效。
    // 头内联：插件（soul_lib/moves_lib 不链接 sim_core）也要调用。
    int cancel_next_parsed_condition() {
        for (std::size_t i = 0; i < parsed_units_.size(); ++i) {
            if (parsed_units_[i].condition != UnitCondition::None) {
                parsed_units_[i].condition = UnitCondition::None;  // 永久取消：无条件触发
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    //--- 三阶段生命周期 ---
    // 选择期（操作确认时）：技能能否被选中 → 立即注册先制等即时效果
    SkillSelectionResult query_selectable(BattleContext* ctx, int owner);
    void on_selected(BattleContext* ctx, int owner);
    // 执行期（先手权判定后、轮到出手时）：本次使用能否成功（miss/封效果）
    SkillUsageResult query_usage(BattleContext* ctx, int owner);
    // 执行期主流程：命中/无效判定 → 注册对应分支效果到各时点桶。
    // 返回结果 + resolution flags（FSM 写回 workspace 供后续时点判断）。
    std::pair<SkillExecResult, SkillResolutionFlags> execute(BattleContext* ctx, int owner, State trigger_state);

private:
    // 把 effectBranches/selection_effects_ 里指向 old_units 元素的 extra 指针重定基到
    // 本对象 parsed_units_（拷贝构造用；旧元素必然整体落在 old_units 缓冲区间内）。
    void rebase_parsed_unit_pointers(const std::vector<EffectUnit>& old_units) {
        if (old_units.empty() || parsed_units_.empty()) {
            return;
        }
        const char* old_lo = reinterpret_cast<const char*>(old_units.data());
        const char* old_hi = old_lo + old_units.size() * sizeof(EffectUnit);
        const char* new_lo = reinterpret_cast<const char*>(parsed_units_.data());
        auto rebase_effect = [&](Effect& e) {
            if (!e.args.extra) {
                return;
            }
            const char* p = static_cast<const char*>(e.args.extra);
            if (p >= old_lo && p < old_hi) {
                e.args.extra = new_lo + (p - old_lo);
            }
        };
        for (auto& [result, nodes] : effectBranches) {
            (void)result;
            for (SkillEffectNode& node : nodes) {
                rebase_effect(node.effect);
            }
        }
        for (SkillEffectNode& node : selection_effects_) {
            rebase_effect(node.effect);
        }
    }

    // 把某结果分支下的效果节点注册到对应时点桶。
    // filter_hit_invalid=true 时按效果元数据 nullify.hit_effect_invalidatable 逐节点过滤（③层）。
    void register_branch(BattleContext* ctx, int owner, SkillExecResult result,
                         const SkillResolutionFlags& flags, bool filter_hit_invalid = false);

public:
    bool is_locked = false;
    int maxPP;
    int pp;  // pp == -1 -> 技能使用无限制
    int id = -1;
    std::string name;

    // 技能分类：与官方 moves.Category 对齐后再映射到本地枚举。
    SkillType type;
    int power;
    int accuracy;
    bool must_hit = false;   // 必中：命中结算无视命中率
    float critical_strike_rate;

    //========== 连击（"1回合做 x~y 次攻击"）==========
    // 官方把"n次连击"与"威力提升n%/n点"并列为**变威力效果**（L453），伤害是
    // **一次伤害公式 × N**（不是 N 次独立结算），`×连击次数` 排在公式最后
    // （浮动 → 暴击抗性 → 暴击系数 → 连击次数，L101/L99）。
    // 静态基数从技能自己的 side_effect 参数里取（`combo_arg_indices` 开关表），
    // 每次技能使用掷一次写进 `ws.combo_view[owner]`；无连击模板的技能是 1~1（退化）。
    // ⚠️ 目前只覆盖"静态 x~y 区间"那一层（12 个模板）；动态加数（1108/484/1795/1863/1930）
    //    与"连击上限修正"（1500/1546/…）见 docs/05-任务清单/待做-变威力与增减伤时点.md §2。
    int combo_min = 1;
    int combo_max = 1;

    // 连击次数掷点：**每次技能使用掷一次**，第一次/第二次结算共用同一个 N。
    // 区间退化（min==max，绝大多数技能是 1~1）时**不消耗 rand()** —— 与"暴击率 0 短路"同理，
    // 保证没有连击的既有场景 rand 序列逐字节不变。
    int roll_combo_count() const {
        if (combo_max <= combo_min) {
            return std::max(1, combo_min);
        }
        return combo_min + (std::rand() % (combo_max - combo_min + 1));
    }
    // 技能请求凭证（697"无视伤害限制"/699"无视攻击免疫"/强制执行等效果模板经 effect_meta 识别后合并）。
    // 攻击时由 materialize_attack_credential 并入 ws.attack_credential，作为"请求携带的凭证"，
    // 在 query_usage 门判定（先于效果注册）消费。force_execute 非穿透但同族（凭证位）。
    struct PenetrationFlags {
        bool ignore_attack_immunity = false;  // 穿"攻击免疫/狮盔"（699）
        bool ignore_damage_limit = false;     // 穿"伤害限制"（697）
        bool force_execute = false;           // 强制执行：必定命中 + 无视命中效果失效（2474 等）
        int  level = 0;                       // 0=无, 1=可穿盔
    };
    PenetrationFlags penetration_flags;
    int priority;   // 先制等级：官方 priority + 本地调整值，数值越大越先行动
    int element[2];  // 元素属性
    std::vector<official_data::SkillEffectRecord> rawEffectRecords;

    // 效果分支表：
    // 官方 moves.SideEffect / SideEffectArg 先被解释成效果调用序列，
    // 再按技能层结果拆到不同分支。
    //
    // - HIT：正常技能效果
    // - SKILL_INVALID：miss / 技能无效类补偿
    // - EFFECT_INVALID：通常不注册任何技能效果，必要时仍可预留分支
    //
    // 每个分支中的节点再额外描述“注册时点”或“是否为未来触发器”。
    // Effect 逻辑函数指针的来源可以是动态库或 EffectFactory，
    // 但真正参与技能分支的 Effect 实例由 Skills 分支节点持有生命周期。
    std::map<SkillExecResult, std::vector<SkillEffectNode>> effectBranches;

    // 选择期效果集合：选技能时立即注册到 BATTLE_FIRST_MOVE_RIGHT 的效果。
    // 至少含基值先制（preemptive_level[owner] += priority）；条件先制效果由数据追加。
    std::vector<SkillEffectNode> selection_effects_;

    // 解析出的条件效果单元（组合语法：模板 → EffectUnit）。
    // 生命周期贯穿 Skills；通用执行器 Effect 的 args.extra 指向其中的单元。
    // loadSkills 开头 reserve 足量防 realloc（解析器的分支指针约定）。
    std::vector<EffectUnit> parsed_units_;

    // 技能可用性修饰列表：
    // 典型用途：魂印/印记带来的“PP=0 仍可释放”或“禁止无视PP”。
    std::vector<SkillUsabilityEffectEntry> usabilityEffects;
};

#endif // SKILLS_H
