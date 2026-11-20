// 场景 P：护盾/护罩分离 + 无视护盾 + 真实伤害直通。
//
// 官方：护盾只响应红伤(NORMAL)、护罩只响应粉伤(FIXED/PERCENT)；二者结构同 ShieldBank，独立实体。
// P1 护盾吸收红伤 / 不响应粉伤
// P2 护罩吸收粉伤 / 不响应红伤
// P3 无视护盾：ws.ignore_shield（魂印 ROUND_START 置位，如无极圣武魂印"自身攻击无视护盾承伤效果"）
// P4 真实伤害直通：护盾+护罩都不响应 TRUE
#include <cassert>
#include <iostream>

#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

constexpr int kSkillZhuangJi = 10001;  // 撞击：普通(8) power 35

ElfPet make_pet(int id, int atk, int hp) {
    return scenario::make_pet(id, "精灵", {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                           kSkillZhuangJi, kSkillZhuangJi},
                              /*speed=*/200, /*atk=*/atk, /*hp=*/hp);
}

// P3 魂印：ROUND_START 置 ignore_shield（无极圣武魂印"自身攻击无视护盾承伤效果"演示）。
inline EffectResult grant_ignore_shield_effect(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ctx->ws.ignore_shield[owner] = true;
    return EffectResult::kOk;
}

inline SoulMark make_ignore_shield_soul_mark() {
    SoulMark sm;
    sm.id = 9992;
    sm.name = "无视护盾(演示)";
    sm.description = "ROUND_START 置 ignore_shield：自身攻击无视护盾/护罩";
    sm.args = EffectArgs(std::vector<int>{0, 1});
    sm.program_ = {SoulMarkNodeRef(State::BATTLE_ROUND_START, &grant_ignore_shield_effect)};
    sm.has_program_ = true;
    return sm;
}

// 红伤对战一回合，返回 pet1 掉血（可给 pet1 挂护盾/护罩）。
int red_damage(scenario::ScenarioBattle& battle, bool give_shield, bool give_hood) {
    battle.start();
    if (give_shield) {
        battle.ctx().getPet(1).shield_bank_.add(0, 100, 9001, 0);
    }
    if (give_hood) {
        battle.ctx().getPet(1).hood_bank_.add(0, 100, 9002, 0);
    }
    battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    battle.drive();
    return 5000 - battle.ctx().getPet(1).hp;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== P1 护盾：吸收红伤、不响应粉伤 ==========
    {
        auto atk = make_pet(0, 100, 800);
        auto def = make_pet(1, 100, 5000);
        scenario::ScenarioBattle b1(scenario::make_party(atk), scenario::make_party(def));
        const int dmg_red = red_damage(b1, /*shield=*/true, /*hood=*/false);
        REQUIRE(dmg_red == 0);  // 护盾 100 挡下红伤（约 31）
        std::cout << "  [P1] 护盾吸收红伤=" << dmg_red << " (HP不变)" << std::endl;

        scenario::ScenarioBattle b2(scenario::make_party(def), scenario::make_party(def));
        b2.ctx().getPet(1).shield_bank_.add(0, 100, 9001, 0);
        const int hp0 = b2.ctx().getPet(1).hp;
        deal_damage(&b2.ctx(), 1, 100, DamageKind::FIXED, 0);
        REQUIRE(hp0 - b2.ctx().getPet(1).hp == 100);  // 护盾不挡粉伤
        std::cout << "  [P1b] 护盾不响应粉伤: FIXED 100 -> "
                  << (hp0 - b2.ctx().getPet(1).hp) << std::endl;
    }

    //========== P2 护罩：吸收粉伤、不响应红伤 ==========
    {
        auto def = make_pet(1, 100, 5000);
        scenario::ScenarioBattle b1(scenario::make_party(def), scenario::make_party(def));
        b1.ctx().getPet(1).hood_bank_.add(0, 100, 9002, 0);
        const int hp0 = b1.ctx().getPet(1).hp;
        deal_damage(&b1.ctx(), 1, 100, DamageKind::FIXED, 0);
        REQUIRE(hp0 - b1.ctx().getPet(1).hp == 0);  // 护罩吸收粉伤
        std::cout << "  [P2] 护罩吸收粉伤: FIXED 100 -> HP不变" << std::endl;

        auto atk = make_pet(0, 100, 800);
        auto def2 = make_pet(1, 100, 5000);
        scenario::ScenarioBattle b2(scenario::make_party(atk), scenario::make_party(def2));
        const int dmg_red = red_damage(b2, /*shield=*/false, /*hood=*/true);
        REQUIRE(dmg_red > 0);  // 护罩不挡红伤
        std::cout << "  [P2b] 护罩不响应红伤: 红伤 -> " << dmg_red << std::endl;
    }

    //========== P3 无视护盾：攻击方魂印置 ignore_shield → 红伤绕过护盾 ==========
    {
        auto atk = make_pet(0, 100, 800);
        atk.soulMark = make_ignore_shield_soul_mark();
        auto def = make_pet(1, 100, 5000);
        scenario::ScenarioBattle b(scenario::make_party(atk), scenario::make_party(def));
        const int dmg = red_damage(b, /*shield=*/true, /*hood=*/false);
        REQUIRE(dmg > 0);  // 无视护盾 → 红伤直接作用 hp
        std::cout << "  [P3] 无视护盾: 红伤绕过护盾 -> " << dmg << std::endl;
    }

    //========== P4 真实伤害直通：护盾+护罩都不响应 TRUE ==========
    {
        auto def = make_pet(1, 100, 5000);
        scenario::ScenarioBattle b(scenario::make_party(def), scenario::make_party(def));
        b.ctx().getPet(1).shield_bank_.add(0, 100, 9001, 0);
        b.ctx().getPet(1).hood_bank_.add(0, 100, 9002, 0);
        const int hp0 = b.ctx().getPet(1).hp;
        deal_damage(&b.ctx(), 1, 100, DamageKind::TRUE_DMG, 0);
        REQUIRE(hp0 - b.ctx().getPet(1).hp == 100);  // TRUE 绕过护盾+护罩
        std::cout << "  [P4] 真实伤害直通: TRUE 100 -> " << (hp0 - b.ctx().getPet(1).hp)
                  << " (护盾+护罩都不响应)" << std::endl;
    }

    std::cout << "scenario_p PASS (护盾/护罩分离 + 无视护盾 + 真实伤害直通)" << std::endl;
    return 0;
}
