// 场景 2991 —— T3 必中×技能无效裁定探针（决策清单 K7，2026-10-05 用户裁定）
//
// 官方口径（用户裁定）：
//   - 必中 = 不可能 Miss，凭证存在时**跳过命中率掷骰**（含命中率乘区/必-miss 窗）；
//   - 必中**不保证命中效果**；面对**技能无效**（封属/盔/龙威）照样按未命中结算；
//   - 只有**强制执行**无视命中效果失效。
//
// 本探针锁三件事（防未来"修必中"把官方口径改坏）：
//   A. 封属盔（SEAL_ATTRIBUTE）× 必中 → SKILL_INVALID（无效照常生效，必中不绕）；
//   B. 封属·命中失效（SEAL_ATTRIBUTE_HIT）× 必中 → EFFECT_INVALID（效果失效，无补偿）；
//   C. 必-miss 窗（hit_rate_mod 压 0，H5 432 家族）× 必中 → 照常命中（官方：跳过掷骰）。
// 必中×失明（50% 正常/50% 效果失效）已有 scenario_041 覆盖，不重复。
#include <iostream>

#include <effects/event_center.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kSkillDaxue = 28319;    // 大雪纷飞：属性（封属类盔的响应对象）
constexpr int kSkillZhuangji = 10001; // 撞击：物理无附加（C 组载体）

ElfPet attacker_pet(int speed = 300) {
    ElfPet p = make_pet(1, "必中攻方", {kSkillDaxue, kSkillZhuangji, kSkillZhuangji,
                                        kSkillZhuangji, kSkillZhuangji},
                        /*speed=*/speed, /*atk=*/100, /*hp=*/5000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;    // 必中凭证（官方固有必中）
        s.accuracy = 100;
    }
    return p;
}

ElfPet defender_pet() {
    ElfPet p = make_pet(2, "守方", {kSkillZhuangji, kSkillZhuangji, kSkillZhuangji,
                                    kSkillZhuangji, kSkillZhuangji},
                        /*speed=*/100, /*atk=*/10, /*hp=*/5000);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

}  // namespace

int main() {
    // ── A. 封属盔 × 必中属性技 → SKILL_INVALID（无效不因必中而绕过）──
    {
        ScenarioBattle battle(make_party(attacker_pet()), make_party(defender_pet()));
        BattleContext& ctx = battle.ctx();
        battle.start();
        // 守方(1)给攻方(0)挂封属盔：不可穿（必中不是穿透凭证）
        ctx.rule_center_.grant_seal(/*source=*/1, /*source_slot=*/0, /*effect_id=*/70061,
                                    /*target=*/0, SealKind::SEAL_ATTRIBUTE, /*counts=*/1,
                                    /*rounds=*/0, /*penetrable=*/false, EffectScope::ON_STAGE);
        battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);  // 大雪纷飞（属性）
        battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.drive();
        CHECK(ctx.ws.skill_exec_result[0] == SkillExecResult::SKILL_INVALID,
              "A：封属盔对必中属性技照常生效（SKILL_INVALID，必中不绕技能无效）");
    }

    // ── B. 封属·命中失效 × 必中属性技 → 照常命中（HIT）但效果失效、无 SKILL_INVALID 补偿 ──
    //（引擎口径：命中失效"算命中"——skills.cpp execute() HIT_INVALID 分支；只有强制执行可绕）
    {
        ScenarioBattle battle(make_party(attacker_pet()), make_party(defender_pet()));
        BattleContext& ctx = battle.ctx();
        battle.start();
        int invalid_events = 0;
        ctx.event_center_.register_watcher(
            EventType::EVENT_SKILL_INVALID, /*owner=*/0, /*register_round=*/0,
            /*duration_rounds=*/0, /*once=*/false,
            [&invalid_events](BattleContext*, const BattleEvent&) { ++invalid_events; },
            /*scope=*/WatcherScope::TEAM);
        ctx.rule_center_.grant_seal(/*source=*/1, /*source_slot=*/0, /*effect_id=*/70062,
                                    /*target=*/0, SealKind::SEAL_ATTRIBUTE_HIT, /*counts=*/1,
                                    /*rounds=*/0, /*penetrable=*/false, EffectScope::ON_STAGE);
        battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.drive();
        CHECK(ctx.ws.skill_exec_result[0] == SkillExecResult::HIT,
              "B：命中失效对必中属性技照常生效——算命中（HIT）但效果被过滤");
        CHECK(invalid_events == 0,
              "B：效果失效不触发 SKILL_INVALID 补偿（必中未绕过命中失效，大雪 2006 未触发）");
    }

    // ── C. 必-miss 窗 × 必中物理技 → 照常命中（官方：必中跳过命中率掷骰）──
    {
        ScenarioBattle battle(make_party(attacker_pet()), make_party(defender_pet()));
        BattleContext& ctx = battle.ctx();
        battle.start();
        ctx.ws.hit_rate_mod[0] = 0.0f;   // H5 432 家族"对手所有攻击必 miss"的实现层
        const int hp_before = battle.pet(1).hp;
        battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);  // 撞击（物理）
        battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.drive();
        CHECK(ctx.ws.skill_exec_result[0] == SkillExecResult::HIT,
              "C：必中跳过命中率掷骰（hit_rate_mod 压 0 仍 HIT）");
        CHECK(battle.pet(1).hp < hp_before, "C：必中在必-miss 窗下照常造成伤害");
    }

    return 0;
}
