#ifndef ICONTROLBLOCK_H
#define ICONTROLBLOCK_H

// FSM 与 IO 之间的**唯一**契约。
//
// 历史教训：这个头文件原来同时装着三样东西 —— FSM 的 IO 抽象、具体的 asio 控制块实现、
// 以及线协议结构（MessageHeader / Command / build_message）。结果"FSM 的 IO 抽象"被
// asio 类型和线格式同时绑死：想换传输层就得连 FSM 的头一起改。
//
// 现在这里只留下 FSM 真正调用的三个方法。线协议在 include/server/protocol.h，
// 具体实现在 server 层（Room）。
//
// 三个就是全部：全仓 `control_block_->` 的调用点只有 wait_for_input / async_write
// （on_fsm_paused 是本轮新增的输出时点）。原先接口上的 broadcast / broadcast_to /
// duplicate_context / log_operation / is_training_mode 全仓零调用，已删除。

#include <string>

class BattleContext;
class BattleFsm;

class IControlBlock {
public:
    virtual ~IControlBlock() = default;

    // FSM 需要输入：把 ctx 记为"等待输入"。数据到达后由控制块再次 post fsm->run(ctx)。
    // 契约：控制块必须先保证 ctx->m_buffer 已填好、ctx->is_empty == false，再唤醒 FSM。
    virtual void wait_for_input(BattleContext* ctx) = 0;

    // 给 player_id 发一段数据，发送完成后继续推进 ctx。
    // FSM 目前只用它回"这个输入非法，请重选"（7 个调用点全是错误文案）。
    // 实现方负责把裸文本包成协议帧 —— 裸文本直接写进流会破坏长度前缀成帧。
    virtual void async_write(int player_id, const std::string& data,
                             BattleContext* ctx, BattleFsm* fsm) = 0;

    // FSM 交出控制权（等待输入 / 单步停住 / 战斗结束）。
    // 控制块在**这里**把本次推进产生的事件带、快照、待输入提示发给客户端。
    //
    // 为什么输出集中在这一个时点：FSM 一次 run 可能连续推进几十个状态（一个回合 41 个时点），
    // 逐状态往外发既没意义又会让客户端半路渲染。让客户端拿到"这一段的完整过程 +
    // 结束时的权威状态"，再自己决定怎么播 —— 播哪些、怎么过渡是渲染层的事。
    virtual void on_fsm_paused(BattleContext* ctx) = 0;
};

#endif  // ICONTROLBLOCK_H
