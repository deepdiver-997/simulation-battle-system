// 场景 1430 —— J57 后手门族二（3 宏，moves_lib/j_second_gate2.cpp）
//（2026-10-03，worktree ../agents/j57；号段 1430-1439 → T1430-T1432 三块）
//
// 断言口径：
//   - 后手构造 = 自身速度 100 vs 对手 300（对手 NONE 出手隔离血量）；对照组=速度
//     倒置（先手门关）；后手判据走 acting_side_is_first（currentState 镜像）；
//   - 1357 伤害断言用比例带（红伤 85%-100% 浮动 → [1.7, 2.36]×，速查 §五）；
//   - 1092/1401 "未触发" = 对手挂 ANOMALY 免疫票（apply_anomaly_checked Modern
//     层被挡，判定确定性）→ fallback 分支；落地对照 = 无免疫 → 异常落地、无 fallback；
//   - 1092 弹粉 = PERCENT_VALUE 精确无浮动（自身当前体力 1000/3=333 + 红伤~2 →
//     窄带 [331,338]）；1401 未触发必暴 = crit_happened 旗（crit_rate=0 下置位）。
//   - 载体全纯链（36504 [1357]|[100] / 33493 [1092]|[6,3] / 36430 [1401]|[100,16,1]）
//     ——无兄弟链激活风险。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <fsm/state.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa  = 10004;  // 拍打：物40，crit_rate=0
constexpr int kYiJing = 36504;  // 以静制动 [1357]|[100]：特130（后手增伤载体）
constexpr int kHongTian = 33493; // 轰天双臂转 [1092]|[6,3]：物150（害怕/弹粉载体）
constexpr int kKuiHen = 36430;  // 魁痕 [1401]|[100,16,1]：物120（异常/必暴载体）
constexpr int kMaxHp  = 1000;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_self(std::array<int, 5> skills, int speed, int atk = 1) {
    ElfPet p = make_pet(1, "己方", skills, speed, atk, kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe(std::array<int, 5> skills, int speed) {
    ElfPet p = make_pet(2, "对手", skills, speed, /*atk=*/1, 99999);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

// 对手全程 NONE（隔离对手伤害/异常面），返回对手掉血量。
// ⚠️ before 必须在提交前读——submit_operation 内部就跑 FSM，第二笔提交即整回合结算。
int foe_none_round(ScenarioBattle& b, int self_skill_idx) {
    const int before = b.ctx().getPet(1).hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, self_skill_idx);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
    return before - b.ctx().getPet(1).hp;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1430 1357 后手伤害+{0}%（以静制动 [100]，比例带）══
    {
        // a) 后手（速度 100 vs 300）→ +100%
        const int d_second = [&] {
            ScenarioBattle b(
                make_party(mk_self({kYiJing, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, /*speed=*/100,
                                   /*atk=*/20)),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300)));
            b.start();
            return foe_none_round(b, 0);
        }();
        // b) 先手对照（速度倒置）→ 门关，无 amp
        const int d_first = [&] {
            ScenarioBattle b(
                make_party(mk_self({kYiJing, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, /*speed=*/300,
                                   /*atk=*/20)),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100)));
            b.start();
            return foe_none_round(b, 0);
        }();
        CHECK(d_first > 0, "T1430 前置：先手基准伤 > 0（实测 " + std::to_string(d_first) + "）");
        // +100% 期望 2 倍；红伤 85%-100% 浮动 → 比例带 [1.7, 2.36]
        CHECK(d_second * 100 >= d_first * 170 && d_second * 100 <= d_first * 236,
              "T1430.a 1357：后手伤 " + std::to_string(d_second) + " 应≈2×先手伤 " +
                  std::to_string(d_first));
        std::cout << "  [T1430] 1357 后手增伤 通过\n";
    }

    // ══ T1431 1092 后手令对手{0}，未触发弹当前体力1/{1}粉（轰天双臂转 [6,3]）══
    {
        // a) 对手 ANOMALY 免疫 → 害怕未落地 → 弹 1000/3=333 粉（+红伤~2）
        {
            ScenarioBattle b(
                make_party(mk_self({kHongTian, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100)),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300)));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.grant_immunity(1, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                               /*duration_rounds=*/1, /*source_id=*/1092,
                               /*soul_immunity=*/false, EffectScope::ON_STAGE,
                               /*counts=*/0, /*source_effect_id=*/1092);
            const int loss = foe_none_round(b, 0);
            CHECK(loss >= 331 && loss <= 338,
                  "T1431.a 1092：未触发 → 弹当前体力/3=333（实测掉 " +
                      std::to_string(loss) + "）");
            CHECK(!ctx.has_active_abnormal_status(1, 6),
                  "T1431.a 1092：害怕未落地");
        }
        // b) 对照：无免疫 → 害怕落地 → 无弹粉
        {
            ScenarioBattle b(
                make_party(mk_self({kHongTian, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100)),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300)));
            b.start();
            BattleContext& ctx = b.ctx();
            const int loss = foe_none_round(b, 0);
            CHECK(ctx.has_active_abnormal_status(1, 6),
                  "T1431.b 1092 对照：害怕落地");
            CHECK(loss <= 5, "T1431.b 1092 对照：落地不弹粉（实测掉 " +
                                 std::to_string(loss) + "）");
        }
        std::cout << "  [T1431] 1092 后手异常/弹粉 通过\n";
    }

    // ══ T1432 1401 后手{0}%令对手{1}，未触发下{2}回合必暴（魁痕 [100,16,1]）══
    {
        // a) 对手 ANOMALY 免疫 → 异常未落地 → R2 必暴（crit_rate=0 下旗置位）
        {
            ScenarioBattle b(
                make_party(mk_self({kKuiHen, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100)),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300)));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.grant_immunity(1, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                               /*duration_rounds=*/1, /*source_id=*/1401,
                               /*soul_immunity=*/false, EffectScope::ON_STAGE,
                               /*counts=*/0, /*source_effect_id=*/1401);
            (void)foe_none_round(b, 0);   // R1 挂窗
            CHECK(!ctx.has_active_abnormal_status(1, 16),
                  "T1432.a 前置：异常码 16 未落地");
            (void)foe_none_round(b, 1);   // R2 拍打（无必暴时 0 率）
            CHECK(ctx.crit_happened[0],
                  "T1432.a 1401：未触发 → 下 1 回合必暴");
        }
        // b) 对照：无免疫 → 异常落地 → 无必暴
        {
            ScenarioBattle b(
                make_party(mk_self({kKuiHen, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100)),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300)));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)foe_none_round(b, 0);
            CHECK(ctx.has_active_abnormal_status(1, 16),
                  "T1432.b 1401 对照：异常码 16 落地");
            (void)foe_none_round(b, 1);
            CHECK(!ctx.crit_happened[0],
                  "T1432.b 1401 对照：落地不授必暴");
        }
        std::cout << "  [T1432] 1401 后手异常/必暴 通过\n";
    }

    std::cout << "scenario_1430 PASS (J57 后手门族二 1357/1092/1401)" << std::endl;
    return 0;
}
