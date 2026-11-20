// 场景 164：斯布林蒂 4600 技能组缺口（三）—— 反馈/镜像/清场族
//（moves_lib/batch_c_skills.cpp，2026-09-27）
//
// 覆盖效果：
//   746  因律耦合 37091：5 回合内对手恢复体力 → 自身恢复等量的 4/5（EVENT_HEAL_RESTORED 镜像）
//   1192 彼空·真识本柢 37093：自身能力下降反馈给对手；反馈失败（免弱）→ 解除自身下降
//   1447 同技能：体力高于 1/2 → 100% 令对手睡眠（含"高于"严格不含等号的边界）
//   1135 同技能：体力低于 1/2 → 回满 + 等量固定伤害
//   1211 因子解离 37092：清双方提升+下降，消除成功 → 自身 300 护盾
//   1850 同技能：消双方回合类效果，成功 → 自身下 1 次攻击伤害 +50%
//        （授予延后到 AFTER_ACTION——不喂给自己这次攻击，1256 同款）
//
// 确定性纪律：红伤随机 → 不断言 exact；粉/固/真伤与等级/授予结构才 CHECK_EQ。
#include <cstdlib>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kYinlv = 37091;    // 因律耦合：746（特攻 130）
constexpr int kBikong = 37093;   // 彼空·真识本柢：881+1192+1447+1135（特攻 150、必中）
constexpr int kYinzi = 37092;    // 因子解离：1211+1850（物攻 140）
constexpr int kCanary1993 = 28327; // 玲·沧吟星海（1993 回合窗金丝雀，可被断回合拆）
constexpr int kFiller190 = 22638;// 虚空之遗（属性、必中；190 窗对无提升方 no-op）
constexpr int kHarmless = 10938;

ElfPet sibu(std::array<int, 5> skills, int speed = 200, int hp = 100000) {
    ElfPet p = scenario::make_pet(0, "斯布林蒂", skills, speed, /*atk=*/100, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe(std::array<int, 5> skills, SoulMark sm = {}, int hp = 100000) {
    ElfPet p = scenario::make_pet(1, "对手", skills, /*speed=*/100, /*atk=*/10, hp, sm);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== T1 746 因律耦合：对手回血 → 自身镜像 4/5 ==========
    {
        ScenarioBattle b(scenario::make_party(sibu({kYinlv, kHarmless, kHarmless, kHarmless,
                                                    kHarmless})),
                        scenario::make_party(foe({kFiller190, kHarmless, kHarmless,
                                                  kHarmless, kHarmless})));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 武装 5 回合窗
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        ctx.getPet(0).hp = 90000;   // 留出镜像回血空间
        ctx.getPet(1).hp = 50000;   // 对手有回血余量（heal 原语按实际落地发事件）
        (void)heal_amount(&ctx, 1, 500);   // 对手恢复 500（窗口内）
        ctx.event_center_.drain(&ctx, ctx.roundCount);   // 回血事件晚 drain（驱动 746 镜像）
        CHECK_EQ(ctx.getPet(1).hp, 50500, "T1 前置：对手实际恢复 500");
        CHECK_EQ(ctx.getPet(0).hp, 90000 + 400,
                 "T1 核心：对手回血 500 → 自身镜像 4/5 = 400");
        std::cout << "  [T1] 746 对手回血镜像 通过\n";
    }

    //========== T2 1192 反馈成功：下降镜像给对手；1447 满血附睡 ==========
    {
        ScenarioBattle b(scenario::make_party(sibu({kBikong, kHarmless, kHarmless, kHarmless,
                                                    kHarmless})),
                        scenario::make_party(foe({kFiller190, kHarmless, kHarmless,
                                                  kHarmless, kHarmless})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[0][0] = -2;   // 自身攻击-2（待反馈的下降）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.ability_levels[1][0], -2, "T2 核心：下降反馈 → 对手攻击-2");
        CHECK_EQ(ctx.ability_levels[0][0], -2, "T2 反馈成功 → 自身下降保留（不清除）");
        CHECK(ctx.has_active_abnormal_status(1, 8),
              "T2 核心：1447 满血（>1/2）→ 100% 令对手睡眠");
        std::cout << "  [T2] 1192 反馈成功 + 1447 睡眠 通过\n";
    }

    //========== T3 1192 反馈失败（对手免弱）→ 解除自身下降 ==========
    {
        ScenarioBattle b(scenario::make_party(sibu({kBikong, kHarmless, kHarmless, kHarmless,
                                                    kHarmless})),
                        scenario::make_party(foe({kFiller190, kHarmless, kHarmless,
                                                  kHarmless, kHarmless},
                                                 scenario::make_stat_drop_immunity_soul_mark())));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[0][0] = -2;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.ability_levels[1][0], 0, "T3 前置：免弱 → 反馈未落地");
        CHECK_EQ(ctx.ability_levels[0][0], 0,
                 "T3 核心：反馈失败 → 解除自身能力下降状态");
        std::cout << "  [T3] 1192 反馈失败解除 通过\n";
    }

    //========== T4 1447 边界：体力恰为 1/2（"高于"严格不含等号）→ 不睡 ==========
    {
        ScenarioBattle b(scenario::make_party(sibu({kBikong, kHarmless, kHarmless, kHarmless,
                                                    kHarmless},
                                                   /*speed=*/200, /*hp=*/100000)),
                         scenario::make_party(foe({kFiller190, kHarmless, kHarmless,
                                                   kHarmless, kHarmless})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 50000;   // 恰为最大体力的 1/2
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(!ctx.has_active_abnormal_status(1, 8),
              "T4 核心：体力恰为 1/2 不算'高于' → 不附睡");
        CHECK_EQ(ctx.getPet(0).hp, 50000, "T4 前置：1135 不触发（50000 不低于 1/2）");
        std::cout << "  [T4] 1447 边界不触发 通过\n";
    }

    //========== T5 1135：低于 1/2 → 回满 + 等量固定伤害 ==========
    {
        ScenarioBattle b(scenario::make_party(sibu({kBikong, kHarmless, kHarmless, kHarmless,
                                                    kHarmless},
                                                   /*speed=*/200, /*hp=*/100000)),
                         scenario::make_party(foe({kFiller190, kHarmless, kHarmless,
                                                   kHarmless, kHarmless})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 30000;           // 低于 1/2
        ctx.getPet(0).skills[0].power = 0;  // 主伤钳小： foe 掉血 = 固伤 70000 + 红伤(1~2)
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(0).hp, 100000, "T5 核心：恢复自身全部体力");
        const int foe_loss = 100000 - ctx.getPet(1).hp;
        CHECK(foe_loss >= 70000 + 1 && foe_loss <= 70000 + 2,
              "T5 核心：等量固定伤害 70000 + 主伤 1~2（红伤随机因子）");
        std::cout << "  [T5] 1135 回满+等量固伤 通过\n";
    }

    //========== T6 1211：清双方提升/下降 → 自身 300 护盾；无消除 → 无盾 ==========
    {
        ScenarioBattle b(scenario::make_party(sibu({kYinzi, kHarmless, kHarmless, kHarmless,
                                                    kHarmless})),
                        scenario::make_party(foe({kFiller190, kHarmless, kHarmless,
                                                  kHarmless, kHarmless})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[0][0] = 2;   // 自身有提升
        ctx.ability_levels[1][0] = 3;   // 对手有提升
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.ability_levels[0][0], 0, "T6 核心：自身提升被清");
        CHECK_EQ(ctx.ability_levels[1][0], 0, "T6 核心：对手提升被清");
        CHECK_EQ(ctx.getPet(0).shield_bank_.quantity_of(1211), 300,
                 "T6 核心：消除成功 → 自身附加 300 护盾");
        CHECK_EQ(ctx.attack_boost_grants[0].size(), 0u,
                 "T6 核心：无回合类效果可消 → 1850 不授予");
        std::cout << "  [T6] 1211 清场+护盾 通过\n";
    }

    //========== T7 1850：消双方回合类效果成功 → 下 1 次攻击伤害 +50%（金丝雀验证破窗）==========
    {
        // 金丝雀 = 1993（玲·沧吟星海）：state 桶回合窗，是断回合可见的"回合类效果"。
        // （1469 是粉伤管线窗，对 break_round_effects 不可见，不能当破窗金丝雀；
        //   其本体已由 162 T9 覆盖。对手后手 → InRounds 窗 R2 起，R1 挂上即可见。）
        ScenarioBattle b(scenario::make_party(sibu({kYinzi, kHarmless, kHarmless, kHarmless,
                                                    kHarmless})),
                        scenario::make_party(foe({kCanary1993, kFiller190, kHarmless,
                                                  kHarmless, kHarmless})));        b.start();
        BattleContext& ctx = b.ctx();
        // R1：对手武装 1993 回合窗；我 filler
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(ctx.has_round_effects(1), "T7 前置：对手 1993 回合窗已挂");
        // R2：我因子解离 → 1850 消双方回合类效果（对手窗在 → 成功）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        CHECK_EQ(ctx.attack_boost_grants[0].size(), 1u, "T7 核心：授予 1 条攻击增伤");
        CHECK_EQ(ctx.attack_boost_grants[0][0].source_effect_id, 1850, "T7 来源 = 1850");
        // 2026-09-29 off-by-one 修复：授 count+1=2（消费点 query_usage 先于应用点
        // ATTACK_DAMAGE，原授 1 实际命中 0 次——探针实证，见 scenario_721）。
        CHECK_EQ(ctx.attack_boost_grants[0][0].remaining, 2, "T7 次数 = 2（含 +1 补偿）");
        CHECK_EQ(ctx.attack_boost_grants[0][0].pct, 50, "T7 增伤 = +50%");
        CHECK(!ctx.has_round_effects(1),
              "T7 核心：对手回合类效果被消除（1993 窗已拆，R2 末不再扣 300 真伤）");
        std::cout << "  [T7] 1850 消回合+下1次增伤 通过\n";
    }

    std::cout << "scenario_164 all passed" << std::endl;
    return 0;
}
