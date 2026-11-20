// 场景 1180 —— J33 条件弱化/状态强化族（3 宏，moves_lib/j_cond_drop_boost.cpp）
//（2026-10-02，worktree ../agents/j33；号段 1180-1189 → T1180-T1182 三块）
//
// 断言口径：
//   - 414 用 ability_levels 精确断言（干净对手 -2、冻伤对手 -4——量 ×2）；
//   - 175 用 100% 概率载体（星辰之舞）确定性断言（对手带异常 → 自身防御 +1，
//     干净 → 0）；
//   - 690 用翩鸿幻彩（链上 48/507/776 不碰触发/封属面；避开 585 任意效果模板链）：
//     R1 挂窗 → 窗内清提升（E13 广播）→ R2/R3/R4 对手属性技被封（修复不掉血差），
//     R5 恢复；窗=NextRounds(3)、封属随窗截止；
//   - 冻伤局 1/8 max tick 不影响 ability_levels 断言面。
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
constexpr int kHanFeng = 34378; // 寒风侵蚀[414]|[3,-2,5]：物140（特防-2，冻伤翻倍）
constexpr int kXingChen = 12951;// 星辰之舞[175]|[2,100,1]：特150（对手异常→自身防御+1）
constexpr int kPianHong = 24399;// 翩鸿幻彩[690,48,507,776]|[3,5,200,2]：属（提升消失→封属）
constexpr int kXiuFu = 20309;   // 神经修复：属（对手属性技探针）
constexpr int kFrostbite = static_cast<int>(AbnormalStatusId::Frostbite);
constexpr int kPoison = static_cast<int>(AbnormalStatusId::Poison);
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

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1180 414 冻伤对手→弱化翻倍（寒风侵蚀 [414]|[3,-2,5]）══
    {
        int srd_clean = 0, srd_frozen = 0;
        {
            ScenarioBattle b(
                make_party(self_pet({kHanFeng, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            srd_clean = ctx.ability_levels[1][3];
        }
        CHECK_EQ(srd_clean, -2, "T1180.a 414：干净对手特防 -2");
        {
            ScenarioBattle b(
                make_party(self_pet({kHanFeng, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)apply_anomaly(&ctx, 1, kFrostbite, /*duration_rounds=*/9, /*actor=*/0);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            srd_frozen = ctx.ability_levels[1][3];
        }
        CHECK_EQ(srd_frozen, -4, "T1180.b 414：冻伤对手特防翻倍 -4");
        std::cout << "  [T1180] 414 条件弱化翻倍 通过\n";
    }

    // ══ T1181 175 对手异常→自身防御+1（星辰之舞 [175]|[2,100,1]）══
    {
        {
            ScenarioBattle b(
                make_party(self_pet({kXingChen, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)apply_anomaly(&ctx, 1, kPoison, /*duration_rounds=*/9, /*actor=*/0);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.ability_levels[0][2], 1,
                     "T1181.a 175：对手异常 → 自身防御 +1");
        }
        {
            ScenarioBattle b(
                make_party(self_pet({kXingChen, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.ability_levels[0][2], 0, "T1181.b 175 门：对手干净不触发");
        }
        std::cout << "  [T1181] 175 条件自我强化 通过\n";
    }

    // ══ T1182 690 提升消失→封对手属性技（翩鸿幻彩 [690]|[3]，窗=下3回合）══
    {
        ScenarioBattle b(
            make_party(self_pet({kPianHong, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
            make_party(foe_pet({kXiuFu, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(1).hp = kMaxHp / 2;   // 预扣对手血（修复可观测）
        (void)stat_change(&ctx, 0, 0, 2);   // 自身有提升（供窗内消失）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 挂窗
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        const int hp0 = ctx.getPet(1).hp;
        CHECK_EQ(clear_stat_boosts(&ctx, 0), 1, "T1182 前置：窗内提升消失（E13 广播）");
        // R2：事件 drain → 封属；对手属性技被封锁（修复不落地）
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(1).hp, hp0, "T1182.a 690：R2 对手属性技无效（未回血）");
        for (int r = 0; r < 2; ++r) {   // R3/R4 窗内（NextRounds 3 = R2..R4）
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
        }
        CHECK_EQ(ctx.getPet(1).hp, hp0, "T1182.b 690：R3/R4 窗内持续封");
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R5 窗过期
        b.drive();
        CHECK(ctx.getPet(1).hp > hp0, "T1182.c 690：R5 窗过期恢复回血");
        std::cout << "  [T1182] 690 提升消失封属 通过\n";
    }

    std::cout << "scenario_1180 PASS (J33 条件弱化/状态强化族 414/175/690)" << std::endl;
    return 0;
}
