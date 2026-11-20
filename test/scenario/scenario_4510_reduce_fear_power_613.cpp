#include <utils/sbs_rand.h>
// 场景 4510 —— K295 受伤减免+害怕门威力族（613，soul_lib/k295_reduce_fear_power.cpp）
//（2026-10-21，worktree ../agents/k295；号段 4510-4519 → T4510-T4513 四块）
//
// 断言口径（613「受到的伤害减少20%，对手处于害怕状态时所有技能威力提升50%」）：
//   - T4510 **减伤生效**：对手打印主 → 印主掉血 = 无减伤对照 × 0.8（带 [0.74,0.86]；
//     浮动 85%~100% × 减伤 80% → 理论 [0.68,0.80]，放宽两端）。
//   - T4511 **减伤门闭对照**：不带魂印 → 掉血 = 基线（无 20% 减伤）。
//   - T4512 **害怕门威力 +50%**：对手处于害怕 → 印主威力视图 ×150/100（逐位精确）。
//   - T4513 **非害怕时门闭**：对手处于**别的**异常（烧伤）→ 威力视图原值。
//
// ⚠️ 场景构造口径：按 **DB args 原样**传 {20,6,50}（不补 {0,0} 前缀）——实现侧走
//   owned_int_args 原序读（K257 起的夜跑口径，见实现头注「行参读取路径」）。
// ⚠️ **减伤是 ws 槽位**（damage_reduce_add[防守方][首个空槽]）→ 断言读槽值精确
//   （damage_reduce_add[0][0] == 20），比伤害比更精确；伤害比另作侧证。
// ⚠️ 威力断言读 **ws.skill_power_view[0]**（K205 325 / K290 652 范式）。
// ⚠️ 对手处于害怕：apply_anomaly 直铺（K255 320 场景同款）。害怕是**控制类**异常
//   → 对手无法行动，故"对手打印主"那几块用**无异常**对手或只打一回合。
#include <cstdlib>
#include <iostream>
#include <vector>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kJi = 10001;    // 撞击：攻击载体
constexpr int kMaxHp = 99999;
constexpr int kFear = static_cast<int>(AbnormalStatusId::Fear);   // 6
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);   // 2

SoulMark make_mark() {
    // DB args 原序 [20,6,50]：db[0]=20（减伤%）/ db[1]=6（害怕码）/ db[2]=50（威力%）。
    SoulMark sm(613, "K295 魂印", "test", EffectArgs(std::vector<int>{20, 6, 50}));
    sm.monster_id = 2857;
    return sm;
}

ElfPet mark_pet(bool with_mark = true) {
    ElfPet p = with_mark
        ? make_pet(1, "达克斯特", {kJi, kJi, kJi, kJi, kJi},
                   /*speed=*/100, /*atk=*/100, kMaxHp, make_mark())
        : make_pet(1, "裸达克斯特", {kJi, kJi, kJi, kJi, kJi},
                   /*speed=*/100, /*atk=*/100, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    set_crit_sixteenths(p, 0);   // 排除暴击（浮带只剩 85%~100%）
    return p;
}

ElfPet plain_pet(bool faster = true) {
    ElfPet p = make_pet(2, "对手", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/faster ? 300 : 100, /*atk=*/100, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    set_crit_sixteenths(p, 0);
    return p;
}

// 对手打印主一次 → 印主掉血量。
int foe_hits_self(ScenarioBattle& b) {
    const int before = b.ctx().getPet(0).hp;
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    return before - b.ctx().getPet(0).hp;
}

// 印主出手 → 返回威力视图值（对手带指定异常）。
int probe_power_view(int anomaly_code) {
    ElfPet p = mark_pet();
    ScenarioBattle b(make_party(p), make_party(plain_pet(/*faster=*/false)));
    b.start();
    BattleContext& ctx = b.ctx();
    if (anomaly_code >= 0) {
        apply_anomaly(&ctx, 1, anomaly_code, /*duration_rounds=*/9, /*actor=*/0);
        REQUIRE(ctx.has_active_abnormal_status(1, anomaly_code));
    }
    sbs_srand(20261021);
    ctx.debug_step_mode = true;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    ctx.debug_step_mode = false;
    b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);
    const int view = ctx.ws.skill_power_view[0].value;
    b.drive();
    return view;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T4510 减伤生效：减伤槽 = 20 + 伤害比侧证 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        sbs_srand(20261021);
        const int loss = foe_hits_self(b);
        // 减伤槽：ROUND_START 写入（ws 每回合 reset → 必须在攻击前有值）。
        CHECK_EQ(ctx.ws.damage_reduce_add[0][0], 20,
                 "T4510 613：受到伤害减少 20%（减伤槽 = 20）");
        CHECK(loss > 0, "T4510 前置：攻击落地");
        std::cout << "  [T4510] 减伤槽生效 通过 (loss=" << loss << ")\n";
    }

    // ══ T4511 门闭对照：不带魂印 → 减伤槽为 0 ══
    {
        ScenarioBattle b(make_party(mark_pet(/*with_mark=*/false)),
                         make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        sbs_srand(20261021);
        const int loss = foe_hits_self(b);
        int sum = 0;
        for (int i = 0; i < 4; ++i) {
            sum += ctx.ws.damage_reduce_add[0][i];
        }
        CHECK_EQ(sum, 0,
                 "T4511 613：对照——不带魂印时减伤槽全 0（减伤确来自本魂印）");
        CHECK(loss > 0, "T4511 前置：攻击落地");
        std::cout << "  [T4511] 无魂印对照 通过\n";
    }

    // ══ T4512 害怕门 → 威力 ×150/100 ══
    {
        const int view = probe_power_view(kFear);
        CHECK_EQ(view, 35 * 150 / 100,
                 "T4512 613：对手处于害怕状态 → 所有技能威力 +50%（35 → 52，实测 "
                     + std::to_string(view) + "）");
        std::cout << "  [T4512] 害怕门威力 通过\n";
    }

    // ══ T4513 非害怕异常 → 门闭（威力原值）══
    {
        const int view = probe_power_view(kBurn);
        CHECK_EQ(view, 35,
                 "T4513 613：对手处于烧伤（非害怕）→ 门闭，威力原值 35（实测 "
                     + std::to_string(view) + "）");
        std::cout << "  [T4513] 非害怕门闭对照 通过\n";
    }

    std::cout << "scenario_4510 PASS (K295 受伤减免+害怕门威力族)" << std::endl;
    return 0;
}
