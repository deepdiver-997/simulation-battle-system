// 场景 4350 —— K279 致命一击回血族（601，soul_lib/k279_crit_heal.cpp）
//（2026-10-21，worktree ../agents/k279；号段 4350-4359 → T4350-T4353 四块）
//
// 断言口径（601「打出致命一击时恢复1/2最大HP」）：
//   - T4350 必暴 → 回血：印主用必暴技打中对手 → 印主回血 = 自身最大HP / 2
//     （精确断言：回血量是整数分之一，无浮动）。
//   - T4351 **非暴击 → 不回血**：同一场景改用普通技（crit_rate=0，永不必暴）→
//     印主体力不变——证明门是"暴击"而非"任何攻击"。
//   - T4352 门闭对照：**不带魂印**时必暴也不回血——证明回血来自本魂印。
//   - T4353 **回血量随最大HP缩放**：把印主最大HP改小 → 回血量按新的 max/2 变
//     （证明读的是"最大HP"而非当前体力）。
//
// ⚠️ 场景构造口径：按 **DB args 原样**传 {2,2}（不补 {0,0} 前缀）——实现侧走
//   owned_int_args 原序读（K257 起的夜跑口径，见实现头注「行参读取路径」）。
// ⚠️ **暴击制造**：make_pet 默认 critical_strike_rate=0（永不必暴，场景 040 口径），
//   必暴用 `scenario::set_crit_sixteenths(pet, 16)`（= 16/16 = 100%）。
// ⚠️ 印主先扣血留回血空间（否则满血时回血不涨体力，看不见效果）——同 K194 场景
//   3275 的做法（压当前体力留回血空间，max 不变）。
#include <cstdlib>
#include <iostream>
#include <vector>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaDa = 10004;   // 拍打：物理攻击载体
constexpr int kMaxHp = 99999;

SoulMark make_mark() {
    // DB args 原序 [2,2]：db[0]=2（"1/2最大HP"，tips 自证）；db[1]=2 不消费（记档）。
    SoulMark sm(601, "K279 魂印", "test", EffectArgs(std::vector<int>{2, 2}));
    sm.monster_id = 2834;
    return sm;
}

ElfPet mark_pet(bool with_mark = true) {
    ElfPet p = with_mark
        ? make_pet(1, "天绝·狂煞", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                   /*speed=*/300, /*atk=*/100, kMaxHp, make_mark())
        : make_pet(1, "裸天绝·狂煞", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                   /*speed=*/300, /*atk=*/100, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_pet() {
    ElfPet p = make_pet(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/100, /*atk=*/1, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// 印主当前最大HP（场景用 numericalBase 口径——make_pet 建的 max 体力）。
int self_max_hp(const BattleContext& ctx, int side) {
    return ctx.seerRobot[side].elfPets[ctx.on_stage[side]]
        .numericalBase[NumericalPropertyIndex::HP];
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T4350 必暴 → 回血 maxHP/2 ══
    {
        ElfPet p = mark_pet();
        set_crit_sixteenths(p, 16);   // 必定暴击
        ScenarioBattle b(make_party(p), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        // 压当前体力留回血空间（max 不变）。
        ctx.seerRobot[0].elfPets[0].hp = 50000;
        const int hp_before = ctx.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        REQUIRE(ctx.crit_happened[0]);   // 前置：本次确实暴击了
        const int gain = ctx.getPet(0).hp - hp_before;
        const int expect = self_max_hp(ctx, 0) / 2;
        CHECK_EQ(gain, expect,
                 "T4350 601：打出致命一击 → 恢复自身 1/2 最大HP（期望 "
                     + std::to_string(expect) + "，实测 " + std::to_string(gain) + "）");
        std::cout << "  [T4350] 必暴回血 通过\n";
    }

    // ══ T4351 非暴击 → 不回血 ══
    {
        ElfPet p = mark_pet();
        // 不调 set_crit_sixteenths → make_pet 默认 critical_strike_rate=0（永不必暴）。
        ScenarioBattle b(make_party(p), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = 50000;
        const int hp_before = ctx.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        REQUIRE(!ctx.crit_happened[0]);   // 前置：本次没有暴击
        CHECK_EQ(ctx.getPet(0).hp, hp_before,
                 "T4351 601：非致命一击 → 不回血（门是'暴击'而非'任何攻击'）");
        std::cout << "  [T4351] 非暴击门闭 通过\n";
    }

    // ══ T4352 门闭对照：不带魂印 → 必暴也不回血 ══
    {
        ElfPet p = mark_pet(/*with_mark=*/false);
        set_crit_sixteenths(p, 16);
        ScenarioBattle b(make_party(p), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = 50000;
        const int hp_before = ctx.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        REQUIRE(ctx.crit_happened[0]);
        CHECK_EQ(ctx.getPet(0).hp, hp_before,
                 "T4352 601：对照——不带魂印时致命一击不回血（回血确来自本魂印）");
        std::cout << "  [T4352] 无魂印对照 通过\n";
    }

    // ══ T4353 回血量随最大HP缩放（读的是 max 而非当前体力）══
    {
        ElfPet p = mark_pet();
        set_crit_sixteenths(p, 16);
        // 最大体力改小（20000），当前体力压到 5000。
        p.numericalBase[NumericalPropertyIndex::HP] = 20000;
        ScenarioBattle b(make_party(p), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = 5000;
        const int hp_before = ctx.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        REQUIRE(ctx.crit_happened[0]);
        const int gain = ctx.getPet(0).hp - hp_before;
        const int expect = 20000 / 2;   // 新最大HP 的一半（不是当前体力 5000 的一半）
        CHECK_EQ(gain, expect,
                 "T4353 601：回血量 = 最大HP/2（max 改 20000 → 期望 "
                     + std::to_string(expect) + "，实测 " + std::to_string(gain) + "）");
        std::cout << "  [T4353] 随最大HP缩放 通过\n";
    }

    // ══ T4354 分母由行参驱动（参数化）：不同 db[0] → 不同回血量 ══
    // ⚠️ 601 的 DB args 是 [2,2]——**db[0] 与 db[1] 同值**，故"读错下标"在这行数据上
    //    **不可观测**（变异测试 MUT2 实测 exit=0 仍绿）。这是数据巧合，不是断言缺口。
    //    补本块：用**参数化**的 args（db[0]=4）验证分母值确实驱动回血量——把
    //    "读 db[0] 且按它算"这件事变成可观测行为。
    {
        ElfPet p = make_pet(1, "天绝·狂煞", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                            /*speed=*/300, /*atk=*/100, kMaxHp,
                            /*soul_mark=*/SoulMark(601, "K279 魂印", "",
                                                   EffectArgs(std::vector<int>{4, 2})));
        for (Skills& s : p.skills) {
            s.must_hit = 0;
            s.accuracy = 100;
        }
        set_crit_sixteenths(p, 16);
        ScenarioBattle b(make_party(p), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = 50000;
        const int hp_before = ctx.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        REQUIRE(ctx.crit_happened[0]);
        const int gain = ctx.getPet(0).hp - hp_before;
        const int expect = self_max_hp(ctx, 0) / 4;   // db[0]=4 → max/4
        CHECK_EQ(gain, expect,
                 "T4354 601：回血量按行参分母 db[0] 驱动（db[0]=4 → max/4，期望 "
                     + std::to_string(expect) + "，实测 " + std::to_string(gain) + "）");
        std::cout << "  [T4354] 分母由行参驱动 通过\n";
    }

    std::cout << "scenario_4350 PASS (K279 致命一击回血族)" << std::endl;
    return 0;
}
