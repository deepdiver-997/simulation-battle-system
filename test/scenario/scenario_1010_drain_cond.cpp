// 场景 1010 —— J16 吸取条件族（3 宏，moves_lib/j_drain_cond.cpp）
//（2026-10-02，worktree ../agents/j16；号段 1010-1019 → T1010-T1012 三块）
//
// 断言口径：
//   - drain_hp 原语 = FIXED 伤 1/{n} 对手最大体力 + heal_impl 回吸（封回血生效）；
//     场景无护罩/抗性 → 精确断言（99999/3 = 33333，make_pet 整除友好）；
//   - 1257 条件负例 = 对手预挂中毒（apply_anomaly 直授，953 同款）；
//   - 1109 触发 = clear_stat_boosts 原语直呼（E13 广播 EVENT_STAT_BOOSTS_LOST 的
//     成功路径——与对手消强技同管道）；窗过期负例 = 窗外再清；
//   - 1290 低体前置 = 直写当前 hp（numericalBase 上限不变，max_hp_of 读 base）；
//   - 载体链污染排查：36967 氤氲之影 [1257] 单效果；25840 混乱魔盾链上 191=免反
//     异常票/1084=护盾（均不干扰 hp 断言）；26954 天幕岩铠链上 48/47=免票（同）。
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

constexpr int kPaiDa   = 10004;   // 拍打：物40（探针攻击）
constexpr int kYinYun  = 36967;   // 氤氲之影[1257]|[3]：物140（无异常→吸 1/3）
constexpr int kHunDun  = 25840;   // 混乱魔盾[191,1084,1109]|[4,250,3,3]（窗3回合→吸1/3）
constexpr int kTianMu  = 26954;   // 天幕岩铠[48,47,1290]|[3,3,3,3]（回1/3·低于1/3固伤）

constexpr int kParalysis = static_cast<int>(AbnormalStatusId::Paralysis);// 麻痹：无扣血 tick（中毒每回合 1/8 会污染掉血断言）
constexpr int kMaxHp  = 99999;

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

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100, int atk = 1, int hp = kMaxHp) {
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

    // ══ T1010 1257 对手无异常 → 吸取对手最大体力 1/3（氤氲之影 [1257]|[3]）══
    {
        // 正例：对手干净 → 吸 33333；对手已中毒 → 不吸（仅载体红伤 1~3）
        //（己方预扣血到 max/2：回吸走 heal_impl，满血时封顶回 0 断言不到）
        int loss_pos = 0, gain_pos = 0, loss_neg = 0, gain_neg = 0;
        {
            ScenarioBattle b(make_party(self_pet({kYinYun, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = kMaxHp / 2;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            loss_pos = kMaxHp - ctx.getPet(1).hp;
            gain_pos = ctx.getPet(0).hp - kMaxHp / 2;
        }
        CHECK(loss_pos >= kMaxHp / 3 && loss_pos <= kMaxHp / 3 + 3,
              "T1010.a 1257：吸取 1/3 对手最大体力（实测掉 " + std::to_string(loss_pos) +
                  "，期望 " + std::to_string(kMaxHp / 3) + "）");
        CHECK_EQ(gain_pos, kMaxHp / 3,
                 "T1010.a 1257：回吸等量（实测回 " + std::to_string(gain_pos) + "）");
        {
            ScenarioBattle b(make_party(self_pet({kYinYun, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = kMaxHp / 2;
            (void)apply_anomaly(&ctx, 1, kParalysis, /*duration_rounds=*/9, /*actor=*/0);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            loss_neg = kMaxHp - ctx.getPet(1).hp;
            gain_neg = ctx.getPet(0).hp - kMaxHp / 2;
        }
        CHECK(loss_neg <= 3, "T1010.b 1257 条件：对手有异常不吸取（仅红伤 "
              + std::to_string(loss_neg) + "）");
        CHECK_EQ(gain_neg, 0, "T1010.b 1257 条件：不回血");
        std::cout << "  [T1010] 1257 无异常吸取 通过 (+" << gain_pos << "/条件 " << loss_neg
                  << ")\n";
    }

    // ══ T1011 1109 窗内自身提升被消除 → 吸 1/3（混乱魔盾 1109 切片 [3,3]）══
    {
        // 正例：R1 用技（窗 3 回合）→ R1 末清自身 +2 提升（E13 广播）→ 吸 1/3
        {
            ScenarioBattle b(make_party(self_pet({kHunDun, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)stat_change(&ctx, 0, 0, 2);   // 自身有提升可被清
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int foe_hp_r1 = ctx.getPet(1).hp;   // R1 基线（链上 1084 固伤已落地）
            CHECK_EQ(clear_stat_boosts(&ctx, 0), 1, "T1011.a 前置：消除 1 项提升");
            // 事件"入队 + 晚 drain"：直呼 emit 后由下一次 FSM run 派发给 watcher。
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);   // R2
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int foe_loss = foe_hp_r1 - ctx.getPet(1).hp;
            CHECK_EQ(foe_loss, kMaxHp / 3,
                     "T1011.a 1109：窗内提升被消除 → 吸取 1/3（实测掉 " +
                         std::to_string(foe_loss) + "）");
        }
        // 负例：无提升可清（clear 返回 0 → 无广播）→ 不吸取
        {
            ScenarioBattle b(make_party(self_pet({kHunDun, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int foe_hp_r1 = ctx.getPet(1).hp;
            CHECK_EQ(clear_stat_boosts(&ctx, 0), 0, "T1011.b 前置：无提升可清");
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);   // R2
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int foe_loss = foe_hp_r1 - ctx.getPet(1).hp;
            CHECK(foe_loss <= 0, "T1011.b 1109 条件：无消除不吸取（实测掉 " +
                  std::to_string(foe_loss) + "）");
        }
        std::cout << "  [T1011] 1109 提升被消除窗 通过\n";
    }

    // ══ T1012 1290 回 1/3·低于 1/3 时对对手等量固伤（天幕岩铠 1290 切片 [3,3]）══
    {
        // a) 低体（hp = max/5 < max/3）→ 回 33333 + 对手吃等量固伤 33333
        {
            ScenarioBattle b(make_party(self_pet({kTianMu, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = kMaxHp / 5;   // 低体前置（base 不动 → max 仍 99999）
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int self_gain = ctx.getPet(0).hp - kMaxHp / 5;
            const int foe_loss = kMaxHp - ctx.getPet(1).hp;
            CHECK_EQ(self_gain, kMaxHp / 3,
                     "T1012.a 1290：恢复 1/3 最大体力（实测回 " +
                         std::to_string(self_gain) + "）");
            CHECK(foe_loss >= kMaxHp / 3 && foe_loss <= kMaxHp / 3 + 3,
                  "T1012.a 1290：低体 → 对手吃等量固伤（实测掉 " +
                      std::to_string(foe_loss) + "）");
        }
        // b) 不低体（hp = max/2）→ 只回血，无固伤
        {
            ScenarioBattle b(make_party(self_pet({kTianMu, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = kMaxHp / 2;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int self_gain = ctx.getPet(0).hp - kMaxHp / 2;
            const int foe_loss = kMaxHp - ctx.getPet(1).hp;
            CHECK_EQ(self_gain, kMaxHp / 3,
                     "T1012.b 1290：恢复照常（实测回 " + std::to_string(self_gain) + "）");
            CHECK(foe_loss <= 3, "T1012.b 1290 条件：不低体无固伤（实测掉 " +
                  std::to_string(foe_loss) + "）");
        }
        std::cout << "  [T1012] 1290 回血低体固伤 通过\n";
    }

    std::cout << "scenario_1010 PASS (J16 吸取条件族 1257/1109/1290)" << std::endl;
    return 0;
}
