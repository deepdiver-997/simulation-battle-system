#include <utils/sbs_rand.h>
// 场景 520：E7 先制族批产——828 有提升先制+1 / 1080 连用先制+1 / 523 未击败位图提升
//（moves_lib/e_priority.cpp，2026-09-29）
//
// 断言组：
//   TA 828 正例：R1 首用（结构性偏差：无先制，1002/1782 同根源记档）→ R2 有提升 +
//      载体技（泰坦盾击 30099）→ preemptive_level==1 且慢者先动。
//   TB 828 反例：无提升 → 载体技不加先制。
//   TC 828 载体判定：有提升但用非载体技（拍打）→ 不加（rawEffectRecords 判定防过宽）。
//   TD 1080 连用：R1 首用（无先制）→ R2 再用载体技（双刃制敌 30731）→ +1。
//   TE 1080 中断：载体→拍打→载体，第三回合不连续（last 停留旧值）→ 不加。
//   TF 523 位图：断裂星空闪 14843（攻/速位）命中未击败 → 自身攻+1、速+1。
//   TG 523 击败反例：一击致死 → 无提升。
//   TH 523 特攻位图：弑神冥火爆 15939（特攻位）→ 特攻+1。
//
// 确定性：载体技 crit_rate=0、must_hit/accuracy 拉满；同 srand；未击败用对手
// 9999 血保证；828/1080 的载体判定依赖 DB 切片（side_effect 单效果载体）。
#include <array>
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kTitanSlam = 30099;    // 泰坦盾击：物攻 150、单效果 [828]
constexpr int kTwinBlade = 30731;    // 双刃制敌：物攻 130、单效果 [1080]
constexpr int kStarBreak = 14843;    // 断裂星空闪：物攻 150、单效果 [523] 位图[攻,速]
constexpr int kGodslayer = 15939;    // 弑神冥火爆：特攻 150、单效果 [523] 位图[特攻]
constexpr int kPaiDa = 10004;        // 拍打：物攻 40、无效果

ElfPet carrier_pet(const std::array<int, 5>& skill_ids, int speed, int atk = 150) {
    ElfPet p = scenario::make_pet(4872, "甲",
                                  {skill_ids[0], skill_ids[1], skill_ids[2], skill_ids[3],
                                   skill_ids[4]},
                                  speed, atk, /*hp=*/9999);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet(int speed = 100, int atk = 10) {
    ElfPet p = scenario::make_pet(1, "乙", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                  speed, atk, /*hp=*/9999);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

void drive_round(ScenarioBattle& b, int slot0, int slot1) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, slot0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, slot1);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== TA 828：有提升 + 载体技 → 先制+1（慢者先动）==========
    {
        sbs_srand(20260929);
        ScenarioBattle b(scenario::make_party(carrier_pet({kTitanSlam, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                          /*speed=*/50)),
                         scenario::make_party(foe_pet(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b, 0, 0);   // R1 首用：tick 尚未武装 → 无先制（结构性偏差记档）
        CHECK(ctx.is_second_mover(0), "TA 前提：R1 首用无先制（偏差记档）");
        ctx.ability_levels[0][0] = 2;   // 自身攻击+2（能力提升状态）
        drive_round(b, 0, 0);   // R2：有提升 + 载体技 → +1
        CHECK_EQ(ctx.ws.preemptive_level[0], 1, "TA 核心：有提升先制+1 已置位");
        CHECK(!ctx.is_second_mover(0), "TA 核心：慢者因先制+1 先动");
        std::cout << "  [TA] 828 有提升先制 通过\n";
    }

    //========== TB 828 反例：无提升 → 不加 ==========
    {
        sbs_srand(20260929);
        ScenarioBattle b(scenario::make_party(carrier_pet({kTitanSlam, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                          200)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b, 0, 0);   // R1 武装
        drive_round(b, 0, 0);   // R2：无提升
        CHECK_EQ(ctx.ws.preemptive_level[0], 0, "TB 核心：无提升不加先制");
        std::cout << "  [TB] 828 无提升反例 通过\n";
    }

    //========== TC 828 载体判定：有提升 + 非载体技 → 不加 ==========
    {
        sbs_srand(20260929);
        ScenarioBattle b(scenario::make_party(carrier_pet({kTitanSlam, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                          200)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b, 0, 0);   // R1 武装（用载体技）
        ctx.ability_levels[0][0] = 2;
        drive_round(b, 1, 0);   // R2：拍打（非载体）+ 有提升 → 不加
        CHECK_EQ(ctx.ws.preemptive_level[0], 0, "TC 核心：非载体技不加先制（防过宽）");
        std::cout << "  [TC] 828 载体判定 通过\n";
    }

    //========== TD 1080：连用 → R2 起先制+1 ==========
    {
        sbs_srand(20260929);
        ScenarioBattle b(scenario::make_party(carrier_pet({kTwinBlade, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                          200)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b, 0, 0);   // R1 首用：非"连续" → 不加
        CHECK_EQ(ctx.ws.preemptive_level[0], 0, "TD 前提：R1 首用不加（本就非连用）");
        drive_round(b, 0, 0);   // R2：连用 → +1
        CHECK_EQ(ctx.ws.preemptive_level[0], 1, "TD 核心：连续使用先制+1");
        CHECK(!ctx.is_second_mover(0), "TD 行为：慢者因先制先动");
        drive_round(b, 0, 0);   // R3：持续连用 → 仍 +1
        CHECK_EQ(ctx.ws.preemptive_level[0], 1, "TD 附加：三连用仍 +1（固定值不递增）");
        std::cout << "  [TD] 1080 连用先制 通过\n";
    }

    //========== TE 1080 中断：穿插非载体技 → 连续性断 ==========
    {
        sbs_srand(20260929);
        ScenarioBattle b(scenario::make_party(carrier_pet({kTwinBlade, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                          200)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b, 0, 0);   // R1 载体（首用）
        drive_round(b, 1, 0);   // R2 拍打（中断）
        drive_round(b, 0, 0);   // R3 载体：last 停在 R1 → 不连续
        CHECK_EQ(ctx.ws.preemptive_level[0], 0, "TE 核心：穿插别的技能 → 连用中断不加");
        std::cout << "  [TE] 1080 中断反例 通过\n";
    }

    //========== TF 523 位图：断裂星空闪（攻/速）未击败 → 攻+1 速+1 ==========
    {
        sbs_srand(20260929);
        ScenarioBattle b(scenario::make_party(carrier_pet({kStarBreak, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                          200)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b, 0, 0);   // R1：命中未击败（对手 9999 血）
        CHECK(ctx.getPet(1).hp > 0, "TF 前提：未击败（对手存活）");
        CHECK_EQ(ctx.ability_levels[0][0], 1, "TF 核心：攻击 +1（位图 pos0）");
        CHECK_EQ(ctx.ability_levels[0][4], 1, "TF 核心：速度 +1（位图 pos4）");
        CHECK_EQ(ctx.ability_levels[0][2], 0, "TF 核心：特攻不变（位图 pos2=0）");
        std::cout << "  [TF] 523 位图提升 通过\n";
    }

    //========== TG 523 击败反例：一击致死 → 无提升 ==========
    {
        sbs_srand(20260929);
        ScenarioBattle b(scenario::make_party(carrier_pet({kStarBreak, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                          200, /*atk=*/3000)),
                         scenario::make_party(foe_pet(/*speed=*/100, /*atk=*/10)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(1).hp = 3000;   // 压低血量保证一击致死
        drive_round(b, 0, 0);
        CHECK(ctx.getPet(1).hp <= 0, "TG 前提：对手已被击败");
        CHECK_EQ(ctx.ability_levels[0][0], 0, "TG 核心：击败对手 → 不提升");
        CHECK_EQ(ctx.ability_levels[0][4], 0, "TG 核心：击败对手 → 速度也不加");
        std::cout << "  [TG] 523 击败反例 通过\n";
    }

    //========== TH 523 特攻位图：弑神冥火爆 → 特攻+1 ==========
    {
        sbs_srand(20260929);
        ScenarioBattle b(scenario::make_party(carrier_pet({kGodslayer, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                          200)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b, 0, 0);
        CHECK_EQ(ctx.ability_levels[0][2], 1, "TH 核心：特攻 +1（位图 pos2）");
        CHECK_EQ(ctx.ability_levels[0][0], 0, "TH 核心：攻击不变（位图 pos0=0）");
        std::cout << "  [TH] 523 特攻位图 通过\n";
    }

    std::cout << "scenario_520 all passed" << std::endl;
    return 0;
}
