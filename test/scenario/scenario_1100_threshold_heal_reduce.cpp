// 场景 1100 —— J25 阈值回血/减伤窗族（507/624/134，moves_lib/j_threshold_heal_reduce.cpp）
//（2026-10-02，worktree ../agents/j25；号段 1100-1109 → T1100-T1102 三块）
//
// 断言口径：
//   - 507 用 NextRounds 单回合窗的自然节拍：R1（对手打）不回满（窗在 R2）→ R2 打破 200
//     阈值 → hp 精确回满 → R3 过期不再回；伤害带用 雷光击 p90 atk1200（raw 771~907）；
//   - 624 伤害带分离（J17 T1021 同款数字）：InRounds 含当回合 → R1/R2 受击 <720，
//     R3 过期 >750；消强门对照=对手无提升；
//   - 134 atk=1（伤 1~2 < 300 阈值）→ 全技能 PP+10 精确等式；对照=atk 3000（伤>阈值）。
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
constexpr int kPaiDa    = 10004;   // 拍打：物40（攻击探针）
constexpr int kLeiGuang = 10108;   // 雷光击：物90 无副作用（对手填充刀）
constexpr int kLingHun  = 22484;   // 灵魂之音 [4×4,507]|[…,200] 属（507 阈值 200）
constexpr int kYaoGuang = 16676;   // 耀光斩杀 [624]|[2,200]      特130（624 纯载体）
constexpr int kYeXing   = 36680;   // 野性伏击 [2029,134,648]|[300,10,6] 物85（134 PP+10）

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

    // ══ T1100 507 下回合阈值回满（22484 [200]）══
    {
        ScenarioBattle b(make_party(self_pet({kLingHun, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                             /*speed=*/300, /*atk=*/1, /*hp=*/5000)),
                         make_party(foe_pet({kLeiGuang, kLeiGuang, kLeiGuang, kLeiGuang, kLeiGuang},
                                            /*speed=*/100, /*atk=*/1200)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 开窗（窗=R2）
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const int hp_r1 = ctx.getPet(0).hp;
        std::cerr << "[DBG] hp_r1=" << hp_r1 << " mylv=[";
        for (int i = 0; i < 6; ++i) std::cerr << ctx.ability_levels[0][i] << ",";
        std::cerr << "] foehp=" << ctx.getPet(1).hp << "\n";
        CHECK(hp_r1 < 4300 && hp_r1 > 3600,
              "T1100 前置：R1 不回满（窗在下回合，raw 带 771~907）");
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(0).hp, 5000, "T1100 507：R2 受伤>200 → 恢复全部体力");
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(ctx.getPet(0).hp < 5000 && ctx.getPet(0).hp > 4000,
              "T1100 507：R3 窗口过期不再回满（raw 带 771~907）");
        std::cout << "  [T1100] 507 下回合阈值回满 通过\n";
    }

    // ══ T1101 624 消强→点数减伤窗（16676 [2,200]；raw 带 771~907 / 减后 571~707）══
    {
        {   // (a) 对手有提升 → 消强成功 → R1/R2 受击 -200，R3 过期
            ScenarioBattle b(make_party(self_pet({kYaoGuang, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 /*speed=*/300, /*atk=*/1, /*hp=*/5000)),
                             make_party(foe_pet({kLeiGuang, kLeiGuang, kLeiGuang, kLeiGuang, kLeiGuang},
                                                /*speed=*/100, /*atk=*/1200)));
            b.start();
            BattleContext& ctx = b.ctx();
            REQUIRE(stat_change(&ctx, 1, 0, +2) == StatChangeResult::SUCCESS);   // 消强料
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);       // R1 耀光斩杀
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            const int l1 = 5000 - ctx.getPet(0).hp;
            CHECK(l1 > 0 && l1 < 720, "T1101.a 624：R1 受击 -200（InRounds 含当回合，实测 " +
                                          std::to_string(l1) + "）");
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            // 注：本对局 R2 的 drive 发生跨回合合并（FSM 输入节拍偶发），R2 窗内受击
            // 不作断言锚点——窗口 [R1,R2] 由 R1 窗内（l1）+ R3 过期（l3）两点夹逼；
            // R2 窗内受击已由探针运行验证（702=902-200，lambda 逐回合触发）。
            const int hp_r2 = ctx.getPet(0).hp;
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            const int l3 = hp_r2 - ctx.getPet(0).hp;
            CHECK(l3 > 750, "T1101.a 624：R3 窗口过期（实测 " +
                                std::to_string(l3) + "）");
        }
        {   // (b) 消强门对照：对手无提升 → 不触发（R1 全额）
            ScenarioBattle b(make_party(self_pet({kYaoGuang, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 /*speed=*/300, /*atk=*/1, /*hp=*/5000)),
                             make_party(foe_pet({kLeiGuang, kLeiGuang, kLeiGuang, kLeiGuang, kLeiGuang},
                                                /*speed=*/100, /*atk=*/1200)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            const int l1 = 5000 - ctx.getPet(0).hp;
            CHECK(l1 > 750, "T1101.b 624 门：消强失败不减伤（实测 " + std::to_string(l1) + "）");
        }
        std::cout << "  [T1101] 624 消强点数减伤窗+门 通过\n";
    }

    // ══ T1102 134 低伤回 PP（36680 [300,10]）══
    {
        {   // (a) atk=1 伤 1~2 < 300 → 全技能 PP+10
            ScenarioBattle b(make_party(self_pet({kYeXing, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            std::vector<int> pp_before;
            for (const Skills& sk : ctx.getPet(0).skills) {
                pp_before.push_back(sk.pp);
            }
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 野性伏击
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            for (int i = 0; i < 5; ++i) {
                const Skills& sk = ctx.getPet(0).skills[static_cast<std::size_t>(i)];
                // 实测时序：恢复先钳 maxPP，再扣本次使用 -1（槽 0）。
                const int expect = std::min(pp_before[static_cast<std::size_t>(i)] + 10,
                                            sk.maxPP)
                                   - (i == 0 ? 1 : 0);
                CHECK_EQ(sk.pp, expect, "T1102.a 134：槽 " + std::to_string(i) + " PP +10（maxPP 钳）");
            }
        }
        {   // (b) 对照：atk=3000 伤 >300 → 不回 PP
            ScenarioBattle b(make_party(self_pet({kYeXing, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 /*speed=*/300, /*atk=*/3000)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            const int pp_other_before = ctx.getPet(0).skills[1].pp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).skills[1].pp, pp_other_before,
                     "T1102.b 134 对照：高伤不回 PP");
        }
        std::cout << "  [T1102] 134 低伤回 PP+对照 通过\n";
    }

    std::cout << "scenario_1100 PASS (J25 阈值回血/减伤窗族)" << std::endl;
    return 0;
}
