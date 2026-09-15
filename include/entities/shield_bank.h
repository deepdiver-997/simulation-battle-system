#ifndef SHIELD_BANK_H
#define SHIELD_BANK_H

#include <array>

/**
 * Shield - 一条护盾记录
 *
 * 护盾机制在原作游戏里分多种来源且消耗有优先级：
 * - 魂印每回合刷新的盾、队友给的盾、道具盾……来源不同。
 * - 攻击时按 priority 从高到低消耗。
 * - 部分护盾有持续时间（N 回合后消失），部分破盾前持续。
 */
struct Shield {
    int priority = 0;    // 吸收优先级（数值越大越先被消耗）
    int quantity = 0;    // 剩余护盾值
    int source_id = 0;   // 来源标识（每回合刷新 / 队友 / 道具）——定位用
    int duration = 0;    // 剩余回合（0 = 破盾前持续）
};

/**
 * ShieldBank - 每只精灵的护盾槽
 *
 * 固定容量数组（n ≤ 8），无堆分配；吸收时线性扫最高优先级，实际 O(1)。
 * 选固定 struct 数组而非 priority_queue / 裸 int[]：
 * - priority_queue 无法按来源定位刷新/过期，且有堆开销；
 * - 裸 int[] 无法容纳同优先级的多来源盾，也没有来源身份。
 */
class ShieldBank {
public:
    static constexpr int kMaxShields = 8;

    // **魂印/专属盾的约定优先级** —— 高于一切普通来源（普通盾从 0 起往上排）。
    // 用法：魂印赋予的"登场盾"用这个优先级登记，于是与其他护盾**叠加时优先被消耗**。
    //
    // 为什么不需要给 ShieldBank 加新结构（用户 2026-09-15 问）：
    //   `{priority, quantity, source_id}` 三件套已经够了 ——
    //   · "优先消耗"= `priority`（吸收时按最高优先级扣，现成）；
    //   · "只对本盾生效"= `source_id`（条款只认自己那一笔，见 `quantity_of`）。
    //   再加一层"专属盾"的概念只会多一份要和这三件套同步的状态。
    // 约定：同一精灵的**魂印专属盾至多一个**（不同魂印/同魂印多次登场都用同一个 source_id +
    //   `refresh_source`，而不是并排叠好几个）——否则"不超过此护盾的数值"会指向哪一笔变得含糊。
    static constexpr int kSoulShieldPriority = 1000;

    /** 吸收 damage，从最高优先级开始扣；返回穿透（护盾没挡完的部分）。 */
    int absorb(int damage, int* broken_count = nullptr) {
        int broken = 0;
        while (damage > 0 && count_ > 0) {
            const int best = find_highest_priority_index();
            if (best < 0) {
                break;
            }
            Shield& s = slots_[best];
            if (s.quantity > damage) {
                s.quantity -= damage;
                damage = 0;
            } else {
                damage -= s.quantity;
                ++broken;
                remove_at(best);
            }
        }
        if (broken_count) {
            *broken_count = broken;
        }
        return damage;
    }

    void add(int priority, int quantity, int source_id, int duration) {
        if (count_ >= kMaxShields) {
            return;
        }
        slots_[count_++] = Shield{priority, quantity, source_id, duration};
    }

    /** 每回合刷新：找到 source_id 的盾，重置为 quantity（保留优先级/时长）。 */
    void refresh_source(int source_id, int quantity) {
        for (int i = 0; i < count_; ++i) {
            if (slots_[i].source_id == source_id) {
                slots_[i].quantity = quantity;
                return;
            }
        }
    }

    /** 回合末：duration 减一，归零的移除。 */
    void tick_duration() {
        for (int i = count_ - 1; i >= 0; --i) {
            if (slots_[i].duration > 0 && --slots_[i].duration == 0) {
                remove_at(i);
            }
        }
    }

    void clear() {
        count_ = 0;
    }

    int total() const {
        int t = 0;
        for (int i = 0; i < count_; ++i) {
            t += slots_[i].quantity;
        }
        return t;
    }

    /** 按**来源**读剩余护盾值（同 source_id 的多笔求和；没有则 0）。
     *  用途："不超过**此**护盾的数值"这类**只认自己那一笔**的条款——不能读 `total()`，
     *  否则别的来源叠上来会把上限一起抬高。 */
    int quantity_of(int source_id) const {
        int t = 0;
        for (int i = 0; i < count_; ++i) {
            if (slots_[i].source_id == source_id) {
                t += slots_[i].quantity;
            }
        }
        return t;
    }

    bool empty() const { return count_ == 0; }
    int count() const { return count_; }

private:
    int find_highest_priority_index() const {
        int best = -1;
        for (int i = 0; i < count_; ++i) {
            if (best < 0 || slots_[i].priority > slots_[best].priority) {
                best = i;
            }
        }
        return best;
    }

    void remove_at(int index) {
        for (int i = index; i < count_ - 1; ++i) {
            slots_[i] = slots_[i + 1];
        }
        --count_;
    }

    std::array<Shield, kMaxShields> slots_{};
    int count_ = 0;
};

#endif // SHIELD_BANK_H
