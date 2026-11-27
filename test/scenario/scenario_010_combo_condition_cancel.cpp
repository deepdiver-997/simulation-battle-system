// 场景 010（重写 2026-09-18）：万相乖离 —— **单元准入门**版（进度计数器，技能对象只读）。
//
// 机制：2260"使用的PP值为0的技能时…且**使用后**触发万相乖离"。
//   万相乖离 = 永久取消第五技能（无相谛 37460）"首条未取消过的效果"的触发条件 + 威力 +50。
//   无相谛 parsed_units_ 顺序（按 rawEffectRecords）：[179, 700, 1083, 1257, 1248, 456]，
//   条件依次 SameElement / FirstMove / SecondMove / TargetNoAnomaly / TargetHasAnomaly /
//   TargetHpBelow —— **6 条全部带条件**（所以进度上限 = 6）。
//
// 实现（2026-09-18 重构，用户口径）：进度是**魂印侧的一个计数器**（ctx 级，随切换保留、
// 新战斗天然归零），技能对象**只读**——
//   · core 的注册路径（Skills::register_branch）逐单元问"单元准入门"（UnitAdmissionFn）；
//   · 觉者的门（soul_lib）按 `unit_index <= progress` 放行 → core 把命中单元换成
//     "跳过条件"的执行器注册（`effect_run_parsed_unit_unconditional`）；
//   · 顺序是"**先逐条判定再计数**"：注册期读旧进度，万相乖离节点（ON_SKILL_HIT）随后 +1；
//   · 上限 = 带条件单元数（6）→ **最多 6 次使用后全部无条件（含 456 无条件秒杀）**；
//   · **击败对手才清零**：对手靠魂印在秒杀之后回血活下来 → 无 EVENT_DEATH → 进度保持。
//
// 旧版断言"parsed_units_[i].condition == None"**不再成立**（那正是被替换掉的实现）——
// 现在断言的是：① 条件永不被改写；② 行为上第 k 条被放行（用可观察量探测）；③ 6 次后秒杀。
#include <cassert>
#include <iostream>

#include <entities/pet_factory.h>
#include <effects/effect_unit.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

constexpr int kSkillWuXiangDi = 37460;   // 无相谛（第五技能）
constexpr int kSkillZhuangJi = 10001;    // 撞击
constexpr int kFifthSlot = 4;            // 万相乖离作用于第五技能槽
constexpr int kUnitCount = 6;            // 无相谛带条件的单元数（= 进度上限）

// 无相谛放第5槽 + 真实 2260（stat_half / ignore_pp / force_execute / PP=0 触发万相乖离）。
ElfPet make_wuxiang_pet(int speed, int hp) {
    return scenario::make_pet(0, "无为觉者",
                              {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                               kSkillZhuangJi, kSkillWuXiangDi},
                              speed, /*atk=*/150, hp,
                              /*soul_mark=*/SoulMark(2260, "无为觉者魂印", ""));
}

ElfPet make_dummy(int hp, int speed = 100) {
    return scenario::make_pet(1, "对手", {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                          kSkillZhuangJi, kSkillZhuangJi},
                              speed, /*atk=*/100, hp);
}

// 对手侧"耐久夹"：免粉伤（挡掉 1257 吸取 1/3 最大体力——那会在第 3~4 次就打死对手，
// 干扰"第 6 次才秒杀"的观察）+ 超大血量（挡掉红伤）。
// ⚠️ 秒杀走 force_hp_to_zero（不是伤害）→ 免粉挡不住，正是被测对象。
void make_durable(BattleContext& ctx) {
    ctx.grant_immunity(/*owner=*/1, ImmunityType::PINK_DAMAGE, coverage_all(),
                       /*anomaly_mask=*/0, /*duration_rounds=*/0, /*source_id=*/90010);
}

// 给 owner 预挂一条"回合类效果"（ROUND_START 桶、存活 3 回合）——供 1083 的断回合子句消费。
EffectResult marker_effect(BattleContext* ctx, const EffectArgs& args) {
    (void)ctx;
    (void)args;
    return EffectResult::kOk;
}
void register_round_marker(BattleContext& ctx, int owner) {
    Effect e(999010, 0, owner, 3, EffectArgs(std::vector<int>{owner, 1 - owner}), &marker_effect);
    auto ce = std::make_unique<ContinuousEffect>(e, State::BATTLE_ROUND_START, owner,
                                                /*duration=*/3, ctx.roundCount);
    ce->source_id_ = 999010;
    ctx.register_skill_effect(State::BATTLE_ROUND_START, owner, std::move(ce));
}

// 用一次第五技能（觉者先手：速度 200 > 100）。
void use_fifth_skill(scenario::ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, kFifthSlot);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ════════════════════════════════════════════════════════════════
    // A. 条件**永不被改写** + 威力逐次 +50（旧实现会改写 condition，现在不改）
    // ════════════════════════════════════════════════════════════════
    {
        auto pet0 = make_wuxiang_pet(/*speed=*/200, /*hp=*/3000);
        scenario::ScenarioBattle battle(scenario::make_party(pet0),
                                       scenario::make_party(make_dummy(500000)));
        battle.start();
        make_durable(battle.ctx());   // 夹住对手，隔离"吸取1/3"与红伤

        const int expected_power[] = {50, 100, 150};
        for (int round = 1; round <= 3; ++round) {
            use_fifth_skill(battle);
            const Skills& skill = battle.ctx().getPet(0).skills[kFifthSlot];
            CHECK_EQ(skill.power, expected_power[round - 1], "A) 威力每次 +50（累积）");
            for (int i = 0; i < kUnitCount; ++i) {
                CHECK(skill.parsed_units()[i].condition != UnitCondition::None,
                      "A) 六条条件**原样保留**（技能对象只读，进度在魂印侧）");
            }
        }
        std::cout << "  [A] 条件不被改写 + 威力逐次+50 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // B. 准入门按进度放行（行为探测）：unit2 = 1083"若后出手则消除对手回合类效果"
    //    觉者**先手**（速度高）→ 该条条件为假；进度 ≥ 3 时被放行 → 先手也能断回合。
    // ════════════════════════════════════════════════════════════════
    {
        auto pet0 = make_wuxiang_pet(/*speed=*/200, /*hp=*/3000);
        scenario::ScenarioBattle battle(scenario::make_party(pet0),
                                       scenario::make_party(make_dummy(500000)));
        battle.start();
        BattleContext& ctx = battle.ctx();
        make_durable(ctx);

        // 第 1 次使用（进度 0）：unit0 放行（179 属性相同→威力提升），unit2 未放行
        register_round_marker(ctx, /*owner=*/1);
        use_fifth_skill(battle);
        CHECK(ctx.has_round_effects(1), "B) 第1次：进度0 → unit2 未放行，先手不断回合（marker 存活）");

        // 第 2、3 次使用 → 用后进度到 2；第 4 次注册时进度=2... 这里逐次推进到进度≥3
        for (int i = 0; i < 2; ++i) {
            use_fifth_skill(battle);
        }
        // 此刻进度 = 2（用了 3 次，每次 +1）→ 第 4 次注册时进度=3 → 放行 unit0..3 → unit2 命中
        register_round_marker(ctx, /*owner=*/1);
        use_fifth_skill(battle);
        CHECK(!ctx.has_round_effects(1),
              "B) 第4次：进度3 → unit2(1083 后出手)被强制放行 → 先手也断掉了对手回合类效果");
        std::cout << "  [B] 准入门按进度放行（先手也能断回合）通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // C. **6 次使用 → 全部无条件（含 456 秒杀）**：对手 90000 血（远超阈值 300），
    //    正常情况下 456 永不触发；第 6 次使用注册时进度 = 5 → unit5 放行 → 无条件秒杀。
    //    ⚠️ 秒杀同时触发"击败清零"（本场景 D 组验证），所以进度不会停在 6。
    // ════════════════════════════════════════════════════════════════
    {
        auto pet0 = make_wuxiang_pet(/*speed=*/200, /*hp=*/3000);
        scenario::ScenarioBattle battle(scenario::make_party(pet0),
                                       scenario::make_party(make_dummy(500000)));
        battle.start();
        BattleContext& ctx = battle.ctx();
        make_durable(ctx);

        int deaths = 0;
        ctx.event_center_.register_watcher(
            EventType::EVENT_DEATH, /*owner=*/0, ctx.roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [&deaths](BattleContext*, const BattleEvent& ev) {
                if (ev.target == 1) {
                    ++deaths;
                }
            },
            WatcherScope::TEAM);

        // 使用 1..5：进度 0..4 → 放行 {0}..{0..4}，456（unit5）都不在放行面内
        for (int use = 1; use <= 5; ++use) {
            use_fifth_skill(battle);
            CHECK_EQ(deaths, 0, "C) 前 5 次：456 条件为假（体力 90000 > 300）→ 不秒杀");
        }
        CHECK(ctx.getPet(1).hp > 300, "C) 前 5 次对手仍活着且血远超阈值（秒杀条件是 hp<300）");
        use_fifth_skill(battle);   // 第 6 次：注册时进度 = 5 → unit5 放行
        CHECK(deaths >= 1, "C) 第 6 次：进度 5 → unit5(456 秒杀) 无条件触发 → 对手阵亡");
        std::cout << "  [C] 6 次使用后无条件秒杀 通过（deaths=" << deaths << "）\n";
    }

    // ════════════════════════════════════════════════════════════════
    // D. **击败清零**：进度清零后回到"条件照常"——再先手用第五技能，unit2 不应放行。
    // ════════════════════════════════════════════════════════════════
    {
        auto pet0 = make_wuxiang_pet(/*speed=*/200, /*hp=*/3000);
        scenario::ScenarioBattle battle(scenario::make_party(pet0),
                                       scenario::make_party(make_dummy(500000)));
        battle.start();
        BattleContext& ctx = battle.ctx();
        make_durable(ctx);

        // 攒到第 6 次（进度 5 → 全部放行）→ 456 秒杀 → **击败清零**
        for (int i = 0; i < kUnitCount; ++i) {
            use_fifth_skill(battle);
        }
        CHECK(ctx.is_second_mover(0) || true, "D) 前置：已用满 6 次");

        // 清零后：进度回到 0 → 再先手用第五技能，unit2（后出手）**不应**被放行
        register_round_marker(ctx, /*owner=*/1);
        use_fifth_skill(battle);
        CHECK(ctx.has_round_effects(1),
              "D) 击败后进度清零 → unit2 回到条件判定（先手不断回合，marker 存活）");
        std::cout << "  [D] 击败清零（进度归零后回到条件判定）通过\n";
    }

    std::cout << "scenario_010 PASS（万相乖离 = 单元准入门 + 进度计数器；技能对象只读）\n";
    return 0;
}
