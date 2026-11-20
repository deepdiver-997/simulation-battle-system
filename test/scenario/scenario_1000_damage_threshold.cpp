// 场景 1000 —— J15 伤害阈值门族（729/775/1088，moves_lib/j_damage_threshold.cpp）
//（2026-10-02，worktree ../agents/j15；号段 1000-1009 → T1000-T1002 三块）
//
// 断言口径：
//   - 775 回满是**重观测**型断言：受伤>200 → hp 精确回到 max（5000），门槛半边 hp<max；
//     窗口过期（rounds=3 → R4 不再回满）在本块钉死（729 与 775 同用 InRounds 窗自检，
//     729 的落地=异常不可逆、重叠刷新不可区分，过期不单测——实现 Review 记档）；
//   - 1088 吸取走 1/5 整数断言（对手 max 1000 → 恰 +200，自方留缺口防 clamp）；
//   - 729 门槛半边用 攻-6/防+6 等级铺设把单刀压到阈值下（≈134-158 < 300，85%~100%
//     浮动带内不越线），chance=100 载体使"过线必害怕"可判定；
//   - 攻击噪声：除 775 门槛半边（foe atk=1）外一律高攻打高防，断言全在 hp/异常平面。
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

// ── 载体技（切片已逐条与 side_effect/arg 核对）──
constexpr int kPaiDa   = 10004;   // 拍打：物40（对攻/填充）
constexpr int kQianXing = 30001;  // 千星死魂灭 [729]|[1,300,100,6]   物150（729 chance=100）
constexpr int kGuiKu   = 25119;   // 鬼哭神嚎 [775]|[3,200]           属（775 rounds=3 阈200）
constexpr int kGuanYan = 36047;   // 贯焱击 [1088]|[300,5]            物130（1088 阈300 1/5）

constexpr int kFear = static_cast<int>(AbnormalStatusId::Fear);   // 6

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300, int atk = 1, int hp = 99999) {
    ElfPet p = make_pet(1, "阈值者", skills, speed, atk, hp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100, int atk = 1, int hp = 99999) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, hp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1000 729 门槛半边（30001 [1,300,100,6]）══
    // 攻-6（×0.25 → 750）打 防+6（×4 → 400）：42×150×750/400/50 ≈ 236，85%~100%
    // 浮动带 201~236 < 300 → 门槛不触发（chance=100 下"过线必害怕"，无害怕=没过线）。
    // （伤害公式实测：((2L/5+2)×P×A/D)/50，L=100 → 42/50 系数。）
    {
        ScenarioBattle b(make_party(self_pet({kQianXing, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                              /*speed=*/300, /*atk=*/3000)),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(stat_change(&ctx, 0, 0, -6) == StatChangeResult::SUCCESS);   // 自攻-6
        REQUIRE(stat_change(&ctx, 1, 2, +6) == StatChangeResult::SUCCESS);   // 对方防+6
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);       // 30001
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK(ctx.getPet(1).hp < 99999, "T1000 前置：攻击已落地（造成伤害>0）");
        CHECK(!ctx.has_active_abnormal_status(1, kFear),
              "T1000 729 门槛：伤害 ≤300 不触发（chance=100 也害怕不了）");
        std::cout << "  [T1000] 729 门槛半边 通过\n";
    }

    // ══ T1001 775 回满/门槛/窗口（25119 [3,200]，self max 5000）══
    {
        {   // (a) 受伤 338±15% > 200 → 回满（hp 精确 == 5000）
            ScenarioBattle b(make_party(self_pet({kGuiKu, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 /*speed=*/300, /*atk=*/1, /*hp=*/5000)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*speed=*/100, /*atk=*/3000)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 25119
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 拍打
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, 5000,
                     "T1001.a 775：受伤>200 → 恢复自身全部体力（hp 回 5000）");
        }
        {   // (b) 门槛：受伤 1~2 ≤ 200 → 不回满
            ScenarioBattle b(make_party(self_pet({kGuiKu, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 /*speed=*/300, /*atk=*/1, /*hp=*/5000)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(ctx.getPet(0).hp < 5000 && ctx.getPet(0).hp > 4900,
                  "T1001.b 775 门槛：小伤不回满（hp 停在受伤后）");
        }
        {   // (c) 窗口：rounds=3 → R1-R3 每次大伤都回满，R4 过期不再回满
            ScenarioBattle b(make_party(self_pet({kGuiKu, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 /*speed=*/300, /*atk=*/1, /*hp=*/5000)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*speed=*/100, /*atk=*/3000)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 开窗
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, 5000, "T1001.c R1 窗内回满");
            for (int r = 2; r <= 3; ++r) {
                b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
                b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
                b.drive();
                CHECK_EQ(ctx.getPet(0).hp, 5000, "T1001.c R" + std::to_string(r) + " 窗内回满");
            }
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(ctx.getPet(0).hp < 5000, "T1001.c R4 窗口过期不再回满");
        }
        std::cout << "  [T1001] 775 回满/门槛/窗口 通过\n";
    }

    // ══ T1002 1088 吸取（36047 [300,5]，对手 max 1000 → 恰吸 200）══
    {
        {   // (a) R1 缺口铺设（挨一刀）→ R2 大于 300 的攻击吸取 +200（不触 clamp）
            ScenarioBattle b(make_party(self_pet({kPaiDa, kGuanYan, kPaiDa, kPaiDa, kPaiDa},
                                                 /*speed=*/300, /*atk=*/1000, /*hp=*/5000)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*speed=*/100, /*atk=*/3000, /*hp=*/1000)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 挨刀留缺口
            b.drive();
            const int hp_r1 = ctx.getPet(0).hp;
            CHECK(hp_r1 < 4800 && hp_r1 > 2000, "T1002 前置：R1 留下吸血缺口");
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 贯焱击
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp - hp_r1, 200,
                     "T1002.a 1088：伤害>300 → 吸取对手最大体力 1/5（恰 +200）");
        }
        {   // (b) 门槛：atk=1 伤害 1~2 ≤ 300 → 不吸取
            ScenarioBattle b(make_party(self_pet({kPaiDa, kGuanYan, kPaiDa, kPaiDa, kPaiDa},
                                                 /*speed=*/300, /*atk=*/1, /*hp=*/5000)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*hp=*/1000)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            const int hp_r1 = ctx.getPet(0).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, hp_r1,
                    "T1002.b 1088 门槛：伤害 ≤300 不吸取");
        }
        std::cout << "  [T1002] 1088 吸取/门槛 通过\n";
    }

    std::cout << "scenario_1000 PASS (J15 伤害阈值门族)" << std::endl;
    return 0;
}
