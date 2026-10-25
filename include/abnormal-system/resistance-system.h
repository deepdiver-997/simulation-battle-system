#include <utils/sbs_rand.h>
#ifndef RESISTANCE_SYSTEM_H
#define RESISTANCE_SYSTEM_H

#include <vector>
#include <cstdlib>
#include <ctime>
#include <abnormal-system/abnormal-types.h>

/**
 * ResistanceSystem - 精灵异常抗性系统
 *
 * 每只精灵可以通过抗性训练获得对特定异常状态的百分比抗性。
 * 抗性分为三类：
 *   - control:  控制类异常抗性（麻痹、害怕、疲惫等）
 *   - uncontrol: 非控制类（弱化类）异常抗性（中毒、烧伤、冻伤等）
 *   - complete_immunity: 全异常免疫百分比
 *
 * 抗性值范围 0~100，表示免疫该异常的概率百分比。
 */

// 抗性槽位数量
constexpr int kResistanceSlotCount = 3;

class ResistanceSystem {
public:
    ResistanceSystem() {
        for (int i = 0; i < kResistanceSlotCount; ++i) {
            control[i] = {0, 0};
            uncontrol[i] = {0, 0};
        }
        complete_immunity = 0;
    }

    // --- 控制类抗性 ---

    /**
     * 设置控制类抗性
     * @param newControl 三个抗性槽位 {anomaly_id, percent}
     *   - anomaly_id: 对应的异常状态 ID
     *   - percent: 抗性百分比 (0~100)
     * @return 设置是否合法（百分比和不超过 100，ID 有效）
     */
    bool setControlResistance(const std::pair<int, int> newControl[kResistanceSlotCount]) {
        int total = 0;
        for (int i = 0; i < kResistanceSlotCount; ++i) {
            int anomaly_id = newControl[i].first;
            int percent = newControl[i].second;
            if (percent < 0 || percent > 100) return false;
            if (!is_valid_abnormal_status_id(anomaly_id)) return false;
            total += percent;
            if (total > 100) return false;
            control[i] = newControl[i];
        }
        return true;
    }

    /**
     * 设置单个控制类抗性槽位
     */
    bool setControlSlot(int slot, int anomaly_id, int percent) {
        if (slot < 0 || slot >= kResistanceSlotCount) return false;
        if (percent < 0 || percent > 100) return false;
        if (!is_valid_abnormal_status_id(anomaly_id)) return false;
        control[slot] = {anomaly_id, percent};
        return true;
    }

    // --- 非控制类（弱化类）抗性 ---

    bool setUncontrolResistance(const std::pair<int, int> newUncontrol[kResistanceSlotCount]) {
        int total = 0;
        for (int i = 0; i < kResistanceSlotCount; ++i) {
            int anomaly_id = newUncontrol[i].first;
            int percent = newUncontrol[i].second;
            if (percent < 0 || percent > 100) return false;
            if (!is_valid_abnormal_status_id(anomaly_id)) return false;
            total += percent;
            if (total > 100) return false;
            uncontrol[i] = newUncontrol[i];
        }
        return true;
    }

    bool setUncontrolSlot(int slot, int anomaly_id, int percent) {
        if (slot < 0 || slot >= kResistanceSlotCount) return false;
        if (percent < 0 || percent > 100) return false;
        if (!is_valid_abnormal_status_id(anomaly_id)) return false;
        uncontrol[slot] = {anomaly_id, percent};
        return true;
    }

    // --- 全异常免疫 ---

    void setCompleteImmunity(int percent) {
        if (percent < 0 || percent > 100) return;
        complete_immunity = percent;
    }

    int getCompleteImmunity() const { return complete_immunity; }

    // --- 抗性判定 ---

    /**
     * 检查是否抵抗指定异常状态
     * @param anomaly_id 异常状态 ID
     * @return true 表示本次抵抗成功（异常不施加）
     *
     * 判定顺序：
     *   1. 全异常免疫百分比
     *   2. 该异常属于控制类 → 查控制类抗性槽位
     *   3. 该异常属于非控制类 → 查非控制类抗性槽位
     */
    bool isResistantTo(int anomaly_id) const {
        if (!is_valid_abnormal_status_id(anomaly_id)) return false;

        // 全异常免疫
        if (complete_immunity > 0) {
            if (sbs_rand() % 100 < complete_immunity) {
                return true;
            }
        }

        AbnormalStatusId id = static_cast<AbnormalStatusId>(anomaly_id);
        bool is_control = is_control_abnormal_status(id);

        const auto& slots = is_control ? control : uncontrol;
        for (int i = 0; i < kResistanceSlotCount; ++i) {
            if (slots[i].first == anomaly_id && slots[i].second > 0) {
                return sbs_rand() % 100 < slots[i].second;
            }
        }

        return false;
    }

    /**
     * 批量检查：对当前活跃的异常状态列表进行抗性判定
     * @param active_anomaly_ids 当前活跃的异常状态 ID 列表
     * @return 被抵抗的异常 ID 列表
     */
    std::vector<int> checkResistedAnomalies(const std::vector<int>& active_anomaly_ids) const {
        std::vector<int> resisted;
        for (int aid : active_anomaly_ids) {
            if (isResistantTo(aid)) {
                resisted.push_back(aid);
            }
        }
        return resisted;
    }

    // --- 查询 ---

    std::pair<int, int> getControlSlot(int slot) const {
        if (slot < 0 || slot >= kResistanceSlotCount) return {0, 0};
        return control[slot];
    }

    std::pair<int, int> getUncontrolSlot(int slot) const {
        if (slot < 0 || slot >= kResistanceSlotCount) return {0, 0};
        return uncontrol[slot];
    }

private:
    std::pair<int, int> control[kResistanceSlotCount];     // 控制类抗性
    std::pair<int, int> uncontrol[kResistanceSlotCount];   // 非控制类（弱化类）抗性
    int complete_immunity;                                  // 全异常免疫百分比 (0~100)
};

#endif // RESISTANCE_SYSTEM_H
