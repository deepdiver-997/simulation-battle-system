// 场景 036：圣灵谱尼 1038 七刻印（批 2 补齐的六条）。
//
// 官方（effect_icon id=753）：
//   虚无：自身受到攻击伤害后，免疫下1次受到的技能伤害
//   元素：登场时，消除对手回合类效果，消除成功则令对手下1次技能无效
//   能量：战斗阶段结束时，附加自身已损失体力50%的百分比伤害
//   生命：战斗阶段结束时，恢复自身最大体力的1/4和随机2个PP值不为满的技能1点PP值
//   永恒：回合开始时，若自身体力高于对手则当回合自身必定先手
//   圣洁：免疫受到的异常状态和能力下降状态
//
// 验证点：
//   S1 圣洁：免疫异常（apply_anomaly 被挡）+ 免疫能力下降（stat_drop 返回 IMMUNE）
//   S2 元素：对手有回合类效果 → 登场断掉 + 给对手挂"下1次技能无效"（龙威）
//   S3 元素：对手**无**回合类效果 → 断回合 NO_EFFECTS → **不给**龙威（三态语义）
//   S4 虚无：受到攻击伤害后 → 免疫下 1 次技能伤害（下一次攻击伤害为 0），且只免一次
//   S5 能量：战斗阶段结束时按"自身已损失体力 50%"造成百分比伤害（走百分比档）
//   S6 生命：战斗阶段结束时回 1/4 最大体力 + 随机 2 个未满 PP 技能各 +1
//   S7 永恒：体力高于对手 → 当回合必先（速度更慢也先手）
#include <cassert>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

constexpr int kZhuangJi = 10001;      // 撞击：power 35，PP 35
constexpr int kPuniMarkId = 1038;

ElfPet make_puni(int hp, int speed) {
    return scenario::make_pet(5000, "圣灵谱尼",
                              {kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                              speed, /*atk=*/100, hp,
                              SoulMark(kPuniMarkId, "圣灵谱尼", "七刻印"));
}

ElfPet make_foe(int hp, int speed, int atk = 100) {
    return scenario::make_pet(1, "对手",
                              {kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                              speed, atk, hp);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== S1 圣洁：免异常 + 免弱（登场即生效）==========
    {
        scenario::ScenarioBattle b(scenario::make_party(make_puni(800, 100)),
                                   scenario::make_party(make_foe(800, 100)));
        b.start();
        BattleContext& ctx = b.ctx();
        constexpr int kBurn = 12;
        const auto r = apply_anomaly(&ctx, /*target=*/0, kBurn, 2, /*actor=*/1);
        CHECK(r == ApplyAnomalyResult::TARGET_IMMUNE, "S1) 圣洁 → 异常被免疫");
        CHECK(!ctx.has_active_abnormal_status(0, kBurn), "S1) 异常不应挂上");

        const StatDropResult d = stat_drop(&ctx, /*target=*/0, /*stat=*/0, /*amount=*/2);
        CHECK(d == StatDropResult::IMMUNE, "S1) 圣洁 → 能力下降被免疫（免弱）");
        CHECK_EQ(ctx.ability_levels[0][0], 0, "S1) 攻击等级保持 0");
        std::cout << "  [S1] 圣洁: 异常免疫 + 免弱（降能力 IMMUNE）\n";
    }

    //========== S2 元素：登场断对手回合 → 对手下1次技能无效 ==========
    {
        auto puni = make_puni(800, 100);
        auto foe = make_foe(800, 100);
        scenario::ScenarioBattle b(scenario::make_party(puni), scenario::make_party(foe));
        b.start();
        BattleContext& ctx = b.ctx();
        // 给对手(1)预置一个回合类效果（用其魂印槽直接注册一个 3 回合的 ROUND_START 效果）
        {
            ElfPet& f = ctx.getPet(1);
            Effect dummy(999001, 0, 1, 3, EffectArgs(std::vector<int>{1, 1}),
                         &scenario::dodge_soul_effect);
            ctx.registerEffect(State::BATTLE_ROUND_START, 1,
                               std::make_unique<ContinuousEffect>(
                                   dummy, State::BATTLE_ROUND_START, 1, 3, ctx.roundCount));
            (void)f;
        }
        // 再走一次"登场"：元素已改 early=true（2026-09-26 登场语义批次）——节点在
        // activate（开局 + **每次上场**）时立即执行，走真实切换路径触发（切出的旧谱尼
        // on_stage_storage 清守卫，新上场的谱尼是全新 pet 对象，元素必然重跑）。
        // 对手侧同发换宠（不攻击）——防止切换回合内就把 count=1 的龙威消耗掉
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 2);
        b.drive();
        CHECK(!ctx.has_round_effects(1), "S2) 元素应断掉对手的回合类效果");
        // 对手下一次技能应被龙威拦下（次数型 SEAL_ALL）
        const int foe_skill_index = 0;
        const bool sealed = ctx.rule_center_.notify(&ctx, /*user=*/1, /*is_attribute=*/false,
                                                    /*power=*/35, /*ignore_attack_immunity=*/false)
            == SkillInvalidNotifyResult::INVALID;
        CHECK(sealed, "S2) 消除成功 → 对手下1次技能无效（龙威）");
        (void)foe_skill_index;
        std::cout << "  [S2] 元素: 断回合成功 + 对手下1次技能无效\n";
    }

    //========== S3 元素：对手无回合类效果 → 不给龙威 ==========
    {
        auto puni = make_puni(800, 100);
        auto foe = make_foe(800, 100);
        scenario::ScenarioBattle b(scenario::make_party(puni), scenario::make_party(foe));
        b.start();
        BattleContext& ctx = b.ctx();
        // 对手无回合类效果时切谱尼上场 → 元素 NO_EFFECTS 分支（early 触发，同 S2）
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const bool sealed = ctx.rule_center_.notify(&ctx, /*user=*/1, /*is_attribute=*/false,
                                                    /*power=*/35, /*ignore_attack_immunity=*/false)
            == SkillInvalidNotifyResult::INVALID;
        CHECK(!sealed, "S3) 本来就没有回合类效果 → 不给龙威（三态区分）");
        std::cout << "  [S3] 元素: NO_EFFECTS → 不授予龙威\n";
    }

    //========== S4 虚无：受攻击伤害后 → 免下1次技能伤害 ==========
    // 注意：不能用"回合末净掉血"判断——生命刻印在回合末会回 1/4 体力，会把小额伤害盖掉。
    // 故用单步卡在"对手攻击伤害结算后"这一点上直接观察。
    {
        auto puni = make_puni(800, 100);
        auto foe = make_foe(800, 300, /*atk=*/150);  // 对手更快 → 先手打谱尼
        scenario::ScenarioBattle b(scenario::make_party(puni), scenario::make_party(foe));
        b.start();
        BattleContext& ctx = b.ctx();

        // 回合 1：停在对手（先手方）攻击伤害结算后——虚无就在这一时点授予免疫
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.step_past(State::BATTLE_FIRST_ATTACK_DAMAGE);
        CHECK(ctx.getPet(0).hp < 800, "S4) 第1回合谱尼应已受到攻击伤害");
        CHECK(ctx.is_immune(0, ImmunityType::DAMAGE, State::BATTLE_FIRST_ATTACK_DAMAGE)
                  || ctx.is_immune(0, ImmunityType::DAMAGE, State::BATTLE_SECOND_ATTACK_DAMAGE),
              "S4) 受攻击伤害后应授予'免疫下1次技能伤害'");
        b.drive();  // 跑完回合 1

        // 回合 2：再停在攻击伤害结算后 → 应被完全挡下（体力不变）
        const int hp_before = ctx.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.step_past(State::BATTLE_FIRST_ATTACK_DAMAGE);
        CHECK_EQ(ctx.getPet(0).hp, hp_before,
                 "S4) 虚无：免疫下1次技能伤害 → 第2回合的攻击伤害为 0");
        std::cout << "  [S4] 虚无: 第1回合受击掉血 → 授予次数免伤；第2回合受击体力不变\n";
    }

    //========== S5 能量：战斗阶段结束时按已损失体力 50% 造成百分比伤害 ==========
    // 精确测量：谱尼带**无伤害**的属性技能（高速移动 20017）→ 对手掉的血只可能来自能量。
    // 又因生命刻印在回合末回血，须在"对手打完、能量尚未结算"时记录已损失体力。
    {
        constexpr int kGaoSuYiDong = 20017;
        auto puni = scenario::make_pet(5000, "圣灵谱尼",
                                       {kGaoSuYiDong, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                       /*speed=*/100, /*atk=*/100, /*hp=*/800,
                                       SoulMark(kPuniMarkId, "圣灵谱尼", "七刻印"));
        auto foe = make_foe(5000, 300, /*atk=*/800);  // 先手重击，打掉的血 > 生命回血量
        scenario::ScenarioBattle b(scenario::make_party(puni), scenario::make_party(foe));
        b.start();
        BattleContext& ctx = b.ctx();

        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.step_past(State::BATTLE_FIRST_AFTER_ACTION);  // 对手（先手）已打完
        const int lost = 800 - ctx.getPet(0).hp;
        const int foe_hp_before = ctx.getPet(1).hp;     // 此刻对手还没吃到能量伤害
        CHECK(lost > 0, "S5) 谱尼应已损失体力（前提）");
        b.drive();                                      // 跑完回合（含 ROUND_END 的能量）
        const int dealt = foe_hp_before - ctx.getPet(1).hp;
        CHECK_EQ(dealt, lost / 2, "S5) 能量 = 自身已损失体力的 50%（百分比伤害）");
        std::cout << "  [S5] 能量: 自身已损失 " << lost << " → 对手受到 " << dealt
                  << "（= 50%，走百分比档）\n";
    }

    //========== S6 生命：战斗阶段结束时回 1/4 最大体力 + 随机2个未满PP技能+1 ==========
    {
        auto puni = make_puni(800, 100);
        auto foe = make_foe(3000, 100, /*atk=*/200);
        scenario::ScenarioBattle b(scenario::make_party(puni), scenario::make_party(foe));
        b.start();
        BattleContext& ctx = b.ctx();
        // 人为扣血 + 扣 PP，观察回合结束的恢复
        ctx.getPet(0).hp = 100;
        ctx.getPet(0).skills[1].pp -= 3;
        ctx.getPet(0).skills[2].pp -= 5;
        int pp_total_before = 0;
        for (int i = 0; i < 5; ++i) {
            pp_total_before += ctx.getPet(0).skills[i].pp;
        }

        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        // 生命回 1/4=200（谱尼先被对手打掉一部分，故只校验"回血确实发生"）
        CHECK(ctx.getPet(0).hp > 100, "S6) 生命：战斗阶段结束时恢复最大体力 1/4");
        // PP 账：本回合用了 0 号技能（-1），生命再给随机 2 个未满技能各 +1 → 合计 +1。
        // 不断言"具体哪两个技能"，因为候选含刚用过的 0 号技能（也是未满）。
        int pp_total_after = 0;
        for (int i = 0; i < 5; ++i) {
            pp_total_after += ctx.getPet(0).skills[i].pp;
        }
        CHECK_EQ(pp_total_after - pp_total_before, 1,
                 "S6) 生命：随机 2 个未满 PP 技能各 +1（本回合共消耗 1）");
        std::cout << "  [S6] 生命: 回血 800/4；PP 合计 " << pp_total_before << " → "
                  << pp_total_after << "（用1、回2）\n";
    }

    //========== S7 永恒：体力高于对手 → 当回合必先（速度更慢也先手）==========
    {
        auto puni = make_puni(900, 50);   // 速度远低于对手
        auto foe = make_foe(700, 400);    // 对手更快，但体力低于谱尼
        scenario::ScenarioBattle b(scenario::make_party(puni), scenario::make_party(foe));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        // 单步模式：让 op1 只推进一个状态，才能停在先手权结算后（否则回合已跑完，
        // step_past 会停在下一回合的 CHOOSE 等输入状态上空转）。
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.step_past(State::BATTLE_FIRST_MOVE_RIGHT);
        CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_1,
              "S7) 永恒：体力高于对手 → 必先（速度 50 也先手）");
        std::cout << "  [S7] 永恒: 速度 50 < 400，但体力高 → 先手权归谱尼\n";
    }

    std::cout << "\n场景 036 通过：七刻印六条全部生效（圣洁免异常免弱 / 元素断回合→龙威 / "
                 "虚无受击免伤 / 能量百分比 / 生命回复 / 永恒必先）。\n";
    return 0;
}
