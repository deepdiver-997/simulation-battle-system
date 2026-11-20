// 场景 Q：典型恢复/吸血/固定伤害效果 + 封回血（位覆盖时点，仿魂免）。
//
// Q1 百分比吸血：drain_hp 吸取最大体力 1/2。
// Q2 吃月亮（二类）：恢复 1/2 最大体力 + 造成等量粉伤；封回血 → 恢复=0 → 月亮=0。
// Q3 吸取固定伤害：drain_hp_amount。
// Q4 造成固定伤害：fixed_damage。
// Q5 封回血位覆盖：coverage_all=闭环恒封；state_coverage_bit(单时点)=低级，未覆盖时点恢复有效。
//
// Q1b/Q2c/Q3b/Q3c（2026-10-21 用户口径补测）：吸取体力（百分比 drain_hp / 固定
//   drain_hp_amount）**即使对手免粉（PINK_DAMAGE 免疫）也照样恢复自己**——吸取走
//   **红伤路径** deal_damage（不查粉伤免疫）；但**自己被封回血（HEAL_BLOCK）就不
//   恢复**（heal_impl 查 HEAL_BLOCK 免疫）。
#include <cassert>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

constexpr int kSkillZhuangJi = 10001;  // 撞击：普通(8)

ElfPet make_pet(int id, int hp) {
    return scenario::make_pet(id, "精灵", {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                           kSkillZhuangJi, kSkillZhuangJi},
                              /*speed=*/100, /*atk=*/100, /*hp=*/hp);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== Q1 百分比吸血（吸取最大体力 1/2） ==========
    {
        auto p0 = make_pet(0, 800);
        auto p1 = make_pet(1, 5000);
        scenario::ScenarioBattle battle(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = battle.ctx();
        const DrainHpResult r = drain_hp(&ctx, 0, 1, 2);  // 50%
        REQUIRE(r == DrainHpResult::SUCCESS);
        REQUIRE(ctx.getPet(1).hp == 2500);  // 5000 - 2500
        REQUIRE(ctx.getPet(0).hp == 800);   // 吸满（clamp 到 max_hp）
        std::cout << "  [Q1] 百分比吸血1/2: 对手 5000->" << ctx.getPet(1).hp
                  << " 自身 800->" << ctx.getPet(0).hp << std::endl;
    }

    //========== Q2 吃月亮（二类：恢复1/2最大体力 + 造成等量粉伤） ==========
    {
        auto p0 = make_pet(0, 800);
        auto p1 = make_pet(1, 5000);
        scenario::ScenarioBattle battle(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = battle.ctx();
        ctx.getPet(0).hp = 400;  // 半血
        heal(&ctx, 0, 2);        // 恢复 1/2 最大体力
        const int healed = ctx.ws.last_heal_amount[0];
        deal_damage(&ctx, 1, healed, DamageKind::FIXED, 0);  // 等量粉伤
        REQUIRE(healed == 400);
        REQUIRE(ctx.getPet(0).hp == 800);   // 恢复满
        REQUIRE(ctx.getPet(1).hp == 4600);  // 掉等量粉
        std::cout << "  [Q2] 吃月亮(二类): 恢复=" << healed
                  << " 对手掉=" << (5000 - ctx.getPet(1).hp) << std::endl;

        // Q2b 封回血 → 恢复被封锁 → 月亮=0
        auto p2 = make_pet(0, 800);
        auto p3 = make_pet(1, 5000);
        scenario::ScenarioBattle b2(scenario::make_party(p2), scenario::make_party(p3));
        BattleContext& c2 = b2.ctx();
        c2.grant_immunity(0, ImmunityType::HEAL_BLOCK, coverage_all(), 0, 0, 8001);
        c2.getPet(0).hp = 400;
        heal(&c2, 0, 2);
        REQUIRE(c2.ws.last_heal_amount[0] == 0);  // 封回血
        REQUIRE(c2.getPet(0).hp == 400);          // 没恢复
        std::cout << "  [Q2b] 封回血: 恢复=" << c2.ws.last_heal_amount[0]
                  << " (月亮=0)" << std::endl;
    }

    //========== Q1b 百分比吸血 + 对手免粉：仍恢复自己（2026-10-21 用户口径）==========
    // 吸取走**红伤路径** deal_damage（不查 PINK_DAMAGE）→ 对手免粉不影响自己回血。
    {
        auto p0 = make_pet(0, 800);
        auto p1 = make_pet(1, 5000);
        scenario::ScenarioBattle battle(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = battle.ctx();
        ctx.getPet(0).hp = 400;   // 自己半血 → 恢复量可观测（不 clamp）
        // 对手挂 PINK_DAMAGE 免疫（免粉）。
        ctx.grant_immunity(1, ImmunityType::PINK_DAMAGE, coverage_all(), 0, 0, 9101);
        const int self_hp0 = ctx.getPet(0).hp;
        const DrainHpResult r = drain_hp(&ctx, 0, 1, 2);  // 50%
        // 观测：对手免粉时对手不掉血（伤害被粉伤免疫挡下），但自己照样恢复——
        // 吸取的"回血"与"伤害是否落地"**解耦**（官方口径 2026-10-21 用户拍板）。
        CHECK(ctx.getPet(1).hp == 5000, "Q1b 百分比吸血：对手免粉 → 对手不掉血");
        // 官方口径（2026-10-21 用户拍板）：**对手免粉也照样恢复自己体力**
        //   ——吸取的"回血"与"伤害是否落地"解耦。
        CHECK(ctx.getPet(0).hp > self_hp0,
              "Q1b 百分比吸血：对手免粉时**自己照样恢复体力**（用户口径 2026-10-21）");
        std::cout << "  [Q1b] 百分比吸血+对手免粉: 仍恢复自己 自身->"
                  << ctx.getPet(0).hp << std::endl;
    }

    //========== Q2c 百分比吸血 + 自己封回血：不恢复（2026-10-21 用户口径）==========
    {
        auto p0 = make_pet(0, 800);
        auto p1 = make_pet(1, 5000);
        scenario::ScenarioBattle battle(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = battle.ctx();
        ctx.grant_immunity(0, ImmunityType::HEAL_BLOCK, coverage_all(), 0, 0, 9102);
        ctx.getPet(0).hp = 400;   // 半血 → 恢复量可观测
        const int hp0 = ctx.getPet(0).hp;
        const DrainHpResult r = drain_hp(&ctx, 0, 1, 2);
        REQUIRE(r == DrainHpResult::SUCCESS);
        REQUIRE(ctx.getPet(1).hp == 2500);  // 对手照样掉血
        REQUIRE(ctx.getPet(0).hp == hp0);    // 自己被封回血 → 不恢复
        std::cout << "  [Q2c] 百分比吸血+自己封回血: 不恢复 自身->"
                  << ctx.getPet(0).hp << std::endl;
    }

    //========== Q3 吸取固定伤害（drain_hp_amount） ==========
    {
        auto p0 = make_pet(0, 800);
        auto p1 = make_pet(1, 5000);
        scenario::ScenarioBattle battle(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = battle.ctx();
        const DrainHpResult r = drain_hp_amount(&ctx, 0, 1, 200);
        REQUIRE(r == DrainHpResult::SUCCESS);
        REQUIRE(ctx.getPet(1).hp == 4800);
        REQUIRE(ctx.getPet(0).hp == 800);  // 恢复 200（clamp）
        std::cout << "  [Q3] 吸取固定200: 对手->" << ctx.getPet(1).hp
                  << " 自身->" << ctx.getPet(0).hp << std::endl;
    }

    //========== Q3b 固定吸取 + 对手免粉：仍恢复自己（2026-10-21 用户口径）==========
    {
        auto p0 = make_pet(0, 800);
        auto p1 = make_pet(1, 5000);
        scenario::ScenarioBattle battle(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = battle.ctx();
        ctx.grant_immunity(1, ImmunityType::PINK_DAMAGE, coverage_all(), 0, 0, 9103);
        ctx.getPet(0).hp = 400;   // 半血 → 恢复量可观测
        const int hp0 = ctx.getPet(0).hp;
        const DrainHpResult r = drain_hp_amount(&ctx, 0, 1, 200);
        REQUIRE(r == DrainHpResult::SUCCESS);
        CHECK(ctx.getPet(0).hp > hp0,
              "Q3b 固定吸取：对手免粉时**自己照样恢复体力**（用户口径 2026-10-21）");
        std::cout << "  [Q3b] 固定吸取+对手免粉: 仍恢复自己 自身->"
                  << ctx.getPet(0).hp << std::endl;
    }

    //========== Q3c 固定吸取 + 自己封回血：不恢复（2026-10-21 用户口径）==========
    {
        auto p0 = make_pet(0, 800);
        auto p1 = make_pet(1, 5000);
        scenario::ScenarioBattle battle(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = battle.ctx();
        ctx.grant_immunity(0, ImmunityType::HEAL_BLOCK, coverage_all(), 0, 0, 9104);
        ctx.getPet(0).hp = 400;   // 半血 → 恢复量可观测
        const int hp0 = ctx.getPet(0).hp;
        const DrainHpResult r = drain_hp_amount(&ctx, 0, 1, 200);
        REQUIRE(r == DrainHpResult::SUCCESS);
        REQUIRE(ctx.getPet(0).hp == hp0);          // 被封回血 → 不恢复
        std::cout << "  [Q3c] 固定吸取+自己封回血: 不恢复 自身->"
                  << ctx.getPet(0).hp << std::endl;
    }

    //========== Q4 造成固定伤害（fixed_damage） ==========
    {
        auto p0 = make_pet(0, 800);
        auto p1 = make_pet(1, 5000);
        scenario::ScenarioBattle battle(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = battle.ctx();
        const FixedDamageResult r = fixed_damage(&ctx, 1, 200);
        REQUIRE(r == FixedDamageResult::SUCCESS);
        REQUIRE(ctx.getPet(1).hp == 4800);
        std::cout << "  [Q4] 造成固定200: 对手->" << ctx.getPet(1).hp << std::endl;
    }

    //========== Q5 封回血位覆盖（仿魂免：闭环恒封 vs 低级部分时点） ==========
    {
        // 闭环：coverage_all → 任何时点恒封
        auto p0 = make_pet(0, 800);
        auto p1 = make_pet(1, 5000);
        scenario::ScenarioBattle battle(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = battle.ctx();
        ctx.grant_immunity(0, ImmunityType::HEAL_BLOCK, coverage_all(), 0, 0, 8002);
        ctx.getPet(0).hp = 400;
        heal(&ctx, 0, 2);
        REQUIRE(ctx.getPet(0).hp == 400);  // 恒封
        std::cout << "  [Q5] 闭环封回血: 恢复=" << ctx.ws.last_heal_amount[0] << " (恒封)"
                  << std::endl;

        // 低级：只覆盖 BATTLE_ROUND_START → 该时点封，未覆盖时点恢复有效
        auto p2 = make_pet(0, 800);
        auto p3 = make_pet(1, 5000);
        scenario::ScenarioBattle b2(scenario::make_party(p2), scenario::make_party(p3));
        BattleContext& c2 = b2.ctx();
        c2.grant_immunity(0, ImmunityType::HEAL_BLOCK,
                          state_coverage_bit(State::BATTLE_ROUND_START), 0, 0, 8003);
        c2.getPet(0).hp = 400;
        c2.currentState = State::BATTLE_ROUND_START;  // 覆盖时点 → 封
        heal(&c2, 0, 2);
        REQUIRE(c2.getPet(0).hp == 400);
        c2.currentState = State::BATTLE_FIRST_ATTACK_DAMAGE;  // 未覆盖时点 → 恢复有效
        heal(&c2, 0, 2);
        REQUIRE(c2.getPet(0).hp == 800);
        std::cout << "  [Q5b] 低级封回血(只覆盖ROUND_START): 该时点封 未覆盖时点恢复="
                  << c2.ws.last_heal_amount[0] << std::endl;
    }

    std::cout << "scenario_q PASS (典型恢复/吸血/固定伤害效果 + 封回血位覆盖)" << std::endl;
    return 0;
}
