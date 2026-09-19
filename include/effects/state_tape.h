#ifndef STATE_TAPE_H
#define STATE_TAPE_H

// 事件带：FSM **每个时点**录一条采样，供服务端整段发给前端播放。
//
// 为什么不是"只采几个特殊时点"：State 枚举有 41 个时点，一个回合会顺序走完其中几十个
// （先后手各自的 行动开始/命中前/命中时/技能效果/伤害结算/行动后/行动结束/额外行动/死亡结算，
// 再加回合结束系列）。"技能造成伤害 → 魂印回血 → 粉伤再扣"这种过程，玩家要看的是每一步；
// 只给回合首尾两张快照，前端渲染不出可验证的过程，测试者也指不出"第 7 步算错了"。
//
// 一段采样 = 执行完某个时点之后那一刻的可观测状态 + 该时点内投递过的事件。
// 采样本身不参与任何判定，**纯只读观测**：录不录、录多细都不影响战斗结果。
// 因此默认关闭（tape_enabled），只有服务端对局才打开 —— 71 个场景用例不受影响。

#include <cstdint>
#include <vector>

#include <effects/event_center.h>
#include <fsm/state.h>

// 与 BattleContext::kAbilityLevelSlotCount 必须一致（battleContext.h 里有 static_assert 兜底）。
constexpr int kSampleLevelSlots = 6;

// 一个时点的采样。
struct StateSample {
    int seq = 0;    // 全局单调序号：客户端可以据此判断"有没有漏收"
    int state = 0;  // 刚执行完的时点（State 的整数值）
    int round = 0;
    int actor = -1;  // 该时点结束后的待输入方（-1 = 无人在等）

    // 双方**场上**精灵的体力。场下体力见快照 —— 事件带只跟踪场上变化，避免每时点搬整队。
    int hp[2] = {0, 0};
    int max_hp[2] = {0, 0};

    // 能力等级：-6..+6。必须逐槽带上 —— 等级变化不产生事件
    // （不是"发生了什么"而是"值变了"），只靠事件流前端看不出强化/弱化。
    int levels[2][kSampleLevelSlots] = {};

    // 伤害管线视图。pending = 本次待结算，resolved = 已结算（护盾/护罩抵消后）。
    int pending_damage = 0;
    int resolved_damage = 0;

    // 本时点内 drain 投递过的事件（含级联波次）。语义事件流，用于让前端知道"为什么变了"。
    std::vector<BattleEvent> events;
};

// 采样录制器。挂在 BattleContext 上，由 FSM 在每个时点调用。
class StateTape {
public:
    bool enabled() const { return enabled_; }
    void set_enabled(bool on) {
        enabled_ = on;
        if (!on) {
            samples_.clear();
            current_events_.clear();
        }
    }

    const std::vector<StateSample>& samples() const { return samples_; }
    int next_seq() const { return next_seq_; }

    // 一次 run 里可能推进几十个时点，采样攒满就整批发走，避免长时间战斗把内存吃掉。
    // FSM 在 on_fsm_paused 时取走 samples()，调用方负责 clear_delivered()。
    void clear_delivered() { samples_.clear(); }

    // EventCenter 的事件抄送目标（只在本时点录制期间非空）。
    std::vector<BattleEvent>* event_sink() { return enabled_ ? &current_events_ : nullptr; }

    // FSM 在"某个时点执行完 + 事件已 drain"之后调用。
    // 调用方保证 current_events_ 里是本时点投递的全部事件。
    void record(int state, int round, int actor, const int hp[2], const int max_hp[2],
                const int levels[2][kSampleLevelSlots], int pending_damage, int resolved_damage) {
        if (!enabled_) {
            current_events_.clear();
            return;
        }
        StateSample s;
        s.seq = ++next_seq_;
        s.state = state;
        s.round = round;
        s.actor = actor;
        for (int i = 0; i < 2; ++i) {
            s.hp[i] = hp[i];
            s.max_hp[i] = max_hp[i];
            for (int k = 0; k < kSampleLevelSlots; ++k) {
                s.levels[i][k] = levels[i][k];
            }
        }
        s.pending_damage = pending_damage;
        s.resolved_damage = resolved_damage;
        s.events = std::move(current_events_);
        current_events_.clear();
        samples_.push_back(std::move(s));
    }

    // 缓存条数上限：到达上限前必须被取走，否则说明取走路径断了（宁可丢带也不吃内存）。
    static constexpr std::size_t kMaxBufferedSamples = 4096;

    bool overflowed() const { return samples_.size() >= kMaxBufferedSamples; }

private:
    bool enabled_ = false;
    int next_seq_ = 0;
    std::vector<StateSample> samples_;
    std::vector<BattleEvent> current_events_;
};

#endif  // STATE_TAPE_H
