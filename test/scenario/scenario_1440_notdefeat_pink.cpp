// 场景 1440 —— J58 未击败/回合末粉伤族（752/1285/411，moves_lib/j_notdefeat_pink.cpp）
//（2026-10-03，worktree ../agents/j58；号段 1440-1449 → T1440-T1442 三块）
//
// 断言口径：
//   - 752 异常门削 max 1/{0}（36922 [3]/36961 [4] 纯链）：loss - 红 == max/3=33332、
//     max/4=24999（FLOOR）；对照无异常 → 无固伤。异常用麻痹（无回合末 tick，
//     烧伤/中毒会污染精确差分）。
//   - 1285 未击败门全槽 PP-1（33608 [1224,1129,1285]|[1]，兄弟只动红伤）：
//     存活局对手全技能 PP 35→34；击杀局门关 → PP 不动。
//   - 411 当前体力% 粉连用递增（34167 [48,411]|[20,10,40]，48=自身免疫良性）：
//     四轮 loss = 红 + hp_before×pct/100（与实现同式整数除法，精确等式）；
//     pct 梯度走审计槽 20/30/40/40（首用=base、封顶 cap，1328 同款）。
//   atk=1 消红噪声；loss 与 resolvedDamage.final 差分即附加粉（T1372 口径）。
#include <any>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

// ── 载体技（切片已逐条与 side_effect/arg 核对）──
constexpr int kPaiDa   = 10004;   // 拍打（填充刀，PP 35）
constexpr int kNianLi  = 36922;   // 念力封锁 [752]|[3] 特140（752 削 max/3）
constexpr int kDongGan = 36961;   // 冻感舞蹈 [752]|[4] 特130（752 削 max/4）
constexpr int kJiFeng  = 33608;   // 寂·凛风残念 [1224,1129,1285]|[2,30,2,1] 物160
constexpr int kLengTong = 34167;  // 冷瞳裁决 [48,411]|[3,20,10,40] 物150

// 411 审计槽键（与 j_notdefeat_pink.cpp 对表；只读）
constexpr int k411PctAuditBase = 990158200;

int audit_int(const BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return 0;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : 0;
}

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300, int atk = 1, int hp = 5000) {
    ElfPet p = make_pet(1, "粉伤族", skills, speed, atk, hp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100, int atk = 1, int hp = 99998) {
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

    // ══ T1440 752 异常门削 max 1/{0}（36922 [3] / 36961 [4]）══
    {
        {   // (a) 对手麻痹 → loss = 红 + 33332（99998/3 FLOOR）
            ScenarioBattle b(make_party(self_pet({kNianLi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)apply_anomaly(&ctx, 1, static_cast<int>(AbnormalStatusId::Paralysis), 9, 0);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = 99998 - ctx.getPet(1).hp;
            CHECK_EQ(loss, ctx.resolvedDamage.final + 33332,
                     "T1440.a 752：对手有异常 → 削 max/3（loss=" + std::to_string(loss) + "）");
        }
        {   // (b) 对照：无异常 → loss = 红
            ScenarioBattle b(make_party(self_pet({kNianLi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = 99998 - ctx.getPet(1).hp;
            CHECK_EQ(loss, ctx.resolvedDamage.final,
                     "T1440.b 752 门：无异常无削（loss=" + std::to_string(loss) + "）");
        }
        {   // (c) 36961 [4]：对手麻痹 → loss = 红 + 24999（99998/4 FLOOR）
            ScenarioBattle b(make_party(self_pet({kDongGan, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)apply_anomaly(&ctx, 1, static_cast<int>(AbnormalStatusId::Paralysis), 9, 0);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = 99998 - ctx.getPet(1).hp;
            CHECK_EQ(loss, ctx.resolvedDamage.final + 24999,
                     "T1440.c 752 [4]：削 max/4（loss=" + std::to_string(loss) + "）");
        }
        std::cout << "  [T1440] 752 异常门削 max 通过\n";
    }

    // ══ T1441 1285 未击败门全槽 PP-1（33608 [1]）══
    {
        {   // (a) 对手存活 → 全技能 PP 35→34
            ScenarioBattle b(make_party(self_pet({kJiFeng, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(ctx.getPet(1).hp > 0, "T1441.a 前置：对手存活（未击败）");
            for (int i = 0; i < 5; ++i) {
                CHECK_EQ(ctx.getPet(1).skills[static_cast<std::size_t>(i)].pp, 34,
                         "T1441.a 1285：对手技能槽 " + std::to_string(i) + " PP 35→34");
            }
        }
        {   // (b) 击杀局：门关 → PP 不动
            ScenarioBattle b(make_party(self_pet({kJiFeng, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 /*speed=*/300, /*atk=*/3000)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*speed=*/100, /*atk=*/1, /*hp=*/2000)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 击杀
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(ctx.getPet(1).hp <= 0, "T1441.b 前置：对手被击败");
            for (int i = 0; i < 5; ++i) {
                CHECK_EQ(ctx.getPet(1).skills[static_cast<std::size_t>(i)].pp, 35,
                         "T1441.b 1285 门：击败局 PP 不减（槽 " + std::to_string(i) + "）");
            }
        }
        std::cout << "  [T1441] 1285 未击败门 PP 通过\n";
    }

    // ══ T1442 411 当前体力%粉连用递增（34167 [20,10,40]）══
    {
        ScenarioBattle b(make_party(self_pet({kLengTong, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        const int expect_pct[4] = {20, 30, 40, 40};
        int hp_before = 99998;
        for (int r = 0; r < 4; ++r) {
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int pct = audit_int(ctx, k411PctAuditBase + 0);
            CHECK_EQ(pct, expect_pct[r], "T1442 411 第 " + std::to_string(r + 1) +
                                             " 用申报 pct（期望 " +
                                             std::to_string(expect_pct[r]) + "）");
            const int hp_after = ctx.getPet(1).hp;
            const int loss = hp_before - hp_after;
            const int pink = hp_before * pct / 100;   // 与实现同式整数除法
            CHECK_EQ(loss, ctx.resolvedDamage.final + pink,
                     "T1442 411 第 " + std::to_string(r + 1) + " 轮：loss=红+当前" +
                         std::to_string(pct) + "%（loss=" + std::to_string(loss) + "）");
            hp_before = hp_after;
        }
        std::cout << "  [T1442] 411 递增粉伤 通过\n";
    }

    std::cout << "scenario_1440 PASS (J58 未击败/回合末粉伤族)" << std::endl;
    return 0;
}
