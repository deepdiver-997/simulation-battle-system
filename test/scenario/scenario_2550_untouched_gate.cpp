// 场景 2550 —— J153 未受击门族（819 / 1708，moves_lib/j_untouched_gate.cpp）
//（2026-10-04，worktree ../agents/j153；号段 2550-2559 → T2550-T2554 五块）
//
// 断言口径：
//   - 819 走纯链载体 板凳毛伏击 32586 [0,50]（物）/ 暗伺瞬影 33864 [2,50]（特）：
//     atk=1000 → 攻击值/特攻值 1000 → rider 500 精确差账（raw_attack_damage 读红伤）；
//     code 判别：暗伺瞬影块把本体物攻直写为 10（特攻仍 1000）→ rider=500 钉死
//     "code 2=特攻值"（若误读攻击值则只有 5）；
//   - 门闭（受击旗）走跨回合持久 watcher：R1 施放武装 → R2 对手先手打我（旗 R2
//     置位）→ R2 再施放 → 门闭无 rider；
//   - 1708 走纯链载体 觉 28296 [1,2]（属）：速度垫差 100 vs 101——门开 → R2 对手
//     先制 -2 翻转我先手（SEER_ROBOT_1）；门闭（R2 受击后施放）→ R3 对手先手。
//   ⚠️ 首施放回合的施放前受击盲区（文件头记档）不在断言面。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kBanDeng = 32586;   // 板凳毛伏击：物 [819] → [0,50]（攻击值）
constexpr int kAnSi    = 33864;   // 暗伺瞬影：特 [819] → [2,50]（特攻值）
constexpr int kXuZhi   = 37639;   // 虚滞：特60 纯链 [1708] → [1,1]（觉 28296 链带
                                  // 946"下2回合先制+2"伴生污染 pl 断言，弃用记档）
constexpr int kPaDa    = 10004;   // 拍打：filler

void always_hit(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300, int atk = 1000) {
    ElfPet p = make_pet(1, "己方", skills, speed, atk, /*hp=*/99999);
    always_hit(p);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100, int hp = 99999) {
    ElfPet p = make_pet(2, "对手", skills, speed, /*atk=*/1, hp);
    always_hit(p);
    return p;
}

void self_skill(ScenarioBattle& b, int slot) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, slot);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

void both_act(ScenarioBattle& b, int self_slot, int foe_slot = 0) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, self_slot);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_slot);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T2550 819 门开 rider（板凳毛伏击 [0,50]：攻击值 1000 → 500 + 回等量）══
    {
        ScenarioBattle b(make_party(self_pet({kBanDeng, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = 6000;
        self_skill(b, 0);   // R1 我先手施放（无人打我 → 门开）
        const int final = ctx.ws.raw_attack_damage[0];
        CHECK(final > 0, "T2550 前置：红伤落地");
        CHECK_EQ(ctx.getPet(1).hp, 99999 - final - 500,
                 "T2550 819：门开 → 附加攻击值 50% = 500 FIXED");
        CHECK_EQ(ctx.getPet(0).hp, 6000 + 500, "T2550 819：恢复等量（按落地 dealt=500）");
        std::cout << "  [T2550] 819 门开 rider 通过\n";
    }

    // ══ T2551 819 值类型码判别（暗伺瞬影 [2,50]：物攻压 10、特攻 1000 → 500）══
    {
        ElfPet me = self_pet({kAnSi, kPaDa, kPaDa, kPaDa, kPaDa});
        me.numericalBase[NumericalPropertyIndex::PHYSICAL_ATTACK] = 10;
        ScenarioBattle b(make_party(std::move(me)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        self_skill(b, 0);
        const int final = ctx.ws.raw_attack_damage[0];
        CHECK_EQ(ctx.getPet(1).hp, 99999 - final - 500,
                 "T2551 819：code 2=特攻值（1000×50%=500；若误读攻击值仅 5）");
        std::cout << "  [T2551] 819 值类型码 通过\n";
    }

    // ══ T2552 819 门闭（跨回合旗：R2 对手先手打我 → 再施放无 rider）══
    {
        ScenarioBattle b(make_party(self_pet({kBanDeng, kPaDa, kPaDa, kPaDa, kPaDa},
                                             /*speed=*/100)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*speed=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        both_act(b, 0);   // R1 我施放（武装旗 watcher）+ 对手打我（旗 R1 置位）
        // 注：R1 施放前盲区（文件头记档）→ R1 门误开有 rider，基线取 R1 末即可。
        const int foe_hp_r1 = ctx.getPet(1).hp;
        both_act(b, 0);   // R2 对手先手打我（旗 R2 置位）→ 我再施放 → 门闭
        const int final2 = ctx.ws.raw_attack_damage[0];
        CHECK(final2 > 0, "T2552 前置：R2 红伤落地");
        CHECK_EQ(ctx.getPet(1).hp, foe_hp_r1 - final2,
                 "T2552 819：受击旗在 → R2 门闭无 rider");
        std::cout << "  [T2552] 819 门闭 通过\n";
    }

    // ══ T2553 1708 门开 → 对手下回合先制 -1（纯链 虚滞 [1,1]）══
    // 先制等级判据：R2 对手 pl=-1 < 我 0 → 我先手（等级优先于速度，无尾巴时
    // 0=0 平级落速度 101>100 对手先——判别性成立）。
    {
        ScenarioBattle b(make_party(self_pet({kXuZhi, kPaDa, kPaDa, kPaDa, kPaDa},
                                             /*speed=*/100, /*atk=*/1)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*speed=*/101)));
        b.start();
        BattleContext& ctx = b.ctx();
        self_skill(b, 0);   // R1 我施放（后手但对手 NONE → 无伤 → 门开）→ 尾巴武装
        both_act(b, 1);     // R2 MOVE_RIGHT：对手 pl=-1 → 翻转我先手
        CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_1,
              "T2553 1708：门开 → 对手下回合先制-1（先手翻转）");
        std::cout << "  [T2553] 1708 门开先制降 通过\n";
    }

    // ══ T2554 1708 门闭（后续回合旗齐全：R3 受击后施放 → 门闭无先制降）══
    // R1 施放武装 watcher（首施放回合施放前受击=盲区，记档）；R2/R3 旗齐——
    // R3 对手先手打我（旗 R3）→ 我再施放 → 回合末门闭 → R4 无先制降。
    {
        ScenarioBattle b(make_party(self_pet({kXuZhi, kPaDa, kPaDa, kPaDa, kPaDa},
                                             /*speed=*/100, /*atk=*/1)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*speed=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        both_act(b, 0);   // R1 我施放（武装 watcher）+ 对手打我（盲区，尾武装记档）
        both_act(b, 1);   // R2 对手打我（旗 R2 置位）
        both_act(b, 0);   // R3 对手先手打我（旗 R3）→ 我再施放 → 门闭
        both_act(b, 1);   // R4 无尾巴 → 对手速度 300 先手
        fprintf(stderr, "[DBG] R4 pr=%d pl0=%d pl1=%d rc=%d me_hp=%d foe_hp=%d\n",
                static_cast<int>(ctx.preemptive_right), ctx.ws.preemptive_level[0],
                ctx.ws.preemptive_level[1], ctx.roundCount, ctx.getPet(0).hp,
                ctx.getPet(1).hp);
        CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_2,
              "T2554 1708：受击旗在（旗 R3）→ 门闭无先制降");
        std::cout << "  [T2554] 1708 门闭 通过\n";
    }

    std::cout << "scenario_2550 PASS (J153 未受击门族)" << std::endl;
    return 0;
}
