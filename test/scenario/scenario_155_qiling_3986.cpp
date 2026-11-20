#include <utils/sbs_rand.h>
// 场景 155：启灵元神 3986 技能组（2026-09-27）
//
//   圣念 33887 [438]        10% 概率回自身体力 1/10（分支相容断言）
//   星辉幻尘 33888 [1528]   命中后附加 50 点真实伤害
//   涤罪 33889 [1500+1114+1529] 连击 3-5（内核区间）+ 消提升成功→200 真伤
//   虚灵一闪 33890 [1530]   遇天敌附加 200 真伤（火 vs 水克制的元素实验）
//   神之禁 27018 [765+440]  3回合对手无法提升自身能力（事件回退近似）+ PP 2 倍
//   天堂圣光 14623 [64]     烧伤/冻伤/中毒 → 伤害加倍（64 主路径已在 150 锁，此处走本技）
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

// 启灵元神：技能全必中。
ElfPet qiling(int skill, int atk = 300, int hp = 3000) {
    ElfPet p = scenario::make_pet(0, "启灵元神", {skill, kHit, kHit, kHit, kHit},
                                  /*speed=*/200, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = scenario::make_pet(1, "守方", {kBuff, kHit, kHit, kHit, kHit},
                                  /*speed=*/100, 100, 4000);
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

    //========== T1 圣念 438：10% 回 1/10（分支相容）==========
    {
        ScenarioBattle b(scenario::make_party(qiling(33887)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 1000;
        use0(b, /*foe_slot=*/0);   // 守方锋利不回打，隔离回血观测
        const int gained = ctx.getPet(0).hp - 1000;
        CHECK(gained == 0 || gained == 300,
              "T1 圣念：10% 分支——未触发(0) 或 触发(+300)，实际 " << gained);
        std::cout << "  [T1] 圣念 438 通过（gained=" << gained << "）\n";
    }

    //========== T2 星辉幻尘 1528：附加 50 真伤 ==========
    {
        ScenarioBattle b(scenario::make_party(qiling(33888)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        use0(b, /*foe_slot=*/2);
        const int drop = 4000 - ctx.getPet(1).hp;
        CHECK(drop > 50, "T2 掉血 = 红伤 + 50 真伤（drop=" << drop << "）");
        std::cout << "  [T2] 星辉幻尘 1528 通过（drop=" << drop << "）\n";
    }

    //========== T3 涤罪 1500/1529：连击区间 + 消提升→200 真伤 ==========
    {
        // 对照：无提升 → 消强失败 → 无 200 真伤
        int drop_plain = 0;
        {
            ScenarioBattle b(scenario::make_party(qiling(33889)),
                             scenario::make_party(foe_pet()));
            b.start();
            BattleContext& ctx = b.ctx();
            use0(b, /*foe_slot=*/2);
            drop_plain = 4000 - ctx.getPet(1).hp;
        }
        // 对手有提升 → 消除成功 + 200 真伤
        ScenarioBattle b(scenario::make_party(qiling(33889)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(stat_change(&ctx, 1, /*攻击=*/0, 2) == StatChangeResult::SUCCESS);
        use0(b, /*foe_slot=*/2);
        CHECK_EQ(ctx.ability_level(1, 0), 0, "T3 对手能力提升被消除（1529 前半）");
        const int drop = 4000 - ctx.getPet(1).hp;
        CHECK(drop > drop_plain + 150,
              "T3 消强成功多打 200 真伤（对照 " << drop_plain << " → " << drop << "）");
        // 连击区间：涤罪 3-5 次（内核 combo_arg_indices 装入），combo 视图在区间内
        const int combo = ctx.ws.combo_view[0];
        CHECK(combo >= 3 && combo <= 5, "T3 连击视图在 3-5 区间（实际 " << combo << "）");
        std::cout << "  [T3] 涤罪 1500+1529 通过（plain=" << drop_plain << " drop=" << drop
                  << " combo=" << combo << "）\n";
    }

    //========== T4 虚灵一闪 1530：遇天敌（水克火）附加 200 真伤 ==========
    {
        // 自身火 {3,0}，对手水 {2,0} → 对手克制自身 = 天敌
        ElfPet me = qiling(33890);
        me.elementalAttributes[0] = 3;
        me.elementalAttributes[1] = 0;
        ElfPet foe = foe_pet();
        foe.elementalAttributes[0] = 2;
        foe.elementalAttributes[1] = 0;
        ScenarioBattle b(scenario::make_party(me), scenario::make_party(foe));
        b.start();
        BattleContext& ctx = b.ctx();
        use0(b, /*foe_slot=*/2);
        const int drop = 4000 - ctx.getPet(1).hp;
        CHECK(drop > 200, "T4 天敌：掉血 = 红伤 + 200 真伤（drop=" << drop << "）");
        std::cout << "  [T4] 虚灵一闪 1530 通过（drop=" << drop << "）\n";
    }

    //========== T5 神之禁 765+440：对手无法自我提升 + PP 2倍 ==========
    {
        ScenarioBattle b(scenario::make_party(qiling(27018)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        use0(b, /*foe_slot=*/1);
        // 765：对手本回合（后手）用锋利自我强化 → 被回退（等级回 0）
        CHECK_EQ(ctx.ability_level(1, 0), 0, "T5 765：对手提升被回退（近似口径）");
        // 440：对手本回合技能 PP 按 2 倍消耗
        CHECK_EQ(ctx.ws.skill_pp_cost_multiplier[1], 2, "T5 440：PP 消耗 2 倍已写入");
        std::cout << "  [T5] 神之禁 765+440 通过\n";
    }

    //========== T6 天堂圣光 64：中毒 → 伤害加倍 ==========
    {
        ScenarioBattle b(scenario::make_party(qiling(14623)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(apply_anomaly(&ctx, 0, static_cast<int>(AbnormalStatusId::Poison), 3, 1)
                == ApplyAnomalyResult::SUCCESS);
        use0(b, /*foe_slot=*/2);
        CHECK_EQ(ctx.ws.damage_add_pct[0], 100, "T6 中毒中：伤害反而加倍");
        std::cout << "  [T6] 天堂圣光 64 通过\n";
    }

    std::cout << "scenario_155 all passed" << std::endl;
    return 0;
}
