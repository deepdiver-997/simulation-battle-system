// 场景 1080 —— J23 状态条件输出族（3 宏，moves_lib/j_status_amp.cpp）
//（2026-10-02，worktree ../agents/j23；号段 1080-1089 → T1080-T1082 三块）
//
// 断言口径：
//   - 97/500 用双局对照（载体打无状态局 vs 带状态局）：比值带 2×[0.85,1/0.85]
//     =[170,235]%；
//   - 97 的冻伤局有 1/8 最大体力回合末扣血 tick → 用 max=800000（tick=100000 整除）
//     做扣减法隔离己方伤害：hit = loss_total − max/8；
//   - 500 的害怕无扣血 tick，直接比掉血；
//   - 36 用极度冰点 [100]（100% 秒杀，pow0 特攻——命中即发 EVENT_HIT）→ 对手倒下；
//   - 载体链排查：11596/33397/10210 均单效果载体。
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

constexpr int kPaiDa  = 10004;   // 拍打：物40
constexpr int kYongHeng = 11596; // 永恒冰封[97]：特90（冻伤→威力翻倍，零参）
constexpr int kZhenHan  = 33397; // 震撼音律[500]：特130（害怕→伤害翻倍，零参）
constexpr int kJiDu     = 10210; // 极度冰点[36]|[100]：特0（命中即 100% 秒杀）
constexpr int kFrostbite = static_cast<int>(AbnormalStatusId::Frostbite);   // 5
constexpr int kFear      = static_cast<int>(AbnormalStatusId::Fear);        // 6
constexpr int kMaxHp   = 800000;   // 1/8 = 100000 整除（冻伤 tick 扣减法用）

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300, int atk = 300) {
    ElfPet p = make_pet(1, "己方", skills, speed, atk, kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100, int atk = 1) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, kMaxHp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1080 97 冻伤→威力翻倍（永恒冰封，双局对照）══
    {
        int d_norm = 0, hit_frozen = 0;
        {
            ScenarioBattle b(make_party(self_pet({kYongHeng, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            d_norm = kMaxHp - ctx.getPet(1).hp;
        }
        CHECK(d_norm > 50, "T1080 前置：无状态局正伤");
        {
            ScenarioBattle b(make_party(self_pet({kYongHeng, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)apply_anomaly(&ctx, 1, kFrostbite, /*duration_rounds=*/9, /*actor=*/0);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            // 冻伤回合末扣 1/8 max（=100000）→ 扣减法隔离己方伤害
            const int loss_total = kMaxHp - ctx.getPet(1).hp;
            hit_frozen = loss_total - kMaxHp / 8;
        }
        const int ratio_x100 = hit_frozen * 100 / d_norm;
        CHECK(ratio_x100 >= 170 && ratio_x100 <= 235,
              "T1080 97：冻伤局威力翻倍（比值 " + std::to_string(ratio_x100) +
                  "%，带 170~235；无状态=" + std::to_string(d_norm) +
                  " 冻伤=" + std::to_string(hit_frozen) + "）");
        std::cout << "  [T1080] 97 冻伤威力翻倍 通过 (" << ratio_x100 << "%)\n";
    }

    // ══ T1081 500 害怕→伤害翻倍（震撼音律，双局对照）══
    {
        int d_norm = 0, d_fear = 0;
        {
            ScenarioBattle b(make_party(self_pet({kZhenHan, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            d_norm = kMaxHp - ctx.getPet(1).hp;
        }
        CHECK(d_norm > 50, "T1081 前置：无状态局正伤");
        {
            ScenarioBattle b(make_party(self_pet({kZhenHan, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)apply_anomaly(&ctx, 1, kFear, /*duration_rounds=*/9, /*actor=*/0);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            d_fear = kMaxHp - ctx.getPet(1).hp;
        }
        const int ratio_x100 = d_fear * 100 / d_norm;
        CHECK(ratio_x100 >= 170 && ratio_x100 <= 235,
              "T1081 500：害怕局伤害翻倍（比值 " + std::to_string(ratio_x100) +
                  "%，带 170~235；无状态=" + std::to_string(d_norm) +
                  " 害怕=" + std::to_string(d_fear) + "）");
        std::cout << "  [T1081] 500 害怕伤害翻倍 通过 (" << ratio_x100 << "%)\n";
    }

    // ══ T1082 36 命中时100%秒杀（极度冰点 [36]|[100]）══
    {
        ScenarioBattle b(make_party(self_pet({kJiDu, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK(ctx.getPet(1).hp <= 0,
              "T1082 36：命中 100% 秒杀（对手 hp=" + std::to_string(ctx.getPet(1).hp) + "）");
        std::cout << "  [T1082] 36 命中秒杀 通过\n";
    }

    std::cout << "scenario_1080 PASS (J23 状态条件输出族 97/500/36)" << std::endl;
    return 0;
}
