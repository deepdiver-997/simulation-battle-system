#ifndef FSM_BATTLE_MUTATE_H
#define FSM_BATTLE_MUTATE_H

#include <string>

#include <fsm/battleContext.h>

// 调试手术原语（对战调试台二期，2026-09-26）。
//
// 面向白名单的"运行时状态插入"：客户端不直接覆盖 context 字段，只发具名操作，
// 由这里走引擎的存储与校验落地（钳制/幂等/合法性），避免把不变量改破。
// 典型用途：泊车在等输入（或逐步/断点停下）时，给在场位上异常、扣 PP、
// 挂死亡印记……然后继续推进，验证目标机制的反应。
//
// 与插件原语（CoreApi / apply_anomaly 等）的差异：这里**绕过免疫/抗性/掷点**
// 直写权威存储——手术语义就是要"无视对抗地摆放状态"；需要走对抗判定的实验
// 请用技能/魂印正常触发。
//
// 线程约定：Room 在 FSM 泊车（waiting_）时调用，并持 context_->run_mutex 与
// 可能的推进任务互斥（见 Room::debug_mutate）。
namespace battle_mutate {

struct Outcome {
    bool ok = false;
    std::string note;   // 成功时给前端的操作回执（含前后值）
    std::string err;    // 失败原因
};

// 场上体力手术：value 钳到 [0, maxHp]。注意 hp=0 是裸状态，不走 defeat_pet
// 漏斗（免死/复活/击败时点不触发）——想正常击杀请走对战本身。
Outcome set_hp(BattleContext& ctx, int side, int slot, int value);

// 技能 PP 手术：value 钳到 [0, maxPP]。
Outcome set_pp(BattleContext& ctx, int side, int slot, int skill, int value);

// 能力等级手术：stat 0..5（攻/特攻/防/特防/速/命中），value 钳到 [-6, 6]。
Outcome set_level(BattleContext& ctx, int side, int stat, int value);

// 异常手术：直写 abnormal_status_end_round（= roundCount + rounds），绕过
// 免疫/抗性/转化/时长钳制。rounds ≥ 1。
Outcome apply_anomaly(BattleContext& ctx, int side, int id, int rounds);

// 解异常：id<0 = 解掉该方全部异常。
Outcome cure_anomaly(BattleContext& ctx, int side, int id);

// 挂死亡印记（消耗全部体力）：双方取**当前在场位**；复用引擎
// attach_hp_consume_mark（幂等 + 目标已倒拒绝 + 结算节点自动注册）。
Outcome attach_mark(BattleContext& ctx, int src_side, int dst_side);

// 清死亡印记：移除以该方**当前在场位**为目标的印记（模拟"切走解除"的手术版）。
Outcome clear_marks(BattleContext& ctx, int side);

}  // namespace battle_mutate

#endif  // FSM_BATTLE_MUTATE_H
