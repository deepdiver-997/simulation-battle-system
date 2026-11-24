// 场景 1780 —— J88 随机异常/同系威力族（2 宏，moves_lib/j_rand_anom_same_elem.cpp）
//（2026-10-03，worktree ../agents/j88；号段 1780-1789 → T1780-T1781 两块）
//
// 断言口径：
//   - 1291（禁灵之咒 [300,100,1] 特130 纯链，type97）：atk=1000 → 红伤>300 →
//     chance=100 随机附加 1 种异常（对对手全池，枚举计数=1）；atk=400 对照（红伤
//     <300）→ 无异常；
//   - 179（机械能量炮 [20] 特150 纯链，type6）：【J315 2026-11-02 勘误重钉】插件
//     j_cond_power_rewrite.cpp 取代解析器内建，判据 = 技能属性==使用者属性（本体
//     元素 start() 前设 {6,0}，首回合 sync 重基进视图）→ 威力 +20 平直（{0} 无 %
//     号、威力0载体 37460 唯一威力来源，口径记档见插件头注）；使用者 {3,0} → 门关；
//   - 双载体全纯链。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa  = 10004;  // 拍打：物40
constexpr int kJinLing = 34079; // 禁灵之咒 [1291]|[300,100,1]：特130（阈值随机异常载体）
constexpr int kJiXie  = 12981;  // 机械能量炮 [179]|[20]：特150 type6（同系威力载体）
constexpr int kMaxHp  = 5000;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_self(std::array<int, 5> skills, int atk) {
    ElfPet p = make_pet(1, "己方", skills, 300, atk, kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe(std::array<int, 5> skills) {
    ElfPet p = make_pet(2, "对手", skills, 100, /*atk=*/1, 99999);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

int foe_anomaly_count(const BattleContext& ctx) {
    int cnt = 0;
    for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
        if (ctx.has_active_abnormal_status(1, id)) {
            ++cnt;
        }
    }
    return cnt;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1780 1291 伤害阈值→随机 1 种异常（禁灵之咒 [300,100,1]）══
    {
        // a) atk=1000：红伤 >300 → chance=100 → 对手带 1 种异常
        {
            ScenarioBattle b(
                make_party(mk_self({kJinLing, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 1000)),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            // 全池扩入 44 改变固定种子下的抽样，可能抽到回合末回血的星哲。
            // 门判据看本刀红伤，不看回合后的净掉血。
            const auto& hit = ctx.last_landed_hit[1][ctx.on_stage[1]];
            CHECK(hit.valid && hit.actor == 0 && hit.kind == static_cast<int>(DamageKind::NORMAL)
                      && hit.amount > 300,
                  "T1780.a 前置：红伤高于 300（本刀 " + std::to_string(hit.amount) + "）");
            CHECK(foe_anomaly_count(ctx) == 1,
                  "T1780.a 1291：随机附加 1 种异常（实测 " +
                      std::to_string(foe_anomaly_count(ctx)) + "）");
        }
        // b) atk=250 对照：红伤 <300 → 无异常
        {
            ScenarioBattle b(
                make_party(mk_self({kJinLing, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 250)),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const auto& hit = ctx.last_landed_hit[1][ctx.on_stage[1]];
            CHECK(hit.valid && hit.actor == 0 && hit.kind == static_cast<int>(DamageKind::NORMAL)
                      && hit.amount < 300,
                  "T1780.b 前置：红伤低于 300（本刀 " + std::to_string(hit.amount) + "）");
            CHECK(foe_anomaly_count(ctx) == 0,
                  "T1780.b 1291 对照：阈值下不附加（实测 " +
                      std::to_string(foe_anomaly_count(ctx)) + "）");
        }
        std::cout << "  [T1780] 1291 阈值随机异常 通过\n";
    }

    // ══ T1781 179 本系（技能属性==使用者属性）→ +{0} 平直威力 ══
    // 勘误（J315 2026-11-02）：179 已由 moves_lib/j_cond_power_rewrite.cpp 插件取代
    // 解析器内建——判据从解析器旧 SameElement（双方精灵 elementalAttributes[0] 相等，
    // pet-vs-pet 降级近似）改为**技能属性==使用者属性**（DB 实证 18/18 载体学习者属性
    // =技能系别）；{0} 平直口径不变（PowerBoost += param0 同向）。机械能量炮 type6：
    // a) 使用者本体元素 {6,0}（start() 前设，首回合 sync 重基进视图）→ 门开；
    // b) 使用者 {3,0} → 门关。机械(steel) vs 草 dummy restraint=1.0（types_relation 实查）。
    {
        // a) 本系：+20 平直威力 ×1.5 STAB（威力视图 150→170；involve 读技能真实系别）
        int d_same = 0;
        {
            auto self_a = mk_self({kJiXie, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100);
            self_a.elementalAttributes[0] = 6;
            self_a.elementalAttributes[1] = 0;
            ScenarioBattle b(
                make_party(self_a),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            d_same = 99999 - b.ctx().getPet(1).hp;
        }
        // b) 异系（{3,0} ≠ type6）：加成关、无 STAB
        int d_diff = 0;
        {
            auto self_b = mk_self({kJiXie, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100);
            self_b.elementalAttributes[0] = 3;
            self_b.elementalAttributes[1] = 0;
            ScenarioBattle b(
                make_party(self_b),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            d_diff = 99999 - b.ctx().getPet(1).hp;
        }
        CHECK(d_diff > 0, "T1781 前置：基础伤 > 0（实测 " + std::to_string(d_diff) + "）");
        // a/b 差 = (0.84×170+2)×1.5×r1 − (0.84×150+2)×r2，r∈[0.85,1]、restraint=1
        // → [56.6, 108.4]；外扩余量取 [40, 130]
        CHECK(d_same - d_diff >= 40 && d_same - d_diff <= 130,
              "T1781.a 179：本系 +20 平直威力（同属 " + std::to_string(d_same) +
                  " / 异属 " + std::to_string(d_diff) + "，差 " +
                  std::to_string(d_same - d_diff) + "）");
        std::cout << "  [T1781] 179 本系平直威力 通过\n";
    }

    std::cout << "scenario_1780 PASS (J88 随机异常族 1291 + 核心179 验证)" << std::endl;
    return 0;
}
