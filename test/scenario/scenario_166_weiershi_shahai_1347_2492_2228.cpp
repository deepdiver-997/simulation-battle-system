// 场景 166：薇尔诗 4000 技能组缺口（二）—— 沙海之萌芽 38420 效果组
//（moves_lib/batch_c_skills.cpp，2026-09-27）
//
// 覆盖效果（沙海之萌芽 = [2488跳, 2498, 1347, 2492, 2228]，物攻 100、优先级+3）：
//   1347 反转对手能力提升，成功 → 令对手中毒；失败 → 消除对手能力提升
//   2498 消除对手回合类效果，成功 → 令对手烧伤+冻伤；每未触发 1 种 → 当次攻击伤害翻倍
//   2492 消除双方护盾/护罩，数额相等 → 对手 8 回合内无法主动切换且属性技能无效
//   2228 2 回合内对手造成的固定伤害和百分比伤害减少 80%
//
// 确定性纪律：红伤随机 → 对比式断言（翻倍支路 2x 差距远超 ±8% 噪声）；
// 粉伤探针/等级/授予结构是确定值，才 CHECK_EQ。
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

constexpr int kShahai = 38420;     // 沙海之萌芽：2498+1347+2492+2228（物攻 100、先制+3）
constexpr int kCanary1993 = 28327; // 玲·沧吟星海（1993 回合窗金丝雀，可被断回合拆）
constexpr int kHarmless = 10938;

ElfPet wei(std::array<int, 5> skills, int atk = 100) {
    ElfPet p = scenario::make_pet(0, "薇尔诗", skills, /*speed=*/200, atk, /*hp=*/100000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe(std::array<int, 5> skills) {
    ElfPet p = scenario::make_pet(1, "对手", skills, /*speed=*/100, /*atk=*/100, /*hp=*/100000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

int count_2492_entries(const BattleContext& ctx) {
    int n = 0;
    for (const RuleTicket& t : ctx.rule_center_.entries()) {
        if (t.source_effect_id == 2492) {
            ++n;
        }
    }
    return n;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== T1 1347：反转成功 → 中毒；2492 零额相等 → 锁切+封属 ==========
    {
        ScenarioBattle b(scenario::make_party(wei({kShahai, kHarmless, kHarmless, kHarmless,
                                                   kHarmless})),
                        scenario::make_party(foe({kHarmless, kHarmless, kHarmless, kHarmless,
                                                  kHarmless})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][0] = 2;   // 对手攻击+2（待反转）
        // R1：我沙海之萌芽（先制+3 必先手）；对手 filler
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.ability_levels[1][0], -2, "T1 核心：反转成功 → +2 变 -2");
        CHECK(ctx.has_active_abnormal_status(1, 8),
              "T1 核心：反转成功 → 100% 令对手睡眠（DB 切片 {0}=8）");
        CHECK(count_2492_entries(ctx) == 2,
              "T1 核心：双方盾罩数额 0==0 相等 → 锁切票+封属票各一条");
        std::cout << "  [T1] 1347 反转+中毒 / 2492 零额锁切 通过\n";
    }

    //========== T2 2498：断回合成功 → 烧伤+冻伤都落地 → 无增伤授予 ==========
    {
        ScenarioBattle b(scenario::make_party(wei({kHarmless, kShahai, kHarmless, kHarmless,
                                                   kHarmless})),
                        scenario::make_party(foe({kCanary1993, kHarmless, kHarmless, kHarmless,
                                                  kHarmless})));
        b.start();
        BattleContext& ctx = b.ctx();
        // R1：对手武装 1993 回合窗（后手 → R2 起结算）；我 filler
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(ctx.has_round_effects(1), "T2 前置：对手 1993 回合窗已挂");
        // R2：我沙海之萌芽 → 2498 断窗成功 → 烧伤+冻伤 100% 落地 → 无翻倍
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        CHECK(!ctx.has_round_effects(1), "T2 核心：对手回合类效果被消除");
        CHECK(ctx.has_active_abnormal_status(1, 2), "T2 核心：烧伤落地");
        CHECK(ctx.has_active_abnormal_status(1, 5), "T2 核心：冻伤落地");
        std::cout << "  [T2] 2498 断窗+双异常落地 通过\n";
    }

    //========== T3 2498：异常全被免 → 两档全 miss → 当次攻击伤害翻倍 ==========
    {
        ScenarioBattle b(scenario::make_party(wei({kHarmless, kShahai, kHarmless, kHarmless,
                                                   kHarmless})),
                        scenario::make_party(foe({kCanary1993, kHarmless, kHarmless, kHarmless,
                                                  kHarmless})));
        b.start();
        BattleContext& ctx = b.ctx();
        // 对手全程异常免疫（窗口 9 回合）→ 2498 两档全 miss（1347 的睡眠同样被免）
        ctx.grant_immunity(1, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                           /*duration_rounds=*/9, /*source_id=*/999123,
                           /*soul_immunity=*/false, EffectScope::ON_STAGE, /*counts=*/0,
                           /*source_effect_id=*/999123);
        // 对手挂 50 点护罩 → 2492 数额不等（0 vs 50）→ 不授锁切/封属票，
        // R3 的断回合因此无可清目标（NO_EFFECTS）→ R3 伤害是干净基线
        ctx.getPet(1).hood_bank_.add(/*priority=*/0, 50, /*source_id=*/998, /*duration=*/0);
        // R1：对手武装 1993 回合窗（R2 起结算）；我 filler
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const int hp_r1 = ctx.getPet(1).hp;
        // R2：断窗成功 + 两档全 miss → 当次攻击伤害翻倍
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        const int hp_r2 = ctx.getPet(1).hp;
        CHECK(!ctx.has_round_effects(1), "T3 前置：断窗成功");
        CHECK(!ctx.has_active_abnormal_status(1, 2), "T3 前置：烧伤被免疫（miss 支路）");
        // R3：窗已断 → 2498 整条不触发（无翻倍）；防御等级两轮相同（-1，反转于 R2 生效）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        const int loss_double = hp_r1 - hp_r2;
        const int loss_normal = hp_r2 - ctx.getPet(1).hp;
        CHECK(loss_double > loss_normal * 8 / 5,
              "T3 核心：当次攻击伤害翻倍（2x 远超 ±8% 红伤随机噪声）");
        std::cout << "  [T3] 2498 全 miss 翻倍 通过（double=" << loss_double
                  << " normal=" << loss_normal << "）\n";
    }

    //========== T4 2228：2 回合内对手造成的固定伤害减少 80% ==========
    {
        ScenarioBattle b(scenario::make_party(wei({kShahai, kHarmless, kHarmless, kHarmless,
                                                   kHarmless})),
                        scenario::make_party(foe({kHarmless, kHarmless, kHarmless, kHarmless,
                                                  kHarmless})));
        b.start();
        BattleContext& ctx = b.ctx();
        // R1：我沙海之萌芽（先手 → 2228 窗 = R1..R2）；对手 filler
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        // 探针（roundCount=2，窗内）：对手源的 100 点粉伤 → 落 20
        {
            const int hp0 = ctx.getPet(0).hp;
            (void)deal_pink_damage(&ctx, 0, 100, DamageKind::FIXED, 1);
            CHECK_EQ(hp0 - ctx.getPet(0).hp, 20, "T4 核心：窗内对手固伤 -80%（100→20）");
        }
        // R2：窗口过期（R1 起算 2 回合 → R3 探针恢复）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        {
            const int hp0 = ctx.getPet(0).hp;
            (void)deal_pink_damage(&ctx, 0, 100, DamageKind::FIXED, 1);
            CHECK_EQ(hp0 - ctx.getPet(0).hp, 100, "T4 核心：窗外恢复全额（100）");
        }
        std::cout << "  [T4] 2228 固伤减 80% 窗 通过\n";
    }

    //========== T5 2492：数额不等 → 只有消除、无锁切/封属 ==========
    {
        ScenarioBattle b(scenario::make_party(wei({kShahai, kHarmless, kHarmless, kHarmless,
                                                   kHarmless})),
                        scenario::make_party(foe({kHarmless, kHarmless, kHarmless, kHarmless,
                                                  kHarmless})));
        b.start();
        BattleContext& ctx = b.ctx();
        ElfPet& self = ctx.getPet(0);
        ElfPet& f = ctx.getPet(1);
        self.shield_bank_.add(/*priority=*/0, 100, /*source_id=*/999, /*duration=*/0);
        f.hood_bank_.add(/*priority=*/0, 50, /*source_id=*/998, /*duration=*/0);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(self.shield_bank_.total(), 0, "T5 核心：己方盾被消除");
        CHECK_EQ(f.hood_bank_.total(), 0, "T5 核心：对方罩被消除");
        CHECK_EQ(count_2492_entries(ctx), 0,
                 "T5 核心：数额 100 != 50 → 不授锁切/封属");
        std::cout << "  [T5] 2492 数额不等分支 通过\n";
    }

    std::cout << "scenario_166 all passed" << std::endl;
    return 0;
}
