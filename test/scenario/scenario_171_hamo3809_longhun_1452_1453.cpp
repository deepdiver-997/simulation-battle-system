#include <utils/sbs_rand.h>
// 场景 171：龙神哈莫 3809 —— 召唤龙神双 BUFF（战之龙魂 1452 / 御之龙魂 1453）
//（resources/moves_lib/hamo_3809_longhun.cpp + RuleCenter CARRIER 载体票，2026-09-28）
//
// 官方形态：龙·守地灵 26595 / 龙·御天神 26596 的「召唤龙神，使自身下{0}回合获得
// X之龙魂效果」。架构 = RuleCenter 载体票真源 + 战斗级读者查票（见 hamo_3809_longhun.cpp
// 头注与 RuleCategory::CARRIER 注）。
//
// 断言组：
//   T1 战之龙魂：起算门（arm 当回合不在）→ 窗口内先制+3、必中、攻击伤害 1.5~2.0 倍带
//      → 过期（票不在；伤害回无 buff 浮动区间——AMP 读者无票不掷点）。
//   T2 御之龙魂：起算前固伤全额 → 窗口内固伤 -25% + 每回合随机 1 技能 PP 归零
//      → ★窗口内断回合 → 票清、固伤回全额、不再归零（读者随票哑火）。
//   T3 断回合整组失效（战之龙魂，纯净化对比——御天神自带 1065/893 已实现效果会
//      污染伤害基线，故本组只用守地灵）：窗口内增伤带 → break → 先制归零、
//      伤害回无 buff 浮动区间。
//   T4 换宠清（ON_STAGE 口径）：arm 后持有者下场 → 票清、buff 消失（v1 绑定在场）。
//
// 确定性：全技能必中、暴击率 0；伤害公式自带官方浮动 (217+sbs_rand()%39)/255 ≈ 0.851~1.0，
// 增伤/无增伤两状态用不重叠的判别带断言（增伤最小 +50%，远超浮动区间）。
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

constexpr int kZhuangJi = 10001;   // 撞击：威力 35 物理，无副作用
constexpr int kGaoSu = 20017;      // 高速移动：属性技，自身强化（对面对我无威胁）
constexpr int kShouDiLing = 26595; // 龙·守地灵：[191, 884, 1452]
constexpr int kYuTianShen = 26596; // 龙·御天神：[893, 1065, 1453]

ElfPet hamo(std::array<int, 5> skills) {
    ElfPet p = scenario::make_pet(0, "龙神哈莫", skills, /*speed=*/200, /*atk=*/300,
                                  /*hp=*/5000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe(std::array<int, 5> skills) {
    ElfPet p = scenario::make_pet(1, "对手", skills, /*speed=*/100, /*atk=*/50,
                                  /*hp=*/1000000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// 打一回合：p0 用 skill0 槽、p1 用 skill1 槽，返回 p1 本回合掉的血（攻击伤害观测）。
int round_p0_attack(ScenarioBattle& b, int skill0, int skill1) {
    BattleContext& ctx = b.ctx();
    const int before = ctx.getPet(1).hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, skill0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, skill1);
    b.drive();
    return before - ctx.getPet(1).hp;
}

int count_pp_zero(BattleContext& ctx, int side) {
    int n = 0;
    for (const Skills& s : ctx.getPet(side).skills) {
        if (s.pp == 0) {
            ++n;
        }
    }
    return n;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== T1 战之龙魂（1452）：起算门 / 先制+必中 / 增伤带 / 过期 ==========
    // 注：roundCount 从 0 起算（第 1 回合执行时 rc=0，回合完成 advanceRound）。
    // 断言一律用 arm 前捕获的 arm_rc 相对表达，不做人类回合数换算。
    {
        ScenarioBattle b(scenario::make_party(hamo({kZhuangJi, kShouDiLing, kYuTianShen, kZhuangJi, kZhuangJi})),
                         scenario::make_party(foe({kZhuangJi, kGaoSu, kZhuangJi, kZhuangJi, kZhuangJi})));
        b.start();
        BattleContext& ctx = b.ctx();
        const int d0 = round_p0_attack(b, 0, 1);   // 基线（无 buff）
        CHECK(d0 > 0, "T1 前置：基线伤害 > 0");

        const int arm_rc = ctx.roundCount;         // arm 回合的 rc（票 register_round = arm_rc+1）
        round_p0_attack(b, 1, 1);                  // 守地灵 arm
        CHECK(!ctx.rule_center_.has_carrier(0, 1452, arm_rc),
              "T1 核心：『下{0}回合』起算门——arm 当回合票不在");
        CHECK(ctx.rule_center_.has_carrier(0, 1452, arm_rc + 1), "T1 下一回合票生效");
        CHECK(ctx.rule_center_.has_carrier(0, 1452, arm_rc + 2), "T1 第二回合票仍生效");
        CHECK(!ctx.rule_center_.has_carrier(0, 1452, arm_rc + 3), "T1 第三回合票过期（rounds=2）");

        // 窗口内第一回合：MOVE_RIGHT 后先制+3、必中置位；伤害落 1.5~2.0 倍带
        const int hp_before = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.step_past(State::BATTLE_FIRST_MOVE_RIGHT);
        ctx.debug_step_mode = false;
        CHECK_EQ(ctx.ws.preemptive_level[0], 3, "T1 核心：战之龙魂 所有技能先制+3（撞击基值 0）");
        CHECK(ctx.ws.must_hit_grant[0], "T1 核心：所有技能必定命中");
        b.drive();
        const int d_win1 = hp_before - ctx.getPet(1).hp;
        // 伤害公式自带官方浮动 (217+sbs_rand()%39)/255 ≈ 0.851~1.0，基线/窗口两次掷点独立。
        // 增伤幅带 [1.5, 2.0] × 浮动比的最坏合成 ≈ [1.28, 2.36]，取 [1.25, 2.40]。
        CHECK(d_win1 * 100 >= d0 * 125 && d_win1 * 100 <= d0 * 240,
              "T1 核心：攻击伤害随机提升 50%-100%（判别带：远离无 buff 浮动区间）");

        const int d_win2 = round_p0_attack(b, 0, 1);  // 窗口内第二回合
        CHECK(d_win2 * 100 >= d0 * 125 && d_win2 * 100 <= d0 * 240,
              "T1 窗口内第二回合增伤带仍成立");

        const int d_after = round_p0_attack(b, 0, 1); // 窗口已过
        // 无票 → 只剩官方浮动（两掷点比 ≈ [0.85, 1.18]，与增伤带不重叠）→ 取 ±20% 带
        CHECK(d_after * 100 >= d0 * 80 && d_after * 100 <= d0 * 120,
              "T1 核心：过期后伤害回无 buff 浮动区间（AMP 读者哑火——+50% 起的增伤带已远离）");
        std::cout << "  [T1] 战之龙魂 起算门/先制+必中/增伤带/过期 通过\n";
    }

    //========== T2 御之龙魂（1453）：起算前全额 / 固伤-25% / 每回合 PP 归零 / 过期 ====
    {
        ScenarioBattle b(scenario::make_party(hamo({kZhuangJi, kShouDiLing, kYuTianShen, kZhuangJi, kZhuangJi})),
                         scenario::make_party(foe({kZhuangJi, kGaoSu, kZhuangJi, kZhuangJi, kZhuangJi})));
        b.start();
        BattleContext& ctx = b.ctx();
        const int arm_rc = ctx.roundCount;
        // arm 回合内 step 到 SKILL_EFFECT 之后（票已授、rc 仍 = arm_rc）→ 起算门当场可验
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 2);
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);
        ctx.debug_step_mode = false;
        CHECK(!ctx.rule_center_.has_carrier(0, 1453, arm_rc),
              "T2 核心：『下{0}回合』起算门——arm 当回合票不在");
        {
            const int before = ctx.getPet(0).hp;
            REQUIRE(fixed_damage(&ctx, 0, 100) == FixedDamageResult::SUCCESS);
            CHECK_EQ(ctx.getPet(0).hp, before - 100, "T2 核心：起算前固伤全额（不受 -25%）");
        }
        b.drive();                                 // 打完 arm 回合（票自下一回合起生效）

        round_p0_attack(b, 0, 1);                  // 窗口第一回合（撞击 + AFTER_ACTION PP 归零）
        CHECK(ctx.rule_center_.has_carrier(0, 1453, arm_rc + 1), "T2 窗口第一回合票生效");
        {
            const int before = ctx.getPet(0).hp;
            REQUIRE(fixed_damage(&ctx, 0, 100) == FixedDamageResult::SUCCESS);
            CHECK_EQ(ctx.getPet(0).hp, before - 75, "T2 核心：受到的固定伤害减少 25%");
        }
        CHECK_EQ(count_pp_zero(ctx, 1), 1,
                 "T2 核心：攻击令对手随机 1 个技能 PP 值归零（第一回合恰 1 个）");

        // ★ 窗口内断回合：票在生效期被 break → 粉伤读者当场哑火（固伤回全额）
        CHECK(ctx.rule_center_.has_carrier(0, 1453, arm_rc + 2),
              "T2 前置：第二回合票在生效期");
        CHECK(break_round_effects(&ctx, 0) == BreakResult::SUCCESS,
              "T2 前置：断回合成功（载体票计入可断物）");
        CHECK(!ctx.rule_center_.has_carrier(0, 1453, arm_rc + 2),
              "T2 核心：断回合 → 载体票即清");
        {
            const int before = ctx.getPet(0).hp;
            REQUIRE(fixed_damage(&ctx, 0, 100) == FixedDamageResult::SUCCESS);
            CHECK_EQ(ctx.getPet(0).hp, before - 100, "T2 核心：粉伤读者随票哑火（固伤回全额）");
        }
        b.drive();                                 // 打完被打断的回合

        round_p0_attack(b, 0, 1);                  // 下一回合：窗口已被断
        CHECK_EQ(count_pp_zero(ctx, 1), 1, "T2 核心：断回合后不再归零（窗口节点已作废）");
        {
            const int before = ctx.getPet(0).hp;
            REQUIRE(fixed_damage(&ctx, 0, 100) == FixedDamageResult::SUCCESS);
            CHECK_EQ(ctx.getPet(0).hp, before - 100, "T2 断回合后固伤维持全额");
        }
        std::cout << "  [T2] 御之龙魂 起算/固伤-25%/PP归零/窗口内断回合 通过\n";
    }

    //========== T3 断回合整组失效（战之龙魂）：攻击增伤读者随票哑火 ==========
    // 注：御天神 26596 的 HIT 分支含其他已实现效果（1065 回血+百分比伤、893 全属性+
    // 满体翻倍——能力等级不属回合类、断回合不清），会污染伤害基线；本组只用守地灵
    // （191 弹控/884 封技均不伤打攻击伤害），保证增伤对比纯净。
    {
        ScenarioBattle b(scenario::make_party(hamo({kZhuangJi, kShouDiLing, kYuTianShen, kZhuangJi, kZhuangJi})),
                         scenario::make_party(foe({kZhuangJi, kGaoSu, kZhuangJi, kZhuangJi, kZhuangJi})));
        b.start();
        BattleContext& ctx = b.ctx();
        const int d0 = round_p0_attack(b, 0, 1);   // 基线（无 buff）

        const int arm_rc = ctx.roundCount;
        round_p0_attack(b, 1, 1);                  // 守地灵 arm（窗口 = arm_rc+1、arm_rc+2）

        // 窗口第一回合：先制+3、增伤带
        const int hp_before = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.step_past(State::BATTLE_FIRST_MOVE_RIGHT);
        ctx.debug_step_mode = false;
        CHECK_EQ(ctx.ws.preemptive_level[0], 3, "T3 核心：战之龙魂 所有技能先制+3");
        b.drive();
        const int d_win = hp_before - ctx.getPet(1).hp;
        CHECK(d_win * 100 >= d0 * 125 && d_win * 100 <= d0 * 240,
              "T3 核心：窗口内攻击伤害随机提升 50%-100%（判别带）");

        // 窗口第二回合开始前断回合：票在生效期被 break
        CHECK(ctx.rule_center_.has_carrier(0, 1452, arm_rc + 2), "T3 前置：第二回合票在生效期");
        CHECK(break_round_effects(&ctx, 0) == BreakResult::SUCCESS, "T3 前置：断回合成功");

        const int hp_before2 = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.step_past(State::BATTLE_FIRST_MOVE_RIGHT);
        ctx.debug_step_mode = false;
        CHECK_EQ(ctx.ws.preemptive_level[0], 0, "T3 核心：先制窗口节点随断回合作废");
        b.drive();
        const int d_broken = hp_before2 - ctx.getPet(1).hp;
        // 断回合后无票 → 只剩官方浮动（±20% 带；+50% 起的增伤带已远离）
        CHECK(d_broken * 100 >= d0 * 80 && d_broken * 100 <= d0 * 120,
              "T3 核心：断回合后攻击伤害回无 buff 浮动区间（AMP 读者随票哑火）");
        std::cout << "  [T3] 断回合整组失效 通过\n";
    }

    //========== T4 换宠清（ON_STAGE 口径）：持有者下场 → 票清 ==========
    {
        ScenarioBattle b(scenario::make_party(hamo({kZhuangJi, kShouDiLing, kYuTianShen, kZhuangJi, kZhuangJi})),
                         scenario::make_party(foe({kZhuangJi, kGaoSu, kZhuangJi, kZhuangJi, kZhuangJi})));
        b.start();
        BattleContext& ctx = b.ctx();
        round_p0_attack(b, 1, 1);                  // arm 1452（下一回合起生效）
        const int switch_rc = ctx.roundCount;      // 换宠回合 rc（票本应在此回合生效）
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        CHECK_EQ(ctx.on_stage[0], 1, "T4 前置：已换宠");
        CHECK(!ctx.rule_center_.has_carrier(0, 1452, switch_rc),
              "T4 核心：票 scope=ON_STAGE，持有者下场即清（v1 绑定在场口径）");
        std::cout << "  [T4] 换宠清 通过\n";
    }

    std::cout << "\n场景 171 通过：龙神哈莫 召唤龙神双 BUFF（载体票 + 战斗级读者）" << std::endl;
    return 0;
}
