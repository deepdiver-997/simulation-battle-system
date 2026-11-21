// 场景 M：技能元素/克制倍率视图层（skill_element_view + restraint_view）。
//
// 官方克制 = 技能系别 vs 防御方系别（非攻击方精灵系别）。本场景验证视图层的三种用法：
//   M1 默认行为：克制按"技能系别视图"算——同一攻击方精灵，草系技能 vs 普通技能打水系，
//       草系克制 2x、普通 1x（旧逻辑按攻击方精灵系别，两者都是水打水=1x，区分不出来）。
//   M2 不会出现微弱：restraint_view 钳制——草打草默认 0.5（微弱），效果算 max(克制,1) 写入
//       视图 → 伤害变 1x（≈2 倍）。
//   M3 向未来的祈愿：skill_element_view 改系别——普通技能打暗影默认 1x，效果在
//       {自然20, 光12, 冰9, 超能10, 神灵223} 中取对暗影最优（光=2.0）改写视图 → 伤害≈2 倍。
//
// 魂印程序节点挂 BATTLE_FIRST_SKILL_EFFECT（攻击方高速先手；SKILL_EFFECT 在伤害计算前执行）。
// 元素 id：草1 水2 光12 暗影13 自然20 神灵223 普通8。
#include <cassert>
#include <iostream>

#include <entities/pet_factory.h>
#include <numerical-calculation/calculation.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

constexpr int kElementGrass = 1;   // 草
constexpr int kElementWater = 2;   // 水
constexpr int kElementDark = 13;   // 暗影
constexpr int kElementLight = 12;  // 光（向未来祈愿对暗影的最优解）
constexpr int kSkillZhuangJi = 10001;  // 撞击：普通(8)，无 side_effect
constexpr int kSkillGrass = 10003;     // 疾风刃：草(1)，无 side_effect

// 造攻击方：指定精灵元素（只影响本系加成；克制不看它）+ 指定技能 + 魂印（可空）。
ElfPet make_attacker(int element, int skill_id, SoulMark soul_mark = {}) {
    auto pet = scenario::make_pet(0, "我方", {skill_id, skill_id, skill_id,
                                              skill_id, skill_id},
                                  /*speed=*/200, /*atk=*/150, /*hp=*/800, std::move(soul_mark));
    pet.elementalAttributes[0] = element;
    pet.elementalAttributes[1] = 0;
    return pet;
}

ElfPet make_defender(int element, int hp = 5000) {
    auto pet = scenario::make_pet(1, "对手", {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                              kSkillZhuangJi, kSkillZhuangJi},
                                  /*speed=*/100, /*atk=*/100, /*hp=*/hp);
    pet.elementalAttributes[0] = element;
    pet.elementalAttributes[1] = 0;
    return pet;
}

// 跑一回合：start → 双方选技能 → drive。调用方持有 battle 可查 ws 视图。
void run_one_round(scenario::ScenarioBattle& battle) {
    battle.start();
    battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    battle.drive();
}

int damage_to_defender(scenario::ScenarioBattle& battle) {
    return 5000 - battle.ctx().getPet(1).hp;
}

//========== M2 效果：克制不会出现微弱（restraint_view 钳到 >=1）==========
inline EffectResult no_weak_restraint_effect(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int defender = 1 - owner;
    const double r = Calculation::calculateRestraintMultiples(
        ctx->ws.skill_element_view[owner], ctx->ws.view_elementalAttributes[defender]);
    ctx->ws.restraint_view[owner] = r < 1.0 ? 1.0 : r;
    return EffectResult::kOk;
}

inline SoulMark make_no_weak_soul_mark() {
    SoulMark sm;
    sm.id = 9995;
    sm.name = "不会出现微弱(演示)";
    sm.description = "SKILL_EFFECT：克制若 <1 钳到 1（精灵王第五技能）";
    sm.args = EffectArgs(std::vector<int>{0, 1});
    sm.program_ = {SoulMarkNodeRef(State::BATTLE_FIRST_SKILL_EFFECT, &no_weak_restraint_effect)};
    sm.has_program_ = true;
    return sm;
}

//========== M3 效果：以自然/光/冰/超能/神灵中最优系别算克制（改写 skill_element_view）==========
inline EffectResult best_element_restraint_effect(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int defender = 1 - owner;
    // 自然(20) / 光(12) / 冰(9) / 超能(10) / 神灵(223)——取对防御方克制最高者
    const int candidates[] = {20, 12, 9, 10, 223};
    int best = candidates[0];
    double best_r = -1.0;
    for (const int e : candidates) {
        const int atk[2] = {e, 0};
        const double r = Calculation::calculateRestraintMultiples(
            atk, ctx->ws.view_elementalAttributes[defender]);
        if (r > best_r) {
            best_r = r;
            best = e;
        }
    }
    ctx->ws.skill_element_view[owner][0] = best;
    ctx->ws.skill_element_view[owner][1] = 0;
    return EffectResult::kOk;
}

inline SoulMark make_best_element_soul_mark() {
    SoulMark sm;
    sm.id = 9994;
    sm.name = "向未来的祈愿(演示)";
    sm.description = "SKILL_EFFECT：以自然/光/冰/超能/神灵中最优系别计算克制倍数";
    sm.args = EffectArgs(std::vector<int>{0, 1});
    sm.program_ = {SoulMarkNodeRef(State::BATTLE_FIRST_SKILL_EFFECT, &best_element_restraint_effect)};
    sm.has_program_ = true;
    return sm;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== M1 默认克制按技能系别（skill_element_view 物化自 skill.element）==========
    {
        // 同一攻击方（水系精灵），分别用草系技能 / 普通技能 打水系防御方。
        // 克制按技能系别：草克水 2x vs 普通 1x。若还按攻击方精灵系别，两者都是水打水=1x。
        scenario::ScenarioBattle grass(scenario::make_party(make_attacker(kElementWater, kSkillGrass)),
                                       scenario::make_party(make_defender(kElementWater)));
        scenario::ScenarioBattle normal(scenario::make_party(make_attacker(kElementWater, kSkillZhuangJi)),
                                        scenario::make_party(make_defender(kElementWater)));
        run_one_round(grass);
        run_one_round(normal);
        const int dmg_grass = damage_to_defender(grass);
        const int dmg_normal = damage_to_defender(normal);
        assert(grass.ctx().ws.skill_element_view[0][0] == 1 && grass.ctx().ws.skill_element_view[0][1] == 0);
        assert(normal.ctx().ws.skill_element_view[0][0] == 8 && normal.ctx().ws.skill_element_view[0][1] == 0);
        assert(dmg_grass > 0 && dmg_normal > 0);
        assert(dmg_grass >= dmg_normal * 3 / 2);  // 2x vs 1x
        std::cout << "  [M1] 草系技能打水=" << dmg_grass << " 普通技能打水=" << dmg_normal
                  << " (克制按技能系别，非精灵系别)" << std::endl;
    }

    //========== M2 不会出现微弱（restraint_view 钳制：0.5 → 1.0）==========
    {
        scenario::ScenarioBattle without(scenario::make_party(make_attacker(kElementGrass, kSkillGrass)),
                                         scenario::make_party(make_defender(kElementGrass)));
        scenario::ScenarioBattle with(scenario::make_party(make_attacker(kElementGrass, kSkillGrass, make_no_weak_soul_mark())),
                                      scenario::make_party(make_defender(kElementGrass)));
        run_one_round(without);
        run_one_round(with);
        const int dmg_without = damage_to_defender(without);
        const int dmg_with = damage_to_defender(with);
        assert(without.ctx().ws.restraint_view[0] == -1.0);   // 未设置 → 按元素算（0.5 微弱）
        assert(with.ctx().ws.restraint_view[0] == 1.0);       // 效果已钳到 1
        assert(dmg_with >= dmg_without * 3 / 2);              // 1x vs 0.5x ≈ 2 倍
        std::cout << "  [M2] 不微弱: 无=" << dmg_without << " 有=" << dmg_with
                  << " restraint_view=" << with.ctx().ws.restraint_view[0] << " (0.5→1.0)" << std::endl;
    }

    //========== M3 向未来的祈愿（skill_element_view 改系别：普通 → 光 12）==========
    {
        scenario::ScenarioBattle without(scenario::make_party(make_attacker(kElementWater, kSkillZhuangJi)),
                                         scenario::make_party(make_defender(kElementDark)));
        scenario::ScenarioBattle with(scenario::make_party(make_attacker(kElementWater, kSkillZhuangJi, make_best_element_soul_mark())),
                                      scenario::make_party(make_defender(kElementDark)));
        run_one_round(without);
        run_one_round(with);
        const int dmg_without = damage_to_defender(without);
        const int dmg_with = damage_to_defender(with);
        assert(with.ctx().ws.skill_element_view[0][0] == kElementLight);  // 光对暗影 2x 最优
        assert(with.ctx().ws.skill_element_view[0][1] == 0);
        assert(dmg_with >= dmg_without * 3 / 2);              // 2x vs 1x
        std::cout << "  [M3] 祈愿: 无=" << dmg_without << " 有=" << dmg_with
                  << " skill_element_view[0]={光," << with.ctx().ws.skill_element_view[0][1]
                  << "}" << std::endl;
    }

    std::cout << "scenario_m PASS (技能元素/克制倍率视图层：默认物化 + 钳制 + 改系别)"
              << std::endl;
    return 0;
}
