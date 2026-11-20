// 场景 790 —— H1 消强成功族（538 / 548，moves_lib/h_purge_success.cpp）
//（2026-09-30，worktree ../agents/h1；号段 790-799 一文件覆盖两效果、五载体）
//
// 机制：
//   538 消除对手能力提升状态，消除成功 → 自身六元掩码（**官方语义序**
//        攻,防,特攻,特防,速,命中 → 引擎槽 {0,2,1,3,4,5}）逐槽 stat_change；
//   548 消除对手能力提升状态，消除成功 → heal_amount 恢复 {0} 点（吃封回血）。
//
// 断言口径：
//   - "消除成功"门槛 = 对手**本体**有正等级（ability_levels 直写预置，730/650 同款
//     直写本体）；失败路对手无强化 → 自身零变化；
//   - 防特防互换由不灭钳杀 [0,2,0,2,0,0] 钉死（引擎槽2=防=2、槽3=特防=2、
//     槽1=特攻必须==0——按引擎序直读会错成"特攻+2"，此处必炸）；
//   - 548 恢复量 = ws.last_heal_amount 精确断言 + hp 终值双断；封回血
//     （HEAL_BLOCK 票，017 同款）→ 消强仍成功但恢复被封锁；
//   - 恢复钳制：hp 压到 max-100 恢复 200 → clamp 到 max；
//   - 载体必中（消 miss 噪声）；载体红伤只落在对手 hp（无断言），不污染等级/恢复。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kJueChen   = 16881;  // 绝尘剑：物攻90 [538] → [2,0,0,0,0,0]（攻+2；固有先制+3）
constexpr int kBuMie     = 34311;  // 不灭钳杀：[538,1153] → 538 六位 [0,2,0,2,0,0]（防+2特防+2）
constexpr int kFengMie   = 19209;  // 封灭决：物攻85 [538] → [1,1,1,1,1,1]（全+1）
constexpr int kHuanMie   = 15289;  // 幻灭之戟：[548] → [200]
constexpr int kZhanLing  = 16208;  // 斩灵涡轮：[548] → [250]
constexpr int kPaDa      = 10004;  // 拍打：filler
constexpr int kMaxHp     = 99999;

// 引擎等级槽（0攻 1特攻 2防 3特防 4速 5命中）
constexpr int kAtkSlot = 0, kSpAtkSlot = 1, kDefSlot = 2, kSpDefSlot = 3, kSpdSlot = 4;

void always_hit(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
}

ElfPet self_pet(std::array<int, 5> skills, int atk = 2000, int hp = kMaxHp, int speed = 300) {
    ElfPet p = make_pet(1, "己方", skills, speed, atk, hp);
    always_hit(p);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100) {
    ElfPet p = make_pet(2, "对手", skills, speed, /*atk=*/1, /*hp=*/9999);
    always_hit(p);
    return p;
}

// 己方出槽位技能 + 对手 NONE。
void play_round(ScenarioBattle& b, int self_slot) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, self_slot);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ════════════════════════════════════════════════════════════════
    // 538 绝尘剑（攻+2）
    // ════════════════════════════════════════════════════════════════

    // T1 消强成功 → 自身攻+2、对手强化清零
    {
        ScenarioBattle b(make_party(self_pet({kJueChen, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][kAtkSlot] = 2;   // 对手预置攻+2（直写本体，730/650 同款）
        play_round(b, 0);
        CHECK_EQ(ctx.ability_levels[0][kAtkSlot], 2, "T1 核心：消除成功 → 自身攻+2");
        CHECK_EQ(ctx.ability_levels[1][kAtkSlot], 0, "T1 核心：对手强化被清零");
        std::cout << "  [T1] 538 成功路（绝尘剑 攻+2）通过\n";
    }

    // T2 消强失败（对手无强化）→ 自身零变化
    {
        ScenarioBattle b(make_party(self_pet({kJueChen, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, 0);
        CHECK_EQ(ctx.ability_levels[0][kAtkSlot], 0, "T2 核心：对手无强化 → 不提升（SUCCESS 门槛）");
        std::cout << "  [T2] 538 失败路通过\n";
    }

    // T3 不灭钳杀 钉死防/特防语义序互换（引擎槽2=防、槽3=特防、槽1=特攻必须为 0）
    {
        ScenarioBattle b(make_party(self_pet({kBuMie, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][kSpdSlot] = 1;   // 对手预置任意正等级保证消强成功
        play_round(b, 0);
        CHECK_EQ(ctx.ability_levels[0][kDefSlot], 2, "T3 核心：语义序 idx1=防 → 引擎槽2 +2");
        CHECK_EQ(ctx.ability_levels[0][kSpDefSlot], 2, "T3 核心：语义序 idx3=特防 → 引擎槽3 +2");
        CHECK_EQ(ctx.ability_levels[0][kSpAtkSlot], 0,
                 "T3 反证：按引擎序直读会把特攻错抬 +2（双序坑必须炸）");
        CHECK_EQ(ctx.ability_levels[1][kSpdSlot], 0, "T3 前提复核：对手强化已清");
        std::cout << "  [T3] 538 语义序映射（不灭钳杀 防+2特防+2）通过\n";
    }

    // T4 封灭决 全六槽+1
    {
        ScenarioBattle b(make_party(self_pet({kFengMie, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][kAtkSlot] = 1;
        play_round(b, 0);
        for (int slot = 0; slot < 6; ++slot) {
            CHECK_EQ(ctx.ability_levels[0][slot], 1, "T4 核心：全属性+1（六槽逐槽）");
        }
        std::cout << "  [T4] 538 全属性+1（封灭决）通过\n";
    }

    // T5 多项强化一次全清（对手攻+2 速+1 → 清零 + 自身攻+2）
    {
        ScenarioBattle b(make_party(self_pet({kJueChen, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][kAtkSlot] = 2;
        ctx.ability_levels[1][kSpdSlot] = 1;
        play_round(b, 0);
        CHECK_EQ(ctx.ability_levels[1][kAtkSlot], 0, "T5 核心：多项强化全清（攻）");
        CHECK_EQ(ctx.ability_levels[1][kSpdSlot], 0, "T5 核心：多项强化全清（速）");
        CHECK_EQ(ctx.ability_levels[0][kAtkSlot], 2, "T5 核心：自身收益照发");
        std::cout << "  [T5] 538 多项全清通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // 548 幻灭之戟（恢复200）/ 斩灵涡轮（恢复250）
    // ════════════════════════════════════════════════════════════════

    // T6 消强成功 → 恢复 200（last_heal + hp 双断）
    {
        ScenarioBattle b(make_party(self_pet({kHuanMie, kPaDa, kPaDa, kPaDa, kPaDa},
                                             /*atk=*/2000)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 1000;   // 压血（make_pet hp 即上限，出场满血——680 同款直改）
        ctx.ability_levels[1][kAtkSlot] = 2;
        play_round(b, 0);
        CHECK_EQ(ctx.ws.last_heal_amount[0], 200, "T6 核心：消除成功 → 恢复 200 点");
        CHECK_EQ(ctx.getPet(0).hp, 1200, "T6 核心：hp 1000 → 1200");
        std::cout << "  [T6] 548 成功路（幻灭之戟 200）通过\n";
    }

    // T7 消强失败 → 不恢复
    {
        ScenarioBattle b(make_party(self_pet({kHuanMie, kPaDa, kPaDa, kPaDa, kPaDa},
                                             /*atk=*/2000)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 1000;   // 压血（make_pet hp 即上限，出场满血——680 同款直改）
        play_round(b, 0);
        CHECK_EQ(ctx.ws.last_heal_amount[0], 0, "T7 核心：对手无强化 → 恢复 0");
        CHECK_EQ(ctx.getPet(0).hp, 1000, "T7 核心：hp 不变");
        std::cout << "  [T7] 548 失败路通过\n";
    }

    // T8 斩灵涡轮 恢复 250（第二载体第二参数值）
    {
        ScenarioBattle b(make_party(self_pet({kZhanLing, kPaDa, kPaDa, kPaDa, kPaDa},
                                             /*atk=*/2000)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 1000;   // 压血（make_pet hp 即上限，出场满血——680 同款直改）
        ctx.ability_levels[1][kAtkSlot] = 2;
        play_round(b, 0);
        CHECK_EQ(ctx.ws.last_heal_amount[0], 250, "T8 核心：第二载体 [250] → 恢复 250 点");
        CHECK_EQ(ctx.getPet(0).hp, 1250, "T8 核心：hp 1000 → 1250");
        std::cout << "  [T8] 548 斩灵涡轮 250 通过\n";
    }

    // T9 封回血（HEAL_BLOCK 票）→ 消强仍成功、恢复被封锁（heal_amount 统一出口）
    {
        ScenarioBattle b(make_party(self_pet({kHuanMie, kPaDa, kPaDa, kPaDa, kPaDa},
                                             /*atk=*/2000)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 1000;   // 压血（make_pet hp 即上限，出场满血——680 同款直改）
        ctx.grant_immunity(0, ImmunityType::HEAL_BLOCK, coverage_all(), /*anomaly_mask=*/0,
                           /*duration_rounds=*/0, /*source_id=*/8001);
        ctx.ability_levels[1][kAtkSlot] = 2;
        play_round(b, 0);
        CHECK_EQ(ctx.ability_levels[1][kAtkSlot], 0, "T9 核心：消强不被封回血牵连");
        CHECK_EQ(ctx.ws.last_heal_amount[0], 0, "T9 核心：封回血 → 恢复 0");
        CHECK_EQ(ctx.getPet(0).hp, 1000, "T9 核心：hp 不变");
        std::cout << "  [T9] 548 封回血路通过\n";
    }

    // T10 恢复钳制：hp = max-100 恢复 200 → clamp 到 max
    {
        ScenarioBattle b(make_party(self_pet({kHuanMie, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = kMaxHp - 100;   // 压血：差 100 满
        ctx.ability_levels[1][kAtkSlot] = 2;
        play_round(b, 0);
        CHECK_EQ(ctx.getPet(0).hp, kMaxHp, "T10 核心：恢复 200 但只差 100 → clamp 到最大体力");
        std::cout << "  [T10] 548 恢复钳制通过\n";
    }

    std::cout << "scenario_790_purge_success all passed" << std::endl;
    return 0;
}
