// 场景 037：pet 两个私有槽的生命周期 + 落芳天华 497「固伤递增 / 天敌翻倍」。
//
// 两个槽（include/entities/elf-pet.h）：
//   soulmark_storage  —— **下场保留**（跨回合 + 跨切换）：无相谛 2260 蓄力这类**故意**要跨场累计的；
//   on_stage_storage  —— **本次上场**（换宠由 perform_switch 清空）：大多数"每次使用递增"计数走这里。
// 官方实测（用户 2026-09-13 游戏内确认）：落芳天华"每次使用额外附加"**下场不保留**。
//
// 天敌（官方实测口径，见 docs/02-效果系统/官方机制理解与引擎缺口对照.md §五）：
//   **对手的固有属性克制自身固有属性（倍数 > 1）**——不看技能属性、不看属性视图
//   （用龙琰改系别不影响翻倍）。
//
// 验证点：
//   P1 递增：落芳天华连续使用 → 固伤 100 / 200 / 300（走 on_stage_storage 计数）
//   P2 封顶：第 4、5 次 → 400（"最高400点"）
//   P3 下场重置：切走再切回 → 计数归零（on_stage_storage 被 perform_switch 清）
//   P4 天敌翻倍：对手固有属性克制我方固有属性 → 首次即 200（100×2）
//   P5 槽分工：on_stage_storage 被清的同时，soulmark_storage **不受影响**（下场保留槽）
#include <cassert>
#include <iostream>

#include <effects/effect.h>
#include <entities/pet_factory.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

constexpr int kLuoFang = 17912;     // 落芳天华：power 80 + effect 85(转化强化) + effect 497(固伤递增)
constexpr int kZhuangJi = 10001;
// 元素 id（skill_types）：草=1 水=2 电=5 地面=7 普通=8。草克水 → 水遇到的天敌是草。
constexpr int kGrass = 1;
constexpr int kWater = 2;

// 我方：元素可控（默认普通），技能 0 = 落芳天华。
ElfPet make_puni_like(int element) {
    ElfPet pet = scenario::make_pet(5000, "落芳方",
                                    {kLuoFang, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                    /*speed=*/300, /*atk=*/100, /*hp=*/3000);
    pet.elementalAttributes[0] = element;
    pet.elementalAttributes[1] = 0;
    pet.skills[0].must_hit = true;
    return pet;
}

// 对手：只挨打，不还手（属性技能）——用 20017 高速移动（power 0）避免红伤干扰测量。
ElfPet make_dummy(int element) {
    ElfPet pet = scenario::make_pet(1, "沙包",
                                    {20017, 20017, 20017, 20017, 20017},
                                    /*speed=*/100, /*atk=*/100, /*hp=*/5000);
    pet.elementalAttributes[0] = element;
    pet.elementalAttributes[1] = 0;
    return pet;
}

// 直接调 effect 497（走 EffectFactory 取插件里注册的实现），返回对手掉血。
// 为什么不"打一回合看差值"：落芳天华本体是 power 80 的攻击技能，红伤带 (217+rand%39)/255
// 的随机浮动（±15%），差值里混进随机量就测不准固伤的**精确**增量。
// 直接调用只产生固定伤害（走 deal_damage FIXED：固定抗性/护罩，默认 0 → 精确）。
int call_497(BattleContext& ctx, int owner, int target) {
    Effect e = EffectFactory::getInstance().getEffect(
        497, EffectArgs(std::vector<int>{owner, target, /*base=*/100, /*step=*/100, /*cap=*/400}));
    CHECK(e.logic != nullptr, "effect 497 应已在 moves_lib 注册");
    const int before = ctx.getPet(target).hp;
    e.logic(&ctx, e.args);
    return before - ctx.getPet(target).hp;
}

// 打一回合（真实走技能），返回对手掉血。
int strike(scenario::ScenarioBattle& b) {
    const int before = b.ctx().getPet(1).hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    return before - b.ctx().getPet(1).hp;
}

// 我方切走再切回（下场 → on_stage_storage 被清）。
void switch_out_and_back(scenario::ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== P1/P2 递增 + 封顶 ==========
    {
        scenario::ScenarioBattle b(scenario::make_party(make_puni_like(8)),
                                   scenario::make_party(make_dummy(8)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int d1 = call_497(ctx, 0, 1);
        const int d2 = call_497(ctx, 0, 1);
        const int d3 = call_497(ctx, 0, 1);
        const int d4 = call_497(ctx, 0, 1);
        const int d5 = call_497(ctx, 0, 1);
        CHECK_EQ(d1, 100, "P1) 第1次：附加 100 点固定伤害");
        CHECK_EQ(d2, 200, "P1) 第2次：额外 +100 → 200");
        CHECK_EQ(d3, 300, "P1) 第3次：→ 300");
        CHECK_EQ(d4, 400, "P2) 第4次：→ 400（正好到顶）");
        CHECK_EQ(d5, 400, "P2) 第5次：封顶 400（不再增加）");
        std::cout << "  [P1/P2] 递增→封顶: " << d1 << "/" << d2 << "/" << d3 << "/"
                  << d4 << "/" << d5 << "（100/200/300/400/400）\n";
    }

    //========== P3 下场重置（on_stage_storage 被 perform_switch 清空）==========
    {
        scenario::ScenarioBattle b(scenario::make_party(make_puni_like(8)),
                                   scenario::make_party(make_dummy(8)));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK_EQ(call_497(ctx, 0, 1), 100, "P3) 前置：首次 = 100");
        CHECK_EQ(call_497(ctx, 0, 1), 200, "P3) 前置：第2次 = 200（计数已在累计）");
        switch_out_and_back(b);  // 真实下场 + 再登场
        CHECK_EQ(call_497(ctx, 0, 1), 100, "P3) 下场重置 → 计数归零（回到 100 档）");
        std::cout << "  [P3] 下场重置: 200 → 切走再切回 → 100\n";
    }

    //========== P4 天敌翻倍（对手固有属性克制我方固有属性）==========
    {
        // 我方水(2)、对手草(1)：草克水 → 对手是我的天敌 → 效果翻倍
        scenario::ScenarioBattle bp(scenario::make_party(make_puni_like(kWater)),
                                    scenario::make_party(make_dummy(kGrass)));
        bp.start();
        const int d_predator = call_497(bp.ctx(), 0, 1);
        CHECK_EQ(d_predator, 200, "P4) 遇天敌 → 固伤翻倍（100 → 200）");

        // 对照：双方同为普通(8) → 无克制关系 → 不翻倍
        scenario::ScenarioBattle bc(scenario::make_party(make_puni_like(8)),
                                    scenario::make_party(make_dummy(8)));
        bc.start();
        const int d_plain = call_497(bc.ctx(), 0, 1);
        CHECK_EQ(d_plain, 100, "P4) 对照：非天敌不翻倍");
        std::cout << "  [P4] 天敌翻倍: 非天敌=" << d_plain << " 天敌=" << d_predator << "\n";
    }

    //========== P5 两槽分工：换宠只清 on_stage_storage，soulmark_storage 保留 ==========
    {
        auto p0 = make_puni_like(8);
        scenario::ScenarioBattle b(scenario::make_party(p0),
                                   scenario::make_party(make_dummy(8)));
        b.start();
        BattleContext& ctx = b.ctx();
        // 人为往两个槽里各放一个计数
        ctx.getPet(0).on_stage_storage[777] = 5;
        ctx.getPet(0).soulmark_storage[777] = 9;
        switch_out_and_back(b);
        ElfPet& old = ctx.seerRobot[0].elfPets[0];  // 换下去的那只（槽位 0）
        CHECK_EQ(old.on_stage_storage.count(777), 0, "P5) 下场 → on_stage_storage 被清空");
        CHECK_EQ(old.soulmark_storage.count(777), 1, "P5) 下场保留槽不受影响（soulmark_storage 留着）");
        std::cout << "  [P5] 槽分工: 下场后 on_stage 空=" << (old.on_stage_storage.count(777) == 0)
                  << "、soulmark 仍在=" << (old.soulmark_storage.count(777) == 1) << "\n";
    }

    //========== P6 端到端：落芳天华(17912) 真的带上了 effect 497 ==========
    {
        scenario::ScenarioBattle b(scenario::make_party(make_puni_like(8)),
                                   scenario::make_party(make_dummy(8)));
        b.start();
        const int dmg = strike(b);
        // 红伤(power 80) + 固伤(≥100) → 总伤必然 ≥ 100（固伤本身）
        CHECK(dmg >= 100, "P6) 落芳天华一回合的伤害应包含 effect 497 的固伤（≥100）");
        std::cout << "  [P6] 端到端: 落芳天华单回合总伤 = " << dmg << "（红伤 + 固伤≥100）\n";
    }

    std::cout << "\n场景 037 通过：固伤递增/封顶/下场重置 + 天敌翻倍；"
                 "两个 pet 私有槽按生命周期分工（下场保留 vs 本次上场）。\n";
    return 0;
}
