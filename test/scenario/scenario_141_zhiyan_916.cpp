#include <utils/sbs_rand.h>
// 场景 141：绮影罗心·织焰 4696 技能组缺口 —— 效果 916（蛛烟罗织 37681）
//（moves_lib/batch_a_skills.cpp，2026-09-27）
//
// 916 官方模板："命中后{0}%使对手{1}，若未触发则恢复自身全部体力"
//（蛛烟罗织 args=[80,10]：80% 混乱；同技后继 936=[80,28,2] 不在本批）。
// 实现：SKILL_EFFECT 掷点 → apply_anomaly 现代通道；"未触发"= 概率没中**或**施加未
// 落地（1814 同口径）→ heal 原语回满（吃封回血）。
//
// 断言组：
//   T1 触发分支：80% 命中 → 对手混乱、自身不回满。
//   T2 未触发分支+顺序：掷点失败 → 回满，且**回满发生在对手反击之前**
//      （ heals@SKILL_EFFECT 早于对手行动 → 回满后仍要吃对手那 1 点保底红伤；
//        若实现成"行动后补血"，体力会是满值——顺序敏感断言锁死）。
//   T3 施加被免疫也算"未触发"→ 回满（"未触发"含施加未落地半边）。
//
// 确定性：全技能强制必中、暴击 0。rand 序：rand#1=916 的 80% 掷点、rand#2=936 的
// 80% 掷点、rand#3=伤害浮动。种子 SA=(rand#1<80 且断言按 T1)、SB=(rand#1≥80 → T2)。
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

constexpr int kZhuyan = 37681;    // 蛛烟罗织：机械物攻 90、先 1、[916, 936]
constexpr int kHarmless = 10938;  // 本能一击：威力 0（对手热身，固定 1 点保底红伤）
constexpr int kConfusion = static_cast<int>(AbnormalStatusId::Confusion);  // 10

// 对手全体技能挂一张异常免疫窗口票（测试桩，隔离 T3 的"未触发=施加未落地"分支）。
EffectResult anomaly_immune_soul(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    ctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(), /*mask=*/0,
                        /*duration_rounds=*/9, /*source_id=*/9995, /*soul_immunity=*/false,
                        EffectScope::ON_STAGE, /*counts=*/0, /*source_effect_id=*/9995);
    return EffectResult::kOk;
}

struct Pets {
    ElfPet self;
    ElfPet foe;
};

Pets make_pets(bool foe_immune) {
    ElfPet self = scenario::make_pet(0, "绮影罗心·织焰",
                                     {kZhuyan, kHarmless, kHarmless, kHarmless, kHarmless},
                                     /*speed=*/200, /*atk=*/200, /*hp=*/1000);
    for (Skills& s : self.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    ElfPet foe = scenario::make_pet(1, "对手", {kHarmless, kHarmless, kHarmless, kHarmless, kHarmless},
                                    /*speed=*/100, /*atk=*/100, /*hp=*/5000);
    for (Skills& s : foe.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    if (foe_immune) {
        SoulMark sm;
        sm.id = 9995;
        sm.name = "免异常桩";
        sm.effect = &anomaly_immune_soul;
        sm.args = EffectArgs(std::vector<int>{1, 0});
        foe = scenario::make_pet(1, "对手",
                                 {kHarmless, kHarmless, kHarmless, kHarmless, kHarmless},
                                 /*speed=*/100, /*atk=*/100, /*hp=*/5000, std::move(sm));
        for (Skills& s : foe.skills) {
            s.must_hit = 1;
            s.accuracy = 100;
        }
    }
    return Pets{std::move(self), std::move(foe)};
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== T1 触发分支：80% 混乱落地、自身不回满 ==========
    {
        sbs_srand(20260927);   // rand#1 < 80 → 916 触发（经验证）
        Pets p = make_pets(/*foe_immune=*/false);
        ScenarioBattle b(scenario::make_party(p.self), scenario::make_party(p.foe));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 700;   // 预留伤势：触发分支不应回满
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 蛛烟罗织
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 对手热身（1 点）
        b.drive();
        CHECK(ctx.has_active_abnormal_status(1, kConfusion), "T1 核心：80% 混乱落地");
        CHECK(ctx.getPet(0).hp <= 700,
              "T1 触发分支不回满（700 - 对手 1 点保底 = 699）");
        std::cout << "  [T1] 触发分支 通过（自身体力 " << ctx.getPet(0).hp << "）\n";
    }

    //========== T2 未触发分支+顺序：掷点失败 → 先回满、后吃对手反击 ==========
    {
        sbs_srand(12);   // 首掷=84 ≥ 80 → 916 未触发（探针验证）
        Pets p = make_pets(/*foe_immune=*/false);
        ScenarioBattle b(scenario::make_party(p.self), scenario::make_party(p.foe));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 700;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(!ctx.has_active_abnormal_status(1, kConfusion), "T2 未触发：对手无混乱");
        CHECK(ctx.getPet(0).hp > 950, "T2 核心：未触发 → 恢复自身全部体力");
        CHECK(ctx.getPet(0).hp < 1000,
              "T2 顺序敏感：回满发生在 SKILL_EFFECT（早于对手行动）→ "
              "回满后仍吃掉对手那 1 点保底红伤；若补在行动后体力应为满值 1000");
        std::cout << "  [T2] 未触发分支+回满时点 通过（自身体力 " << ctx.getPet(0).hp << "）\n";
    }

    //========== T3 施加被免疫也算"未触发"→ 回满 ==========
    {
        sbs_srand(20260927);   // 同 T1：rand#1 < 80 → 掷点通过但施加被免疫挡下
        Pets p = make_pets(/*foe_immune=*/true);
        ScenarioBattle b(scenario::make_party(p.self), scenario::make_party(p.foe));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 700;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(!ctx.has_active_abnormal_status(1, kConfusion), "T3 免疫：混乱未落地");
        CHECK(ctx.getPet(0).hp > 950,
              "T3 核心：施加未落地也算'未触发' → 恢复自身全部体力（1814 同口径）");
        std::cout << "  [T3] 免疫→未触发→回满 通过（自身体力 " << ctx.getPet(0).hp << "）\n";
    }

    std::cout << "scenario_141 all passed" << std::endl;
    return 0;
}
