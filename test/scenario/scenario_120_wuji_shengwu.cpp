#include <utils/sbs_rand.h>
// 场景 120：无极圣武 4800 —— 英雄之耀计数 / 砥砺登场触发器 / 亮节 / 威怯（圣武批①）。
//
// 口径：effect_icon 2009 + 机制解析帖（详见开工文档 §二 / §六.4）。
// 验证点：
//   A 英耀计数：干净队友 +1、毒特性队友 -1、层数钳 0（魂印静态表路径随表扩充另测）
//   B 砥砺登场触发器：战斗开始圣武在背包 → 双方首发吃砥砺、圣武得亮节；
//     圣武在场 → 不触发；免疫挡下砥砺 → 威怯（失败分支）
//   C 威怯 -60%（乘法）：持威怯者红伤 = 对照 × 40/100（同种子隔离随机）
//   D 亮节：回合结束给对手 pp==0 的技能回 5 点（only_empty；非零槽不动）
#include <cstdlib>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <effects/trait_state.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "../resources/wuji_shengwu_runtime.h"
#include "scenario_harness.h"

namespace {

// 魂印注册键 = effect_icon.effect_id = 2436（2026-09-27 对齐装配层；4800=monster id）。
constexpr int kWuji = 2436;
constexpr int kHit = 10004;   // 拍打：物攻 40

using scenario::ScenarioBattle;

ElfPet wuji_pet() {
    return scenario::make_pet(0, "无极圣武", {kHit, kHit, kHit, kHit, kHit},
                              /*speed=*/200, /*atk=*/100, /*hp=*/3000,
                              SoulMark(kWuji, "无极圣武", "英雄之耀"));
}

ElfPet plain_pet(int side, int speed = 100) {
    return scenario::make_pet(side, "队友", {kHit, kHit, kHit, kHit, kHit},
                              speed, /*atk=*/100, /*hp=*/2000);
}

ElfPet poison_trait_pet(int side) {
    return scenario::make_pet(side, "静电队友", {kHit, kHit, kHit, kHit, kHit},
                              100, 100, 2000,
                              {}, scenario::make_trait("静电", 5, {5, 0}));
}

int yaolt_of(const BattleContext& ctx, int side) {
    for (const ElfPet& pet : ctx.seerRobot[side].elfPets) {
        if (pet.soulMark.id == kWuji || pet.soulMark.monster_id == kWuji) {
            auto it = pet.soulmark_storage.find(kWujiYaoltKey);
            return it == pet.soulmark_storage.end() ? -1 : std::any_cast<int>(it->second);
        }
    }
    return -1;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== A 英雄之耀计数 ==========
    {
        // 队友：2 静电（-2）、3 干净（+3）→ 层 1；对照：全静电 → -5 → 钳 0
        std::array<ElfPet, 6> team{wuji_pet(), poison_trait_pet(0), plain_pet(0),
                                   plain_pet(0), poison_trait_pet(0), plain_pet(0)};
        std::array<ElfPet, 6> foe{plain_pet(1), plain_pet(1), plain_pet(1),
                                  plain_pet(1), plain_pet(1), plain_pet(1)};
        ScenarioBattle b(team, foe);
        b.start();
        CHECK_EQ(yaolt_of(b.ctx(), 0), 1, "A1 计数：+3 干净 -2 毒特性 = 1 层");
        std::cout << "  [A1] 英耀计数（干净+1 / 毒特性-1）通过\n";
    }
    {
        std::array<ElfPet, 6> team{wuji_pet(), poison_trait_pet(0), poison_trait_pet(0),
                                   poison_trait_pet(0), poison_trait_pet(0), poison_trait_pet(0)};
        std::array<ElfPet, 6> foe{plain_pet(1), plain_pet(1), plain_pet(1),
                                  plain_pet(1), plain_pet(1), plain_pet(1)};
        ScenarioBattle b(team, foe);
        b.start();
        CHECK_EQ(yaolt_of(b.ctx(), 0), 0, "A2 全毒特性 → -5 钳 0（§三.2 默认）");
        std::cout << "  [A2] 层数钳 0 通过\n";
    }

    //========== B 砥砺登场触发器 ==========
    {
        // B1 战斗开始圣武在背包（3 号槽）→ 双方首发吃砥砺、圣武得亮节
        std::array<ElfPet, 6> team{plain_pet(0), plain_pet(0), plain_pet(0),
                                   wuji_pet(), plain_pet(0), plain_pet(0)};
        std::array<ElfPet, 6> foe{plain_pet(1), plain_pet(1), plain_pet(1),
                                  plain_pet(1), plain_pet(1), plain_pet(1)};
        ScenarioBattle b(team, foe);
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK(ctx.has_active_abnormal_status(0, static_cast<int>(AbnormalStatusId::Resolution)),
              "B1 我方首发吃砥砺（战斗开始的登场）");
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Resolution)),
              "B1 对方首发吃砥砺");
        const ElfPet& self = ctx.seerRobot[0].elfPets[3];
        CHECK(wuji_mark_active(self.on_stage_storage, kWujiLiangjieKey, ctx.roundCount),
              "B1 触发成功 → 圣武获得亮节");
        CHECK(!wuji_mark_active(self.on_stage_storage, kWujiWeiqieKey, ctx.roundCount),
              "B1 成功路径不产威怯");
        std::cout << "  [B1] 战斗开始背包触发：双方首发砥砺 + 亮节 通过\n";
    }
    {
        // B2 圣武在场 → 不触发（"位于出战背包时"）
        std::array<ElfPet, 6> team{wuji_pet(), plain_pet(0), plain_pet(0),
                                   plain_pet(0), plain_pet(0), plain_pet(0)};
        std::array<ElfPet, 6> foe{plain_pet(1), plain_pet(1), plain_pet(1),
                                  plain_pet(1), plain_pet(1), plain_pet(1)};
        ScenarioBattle b(team, foe);
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK(!ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Resolution)),
              "B2 圣武在场：对方首发不吃砥砺");
        CHECK(!wuji_mark_active(b.pet(0).on_stage_storage, kWujiLiangjieKey, ctx.roundCount),
              "B2 无亮节");
        CHECK(!wuji_mark_active(b.pet(0).on_stage_storage, kWujiWeiqieKey, ctx.roundCount),
              "B2 无威怯");
        std::cout << "  [B2] 圣武在场不触发 通过\n";
    }
    {
        // B3 失败分支：对手挂 Modern 异常免疫票后换宠登场 → 砥砺被挡 → 威怯
        std::array<ElfPet, 6> team{plain_pet(0), plain_pet(0), plain_pet(0),
                                   wuji_pet(), plain_pet(0), plain_pet(0)};
        std::array<ElfPet, 6> foe{plain_pet(1), plain_pet(1), plain_pet(1),
                                  plain_pet(1), plain_pet(1), plain_pet(1)};
        ScenarioBattle b(team, foe);
        b.start();
        BattleContext& ctx = b.ctx();
        const int dinli = static_cast<int>(AbnormalStatusId::Resolution);
        CHECK(ctx.has_active_abnormal_status(0, dinli), "B3 前置：战斗 start 已给首发砥砺");
        // 战斗开始结算后：撤掉首发的砥砺（清槽），并给对手方挂 Modern 免疫票，
        // 使"下一次登场"的砥砺必然被挡（砥砺走 Modern 通道）
        (void)dispel_active_anomalies(&ctx, 0);
        (void)dispel_active_anomalies(&ctx, 1);
        ctx.grant_immunity(1, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                           /*duration_rounds=*/5, /*source_id=*/990, /*soul_immunity=*/false,
                           EffectScope::TEAM, /*counts=*/0, /*source_effect_id=*/990,
                           /*tier=*/ImmunityTier::Modern);
        // 圣武此前的亮节清掉，隔离断言
        ctx.seerRobot[0].elfPets[3].on_stage_storage.erase(kWujiLiangjieKey);

        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);   // 对手换入 1 号
        b.drive();
        CHECK_EQ(ctx.on_stage[1], 1, "B3 前置：对手已换入 1 号（EVENT_ENTER_STAGE 的来源）");
        CHECK(!ctx.has_active_abnormal_status(1, dinli), "B3 砥砺被免疫挡下（未落地）");
        CHECK(wuji_mark_active(ctx.seerRobot[0].elfPets[3].on_stage_storage, kWujiWeiqieKey,
                               ctx.roundCount),
              "B3 触发失败 → 圣武获得威怯");
        std::cout << "  [B3] 免疫挡砥砺 → 威怯（失败分支）通过\n";
    }

    //========== C 威怯 -60%（红伤乘法，同种子对照）==========
    {
        // ⚠️ 威怯减伤条目由**圣武魂印的战斗开始节点**注册（有圣武在场才有威怯来源），
        //     所以攻击方必须是持印记的圣武本体。
        auto run = [](bool with_weiqie) {
            // ⚠️ 0 层队（毒特性队友）：圣武登场不会自然附武诛——否则武诛的增伤链
            //    （五条之1/体力分档）会混进威怯的乘法断言（批② 回归抓到过）。
            std::array<ElfPet, 6> mine{wuji_pet(), poison_trait_pet(0), poison_trait_pet(0),
                                       poison_trait_pet(0), poison_trait_pet(0), poison_trait_pet(0)};
            std::array<ElfPet, 6> foe{plain_pet(1), plain_pet(1), plain_pet(1),
                                      plain_pet(1), plain_pet(1), plain_pet(1)};
            sbs_srand(20260924);
            ScenarioBattle b(mine, foe);
            b.start();
            if (with_weiqie) {
                b.pet(0).on_stage_storage[kWujiWeiqieKey] = 999;   // 预置：持威怯
            }
            const int hp0 = b.pet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            return hp0 - b.pet(1).hp;
        };
        const int control = run(false);
        const int tested = run(true);
        std::cerr << "[dbg-C] control=" << control << " tested=" << tested
                  << " expect=" << control * 40 / 100 << std::endl;
        CHECK(control > 0, "C 前置：对照伤害为正");
        CHECK_EQ(tested, control * 40 / 100, "C 威怯 -60% 乘法：伤害 = 对照 × 40/100");
        std::cout << "  [C] 威怯红伤乘法（" << control << " → " << tested << "）通过\n";
    }

    //========== D 亮节：给对手 pp==0 的技能回 5 点 ==========
    {
        std::array<ElfPet, 6> mine{wuji_pet(), plain_pet(0), plain_pet(0),
                                   plain_pet(0), plain_pet(0), plain_pet(0)};
        std::array<ElfPet, 6> foe{plain_pet(1), plain_pet(1), plain_pet(1),
                                  plain_pet(1), plain_pet(1), plain_pet(1)};
        ScenarioBattle b(mine, foe);
        b.start();
        BattleContext& ctx = b.ctx();
        b.pet(0).on_stage_storage[kWujiLiangjieKey] = 999;   // 预置：持亮节
        b.pet(1).skills[0].pp = 0;                           // pp==0 槽（应回 5）
        b.pet(1).skills[1].pp = 2;                           // 非零槽（only_empty 不动）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        CHECK_EQ(b.pet(1).skills[0].pp, 5, "D pp==0 槽回 5 点");
        CHECK_EQ(b.pet(1).skills[1].pp, 1, "D 非零槽不动（only_empty；本回合使用扣 1：2→1）");
        std::cout << "  [D] 亮节 pp==0 回复 通过\n";
    }

    std::cout << "scenario_120_wuji_shengwu: all passed" << std::endl;
    return 0;
}
