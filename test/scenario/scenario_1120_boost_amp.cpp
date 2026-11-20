// 场景 1120 —— J27 条件强化输出族（3 宏，moves_lib/j_boost_cond_amp.cpp）
//（2026-10-02，worktree ../agents/j27；号段 1120-1129 → T1120-T1122 三块）
//
// 断言口径：
//   - 845 用双局对照（atk=300 定伤害量级）：满血局（>1/2）威力 +50% → 比值带
//     1.5×[0.85,1/0.85]=[127,176]%；半血以下局（直写 hp=max/3）= 基线对照；
//   - 709 用 ability_levels 精确断言（击败门 + 有提升 → +2 翻 +4；存活门/无提升门
//     两个负例）；
//   - 808 用掉血带断言（atk=1 蹭伤 1~2 + 种数×80 固伤精确）；
//   - 载体链排查：30848/16981/30952 均单效果载体。
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

constexpr int kPaiDa  = 10004;   // 拍打：物40
constexpr int kDianAn = 30848;   // 电暗天威[845]|[2,50]：特130（高体威力+50%）
constexpr int kAoYi   = 16981;   // 奥义神灭斩[709]：物160（击败→提升翻倍，零参）
constexpr int kPiLi   = 30952;   // 霹雳迅击[808]|[80]：物130（每种提升+80固伤）
constexpr int kMaxHp = 999999;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300, int atk = 300) {
    ElfPet p = make_pet(1, "己方", skills, speed, atk, kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100, int atk = 1, int hp = kMaxHp) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, hp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

int boost_slot_count(BattleContext& ctx, int side) {
    int n = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx.ability_levels[side][i] > 0) {
            ++n;
        }
    }
    return n;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1120 845 高体→威力+50%（电暗天威 [845]|[2,50]，双局对照）══
    {
        int d_lowhp = 0, d_full = 0;
        {
            // 基线：hp 压到 max/3（< 1/2）→ 无威力加成
            ScenarioBattle b(
                make_party(self_pet({kDianAn, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = kMaxHp / 3;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            d_lowhp = kMaxHp - ctx.getPet(1).hp;   // 对手满血起算（未还手）
        }
        CHECK(d_lowhp > 50, "T1120 前置：低血局正伤");
        {
            ScenarioBattle b(
                make_party(self_pet({kDianAn, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            d_full = kMaxHp - ctx.getPet(1).hp;
        }
        const int ratio_x100 = d_full * 100 / d_lowhp;
        CHECK(ratio_x100 >= 127 && ratio_x100 <= 176,
              "T1120 845：满血局威力 +50%（比值 " + std::to_string(ratio_x100) +
                  "%，带 127~176；低血=" + std::to_string(d_lowhp) +
                  " 满血=" + std::to_string(d_full) + "）");
        std::cout << "  [T1120] 845 高体威力提升 通过 (" << ratio_x100 << "%)\n";
    }

    // ══ T1121 709 击败且自身有提升 → 提升翻倍（奥义神灭斩，零参）══
    {
        // a) 有提升 + 击杀（对手 hp=1，atk=1 即可）→ 攻击 +2 → +4
        {
            ScenarioBattle b(
                make_party(self_pet({kAoYi, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                    /*speed=*/300, /*atk=*/1)),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                   /*speed=*/100, /*atk=*/1, /*hp=*/1)));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)stat_change(&ctx, 0, 0, 2);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(ctx.getPet(1).hp <= 0, "T1121.a 前置：对手被击倒");
            CHECK_EQ(ctx.ability_levels[0][0], 4,
                     "T1121.a 709：击败 → 攻击 +2 翻倍为 +4");
        }
        // b) 门负例：对手存活 → 不翻倍
        {
            ScenarioBattle b(
                make_party(self_pet({kAoYi, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                    /*speed=*/300, /*atk=*/1)),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)stat_change(&ctx, 0, 0, 2);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(ctx.getPet(1).hp > 0, "T1121.b 前置：对手存活");
            CHECK_EQ(ctx.ability_levels[0][0], 2, "T1121.b 709 门：未击败不翻倍");
        }
        // c) 门负例：击杀但无提升 → 不动
        {
            ScenarioBattle b(
                make_party(self_pet({kAoYi, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                    /*speed=*/300, /*atk=*/1)),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                   /*speed=*/100, /*atk=*/1, /*hp=*/1)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(boost_slot_count(ctx, 0), 0, "T1121.c 709 门：无提升不凭空翻倍");
        }
        std::cout << "  [T1121] 709 击败提升翻倍 通过\n";
    }

    // ══ T1122 808 每种提升 +80 固伤（霹雳迅击 [808]|[80]）══
    {
        // a) 两种提升（攻+2 防+1）→ 160 固伤 + 蹭伤 1~2
        {
            ScenarioBattle b(
                make_party(self_pet({kPiLi, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                    /*speed=*/300, /*atk=*/1)),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)stat_change(&ctx, 0, 0, 2);
            (void)stat_change(&ctx, 0, 2, 1);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = kMaxHp - ctx.getPet(1).hp;
            CHECK(loss >= 160 && loss <= 164,
                  "T1122.a 808：两种提升 → 160 固伤（实测掉 " + std::to_string(loss) + "）");
        }
        // b) 门负例：无提升 → 只有蹭伤
        {
            ScenarioBattle b(
                make_party(self_pet({kPiLi, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                    /*speed=*/300, /*atk=*/1)),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = kMaxHp - ctx.getPet(1).hp;
            CHECK(loss >= 0 && loss <= 3,
                  "T1122.b 808 门：无提升无固伤（实测掉 " + std::to_string(loss) + "）");
        }
        std::cout << "  [T1122] 808 每种提升固伤 通过\n";
    }

    std::cout << "scenario_1120 PASS (J27 条件强化输出族 845/709/808)" << std::endl;
    return 0;
}
