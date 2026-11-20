// 场景 2360 —— J143 DETECT 收尾族（1173/172/689，moves_lib/j_detect_closeout.cpp）
//（2026-10-04，worktree ../agents/j143；号段 2360-2369）
//
// 断言口径：
//   - 1173 回合末未击败粉（洗涤尘埃 34057 [905,1173]|[?,3]，特85）：R1 攻击未击杀 →
//     ROUND_END 粉 = 对手 max/3 = 333333（对手 hp 999999 扛得住）→ 轮差 = red + 333333；
//     R2 再击杀对照不做（击杀分支=hp<=0 门，逻辑浅）。
//   - 172 后手损伤回体（野性冲击 12916 [3]，物）：(a) 我速 50 后手 → red/3 回体
//     （先削血造空间）；(b) 我速 400 先手对照 → 不回。
//     ⚠️ 实测教训：对手提交 NONE（跳过）时引擎按跳过方=先手处理——先后手断言的
//     R2 必须双方都真实出手（NONE 路线的"先手对照"实际是后手，假阳记档）。
//   - 689 高伤回体（蓝色魔石 31187 [200,4]，物）：(a) atk=1000（红 ≈730>200）→
//     回 max/4；(b) atk=1（红 ~2）→ 不回。
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa  = 10004;    // 拍打
constexpr int kXiDi   = 34057;    // 洗涤尘埃 [905,1173]|[?,3]（特85）
constexpr int kYeXing = 12916;    // 野性冲击 [172]|[3]（物）
constexpr int kLanSe  = 31187;    // 蓝色魔石 [689]|[200,4]（物）
constexpr int kJiGuangDao = 19011; // 激光刀（foe 削血刀）
constexpr int kHunDun  = 17201;   // 混沌利爪（物150 大威力削血）

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300, int atk = 1, int hp = 5000) {
    ElfPet p = make_pet(1, "收尾族", skills, speed, atk, hp);
    accuracy_100(p);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100, int hp = 999999, int atk = 1) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, hp);
    accuracy_100(p);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T2360 1173 回合末未击败粉 ══
    {
        ScenarioBattle b(make_party(self_pet({kXiDi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        const int loss = 999999 - ctx.getPet(1).hp;
        const int red = ctx.resolvedDamage.final;
        const int pink = 999999 / 3;
        CHECK_EQ(loss, red + pink,
                 "T2360 1173：loss==red+max/3（loss=" + std::to_string(loss) + " red=" +
                     std::to_string(red) + " pink=" + std::to_string(pink) + "）");
        std::cout << "  [T2360] 1173 回合末未击败粉 通过\n";
    }

    // ══ T2361 172 后手损伤回体 ══
    {
        {   // (a) 后手 → red/3 回体
            ScenarioBattle b(make_party(self_pet({kYeXing, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 50,
                                                 /*atk=*/1000, /*hp=*/6000)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                300, 999999, /*atk=*/1100)));
            b.start();
            BattleContext& ctx = b.ctx();
            // R1：我后手挨刀（-604..672）→ R2 双方都出手（我速 50 真后手攻，red/3 回体）
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            const int before = ctx.getPet(0).hp;
            // R2 前把对手命中率压没（必 miss）→ 本轮净变化=纯回复
            for (Skills& sk : ctx.getPet(1).skills) {
                sk.accuracy = 1;
                sk.must_hit = 0;
            }
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            const int red = ctx.resolvedDamage.final;
            CHECK_EQ(ctx.getPet(0).hp - before, red / 3,
                     "T2361.a 172 后手：回 red/3（回 " +
                         std::to_string(ctx.getPet(0).hp - before) + " red=" +
                         std::to_string(red) + "）");
        }
        {   // (b) 先手对照 → 不回
            ScenarioBattle b(make_party(self_pet({kYeXing, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 400,
                                                 /*atk=*/1000, /*hp=*/6000)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                300, 999999, /*atk=*/1100)));
            b.start();
            BattleContext& ctx = b.ctx();
            // R1：挨刀造缺血
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            const int before = ctx.getPet(0).hp;
            // R2：双方都出手（我速 400 真先手攻 → 172 后手门不成立，不回）；对手必 miss
            for (Skills& sk : ctx.getPet(1).skills) {
                sk.accuracy = 1;
                sk.must_hit = 0;
            }
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, before, "T2361.b 172 先手对照：不回体");
        }
        std::cout << "  [T2361] 172 后手损伤回体+对照 通过\n";
    }

    // ══ T2362 689 高伤回体 ══
    {
        {   // (a) atk=1000：红 >200 → 回 max/4（先造 ≥1250 缺口防钳制）
            ScenarioBattle b(make_party(self_pet({kLanSe, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300,
                                                 /*atk=*/1000, /*hp=*/3000)),
                             make_party(foe_pet({kHunDun, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                300, 999999, /*atk=*/1300)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            const int before = ctx.getPet(0).hp;
            CHECK(before <= 3000 - 1250, "T2362.a 前置：缺口≥1250（before=" +
                                             std::to_string(before) + "）");
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int red = ctx.resolvedDamage.final;
            CHECK(red > 200, "T2362.a 前置：红>200（red=" + std::to_string(red) + "）");
            CHECK_EQ(ctx.getPet(0).hp - before, 3000 / 4,
                     "T2362.a 689：回 max/4=750（回 " +
                         std::to_string(ctx.getPet(0).hp - before) + "）");
        }
        {   // (b) atk=1：红 <200 → 不回
            ScenarioBattle b(make_party(self_pet({kLanSe, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300,
                                                 /*atk=*/1)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            const int before = ctx.getPet(0).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, before, "T2362.b 689 低伤对照：不回");
        }
        std::cout << "  [T2362] 689 高伤回体+对照 通过\n";
    }

    std::cout << "scenario_2360 ALL PASS" << std::endl;
    return 0;
}
