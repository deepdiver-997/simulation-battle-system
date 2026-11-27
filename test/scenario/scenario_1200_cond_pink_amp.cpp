#include <utils/sbs_rand.h>
// 场景 1200 —— J35 条件粉伤/条件增伤族（1046/1198/943，moves_lib/j_cond_pink_amp.cpp）
//（2026-10-02，worktree ../agents/j35；号段 1200-1209 → T1200-T1202 三块）
//
// 断言口径：
//   - 1046 粉=max_hp/3 精确带（foe hp 3000 → 1000+红 1~3）；异常门对照=中毒后不附；
//   - 1198 伤害带分离（J17 口径）：对照（对手带速-2，条件不成立）vs 测试（对手净）
//     → 测试/对照 ∈ (1.2, 1.8)——恰 +50% 落 1.275~1.5；
//   - 943 粉=视图双防和×30%（运行时读取算期望，粉+红 1~3 带）；atk=1 消红噪声。
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
constexpr int kTianShe = 19989;   // 王·天蛇决意 [1046]|[3] 特160（1046 分母 3）
constexpr int kZaiE    = 30895;   // 妃·灾厄黎明 [1198]|[50] 特160（1198 +50%）
constexpr int kKuangYe = 19615;   // 狂野搏击 [943]|[30] 物85（943 双防 30%）
constexpr int kPaiDa   = 10004;   // 拍打（填充）
constexpr int kGaoSu   = 20017;   // 高速移动（filler）

constexpr int kPoison = static_cast<int>(AbnormalStatusId::Poison);   // 1

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300, int atk = 1, int hp = 5000) {
    ElfPet p = make_pet(1, "条件者", skills, speed, atk, hp);
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

    // ══ T1200 1046 异常门（19989 [3]；foe hp 3000 → 粉恰 1000）══
    {
        {   // (a) 对手无异常 → 粉 1000（+红 1~3）
            ScenarioBattle b(make_party(self_pet({kTianShe, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*speed=*/100, /*atk=*/1, /*hp=*/3000)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = 3000 - ctx.getPet(1).hp;
            CHECK(loss >= 1000 && loss <= 1004,
                  "T1200.a 1046：粉 1000（max_hp/3）+红（实测 " + std::to_string(loss) + "）");
        }
        {   // (b) 对照：对手中毒 → 门关不附
            ScenarioBattle b(make_party(self_pet({kTianShe, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*speed=*/100, /*atk=*/1, /*hp=*/3000)));
            b.start();
            BattleContext& ctx = b.ctx();
            REQUIRE(try_apply_anomaly(&ctx, 1, kPoison, /*duration=*/3, /*actor=*/0));
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = 3000 - ctx.getPet(1).hp;
            CHECK(loss < 500, "T1200.b 1046 门：对手处于异常不附粉（中毒跳伤 ~375 属噪声，实测 " +
                                  std::to_string(loss) + "）");
        }
        std::cout << "  [T1200] 1046 异常门粉伤 通过\n";
    }

    // ══ T1201 1198 条件增伤 ×1.5（30895 [50]；atk=1000；同种子对拍红伤）══
    // ⚠️ 载体伴技含 max_hp/3 粉（总量噪声大）→ 断言读 **resolvedDamage.final**（纯红伤），
    //    同种子浮动同源：red_A = red_B × 1.5（±3 取整容差）。
    {
        int red_clean = 0;
        int red_dropped = 0;
        {
            ScenarioBattle b(make_party(self_pet({kZaiE, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 /*speed=*/300, /*atk=*/1000)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.seed_rng(20261201);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            red_clean = ctx.resolvedDamage.final;   // 对手无下降 → +50%
        }
        {
            ScenarioBattle b(make_party(self_pet({kZaiE, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 /*speed=*/300, /*atk=*/1000)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            REQUIRE(stat_change(&ctx, 1, 4, -2) == StatChangeResult::SUCCESS);   // 对手速-2
            ctx.seed_rng(20261201);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            red_dropped = ctx.resolvedDamage.final;   // 对手有下降 → 基线
        }
        CHECK(red_dropped > 0, "T1201 前置：基线红伤落地");
        CHECK(red_clean * 2 >= red_dropped * 3 - 3 && red_clean * 2 <= red_dropped * 3 + 3,
              "T1201 1198：同种子对拍恰 +50%（red_clean=" + std::to_string(red_clean) +
                  " vs red_dropped=" + std::to_string(red_dropped) + "）");
        std::cout << "  [T1201] 1198 条件增伤 通过 (base=" << red_dropped << " amp="
                  << red_clean << ")\n";
    }

    // ══ T1202 943 双防粉（19615 [30]）══
    {
        ScenarioBattle b(make_party(self_pet({kKuangYe, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                             /*speed=*/300, /*atk=*/1)),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 狂野搏击
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        // 视图在回合内才物化 → drive 后读（本局无等级变化，值与攻击时一致）。
        const int def_sum = ctx.ws.getTempAbilityValue(0, NumericalPropertyIndex::DEFENSE)
            + ctx.ws.getTempAbilityValue(0, NumericalPropertyIndex::SPECIAL_DEFENSE);
        const int pink = def_sum * 30 / 100;
        CHECK(pink > 0, "T1202 前置：视图双防和>0（实测和 " + std::to_string(def_sum) + "）");
        const int loss = 99999 - ctx.getPet(1).hp;
        CHECK(loss >= pink + 1 && loss <= pink + 4,
              "T1202 943：粉 " + std::to_string(pink) + "（双防和 " +
                  std::to_string(def_sum) + "×30%）+红 1~3（实测掉 " +
                  std::to_string(loss) + "）");
        std::cout << "  [T1202] 943 双防粉 通过 (pink=" << pink << ")\n";
    }

    std::cout << "scenario_1200 PASS (J35 条件粉伤/条件增伤族)" << std::endl;
    return 0;
}
