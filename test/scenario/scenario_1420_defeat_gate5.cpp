// 场景 1420 —— J56 击败门族五（3 宏，moves_lib/j_defeat_gate5.cpp）
//（2026-10-03，worktree ../agents/j56；号段 1420-1429 → T1420-T1422 三块）
//
// 断言口径：
//   - 击败门正例 = hp=1 稻草靶 + 两宠队伍死切驱动（953 协议：CHOOSE_AFTER_DEATH 停点
//     → CHOOSE_PET 队友上场）；门负例 = 对手 99999 血存活；
//   - 650 六槽断言 = ability_levels 直读（kStatMap 语义序：[1,0,0,0,1,0] → 槽0/槽4 +1）；
//   - 1719 增伤行为断言 = R2 拍打（物40·atk200→带 [58,69]）相对带：+50% → [84,108]、
//     +100%（暴击击败加层）→ [112,144]，R3 窗过期回 [55,72]；暴击只挂在载体技
//     （critical_strike_rate=100，拍打 0 → 不消耗 rand、不污染带宽）；
//   - 1090 白板断言 = R2 对手攻击后自身 hp 不动（负例掉 [55,72] 对照）；
//   - 载体链污染排查：17493 惊天破岩冲 [650] / 36414 星汉呼啸 [1719] / 36215 灾厄穿刺
//     [1090] 全部单效果载体，对断言零干扰。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa   = 10004;   // 拍打：物40（探针攻击）
constexpr int kJingTian = 17493;  // 惊天破岩冲[650]|[1,0,0,0,1,0]：物150（击败→攻+1速+1）
constexpr int kXingHanC = 36414;  // 星汉呼啸[1719]|[1,50,50]：物130（击败→下1回合+50%，暴击再+50%）
constexpr int kZaiE    = 36215;   // 灾厄穿刺[1090]|[1]：物130（击败→1回合对手攻击技白板）

constexpr int kSelfMaxHp = 99999;

// 语义序（攻,防,特攻,特防,速,命中）→ 引擎槽（实现侧 kStatMap 同表，断言换算用）。
constexpr int kStatMapSem[6] = {0, 2, 1, 3, 4, 5};

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300, int atk = 200) {
    ElfPet p = make_pet(1, "己方", skills, speed, atk, kSelfMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100, int atk = 1, int hp = kSelfMaxHp) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, hp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

// 击杀稻草靶的通用流程：R1 用 idx0 技能击杀 → 停死切 → 队友上场。
void kill_fodder(ScenarioBattle& b, BattleContext& ctx) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
    CHECK_EQ(static_cast<int>(ctx.currentState),
             static_cast<int>(State::CHOOSE_AFTER_DEATH), "前置：停在死切");
    b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1420 650 当次攻击击败 → 自身六槽提升（惊天破岩冲 [1,0,0,0,1,0]）══
    {
        // a) 正例：击杀 → 攻+1（槽0）、速+1（槽4），其余 0
        {
            ElfPet me = self_pet({kJingTian, kPaiDa, kPaiDa, kPaiDa, kPaiDa});
            ElfPet fodder = foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                    /*speed=*/100, /*atk=*/1, /*hp=*/1);
            ElfPet mate = foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa});
            std::array<ElfPet, 6> foes{fodder, mate, mate, mate, mate, mate};
            ScenarioBattle b(make_party(std::move(me)), std::move(foes));
            b.start();
            BattleContext& ctx = b.ctx();
            kill_fodder(b, ctx);
            const char* names[6] = {"攻", "防", "特攻", "特防", "速", "命中"};
            const int expect[6] = {1, 0, 0, 0, 1, 0};
            for (int i = 0; i < 6; ++i) {
                const int slot = kStatMapSem[i];
                CHECK_EQ(ctx.ability_levels[0][slot], expect[i],
                         std::string("T1420.a 650：") + names[i] + " 槽==" +
                             std::to_string(expect[i]));
            }
        }
        // b) 负例：对手存活 → 六槽不动
        {
            ScenarioBattle b(make_party(self_pet({kJingTian, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(ctx.getPet(1).hp > 0, "T1420.b 前置：对手存活（未击败）");
            bool all_zero = true;
            for (int i = 0; i < 6; ++i) {
                if (ctx.ability_levels[0][i] != 0) {
                    all_zero = false;
                }
            }
            CHECK(all_zero, "T1420.b 650 门：未击败六槽不动");
        }
        std::cout << "  [T1420] 650 击败六槽提升 通过\n";
    }

    // ══ T1421 1719 击败 → 下 1 回合增伤 +50%，暴击击败再 +50%（星汉呼啸 [1,50,50]）══
    {
        // a) 暴击击败 → 总 +100%：R2 拍打带 [112,144]，R3 过期回 [55,72]
        {
            ElfPet me = self_pet({kXingHanC, kPaiDa, kPaiDa, kPaiDa, kPaiDa});
            me.skills[0].critical_strike_rate = 100.0f;   // 载体技必暴（拍打仍 0）
            ElfPet fodder = foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                    /*speed=*/100, /*atk=*/1, /*hp=*/1);
            ElfPet mate = foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa});
            std::array<ElfPet, 6> foes{fodder, mate, mate, mate, mate, mate};
            ScenarioBattle b(make_party(std::move(me)), std::move(foes));
            b.start();
            BattleContext& ctx = b.ctx();
            kill_fodder(b, ctx);
            int hp0 = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);  // R2 拍打
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss_r2 = hp0 - ctx.getPet(1).hp;
            CHECK(loss_r2 >= 112 && loss_r2 <= 144,
                  "T1421.a 1719：暴击击败 → R2 增伤 +100%（实测掉 " +
                      std::to_string(loss_r2) + "，带 [112,144]）");
            hp0 = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);  // R3 拍打
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss_r3 = hp0 - ctx.getPet(1).hp;
            CHECK(loss_r3 >= 55 && loss_r3 <= 72,
                  "T1421.a 1719：R3 窗过期回基准带（实测掉 " + std::to_string(loss_r3) + "）");
        }
        // b) 非暴击击败 → +50%：R2 拍打带 [84,108]
        {
            ElfPet me = self_pet({kXingHanC, kPaiDa, kPaiDa, kPaiDa, kPaiDa});
            ElfPet fodder = foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                    /*speed=*/100, /*atk=*/1, /*hp=*/1);
            ElfPet mate = foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa});
            std::array<ElfPet, 6> foes{fodder, mate, mate, mate, mate, mate};
            ScenarioBattle b(make_party(std::move(me)), std::move(foes));
            b.start();
            BattleContext& ctx = b.ctx();
            kill_fodder(b, ctx);
            const int hp0 = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);  // R2 拍打
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss_r2 = hp0 - ctx.getPet(1).hp;
            CHECK(loss_r2 >= 84 && loss_r2 <= 108,
                  "T1421.b 1719：非暴击击败 → R2 增伤 +50%（实测掉 " +
                      std::to_string(loss_r2) + "，带 [84,108]）");
        }
        // c) 门负例：未击败 → R2 无增伤（[55,72]）
        {
            ScenarioBattle b(make_party(self_pet({kXingHanC, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            int hp0 = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);  // R2 拍打
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss_r2 = hp0 - ctx.getPet(1).hp;
            CHECK(loss_r2 >= 55 && loss_r2 <= 72,
                  "T1421.c 1719 门：未击败 R2 无增伤（实测掉 " + std::to_string(loss_r2) + "）");
        }
        std::cout << "  [T1421] 1719 击败增伤窗 通过\n";
    }

    // ══ T1422 1090 击败 → 1 回合对手攻击技白板（灾厄穿刺 [1090]|[1]）══
    {
        // a) 正例：击杀 → R2 对手攻击 self hp 不动
        {
            ElfPet me = self_pet({kZaiE, kPaiDa, kPaiDa, kPaiDa, kPaiDa});
            ElfPet fodder = foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                    /*speed=*/100, /*atk=*/1, /*hp=*/1);
            // 队友 atk=200：若无白板，拍打能打掉 [55,72]——断言有感
            ElfPet mate = foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                  /*speed=*/100, /*atk=*/200);
            std::array<ElfPet, 6> foes{fodder, mate, mate, mate, mate, mate};
            ScenarioBattle b(make_party(std::move(me)), std::move(foes));
            b.start();
            BattleContext& ctx = b.ctx();
            kill_fodder(b, ctx);
            const int self_hp = ctx.getPet(0).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);  // R2 对手拍打
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, self_hp,
                     "T1422.a 1090：对手攻击被白板（自身未掉血，实测 "
                         + std::to_string(self_hp - ctx.getPet(0).hp) + "）");
        }
        // b) 门负例：未击败 → R2 对手攻击正常落地（掉 [55,72]）
        {
            ScenarioBattle b(make_party(self_pet({kZaiE, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*speed=*/100, /*atk=*/200)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int self_hp = ctx.getPet(0).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);  // R2 对手拍打
            b.drive();
            const int taken = self_hp - ctx.getPet(0).hp;
            CHECK(taken >= 55 && taken <= 72,
                  "T1422.b 1090 门：未击败对手攻击正常落地（实测掉 " +
                      std::to_string(taken) + "）");
        }
        std::cout << "  [T1422] 1090 击败白板窗 通过\n";
    }

    std::cout << "scenario_1420 PASS (J56 击败门族五 650/1719/1090)" << std::endl;
    return 0;
}
