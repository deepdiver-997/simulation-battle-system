// 场景 1500 —— J64 粉减/自状门族（2 宏，moves_lib/j_pink_reduce_selfgate.cpp）
//（2026-10-03，worktree ../agents/j64；号段 1500-1509 → T1500-T1501 两块）
//
// 断言口径：
//   - 1121 计次窗 = 场景直呼 deal_pink_damage（FIXED 粉伤走 run_pink_damage 管线，
//     REDUCE_EXTRA 阶段生效——battle_primitives.cpp 1225 入口）：400 固伤首击 -50% 掉
//     200、次击预算耗尽掉 400；负例无 buff 掉 400；
//   - 1169 = 自身预挂中毒（apply_anomaly 直授，953 同款）：红伤带 ×2（AMP_EXTRA 乘法
//     通道：物160·atk200 带 [230,270] → [460,540]），回合末中毒被解除（has_active
//     翻转）；负例 = 干净自身走原带；
//   - 中毒选型：无行动封锁（麻痹会吞出手）、自伤 tick 只掉自身血不碰断言面；
//   - 载体链污染排查：25425 刀剑如梦链上 1154=使用技能附加（只吃自身后续用技，测试
//     期无后续用技）/48=异常免疫（无害）；30706 真龙焚天舞链首 797=消对手回合类
//     （空场消除失败=良性，已注册）。
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

constexpr int kPaiDa  = 10004;   // 拍打：物40（探针攻击）
constexpr int kDaoJian = 25425;  // 刀剑如梦[1121,1154,48]|[1,50,3,200,0,5]：属性技（下1次粉伤-50%）
constexpr int kZhenLong = 30706; // 真龙焚天舞[797,1169]|[2,100]：物160（自身有异常→伤害额外+100%）

constexpr int kPoison = static_cast<int>(AbnormalStatusId::Poison);   // 中毒=1
constexpr int kMaxHp  = 99999;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300, int atk = 200) {
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

    // ══ T1500 1121 下 1 次受到的固伤/百分伤 -50%（刀剑如梦 [1121]|[1,50]）══
    {
        // a) 正例：R1 用属性技上 buff → 直呼粉伤 400：首击 -50% 掉 200，次击掉 400
        {
            ScenarioBattle b(make_party(self_pet({kDaoJian, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 刀剑如梦
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int hp0 = ctx.getPet(0).hp;
            (void)deal_pink_damage(&ctx, 0, 400, DamageKind::FIXED, 1);
            const int first = hp0 - ctx.getPet(0).hp;
            CHECK_EQ(first, 200, "T1500.a 1121：首击 400 固伤 -50%（实测掉 " +
                                     std::to_string(first) + "）");
            const int hp1 = ctx.getPet(0).hp;
            (void)deal_pink_damage(&ctx, 0, 400, DamageKind::FIXED, 1);
            const int second = hp1 - ctx.getPet(0).hp;
            CHECK_EQ(second, 400, "T1500.a 1121：次击预算耗尽掉全额（实测掉 " +
                                      std::to_string(second) + "）");
        }
        // b) 负例：无 buff → 粉伤全额
        {
            ScenarioBattle b(make_party(self_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            const int hp0 = ctx.getPet(0).hp;
            (void)deal_pink_damage(&ctx, 0, 400, DamageKind::FIXED, 1);
            const int taken = hp0 - ctx.getPet(0).hp;
            CHECK_EQ(taken, 400, "T1500.b 1121 门：无 buff 粉伤全额（实测掉 " +
                                     std::to_string(taken) + "）");
        }
        std::cout << "  [T1500] 1121 粉伤计次减免 通过\n";
    }

    // ══ T1501 1169 自身有异常 → 伤害额外+100%·回合末解除（真龙焚天舞 [1169]|[100]）══
    {
        // a) 正例：预挂中毒 → 红伤 ×2（[460,540]），回合末中毒被解除
        {
            ScenarioBattle b(make_party(self_pet({kZhenLong, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            CHECK(ctx.has_active_abnormal_status(0, kPoison) == false,
                  "T1501.a 前置：开局自身干净");
            (void)apply_anomaly(&ctx, 0, kPoison, /*duration_rounds=*/9, /*actor=*/0);
            CHECK(ctx.has_active_abnormal_status(0, kPoison),
                  "T1501.a 前置：自身已中毒");
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 真龙焚天舞
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int foe_loss = kMaxHp - ctx.getPet(1).hp;
            CHECK(foe_loss >= 460 && foe_loss <= 540,
                  "T1501.a 1169：异常中攻击 ×2（实测掉 " + std::to_string(foe_loss) +
                      "，带 [460,540]）");
            CHECK(!ctx.has_active_abnormal_status(0, kPoison),
                  "T1501.a 1169：回合末自身异常被解除");
        }
        // b) 负例：自身干净 → 原带 [230,270]（无加成）
        {
            ScenarioBattle b(make_party(self_pet({kZhenLong, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int foe_loss = kMaxHp - ctx.getPet(1).hp;
            CHECK(foe_loss >= 230 && foe_loss <= 270,
                  "T1501.b 1169 门：无异常原带（实测掉 " + std::to_string(foe_loss) +
                      "，带 [230,270]）");
        }
        std::cout << "  [T1501] 1169 自状门增伤·回合末解除 通过\n";
    }

    std::cout << "scenario_1500 PASS (J64 粉减/自状门族 1121/1169)" << std::endl;
    return 0;
}
