// 场景 7670 —— J319 元模板窗族（1 宏，moves_lib/j_meta_template_window.cpp）
//（2026-11-03，worktree ../agents/j319；号段 7670-7679 → T7670/T7671/T7672 三块）
//
// 断言口径：
//   - 730「下回合起，{0}回合内每回合{1}」={0} 回合 × 语义序六槽逐回合 stat_change。
//     「下回合起」= NextRounds（施放回合不应用，R1 末全 0）；tick 落 ROUND_END，
//     drive() 停在下一回合 CHOOSE → 每轮 drive 后读到的 level = 已落地的 tick 数。
//   - 载体链上兄弟效果全不碰自身能力等级（47/48=免疫票、551=对手侧 ws 旗、
//     693=攻击增伤、125=受伤上限、927=用技回血），等级阶梯纯净可精确断言。
//   - T7671 万物生防/特防载体是**属性排列双序坑**的判别断言：语义序 idx1→引擎槽 2(防)，
//     若按引擎序直读 idx1=特攻 → 防 0/特攻 +1，CHECK 必红。
//   - 对手全程 NONE；属性技零伤害阶段，hp 不动；技能 accuracy=100+must_hit 消 miss。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;      // 拍打（filler）
constexpr int kLongShen = 24887;   // 龙神守护 [47,551,730]|[4,3,1,0,0,0,1,0]：3 回合 攻+1 速+1
constexpr int kWanWu = 27526;      // 万物生 [1491,48,47,730,927]|[…,0,1,0,1,0,0,…]：4 回合 防+1 特防+1
constexpr int kShengDian = 24551;  // 盛典进行曲 [693,730,125]|[…,3,1,1,0,1,0,0,…]：3 回合 攻+1 防+1 特防+1

constexpr int kMaxHp = 99999;

// 引擎槽序：0攻 1特攻 2防 3特防 4速 5命中。
void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
}

ElfPet self_pet(int carrier) {
    ElfPet p = make_pet(1, "己方", {carrier, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/300, kMaxHp);
    accuracy_100(p);
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, kMaxHp);
    accuracy_100(p);
    return p;
}

// 对手 NONE 一回合。
void round_none(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

// 施放载体一回合（self 选槽 0）。
void round_cast(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7670 龙神守护 730[3,1,0,0,0,1,0]：下回合起 3 回合每回合攻+1、速+1 ══
    {
        ScenarioBattle b(make_party(self_pet(kLongShen)), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        round_cast(b);   // R1 武装：「下回合起」→ 施放回合不应用
        CHECK_EQ(ctx.ability_levels[0][0], 0,
                 "T7670 R1 末攻=0（下回合起，施放回合不 tick）");
        CHECK_EQ(ctx.ability_levels[0][4], 0,
                 "T7670 R1 末速=0（下回合起，施放回合不 tick）");
        round_none(b);   // R2：窗第 1 回合
        CHECK_EQ(ctx.ability_levels[0][0], 1, "T7670 R2 末攻=1（窗 R1）");
        CHECK_EQ(ctx.ability_levels[0][4], 1, "T7670 R2 末速=1（窗 R1）");
        round_none(b);   // R3：窗第 2 回合
        CHECK_EQ(ctx.ability_levels[0][0], 2, "T7670 R3 末攻=2（窗 R2）");
        CHECK_EQ(ctx.ability_levels[0][4], 2, "T7670 R3 末速=2（窗 R2）");
        round_none(b);   // R4：窗第 3 回合（最后一回合）
        CHECK_EQ(ctx.ability_levels[0][0], 3, "T7670 R4 末攻=3（窗 R3 收尾）");
        CHECK_EQ(ctx.ability_levels[0][4], 3, "T7670 R4 末速=3（窗 R3 收尾）");
        round_none(b);   // R5：窗已过期
        CHECK_EQ(ctx.ability_levels[0][0], 3, "T7670 R5 末攻不再涨（窗过期）");
        CHECK_EQ(ctx.ability_levels[0][4], 3, "T7670 R5 末速不再涨（窗过期）");
        CHECK_EQ(ctx.ability_levels[0][1] + ctx.ability_levels[0][2] +
                     ctx.ability_levels[0][3] + ctx.ability_levels[0][5],
                 0, "T7670 其余四槽（特攻/防/特防/命中）不受影响");
        std::cout << "  [T7670] 730 攻速窗（龙神守护 3 回合）通过\n";
    }

    // ══ T7671 万物生 730[4,0,1,0,1,0,0]：下回合起 4 回合每回合防+1、特防+1 ══
    //    双序坑判别：语义序 idx1→引擎槽 2（防）。按引擎序直读会落特攻槽 → 必红。
    {
        ScenarioBattle b(make_party(self_pet(kWanWu)), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        round_cast(b);   // R1 武装
        CHECK_EQ(ctx.ability_levels[0][2] + ctx.ability_levels[0][3], 0,
                 "T7671 R1 末防+特防=0（下回合起）");
        round_none(b);   // R2
        CHECK_EQ(ctx.ability_levels[0][2], 1, "T7671 R2 末防=1（语义序 idx1→槽2）");
        CHECK_EQ(ctx.ability_levels[0][3], 1, "T7671 R2 末特防=1（语义序 idx3→槽3）");
        CHECK_EQ(ctx.ability_levels[0][1], 0,
                 "T7671 特攻槽必为 0（引擎序直读会把它打成 +1——双序坑判别位）");
        round_none(b);   // R3
        CHECK_EQ(ctx.ability_levels[0][2], 2, "T7671 R3 末防=2（窗 R2）");
        round_none(b);   // R4
        CHECK_EQ(ctx.ability_levels[0][2], 3, "T7671 R4 末防=3（窗 R3）");
        round_none(b);   // R5：窗第 4 回合（最后一回合）
        CHECK_EQ(ctx.ability_levels[0][2], 4, "T7671 R5 末防=4（窗 R4 收尾）");
        CHECK_EQ(ctx.ability_levels[0][3], 4, "T7671 R5 末特防=4（窗 R4 收尾）");
        round_none(b);   // R6：窗已过期
        CHECK_EQ(ctx.ability_levels[0][2] + ctx.ability_levels[0][3], 8,
                 "T7671 R6 不再涨（窗过期）");
        std::cout << "  [T7671] 730 双防窗（万物生 4 回合·双序坑判别）通过\n";
    }

    // ══ T7672 盛典进行曲 730[3,1,1,0,1,0,0]：下回合起 3 回合每回合攻+1、双防+1 ══
    {
        ScenarioBattle b(make_party(self_pet(kShengDian)), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        round_cast(b);   // R1 武装
        CHECK_EQ(ctx.ability_levels[0][0] + ctx.ability_levels[0][2] +
                     ctx.ability_levels[0][3],
                 0, "T7672 R1 末三槽=0（下回合起）");
        round_none(b);   // R2
        CHECK_EQ(ctx.ability_levels[0][0], 1, "T7672 R2 末攻=1（idx0→槽0）");
        CHECK_EQ(ctx.ability_levels[0][2], 1, "T7672 R2 末防=1（idx1→槽2）");
        CHECK_EQ(ctx.ability_levels[0][3], 1, "T7672 R2 末特防=1（idx3→槽3）");
        round_none(b);   // R3
        CHECK_EQ(ctx.ability_levels[0][0], 2, "T7672 R3 末攻=2（窗 R2）");
        round_none(b);   // R4：窗第 3 回合（最后一回合）
        CHECK_EQ(ctx.ability_levels[0][0], 3, "T7672 R4 末攻=3（窗收尾）");
        CHECK_EQ(ctx.ability_levels[0][2], 3, "T7672 R4 末防=3（窗收尾）");
        CHECK_EQ(ctx.ability_levels[0][3], 3, "T7672 R4 末特防=3（窗收尾）");
        round_none(b);   // R5：窗已过期
        CHECK_EQ(ctx.ability_levels[0][0] + ctx.ability_levels[0][2] +
                     ctx.ability_levels[0][3],
                 9, "T7672 R5 不再涨（窗过期）");
        std::cout << "  [T7672] 730 攻双防窗（盛典进行曲 3 回合）通过\n";
    }

    std::cout << "scenario_7670 PASS (J319 元模板窗族 730；2190 哑模板记档不产码)" << std::endl;
    return 0;
}
