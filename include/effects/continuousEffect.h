#ifndef CONTINUOUS_EFFECT_H
#define CONTINUOUS_EFFECT_H

#include <memory>
#include <functional>
#include <effects/effect.h>

class BattleContext;
class Skills;
enum class State;

/**
 * ContinuousEffect - 持续效果基类
 *
 * Skill/魂印中的 Effect 是模板（函数指针+参数），不可变。
 * ContinuousEffect 是 Effect 的实例化，保存运行状态和上下文。
 *
 * 回合过期机制：
 * - registered_round_: 注册时的回合数
 * - duration_rounds_: 持续回合数
 * 判断过期：currentRound - registered_round_ >= duration_rounds_
 */

// 效果类别（区分行为差异，替代多重继承）
enum class EffectKind {
    GENERIC,    // 通用效果（到时点执行函数指针）
    ROUND,      // 回合效果（isRoundEffect()=true）
};

// 效果作用域：ON_STAGE = 当前场上精灵，切换作废；TEAM = 全队绑定，切换保留
enum class EffectScope {
    ON_STAGE,
    TEAM,
};

// 回合数窗口家族（官方 effect 文本两类，语义不同 —— 用户 2026-09-13 定；引擎级通用规则）：
//   InRounds   —— "{N}回合内"（DB 里约 495 条模板）。**先出手**：本回合就结算一次 → 生效 [R, R+N-1]；
//                 **后出手**：本回合已"错过结算" → 顺延一回合 → 生效 [R+1, R+N]（N≥2 时才顺延；
//                 N=1 的"本回合内"照旧只覆盖本回合，不挪到下一回合）。
//   NextRounds —— "下{N}回合"（约 287 条，如"下2回合必定先手"）。语义上**本回合就不算** →
//                 先/后出手**都**从下一回合起算：[R+1, R+N]（后出手**不再**额外顺延）。
// 声明来源 = **认证数据层** custom_effect_overrides(override_type='window', map_value=...)，未声明默认 InRounds。
enum class EffectWindowKind {
    InRounds,
    NextRounds,
};

class ContinuousEffect {
public:
    // ⚠️ 唯一构造入口（Effect 全参 + 时点/owner/duration）。旧的无 Effect 版本
    //    （`ContinuousEffect(int owner = -1)`）已删：它会把 effect_ 留成"logic 为空"
    //    的空壳，operator()/getEffectId() 全部静默失效——构造来源必须显式给 Effect。
    // 构造：持有 Effect（函数指针+参数）+ 运行时状态。
    // duration_rounds_ > 0 = 回合效果；<= 0 = 永久/一次性。
    ContinuousEffect(Effect e, State trigger, int owner, int duration, int registeredRound)
        : owner_(owner)
        , effect_(std::move(e))
        , trigger_state_(trigger)
        , registered_round_(registeredRound)
        , duration_rounds_(duration) {}

    virtual ~ContinuousEffect() = default;

    // 内联：执行内部 Effect 模板函数。isExpired 由调用方（TimedBucket::execute_at）先查，
    // 这里不再重复检查（避免头文件访问 ctx->roundCount 需要完整 BattleContext 类型）。
    virtual bool operator()(BattleContext* ctx) {
        if (!effect_.logic) {
            return false;
        }
        effect_.logic(ctx, effect_.args);
        return true;
    }
    virtual bool check(BattleContext* ctx) const { (void)ctx; return false; }
    virtual bool consume(BattleContext* ctx) { (void)ctx; return false; }
    State getTriggerState() const { return trigger_state_; }
    int owner() const { return owner_; }
    virtual bool isExpired(int currentRound) const {
        if (duration_rounds_ < 0) {
            return false;  // 永久效果
        }
        return currentRound - registered_round_ >= duration_rounds_;
    }
    int getEffectId() const { return effect_.logic ? effect_.id : -1; }
    bool isRoundEffect() const { return duration_rounds_ > 0; }
    // ★ 一次性动作节点（技能自身效果在桶里的执行条目，2026-09-27）：容器要求
    //   duration≥1 才能活到执行（left_round==0 归一化为 1），但它们是"本次动作的
    //   在途节点"、不是回合类效果——不计入 active_round_count（has_round_effects /
    //   "有无可断物"判定面），也不吃断回合的 force_expire。否则"消回合类效果"类
    //   效果（1237 等）会把自己技能的后继条目当回合类断掉（渎神 1237 断掉自家
    //   43/1814，2026-09-27 场景 137 实测）。真回合窗口 left_round>0 不受影响。
    bool isActionOneShot() const { return duration_rounds_ == 1 && effect_.left_round == 0; }
    int getEffectCategory() const { return effect_.logic ? effect_.id : -1; }
    Effect* getEffect() { return &effect_; }
    int getRegisteredRound() const { return registered_round_; }

    // 断回合用：把回合类条目立即置为过期（isExpired 在 currentRound 起为 true）。
    // 常驻条目（duration<=0）不可被断回合终结，调用方应先用 isRoundEffect 过滤。
    void force_expire(int currentRound) {
        if (duration_rounds_ > 0) {
            registered_round_ = currentRound - duration_rounds_;
        }
    }

    // 回合效果版本号 — 用于 O(1) 断回合/切换作废
    // 注册时从 BattleContext::round_effect_valid_id[owner] 复制。
    // 执行前比较 this->valid_id_ == ctx->round_effect_valid_id[owner]，
    // 不等说明该效果已被断回合/切换失效（仅 ON_STAGE 效果检查）。
    int valid_id_ = 0;

    // 来源标识：技能/魂印 ID。>0 时同源去重（同 key 覆盖旧效果）；0 = 不参与去重。
    int source_id_ = 0;
    // 作用域：ON_STAGE（切换作废）/ TEAM（切换保留，不可被清回合类作废）
    EffectScope scope_ = EffectScope::ON_STAGE;
    // 回合限一次：执行一次后由 TimedBucket::execute_at 移除（下回合重注册重新生效）。
    bool once_ = false;
    // 效果类别（GENERIC / ROUND / SKILL_EXEC）
    EffectKind kind_ = EffectKind::GENERIC;

protected:
    int owner_;
    Effect effect_;                    // 效果模板（函数指针+参数）
    State trigger_state_{};            // 由构造函数指定
    int registered_round_ = -1;        // 注册时的回合数
    int duration_rounds_ = -1;         // 持续回合数；>0 = 回合效果
};

#endif // CONTINUOUS_EFFECT_H
