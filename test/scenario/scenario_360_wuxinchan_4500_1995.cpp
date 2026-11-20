// 场景 360：武心婵 4500 第五技 婵·命定星缘 28328 —— effect 1995（D20，d5_D20.cpp）
//
// 覆盖：
//   选择锁   CARRY 扫描置 Skills::is_locked → query_selectable=LOCKED（§7.6 选择锁层）
//   解封线   BATTLE_ROUND_END watcher：本场曾持有三色鳞 → 全部消失的首个回合末解锁
//            （无鳞从不解封；鳞存储键 4501/4502/4503 与 batch_c 同源，场景直写模拟 soul 线）
//   1995     解放全部龙鳞：每鳞三类保护票（BREAK/STAT_CLEAR/STAT_DROP_LOCK，时长=鳞数）
//            白鳞（三色齐）五段：下1次攻击+100% / 300 固伤 / 30% 自身最大体力粉伤 /
//            随机 2 控制异常 / 对手全属性-2；无鳞解放 → 全部不发生
//
// 断言组：T1 锁与无鳞不解封 / T2 有鳞→全消→回合末解封 / T3 解放全载荷 / T4 无鳞解放空转。
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

constexpr int kMingding = 28328;  // 婵·命定星缘（1995+43+87；max_pp=1）
constexpr int kHarmless = 10938;  // 本能一击：0 威力热身（对手同款）
constexpr int kScaleR = 4501;
constexpr int kScaleY = 4502;
constexpr int kScaleB = 4503;

ElfPet wuxin(std::array<int, 5> skills, int speed = 200, int hp = 500) {
    ElfPet p = scenario::make_pet(0, "武心婵", skills, speed, /*atk=*/100, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet harmless_foe(int atk = 10, int hp = 100000) {
    ElfPet p = scenario::make_pet(1, "对手", {kHarmless, kHarmless, kHarmless, kHarmless, kHarmless},
                                  /*speed=*/100, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

int count_active_anomalies(const BattleContext& ctx, int side) {
    int n = 0;
    for (int id = 0; id <= 43; ++id) {
        if (ctx.has_active_abnormal_status(side, id)) {
            ++n;
        }
    }
    return n;
}

bool all_anomalies_control(const BattleContext& ctx, int side) {
    for (int id = 0; id <= 43; ++id) {
        if (ctx.has_active_abnormal_status(side, id)
            && !is_control_abnormal_status(static_cast<AbnormalStatusId>(id))) {
            return false;
        }
    }
    return true;
}

// 龙鳞键读写（与实现同一存储：pet.soulmark_storage；场景直写模拟 soul 线的授予/消耗）。
bool has_scale(BattleContext& ctx, int side, int key) {
    const auto& st = ctx.getPet(side).soulmark_storage;
    const auto it = st.find(key);
    if (it == st.end()) {
        return false;
    }
    const int* v = std::any_cast<int>(&it->second);
    return v && *v > 0;
}
void set_scale(BattleContext& ctx, int side, int key, int value) {
    ctx.getPet(side).soulmark_storage[key] = value;
}
void grant_three_scales(BattleContext& ctx, int side) {
    set_scale(ctx, side, kScaleR, 1);
    set_scale(ctx, side, kScaleY, 1);
    set_scale(ctx, side, kScaleB, 1);
}
void clear_three_scales(BattleContext& ctx, int side) {
    set_scale(ctx, side, kScaleR, 0);
    set_scale(ctx, side, kScaleY, 0);
    set_scale(ctx, side, kScaleB, 0);
}

Skills* find_mingding(ElfPet& pet) {
    for (Skills& s : pet.skills) {
        if (s.id == kMingding) {
            return &s;
        }
    }
    return nullptr;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== T1 选择锁 + 无鳞不解封 ==========
    // 解封判据含"本场曾持有过三色鳞"——无鳞打满整场也不白送解封（保守档）。
    {
        ScenarioBattle b(scenario::make_party(wuxin({kMingding, kHarmless, kHarmless,
                                                     kHarmless, kHarmless})),
                         scenario::make_party(harmless_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        Skills* ming = find_mingding(ctx.getPet(0));
        REQUIRE(ming != nullptr);
        CHECK(ming->is_locked, "T1 核心：战斗开始 CARRY 扫描 → 28328 置选择锁");
        CHECK(ming->query_selectable(&ctx, 0) == SkillSelectionResult::LOCKED,
              "T1 核心：query_selectable → LOCKED（§7.6 选择锁层）");

        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // 无鳞可用热身技
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(ming->is_locked, "T1 核心：无鳞回合末不解封（曾持有前提不成立）");
        std::cout << "  [T1] 选择锁/无鳞不解封 通过\n";

        //========== T2 有鳞 → 全部消失 → 首个回合末解封（同战场接力）==========
        // 第 2 回合：直写三色鳞（模拟 soul 线授予）→ 回合末记"曾持有"，仍不解封。
        grant_three_scales(ctx, 0);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(ming->is_locked, "T2 核心：持有期回合末不解封（鳞未消失）");

        // 第 3 回合：清鳞（模拟 soul 线"免疫成功当回合末消失"）→ 回合末解封。
        clear_three_scales(ctx, 0);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(!ming->is_locked, "T2 核心：曾持有 → 全部消失的首个回合末解封");
        CHECK(ming->query_selectable(&ctx, 0) == SkillSelectionResult::SELECTABLE,
              "T2 核心：解封后可选");
        std::cout << "  [T2] 解封线（曾持有→全消→回合末解锁） 通过\n";

        //========== T3 解放：三色保护 + 白鳞五段载荷（同战场，第 4 回合用 28328）==========
        grant_three_scales(ctx, 0);
        const int foe_hp_before = ctx.getPet(1).hp;
        const int self_max_hp = ctx.getPet(0).numericalBase[NumericalPropertyIndex::HP];
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 命定星缘
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();

        CHECK(!has_scale(ctx, 0, kScaleR) && !has_scale(ctx, 0, kScaleY)
                  && !has_scale(ctx, 0, kScaleB),
              "T3 核心：解放自身当前持有的全部龙鳞（三键清零）");
        CHECK(ctx.is_immune(0, ImmunityType::BREAK, ctx.currentState),
              "T3 核心：回合类效果无法被消除（BREAK 票）");
        CHECK(ctx.is_immune(0, ImmunityType::STAT_CLEAR, ctx.currentState),
              "T3 核心：能力提升无法被消除或吸取（STAT_CLEAR 票）");
        CHECK(ctx.is_immune(1, ImmunityType::STAT_DROP_LOCK, ctx.currentState),
              "T3 核心：对手能力下降无法被解除或反转（STAT_DROP_LOCK 票挂对手）");

        bool boost_ok = false;
        for (const auto& g : ctx.attack_boost_grants[0]) {
            if (g.source_effect_id == 1995 && g.remaining == 1 && g.pct == 100) {
                boost_ok = true;
            }
        }
        CHECK(boost_ok, "T3 核心：下1次攻击技能伤害提升100%（次数型 1×100%）");
        CHECK_EQ(ctx.getPet(1).hp, foe_hp_before - 300 - self_max_hp * 30 / 100,
                 "T3 核心：附加 300 固伤 + 自身最大体力 30% 百分比伤");
        CHECK_EQ(count_active_anomalies(ctx, 1), 2,
                 "T3 核心：随机附加 2 种控制类异常（不重复挑选）");
        CHECK(all_anomalies_control(ctx, 1), "T3 核心：异常全部落在控制类池");
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            CHECK_EQ(ctx.ability_levels[1][i], -2, "T3 核心：对手全属性-2（六槽）");
        }
        std::cout << "  [T3] 解放龙鳞/三类保护票/白鳞五段载荷 通过\n";
    }

    //========== T4 无鳞解放：空转（保护与载荷都不发生）==========
    // 直接解锁（模拟外部解封者）压测 1995 本体的 released=0 分支。
    {
        ScenarioBattle b(scenario::make_party(wuxin({kMingding, kHarmless, kHarmless,
                                                     kHarmless, kHarmless})),
                         scenario::make_party(harmless_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        Skills* ming = nullptr;
        for (Skills& s : ctx.getPet(0).skills) {
            if (s.id == kMingding) {
                ming = &s;
            }
        }
        REQUIRE(ming != nullptr);
        ming->is_locked = false;   // 外部解封（不走 watcher，专测本体空转分支）

        const int foe_hp_before = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();

        CHECK(!ctx.is_immune(0, ImmunityType::BREAK, ctx.currentState),
              "T4 核心：无解放 → 无 BREAK 票");
        CHECK(!ctx.is_immune(0, ImmunityType::STAT_CLEAR, ctx.currentState),
              "T4 核心：无解放 → 无 STAT_CLEAR 票");
        CHECK(!ctx.is_immune(1, ImmunityType::STAT_DROP_LOCK, ctx.currentState),
              "T4 核心：无解放 → 无 STAT_DROP_LOCK 票");
        CHECK(ctx.attack_boost_grants[0].empty(), "T4 核心：无解放 → 无攻击增伤凭证");
        CHECK_EQ(ctx.getPet(1).hp, foe_hp_before, "T4 核心：无解放 → 零附加伤害");
        CHECK_EQ(count_active_anomalies(ctx, 1), 0, "T4 核心：无解放 → 零附加异常");
        CHECK_EQ(ctx.ability_levels[1][0], 0, "T4 核心：无解放 → 对手属性不动");
        std::cout << "  [T4] 无鳞解放空转 通过\n";
    }

    std::cout << "scenario_360 全部通过" << std::endl;
    return 0;
}
