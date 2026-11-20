#ifndef FSM_STATE_H
#define FSM_STATE_H

// 战斗状态机时点枚举（独立头文件）。
// 依赖自由（仅 <cstdint>/<initializer_list>）：供 plugin_interface.h（插件注册魂印程序时点）、
// continuousEffect.h、skills.h 等安全 include，避免与 battleContext.h 成环。

#include <cstdint>
#include <initializer_list>

enum class State {
// 操作阶段
GAME_START = -1,                        // 游戏开始时
OPERATION_ENTER_EXIT_STAGE,        // 出战时/下场时
OPERATION_CHOOSE_SKILL_MEDICAMENT, // 选择技能/药剂
OPERATION_PROTECTION_MECHANISM_1,  // 保护机制1
OPERATION_ENTER_STAGE,             // 登场时

// 战斗阶段
BATTLE_ROUND_START,                        // 回合开始时
BATTLE_FIRST_MOVE_RIGHT,                   // 双方先手权结算
BATTLE_FIRST_ACTION_START,                 // 先手方行动开始时
BATTLE_FIRST_BEFORE_SKILL_HIT,             // 先手方技能命中前
BATTLE_FIRST_ON_SKILL_HIT,                 // 先手方技能命中时
BATTLE_FIRST_SKILL_EFFECT,                 // 先手方技能效果结算
BATTLE_FIRST_ATTACK_DAMAGE,                // 先手方攻击伤害结算
BATTLE_FIRST_AFTER_ACTION,                 // 先手方行动后
BATTLE_FIRST_ACTION_END,                   // 先手方行动结束时
BATTLE_FIRST_AFTER_ACTION_END,             // 先手方行动结束后
BATTLE_FIRST_EXTRA_ACTION,                 // 先手方额外行动
BATTLE_FIRST_MOVER_DEATH,                  // 先手方死亡结算
BATTLE_SECOND_ACTION_START,                // 后手方行动开始时
BATTLE_SECOND_BEFORE_SKILL_HIT,            // 后手方技能命中前
BATTLE_SECOND_ON_SKILL_HIT,                // 后手方技能命中时
BATTLE_SECOND_SKILL_EFFECT,                // 后手方技能效果结算
BATTLE_SECOND_ATTACK_DAMAGE,               // 后手方攻击伤害结算
BATTLE_SECOND_AFTER_ACTION,                // 后手方行动后
BATTLE_SECOND_ACTION_END,                  // 后手方行动结束时
BATTLE_SECOND_AFTER_ACTION_END,            // 后手方行动结束后
BATTLE_SECOND_EXTRA_ACTION,                // 后手方额外行动
BATTLE_ROUND_END,                          // 回合结束时
BATTLE_SECOND_MOVER_DEATH,                 // 后手方死亡结算
BATTLE_OLD_ROUND_END_1,                    // 旧版回合结束后1
BATTLE_ROUND_REDUCTION_ALL_ROUND_MINUS,    // 回合扣减点
BATTLE_ROUND_REDUCTION_NEW_ROUND_END,      // 新版回合结束后
BATTLE_OLD_ROUND_END_2,                    // 旧版回合结束后2
BATTLE_DEATH_TIMING,                       // 死亡时点
BATTLE_DEFEAT_STATUS,                      // 新版未被击败/被击败时
BATTLE_OPPONENT_DEFEAT_STATUS,             // 新版未击败对手/击败对手时
BATTLE_NEW_DEFEAT_MECHANISM,               // 新版保护机制
OPERATION_PROTECTION_MECHANISM_2,          // 保护机制2
BATTLE_AFTER_DEFEATED,                     // 被击败后
CHOOSE_AFTER_DEATH,                        // 死后选择
BATTLE_AFTER_DEFEATING_OPPONENT,           // 击败对手后
BATTLE_ROUND_COMPLETION,                   // 回合完成
FINISHED                                   // 战斗结束
};

//--- 时点覆盖 bitmap 辅助（免疫内核 coverage 用）---
// State 枚举含 GAME_START = -1，归一化 bit = (int)State + 1。
// 42 个状态（-1..40）落在 uint64_t 低 42 位内（bit0..41：GAME_START→bit0、
// ROUND_COMPLETION→bit40、FINISHED→bit41）。
// 【勘误 J323 2026-11-05】旧注「41 个状态（-1..39）落低 40 位」为笔误——枚举
// 实际到 FINISHED=40 共 42 项。勘误依据：S18 编译期 static_assert 钉死三位偏移
// + 运行时用例实证（test/scenario/scenario_7791_round_end_order_state_bits.cpp
// C 组：coverage_all 含 bit40/bit41）。
inline uint64_t state_coverage_bit(State s) {
    return 1ULL << (static_cast<int>(s) + 1);
}
inline uint64_t coverage_all() {
    return ~0ULL;  // 闭环：任何时点都覆盖
}
inline uint64_t coverage_union(std::initializer_list<State> states) {
    uint64_t mask = 0;
    for (State s : states) {
        mask |= state_coverage_bit(s);
    }
    return mask;
}

inline const char* state_name_cn(State state) {
    switch (state) {
        case State::GAME_START: return "游戏开始时";
        case State::OPERATION_ENTER_EXIT_STAGE: return "出战时/下场时";
        case State::OPERATION_CHOOSE_SKILL_MEDICAMENT: return "选择技能/药剂";
        case State::OPERATION_PROTECTION_MECHANISM_1: return "保护机制1";
        case State::OPERATION_ENTER_STAGE: return "登场时";
        case State::BATTLE_ROUND_START: return "回合开始时";
        case State::BATTLE_FIRST_MOVE_RIGHT: return "双方先手权结算";
        case State::BATTLE_FIRST_ACTION_START: return "先手方行动开始时";
        case State::BATTLE_FIRST_BEFORE_SKILL_HIT: return "先手方技能命中前";
        case State::BATTLE_FIRST_ON_SKILL_HIT: return "先手方技能命中时";
        case State::BATTLE_FIRST_SKILL_EFFECT: return "先手方技能效果结算";
        case State::BATTLE_FIRST_ATTACK_DAMAGE: return "先手方攻击伤害结算";
        case State::BATTLE_FIRST_AFTER_ACTION: return "先手方行动后";
        case State::BATTLE_FIRST_ACTION_END: return "先手方行动结束时";
        case State::BATTLE_FIRST_AFTER_ACTION_END: return "先手方行动结束后";
        case State::BATTLE_FIRST_EXTRA_ACTION: return "先手方额外行动";
        case State::BATTLE_FIRST_MOVER_DEATH: return "先手方死亡结算";
        case State::BATTLE_SECOND_ACTION_START: return "后手方行动开始时";
        case State::BATTLE_SECOND_BEFORE_SKILL_HIT: return "后手方技能命中前";
        case State::BATTLE_SECOND_ON_SKILL_HIT: return "后手方技能命中时";
        case State::BATTLE_SECOND_SKILL_EFFECT: return "后手方技能效果结算";
        case State::BATTLE_SECOND_ATTACK_DAMAGE: return "后手方攻击伤害结算";
        case State::BATTLE_SECOND_AFTER_ACTION: return "后手方行动后";
        case State::BATTLE_SECOND_ACTION_END: return "后手方行动结束时";
        case State::BATTLE_SECOND_AFTER_ACTION_END: return "后手方行动结束后";
        case State::BATTLE_SECOND_EXTRA_ACTION: return "后手方额外行动";
        case State::BATTLE_ROUND_END: return "回合结束时";
        case State::BATTLE_SECOND_MOVER_DEATH: return "后手方死亡结算";
        case State::BATTLE_OLD_ROUND_END_1: return "旧版回合结束后1";
        case State::BATTLE_ROUND_REDUCTION_ALL_ROUND_MINUS: return "回合扣减点";
        case State::BATTLE_ROUND_REDUCTION_NEW_ROUND_END: return "新版回合结束后";
        case State::BATTLE_OLD_ROUND_END_2: return "旧版回合结束后2";
        case State::BATTLE_DEATH_TIMING: return "死亡时点";
        case State::BATTLE_DEFEAT_STATUS: return "新版未被击败/被击败时";
        case State::BATTLE_OPPONENT_DEFEAT_STATUS: return "新版未击败对手/击败对手时";
        case State::BATTLE_NEW_DEFEAT_MECHANISM: return "新版保护机制";
        case State::OPERATION_PROTECTION_MECHANISM_2: return "保护机制2";
        case State::BATTLE_AFTER_DEFEATED: return "被击败后";
        case State::CHOOSE_AFTER_DEATH: return "死后选择";
        case State::BATTLE_AFTER_DEFEATING_OPPONENT: return "击败对手后";
        case State::BATTLE_ROUND_COMPLETION: return "回合完成";
        case State::FINISHED: return "战斗结束";
        default: return "未知状态";
    }
}

#endif // FSM_STATE_H
