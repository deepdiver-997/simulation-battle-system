#ifndef EVENT_CENTER_H
#define EVENT_CENTER_H

#include <algorithm>
#include <deque>
#include <functional>
#include <iostream>
#include <unordered_map>
#include <vector>

class BattleContext;

/**
 * EventType - 事件类型
 *
 * 事件是"发生了什么"的声明，由原语在成功路径末尾 emit。
 * 枚举值增长 = 数据扩展，不是新 struct。
 */
enum class EventType {
    EVENT_BREAK,             // 断回合成功（target = 被断方）
    EVENT_DEATH,             // 精灵死亡
    EVENT_CONTROLLED,        // 控场异常施加成功
    EVENT_ANOMALY_APPLIED,   // 任意异常施加成功
    EVENT_HIT,               // 攻击命中
    EVENT_TAKE_DAMAGE,       // 受到伤害结算后
    EVENT_SHIELD_BROKEN,     // 护盾被击破（护盾消失时触发）
    EVENT_ENTER_STAGE,       // 上场
    EVENT_SWAP,              // 换宠
    EVENT_OPPONENT_DEFEATED, // 击败对手
    EVENT_SKILL_INVALID,     // 技能无效/未命中（Skills::execute 中 emit，target = 对方）
    EVENT_ATTACK_BLOCKED,    // 攻击被拦下/归零（apply_resolved_damage 中 final<=0 时 emit）
    // 盔/威/封属**被结算**（真正生效 或 被穿）——RuleCenter::notify 每结算一条拦截条目就 emit。
    // actor = 挂载方(source_owner)，target = 被拦方(user)，amount = 该条目的 source_effect_id，
    // **grant_id = 授予句柄**（`seal_skill` 返回值），**blocked** = true 表示真正生效 / false 表示被穿。
    // 用途：带后续子句的盔的"触发成功则…"（2006「免疫成功则令对手全属性+1」、
    //   2270「触发成功则{X}%令对手{异常}」）——插件按 **grant_id** 精确匹配自己那一条盔。
    // ⚠️ 为什么被穿也要发：**被穿的盔必须能把它的监听器一起撤掉**。否则——
    //   盔A被穿（监听器留下）→ 之后盔B生效 → A/B 两个监听器都触发 → 子句多触发一次。
    //   插件收到**自己 grant_id** 的结算信号时，无论 blocked 与否都该自删；blocked=false 就不执行子句。
    EVENT_SKILL_ARMOR_RESOLVED,
    // 粉伤**结算到本体**（体力真的下降了）。amount = 实际扣血量。
    // ⚠️ 经过护罩抵消后没有剩余就不发——"有没有受到粉伤"在这里就是字面意思：体力降没降。
    //    要看护罩吃掉多少（L338 类2 的检测口径）在管线 DETECT 阶段读 `ctx->resolvedPink.absorbed`。
    // ⚠️ **多段粉每段各发一次**（每次 deal_pink_damage 一段）——阈值类检测（箫澈 2099「受到粉伤
    //    ≥300」）因此天然是"逐段判定"，不需要任何段计数逻辑。
    EVENT_TAKE_PINK_DAMAGE,
    // **体力归零原语**（force_hp_to_zero）触发：target = 被归零方、actor = 来源方、
    // amount = 归零前体力。秒杀族（通用特性·瞬杀 1-5 星、技能/魂印秒杀）的统一检测点——
    // 它**不是伤害**（护盾/护罩/减伤不参与），所以不走 EVENT_TAKE_DAMAGE。
    EVENT_HP_TO_ZERO,
    // **消逝**（vanish_spirit）：target = 被消逝方，slot = 槽位。
    // ⚠️ 与 EVENT_DEATH 是**两个事件**：消逝蕴含阵亡但**不是死亡**——死亡可复活、
    //    仍占位仍算背包/场下；消逝不可逆、从所有空间与位置基准剔除。
    //    监听击败/亡语类效果请只认 EVENT_DEATH，避免把消逝当死亡处理。
    EVENT_VANISH,
    // **能力等级变化**（`write_ability_level`：所有改能力等级的路径的**唯一落点**）。
    // target = 等级发生变化的一方（0/1）、stat_index = 能力下标（0=攻击…4=速度、5=体力）、
    // level_after = 变化后的等级、level_delta = 本次变化量（正=提升）。
    // ⚠️ **无 actor**：`stat_change` / `stat_drop` 等原语本身不带归因参数 → actor 恒 -1，
    //    检测方只能判"谁的等级变了"，判不出"被谁改的"（自身增益与对手弱化在此同形）。
    // 用途：**监测能力等级变化的窗口效果**——典型「每回合开始至战斗阶段结束时，若对手存在
    //    高于自身的能力等级则令其变为与自身相同」（混沌魔君索伦森 1011）。
    //    这类效果无法用"在某个时点注册一次"表达：同回合内对方会在**它自己的时点桶**里强化，
    //    而同一 State 的桶按 (owner 0 → owner 1) 定序跑（房主/挑战方判定同源问题）——
    //    先跑的一方看到的是旧等级，压制迟到一拍、**连带影响出手先后**。
    //    事件是唯一的"写后即知"通道 → 挂在本事件上的压制天然与时点定序无关。
    EVENT_STAT_CHANGED,
};

/**
 * BattleEvent - 事件载荷
 * actor = 发起方，target = 承受方。amount 按事件类型带语义：
 *   - EVENT_TAKE_DAMAGE / EVENT_HIT：伤害量（受高伤/受低伤判断用）
 *   - 其余事件：可忽略
 * payload 先最小化，随需要以数据扩展。
 */
struct BattleEvent {
    EventType type;
    int actor = -1;
    int target = -1;
    int amount = 0;
    // **emit 当时**的 FSM 时点（(int)State；kNoEventState = 未记录）。
    // ⚠️ 为什么要存：事件是"入队 + 晚 drain"的——handler 跑完（其间 generateState 已把
    //    currentState 推到下一状态）才派发给 watcher。回调里读 ctx->currentState 拿到的是
    //    **下一个**状态，判不出"这次伤害是在哪个时点造成的"（如"受到攻击伤害后"要区分
    //    攻击伤害时点 vs 粉伤时点）。emit 点当场把时点记进事件，回调读 ev.state 即可。
    int state = kNoEventState;
    // 规则票据的**授予句柄**（盔事件用；= `seal_skill` 的返回值）。非该族事件为 -1。
    // 用途：同 effect_id 的多条盔互相区分——监听器据此精确匹配"是不是我这条"。
    int grant_id = -1;
    // 盔事件专用：true = 盔**真正生效**（挡住了本次技能）；false = 被穿（penetrable + 穿盔凭证）。
    bool blocked = false;
    // ── 以下为死亡族事件（EVENT_DEATH）扩展字段（**追加在末尾**，别插在中间）──
    // 死亡发生的**槽位**（0..5）。用途：薇尔诗 2513「相邻/隔位精灵死亡」类效果要判
    //   "死的是哪一槽"——只给 side 判不出。非死亡事件为 -1。
    int slot = -1;
    // 本次倒下的**成因**（DefeatCause 的整数值）。用途：拦截器按成因过滤
    //   （官方 idx=339：消耗全部体力穿所有残留免死、但不穿复活）。非死亡事件为 0。
    int cause = 0;
    // ── 能力等级变化族（EVENT_STAT_CHANGED）扩展字段（**追加在末尾**）──
    int stat_index = -1;   // 能力下标（0=攻击 1=特攻 2=防御 3=特防 4=速度 5=体力）；非该族为 -1
    int level_after = 0;   // 变化**之后**的等级（检测方通常只关心"是不是正等级"）
    int level_delta = 0;   // 本次变化量（正=提升、负=下降）；便于"只认提升"的检测方早退
    static constexpr int kNoEventState = -999;
};

/**
 * EventWatcher - 事件监听器
 *
 * 注册方声明"当 type 事件发生时，运行 fn"。fn 有两种反应模式：
 * 1. 直接改 BattleContext（即时补偿，如被断反伤）——在 drain 点改值安全。
 * 2. registerEffect 注册到未来时点，等 FSM 自然推进（延迟补偿）。
 *
 * 生命周期：
 * - duration_rounds == 0 永久；>0 窗口（register_round 起算）。
 * - once == true 触发一次后自动移除（断回合补偿的语义）。
 */

// 监听器作用域（与 ContinuousEffect 的 EffectScope 语义一致，独立定义避免跨头耦合）
enum class WatcherScope {
    ON_STAGE,  // 当前场上精灵：切换作废
    TEAM,      // 全队绑定：切换保留，不可被清回合类作废
};

struct EventWatcher {
    EventType type;
    int owner;                                    // 谁注册的（回调里 self 视角）
    int register_round;
    int duration_rounds;                          // 0 = 永久
    bool once;
    WatcherScope scope_ = WatcherScope::ON_STAGE;
    int valid_id_ = 0;   // 注册时从 BattleContext::watcher_valid_id[owner] 复制，切换作废用
    std::function<void(BattleContext*, const BattleEvent&)> fn;
};

/**
 * EventCenter - 事件通道内核
 *
 * 投递策略（已定）：emit 只入队，绝不内联执行 watcher —— 从根上消除重入。
 * FSM 在"每个 State 的效果桶跑完后、推进下一个 State 前"统一 drain 投递。
 * 投递时机对状态机相对位置恒定，消除未知时序；桶迭代安全（drain 在桶之后）。
 *
 * 重入与循环：watcher 内再次 emit 合法（支持事件级联，如 断→击杀→死亡），
 * 进入下一"波"继续投递；一次 drain 最多投递 kMaxWaves 波。若超过波次上限，
 * 说明 watcher 无条件互相 emit 形成反馈环 —— 打印响亮警告并丢弃剩余事件
 * （不静默，便于定位）。不会死锁：波次上限保证有界。
 */
class EventCenter {
public:
    EventCenter() = default;
    EventCenter(const EventCenter&) = delete;
    EventCenter& operator=(const EventCenter&) = delete;

    /**
     * 注册事件监听。
     * @param owner           注册方（self 视角）
     * @param register_round  注册时的回合数（由 BattleContext 传入 roundCount）
     * @param duration_rounds 0 = 永久; >0 = 窗口
     * @param once            触发一次后自动移除
     * @return watcher_id     用于 remove_watcher 手动注销
     */
    int register_watcher(EventType type, int owner, int register_round, int duration_rounds,
                         bool once, std::function<void(BattleContext*, const BattleEvent&)> fn,
                         WatcherScope scope = WatcherScope::ON_STAGE,
                         int valid_id = 0) {
        const int id = next_id_++;
        watchers_.emplace(id, EventWatcher{type, owner, register_round, duration_rounds,
                                           once, scope, valid_id, std::move(fn)});
        by_type_[type].push_back(id);
        return id;
    }

    void remove_watcher(int watcher_id) {
        auto it = watchers_.find(watcher_id);
        if (it == watchers_.end()) {
            return;
        }
        const EventType type = it->second.type;
        watchers_.erase(it);
        auto bit = by_type_.find(type);
        if (bit == by_type_.end()) {
            return;
        }
        auto& ids = bit->second;
        ids.erase(std::remove(ids.begin(), ids.end(), watcher_id), ids.end());
        if (ids.empty()) {
            by_type_.erase(bit);
        }
    }

    /**
     * emit - 只入队，不执行任何代码。
     * 原语在成功路径末尾调用；投递发生在 FSM 的 drain 点。
     */
    void emit(const BattleEvent& event) {
        pending_.push_back(event);
    }

    /**
     * set_delivery_sink - 投递时把事件**同时抄一份**给外部（事件带录制用）。
     *
     * 为什么在投递处抄而不是 emit 处：emit 只入队，可能因为反馈环超波次被丢弃；
     * 抄在投递处，记下来的与 watcher 真正看到的完全一致。
     * 传 nullptr 关闭抄送。抄送是纯观测，不改变任何投递行为。
     */
    void set_delivery_sink(std::vector<BattleEvent>* sink) { sink_ = sink; }

    /**
     * drain - FSM 在 State 桶之后调用，统一投递待处理事件。
     * @param ctx          拥有本 EventCenter 的 BattleContext（传给 watcher fn）
     * @param current_round 当前回合（窗口过期判定）
     */
    void drain(BattleContext* ctx, int current_round) {
        drain(ctx, current_round, nullptr);
    }
    // 重载：额外传入监听器版本号（切换作废判定用；nullptr = 不检查切换作废）
    void drain(BattleContext* ctx, int current_round, const int* watcher_valid_id) {
        if (pending_.empty()) {
            return;
        }
        draining_ = true;
        constexpr int kMaxWaves = 8;   // 一次级联链（断→击杀→死亡）合理上限
        int waves = 0;
        while (!pending_.empty() && waves++ < kMaxWaves) {
            // 取当前波次；投递中 watcher 新 emit 的事件进入 pending_，形成下一波
            std::deque<BattleEvent> wave = std::move(pending_);
            pending_.clear();
            for (const BattleEvent& event : wave) {
                if (sink_ != nullptr) {
                    sink_->push_back(event);
                }
                deliver(ctx, current_round, event, watcher_valid_id);
            }
        }
        draining_ = false;
        if (!pending_.empty()) {
            // 波次上限被突破 → watcher 无条件互相 emit 形成反馈环。响亮报告，不静默丢弃。
            std::cerr << "[EventCenter] drain 超过 " << kMaxWaves
                      << " 波，疑似事件反馈环（watcher 无条件互相 emit）；丢弃 "
                      << pending_.size() << " 个待投递事件。请检查相关 watcher 实现。\n";
            pending_.clear();
        }
    }

    /**
     * cleanup - 移除窗口已过 或 被切换作废的 watcher。
     * 由 BattleContext::cleanup_expired_effects 在回合扣减点统一调用。
     */
    void cleanup(int current_round, const int* watcher_valid_id = nullptr) {
        for (auto it = watchers_.begin(); it != watchers_.end();) {
            const EventWatcher& watcher = it->second;
            const bool window_expired = watcher.duration_rounds > 0
                && current_round - watcher.register_round >= watcher.duration_rounds;
            const bool switch_invalidated = (watcher_valid_id != nullptr)
                && watcher.scope_ == WatcherScope::ON_STAGE
                && watcher.valid_id_ != watcher_valid_id[watcher.owner];
            if (window_expired || switch_invalidated) {
                const int id = it->first;
                const EventType type = watcher.type;
                it = watchers_.erase(it);
                auto bit = by_type_.find(type);
                if (bit == by_type_.end()) {
                    continue;
                }
                auto& ids = bit->second;
                ids.erase(std::remove(ids.begin(), ids.end(), id), ids.end());
                if (ids.empty()) {
                    by_type_.erase(bit);
                }
            } else {
                ++it;
            }
        }
    }

    void clear_all() {
        watchers_.clear();
        by_type_.clear();
        pending_.clear();
    }

    int watch_count(EventType type) const {
        auto it = by_type_.find(type);
        return it == by_type_.end() ? 0 : static_cast<int>(it->second.size());
    }

    bool has_watchers(EventType type) const {
        return watch_count(type) > 0;
    }

private:
    // 把单个事件投递给其类型的所有 watcher。
    // watcher_valid_id: 监听器版本号数组（切换作废判定）；nullptr = 不检查
    void deliver(BattleContext* ctx, int current_round, const BattleEvent& event,
                 const int* watcher_valid_id) {
        auto it = by_type_.find(event.type);
        if (it == by_type_.end()) {
            return;
        }
        // 复制 id 列表：投递过程中 watcher 可能增删
        const std::vector<int> ids = it->second;
        for (int wid : ids) {
            auto eit = watchers_.find(wid);
            if (eit == watchers_.end()) {
                continue;
            }
            EventWatcher& watcher = eit->second;
            if (watcher.duration_rounds > 0
                && current_round - watcher.register_round >= watcher.duration_rounds) {
                continue;  // 窗口已过，等待 cleanup 移除
            }
            // ON_STAGE 监听器：切换作废（valid_id 不匹配则跳过）
            if (watcher.scope_ == WatcherScope::ON_STAGE
                && watcher_valid_id != nullptr
                && watcher.valid_id_ != watcher_valid_id[watcher.owner]) {
                continue;
            }
            // ⚠️ once 提前拷贝：fn 内部可能自删（remove_watcher 自身 id）——删除后
            //    watcher 引用悬垂，不能再读其成员。"回调内自删"是"下一只/下一次"类
            //    监控的推荐写法（once=true 无法做"过滤后才算触发"）。
            const bool once = watcher.once;
            // ⚠️⚠️ **必须拷贝 fn 再调**：回调内自删会 erase 掉 watchers_ 里的条目，
            //    那个 std::function（连同它的**闭包存储**）当场被析构——而它正在执行。
            //    之后回调里再读任何捕获变量都是在读**已释放内存**（表现为捕获值与注册时
            //    不一致、行为随机）。拷贝一份再调，闭包就有了独立存储，自删才真的安全。
            //    （2026-09-14 踩到：effect 2006 的"盔生效→对手全属性+1"里 self/opp 读成垃圾。）
            std::function<void(BattleContext*, const BattleEvent&)> fn = watcher.fn;
            if (fn) {
                fn(ctx, event);
            }
            if (once) {
                remove_watcher(wid);
            }
        }
    }

    std::unordered_map<int, EventWatcher> watchers_;          // id -> watcher
    std::unordered_map<EventType, std::vector<int>> by_type_; // type -> watcher ids
    std::deque<BattleEvent> pending_;                         // 待投递事件队列
    std::vector<BattleEvent>* sink_ = nullptr;                // 投递抄送目标（事件带）
    bool draining_ = false;                                   // 是否正在 drain（日志/调试定位用）
    int next_id_ = 1;
};

#endif // EVENT_CENTER_H
