// 场景 8030：J334 效果 410「{0}%恢复自身1/{1}体力值」——概率比例回血
//（resources/moves_lib/j410_prob_heal_fraction.cpp，2026-11-06）
//
// 覆盖：
//   T1  P=15 拒绝侧（钉种子 r=15）：掷点申报、零治疗、账置 0
//   T2  P=15 通过侧（r=14）：实际=max_hp/4 整除精确、attempts 累计
//   T3  [30,3] 雪花治愈边界对（r=29 过 / r=30 拒）+ 1000/3=333 整除
//   T4  [50,2] 17603 混链装配（前 410 后 93，93 已实现良性）：名义 500
//   T5  确定性（同种子两次对局审计全等）
//   T6  减疗（heal_mod_pct=-50 → 实际=名义×50%）
//   T7  封疗（HEAL_BLOCK → 实际=0）
//   T8  双方镜像（审计键 owner×16 隔离）
//   T9  固定 20 种子扫描：过闸率侧证 + 逐种子 slot2==(r<P) 一致性
//
// 确定性纪律：掷点发生在 SKILL_EFFECT 时点（伤害浮动抽取之前）→ 掷点 = 第 1 抽；
// MINSTD 闭式钉种子 seed ≡ 目标态×16807^-1 (mod 2^31-1)。审计槽 1004100000 段。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>
#include <utils/sbs_rand.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kXingguang = 13292;  // 星光奔腾击：纯链 [410] [15,4]，物理 150
constexpr int kXuehua = 32389;     // 雪花治愈：纯链 [410] [30,3]，特殊 130
constexpr int kGuangmang = 17603;  // 闪耀光束波：[410, 93] [50,2, 50,200]（93 已实现）
constexpr int kZero = 10938;       // 本能一击（power 0）
constexpr int kAttr = 22638;       // 虚空之遗（属性技，无伤害）

constexpr int kAuditBase = 1004100000;
int aint(BattleContext& ctx, int owner, int slot, int def = 0) {
    const auto it = ctx.plugin_storage.find(kAuditBase + owner * 16 + slot);
    if (it == ctx.plugin_storage.end()) {
        return def;
    }
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : def;
}

ElfPet wei(std::array<int, 5> skills, int speed = 200, int atk = 100, int hp = 1000) {
    ElfPet p = scenario::make_pet(0, "回血方", skills, speed, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe(std::array<int, 5> skills, int speed = 100, int atk = 10, int hp = 1000) {
    ElfPet p = scenario::make_pet(1, "对手", skills, speed, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// 标准局骨架：我方 speed 200 先手出 410 载体，对手属性技空转（我方零受击）。
void run_cast(ScenarioBattle& b, int skill_index, int seed) {
    sbs_srand(static_cast<unsigned>(seed));
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, skill_index);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== T1 P=15 拒绝侧（r=15，钉种子 1787802177）==========
    {
        ScenarioBattle b(scenario::make_party(wei({kXingguang, kZero, kZero, kZero, kZero})),
                        scenario::make_party(foe({kAttr, kZero, kZero, kZero, kZero})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 500;   // 压半血：max_hp=1000，缺口 500 > 名义 250 → 不钳制
        run_cast(b, 0, 1787802177);
        CHECK_EQ(aint(ctx, 0, 0, -1), 15, "T1 申报 P=15");
        CHECK_EQ(aint(ctx, 0, 1, -1), 4, "T1 申报 denom=4");
        CHECK_EQ(aint(ctx, 0, 3), 250, "T1 名义=max_hp/4=250");
        CHECK_EQ(aint(ctx, 0, 6, -1), 15, "T1 原始掷点 r=15（第 1 抽，SKILL_EFFECT 时点）");
        CHECK_EQ(aint(ctx, 0, 2, -1), 0, "T1 掷点结果=拒（15<15 不成立）");
        CHECK_EQ(aint(ctx, 0, 4), 0, "T1 实际账置 0（不遗留）");
        CHECK_EQ(aint(ctx, 0, 5), 0, "T1 过闸次数=0");
        CHECK_EQ(ctx.getPet(0).hp, 500, "T1 hp 不变（对手属性技零伤）");
        std::cout << "  [T1] P=15 拒绝侧 通过\n";
    }

    //========== T2 P=15 通过侧（r=14，钉种子 380125177）==========
    {
        ScenarioBattle b(scenario::make_party(wei({kXingguang, kZero, kZero, kZero, kZero})),
                        scenario::make_party(foe({kAttr, kZero, kZero, kZero, kZero})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 500;
        run_cast(b, 0, 380125177);
        CHECK_EQ(aint(ctx, 0, 6, -1), 14, "T2 原始掷点 r=14");
        CHECK_EQ(aint(ctx, 0, 2, -1), 1, "T2 掷点结果=过");
        CHECK_EQ(aint(ctx, 0, 4), 250, "T2 实际=max_hp/4=250（整除精确）");
        CHECK_EQ(aint(ctx, 0, 5), 1, "T2 过闸次数=1");
        CHECK_EQ(ctx.getPet(0).hp, 750, "T2 hp=500+250（我方攻击打对手，自身零伤）");
        std::cout << "  [T2] P=15 通过侧 通过\n";
    }

    //========== T3 [30,3] 雪花治愈边界对（29 过 / 30 拒）==========
    for (int cs = 0; cs < 2; ++cs) {
        const int seed = (cs == 0) ? 20443707 : 1428120707;   // r=29 过 / r=30 拒
        const bool expect_pass = (cs == 0);
        ScenarioBattle b(scenario::make_party(wei({kXuehua, kZero, kZero, kZero, kZero})),
                        scenario::make_party(foe({kAttr, kZero, kZero, kZero, kZero})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 500;
        run_cast(b, 0, seed);
        CHECK_EQ(aint(ctx, 0, 6, -1), expect_pass ? 29 : 30, "T3 原始掷点吻合钉种子");
        CHECK_EQ(aint(ctx, 0, 2, -1), expect_pass ? 1 : 0, "T3 掷点结果（30<30 不成立=拒）");
        if (expect_pass) {
            CHECK_EQ(aint(ctx, 0, 4), 333, "T3 实际=max_hp/3=333（整除截断精确）");
            CHECK_EQ(ctx.getPet(0).hp, 833, "T3 hp=500+333");
        } else {
            CHECK_EQ(aint(ctx, 0, 4), 0, "T3 拒绝侧零治疗");
        }
    }
    std::cout << "  [T3] [30,3] 边界对 通过\n";

    //========== T4 [50,2] 17603 混链装配（410+93，93 已实现）==========
    {
        ScenarioBattle b(scenario::make_party(wei({kGuangmang, kZero, kZero, kZero, kZero})),
                        scenario::make_party(foe({kAttr, kZero, kZero, kZero, kZero})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 500;   // 500+500=1000=max：恰好回满且不越钳制线
        run_cast(b, 0, 256696296);   // 第 1 抽 r=49 < 50 过
        CHECK_EQ(aint(ctx, 0, 0, -1), 50, "T4 混链 410 切片申报 P=50");
        CHECK_EQ(aint(ctx, 0, 1, -1), 2, "T4 混链 410 切片 denom=2（93 的参不串位）");
        CHECK_EQ(aint(ctx, 0, 6, -1), 49, "T4 原始掷点 r=49");
        CHECK_EQ(aint(ctx, 0, 4), 500, "T4 实际=max_hp/2=500");
        CHECK_EQ(ctx.getPet(0).hp, 1000, "T4 hp=500+500（恰好回满；93 段行为不在本单断言域）");
        std::cout << "  [T4] [50,2] 混链装配 通过\n";
    }

    //========== T5 确定性（同种子两次对局审计全等）==========
    {
        std::tuple<int, int, int, int, int> first;
        for (int rep = 0; rep < 2; ++rep) {
            ScenarioBattle b(scenario::make_party(wei({kXingguang, kZero, kZero, kZero, kZero})),
                            scenario::make_party(foe({kAttr, kZero, kZero, kZero, kZero})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 500;
            run_cast(b, 0, 380125177);
            auto cur = std::make_tuple(aint(ctx, 0, 6, -1), aint(ctx, 0, 2, -1), aint(ctx, 0, 4),
                                       aint(ctx, 0, 5), ctx.getPet(0).hp);
            if (rep == 0) {
                first = cur;
            } else {
                CHECK(first == cur, "T5 同种子两次对局审计链逐项全等");
            }
        }
        std::cout << "  [T5] 确定性 通过\n";
    }

    //========== T6 减疗：heal_mod_pct=-50 → 实际=名义×50% ==========
    {
        ScenarioBattle b(scenario::make_party(wei({kXingguang, kZero, kZero, kZero, kZero})),
                        scenario::make_party(foe({kAttr, kZero, kZero, kZero, kZero})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 500;
        ctx.heal_mod_pct[0] = -50;
        run_cast(b, 0, 380125177);
        CHECK_EQ(aint(ctx, 0, 4), 125, "T6 实际=250×(100-50)%=125（heal_amount 修正通道）");
        CHECK_EQ(ctx.getPet(0).hp, 625, "T6 hp=500+125");
        std::cout << "  [T6] 减疗 通过\n";
    }

    //========== T7 封疗：HEAL_BLOCK → 实际=0 ==========
    {
        ScenarioBattle b(scenario::make_party(wei({kXingguang, kZero, kZero, kZero, kZero})),
                        scenario::make_party(foe({kAttr, kZero, kZero, kZero, kZero})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 500;
        ctx.grant_immunity(0, ImmunityType::HEAL_BLOCK, coverage_all(), 0, 0, 8001);
        run_cast(b, 0, 380125177);
        CHECK_EQ(aint(ctx, 0, 2, -1), 1, "T7 掷点照常过闸");
        CHECK_EQ(aint(ctx, 0, 4), 0, "T7 实际=0（封回血封锁 heal_amount）");
        CHECK_EQ(ctx.getPet(0).hp, 500, "T7 hp 不变");
        std::cout << "  [T7] 封疗 通过\n";
    }

    //========== T8 双方镜像（审计键 owner×16 隔离）==========
    {
        ScenarioBattle b(scenario::make_party(wei({kXingguang, kZero, kZero, kZero, kZero})),
                        scenario::make_party(foe({kXingguang, kZero, kZero, kZero, kZero},
                                                 /*speed=*/100, /*atk=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 500;
        ctx.getPet(1).hp = 500;
        // R0 双方各用星光奔腾击（都掷 410）：我 roll=第 1 抽（53，拒）→ 我 float 第 2 抽；
        // 对手 roll=第 3 抽（93，拒）→ 对手 float 第 4 抽（424242 链 53/17/93/70）
        sbs_srand(424242);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(aint(ctx, 0, 6, -1), 53, "T8 己方掷点=第 1 抽（53，拒）");
        CHECK_EQ(aint(ctx, 1, 6, -1), 93, "T8 对方掷点=第 3 抽（93，拒——独立键段独立抽取）");
        CHECK_EQ(aint(ctx, 0, 4), 0, "T8 己方零回血");
        CHECK_EQ(aint(ctx, 1, 4), 0, "T8 对方零回血");
        CHECK(ctx.getPet(0).hp < 500, "T8 己方挨了对手 150 威力实伤");
        CHECK(ctx.getPet(1).hp < 500, "T8 对方挨了我方 150 威力实伤");
        std::cout << "  [T8] 双方镜像隔离 通过\n";
    }

    //========== T9 固定 20 种子扫描：过闸率侧证 + 逐种子一致性 ==========
    {
        int passes = 0;
        for (int s = 0; s < 20; ++s) {
            ScenarioBattle b(scenario::make_party(wei({kXingguang, kZero, kZero, kZero, kZero})),
                            scenario::make_party(foe({kAttr, kZero, kZero, kZero, kZero})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 500;
            run_cast(b, 0, 3000 + s * 7919);
            const int r = aint(ctx, 0, 6, -1);
            const int res = aint(ctx, 0, 2, -1);
            CHECK(r >= 0 && r < 100, "T9 抽取在域内");
            CHECK_EQ(res, (r < 15) ? 1 : 0, "T9 逐种子 slot2==(r<P) 一致");
            if (res == 1) {
                ++passes;
                CHECK_EQ(aint(ctx, 0, 4), 250, "T9 过闸种子实际=250");
            } else {
                CHECK_EQ(aint(ctx, 0, 4), 0, "T9 拒绝种子实际=0");
            }
        }
        CHECK(passes >= 1 && passes <= 8,
              "T9 固定 20 种子过闸计数在 15% 侧证带内（实测确定值，非随机断言）");
        std::cout << "  [T9] 种子扫描通过（过闸 " << passes << "/20）\n";
    }

    std::cout << "scenario_8030 all passed" << std::endl;
    return 0;
}
