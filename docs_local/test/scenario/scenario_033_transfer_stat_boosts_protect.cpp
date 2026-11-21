// 场景 033：转换强化原语 `transfer_stat_boosts` + 新免疫类型「免消除强化」`STAT_CLEAR`。
//
// 转换强化（官方 effect 85："使对手的能力提升效果转化到自己身上"，落芳天华 17912）与
// 吸取（effect 1287："吸取对手能力提升"，烬灭神咒剑）是**同一动作**：对手正等级清零、
// 自己等量拿到 → 共用一个原语（1287 只是成功后额外给"下N次受击减伤"）。
//
// 免消除强化（官方 effect 1960："令自身{0}回合内的能力提升状态无法被消除或吸取"，
// 希拓·神煌炎舞斩 37222）：**三个入口都要查**，否则保护会被绕道——
//   ① 消除  clear_stat_boosts
//   ② 转化  transfer_stat_boosts（effect 85）
//   ③ 吸取  transfer_stat_boosts（effect 1287 走同一原语）
//
// 验证点：
//   T1 无保护：转化成功——对手 +2/+1 清零，自己等量拿到；**ws 视图同步**
//   T2 上限：自己已有 +5，转化对手 +2 → 封顶 +6（不是 +7）
//   T3 保护生效：对手有 STAT_CLEAR → ①消除 ②转化 **双双失败**，双方等级原样不动
//   T4 保护只护"提升"：对手的负等级不受影响（免消除强化不是免弱）
//   T5 回合类生命周期：保护过期后转化恢复正常
#include <cassert>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

constexpr int kAtk = static_cast<int>(NumericalPropertyIndex::PHYSICAL_ATTACK);
constexpr int kSpAtk = static_cast<int>(NumericalPropertyIndex::SPECIAL_ATTACK);

void grant_stat_clear(BattleContext& ctx, int owner, int rounds) {
    ctx.grant_immunity(owner, ImmunityType::STAT_CLEAR, coverage_all(),
                       /*anomaly_mask=*/0, /*duration_rounds=*/rounds, /*source_id=*/1960,
                       /*soul_immunity=*/false, EffectScope::ON_STAGE,
                       /*counts=*/0, /*source_effect_id=*/1960);
}

// 双方都只带"撞击"，本场景不使用技能效果，直接驱动原语。
scenario::ScenarioBattle make_battle() {
    auto p0 = scenario::make_pet(0, "我方", {10001, 10001, 10001, 10001, 10001}, 100, 100, 800);
    auto p1 = scenario::make_pet(1, "对手", {10001, 10001, 10001, 10001, 10001}, 100, 100, 800);
    return scenario::ScenarioBattle(scenario::make_party(p0), scenario::make_party(p1));
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== T1 无保护：转化成功 + 视图同步 ==========
    {
        scenario::ScenarioBattle b = make_battle();
        BattleContext& ctx = b.ctx();
        ctx.getPet(1).levels[kAtk] = 2;
        ctx.getPet(1).levels[kSpAtk] = 1;
        ctx.ws.view_levels[1][kAtk] = 2;  // 模拟真实路径：本体/视图同源
        ctx.ws.view_levels[1][kSpAtk] = 1;

        const int moved = transfer_stat_boosts(&ctx, /*from=*/1, /*to=*/0);
        CHECK_EQ(moved, 2, "T1) 应转化 2 项提升（攻击+特攻）");
        CHECK_EQ(ctx.getPet(1).levels[kAtk], 0, "T1) 对手攻击提升应被清零");
        CHECK_EQ(ctx.getPet(0).levels[kAtk], 2, "T1) 我方应拿到 +2 攻击");
        CHECK_EQ(ctx.getPet(0).levels[kSpAtk], 1, "T1) 我方应拿到 +1 特攻");
        // 视图同步（伤害公式读的就是它；不同步会让转化在伤害上"没发生"）
        CHECK_EQ(ctx.ws.view_levels[1][kAtk], 0, "T1) 对手攻击视图应同步为 0");
        CHECK_EQ(ctx.ws.view_levels[0][kAtk], 2, "T1) 我方攻击视图应同步为 +2");
        std::cout << "  [T1] 转化: 对手 攻+2/特攻+1 → 0/0；我方 → +2/+1（视图同步）\n";
    }

    //========== T2 上限 +6 ==========
    {
        scenario::ScenarioBattle b = make_battle();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).levels[kAtk] = 5;
        ctx.ws.view_levels[0][kAtk] = 5;
        ctx.getPet(1).levels[kAtk] = 2;
        ctx.ws.view_levels[1][kAtk] = 2;

        transfer_stat_boosts(&ctx, /*from=*/1, /*to=*/0);
        CHECK_EQ(ctx.getPet(0).levels[kAtk], 6, "T2) 转化后应封顶 +6（不是 +7）");
        CHECK_EQ(ctx.ws.view_levels[0][kAtk], 6, "T2) 视图同样封顶 +6");
        std::cout << "  [T2] 上限: 自身 +5 转化对手 +2 → " << ctx.getPet(0).levels[kAtk] << "\n";
    }

    //========== T3 免消除强化：消除 / 转化 双双失败 ==========
    {
        scenario::ScenarioBattle b = make_battle();
        BattleContext& ctx = b.ctx();
        ctx.getPet(1).levels[kAtk] = 2;
        ctx.ws.view_levels[1][kAtk] = 2;
        grant_stat_clear(ctx, /*owner=*/1, /*rounds=*/3);

        const int cleared = clear_stat_boosts(&ctx, 1);
        CHECK_EQ(cleared, 0, "T3) ①消除：受保护时消强应失败（返回 0，不触发'消强成功'分支）");
        CHECK_EQ(ctx.getPet(1).levels[kAtk], 2, "T3) ①消除：对手提升应原样保留");

        const int moved = transfer_stat_boosts(&ctx, /*from=*/1, /*to=*/0);
        CHECK_EQ(moved, 0, "T3) ②转化：受保护时应失败");
        CHECK_EQ(ctx.getPet(1).levels[kAtk], 2, "T3) ②转化：对手提升应原样保留");
        CHECK_EQ(ctx.getPet(0).levels[kAtk], 0, "T3) ②转化：我方不应拿到任何提升");
        std::cout << "  [T3] 免消除强化(STAT_CLEAR): 消除=0 转化=0，对手 +2 原样保留\n";
    }

    //========== T4 保护只护"提升"：负等级照常可被吸取 ==========
    {
        scenario::ScenarioBattle b = make_battle();
        BattleContext& ctx = b.ctx();
        ctx.getPet(1).levels[kAtk] = -2;   // 弱化
        ctx.getPet(1).levels[kSpAtk] = 3;  // 提升
        ctx.ws.view_levels[1][kAtk] = -2;
        ctx.ws.view_levels[1][kSpAtk] = 3;
        grant_stat_clear(ctx, /*owner=*/1, /*rounds=*/3);

        const int moved = transfer_stat_boosts(&ctx, /*from=*/1, /*to=*/0);
        CHECK_EQ(moved, 0, "T4) 整体仍被保护挡下");
        CHECK_EQ(ctx.getPet(1).levels[kAtk], -2, "T4) 弱化不受影响（免消除强化 ≠ 免弱）");
        CHECK_EQ(ctx.getPet(1).levels[kSpAtk], 3, "T4) 提升被保护保留");
        std::cout << "  [T4] 保护仅覆盖提升：对手 攻-2 保留 / 特攻+3 保留\n";
    }

    //========== T5 窗口过期后保护解除 ==========
    {
        scenario::ScenarioBattle b = make_battle();
        BattleContext& ctx = b.ctx();
        ctx.getPet(1).levels[kAtk] = 2;
        ctx.ws.view_levels[1][kAtk] = 2;
        grant_stat_clear(ctx, /*owner=*/1, /*rounds=*/2);
        CHECK_EQ(transfer_stat_boosts(&ctx, 1, 0), 0, "T5) 窗口内被保护");

        ctx.roundCount += 2;  // 走完 2 回合窗口
        CHECK_EQ(transfer_stat_boosts(&ctx, 1, 0), 1, "T5) 窗口过期后转化恢复正常");
        CHECK_EQ(ctx.getPet(0).levels[kAtk], 2, "T5) 过期后我方拿到 +2");
        std::cout << "  [T5] 窗口(2回合)过期 → 保护解除，转化恢复\n";
    }

    //========== T6 落芳天华(17912, effect 85) 端到端：技能→原语 ==========
    {
        auto atk = scenario::make_pet(0, "落芳", {17912, 10001, 10001, 10001, 10001},
                                      /*speed=*/300, /*atk=*/100, /*hp=*/800);
        atk.skills[0].must_hit = true;
        auto def = scenario::make_pet(1, "对手", {10001, 10001, 10001, 10001, 10001},
                                      /*speed=*/100, /*atk=*/100, /*hp=*/800);
        def.levels[kAtk] = 2;  // 对手处于能力提升 → 落芳天华把提升转化给我方
        scenario::ScenarioBattle b(scenario::make_party(atk), scenario::make_party(def));
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        BattleContext& ctx = b.ctx();
        CHECK_EQ(ctx.getPet(1).levels[kAtk], 0, "T6) 对手攻击提升应被转走");
        CHECK(ctx.getPet(0).levels[kAtk] >= 2, "T6) 我方应拿到对手的提升（+2）");
        std::cout << "  [T6] 落芳天华(85): 对手 攻+2 → 0，我方 攻 → +"
                  << ctx.getPet(0).levels[kAtk] << "\n";
    }

    //========== T7 吸取(effect 1287, 烬灭神咒剑) 同样被保护挡下 ==========
    {
        auto atk = scenario::make_pet(0, "希拓", {37218, 10001, 10001, 10001, 10001},
                                      /*speed=*/300, /*atk=*/100, /*hp=*/800);
        atk.skills[0].must_hit = true;
        auto def = scenario::make_pet(1, "对手", {10001, 10001, 10001, 10001, 10001},
                                      /*speed=*/100, /*atk=*/100, /*hp=*/800);
        def.levels[kAtk] = 2;
        scenario::ScenarioBattle b(scenario::make_party(atk), scenario::make_party(def));
        b.start();
        grant_stat_clear(b.ctx(), /*owner=*/1, /*rounds=*/3);  // 对手：强化无法被消除或吸取
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        BattleContext& ctx = b.ctx();
        CHECK_EQ(ctx.getPet(1).levels[kAtk], 2, "T7) ③吸取：受保护时对手提升应原样保留");
        CHECK(ctx.getPet(0).levels[kAtk] < 2, "T7) ③吸取：我方不应拿到对手的提升");
        std::cout << "  [T7] 烬灭神咒剑(1287 吸取): 对手 +2 保留（受保护，吸取失败）\n";
    }

    std::cout << "\n场景 033 通过：转换强化原语（消除/转化/吸取同一通道）+ 免消除强化"
                 "（三个入口都查、只护提升、窗口过期即解除）。\n";
    return 0;
}
