// 场景 6250 —— J292 双防回合弱化/先手自增族（150 / 588，moves_lib/j_stat_window_priority.cpp）
//（2026-10-22，worktree feat/jbatch；号段 6250-6259）
//
// 断言口径：
//   - 能力等级一律读**本体** `ctx.ability_levels[side][槽]`（stat_drop/stat_change 同步本体；
//     逐槽快照，不看"等级和"）。
//   - 150（21404 魔界之种 [3,-1] 属性纯链）：当回合立即 -1，后续每回合末再 -1，
//     3 回合恰 3 次（R1/R2/R3），R4 窗外保持 -3；首/后手两个站位同断言。
//   - 588（16114 战天灭魂击 [100,0,1] / 16395 狂龙破 [100,4,1] 纯链）：先出手 + 100%
//     → 指定槽 +1；后手站位 → 不触发（acting_side_is_first 门）。
//   - 审计槽 991500000/991500100（150 次数/档位）、995880000/995880100（588 先手/掷骰）。
#include <any>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;    // 拍打：物 40（填充刀）
constexpr int k150Skill = 21404; // 魔界之种 [150]|[3,-1] 属性技
constexpr int k588Atk = 16114;   // 战天灭魂击 [588]|[100,0,1]（攻击槽 +1）
constexpr int k588Spd = 16395;   // 狂龙破 [588]|[100,4,1]（速度槽 +1）

constexpr int k150Apply = 991500000;
constexpr int k150Delta = 991500100;
constexpr int k588First = 995880000;
constexpr int k588Roll = 995880100;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_self(int skill, int speed, int hp = 99999) {
    ElfPet p = make_pet(1, "己方", {skill, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, speed,
                        /*atk=*/1, hp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe(int speed, int hp = 99999) {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, speed,
                        /*atk=*/1, hp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

int stor(const BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return 0;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : 0;
}

void run(ScenarioBattle& b, int skill0, BattleFsm::ActionType op1, int skill1) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, skill0);
    b.submit_operation(1, op1, skill1);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T6250 150 先手站位：R1 立即 -1，R2/R3 每回合末 -1，R4 窗外不变 ══
    {
        ScenarioBattle b(make_party(mk_self(k150Skill, /*speed=*/300)),
                         make_party(mk_foe(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        run(b, 0, BattleFsm::ActionType::NONE, -1);
        CHECK_EQ(ctx.ability_levels[1][2], -1, "T6250.a 150：R1 防御立即 -1");
        CHECK_EQ(ctx.ability_levels[1][3], -1, "T6250.a 150：R1 特防立即 -1");
        CHECK_EQ(stor(ctx, k150Apply), 1, "T6250.a 150：已结算 1 次");
        CHECK_EQ(stor(ctx, k150Delta), -1, "T6250.a 150：档位 -1");
        run(b, 1, BattleFsm::ActionType::NONE, -1);
        CHECK_EQ(ctx.ability_levels[1][2], -2, "T6250.a 150：R2 防御 -2");
        CHECK_EQ(ctx.ability_levels[1][3], -2, "T6250.a 150：R2 特防 -2");
        run(b, 1, BattleFsm::ActionType::NONE, -1);
        CHECK_EQ(ctx.ability_levels[1][2], -3, "T6250.a 150：R3 防御 -3（3 回合 3 次）");
        CHECK_EQ(ctx.ability_levels[1][3], -3, "T6250.a 150：R3 特防 -3");
        CHECK_EQ(stor(ctx, k150Apply), 3, "T6250.a 150：累计结算 3 次");
        run(b, 1, BattleFsm::ActionType::NONE, -1);
        CHECK_EQ(ctx.ability_levels[1][2], -3, "T6250.a 150：R4 窗外防御保持 -3");
        CHECK_EQ(ctx.ability_levels[1][3], -3, "T6250.a 150：R4 窗外特防保持 -3");
        CHECK_EQ(stor(ctx, k150Apply), 3, "T6250.a 150：窗外不再结算");
    }

    // ══ T6251 150 后手站位：当回合立即结算，窗口仍恰 3 次 ══
    {
        ScenarioBattle b(make_party(mk_self(k150Skill, /*speed=*/100)),
                         make_party(mk_foe(/*speed=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        run(b, 0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 对手先动（攻击）
        CHECK_EQ(ctx.ability_levels[1][2], -1, "T6251 150：后手 R1 仍立即 -1");
        CHECK_EQ(ctx.ability_levels[1][3], -1, "T6251 150：后手 R1 特防 -1");
        run(b, 1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.ability_levels[1][2], -2, "T6251 150：后手 R2 -2");
        run(b, 1, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(ctx.ability_levels[1][2], -3, "T6251 150：后手 R3 -3");
        CHECK_EQ(stor(ctx, k150Apply), 3, "T6251 150：后手累计恰 3 次");
    }

    // ══ T6252 588 先出手 + 100% → 自身攻击/速度槽 +1 ══
    {
        {   // (a) 攻击槽
            ScenarioBattle b(make_party(mk_self(k588Atk, /*speed=*/300)),
                             make_party(mk_foe(/*speed=*/100)));
            b.start();
            BattleContext& ctx = b.ctx();
            run(b, 0, BattleFsm::ActionType::NONE, -1);
            CHECK_EQ(stor(ctx, k588First), 1, "T6252.a 588：先手判定成立");
            CHECK_EQ(stor(ctx, k588Roll), 1, "T6252.a 588：100% 掷骰命中");
            CHECK_EQ(ctx.ability_levels[0][0], 1, "T6252.a 588：攻击槽 +1");
        }
        {   // (b) 速度槽（{1}=4 映射）
            ScenarioBattle b(make_party(mk_self(k588Spd, /*speed=*/300)),
                             make_party(mk_foe(/*speed=*/100)));
            b.start();
            BattleContext& ctx = b.ctx();
            run(b, 0, BattleFsm::ActionType::NONE, -1);
            CHECK_EQ(ctx.ability_levels[0][4], 1, "T6252.b 588：速度槽 +1");
            CHECK_EQ(ctx.ability_levels[0][0], 0, "T6252.b 588：攻击槽不受影响");
        }
    }

    // ══ T6253 588 后出手 → 门闭不触发 ══
    {
        ScenarioBattle b(make_party(mk_self(k588Atk, /*speed=*/100)),
                         make_party(mk_foe(/*speed=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        run(b, 0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 对手先动
        CHECK_EQ(stor(ctx, k588First), 0, "T6253 588 门：先手判定不成立");
        CHECK_EQ(stor(ctx, k588Roll), 0, "T6253 588 门：未掷骰/未生效");
        CHECK_EQ(ctx.ability_levels[0][0], 0, "T6253 588 门：攻击槽不变");
    }

    std::cout << "scenario_6250_stat_window_priority: ALL PASS\n";
    return 0;
}
