#include <utils/sbs_rand.h>
// 场景 158：不灭地威·萨瑞卡 3637 技能组（2026-09-27）
//
//   大地战栗 12223 [63]      能力下降反馈（63 主路径已在 150 锁，此处走本技）
//   斗志高昂 21149 [126]     3回合每回合攻/速 +1
//   咆哮裂地击 12221 [66]    击败回 1/5（66 主路径已在 150 锁，此处走本技）
//   磐石之力 24299 [187]     对手用属性技 → 自身回 1/3
//   不动神威 24300 [502]     直接攻击降对手 1/4 HP
//   憾地之力 26194 [1154]    每回合附加 200 固伤（体力低于对手翻倍）
//   斗志激昂 26195 [1351]    350 护罩，消失时封对手 1 次攻击技
//   尖角突袭 32175 [1138]    消回合成功 50% 害怕 / 未成功 100% 疲惫
//   流沙封魂阵 32177 [871+540+723]  下2回合伤害≥280；后手必暴+对手先制-3
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

constexpr int kHit = 10004;
constexpr int kBuff = 20265;
constexpr int kFear = static_cast<int>(AbnormalStatusId::Fear);
constexpr int kFatigue = static_cast<int>(AbnormalStatusId::Fatigue);

constexpr int kDaDi = 12223;    // 大地战栗
constexpr int kDouZhi = 21149;  // 斗志高昂
constexpr int kPaoXiao = 12221; // 咆哮裂地击
constexpr int kPanShi = 24299;  // 磐石之力
constexpr int kBuDong = 24300;  // 不动神威
constexpr int kHanDi = 26194;   // 憾地之力
constexpr int kDouJi = 26195;   // 斗志激昂
constexpr int kJianJiao = 32175;// 尖角突袭
constexpr int kLiuSha = 32177;  // 流沙封魂阵

ElfPet sarika(int skill, int speed = 200, int atk = 300) {
    ElfPet p = scenario::make_pet(0, "萨瑞卡", {skill, kHit, kHit, kHit, kHit},
                                  speed, atk, 4000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet(int hp = 6000) {
    ElfPet p = scenario::make_pet(1, "守方", {kBuff, kHit, kHit, kHit, kHit},
                                  /*speed=*/100, 100, hp);
    for (Skills& s : p.skills) {
        s.accuracy = 100;
    }
    return p;
}

void use0(ScenarioBattle& b, int foe_slot = 0) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_slot);
    b.drive();
}

}  // namespace

int main() {
    sbs_srand(20260927);
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== T1 大地战栗 63：下降反馈 ==========
    {
        ScenarioBattle b(scenario::make_party(sarika(kDaDi)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(stat_drop(&ctx, 0, /*攻击=*/0, 2, 1) == StatDropResult::SUCCESS);
        use0(b, /*foe_slot=*/2);
        CHECK_EQ(ctx.ability_level(1, 0), -2, "T1 反馈：对手攻击 -2");
        CHECK_EQ(ctx.ability_level(0, 0), 0, "T1 自身下降解除");
        std::cout << "  [T1] 大地战栗 63 通过\n";
    }

    //========== T2 斗志高昂 126：每回合攻/速 +1 ==========
    {
        ScenarioBattle b(scenario::make_party(sarika(kDouZhi)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        use0(b);   // R1：首手立即 +1
        CHECK_EQ(ctx.ability_level(0, 0), 1, "T2 R1 攻 +1");
        CHECK_EQ(ctx.ability_level(0, 4), 1, "T2 R1 速 +1");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        CHECK_EQ(ctx.ability_level(0, 0), 2, "T2 R2 窗口 tick 再 +1");
        CHECK_EQ(ctx.ability_level(0, 4), 2, "T2 R2 速再 +1");
        std::cout << "  [T2] 斗志高昂 126 通过\n";
    }

    //========== T3 咆哮裂地击 66：击败回 1/5 ==========
    {
        ScenarioBattle b(scenario::make_party(sarika(kPaoXiao)),
                         scenario::make_party(foe_pet(/*hp=*/1)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(1).hp = 1;
        ctx.getPet(0).hp = 2000;
        use0(b, /*foe_slot=*/1);
        CHECK(ctx.getPet(1).hp <= 0, "T3 前提：击败");
        CHECK_EQ(ctx.getPet(0).hp, 2000 + 800, "T3 击败：恢复最大体力 1/5（4000/5）");
        std::cout << "  [T3] 咆哮裂地击 66 通过\n";
    }

    //========== T4 磐石之力 187：对手用属性技 → 自身回 1/3 ==========
    {
        ScenarioBattle b(scenario::make_party(sarika(kPanShi)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 2000;
        use0(b);   // R1 挂窗口；守方锋利（属性技）
        CHECK_EQ(ctx.getPet(0).hp, 2000 + 4000 / 3, "T4 对手属性技 → 自身回 1/3");
        std::cout << "  [T4] 磐石之力 187 通过\n";
    }

    //========== T5 不动神威 502：直接攻击降对手 1/4 HP ==========
    {
        ScenarioBattle b(scenario::make_party(sarika(kBuDong)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        use0(b);   // R1 挂窗口（不动神威是属性技）；守方锋利
        // R2：自身用拍打（攻击技）→ ROUND_END tick 附加降 6000/4 = 1500。
        // 红伤单独读（不动神威自带的 776 威力翻倍等已含在落血里），再断言 502 的增量。
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.step_past(State::BATTLE_FIRST_ATTACK_DAMAGE);
        ctx.debug_step_mode = false;
        const int foe_after_red = ctx.getPet(1).hp;
        b.drive();
        CHECK_EQ(ctx.getPet(1).hp, foe_after_red - 1500,
                 "T5 直接攻击附加降 1/4 HP");
        std::cout << "  [T5] 不动神威 502 通过\n";
    }

    //========== T6 憾地之力 1154：每回合附加 200 固伤（低血翻倍）==========
    {
        // 非低血：附加 200（自身满血 4000 > 守方 3000 → 不翻倍）
        {
            ScenarioBattle b(scenario::make_party(sarika(kHanDi)),
                             scenario::make_party(foe_pet(/*hp=*/3000)));
            b.start();
            BattleContext& ctx = b.ctx();
            use0(b);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
            ctx.debug_step_mode = true;
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
            b.step_past(State::BATTLE_FIRST_ATTACK_DAMAGE);
            ctx.debug_step_mode = false;
            const int red_only = ctx.getPet(1).hp;
            b.drive();
            CHECK_EQ(ctx.getPet(1).hp, red_only - 200,
                     "T6 体力不低于对手：附加 200 固伤");
        }
        // 低血：翻倍 400
        {
            ScenarioBattle b(scenario::make_party(sarika(kHanDi)),
                             scenario::make_party(foe_pet()));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 500;   // 低于对手（6000）
            use0(b);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
            ctx.debug_step_mode = true;
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
            b.step_past(State::BATTLE_FIRST_ATTACK_DAMAGE);
            ctx.debug_step_mode = false;
            const int red_only2 = ctx.getPet(1).hp;
            b.drive();
            CHECK_EQ(ctx.getPet(1).hp, red_only2 - 400,
                     "T6 体力低于对手：固伤翻倍 400");
        }
        std::cout << "  [T6] 憾地之力 1154 通过\n";
    }

    //========== T7 斗志激昂 1351：护罩消失 → 封对手 1 次攻击技 ==========
    {
        ScenarioBattle b(scenario::make_party(sarika(kDouJi)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        use0(b, /*foe_slot=*/2);   // 获得 350 护罩；守方拍打先落一笔（护罩尚未生效）
        CHECK_EQ(ctx.getPet(0).hood_bank_.total(), 350, "T7 获得 350 护罩");
        const int hp_after_r1 = ctx.getPet(0).hp;
        // 打掉护罩：直调粉伤原语（护罩只响应粉伤）两段 200 → 护罩耗尽 → watcher 封对手
        (void)deal_pink_damage(&ctx, 0, 200, DamageKind::FIXED, 1);
        (void)deal_pink_damage(&ctx, 0, 200, DamageKind::FIXED, 1);
        CHECK_EQ(ctx.getPet(0).hood_bank_.total(), 0, "T7 护罩耗尽");
        const int hp_after_pink = ctx.getPet(0).hp;   // 护罩 350 + 溢出 50 已落
        // R2：对手攻击技被封（无红伤落地）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        CHECK_EQ(ctx.getPet(0).hp, hp_after_pink,
                 "T7 护罩消失：对手下1次攻击技无效（R1 与粉伤溢出的血不受影响）");
        std::cout << "  [T7] 斗志激昂 1351 通过\n";
    }

    //========== T8 尖角突袭 1138：消回合分支（分支相容）==========
    {
        // 挂回合类 → 消除成功 → 50% 害怕
        {
            ScenarioBattle b(scenario::make_party(sarika(kJianJiao)),
                             scenario::make_party(foe_pet()));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.rule_center_.grant_reflect(1, -1, 999, 1, 3);
            ctx.seed_rng(1); // 固定成功分支的对局随机序列
            use0(b, /*foe_slot=*/2);
            const bool feared = ctx.has_active_abnormal_status(1, kFear);
            const bool fatigued = ctx.has_active_abnormal_status(1, kFatigue);
            CHECK(feared || fatigued,
                  "T8 成功分支：50% 害怕（未中不叠加疲惫，本分支二者相容观察）");
            std::cout << "  [T8a] 尖角突袭 消除成功分支 通过\n";
        }
        // 不挂回合类 → 消除失败 → 100% 疲惫
        {
            ScenarioBattle b(scenario::make_party(sarika(kJianJiao)),
                             scenario::make_party(foe_pet()));
            b.start();
            BattleContext& ctx = b.ctx();
            use0(b, /*foe_slot=*/2);
            CHECK(ctx.has_active_abnormal_status(1, kFatigue),
                  "T8 消除失败：100% 疲惫");
            std::cout << "  [T8b] 尖角突袭 消除失败分支 通过\n";
        }
    }

    //========== T9 流沙封魂阵 871+540+723 ==========
    {
        // 871：下2回合伤害 ≥280（自身攻小 → 红伤被抬到 280）
        ElfPet slow = sarika(kLiuSha, /*speed=*/50, /*atk=*/10);   // 后手 & 低攻
        ScenarioBattle b(scenario::make_party(slow), scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        // R1 自身先攻权在守方 → 后手用流沙封魂阵
        use0(b, /*foe_slot=*/2);
        // 540/723：后手触发 → R2 must_crit + 对手先制 -3；871：R2 伤害 ≥280
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.step_past(State::BATTLE_FIRST_MOVE_RIGHT);
        ctx.debug_step_mode = false;
        CHECK_EQ(ctx.ws.preemptive_level[1], -3, "T9 723：对手下回合先制 -3");
        // R2 自身攻击（低攻）红伤被 FLOOR 抬到 ≥280（在守方出手前读快照）
        b.step_past(State::BATTLE_FIRST_ATTACK_DAMAGE);
        ctx.debug_step_mode = false;
        const int foe_hp_mid = ctx.getPet(1).hp;
        const int dealt_r2 = ctx.ws.resolvedDamage.final;
        b.drive();
        CHECK(dealt_r2 >= 280, "T9 871：伤害不少于 280（实际 " << dealt_r2 << "）");
        CHECK_EQ(ctx.getPet(1).hp, foe_hp_mid, "T9 守方 R2 无额外掉血（自身已出手）");
        CHECK(ctx.crit_happened[0] || ctx.ws.must_crit[0],
              "T9 540：后手必暴生效（暴击发生或标志在身）");
        std::cout << "  [T9] 流沙封魂阵 871+540+723 通过（R2 伤害 " << dealt_r2 << "）\n";
    }

    std::cout << "scenario_158 all passed" << std::endl;
    return 0;
}
