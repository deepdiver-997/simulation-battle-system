// 场景 168：薇尔诗 4000 技能组缺口（四）—— 白皑之纷争 29371 效果组
//（moves_lib/batch_c_skills.cpp，2026-09-27）
//
// 覆盖效果（白皑之纷争 = [2489跳, 697跳, 2494, 2495, 2497, 1440已有]，属性技、必中）：
//   2494 上回合受到过攻击伤害 → 附加 170 点冰系伤害（攻击值每 260 点翻倍，封顶 1 次）
//   2495 上回合未受到攻击伤害 → 获得最大体力 26% 的护盾、护罩
//   2497 解除自身所处的异常状态（后半"上次登场免疫"子句待魂印侧，见实现注释）
//
// 确定性纪律：属性伤害 = 点数 × 克制倍数（无随机）→ rider 断言取 170/340/680 三档。
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

constexpr int kBaiai = 29371;    // 白皑之纷争：2494+2495+2497+1440（属性技、必中）
constexpr int kXukong = 22638;   // 虚空之遗（属性技：不受击对照用）
constexpr int kHit = 10004;      // 拍打（物攻 40，真实扣血）
constexpr int kHarmless = 10938; // 本能一击（power 0 → 0 伤，不记"受到攻击伤害"）

ElfPet wei(std::array<int, 5> skills, int atk = 1000) {
    ElfPet p = scenario::make_pet(0, "薇尔诗", skills, /*speed=*/200, atk, /*hp=*/100000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe(std::array<int, 5> skills, int atk = 10) {
    ElfPet p = scenario::make_pet(1, "对手", skills, /*speed=*/100, atk, /*hp=*/100000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== T1 2495：未受击 → 最大体力 26% 护盾+护罩 ==========
    {
        ScenarioBattle b(scenario::make_party(wei({kBaiai, kHarmless, kHarmless, kHarmless,
                                                   kHarmless})),
                        scenario::make_party(foe({kXukong, kXukong, kXukong, kXukong,
                                                  kXukong})));
        b.start();
        BattleContext& ctx = b.ctx();
        // R1：我白皑之纷争（无上回合 → 未受击支路）；对手属性技（真·不受击）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(0).shield_bank_.quantity_of(2495), 26000,
                 "T1 核心：未受击 → 护盾 = 100000×26% = 26000");
        CHECK_EQ(ctx.getPet(0).hood_bank_.quantity_of(2495), 26000,
                 "T1 核心：未受击 → 护罩 = 26000");
        std::cout << "  [T1] 2495 未受击双 bank 通过\n";
    }

    //========== T2 2494：上回合受击 → 附加 170 点冰系伤害（×2 翻倍档）+ 2495 不触发 ==========
    {
        // 时序：记账 watcher 随白皑之纷争首次使用武装 → R0 先施白皑（武装记账 + 2495 授
        // 26000 盾罩）；对手拍打（atk=100000，伤害恒 >26000）击破盾并真实扣血 → 记 R0 受击；
        // R1 再施白皑 → rider：170 × 2^min(1000/260, 1) = 340 点（属性伤无随机因子、
        // 元素 9 vs 无属性 0 = 矩阵默认 1.0）；2495 因"上回合受击"不再授予。
        ScenarioBattle b(scenario::make_party(wei({kBaiai, kHarmless, kHarmless, kHarmless,
                                                   kHarmless})),
                        scenario::make_party(foe({kHit, kHit, kHit, kHit,
                                                  kHit}, /*atk=*/100000)));
        b.start();
        BattleContext& ctx = b.ctx();
        // R0：我白皑之纷争（武装记账 + 2495 授盾）；对手拍打破盾 + 真实扣血
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(0).shield_bank_.quantity_of(2495), 0,
                 "T2 前置：R0 授的 26000 盾已被拍打击破（护盾只响应红伤）");
        CHECK(ctx.getPet(0).hp < 100000, "T2 前置：R0 受到过攻击伤害（真实扣血）");
        const int foe_hp_r1 = ctx.getPet(1).hp;
        // R1：我再白皑之纷争 → 上回合(0)受击 → rider 340；2495 不触发
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        const int loss = foe_hp_r1 - ctx.getPet(1).hp;
        CHECK(loss == 340,
              "T2 核心：rider 170 点 × 2 翻倍档 = 340（属性伤无随机；克制 1.0，实测 "
                  << loss << "）");
        CHECK_EQ(ctx.getPet(0).shield_bank_.quantity_of(2495), 0,
                 "T2 核心：上回合受击 → 2495 不授予护盾");
        std::cout << "  [T2] 2494 rider + 2495 受击支路 通过（loss=" << loss << "）\n";
    }

    //========== T3 2497：解除自身所处的异常状态 ==========
    {
        ScenarioBattle b(scenario::make_party(wei({kBaiai, kHarmless, kHarmless, kHarmless,
                                                   kHarmless})),
                        scenario::make_party(foe({kHarmless, kHarmless, kHarmless, kHarmless,
                                                  kHarmless})));
        b.start();
        BattleContext& ctx = b.ctx();
        // 直调原语给自身挂烧伤（无免疫 → 必落地）
        const ApplyAnomalyResult r = apply_anomaly(
            &ctx, 0, static_cast<int>(AbnormalStatusId::Burn), 3, /*actor=*/1, 100,
            ChanceSource::Skill);
        ctx.event_center_.drain(&ctx, ctx.roundCount);
        CHECK(r == ApplyAnomalyResult::SUCCESS, "T3 前置：烧伤已挂");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(!ctx.has_active_abnormal_status(0, 2), "T3 核心：自身异常被解除");
        std::cout << "  [T3] 2497 解除自身异常 通过\n";
    }

    std::cout << "scenario_168 all passed" << std::endl;
    return 0;
}
