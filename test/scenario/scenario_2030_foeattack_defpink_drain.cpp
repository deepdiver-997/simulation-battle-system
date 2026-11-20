// 场景 2030 —— J109 双防粉/吸取窗族（2 宏，moves_lib/j_foeattack_defpink_drain.cpp）
//（2026-10-03，worktree ../agents/j109；号段 2030-2039 → T2030-T2031 两块）
//
// 断言口径：
//   - 944 = 泰坦结界 26348 [48,47,944]|[4,4,4,40]（48/47 自侧免票良性已注册；
//     944=[4,40]）：make_pet 双防视图和 200 → 40% 粉 = 80/次对手攻击。
//     R1 自身武装（对手同回合拍打即触发：对手掉 80——自身承红 [58,69] 另计）；
//     R2 对手再拍打 → 累计 160；R3 对手用属性技（泰坦结界再上：used_attribute 门
//     → 无粉，对手 hp 持平）；
//   - 924 = 智能形变 35849 [924]|[2,3]（特120 纯链）：NextRounds(2) R2-R3 窗——
//     R2 自身拍打（攻击技）→ 吸对手 1/3 最大体力 = 33333（自身 hp=max/2 预设承接
//     回吸可见：gain == 33333）；R4 窗过期 → 无吸（自身 hp 持平、对手仅红 1~2）；
//   - 对手动作仅拍打/属性技两种，承伤面唯一来源为本族与拍打。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa  = 10004;   // 拍打
constexpr int kTaiTan = 26348;   // 泰坦结界[48,47,944]|[4,4,4,40]：属性技（4回合窗·双防和40%粉）
constexpr int kZhiNeng = 35849;  // 智能形变[924]|[2,3]：特120（下2回合攻击技吸血1/3）

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
    ElfPet p = make_pet(2, "对手", skills, 100, 200, kMaxHp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T2030 944 对手攻击时受双防和 40% 粉伤（泰坦结界 [4,40]）══
    {
        ScenarioBattle b(make_party(self_pet({kTaiTan, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kTaiTan, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 自身武装
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);   // R1 对手拍打（触发）
        b.drive();
        int foe_loss = kMaxHp - ctx.getPet(1).hp;
        CHECK(foe_loss >= 80 && foe_loss <= 80,
              "T2030 944：R1 对手攻击触发 80 粉（实测 " + std::to_string(foe_loss) + "）");
        int prev = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 自身拍打
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 对手拍打（再触发）
        b.drive();
        const int delta_r2 = prev - ctx.getPet(1).hp;
        CHECK(delta_r2 >= 80 && delta_r2 <= 82,
              "T2030 944：R2 再触发 80 粉（实测 " + std::to_string(delta_r2) +
                  "，含自身拍打红伤 1~2）");
        prev = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R3 自身拍打
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R3 对手属性技（不触发）
        b.drive();
        const int delta_r3 = prev - ctx.getPet(1).hp;
        CHECK(delta_r3 >= 0 && delta_r3 <= 2,
              "T2030 944：R3 对手属性技不触发（实测 " + std::to_string(delta_r3) +
                  "，仅自身拍打红伤 1~2）");
        std::cout << "  [T2030] 944 双防粉窗 通过\n";
    }

    // ══ T2031 924 下 2 回合攻击技吸血 1/3（智能形变 [2,3]）══
    {
        ScenarioBattle b(make_party(self_pet({kZhiNeng, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 武装（当次不吸）
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        ctx.getPet(0).hp = kMaxHp / 2;   // 预扣血承接回吸
        const int self_hp0 = ctx.getPet(0).hp;
        const int foe_hp0 = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 拍打（触发）
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        const int self_gain = ctx.getPet(0).hp - self_hp0;
        const int foe_loss = foe_hp0 - ctx.getPet(1).hp;
        CHECK_EQ(self_gain, kMaxHp / 3,
                 "T2031 924：R2 攻击技吸血 1/3（实测回 " + std::to_string(self_gain) + "）");
        CHECK(foe_loss >= kMaxHp / 3 && foe_loss <= kMaxHp / 3 + 2,
              "T2031 924：对手掉吸取量+红伤（实测 " + std::to_string(foe_loss) + "）");
        // R3 仍在 2 回合窗内 → 再吸 33333
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R3 拍打
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        const int self_hp1 = ctx.getPet(0).hp;
        const int foe_hp1 = ctx.getPet(1).hp;
        // R4 窗过期 → 无吸血
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R4 拍打
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(0).hp - self_hp1, 0,
                 "T2031 924：R4 窗过期无吸血（实测回 " +
                     std::to_string(ctx.getPet(0).hp - self_hp1) + "）");
        CHECK(foe_hp1 - ctx.getPet(1).hp <= 2,
              "T2031 924：R4 对手仅红伤 1~2（实测 " +
                  std::to_string(foe_hp1 - ctx.getPet(1).hp) + "）");
        std::cout << "  [T2031] 924 吸取窗 通过\n";
    }

    std::cout << "scenario_2030 PASS (J109 双防粉/吸取窗族 944/924)" << std::endl;
    return 0;
}
