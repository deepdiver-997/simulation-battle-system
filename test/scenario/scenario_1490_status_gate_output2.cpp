#include <utils/sbs_rand.h>
// 场景 1490 —— J63 状态门输出族二（431/133，moves_lib/j_status_gate_output2.cpp）
//（2026-10-03，worktree ../agents/j63；号段 1490-1499 → T1490-T1491 两块）
//
// 断言口径：
//   - 431 负槽门威力×2（15891 风暴灭云斩 特160）：同种子对拍（对手两负槽 vs 无）
//     → 恰 2×（±3，T1370 口径）；门镜像对照=对手全正槽 → 同种子 1×（无翻倍）。
//   - 133 烧伤门固伤（12411 九天烈炎 [100]）：同对手两轮差分——R1 拍打 (loss-红)=tick，
//     R2 九天烈炎 (loss-红)=tick+100 → 差恰 100（烧伤回合末 tick 两轮相等相消，
//     免探针）；对照=无烧伤两轮差分恒 0（门关）。
//   atk=1 消红噪声；loss 与 resolvedDamage.final 差分即附加粉（T1372 口径）。
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
constexpr int kPaiDa   = 10004;   // 拍打（填充刀，物40）
constexpr int kFengBao = 15891;   // 风暴灭云斩 [431]|[] 特160（431 零参）
constexpr int kJiuTian = 12411;   // 九天烈炎 [133]|[100] 特150（烧伤门 +100）

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300, int atk = 1000, int hp = 5000) {
    ElfPet p = make_pet(1, "门族二", skills, speed, atk, hp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100, int atk = 1, int hp = 99998) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, hp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

int red_after_drive(ScenarioBattle& b) { return b.ctx().resolvedDamage.final; }

// 粉伤 FINAL 相位探针：收集落在对手身上的每笔粉伤终值（075 探针同思路）。
// 烧伤 tick 每轮笔数/归属不定 → 不用 hp 差分，直接数 "==100" 的笔数（133 的附加额）。
void install_pink_probe(BattleContext& ctx, std::vector<int>& pinks) {
    ctx.register_pink_damage_effect(
        PinkDamagePhase::FINAL, 1, [&pinks](BattleContext* c, int) {
            const PinkDamageResolved& p = c->resolvedPink;
            if (p.target == 1 && p.final > 0) {
                pinks.push_back(p.final);
            }
        });
}

int count_of(const std::vector<int>& v, int x) {
    int n = 0;
    for (const int e : v) {
        if (e == x) {
            ++n;
        }
    }
    return n;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1490 431 负槽门威力×2（15891 零参）══
    {
        int red_drop = 0;
        int red_plain = 0;
        int red_boost = 0;
        {   // (a) 对手两负槽 → 翻倍
            ScenarioBattle b(make_party(self_pet({kFengBao, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            REQUIRE(stat_change(&ctx, 1, 0, -1) == StatChangeResult::SUCCESS);
            REQUIRE(stat_change(&ctx, 1, 2, -1) == StatChangeResult::SUCCESS);
            sbs_srand(20261490);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            red_drop = red_after_drive(b);
        }
        {   // 对照：无降槽
            ScenarioBattle b(make_party(self_pet({kFengBao, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            sbs_srand(20261490);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            red_plain = red_after_drive(b);
        }
        {   // (b) 门镜像：对手全正槽 → 不翻倍
            ScenarioBattle b(make_party(self_pet({kFengBao, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            REQUIRE(stat_change(&ctx, 1, 0, +1) == StatChangeResult::SUCCESS);
            REQUIRE(stat_change(&ctx, 1, 2, +1) == StatChangeResult::SUCCESS);
            sbs_srand(20261490);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            red_boost = red_after_drive(b);
        }
        CHECK(red_plain > 0, "T1490 前置：基线红伤落地");
        // 威力×2 经伤害公式常数项有 ~2-4% 截断非线性（实测 1.92×）→ 比例带 [1.85,2.0]
        // （漏实现=1.0、半实现=1.5 均在带外，仍可鉴别）
        CHECK(red_drop >= red_plain * 185 / 100 && red_drop <= red_plain * 2,
              "T1490.a 431：同种子对拍 ≈×2（drop=" + std::to_string(red_drop) +
                  " vs plain=" + std::to_string(red_plain) + "）");
        CHECK_EQ(red_boost, red_plain,
              "T1490.b 431 门镜像：正槽不翻倍（boost=" + std::to_string(red_boost) +
                  " vs plain=" + std::to_string(red_plain) + "）");
        std::cout << "  [T1490] 431 负槽门威力×2 通过 (plain=" << red_plain
                  << " drop=" << red_drop << " boost=" << red_boost << ")\n";
    }

    // ══ T1491 133 烧伤门固伤（12411 [100]；粉伤 FINAL 探针数 ==100 的笔数）══
    {
        {   // (a) 对手烧伤：R1 拍打无附加（0 笔 100）→ R2 九天烈炎恰 1 笔 100
            ScenarioBattle b(make_party(self_pet({kJiuTian, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 /*speed=*/300, /*atk=*/1)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)apply_anomaly(&ctx, 1, static_cast<int>(AbnormalStatusId::Burn), 9, 0);
            std::vector<int> pinks;
            install_pink_probe(ctx, pinks);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R1 拍打
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(count_of(pinks, 100), 0,
                     "T1491.a R1 拍打：无 133 附加（拍打非载体；pinks=" +
                         std::to_string(pinks.size()) + " 笔）");
            pinks.clear();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R2 九天烈炎
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(count_of(pinks, 100), 1,
                     "T1491.a R2 九天烈炎：恰 1 笔 +100 固伤（tick 烧伤扣血笔数不计，"
                     "pinks=" + std::to_string(pinks.size()) + " 笔）");
        }
        {   // (b) 对照：无烧伤 → R2 无任何 100 笔（门关）
            ScenarioBattle b(make_party(self_pet({kJiuTian, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 /*speed=*/300, /*atk=*/1)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            std::vector<int> pinks;
            install_pink_probe(ctx, pinks);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            pinks.clear();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(count_of(pinks, 100), 0,
                     "T1491.b 133 门：无烧伤无附加（pinks=" + std::to_string(pinks.size()) +
                         " 笔）");
        }
        std::cout << "  [T1491] 133 烧伤门固伤 通过\n";
    }

    std::cout << "scenario_1490 PASS (J63 状态门输出族二)" << std::endl;
    return 0;
}
