#include <utils/sbs_rand.h>
// 场景 070：技能批 5 —— 状态窗口族（2026-09-17）。
//
// 口径来源：effect_info 模板原文（数据库为准）：
//   - 32  使用后{0}回合攻击击中对象要害概率增加1/16（蓄气 20039：args [5]）
//   - 408 {0}回合内每回合所受的伤害减少{1}（大气护盾 21720：args [3,100]，点数减伤）
//   - 426 {0}回合内每回合{1}%闪避对手攻击（暗尘弥散 21878：args [2,50]）
// in_rounds 首回合生效：效果体立即写当回合 ws + ROUND_START 窗口 tick 供后续回合重写；
// 408 走管线 REDUCE_FLAT 条目 + roundCount 半开区间（525 同款）。
//
// 验证点：
//   C1 32：base 15/16(93.75%) + 6.25 = 100% → 下一回合攻击**必定**暴击（加算通道钉死）
//   R1 408：先手开窗当回合即减伤 100 点；窗口外（第 4 回合）不再减
//   D1 426：当回合 dodge_rate=0.5；窗口过期后归零
#include <cassert>
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

constexpr int kPaiDa = 10004;      // 拍打：物攻 40
constexpr int k32 = 20039;         // 蓄气：[32] args[5]（acc --，必中）
constexpr int k408 = 21720;        // 大气护盾：[408] args[3,100]（必中）
constexpr int k426 = 21878;        // 暗尘弥散：[426] args[2,50]（必中）

using scenario::ScenarioBattle;

ScenarioBattle make_battle(const ElfPet& p0, const ElfPet& p1) {
    sbs_srand(20260916);
    return ScenarioBattle(scenario::make_party(p0), scenario::make_party(p1));
}

void round_of(ScenarioBattle& b, int idx0 = 0, int idx1 = 0) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, idx0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, idx1);
    b.drive();
}

ElfPet make_side(int id, const char* name, const std::array<int, 5>& skills, int speed, int atk) {
    return scenario::make_pet(id, name, skills, speed, atk, 4000);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== C1 effect 32：暴击加算通道 ==========
    {
        // 攻方速度 200 先手；回合 1 蓄气（开 5 回合窗），回合 2 拍打（攻击判定暴击）。
        // base 15/16 = 93.75%，+6.25 → 100% → `rate >= 100` 分支**必暴**（确定性断言）。
        auto atk_pet = make_side(0, "攻方", {k32, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 200, 200);
        scenario::set_crit_sixteenths(atk_pet, 15);
        auto def_pet = make_side(1, "守方", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100, 100);
        ScenarioBattle b = make_battle(atk_pet, def_pet);
        b.start();
        round_of(b, /*idx0=*/0);   // 蓄气：当回合立即写 + 窗口 tick
        CHECK_EQ(b.ctx().ws.crit_rate_add[0], 6.25f, "C1) 蓄气当回合 crit_rate_add 应 +6.25");
        round_of(b, /*idx0=*/1);   // 拍打：窗口内（tick 已在 ROUND_START 重写）
        CHECK_EQ(b.ctx().ws.crit_rate_add[0], 6.25f, "C1) 窗口内每回合重写 +6.25");
        CHECK_EQ(b.ctx().crit_happened[0], true, "C1) 93.75+6.25=100 → 本回合攻击必定暴击");
        std::cout << "C1 PASS 32 暴击加算（base 93.75 + 6.25 → 必暴）\n";
    }

    //========== R1 effect 408：点数减伤窗口 ==========
    {
        // 守方速度 300（先手）。**只在第 1 回合开盾**，之后改甩蓄气（零 rand 消耗的属性技）
        // → 与对照组逐回合 rand 流镜像，任何回合的伤害差都恰好 = 护盾减伤（或 0 = 已过期）。
        // ⚠️ 两场必须**顺序**跑完（make_battle 各自重播种）；交替驱动会共享同一条全局 rand 流。
        // ⚠️ 守方若每回合都重开盾，窗口会一直被刷新（"重复使用=刷新"），永远测不到过期。
        auto atk_pet = make_side(0, "攻方", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100, 500);
        int dmg_b[6] = {0}, dmg_c[6] = {0};
        {
            ScenarioBattle b = make_battle(
                atk_pet, make_side(1, "守方", {k408, k32, k32, k32, k32}, 300, 100));
            b.start();
            int hp = b.pet(1).hp;
            for (int r = 1; r <= 5; ++r) {
                round_of(b, 0, r == 1 ? 0 : 1);   // 守方：第 1 回合开盾，之后蓄气
                dmg_b[r] = hp - b.pet(1).hp;
                hp = b.pet(1).hp;
            }
        }
        {
            ScenarioBattle c = make_battle(
                atk_pet, make_side(1, "对照", {k32, k32, k32, k32, k32}, 300, 100));
            c.start();
            int hp = c.pet(1).hp;
            for (int r = 1; r <= 5; ++r) {
                round_of(c, 0, 0);                // 对照：守方全程蓄气垫位
                dmg_c[r] = hp - c.pet(1).hp;
                hp = c.pet(1).hp;
            }
        }
        for (int r = 1; r <= 5; ++r) {
            if (r <= 3) {   // in_rounds 先手开窗：[R, R+N) = 回合 1~3
                CHECK_EQ(dmg_c[r] - dmg_b[r], 100,
                         "R1) 窗口内（第 " + std::to_string(r) + " 回合）伤害应恰好 -100");
            } else {
                CHECK_EQ(dmg_b[r], dmg_c[r],
                         "R1) 窗口外（第 " + std::to_string(r) + " 回合）应恢复裸伤");
            }
        }
        std::cout << "R1 PASS 408 减伤窗口（第 1~3 回合 -100，第 4 回合起恢复）\n";
    }

    //========== D1 effect 426：闪避窗口 ==========
    {
        // 守方速度 300 先手开窗（50% 闪避）→ 攻方同回合命中公式读 dodge_rate。
        auto def_pet = make_side(1, "守方", {k426, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300, 100);
        auto atk_pet = make_side(0, "攻方", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100, 200);
        ScenarioBattle b = make_battle(atk_pet, def_pet);
        b.start();
        round_of(b, 0, 0);
        CHECK_EQ(b.ctx().ws.dodge_rate[1], 0.5f, "D1) 当回合 dodge_rate 应为 50%");
        // 窗口 2 回合：第 2 回合 tick 重写仍 0.5
        round_of(b, 0, 1);
        CHECK_EQ(b.ctx().ws.dodge_rate[1], 0.5f, "D1) 窗口内第 2 回合仍 50%");
        // 第 3 回合（过期）→ memset 清零且无 tick 重写
        round_of(b, 0, 1);
        CHECK_EQ(b.ctx().ws.dodge_rate[1], 0.0f, "D1) 窗口过期后归零");
        std::cout << "D1 PASS 426 闪避窗口（0.5 → 0.5 → 过期归零）\n";
    }

    std::cout << "\n场景 070 全部通过：技能批 5（32/408/426）✓" << std::endl;
    return 0;
}
