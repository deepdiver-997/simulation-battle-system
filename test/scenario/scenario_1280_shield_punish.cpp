// 场景 1280 —— J43 护盾爆发/嘲讽惩罚族（2 宏，moves_lib/j43_shield_punish.cpp）
//（2026-10-02，worktree ../agents/j43；号段 1280-1289 → T1280-T1281 两块）
//
// 断言口径：
//   - 1017 精确（FIXED 0 抗）：预置 500 盾 → 用后盾清零、对手掉 500+红伤带；
//     无盾负例不掉血差；
//   - 767 用 ability_levels 门 + 掉血带：对手带 -1 下降 → 用技后 +300 固伤；
//     无下降对照只有红伤；R2（窗内）再吃一刀叠加；R4 窗过期停；
//   - 载体链排查：31284 单效果；26743 链上 952/1067 不碰固伤面。
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

constexpr int kPaiDa = 10004;   // 拍打：物40
constexpr int kWuQing = 31284;  // 无情岩爆斩[1017]：物150（消耗全部护盾→等量固伤）
constexpr int kMuLing = 26743;  // 木灵低吟[952,1067,767]|[1,4,3,2,3,3,300]：属（3回合窗+300）
constexpr int kMaxHp = 999999;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300, int atk = 1) {
    ElfPet p = make_pet(1, "己方", skills, speed, atk, kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100, int atk = 1) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, kMaxHp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

bool foe_has_drop(BattleContext& ctx) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx.ability_levels[1][i] < 0) {
            return true;
        }
    }
    return false;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1280 1017 消耗全部护盾→等量固伤（无情岩爆斩，零参）══
    {
        // a) 有盾：预置 500 → 用后盾 0、对手掉 500+红伤带
        {
            ScenarioBattle b(
                make_party(self_pet({kWuQing, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).shield_bank_.add(/*priority=*/0, /*quantity=*/500,
                                           /*source_id=*/99001, /*duration=*/0);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).shield_bank_.total(), 0, "T1280.a 前置：护盾清零");
            const int loss = kMaxHp - ctx.getPet(1).hp;
            CHECK(loss >= 500 && loss <= 504,
                  "T1280.a 1017：等量固伤 500（实测掉 " + std::to_string(loss) +
                      "，含载体红伤）");
        }
        // b) 无盾负例：只有红伤
        {
            ScenarioBattle b(
                make_party(self_pet({kWuQing, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = kMaxHp - ctx.getPet(1).hp;
            CHECK(loss >= 0 && loss <= 4, "T1280.b 1017 门：无盾无固伤（实测掉 " +
                  std::to_string(loss) + "）");
        }
        std::cout << "  [T1280] 1017 护盾爆发 通过\n";
    }

    // ══ T1281 767 窗内对手有下降→+300 固伤（木灵低吟 [767]|[3,300]）══
    {
        // a) 正例：对手带 -1 → R1 用技（属，无红伤）→ 出手结束 +300
        {
            ScenarioBattle b(
                make_party(self_pet({kMuLing, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)stat_drop(&ctx, 1, 0, 1, 0);   // 对手攻击 -1（有下降）
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = kMaxHp - ctx.getPet(1).hp;
            CHECK(loss == 300 || (loss >= 300 && loss <= 302),
                  "T1281.a 767：对手有下降 → +300 固伤（实测掉 " + std::to_string(loss) +
                      "，容链上零星红伤）");
            // b) 窗内对照：清掉对手下降 → R2 用技不附加
            (void)stat_change(&ctx, 1, 0, 1);   // 补回（清下降）
            const int hp0 = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss_r2 = hp0 - ctx.getPet(1).hp;
            CHECK(loss_r2 >= 0 && loss_r2 <= 4,
                  "T1281.b 767 门：无下降不附加（实测掉 " + std::to_string(loss_r2) +
                      "，仅拍打红伤）");
        }
        // c) 窗过期：R1 挂窗（3 回合）后空转 3 场 → R5 用技不附加
        {
            ScenarioBattle b(
                make_party(self_pet({kMuLing, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)stat_drop(&ctx, 1, 0, 1, 0);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 挂窗
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            for (int r = 0; r < 3; ++r) {   // R2..R4 空转过窗
                b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
                b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
                b.drive();
            }
            const int hp0 = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R5 拍打
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = hp0 - ctx.getPet(1).hp;
            CHECK(loss >= 0 && loss <= 4, "T1281.c 767：窗过期不附加（实测掉 " +
                  std::to_string(loss) + "）");
        }
        std::cout << "  [T1281] 767 嘲讽惩罚窗 通过\n";
    }

    std::cout << "scenario_1280 PASS (J43 护盾爆发/嘲讽惩罚族 1017/767)" << std::endl;
    return 0;
}
