// 场景 1390 —— J53 条件先制族（3 宏，moves_lib/j_cond_priority.cpp）
//（2026-10-02，worktree ../agents/j53；号段 1390-1399 → T1390-T1392 三块）
//
// 断言口径（全走 skills.cpp 选择期路由 2026-10-02 加 454/1484/1487 三行，2298/779/
// 1860 同款）：先制比较只看 ws.preemptive_level（基值先制已在 on_selected 折入，
// battleFsm 不再读 skill.priority）——载体 moves.priority 全 0、无魂印/特性，等级
// 值即插件净贡献；速度 100 vs 300 + 空等级 = 稳定对照。
//   - 454"同一 hp 档位双载体分档"法钉死双参：hp=40%max 时屠戮之刃 [2,2] 触发
//     （40×2<100，+2 翻盘）、狂暴撕咬 [3,1] 不触发（40×3≥100）；再以 hp=30% 钉
//     狂暴撕咬触发（+1）；边界 hp=50% 严格低于不触发（1397 整数直乘口径）；
//   - 1484 = 454 零参定档（denom=2/bonus=1）：40% 触发 / 60% 不触发 / 50% 边界；
//   - 1487 护盾账直添（shield_bank_.add，J26 口径）触发 / 无盾对照；第二纯链
//     载体岩盾投掷复验（护罩是另一本账，不在本口径）。
//   - 兄弟链甄别（并行链激活陷阱防线，速查 §五）：屠戮之刃/狂暴撕咬链上 33=
//     消除对手提升（对手无提升=哑）；木心归真链上 1237=PP 归零+清场（命中后
//     结算，选择期无扰动）；扬尘地冲/岩盾投掷纯链零参。所有载体单回合块内断言，
//     rand 序不跨块。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <entities/shield_bank.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa    = 10004;  // 拍打：物40，priority 0
constexpr int kTuLu     = 15984;  // 屠戮之刃 [33,454]|[2,2]：血量<1/2 先制+2
constexpr int kKuangBao = 13827;  // 狂暴撕咬 [33,454]|[3,1]：血量<1/3 先制+1
constexpr int kMuXin    = 34957;  // 木心归真 [1484,1237]：半血先制+1（零参）
constexpr int kYangChen = 35460;  // 扬尘地冲 [1487]：护盾先制+1（零参纯链）
constexpr int kYanDun   = 35635;  // 岩盾投掷 [1487]：同上（复验载体）
constexpr int kMaxHp    = 1000;   // hp 档位整数好算：40% = 400

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed) {
    ElfPet p = make_pet(1, "己方", skills, speed, /*atk=*/1, kMaxHp);
    accuracy_100(p);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 300) {
    ElfPet p = make_pet(2, "对手", skills, speed, /*atk=*/1, kMaxHp);
    accuracy_100(p);
    return p;
}

// 双方各一回合出招，返回决战后的 ctx（先手权已定）。
// hp_num = 开战后把己方当前体力设成 kMaxHp×hp_num%（直写 pet.hp 引用别名，
// max 在 numericalBase[HP] 不动）。
void drive_one_round(ScenarioBattle& b, int hp_permille) {
    b.start();
    BattleContext& ctx = b.ctx();
    ctx.getPet(0).hp = kMaxHp * hp_permille / 1000;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1390 454 血量<1/{0} 先制+{1}（屠戮之刃 [2,2] / 狂暴撕咬 [3,1]）══
    {
        // a) hp=40%：屠戮之刃 [2,2] 触发（40×2<100）→ +2 翻盘先手
        {
            ScenarioBattle b(
                make_party(self_pet({kTuLu, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, /*speed=*/100)),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            drive_one_round(b, 400);
            BattleContext& ctx = b.ctx();
            CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_1,
                  "T1390.a 454：血量 40%<1/2 → 先制+2 翻盘先手");
            CHECK_EQ(ctx.ws.preemptive_level[0], 2,
                     "T1390.a 454：preemptive_level=" +
                         std::to_string(ctx.ws.preemptive_level[0]));
        }
        // b) 同档 hp=40%：狂暴撕咬 [3,1] 不触发（40×3≥100）→ 高速对手先手
        {
            ScenarioBattle b(
                make_party(self_pet({kKuangBao, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100)),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            drive_one_round(b, 400);
            BattleContext& ctx = b.ctx();
            CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_2,
                  "T1390.b 454 门：40% 不低于 1/3 → 不触发（对手先手）");
            CHECK_EQ(ctx.ws.preemptive_level[0], 0,
                     "T1390.b 454 门：preemptive_level 保持 0");
        }
        // c) hp=30%：狂暴撕咬 [3,1] 触发（30×3<100）→ +1 翻盘（钉死除数 3 与量 1）
        {
            ScenarioBattle b(
                make_party(self_pet({kKuangBao, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100)),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            drive_one_round(b, 300);
            BattleContext& ctx = b.ctx();
            CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_1,
                  "T1390.c 454：血量 30%<1/3 → 先制+1 翻盘先手");
            CHECK_EQ(ctx.ws.preemptive_level[0], 1,
                     "T1390.c 454：preemptive_level=" +
                         std::to_string(ctx.ws.preemptive_level[0]));
        }
        // d) 边界 hp=50%：屠戮之刃严格低于不触发（50×2=100 不小于）→ 对手先手
        {
            ScenarioBattle b(
                make_party(self_pet({kTuLu, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100)),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            drive_one_round(b, 500);
            BattleContext& ctx = b.ctx();
            CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_2,
                  "T1390.d 454 边界：恰在 1/2 不触发（严格低于）");
        }
        std::cout << "  [T1390] 454 血量门先制 通过\n";
    }

    // ══ T1391 1484 体力<1/2 先制+1（木心归真，零参定档）══
    {
        // a) hp=40% → 触发 +1 翻盘
        {
            ScenarioBattle b(
                make_party(self_pet({kMuXin, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100)),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            drive_one_round(b, 400);
            BattleContext& ctx = b.ctx();
            CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_1,
                  "T1391.a 1484：体力 40%<1/2 → 先制+1 翻盘先手");
            CHECK_EQ(ctx.ws.preemptive_level[0], 1,
                     "T1391.a 1484：preemptive_level=" +
                         std::to_string(ctx.ws.preemptive_level[0]));
        }
        // b) hp=60% → 不触发
        {
            ScenarioBattle b(
                make_party(self_pet({kMuXin, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100)),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            drive_one_round(b, 600);
            BattleContext& ctx = b.ctx();
            CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_2,
                  "T1391.b 1484 门：60% 不低于 1/2 → 不触发");
            CHECK_EQ(ctx.ws.preemptive_level[0], 0,
                     "T1391.b 1484 门：preemptive_level 保持 0");
        }
        // c) 边界 hp=50% → 严格低于不触发
        {
            ScenarioBattle b(
                make_party(self_pet({kMuXin, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100)),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            drive_one_round(b, 500);
            BattleContext& ctx = b.ctx();
            CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_2,
                  "T1391.c 1484 边界：恰在 1/2 不触发");
        }
        std::cout << "  [T1391] 1484 半血门先制 通过\n";
    }

    // ══ T1392 1487 护盾状态先制+1（扬尘地冲/岩盾投掷，零参纯链）══
    {
        // a) 无盾对照 → 不触发
        {
            ScenarioBattle b(
                make_party(self_pet({kYangChen, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100)),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            BattleContext& ctx = b.ctx();
            CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_2,
                  "T1392.a 1487 门：无护盾 → 不触发（对手先手）");
            CHECK_EQ(ctx.ws.preemptive_level[0], 0,
                     "T1392.a 1487 门：preemptive_level 保持 0");
        }
        // b) 护盾账直添（J26 口径）→ 触发 +1 翻盘
        {
            ScenarioBattle b(
                make_party(self_pet({kYangChen, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100)),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            b.ctx().getPet(0).shield_bank_.add(/*priority=*/0, /*quantity=*/100,
                                               /*source_id=*/1487, /*duration=*/0);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            BattleContext& ctx = b.ctx();
            CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_1,
                  "T1392.b 1487：护盾状态 → 先制+1 翻盘先手");
            CHECK_EQ(ctx.ws.preemptive_level[0], 1,
                     "T1392.b 1487：preemptive_level=" +
                         std::to_string(ctx.ws.preemptive_level[0]));
        }
        // c) 第二纯链载体岩盾投掷同判（参数化=零参全族共享一个实现）
        {
            ScenarioBattle b(
                make_party(self_pet({kYanDun, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100)),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            b.ctx().getPet(0).shield_bank_.add(/*priority=*/0, /*quantity=*/100,
                                               /*source_id=*/1487, /*duration=*/0);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            BattleContext& ctx = b.ctx();
            CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_1,
                  "T1392.c 1487：岩盾投掷同判（护盾 → 翻盘先手）");
        }
        std::cout << "  [T1392] 1487 护盾门先制 通过\n";
    }

    std::cout << "scenario_1390 PASS (J53 条件先制族 454/1484/1487)" << std::endl;
    return 0;
}
