// 场景 920 —— J7 粉伤回吸族（7 宏，moves_lib/j_pink_drain.cpp）
//（2026-10-01，worktree ../agents/j7；号段 920-924 → T920-T924 五块）
//
// 断言口径：粉伤量 = 自身最大体力×pct/100（整除，引擎 PERCENT 换算同口径）——
// 99999×20%=19999、×25%=24999、×30%=29999、×60%(852 双翻)=59999；载体全 priority
// 任性无先制对照、atk=1 消攻击噪声 → 对手掉血断言用 [粉伤, 粉伤+3] 带；回血断言
// 预置残血（满血治疗为 0 不可观测）；852 双翻判定 = 动作前 hp<max/3 一次性求值；
// 1206 爬档 R1-R3 = 20/30/40%（cap 用例因对手血量不足不立，min 逻辑走 Review）；
// 1697 免粉支净效果 = 等量真伤（若误实现为纯粉伤，免粉窗下对手掉血≈0 即红）。
#include <algorithm>
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

constexpr int kPaDa     = 10004;   // 拍打：物40（对手对攻/我方补刀）
constexpr int kHouFa    = 19078;   // 后发制人诀[[831]]        [20] 特150
constexpr int kTianShen = 18199;   // 天神灭世罚[180,839]      [25] 物160
constexpr int kShanHe   = 18330;   // 山河破碎[180,852]        [30,3] 物150
constexpr int kGuangYan = 36777;   // 光炎烈击[[1206]]         [20,10,40] 物130
constexpr int kBoLan    = 35177;   // 波澜怒起[945,1697]       [25] 特85
constexpr int kHunQi    = 34811;   // 魂起·独步苍冥[881,733,1648] [25] 特150
constexpr int kShengNu  = 35620;   // 圣怒·生杀予夺[1763,1764,1765,897] → 1765=[25] 特160

int max_hp_of(BattleContext& ctx, int side) {
    return std::max(1, ctx.getPet(side).numericalBase[NumericalPropertyIndex::HP]);
}

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int hp = 99999) {
    ElfPet p = make_pet(1, "己方", skills, /*speed=*/300, /*atk=*/1, /*hp=*/hp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int atk = 1) {
    ElfPet p = make_pet(2, "对手", skills, /*speed=*/100, atk, /*hp=*/99999);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

void grant_pink_window(BattleContext& ctx, int side) {
    ctx.grant_immunity(side, ImmunityType::PINK_DAMAGE, coverage_all(), /*anomaly_mask=*/0,
                       /*duration_rounds=*/0, /*source_id=*/9201, /*soul_immunity=*/false,
                       EffectScope::ON_STAGE, /*counts=*/0, /*source_effect_id=*/9201);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T920 831（后发制人诀 [20]）：裸粉伤 20%，无回血 ══
    {
        ScenarioBattle b(make_party(self_pet({kHouFa, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        const int foe0 = ctx.getPet(1).hp;
        const int my0 = ctx.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        const int loss = foe0 - ctx.getPet(1).hp;
        CHECK(loss >= 19999 && loss <= 20002, "T920 831：粉伤 19999 + 攻击 1~2");
        CHECK_EQ(ctx.getPet(0).hp, my0, "T920 831：无回血（自身 HP 不变）");
        std::cout << "  [T920] 831 裸粉伤通过\n";
    }

    // ══ T921 839（天神灭世罚 [25]）：粉伤 25% + 恢复等量 ══
    {
        ElfPet me = self_pet({kTianShen, kPaDa, kPaDa, kPaDa, kPaDa});
        me.hp = 99999 - 30000;   // 残血，治疗可观测
        ScenarioBattle b(make_party(std::move(me)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        const int my0 = ctx.getPet(0).hp;
        const int foe0 = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        const int loss = foe0 - ctx.getPet(1).hp;
        CHECK(loss >= 24999 && loss <= 25002, "T921 839：粉伤 24999 + 攻击 1~2");
        CHECK_EQ(ctx.getPet(0).hp - my0, 24999, "T921 839：恢复等量 24999");
        std::cout << "  [T921] 839 粉伤+回血通过\n";
    }

    // ══ T922 852（山河破碎 [30,3]）：低体(<1/3)双翻 ══
    {
        // (a) 非低体（hp=59999 ≥ 33333）：30% 粉伤 + 等量回血
        {
            ElfPet me = self_pet({kShanHe, kPaDa, kPaDa, kPaDa, kPaDa});
            me.hp = 99999 - 40000;   // 59999，不低体
            ScenarioBattle b(make_party(std::move(me)),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            const int my0 = ctx.getPet(0).hp;
            const int foe0 = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = foe0 - ctx.getPet(1).hp;
            CHECK(loss >= 29999 && loss <= 30002, "T922.a 852 非低体：粉伤 29999（不翻倍）");
            CHECK_EQ(ctx.getPet(0).hp - my0, 29999, "T922.a 852：恢复 29999");
        }
        // (b) 低体（hp=30000 < 33333）：粉伤与恢复双翻 → 60% = 59999
        {
            ElfPet me = self_pet({kShanHe, kPaDa, kPaDa, kPaDa, kPaDa});
            me.hp = 30000;   // < 99999/3
            ScenarioBattle b(make_party(std::move(me)),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            const int my0 = ctx.getPet(0).hp;
            const int foe0 = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = foe0 - ctx.getPet(1).hp;
            CHECK(loss >= 59999 && loss <= 60002, "T922.b 852 低体：粉伤双翻 59999");
            CHECK_EQ(ctx.getPet(0).hp - my0, 59999, "T922.b 852：恢复同倍率 59999");
        }
        std::cout << "  [T922] 852 低体双翻通过\n";
    }

    // ══ T923 1206（光炎烈击 [20,10,40]）：每次使用 +10%，R1-R3 = 20/30/40% ══
    {
        ScenarioBattle b(make_party(self_pet({kGuangYan, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        const int foe0 = ctx.getPet(1).hp;
        const int expect[3] = {19999, 29999, 39999};
        for (int r = 0; r < 3; ++r) {
            const int before = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = before - ctx.getPet(1).hp;
            CHECK(loss >= expect[r] && loss <= expect[r] + 3,
                  "T923 1206：R" + std::to_string(r + 1) + " 粉伤爬档 " +
                      std::to_string(expect[r]) + "+攻击");
        }
        CHECK(ctx.getPet(1).hp > 0, "T923 前置自洽：对手存活（累计掉血 < 99999）");
        std::cout << "  [T923] 1206 爬档粉伤通过（20/30/40%）\n";
    }

    // ══ T924 1697 免粉转真伤；1648/1765 常径（裂痕/印记恒无，记档）══
    {
        // 1697（波澜怒起 [25]）：(a) 无免粉 → 粉伤落地；(b) 免粉窗 → 等量真伤补位
        {
            ScenarioBattle b(make_party(self_pet({kBoLan, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            const int foe0 = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = foe0 - ctx.getPet(1).hp;
            CHECK(loss >= 24999 && loss <= 25002, "T924.a 1697：粉伤 24999 落地");
        }
        {
            ScenarioBattle b(make_party(self_pet({kBoLan, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            grant_pink_window(ctx, 1);
            const int foe0 = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = foe0 - ctx.getPet(1).hp;
            CHECK(loss >= 24999 && loss <= 25002,
                  "T924.a 1697：免粉下等量真伤补位（纯粉伤实现此处≈0 即红）");
        }
        // 1648（魂起·独步苍冥 → 1648=[25]）：裂痕恒无 → 常径粉伤+回血
        {
            ElfPet me = self_pet({kHunQi, kPaDa, kPaDa, kPaDa, kPaDa});
            me.hp = 99999 - 30000;
            ScenarioBattle b(make_party(std::move(me)),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            const int my0 = ctx.getPet(0).hp;
            const int foe0 = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = foe0 - ctx.getPet(1).hp;
            CHECK(loss >= 24999 && loss <= 25002, "T924.b 1648：常径粉伤 24999");
            CHECK_EQ(ctx.getPet(0).hp - my0, 24999, "T924.b 1648：恢复等量");
        }
        // 1765（圣怒·生杀予夺 → 1765=[25]）：印记恒无 → 常径粉伤+回血
        {
            ElfPet me = self_pet({kShengNu, kPaDa, kPaDa, kPaDa, kPaDa});
            me.hp = 99999 - 30000;
            ScenarioBattle b(make_party(std::move(me)),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            const int my0 = ctx.getPet(0).hp;
            const int foe0 = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = foe0 - ctx.getPet(1).hp;
            CHECK(loss >= 24999 && loss <= 25002, "T924.c 1765：常径粉伤 24999");
            CHECK_EQ(ctx.getPet(0).hp - my0, 24999, "T924.c 1765：恢复等量");
        }
        std::cout << "  [T924] 1697 免粉转真 + 1648/1765 常径通过\n";
    }

    return 0;
}
