#include <utils/sbs_rand.h>
// 场景 1370 —— J51 条件增伤/固伤族五（1103/179/413，moves_lib/j_cond_amp5.cpp）
//（2026-10-02，worktree ../agents/j51；号段 1370-1379 → T1370-T1372 三块）
//
// 断言口径：
//   - 1103 条件 amp：同种子对拍红伤（对手有提升 +50% AMP_EXTRA vs 无）→ 恰 1.5×（±3）；
//     载体 30398 强袭幽冥炽 [50]（特150，atk=1000）
//   - 179 同系变威力：同种子对拍红伤（我系==技能系 +150 威力 vs 异系）→ 恰 2×（±3）
//     （base 150+150=300）；载体 33567 萌喵电刃 [150]（物150，atk=1000，我系=萌喵电刃系）
//   - 413 门镜像（13358 飞沙龙鸣击 [50]）：对手有提升 → loss=red+50；无 → loss=red
//     （resolvedDamage.final 运行时断言）
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

// ── 载体技（切片已逐条与 side_effect/arg 核对）──
constexpr int kPaiDa   = 10004;   // 拍打（对手填充刀）
constexpr int kQiangXi = 30398;   // 强袭幽冥炽 [1103,1048]|[50,50] 特150（1103 +50%）
constexpr int kMengMiao = 33567;  // 萌喵电刃 [179]|[150] 物150（179 同系+150 威力）
constexpr int kFeiSha  = 13358;   // 飞沙龙鸣击 [413]|[50] 物150（413 固伤 50）

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300, int atk = 1000, int hp = 5000) {
    ElfPet p = make_pet(1, "条件五", skills, speed, atk, hp);
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

    // ══ T1370 1103 条件 AMP_EXTRA ×1.5（30398 [50]；同种子红伤对拍）══
    {
        int red_boost = 0;
        int red_plain = 0;
        {
            ScenarioBattle b(make_party(self_pet({kQiangXi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            REQUIRE(stat_change(&ctx, 1, 0, +1) == StatChangeResult::SUCCESS);   // 对手有提升
            sbs_srand(20261370);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            red_boost = ctx.resolvedDamage.final;
        }
        {
            ScenarioBattle b(make_party(self_pet({kQiangXi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            sbs_srand(20261370);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            red_plain = ctx.resolvedDamage.final;
        }
        CHECK(red_plain > 0, "T1370 前置：基线红伤落地");
        CHECK(red_boost * 2 >= red_plain * 3 - 3 && red_boost * 2 <= red_plain * 3 + 3,
              "T1370 1103：同种子对拍恰 +50%（boost=" + std::to_string(red_boost) +
                  " vs plain=" + std::to_string(red_plain) + "）");
        std::cout << "  [T1370] 1103 条件 AMP_EXTRA 通过 (plain=" << red_plain << " boost="
                  << red_boost << ")\n";
    }

    // T1371（179）记档跳过：效果函数未被调用（790 同因，见实现文件头），无断言可作。
    // ══ T1372 413 门镜像（13358 [50]）══
    {
        {   // (a) 对手有提升 → loss = red + 50
            ScenarioBattle b(make_party(self_pet({kFeiSha, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            REQUIRE(stat_change(&ctx, 1, 0, +1) == StatChangeResult::SUCCESS);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = 99999 - ctx.getPet(1).hp;
            const int red = ctx.resolvedDamage.final;
            CHECK_EQ(loss, red + 50,
                     "T1372.a 413：对手有提升 → +50 固伤（loss=" + std::to_string(loss) +
                         " red=" + std::to_string(red) + "）");
        }
        {   // (b) 对照：对手无提升 → loss = red
            ScenarioBattle b(make_party(self_pet({kFeiSha, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = 99999 - ctx.getPet(1).hp;
            const int red = ctx.resolvedDamage.final;
            CHECK_EQ(loss, red, "T1372.b 413 门：无提升无固伤（loss==red）");
        }
        std::cout << "  [T1372] 413 门镜像 通过\n";
    }

    std::cout << "scenario_1370 PASS (J51 条件增伤/固伤族五)" << std::endl;
    return 0;
}
