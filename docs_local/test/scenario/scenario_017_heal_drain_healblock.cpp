// 场景 Q：典型恢复/吸血/固定伤害效果 + 封回血（位覆盖时点，仿魂免）。
//
// Q1 百分比吸血：drain_hp 吸取最大体力 1/2。
// Q2 吃月亮（二类）：恢复 1/2 最大体力 + 造成等量粉伤；封回血 → 恢复=0 → 月亮=0。
// Q3 吸取固定伤害：drain_hp_amount。
// Q4 造成固定伤害：fixed_damage。
// Q5 封回血位覆盖：coverage_all=闭环恒封；state_coverage_bit(单时点)=低级，未覆盖时点恢复有效。
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
        assert(r == DrainHpResult::SUCCESS);
        assert(ctx.getPet(1).hp == 2500);  // 5000 - 2500
        assert(ctx.getPet(0).hp == 800);   // 吸满（clamp 到 max_hp）
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
        assert(healed == 400);
        assert(ctx.getPet(0).hp == 800);   // 恢复满
        assert(ctx.getPet(1).hp == 4600);  // 掉等量粉
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
        assert(c2.ws.last_heal_amount[0] == 0);  // 封回血
        assert(c2.getPet(0).hp == 400);          // 没恢复
        std::cout << "  [Q2b] 封回血: 恢复=" << c2.ws.last_heal_amount[0]
                  << " (月亮=0)" << std::endl;
    }

    //========== Q3 吸取固定伤害（drain_hp_amount） ==========
    {
        auto p0 = make_pet(0, 800);
        auto p1 = make_pet(1, 5000);
        scenario::ScenarioBattle battle(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = battle.ctx();
        const DrainHpResult r = drain_hp_amount(&ctx, 0, 1, 200);
        assert(r == DrainHpResult::SUCCESS);
        assert(ctx.getPet(1).hp == 4800);
        assert(ctx.getPet(0).hp == 800);  // 恢复 200（clamp）
        std::cout << "  [Q3] 吸取固定200: 对手->" << ctx.getPet(1).hp
                  << " 自身->" << ctx.getPet(0).hp << std::endl;
    }

    //========== Q4 造成固定伤害（fixed_damage） ==========
    {
        auto p0 = make_pet(0, 800);
        auto p1 = make_pet(1, 5000);
        scenario::ScenarioBattle battle(scenario::make_party(p0), scenario::make_party(p1));
        BattleContext& ctx = battle.ctx();
        const FixedDamageResult r = fixed_damage(&ctx, 1, 200);
        assert(r == FixedDamageResult::SUCCESS);
        assert(ctx.getPet(1).hp == 4800);
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
        assert(ctx.getPet(0).hp == 400);  // 恒封
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
        assert(c2.getPet(0).hp == 400);
        c2.currentState = State::BATTLE_FIRST_ATTACK_DAMAGE;  // 未覆盖时点 → 恢复有效
        heal(&c2, 0, 2);
        assert(c2.getPet(0).hp == 800);
        std::cout << "  [Q5b] 低级封回血(只覆盖ROUND_START): 该时点封 未覆盖时点恢复="
                  << c2.ws.last_heal_amount[0] << std::endl;
    }

    std::cout << "scenario_q PASS (典型恢复/吸血/固定伤害效果 + 封回血位覆盖)" << std::endl;
    return 0;
}
