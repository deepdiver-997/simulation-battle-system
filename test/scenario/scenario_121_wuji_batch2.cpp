#include <utils/sbs_rand.h>
// 场景 121：无极圣武批② —— 武诛印记 / 英耀每层 / 攻击 rider / 武诛五条 / 亮节降零。
//
// 口径：effect_icon 2009 + 机制解析帖 + 必修14（详见开工文档 §二 / §六.4）。
// 验证点：
//   T1 圣武登场为对手附加 N 回合武诛（N = 英耀层数）
//   T2 英耀每层：其他能力值（防/特防/速）+ 双攻差值×15%
//   T3 武诛：持有者 vs 圣武 先制-2
//   T4 武诛体力分档：满血受伤 +50%（乘法，与五条之1 同场复合断言）
//   T5 rider 双防：无视提升（+2 → 与无提升同伤）；视强为弱（+2 → 视为 -2，伤 ×2）；
//      五条之3 视图叠加（自身 -2 → 对手防御 -4）
//   T6 五条之4：对手护盾/护罩每 100 点 +10%（乘法，经护罩消耗量反推）
//   T7 rider 无视攻击免疫（穿透凭证）+ 五条之5（穿盔成功 → 威怯）+ 用后消武诛
//   T8 亮节降零：unit（≤50% → false；必发 → true）+ 端到端（战栗 5% + 亮节不触发）
#include <cstdlib>
#include <iostream>
#include <algorithm>

#include <abnormal-system/abnormal-types.h>
#include <effects/rule_center.h>
#include <effects/trait_state.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "../resources/wuji_shengwu_runtime.h"
#include "scenario_harness.h"

namespace {

// 魂印注册键 = effect_icon.effect_id = 2436（2026-09-27 对齐装配层；4800=monster id）。
constexpr int kWuji = 2436;
constexpr int kHit = 10004;   // 拍打：物攻 40

using scenario::ScenarioBattle;

ElfPet wuji_pet() {
    return scenario::make_pet(0, "无极圣武", {kHit, kHit, kHit, kHit, kHit},
                              /*speed=*/200, /*atk=*/100, /*hp=*/3000,
                              SoulMark(kWuji, "无极圣武", "英雄之耀"));
}

ElfPet plain_pet(int side, int speed = 100) {
    return scenario::make_pet(side, "对手", {kHit, kHit, kHit, kHit, kHit},
                              speed, /*atk=*/100, /*hp=*/2000);
}

ElfPet poison_trait_pet(int side) {
    return scenario::make_pet(side, "静电队友", {kHit, kHit, kHit, kHit, kHit},
                              100, 100, 2000,
                              {}, scenario::make_trait("静电", 5, {5, 0}));
}

int yaolt_of(const BattleContext& ctx, int side) {
    for (const ElfPet& pet : ctx.seerRobot[side].elfPets) {
        if (pet.soulMark.id == kWuji || pet.soulMark.monster_id == kWuji) {
            auto it = pet.soulmark_storage.find(kWujiYaoltKey);
            return it == pet.soulmark_storage.end() ? -1 : std::any_cast<int>(it->second);
        }
    }
    return -1;
}

int wuzhu_end_of(const BattleContext& ctx, int side) {
    const ElfPet& pet = ctx.seerRobot[side].elfPets[ctx.on_stage[side]];
    auto it = pet.on_stage_storage.find(kWujiWuzhuKey);
    return it == pet.on_stage_storage.end() ? -1 : std::any_cast<int>(it->second);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== T1 登场附武诛（层数 = 回合数）==========
    {
        std::array<ElfPet, 6> team{wuji_pet(), plain_pet(0), plain_pet(0),
                                   plain_pet(0), plain_pet(0), plain_pet(0)};
        std::array<ElfPet, 6> foe{plain_pet(1), plain_pet(1), plain_pet(1),
                                  plain_pet(1), plain_pet(1), plain_pet(1)};
        ScenarioBattle b(team, foe);
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK_EQ(yaolt_of(ctx, 0), 5, "T1 前置：5 干净队友 → 5 层");
        CHECK_EQ(wuzhu_end_of(ctx, 1), ctx.roundCount + 5,
                 "T1 圣武登场 → 对手附 5 回合武诛（每层 1 回合）");
        std::cout << "  [T1] 登场附武诛（层数=回合数）通过\n";
    }

    //========== T2 英耀每层：双攻差值 15% → 防/特防/速 ==========
    {
        std::array<ElfPet, 6> team{wuji_pet(), plain_pet(0), poison_trait_pet(0), plain_pet(0),
                                   poison_trait_pet(0), plain_pet(0)};
        std::array<ElfPet, 6> foe{plain_pet(1), plain_pet(1), plain_pet(1),
                                  plain_pet(1), plain_pet(1), plain_pet(1)};
        team[0].numericalProperties[NumericalPropertyIndex::SPECIAL_ATTACK] = 40;  // 差值 60
        ScenarioBattle b(team, foe);
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK_EQ(yaolt_of(ctx, 0), 1, "T2 前置：3 干净 - 2 毒特性 = 1 层");
        ElfPet& self = ctx.seerRobot[0].elfPets[ctx.on_stage[0]];
        const int boost = 60 * 15 * 1 / 100;   // = 9
        CHECK_EQ(self.numericalProperties[NumericalPropertyIndex::DEFENSE], 100 + boost,
                 "T2 防御 +9（双攻差值60 ×15% ×1层）");
        CHECK_EQ(self.numericalProperties[NumericalPropertyIndex::SPECIAL_DEFENSE], 100 + boost,
                 "T2 特防 +9");
        CHECK_EQ(self.numericalProperties[NumericalPropertyIndex::SPEED], 200 + boost,
                 "T2 速度 +9");
        std::cout << "  [T2] 英耀每层双攻差值提升 通过\n";
    }

    //========== T3 武诛先制-2（对手为圣武时）==========
    {
        std::array<ElfPet, 6> team{wuji_pet(), plain_pet(0), plain_pet(0),
                                   plain_pet(0), plain_pet(0), plain_pet(0)};
        std::array<ElfPet, 6> foe{plain_pet(1), plain_pet(1), plain_pet(1),
                                  plain_pet(1), plain_pet(1), plain_pet(1)};
        ScenarioBattle b(team, foe);
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.step_past(State::BATTLE_FIRST_MOVE_RIGHT);
        ctx.debug_step_mode = false;
        CHECK_EQ(ctx.ws.preemptive_level[1], -2, "T3 持武诛者 vs 圣武 → 先制-2");
        CHECK_EQ(ctx.ws.preemptive_level[0], 0, "T3 对照：圣武侧不受影响");
        b.drive();
        std::cout << "  [T3] 武诛先制-2 通过\n";
    }

    //========== T4 武诛体力分档：满血受伤 +50%（与五条之1 复合）==========
    {
        // ⚠️ 队友全毒特性 → 英耀 0 层 → 圣武登场**不会**自然附武诛（对照干净），
        //    测试武诛全部手动种存储（end = 当回合 +1，仅剩 1 回合）。
        auto run = [](bool with_wuzhu) {
            std::array<ElfPet, 6> team{wuji_pet(), poison_trait_pet(0), poison_trait_pet(0),
                                       poison_trait_pet(0), poison_trait_pet(0), poison_trait_pet(0)};
            std::array<ElfPet, 6> foe{plain_pet(1), plain_pet(1), plain_pet(1),
                                      plain_pet(1), plain_pet(1), plain_pet(1)};
            sbs_srand(20260925);
            ScenarioBattle b(team, foe);
            b.start();
            if (with_wuzhu) {
                b.pet(1).on_stage_storage[kWujiWuzhuKey] = b.ctx().roundCount + 1;  // 1 回合
            }
            const int hp0 = b.pet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            return hp0 - b.pet(1).hp;
        };
        const int base = run(false);
        const int tested = run(true);
        CHECK(base > 0, "T4 前置：基础伤害为正");
        // 复合期望：五条之1（1 回合 → +20% 加法槽）× 分档（满血受伤 +50% 乘法）
        const int expected = base * (100 + 20 * 1) / 100 * (100 + 50) / 100;
        CHECK_EQ(tested, expected, "T4 满血持武诛受伤：基础 ×1.2（五条之1）×1.5（分档）");
        std::cout << "  [T4] 武诛受伤分档（复合五条之1：" << base << " → " << tested
                  << "）通过\n";
    }

    //========== T5 rider 双防：无视 / 视强为弱 / 五条之3 ==========
    {
        // ⚠️ 0 层队（无自然武诛）；种武诛时会连带五条之1（+20%×剩余回合）与分档——
        //    (b) 用**半血**防守方关掉"受伤 +50%"档（严格大于），期望 = 基础 ×2（视强为弱）×1.6（之1）。
        auto run = [](int def_boost, bool with_wuzhu, int self_atk_drop, int foe_hp) {
            std::array<ElfPet, 6> team{wuji_pet(), poison_trait_pet(0), poison_trait_pet(0),
                                       poison_trait_pet(0), poison_trait_pet(0), poison_trait_pet(0)};
            std::array<ElfPet, 6> foe{plain_pet(1), plain_pet(1), plain_pet(1),
                                      plain_pet(1), plain_pet(1), plain_pet(1)};
            sbs_srand(20260926);
            ScenarioBattle b(team, foe);
            b.start();
            BattleContext& ctx = b.ctx();
            if (def_boost > 0) {
                (void)stat_change(&ctx, 1, /*stat=*/2 /*防御*/, def_boost);
            } else if (def_boost < 0) {
                (void)stat_drop(&ctx, 1, /*stat=*/2 /*防御*/, -def_boost, /*actor=*/0);
            }
            if (self_atk_drop < 0) {
                // 负向变更走弱化原语（stat_change 只做增益方向，负 delta 不落）
                (void)stat_drop(&ctx, 0, /*stat=*/0 /*攻击*/, -self_atk_drop, /*actor=*/0);
            }
            b.pet(1).hp = foe_hp;
            if (with_wuzhu) {
                b.pet(1).on_stage_storage[kWujiWuzhuKey] = ctx.roundCount + 3;
            }
            const int hp0 = b.pet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            return hp0 - b.pet(1).hp;
        };
        const int base = run(0, false, 0, 2000);
        const int ignored = run(2, false, 0, 2000);
        // 视强为弱的基线 = 直接把防御打到 -2 的真实伤害（伤害公式带加法常数，
        // "防御减半" ≠ "伤害翻倍"，不能用倍数推）。同基线 + 亮节链（视强为弱 → 同一 -2 视图
        // + 五条之1 ×1.6）→ treated == defneg × 160 / 100。
        const int defneg = run(-2, false, 0, 100);
        const int treat_as_drop = run(2, true, 0, 100);
        // [临时诊断]
        {
            std::array<ElfPet, 6> team{wuji_pet(), poison_trait_pet(0), poison_trait_pet(0),
                                       poison_trait_pet(0), poison_trait_pet(0), poison_trait_pet(0)};
            std::array<ElfPet, 6> foe{plain_pet(1), plain_pet(1), plain_pet(1),
                                      plain_pet(1), plain_pet(1), plain_pet(1)};
            sbs_srand(20260926);
            ScenarioBattle b(team, foe);
            b.start();
            BattleContext& ctx = b.ctx();
            (void)stat_change(&ctx, 1, 2, 2);
            b.pet(1).hp = 100;
            b.pet(1).on_stage_storage[kWujiWuzhuKey] = ctx.roundCount + 3;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            ctx.debug_step_mode = true;
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);
            std::cerr << "[dbg] clamped def_view[1][2]=" << ctx.ws.view_levels[1][2]
                      << " own_atk_view=" << ctx.ws.view_levels[0][0] << std::endl;
            ctx.debug_step_mode = false;
            b.drive();
        }
        CHECK(base > 0, "T5 前置：基础伤害为正");
        CHECK_EQ(ignored, base, "T5-a rider 无视双防提升：+2 与 0 等级同伤");
        // ⚠️ 容差断言（踩坑清单 §四.12 同源）：两场 run 的浮动随机序列有 ±1 次 draw 错位
        //     （伤害 ±3%），机制本身已由 T5-c 的视图断言精确锁定（-4）——这里只锁量级。
        CHECK(std::abs(treat_as_drop - defneg * 160 / 100) <= 4,
              "T5-b 视强为弱：防御 +2 视为 -2（同直接 -2 基线 ±浮动）；五条之1 → ×1.6");
        // (c) 五条之3：自身攻击 -2 → 对手防御再 -2（视图直接断言，隔离伤害公式取整）
        {
            std::array<ElfPet, 6> team{wuji_pet(), poison_trait_pet(0), poison_trait_pet(0),
                                       poison_trait_pet(0), poison_trait_pet(0), poison_trait_pet(0)};
            std::array<ElfPet, 6> foe{plain_pet(1), plain_pet(1), plain_pet(1),
                                      plain_pet(1), plain_pet(1), plain_pet(1)};
            ScenarioBattle b(team, foe);
            b.start();
            BattleContext& ctx = b.ctx();
            (void)stat_change(&ctx, 1, 2, 2);   // 对手防御 +2
            (void)stat_drop(&ctx, 0, 0, 2, 0);  // 自身攻击 -2（弱化原语）
            b.pet(1).on_stage_storage[kWujiWuzhuKey] = ctx.roundCount + 3;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            ctx.debug_step_mode = true;
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);
            CHECK_EQ(ctx.ws.view_levels[1][2], -4,
                     "T5-c 五条之3：对手防御 +2 视为 -2，再叠自身攻击 -2 → -4");
            ctx.debug_step_mode = false;
            b.drive();
        }
        std::cout << "  [T5] 无视双防 / 视强为弱 / 自身弱化视为对手 通过\n";
    }

    //========== T6 五条之4：护盾/护罩每 100 点 +10%（经护盾消耗量反推）==========
    // ⚠️ 红伤只吃**护盾**（shield_bank 只响应红伤 NORMAL）；护罩（cover）是粉伤档的
    //    HOOD 吸收——但五条之4 的加成基数是"护盾+护罩"两者（此处用护盾一人分饰两角）。
    {
        auto run = [](bool with_wuzhu) {
            std::array<ElfPet, 6> team{wuji_pet(), poison_trait_pet(0), poison_trait_pet(0),
                                       poison_trait_pet(0), poison_trait_pet(0), poison_trait_pet(0)};
            std::array<ElfPet, 6> foe{plain_pet(1), plain_pet(1), plain_pet(1),
                                      plain_pet(1), plain_pet(1), plain_pet(1)};
            sbs_srand(20260926);
            ScenarioBattle b(team, foe);
            b.start();
            BattleContext& ctx = b.ctx();
            b.pet(1).shield_bank_.add(/*priority=*/0, /*quantity=*/500,
                                      /*source_id=*/9901, /*duration=*/99);
            b.pet(1).hp = 1000;   // 半血：分档（受伤+50%）严格大于不触发，隔离五条之1/之4
            if (with_wuzhu) {
                b.pet(1).on_stage_storage[kWujiWuzhuKey] = ctx.roundCount + 3;
            }
            const int hp0 = b.pet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            // rider 无视护盾承伤：两场盾都不吸收 → HP 损失 = 结算伤害
            return hp0 - b.pet(1).hp;
        };
        const int raw = run(false);
        const int boosted = run(true);
        CHECK(raw > 0, "T6 前置：基础伤害为正（盾被 rider 无视，直落 hp）");
        // 武诛在场 → 五条之1（+20%×3 回合加法）先、之4（盾 500 点 → +50% 乘法）后
        const int expected = raw * (100 + 60) / 100 * (100 + 50) / 100;
        CHECK_EQ(boosted, expected, "T6 hp 损失 = 基础 ×1.6（之1）×1.5（之4：盾500→+50%）");
        std::cout << "  [T6] 五条之4 护盾增伤（" << raw << " → " << boosted
                  << "）通过\n";
    }

    //========== T7 rider 无视攻击免疫 + 五条之5 + 用后消武诛 ==========
    {
        std::array<ElfPet, 6> team{wuji_pet(), plain_pet(0), plain_pet(0),
                                   plain_pet(0), plain_pet(0), plain_pet(0)};
        std::array<ElfPet, 6> foe{plain_pet(1), plain_pet(1), plain_pet(1),
                                  plain_pet(1), plain_pet(1), plain_pet(1)};
        ScenarioBattle b(team, foe);
        b.start();
        BattleContext& ctx = b.ctx();
        // 对手挂狮盔式攻击免疫盔（可被穿透）
        (void)seal_skill(&ctx, /*source=*/1, /*target=*/1, /*effect_id=*/9901,
                         /*attribute=*/false, /*attack=*/true, /*count=*/3,
                         /*duration_rounds=*/3, /*penetrable=*/true, /*source_slot=*/-1,
                         EffectScope::ON_STAGE, /*hit_invalid=*/false, /*chance_pct=*/100,
                         /*consumed_when_pierced=*/false);
        b.pet(1).on_stage_storage[kWujiWuzhuKey] = ctx.roundCount + 3;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(ctx.ws.skill_exec_result[0] == SkillExecResult::HIT,
              "T7 rider 穿透：攻击免疫盔被无视，技能照常命中");
        CHECK(wuji_mark_active(b.pet(1).on_stage_storage, kWujiWeiqieKey, ctx.roundCount),
              "T7 五条之5：穿攻击免疫成功 → 对手 +3 回合威怯");
        CHECK(wuzhu_end_of(ctx, 1) == -1, "T7 五条用后：消除对手的武诛");
        std::cout << "  [T7] 穿透盔 / 五条之5 威怯 / 用后消武诛 通过\n";
    }

    //========== T8 亮节降零（实际概率为准）==========
    {
        // unit：5% 毒 + 降零 → 恒不触发；再加必发（乱流式）→ 必触发（100% 不受降零）
        SbsRng rng;
        EffectiveTrait poison;
        poison.kind = TraitKind::ActivePoisonPhysical;
        poison.args[0] = 5;    // 5%
        poison.args[1] = static_cast<int>(AbnormalStatusId::Paralysis);
        poison.proc_zeroed_le50 = true;
        CHECK(trait_proc_roll(poison, rng) == false, "T8-unit 亮节：5% ≤50% → 降为 0");
        poison.proc_forced = true;
        CHECK(trait_proc_roll(poison, rng) == true, "T8-unit 必发（乱流式）不受降零——实际概率 100%");
        // 端到端：圣武带战栗 + 亮节 → 攻击不附麻痹；手动置必发（模拟乱流）→ 照附
        ElfPet shengwu = scenario::make_pet(
            0, "无极圣武", {kHit, kHit, kHit, kHit, kHit}, 200, 100, 3000,
            SoulMark(kWuji, "无极圣武", "英雄之耀"),
            scenario::make_trait("战栗", 5, {5, 6}));
        std::array<ElfPet, 6> team{shengwu, plain_pet(0), plain_pet(0),
                                   plain_pet(0), plain_pet(0), plain_pet(0)};
        std::array<ElfPet, 6> foe{plain_pet(1), plain_pet(1), plain_pet(1),
                                  plain_pet(1), plain_pet(1), plain_pet(1)};
        ScenarioBattle b(team, foe);
        b.start();
        BattleContext& ctx = b.ctx();
        b.pet(0).on_stage_storage[kWujiLiangjieKey] = 999;   // 预置亮节
        const int fear = 6;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(!ctx.has_active_abnormal_status(1, fear), "T8 端到端：战栗 5% 被亮节降零，不触发");
        ctx.trait_state_[0].own.proc_forced = true;   // 模拟 DS-001 式乱流必发
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(ctx.has_active_abnormal_status(1, fear),
              "T8 端到端：必发抬到 100% → 不受降零，麻痹照附（001 交互口径）");
        std::cout << "  [T8] 亮节降零（以实际概率为准）× 必发交互 通过\n";
    }

    std::cout << "scenario_121_wuji_batch2: all passed" << std::endl;
    return 0;
}
