// 场景 4390 —— 659 统一使用 k289_boost_damage_amp.cpp，K283 不再注册。
// 官方载体：拍打10004（物理40，无副作用）；花瓣舞10044（特攻50，仅31连击[2,3]）。
// 红伤全部走真实选技/FSM；在 ATTACK_DAMAGE 刚结算后观测 HP + 快照，避免读下一回合 ws。
// +20% 是每笔临时加算，不再断言 ws 常驻20；比例容差允许整数截断，不容许重复注册的40%。
#include <utils/sbs_rand.h>

#include <cmath>
#include <iostream>
#include <vector>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kSlap = 10004;
constexpr int kPetal = 10044;
constexpr int kMaxHp = 99999;

SoulMark make_mark(const std::vector<int>& args) {
    SoulMark sm(659, "穆恩斯达", "自身处于能力提升状态,则自身伤害提升20%", EffectArgs(args));
    sm.monster_id = 2930;
    return sm;
}

ElfPet attacker(bool marked, const std::vector<int>& args, int skill_id) {
    auto pet = make_pet(1, "659载体", {skill_id, kSlap, kSlap, kSlap, kSlap},
                        300, 300, kMaxHp, marked ? make_mark(args) : SoulMark{});
    for (auto& skill : pet.skills) {
        skill.must_hit = 1;
        skill.critical_strike_rate = 0;
    }
    const auto& effects = pet.skills[0].rawEffectRecords;
    if (skill_id == kPetal) {
        CHECK(effects.size() == 1 && effects[0].effect_id == 31
                  && effects[0].args == std::vector<int>({2, 3}),
              "前提：花瓣舞官方副作用恰为31/[2,3]，无其他增伤/自强");
    } else {
        CHECK(effects.empty(), "前提：拍打官方无副作用");
    }
    return pet;
}

void reach(ScenarioBattle& b, State target) {
    for (int steps = 0; steps < 100 && b.state() != target
         && b.state() != State::FINISHED; ++steps) {
        b.step();
    }
    CHECK(b.state() == target, "有限步进必须到达预期技能时点");
}

struct Hit {
    int loss;
    DamageSnapshot damage;
};

Hit use_once(bool marked, int boost_slot = -1, std::vector<int> args = {2, 20},
             int other_amp = 0, int skill_id = kSlap) {
    auto foe = make_pet(2, "无印对照", {kSlap, kSlap, kSlap, kSlap, kSlap},
                        100, 300, kMaxHp);
    ScenarioBattle b(make_party(attacker(marked, args, skill_id)), make_party(foe));
    b.start();
    auto& ctx = b.ctx();
    if (boost_slot >= 0) {
        ctx.ability_levels[0][boost_slot] = 1;
    }
    // 单变量同 seed：能力门不掷骰；拍打本身无任何副作用可污染掉血。
    sbs_srand(4390);
    ctx.debug_step_mode = true;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    ctx.debug_step_mode = false;
    reach(b, State::BATTLE_FIRST_SKILL_EFFECT);
    CHECK(ctx.ws.skill_resolution_flags[0].allowAttackDamagePipeline,
          "前提：真实技能成功，红伤管线未被跳过");
    ctx.ws.damage_add_pct[0] += other_amp;  // 另一独立通用加算来源（在回合 reset 后设置）
    const int hp_before = b.pet(1).hp;
    reach(b, State::BATTLE_FIRST_AFTER_ACTION);
    const Hit hit{hp_before - b.pet(1).hp, ctx.resolvedDamage};
    CHECK_EQ(hit.loss, hit.damage.final, "真实HP差等于本次最终红伤，没有混入粉伤/技能附伤");
    CHECK(hit.loss > 0 && hit.damage.base > 0, "前提：真实攻击造成正红伤");
    CHECK_EQ(ctx.ws.damage_add_pct[0], other_amp,
             "本刀临时量已撤回，只保留其他来源，不能断言常驻20");
    return hit;
}

void expect_amp(const Hit& hit, int pct, const std::string& label) {
    CHECK_EQ(hit.damage.addPct, pct, label + "：AMP快照精确记录通用加算量");
    const double ratio = static_cast<double>(hit.loss) / hit.damage.base;
    CHECK(std::abs(ratio - (1.0 + pct / 100.0)) < 0.015,
          label + "：真实HP/公式原伤比例正确（实测 " + std::to_string(ratio) + "）");
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data\n";
        return 1;
    }

    // T4390 门闭：纯拍打的基线带是[87,102]，不是只比较两份可能同时错误的字段。
    const auto closed = use_once(true);
    CHECK(closed.loss >= 87 && closed.loss <= 102, "T4390 官方拍打无副作用的绝对红伤带");
    expect_amp(closed, 0, "T4390 无能力提升");

    // T4391 六个正槽均能开门；比例取各自公式原伤，攻击等级的原生增幅不混作魂印增幅。
    for (int slot = 0; slot < BattleContext::kAbilityLevelSlotCount; ++slot) {
        expect_amp(use_once(true, slot), 20, "T4391 正等级槽" + std::to_string(slot));
    }
    // 中性速度槽与无提升用同seed，实际伤害增幅也必须是1.2（允许截断）。
    const auto raised = use_once(true, 4);
    CHECK(std::abs(static_cast<double>(raised.loss) / closed.loss - 1.2) < 0.015,
          "T4391 单变量速度提升：+20%真实伤害，排除双注册/回合首+用技重复相加");

    // T4392 无印/有速度提升，不得产生魂印增伤。
    expect_amp(use_once(false, 4), 0, "T4392 无印对照");
    // T4393 DB原序：db[0]不消费；可变db[1]真驱动，不是写死20。
    expect_amp(use_once(true, 4, {999, 37}), 37, "T4393 db[1]=37，db[0]不猜");
    // T4394 显式0不能被fallback20覆盖；缺参才兜底tips20。
    expect_amp(use_once(true, 4, {2, 0}), 0, "T4394 在位0关闸");
    expect_amp(use_once(true, 4, {2}), 20, "T4394 缺失db[1]才兜底20");
    // T4395 另一+30%必须和本魂印加成合为+50%，不是乘算得到1.56。
    expect_amp(use_once(true, 4, {2, 20}, 30), 50, "T4395 20+30通用加算共存");
    // T4396 连击目前由引擎聚合原伤×N，并非N次独立伤害结算；只消费一份20%。
    const auto combo = use_once(true, 4, {2, 20}, 0, kPetal);
    CHECK(combo.damage.hitCount >= 2 && combo.damage.hitCount <= 3,
          "T4396 官方花瓣舞仅副作用31连击[2,3]确实生效");
    expect_amp(combo, 20, "T4396 聚合连击不叠加增伤");

    std::cout << "scenario_4390 PASS (659真实伤害/参数/加算/连击)\n";
    return 0;
}
