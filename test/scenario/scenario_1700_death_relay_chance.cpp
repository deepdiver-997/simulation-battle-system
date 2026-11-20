// 场景 1700 —— J81 概率施加/死亡接棒族（2 宏，moves_lib/j_death_relay_chance.cpp）
//（2026-10-03，worktree ../agents/j81；号段 1700-1709 → T1700-T1701 两块）
//
// 断言口径：
//   - 1424 翻倍梯度 = 审计槽（990142400+owner）无随机断言：灰烬 36467 [50,2]——
//     自身无提升申报 50、有提升（+2）申报 100（封顶）；落地与否不进断言（50% 掷点）；
//   - 1319 = 死亡接棒两段 watcher：自身 hp=1 被对手拍打（atk=200）击杀 → 死切 →
//     下只登场宠吃语义序向量 [2,2,0,0,0,0]（引擎槽 0/2 各 +2）；负例 = 未被击败无暂存
//     → 登场宠不加（同场直接断六槽 0）；
//   - 载体链排查：灰烬 36467 [1424] 纯链物5；黑死魔能 36577 [1319] 纯链特攻140
//     （4399 文本"攻击+2、防御+2"与切片逐一吻合）。
#include <iostream>

#include <any>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa  = 10004;   // 拍打
constexpr int kHuiJin = 36467;   // 灰烬[1424]|[50,2]：物5（50%烧伤·有提升翻倍）
constexpr int kHeiSi  = 36577;   // 黑死魔能[1319]|[2,2,0,0,0,0]：特140（被击败→下只接棒攻+2防+2）

constexpr int kMaxHp = 99999;
constexpr int kAuditBase = 990142400;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int hp = kMaxHp, int atk = 1) {
    ElfPet p = make_pet(1, "己方", skills, 300, atk, hp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int atk = 1) {
    ElfPet p = make_pet(2, "对手", skills, 100, atk, kMaxHp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1700 1424 有提升概率翻倍（灰烬 [50,2]，审计槽无随机断言）══
    {
        // a) 自身无提升 → 申报 50
        {
            ScenarioBattle b(make_party(self_pet({kHuiJin, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(std::any_cast<int>(ctx.plugin_storage[kAuditBase + 0]), 50,
                     "T1700.a 1424：无提升申报 50（实测 " +
                         std::to_string(std::any_cast<int>(ctx.plugin_storage[kAuditBase + 0])) + "）");
        }
        // b) 自身 +2 提升 → 申报 100（翻倍封顶）
        {
            ScenarioBattle b(make_party(self_pet({kHuiJin, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            CHECK(stat_change(&ctx, 0, 0, 2) == StatChangeResult::SUCCESS,
                  "T1700.b 前置：自身 +2 攻");
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(std::any_cast<int>(ctx.plugin_storage[kAuditBase + 0]), 100,
                     "T1700.b 1424：有提升申报 100（实测 " +
                         std::to_string(std::any_cast<int>(ctx.plugin_storage[kAuditBase + 0])) + "）");
        }
        std::cout << "  [T1700] 1424 提升翻倍审计 通过\n";
    }

    // ══ T1701 1319 被击败 → 下只登场宠接棒攻+2防+2（黑死魔能 [2,2,0,0,0,0]）══
    {
        // a) 正例：自身 hp=1 被击杀 → 死切 → 下只登场宠槽0/槽2 各 +2
        {
            ElfPet carrier = self_pet({kHeiSi, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, /*hp=*/1);
            ElfPet mate = self_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa});
            std::array<ElfPet, 6> mine{carrier, mate, mate, mate, mate, mate};
            ScenarioBattle b(std::move(mine),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*atk=*/200)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 用接棒技
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 对手击杀
            b.drive();
            CHECK_EQ(static_cast<int>(ctx.currentState),
                     static_cast<int>(State::CHOOSE_AFTER_DEATH), "T1701.a 前置：停在死切");
            b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);     // 队友上场
            b.drive();
            CHECK_EQ(ctx.ability_levels[0][0], 2,
                     "T1701.a 1319：登场宠攻槽接棒 +2（实测 " +
                         std::to_string(ctx.ability_levels[0][0]) + "）");
            CHECK_EQ(ctx.ability_levels[0][2], 2,
                     "T1701.a 1319：登场宠防槽接棒 +2（实测 " +
                         std::to_string(ctx.ability_levels[0][2]) + "）");
        }
        // b) 负例：未被击败 → 无接棒（同场六槽 0）
        {
            ScenarioBattle b(make_party(self_pet({kHeiSi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            bool all_zero = true;
            for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
                if (ctx.ability_levels[0][i] != 0) {
                    all_zero = false;
                }
            }
            CHECK(all_zero, "T1701.b 1319 门：未被击败无接棒（六槽 0）");
        }
        std::cout << "  [T1701] 1319 死亡接棒 通过\n";
    }

    std::cout << "scenario_1700 PASS (J81 概率施加/死亡接棒族 1424/1319)" << std::endl;
    return 0;
}
