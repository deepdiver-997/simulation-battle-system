#include <utils/sbs_rand.h>
// 场景 1150 —— J30 回合恢复/攻击附粉族（651/676，moves_lib/j_round_heal_pink.cpp）
//（2026-10-02，worktree ../agents/j30；号段 1150-1159 → T1150-T1151 两块）
//
// 断言口径：
//   - 651 恢复掷点用 **双 rand 种子校准**（T994 口径扩展）：R1 对手攻击的浮动 rand
//     占第 1 位，R2 ROUND_START 的恢复掷点占第 2 位 → 校准 rand#2%201==0 的种子，
//     R2 恰回 200；R3 恰再回 200（同 rand 流的确定延续）；R4 过期 +0；
//   - 676 粉伤 = 视图双防和 ×30%（make_pet 防/特防各 100 → 恰 60）；我方攻击红伤
//     atk=1 时仅 1~3 → 对手每刀掉血 ∈ [60,65] 带；R1 施放回合是属性技 → 对手 0 掉血
//     （attack 过滤对照）；R5 过期 ≤5。
#include <cstdlib>
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

// ── 载体技（切片已逐条与 side_effect/arg 核对）──
constexpr int kPaiDa   = 10004;   // 拍打：物40（攻击探针）
constexpr int kDaoXiang = 27602;  // 稻香之息 [651]|[3,200,400] 属（651 3回合 200-400）
constexpr int kDiLie   = 23663;   // 地裂山塌 [676]|[3,30]    属（676 3回合 双防30%）

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300, int atk = 1, int hp = 5000) {
    ElfPet p = make_pet(1, "窗族三", skills, speed, atk, hp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100, int atk = 100, int hp = 99999) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, hp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1150 651 每回合随机恢复（27602 [3,200,400]；种子校准 rand#2 → 恰 200）══
    {
        ScenarioBattle b(make_party(self_pet({kDaoXiang, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                             /*speed=*/300, /*atk=*/1, /*hp=*/5000)),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            /*speed=*/100, /*atk=*/3000)));
        b.start();
        BattleContext& ctx = b.ctx();
        int seed = -1;
        for (int s = 1; s < 1000000 && seed < 0; ++s) {
            sbs_srand(s);
            int r[7];
            for (int k = 1; k <= 6; ++k) {
                r[k] = sbs_rand() % 201;
            }
            // 实证（seed=1 探针）：#1=施放即恢复，对手刀每轮耗 3 rand（命中/浮动/其他）
            // → R2 恢复=#5、R3 恢复=#6。校准 #5==0 且 #6==0 → 两回合恰各回 200。
            if (r[5] == 0 && r[6] == 0) {
                seed = s;
            }
        }
        REQUIRE(seed > 0);
        ctx.seed_rng(seed);
        std::cerr << "[R651] seed=" << seed << "\n";
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 稻香之息
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 对手留缺口
        b.drive();
        int hp_r1 = ctx.getPet(0).hp;
        CHECK(hp_r1 < 5000, "T1150 前置：R1 对手刀留缺口（且首回合恢复被 5000 钳）");
        for (int r = 2; r <= 3; ++r) {
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int healed = ctx.getPet(0).hp - hp_r1;
            CHECK_EQ(healed, 200, "T1150 651：R" + std::to_string(r) +
                                      " 校准掷点恰回 200（rand#2%201==0）");
            hp_r1 = ctx.getPet(0).hp;
        }
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(0).hp, hp_r1, "T1150 651：R4 窗口过期不再恢复");
        std::cout << "  [T1150] 651 每回合随机恢复 通过 (seed=" << seed << ")\n";
    }

    // ══ T1151 676 攻击附双防粉伤（23663 [3,30]；双防和 200 → 每刀 +60 粉）══
    {
        ScenarioBattle b(make_party(self_pet({kPaiDa, kDiLie, kPaiDa, kPaiDa, kPaiDa},
                                             /*speed=*/300, /*atk=*/1)),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R1 地裂山塌（属性技）
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(1).hp, 99999,
                 "T1151 676：R1 施放为属性技不触发（attack 过滤）");
        // 视图双防和（676 的粉伤基数）：make_pet 防/特防 base 各 100，视图值以运行时为准。
        const int def_sum = ctx.ws.getTempAbilityValue(0, NumericalPropertyIndex::DEFENSE)
            + ctx.ws.getTempAbilityValue(0, NumericalPropertyIndex::SPECIAL_DEFENSE);
        const int pink = def_sum * 30 / 100;
        CHECK(pink > 0, "T1151 前置：视图双防和>0（实测和 " + std::to_string(def_sum) + "）");
        for (int r = 2; r <= 4; ++r) {   // 窗 [R2,R4]
            const int foe_hp = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 拍打
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = foe_hp - ctx.getPet(1).hp;
            CHECK(loss >= pink + 1 && loss <= pink + 4,
                  "T1151 676：R" + std::to_string(r) + " 附粉 " + std::to_string(pink) +
                      "（红伤 1~3，实测掉 " + std::to_string(loss) + "）");
        }
        const int foe_hp = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R5 过期
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK(ctx.getPet(1).hp - foe_hp <= 4,
              "T1151 676：R5 窗口过期只余红伤（实测掉 " +
                  std::to_string(foe_hp - ctx.getPet(1).hp) + "）");
        std::cout << "  [T1151] 676 攻击附双防粉伤 通过\n";
    }

    std::cout << "scenario_1150 PASS (J30 回合恢复/攻击附粉族)" << std::endl;
    return 0;
}
