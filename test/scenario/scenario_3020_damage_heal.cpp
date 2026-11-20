#include <utils/sbs_rand.h>
// 场景 3020 —— K168 伤害回血族（363，soul_lib/k168_damage_heal.cpp）
//（2026-10-06，worktree ../agents/k168；⚠️ 号段记档：看板行 2730-2739 与 J171 场景段
//   撞号（author 分配竞态），本单场景改用 alloc 实配 3020-3029；看板行不回改，以本
//   记档对账。→ T3020-T3021 两块）
//
// 断言口径（对照对局对，同 seed 同操作——伤害浮动序列一致）：
//   - 363 走 阿米尔 [30, 0]（行参经 SoulMark args 传入，{0,0,30,0} 占位形）：
//     R1 我方攻击 → 我方回血 = plain 失血 ×30/100（同 base 整除一致）；
//   - 对手也带同款？否——只有持有方带印；对照局无印 → 无回血。
#include <array>
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

constexpr int kPaDa = 10004;   // 拍打：攻击载体

template <int MarkId>
ElfPet mark_pet(const std::array<int, 5>& skills, int speed = 300, int hp = 99999,
                EffectArgs mark_args = {}) {
    ElfPet p = make_pet(1, "印主", skills, speed, /*atk=*/100, hp,
                        /*soul_mark=*/SoulMark(MarkId, "K168 魂印", "",
                                               std::move(mark_args)));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_me(const std::array<int, 5>& skills, int speed = 300, int hp = 99999) {
    ElfPet p = make_pet(1, "对照", skills, speed, /*atk=*/100, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_pet(const std::array<int, 5>& skills, int speed = 100, int hp = 99999) {
    ElfPet p = make_pet(2, "对手", skills, speed, /*atk=*/1, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
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

    // ══ T3020 363 攻击伤害 30% 回血（阿米尔 [30]，对照对局对）══
    {
        int marked_heal = -1;
        int marked_loss = -1;
        int plain_heal = -1;
        int plain_loss = -1;
        for (int side = 0; side < 2; ++side) {
            auto me = side == 0
                ? mark_pet<363>({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa}, 300, 99999,
                                EffectArgs(std::vector<int>{30, 0}))
                : plain_me({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa}, 300, 99999);
            ScenarioBattle b(make_party(std::move(me)),
                             make_party(plain_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.seerRobot[0].elfPets[0].hp = 50000;   // 留回血空间
            sbs_srand(7);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int foe_loss = 99999 - ctx.getPet(1).hp;
            const int self_gain = ctx.getPet(0).hp - 50000;
            if (side == 0) {
                marked_loss = foe_loss;
                marked_heal = self_gain;
            } else {
                plain_loss = foe_loss;
                plain_heal = self_gain;
            }
        }
        CHECK_EQ(marked_heal, marked_loss * 30 / 100,
                 "T3020 363：回血 = 伤害×30/100（同 base 整除）");
        CHECK_EQ(plain_heal, 0, "T3020 363：无印对照 → 不回血");
        CHECK(marked_loss > 0, "T3020 363 前置：攻击落地");
        std::cout << "  [T3020] 363 伤害回血 通过 (loss=" << marked_loss
                  << " heal=" << marked_heal << ")\n";
    }

    // ══ T3021 363 回血钳 max（hp 顶格不溢出）══
    {
        ScenarioBattle b(make_party(mark_pet<363>({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                                  300, 99999)),
                         make_party(plain_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK(ctx.getPet(0).hp <= 99999, "T3021 363：回血钳 max");
        std::cout << "  [T3021] 363 回血钳 max 通过\n";
    }

    std::cout << "scenario_3020 PASS (K168 伤害回血族)" << std::endl;
    return 0;
}
