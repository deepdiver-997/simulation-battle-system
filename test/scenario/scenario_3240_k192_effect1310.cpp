#include <utils/sbs_rand.h>
// 场景 3240 —— K192 effect 1310（连用增伤+战阶条件分支族）：苍天歌/巳璟
//
//   A 增幅爬坡（row1 [20,80,…]）：r1 攻击后审计槽 =20、r2 后 =40（无随机精确断言）。
//   B 封顶（row2 [40,160,…]）：5 次攻击后 = min(200,160) = 160。
//   C 巳璟确定性分支：低体 + db[2]=0 → 零害怕、必走必爆支 → r2 must_crit=true、
//     先制不动（db[3]=0）。
//   D 苍天歌害怕 80%：低体 40 回合统计带（查后 dispel 独立观测，期望 ~32）。
//   E 门关（体力不低于对手）：零害怕、r2 无必爆。
#include <cstdlib>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kYanFan = 10037;   // 燕返：威力 60（直接攻击载体）
constexpr int kGaoSu = 20017;    // 高速移动：属性技
constexpr int kFear = static_cast<int>(AbnormalStatusId::Fear);
constexpr int kAuditKeyBase = 990713000;

SoulMark make_mark(int monster_id, int step, int cap, int fear_pct, int preempt, int crit) {
    SoulMark sm(1310, "K1310", "test", EffectArgs({step, cap, fear_pct, preempt, crit}));
    sm.monster_id = monster_id;
    return sm;
}

int audit_pct(const BattleContext& ctx) {
    const auto it = ctx.plugin_storage.find(kAuditKeyBase);
    return it == ctx.plugin_storage.end() ? 0 : std::any_cast<int>(it->second);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ A 增幅爬坡（row1）：r1→20、r2→40 ══════════════════════════════════
    {
        ElfPet holder = scenario::make_pet(1, "苍天歌", {kYanFan, kYanFan, kGaoSu, kGaoSu, kGaoSu},
                                           /*speed=*/300, /*atk=*/10, /*hp=*/1000000,
                                           make_mark(3735, 20, 80, 80, 1, 1));
        ScenarioBattle b(scenario::make_party(holder),
                         scenario::make_party(scenario::make_pet(
                             2, "沙包", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                             /*speed=*/100, /*atk=*/10, /*hp=*/1000000)));
        BattleContext& ctx = b.ctx();
        sbs_srand(20261007);
        b.start();
        for (int r = 1; r <= 2; ++r) {
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, r - 1);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
        }
        CHECK_EQ(audit_pct(ctx), 40, "A 两次攻击后增幅 =40（步长 20 累计）");
    }

    // ══ B 封顶（row2）：5 次攻击 → 160 ════════════════════════════════════
    {
        ElfPet holder = scenario::make_pet(
            1, "巳璟", {kYanFan, kYanFan, kYanFan, kYanFan, kYanFan},
            /*speed=*/300, /*atk=*/10, /*hp=*/1000000, make_mark(4719, 40, 160, 0, 0, 1));
        ScenarioBattle b(scenario::make_party(holder),
                         scenario::make_party(scenario::make_pet(
                             2, "沙包", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                             /*speed=*/100, /*atk=*/10, /*hp=*/1000000)));
        BattleContext& ctx = b.ctx();
        sbs_srand(20261007);
        b.start();
        for (int r = 0; r < 5; ++r) {
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, r);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
        }
        CHECK_EQ(audit_pct(ctx), 160, "B 5 次攻击增幅封顶 =160（min(200,160)）");
    }

    // ══ C 巳璟确定性分支：低体 → 零害怕、r2 必爆 ══════════════════════════
    {
        ElfPet holder = scenario::make_pet(
            1, "巳璟", {kYanFan, kYanFan, kYanFan, kYanFan, kYanFan},
            /*speed=*/300, /*atk=*/10, /*hp=*/1000000, make_mark(4719, 40, 160, 0, 0, 1));
        ScenarioBattle b(scenario::make_party(holder),
                         scenario::make_party(scenario::make_pet(
                             2, "沙包", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                             /*speed=*/100, /*atk=*/10, /*hp=*/1000000)));
        BattleContext& ctx = b.ctx();
        sbs_srand(20261007);
        b.start();
        ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp = 1000;   // 低体门满足
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(ctx.has_active_abnormal_status(1, kFear), false, "C db[2]=0：零害怕");
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(ctx.ws.must_crit[0], true, "C r2 必爆旗（未触发支）");
        CHECK_EQ(ctx.ws.preemptive_level[0], 0, "C db[3]=0：先制不动");
    }

    // ══ D 苍天歌害怕 80%：40 回合统计带 ═══════════════════════════════════
    {
        ElfPet holder = scenario::make_pet(
            1, "苍天歌", {kYanFan, kYanFan, kYanFan, kYanFan, kYanFan},
            /*speed=*/300, /*atk=*/10, /*hp=*/1000000, make_mark(3735, 20, 80, 80, 1, 1));
        ScenarioBattle b(scenario::make_party(holder),
                         scenario::make_party(scenario::make_pet(
                             2, "沙包", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                             /*speed=*/100, /*atk=*/10, /*hp=*/1000000)));
        BattleContext& ctx = b.ctx();
        sbs_srand(20261007);
        b.start();
        ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp = 1000;
        int wins = 0;
        for (int r = 0; r < 40; ++r) {
            b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
            if (ctx.has_active_abnormal_status(1, kFear)) {
                ++wins;
                (void)dispel_active_anomalies(&ctx, 1);
            }
        }
        std::cout << "  [D] 40 回合害怕落地 " << wins << "（期望 ~32）" << std::endl;
        CHECK(wins >= 18 && wins <= 38, "D 80% 档 40 回合统计带 18~38");
    }

    // ══ E 门关：体力不低于对手 → 零害怕无必爆 ═════════════════════════════
    {
        ElfPet holder = scenario::make_pet(
            1, "巳璟", {kYanFan, kYanFan, kYanFan, kYanFan, kYanFan},
            /*speed=*/300, /*atk=*/10, /*hp=*/1000000, make_mark(4719, 40, 160, 0, 0, 1));
        ScenarioBattle b(scenario::make_party(holder),
                         scenario::make_party(scenario::make_pet(
                             2, "沙包", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                             /*speed=*/100, /*atk=*/10, /*hp=*/1000)));
        BattleContext& ctx = b.ctx();
        sbs_srand(20261007);
        b.start();
        for (int r = 1; r <= 2; ++r) {
            b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
        }
        CHECK_EQ(ctx.has_active_abnormal_status(1, kFear), false, "E 门关零害怕");
        CHECK_EQ(ctx.ws.must_crit[0], false, "E 门关无必爆");
    }

    std::cout << "scenario_3240 PASS (K192 1310 连用增伤+战阶分支)" << std::endl;
    return 0;
}
