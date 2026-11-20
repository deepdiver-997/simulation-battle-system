#include <utils/sbs_rand.h>
// 场景 2100 —— J116 保命窗族（1068 / 1259 / 874 / 62，moves_lib/j_lifeguard_window.cpp）
//（2026-10-03，worktree ../agents/j116；号段 2100-2109 → T2100-T2109 十块）
//
// 断言口径（820/J14 同款）：
//   - 1068 走纯链载体 魔磁障壁 28402 [3,1]：atk=3000 vs hp=100 闭式致死（raw 恒
//     >100），拦截器写回恰 1 点；窗长 3 靠连续三年致死各残 1、第四年死钉死
//     （NextRounds 字面：武装当回合不保）；
//   - 1259 走纯链载体 盛夏相逢 27995 [3,50,0,0,0,-2,-2,0]（特防/速各-2，50%）：
//     50% 掷点用 **种子校准** 钉死（载体属性技纯链无 rand 消费、拍打 must_hit，
//     掷点=校准后第 n 个 sbs_rand()%100）；"恰 {0} 掷"用 交替序列 fail/fail/hit + 第四回合
//     驱动不变 钉死（InRounds [R1..R3]，NextRounds 误版会在 R4 再掷）；
//   - 874 走纯链载体 芸生长啸 37086 [3,350]：atk=1 raw 1~3 < 350 免疫（loss==0）、
//     atk=3000 raw ≥858 > 350 照常（浮动带远离阈值，无需种子）；R4 窗过期低伤落地；
//   - 62 走纯链载体 镇魂歌 20215 [3] / 忍辱负重 23344 [5] / 龙契·逝 37740 [4]：
//     窗末 ROUND_END 判死（InRounds 含当回合口径——[5] 载体 R5 末死、R4 末活是
//     判别性断言）；被击败承诺破不判死；62×1068 交互按引擎实测记档（死亡漏斗
//     DAMAGE 重判 → 残留免死拦得下判死，详见交互块注）。
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

// ── 载体技（切片已逐条与 side_effect/arg 核对，全纯链）──
constexpr int kMoCi    = 28402;   // 魔磁障壁：属 [1068] → [3,1]（下3回合残留1）
constexpr int kShengXia= 27995;   // 盛夏相逢：属 [1259] → [3,50,0,0,0,-2,-2,0]（特防速）
constexpr int kYunSheng= 37086;   // 芸生长啸：物140 [874] → [3,350]
constexpr int kZhenHun = 20215;   // 镇魂歌：属 [62] → [3]
constexpr int kRenRu   = 23344;   // 忍辱负重：属 [62] → [5]
constexpr int kLongQi  = 37740;   // 龙契·逝：物120 [62] → [4]
constexpr int kPaDa    = 10004;   // 拍打：物攻40（filler/对手攻击载体）

// 引擎槽（速查 §三：0攻 1特攻 2防 3特防 4速 5命中）
constexpr int kSlotSpd = 4;
constexpr int kSlotSpdef = 3;

void always_hit(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
}

ElfPet self_pet(std::array<int, 5> skills, int atk = 1, int hp = 99999, int speed = 300) {
    ElfPet p = make_pet(1, "己方", skills, speed, atk, hp);
    always_hit(p);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int atk = 1, int hp = 99999, int speed = 100) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, hp);
    always_hit(p);
    return p;
}

void self_skill(ScenarioBattle& b, int slot) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, slot);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

void foe_attacks(ScenarioBattle& b, int foe_slot = 0) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_slot);
    b.drive();
}

void both_act(ScenarioBattle& b, int self_slot, int foe_slot = 0) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, self_slot);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_slot);
    b.drive();
}

void both_pass(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

int foe_level(BattleContext& ctx, int slot) { return ctx.ability_levels[1][slot]; }

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T2100 1068 窗长3（魔磁障壁 [3,1]）：R2/R3/R4 各残1，R5 死 ══
    {
        ScenarioBattle b(make_party(self_pet({kMoCi, kPaDa, kPaDa, kPaDa, kPaDa},
                                             /*atk=*/1, /*hp=*/100)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*atk=*/3000)));
        b.start();
        BattleContext& ctx = b.ctx();
        self_skill(b, 0);   // R1 武装（窗口 = R2..R4，"下3回合"字面）
        foe_attacks(b);     // R2 致死 → 残1
        CHECK_EQ(ctx.getPet(0).hp, 1, "T2100 1068：下1回合致死 → 残留 1 点");
        foe_attacks(b);     // R3 窗内 → 残1
        CHECK_EQ(ctx.getPet(0).hp, 1, "T2100 1068：下2回合致死 → 仍残 1 点");
        foe_attacks(b);     // R4 窗内（第3个保护回合）→ 残1
        CHECK_EQ(ctx.getPet(0).hp, 1, "T2100 1068：下3回合致死 → 仍残 1 点");
        foe_attacks(b);     // R5 窗已过 → 死
        CHECK(ctx.getPet(0).hp <= 0, "T2100 1068：窗过期后致死成立（恰保 3 回合）");
        std::cout << "  [T2100] 1068 窗长3 致死残留 通过\n";
    }

    // ══ T2101 1068 当回合不保（"下{0}回合"NextRounds 字面：后手被先手打死不拦）══
    {
        ScenarioBattle b(make_party(self_pet({kMoCi, kPaDa, kPaDa, kPaDa, kPaDa},
                                             /*atk=*/1, /*hp=*/100, /*speed=*/100)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*atk=*/3000, /*hp=*/99999, /*speed=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        both_act(b, 0);   // R1 对手先手致死 → 我后手的 1068 尚未武装
        CHECK(ctx.getPet(0).hp <= 0, "T2101 1068：当回合（后手）致死不保");
        std::cout << "  [T2101] 1068 当回合不保 通过\n";
    }

    // ══ T2102 1259 掷中免攻 + 票当回合过期 + 次回合掷歪罚则（盛夏相逢）══
    // 种子：d1=掷1(hit) d2=R1浮伤 d3=R2掷(fail)——载体纯链 + must_hit → 序列干净。
    {
        int seed = -1;
        for (int s = 1; s < 100000 && seed < 0; ++s) {
            sbs_srand(s);
            const int d1 = sbs_rand() % 100;
            (void)sbs_rand();   // R1 对手攻击的 85%~100% 浮伤
            const int d3 = sbs_rand() % 100;
            if (d1 < 50 && d3 >= 50) {
                seed = s;
            }
        }
        REQUIRE(seed > 0);
        ScenarioBattle b(make_party(self_pet({kShengXia, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*atk=*/3000)));
        b.start();
        BattleContext& ctx = b.ctx();
        sbs_srand(seed);
        both_act(b, 0);   // R1 我掷中（免攻票 duration=1）→ 对手攻击被免
        CHECK_EQ(ctx.getPet(0).hp, 99999, "T2102 1259：掷中 → 免疫对手攻击伤害");
        CHECK_EQ(foe_level(ctx, kSlotSpdef), 0, "T2102 1259：掷中不降槽（特防）");
        CHECK_EQ(foe_level(ctx, kSlotSpd), 0, "T2102 1259：掷中不降槽（速度）");
        foe_attacks(b);   // R2 票过期 + 掷歪 → 罚则降槽 + 伤害落地
        CHECK(ctx.getPet(0).hp < 99999, "T2102 1259：免票当回合过期（次回合受伤）");
        CHECK_EQ(foe_level(ctx, kSlotSpdef), -2, "T2102 1259：掷歪 → 对手特防-2（语义槽3）");
        CHECK_EQ(foe_level(ctx, kSlotSpd), -2, "T2102 1259：掷歪 → 对手速度-2（语义槽4）");
        CHECK_EQ(foe_level(ctx, 0), 0, "T2102 1259：未列槽不动（攻击）");
        std::cout << "  [T2102] 1259 掷中免攻/票过期/掷歪降槽 通过 (seed " << seed << ")\n";
    }

    // ══ T2103 1259 恰 {0}=3 掷 + 罚则累计（fail/fail/hit + R4 驱动不变）══
    // 判别性：InRounds [R1..R3] → R4 无掷点；NextRounds 误版会在 R4 再掷（必挂）。
    {
        int seed = -1;
        for (int s = 1; s < 100000 && seed < 0; ++s) {
            sbs_srand(s);
            const int d1 = sbs_rand() % 100;
            const int d2 = sbs_rand() % 100;
            const int d3 = sbs_rand() % 100;
            if (d1 >= 50 && d2 >= 50 && d3 < 50) {
                seed = s;
            }
        }
        REQUIRE(seed > 0);
        ScenarioBattle b(make_party(self_pet({kShengXia, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        sbs_srand(seed);
        self_skill(b, 0);   // R1 掷歪 → -2
        CHECK_EQ(foe_level(ctx, kSlotSpd), -2, "T2103 1259：R1 掷歪 → 速度-2");
        both_pass(b);       // R2 掷歪 → -4
        CHECK_EQ(foe_level(ctx, kSlotSpd), -4, "T2103 1259：R2 再掷歪 → 累计-4");
        both_pass(b);       // R3 掷中 → 免票（无罚则）
        CHECK_EQ(foe_level(ctx, kSlotSpd), -4, "T2103 1259：R3 掷中 → 不再降槽");
        both_pass(b);       // R4 窗已过 → 无掷点
        CHECK_EQ(foe_level(ctx, kSlotSpd), -4, "T2103 1259：恰3掷（R4 无掷点，窗长=3）");
        std::cout << "  [T2103] 1259 恰3掷/罚则累计 通过 (seed " << seed << ")\n";
    }

    // ══ T2104 874 阈值下免疫 + 窗过期（芸生长啸 [3,350]，atk=1 闭式界）══
    {
        ScenarioBattle b(make_party(self_pet({kYunSheng, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*atk=*/1)));
        b.start();
        BattleContext& ctx = b.ctx();
        self_skill(b, 0);   // R1 武装（窗 [R1..R3]；自方 pow140 atk=1 打对手 1~3，不断言）
        foe_attacks(b);     // R2 raw 1~3 < 350 → 免疫
        CHECK_EQ(ctx.getPet(0).hp, 99999, "T2104 874：低于阈值 → 免疫该伤害");
        foe_attacks(b);     // R3 窗内 → 免疫
        CHECK_EQ(ctx.getPet(0).hp, 99999, "T2104 874：窗内第2年仍免疫");
        foe_attacks(b);     // R4 窗过期 → 低伤落地（loss 1~3）
        CHECK(ctx.getPet(0).hp < 99999, "T2104 874：窗过期后低伤照常落地");
        std::cout << "  [T2104] 874 阈值下免疫/窗过期 通过\n";
    }

    // ══ T2105 874 高于阈值照常（atk=3000 raw ≥858 > 350，浮动带远离阈值）══
    {
        ScenarioBattle b(make_party(self_pet({kYunSheng, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*atk=*/3000)));
        b.start();
        BattleContext& ctx = b.ctx();
        self_skill(b, 0);   // R1 武装
        foe_attacks(b);     // R2 raw ~1010 ≥ 350 → 照常落地
        CHECK(ctx.getPet(0).hp < 99999, "T2105 874：不低于阈值 → 免疫不触发");
        std::cout << "  [T2105] 874 高于阈值照常 通过\n";
    }

    // ══ T2106 62 窗末判死（镇魂歌 [3]）：R1 武装 → R2 末活 → R3 末死 ══
    {
        ScenarioBattle b(make_party(self_pet({kZhenHun, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        self_skill(b, 0);   // R1 镇魂歌（窗 [R1..R3]，末回合 ROUND_END 判死）
        both_pass(b);       // R2 对手没击败我
        CHECK_EQ(ctx.getPet(1).hp, 99999, "T2106 62：窗内对手仍存活");
        both_pass(b);       // R3 末 → 对方死亡
        CHECK(ctx.getPet(1).hp <= 0, "T2106 62：3回合后未被击败 → 对方死亡");
        CHECK(ctx.getPet(0).hp > 0, "T2106 62：自己存活（承诺成立的前提）");
        std::cout << "  [T2106] 62 窗末判死 通过\n";
    }

    // ══ T2107 62 被击败承诺破（镇魂歌）：R2 致死 → 不判死 ══
    {
        ScenarioBattle b(make_party(self_pet({kZhenHun, kPaDa, kPaDa, kPaDa, kPaDa},
                                             /*atk=*/1, /*hp=*/100, /*speed=*/100)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*atk=*/3000, /*hp=*/99999, /*speed=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        self_skill(b, 0);   // R1 镇魂歌
        foe_attacks(b);     // R2 对手致死 → 承诺破（战斗停在 CHOOSE_AFTER_DEATH）
        CHECK(ctx.getPet(1).hp > 0, "T2107 62：自己被击败 → 对方不判死");
        std::cout << "  [T2107] 62 被击败承诺破 通过\n";
    }

    // ══ T2108 62 窗长5（忍辱负重 [5]）：R4 末活 / R5 末死（口径判别性断言）══
    {
        ScenarioBattle b(make_party(self_pet({kRenRu, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        self_skill(b, 0);   // R1 武装（窗 [R1..R5]）
        both_pass(b);       // R2
        both_pass(b);       // R3
        both_pass(b);       // R4 末 → 仍活（InRounds 含当回合：判死在 R5 末）
        CHECK_EQ(ctx.getPet(1).hp, 99999, "T2108 62：R4 末仍存活（窗长=5 口径）");
        both_pass(b);       // R5 末 → 死
        CHECK(ctx.getPet(1).hp <= 0, "T2108 62：5回合后对方死亡");
        std::cout << "  [T2108] 62 窗长5 通过\n";
    }

    // ══ T2109 62 攻击技载体 + 窗长4（龙契·逝 [4] 物攻120）══
    {
        ScenarioBattle b(make_party(self_pet({kLongQi, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        self_skill(b, 0);   // R1 武装（自方攻击打对手 1~3 → 以下断言以武装后为基线）
        const int foe_hp_base = ctx.getPet(1).hp;
        both_pass(b);       // R2
        both_pass(b);       // R3 末仍活
        CHECK_EQ(ctx.getPet(1).hp, foe_hp_base, "T2109 62：R3 末仍存活");
        both_pass(b);       // R4 末 → 死
        CHECK(ctx.getPet(1).hp <= 0, "T2109 62：4回合后对方死亡（攻击技载体）");
        std::cout << "  [T2109] 62 攻击技载体/窗长4 通过\n";
    }

    // ══ 附：62 判死 × 1068 残留免死（交互实测记档，2026-10-03）══
    // 对手 R1 先手武装 1068（魔磁障壁），我后手镇魂歌（62 的 InRounds 窗对后手顺延
    // [R+1, R+N]——round_effect_start_round 官方回合数判定，判死落在第 4 驱动回合末）。
    // ⚠️ 实测：force_hp_to_zero 本体直写 hp=0、不走拦截器，但其后 ROUND_END 死亡漏斗
    // （battleFsm funnel_on_stage_deaths）对在场上 hp≤0 者一律以 DefeatCause::DAMAGE
    // 重判 → 1068 式残留拦截器（只拦 DAMAGE）把判死拦下 → hp=残留 1。即引擎现状 =
    // 镇魂歌判死可被残留免死救下（官方对"残留体力 vs 死亡条款"无实证，引擎为准，
    // 留 R 组台账；无 1068 时的判死已由 T2106 证明）。
    {
        ScenarioBattle b(make_party(self_pet({kZhenHun, kPaDa, kPaDa, kPaDa, kPaDa},
                                             /*atk=*/1, /*hp=*/99999, /*speed=*/100)),
                         make_party(foe_pet({kMoCi, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*atk=*/1, /*hp=*/99999, /*speed=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        both_act(b, 0);   // R1 对手先手魔磁障壁武装 1068，我后手镇魂歌
        both_pass(b);     // R2
        both_pass(b);     // R3 末：后手顺延 → 未到判死回合
        CHECK(ctx.getPet(1).hp > 0, "T2109 交互：后手顺延，R3 末对手仍存活");
        both_pass(b);     // R4 末：镇魂歌判死 → 被对手 1068 残留免死拦下
        CHECK_EQ(ctx.seerRobot[1].elfPets[0].hp, 1,
                 "T2109 交互：判死被 1068 残留免死拦下（hp=残留 1，漏斗 DAMAGE 重判）");
        std::cout << "  [T2109] 62×1068 交互 通过\n";
    }

    std::cout << "scenario_2100 PASS (J116 保命窗族)" << std::endl;
    return 0;
}
