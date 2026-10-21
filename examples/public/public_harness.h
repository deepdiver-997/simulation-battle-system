// examples/public/public_harness.h —— 公开示例用的最小对局驱动
//
// 与本仓私有测试基架（test/scenario/scenario_harness.h）**同思路但更薄**：
// 这里只做三件事——①断言计数与失败退出；②两只精灵的最小对局；③操作提交。
// 引擎的 SeerRobot / BattleContext / BattleFsm 本身就是公开内核 API，直接用。
//
// 关键差异（相对私有基架）：**不查官方数据库**——技能与面板全部手工装配
// （见 bare_pet.h），所以公开检出无需任何数据文件即可运行。
#ifndef EXAMPLES_PUBLIC_PUBLIC_HARNESS_H
#define EXAMPLES_PUBLIC_PUBLIC_HARNESS_H

#include <array>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include <entities/elf-pet.h>
#include <entities/seer-robot.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <fsm/iControlBlock.h>
#include <memory>

namespace examples {

// ── 断言（失败即非零退出，CI 可直接用）────────────────────────────────────
inline int& check_count() {
    static int n = 0;
    return n;
}

inline void check_eq(long long actual, long long expected, const std::string& what) {
    ++check_count();
    if (actual != expected) {
        std::cerr << "[CHECK FAILED] " << what << "：实际 " << actual << "，期望 " << expected
                  << std::endl;
        std::exit(1);
    }
}

inline void check_true(bool ok, const std::string& what) {
    ++check_count();
    if (!ok) {
        std::cerr << "[CHECK FAILED] " << what << std::endl;
        std::exit(1);
    }
}

// ── 最小对局 ───────────────────────────────────────────────────────────────
class Battle {
public:
    Battle(const std::array<ElfPet, 6>& party0, const std::array<ElfPet, 6>& party1)
        : robots_{SeerRobot(party0, MedicineStock{}), SeerRobot(party1, MedicineStock{})} {
        ctx_ = std::make_unique<BattleContext>(&cb_, robots_.data(), /*pvp_battle=*/false);
        fsm_ = std::make_unique<BattleFsm>(/*enable_debug=*/false);
    }

    void start() { fsm_->run(ctx_.get()); }

    // 提交一次操作：action = BattleFsm::ActionType::NONE（空过）或 SELECT_SKILL（用第 index 招）
    void submit(int player, BattleFsm::ActionType action, int index) {
        const int buf[4] = {player, static_cast<int>(action), index, 0};
        ctx_->m_buffer.assign(reinterpret_cast<const char*>(buf),
                              reinterpret_cast<const char*>(buf) + 4 * sizeof(int));
        ctx_->is_empty = false;
        fsm_->run(ctx_.get());
    }

    // 双方都空过，驱动一整个回合
    void idle_round() {
        submit(0, BattleFsm::ActionType::NONE, -1);
        submit(1, BattleFsm::ActionType::NONE, -1);
    }

    BattleContext& ctx() { return *ctx_; }

private:
    struct NoopControl : IControlBlock {
        void wait_for_input(BattleContext*) override {}
        void async_write(int, const std::string&, BattleContext*, BattleFsm*) override {}
        void on_fsm_paused(BattleContext*) override {}
    } cb_;

    std::array<SeerRobot, 2> robots_;
    std::unique_ptr<BattleContext> ctx_;
    std::unique_ptr<BattleFsm> fsm_;
};

}  // namespace examples

#endif  // EXAMPLES_PUBLIC_PUBLIC_HARNESS_H
