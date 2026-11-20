#include <utils/sbs_rand.h>
// 场景 144：圣光格劳瑞 4004 技能组缺口 —— 效果 868+835（万钧奔腾 33991）+ 495（化影神雷
// 33992）（moves_lib/batch_a_skills.cpp，2026-09-27）
//
// 868："消除对手能力提升状态，消除成功则令对手下回合所有技能先制-{0}"（args=[2]）。
//   实现 = clear_stat_boosts（>0 判成功）+ NextRounds(1)+MOVE_RIGHT tick（867 同款落点）。
// 835："反转自身能力下降状态，反转成功则恢复自身全部体力"（args_num=0）。
//   实现 = AFTER_ACTION 延迟器（521/1221 口径：反转不参与本次伤害）+ REVERSED→回满。
// 495："若对手处于{0}状态，则{1}%概率秒杀对手"（args=[0,10]：麻痹→10%秒杀；
//   4399："若对手处于麻痹状态则10%几率秒杀对手"）。
//   实现 = AFTER_ACTION 延迟器：结算后复判麻痹+存活门 → force_hp_to_zero（秒杀体系）。
//
// 断言组：
//   T1 万钧奔腾主路径+顺序敏感：对手防+1 → 868 在 SKILL_EFFECT（伤害公式**之前**）消强
//      → 该击伤害高于"消的是特攻"的对照（同种子：868 都成功、968 掷点/浮动一致，
//      差值只来自防等级提前掉）；对手提升清零、自身 -2 反转为 +2、AFTER_ACTION 回满；
//      下回合 MOVE_RIGHT 对手先制-2。
//   T2 化影神雷 495：对手预先麻痹（seedSK）→ 打完不死 → 10% 秒杀收头（体力归零）；
//      对照：无麻痹 → 不秒杀。
//
// 确定性：全技能强制必中、暴击 0。rand 序（T1）：rand#1=968 的 70%（先手不翻倍档 35%×2）。
//（T2）：rand#1=749 的 50%、rand#2=伤害浮动、rand#3=495 的 10%。
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

constexpr int kWanjun = 33991;    // 万钧奔腾：物攻 85、先 3、必中、[868, 835, 968]
constexpr int kBuff = 20265;      // 锋利：属性技（自身攻击+1、无伤害）——T1 对手热身用
constexpr int kHuaying = 33992;   // 化影神雷：特攻 140、[749, 495]
constexpr int kHarmless = 10938;  // 本能一击：威力 0
constexpr int kParalysis = static_cast<int>(AbnormalStatusId::Paralysis);  // 0 麻痹

ElfPet gelaorui(std::array<int, 5> skills) {
    ElfPet p = scenario::make_pet(0, "圣光格劳瑞", skills, /*speed=*/200, /*atk=*/300,
                                  /*hp=*/3000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe(std::array<int, 5> skills, int atk, int hp) {
    ElfPet p = scenario::make_pet(1, "对手", skills, /*speed=*/100, atk, hp);
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

    //========== T1 万钧奔腾：868 消强（伤害前）+ 835 反转回满 + 下回合先制-2 ==========
    {
        // 主场：对手防御+1（868 消掉它 → 本击伤害吃到"防等级提前掉"的收益）
        int d_def = 0;
        {
            sbs_srand(21);
            ScenarioBattle b(scenario::make_party(gelaorui({kWanjun, kHarmless, kHarmless, kHarmless, kHarmless})),
                             scenario::make_party(foe({kBuff, kBuff, kBuff, kBuff, kBuff}, 100, 100000)));
            b.start();
            BattleContext& ctx = b.ctx();
            REQUIRE(stat_change(&ctx, 1, /*防御=*/2, 1) == StatChangeResult::SUCCESS);
            REQUIRE(stat_drop(&ctx, 0, /*攻击=*/0, 2, 1) == StatDropResult::SUCCESS);
            ctx.getPet(0).hp = 2200;   // 预留回满空间
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 万钧奔腾
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            d_def = ctx.ws.raw_attack_damage[0];
            CHECK_EQ(ctx.ability_level(1, 2), 0, "T1 868：对手防御提升被消除");
            CHECK_EQ(ctx.ability_level(0, 0), 2, "T1 835：自身攻击-2 反转为 +2");
            CHECK_EQ(ctx.getPet(0).hp, 3000, "T1 835：反转成功 → 恢复自身全部体力");
        }
        // 对照：同给对手防+1，但挂"免消除强化"（STAT_CLEAR）票 → 868 消除**失败**、
        // 防等级保留到伤害公式时 → 伤害更低。两场同种子、rand 序一致，差值只来自
        // "防等级是否在公式之前被 868 提前消掉"（消强时点=SKILL_EFFECT，先于公式）。
        int d_keep = 0;
        {
            sbs_srand(21);   // 同种子：968 掷点/伤害浮动一致
            ScenarioBattle b(scenario::make_party(gelaorui({kWanjun, kHarmless, kHarmless, kHarmless, kHarmless})),
                             scenario::make_party(foe({kBuff, kBuff, kBuff, kBuff, kBuff}, 100, 100000)));
            b.start();
            BattleContext& ctx = b.ctx();
            REQUIRE(stat_change(&ctx, 1, /*防御=*/2, 1) == StatChangeResult::SUCCESS);
            REQUIRE(stat_drop(&ctx, 0, /*攻击=*/0, 2, 1) == StatDropResult::SUCCESS);
            ctx.grant_immunity(1, ImmunityType::STAT_CLEAR, coverage_all(), /*mask=*/0,
                               /*duration_rounds=*/9, /*source_id=*/9994,
                               /*soul_immunity=*/false, EffectScope::ON_STAGE,
                               /*counts=*/0, /*source_effect_id=*/9994);
            REQUIRE(ctx.ability_level(1, 2) == 1);
            ctx.getPet(0).hp = 2200;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            d_keep = ctx.ws.raw_attack_damage[0];
            CHECK_EQ(ctx.ability_level(1, 2), 1, "T1 对照：免消除强化 → 防+1 保留");
        }
        CHECK(d_def > d_keep,
              "T1 顺序敏感：868 在 SKILL_EFFECT（伤害公式之前）消强 → 被消的防等级"
              "不再参与本次伤害（消除成功 > 被免消除挡下的对照）");
        std::cout << "  [T1] 868/835 主路径+消强时点 通过（消防 " << d_def
                  << " > 保留防 " << d_keep << "）\n";
    }
    // T1 续：下回合 MOVE_RIGHT 对手先制-2（868 的 NextRounds 窗口 tick）
    {
        sbs_srand(21);
        ScenarioBattle b(scenario::make_party(gelaorui({kWanjun, kHarmless, kHarmless, kHarmless, kHarmless})),
                         scenario::make_party(foe({kBuff, kBuff, kBuff, kBuff, kBuff}, 100, 100000)));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(stat_change(&ctx, 1, /*防御=*/2, 1) == StatChangeResult::SUCCESS);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.ability_level(1, 2), 0, "T1' 前置：消强成功");
        // 第 2 回合：夹在两次 submit 之间 step 到 MOVE_RIGHT（139 T5b 同款手法）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.step_past(State::BATTLE_FIRST_MOVE_RIGHT);
        ctx.debug_step_mode = false;
        CHECK_EQ(ctx.ws.preemptive_level[1], -2,
                 "T1' 核心：下回合对手所有技能先制-2（MOVE_RIGHT 窗口 tick）");
        b.drive();
        std::cout << "  [T1'] 868 下回合先制-2 通过\n";
    }

    //========== T2 化影神雷 495：对手麻痹 → 10% 秒杀（伤害落地后收头） ==========
    {
        {
            sbs_srand(8);   // 第三掷=2 < 10 → 495 秒杀掷点通过（探针验证）
            ScenarioBattle b(scenario::make_party(gelaorui({kHuaying, kHarmless, kHarmless, kHarmless, kHarmless})),
                             scenario::make_party(foe({kHarmless, kHarmless, kHarmless, kHarmless, kHarmless}, 10, 100000)));
            b.start();
            BattleContext& ctx = b.ctx();
            REQUIRE(apply_anomaly(&ctx, 1, kParalysis, 3, 0) == ApplyAnomalyResult::SUCCESS);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 化影神雷
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(1).hp, 0,
                     "T2 核心：对手处于麻痹 → 10% 概率秒杀（force_hp_to_zero 体力归零）");
            std::cout << "  [T2a] 495 秒杀分支 通过\n";
        }
        // 对照：对手无麻痹 → 不秒杀（10 万体力也扛着 140 威力的伤害）
        {
            sbs_srand(9);
            ScenarioBattle b(scenario::make_party(gelaorui({kHuaying, kHarmless, kHarmless, kHarmless, kHarmless})),
                             scenario::make_party(foe({kHarmless, kHarmless, kHarmless, kHarmless, kHarmless}, 10, 100000)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(ctx.getPet(1).hp > 0, "T2 对照：无麻痹不秒杀（伤害打不死 10 万血）");
            std::cout << "  [T2b] 495 无麻痹对照 通过\n";
        }
    }

    std::cout << "scenario_144 all passed" << std::endl;
    return 0;
}
