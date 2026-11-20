// 场景 2290 —— J134 用技吸窗族三（1462 / 1927 / 1396，moves_lib/j_skill_window3.cpp）
//（2026-10-04，worktree ../agents/j134；号段 2290-2299 → T2290-T2292 三块）
//
// 断言口径（J6 场景 915 同款）：
//   - 纯链载体：落英缤纷 26681 [1462]|[3,200]、浴血杀阵 28223 [1927]|[4,300,2]、
//     黯辰陨陆 26415 [1396]|[4,3,2,2]（全 cat4 属性技，施放零 rand 消费）；
//   - 1462 翻倍门用 stat_change 直授正槽切换（无提升 400 / 有提升 200 精确差账，
//     红伤 final 用 ws.raw_attack_damage[0] 读）；窗长 3 靠 R4 过期钉死；
//   - 1927 低体档直写 hp=300（max=1000，300<500）→ 600 吸取、回血到 900；
//   - 1396 无用技门：R2 双方 PASS 仍结算（判别性）；动态转变：低体回合 1/2
//     （49999）、回血后非低体回合 1/3（33333）——同一窗口内就地切档。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kLuoYing = 26681;   // 落英缤纷：属 [1462] → [3,200]
constexpr int kYuXue   = 28223;   // 浴血杀阵：属 [1927] → [4,300,2]
constexpr int kAnChen  = 26415;   // 黯辰陨陆：属 [1396] → [4,3,2,2]
constexpr int kPaDa    = 10004;   // 拍打：物攻40（filler/红伤触发器）
constexpr int kSlotAtk = 0;       // 引擎攻击等级槽

void always_hit(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
}

ElfPet self_pet(std::array<int, 5> skills, int hp = 99999) {
    ElfPet p = make_pet(1, "己方", skills, /*speed=*/300, /*atk=*/1, hp);
    always_hit(p);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int hp = 99999) {
    ElfPet p = make_pet(2, "对手", skills, /*speed=*/100, /*atk=*/1, hp);
    always_hit(p);
    return p;
}

void self_skill(ScenarioBattle& b, int slot) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, slot);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

void both_act(ScenarioBattle& b, int self_slot, int foe_slot = 0) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, self_slot);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_slot);
    b.drive();
}

void both_pass(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T2290 1462 用技附加：无提升 400 / 有提升 200 / 窗长 3（落英缤纷 [3,200]）══
    {
        ScenarioBattle b(make_party(self_pet({kLuoYing, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        int foe_hp = 99999;
        self_skill(b, 0);   // R1 施放即首跳（用技 ✓，无提升 → 400；属性技无红伤）
        foe_hp -= 400;
        CHECK_EQ(ctx.getPet(1).hp, foe_hp, "T2290 R1 1462：无提升 → 附加 400（翻倍）");
        both_act(b, 1);     // R2 拍打（用技 ✓，无提升 → 400 + 红伤 raw）
        const int raw2 = ctx.ws.raw_attack_damage[0];
        foe_hp -= raw2 + 400;
        CHECK_EQ(ctx.getPet(1).hp, foe_hp, "T2290 R2 1462：附加 400 + 红伤精确差账");
        REQUIRE(stat_change(&ctx, 0, kSlotAtk, 1) == StatChangeResult::SUCCESS);
        both_act(b, 1);     // R3 有提升 → 只 200（"附加时"动态判）
        const int raw3 = ctx.ws.raw_attack_damage[0];
        foe_hp -= raw3 + 200;
        CHECK_EQ(ctx.getPet(1).hp, foe_hp, "T2290 R3 1462：有提升 → 附加 200（不翻倍）");
        both_act(b, 1);     // R4 窗过期 → 无附加
        const int raw4 = ctx.ws.raw_attack_damage[0];
        foe_hp -= raw4;
        CHECK_EQ(ctx.getPet(1).hp, foe_hp, "T2290 R4 1462：窗过期无附加（窗长=3）");
        std::cout << "  [T2290] 1462 用技附加/翻倍门/窗长 通过\n";
    }

    // ══ T2291 1927 吸取：满血 300 不回 / 低体 600 翻倍回血（浴血杀阵 [4,300,2]）══
    {
        {   // (a) 满血：非低体 → 吸 300，回血被钳 0
            ScenarioBattle b(make_party(self_pet({kYuXue, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            self_skill(b, 0);   // R1 施放即结算（用技 ✓，99999×2 ≥ 99999 非低体）
            CHECK_EQ(ctx.getPet(1).hp, 99999 - 300, "T2291.a 1927：非低体 → 吸 300 点");
            CHECK_EQ(ctx.getPet(0).hp, 99999, "T2291.a 1927：满血回血钳 0");
        }
        {   // (b) 低体（hp=300 < 1000/2）→ 吸 600、回血到 900
            ScenarioBattle b(make_party(self_pet({kYuXue, kPaDa, kPaDa, kPaDa, kPaDa},
                                                 /*hp=*/1000)),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.seerRobot[0].elfPets[0].hp = 300;   // 造低体档（300×2 < 1000）
            self_skill(b, 0);
            CHECK_EQ(ctx.getPet(1).hp, 99999 - 600, "T2291.b 1927：低体 → 吸取翻倍 600");
            CHECK_EQ(ctx.getPet(0).hp, 900, "T2291.b 1927：按实际扣减回血 300→900");
        }
        std::cout << "  [T2291] 1927 吸取/低体翻倍 通过\n";
    }

    // ══ T2292 1396 无用技门 + 动态转变（黯辰陨陆 [4,3,2,2]，低体 1/2 → 非低体 1/3）══
    {
        ScenarioBattle b(make_party(self_pet({kAnChen, kPaDa, kPaDa, kPaDa, kPaDa},
                                             /*hp=*/1000)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = 400;   // 低体档（400×2 < 1000）
        self_skill(b, 0);   // R1 施放 → 首跳即结算：低体 → 吸 1/2 = 49999
        CHECK_EQ(ctx.getPet(1).hp, 99999 - 49999, "T2292 R1 1396：低体 → 吸对手最大体力 1/2");
        CHECK_EQ(ctx.getPet(0).hp, 1000,
                 "T2292 R1 1396：回血等量但被自身上限钳断（400→1000，max=1000）");
        both_pass(b);       // R2 双方 PASS 仍结算（无用技门判别）+ 已非低体 → 1/3
        CHECK_EQ(ctx.getPet(1).hp, 50000 - 33333,
                 "T2292 R2 1396：非低体转 1/3 且 PASS 回合照常结算（无用技门）");
        CHECK_EQ(ctx.getPet(0).hp, 1000, "T2292 R2 1396：满血回血钳 0");
        std::cout << "  [T2292] 1396 无用技门/动态转变 通过\n";
    }

    std::cout << "scenario_2290 PASS (J134 用技吸窗族三)" << std::endl;
    return 0;
}
