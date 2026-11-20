// 场景 1810 —— J92 百分比窗族（1 宏，moves_lib/j_percent_window.cpp）
//（2026-10-03，worktree ../agents/j92；号段 1810-1819 → T1810 一块）
//
// 断言口径：
//   - 载体皇权在握 25597 [1001,57,1213,1011]|[1,4,3,4,3,3,60]：属性技零伤害；
//     ⚠️ 参差载体（57 args_num=2 → 1213 实际切片 [4,3]=4 回合·1/3，尾 [3,60] 未绑定，
//     记档）——99999/3 = 33333 精确；R2 增量 = 33333+拍打红伤 1~2；
//   - 对手全程 NONE（foe hp 差分来源 = 1213 百分比伤 + 拍打红伤 1~2）；
//   - 链上 1001/57/1011 全自侧效果（对手 NONE 下零干扰）。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;    // 拍打
constexpr int kWangQuan = 25597; // 皇权在握[1001,57,1213,1011]|[1,4,3,4,3,3,60]：属性技（3回合窗·1/60）

constexpr int kMaxHp = 99999;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills) {
    ElfPet p = make_pet(1, "己方", skills, 300, 1, kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills) {
    ElfPet p = make_pet(2, "对手", skills, 100, 1, kMaxHp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1810 1213 每回合用技附加最大体力 1/60 百分比伤（皇权在握 [3,60]）══
    {
        ScenarioBattle b(make_party(self_pet({kWangQuan, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 武装（当次不附加）
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(kMaxHp - ctx.getPet(1).hp, 0,
                 "T1810 前置：R1 载体当次不附加（对手满血，实测掉 " +
                     std::to_string(kMaxHp - ctx.getPet(1).hp) + "）");
        const int hp0 = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 拍打（用技）
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        const int delta_r2 = hp0 - ctx.getPet(1).hp;
        CHECK(delta_r2 >= 33334 && delta_r2 <= 33335,
              "T1810 1213：R2 附加 1/3 最大体力（实测 " + std::to_string(delta_r2) +
                  "，=33333+拍打红伤 1~2）");
        std::cout << "  [T1810] 1213 百分比窗 通过\n";
    }

    std::cout << "scenario_1810 PASS (J92 百分比窗族 1213)" << std::endl;
    return 0;
}
