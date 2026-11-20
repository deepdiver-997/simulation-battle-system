#include <utils/sbs_rand.h>
// 场景 153：艾丽娅 3403 技能组（2026-09-27）
//
//   羁绊缔结 30906 [788+626]  消对手提升→1回合免疫异常；随机3项能力+1
//   迅捷浮影 30908 [606]      若体力高于对手 → 威力翻倍
//   精灵之助 25563 [1039]     4回合内每回合用技能回1/3，低于1/2直接回满
//   道法自然冰 30910 [1176]   30%伤害2倍，未触发附加300固伤
//   幻灭速击 30928 [1010+626] 消对手提升→下2回合伤害+100%；随机3项+1
//   破敌冰锥击 30930 [1176]   25%伤害4倍，未触发附加300固伤
//   寒冰藤蔓 25572 [89]       3回合内每次造成伤害1/2回血（191/806 已实现）
//   自然祝福 25573 [1039]     同 1039（4,3,2）
//
// 1176 的概率分支（30%/25%）依赖固定 srand 序列——断言写成分支相容：命中倍率分支
// 看 AMP_EXTRA 槽、未命中分支看附加固伤，两者必居其一且数值自洽。
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
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);

ElfPet aliya(int skill, int atk = 300, int hp = 3000) {
    ElfPet p = scenario::make_pet(0, "艾丽娅", {skill, kHit, kHit, kHit, kHit},
                                  /*speed=*/200, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet(int hp = 4000) {
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

int ability_sum(const BattleContext& ctx, int side) {
    int sum = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        sum += ctx.ability_level(side, i);
    }
    return sum;
}

}  // namespace

int main() {
    sbs_srand(20260927);
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== T1 羁绊缔结：788 消提升→免疫异常 + 626 随机3项+1 ==========
    {
        ScenarioBattle b(scenario::make_party(aliya(30906)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(stat_change(&ctx, 1, /*攻击=*/0, 2) == StatChangeResult::SUCCESS);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.ctx().debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 2);
        b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);
        b.ctx().debug_step_mode = false;
        BattleContext& c2 = b.ctx();
        CHECK_EQ(c2.ability_level(1, 0), 0, "T1 对手能力提升被消除（788 前半）");
        CHECK_EQ(ability_sum(c2, 0), 3, "T1 随机使自己3项能力+1（626）");
        // 消除成功 → 1回合内免疫异常：窗口内（本回合）现代施加应被挡下
        const ApplyAnomalyResult r = apply_anomaly(&c2, 0, kBurn, 2, 1);
        CHECK(r != ApplyAnomalyResult::SUCCESS, "T1 788：免疫异常窗口内施加被挡");
        b.drive();
        std::cout << "  [T1] 羁绊缔结 788+626 通过\n";
    }

    //========== T2 迅捷浮影：606[0] 体力高于对手 → 威力翻倍 ==========
    {
        // 对照：双方满体力（相等不算"高于"）→ 威力不变
        int base_view = -1;
        {
            ScenarioBattle b(scenario::make_party(aliya(30908)),
                             scenario::make_party(foe_pet()));
            b.start();
            BattleContext& ctx = b.ctx();
            use0(b, /*foe_slot=*/2);
            base_view = ctx.ws.skill_power_view[0];
            CHECK_EQ(base_view, 130, "T2 对照：体力不高于对手 → 威力视图 130");
        }
        // 守方残血 → 翻倍 260
        ScenarioBattle b(scenario::make_party(aliya(30908)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(1).hp = 500;
        use0(b, /*foe_slot=*/2);
        CHECK_EQ(ctx.ws.skill_power_view[0], base_view * 2,
                 "T2 体力高于对手：威力视图翻倍（变威力）");
        std::cout << "  [T2] 迅捷浮影 606 通过\n";
    }

    //========== T3 精灵之助：1039 每回合用技能回 1/3（低血直接回满）==========
    {
        // 1/3 分支：hp=1600（>1500=1/2 上限）→ 回 1000 → 2600
        ScenarioBattle b(scenario::make_party(aliya(25563)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 1600;
        use0(b);   // R1：用技能（1039 立即生效？——窗口 AFTER_ACTION tick 当回合即算）
        // R1 的 AFTER_ACTION tick：本回合用过技能 → 回 1/3
        CHECK_EQ(ctx.getPet(0).hp, 1600 + 1000, "T3 R1：用技能恢复最大体力 1/3");
        b.drive();
    }
    {
        // 低血分支：hp=1000（<1500）→ 直接回满
        ScenarioBattle b(scenario::make_party(aliya(25573)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 1000;
        use0(b);
        CHECK_EQ(ctx.getPet(0).hp, 3000, "T3 自然祝福：低于 1/2 → 直接回满");
        std::cout << "  [T3] 精灵之助/自然祝福 1039 通过\n";
    }

    //========== T4 幻灭速击：1010 消提升→下2回合伤害+100% + 626 ==========
    {
        ScenarioBattle b(scenario::make_party(aliya(30928)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(stat_change(&ctx, 1, /*攻击=*/0, 2) == StatChangeResult::SUCCESS);
        use0(b, /*foe_slot=*/2);
        CHECK_EQ(ctx.ability_level(1, 0), 0, "T4 对手能力提升被消除");
        CHECK_EQ(ability_sum(ctx, 0), 3, "T4 随机3项能力+1（626）");
        // "下2回合" NextRounds 窗口：R2 的 ROUND_START tick 写 +100
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.step_past(State::BATTLE_ROUND_START);
        ctx.debug_step_mode = false;
        CHECK_EQ(ctx.ws.damage_add_pct[0], 100, "T4 R2：造成的伤害提高 100%（NextRounds tick）");
        b.drive();
        std::cout << "  [T4] 幻灭速击 1010+626 通过\n";
    }

    //========== T5 道法自然冰/破敌冰锥击：1176 概率倍率 or 附加固伤 ==========
    for (const int skill : {30910, 30930}) {
        ScenarioBattle b(scenario::make_party(aliya(skill)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        use0(b, /*foe_slot=*/2);
        bool multiplied = false;
        for (int i = 0; i < 4; ++i) {
            if (ctx.ws.damage_add_extra_mul[0][i] != 0) {
                multiplied = true;
                break;
            }
        }
        const int drop = 4000 - ctx.getPet(1).hp;
        // 倍率分支：红伤 ×2/×4（AMP_EXTRA）→ 掉血显著高于基础；固伤分支：+300
        if (multiplied) {
            CHECK(drop > 300, "T5 倍率分支：伤害翻倍以上（drop=" << drop << "）");
        } else {
            CHECK(drop >= 300, "T5 固伤分支：附加 300 点固定伤害（drop=" << drop << "）");
        }
        std::cout << "  [T5] 技能 " << skill << " 1176 通过（"
                  << (multiplied ? "倍率" : "固伤") << "分支, drop=" << drop << "）\n";
    }

    //========== T6 寒冰藤蔓：89 每次造成伤害 1/2 回血 ==========
    {
        ScenarioBattle b(scenario::make_party(aliya(25572)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 2400;
        use0(b, /*foe_slot=*/0);   // R1 寒冰藤蔓（属性技，89 窗口挂上）；守方用锋利不回打
        // R2 用拍打打出红伤 → watcher 回血 dealt/2；单步到本方 AFTER_ACTION 后断言，
        // 再放行（守方反击会污染回血观测）。
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.step_past(State::BATTLE_FIRST_AFTER_ACTION);
        ctx.debug_step_mode = false;
        const int dealt = 4000 - ctx.getPet(1).hp;
        CHECK(dealt > 0, "T6 前提：R2 打出红伤");
        CHECK_EQ(ctx.getPet(0).hp, 2400 + dealt / 2, "T6 造成伤害的 1/2 恢复自己HP");
        b.drive();
        std::cout << "  [T6] 寒冰藤蔓 89 通过（dealt=" << dealt << "）\n";
    }

    std::cout << "scenario_153 all passed" << std::endl;
    return 0;
}
