// 场景 5560 —— J225 命中异常未触发封攻族（1058，moves_lib/j_hit_anomaly_else_seal_attack.cpp）
//（2026-10-22，worktree feat/jbatch；号段 5560-5569）
//
// 断言口径（异常位 + 次回合攻击落地/被封 双观测）：
//   - 1058（火光烛天 26539 [1058,1239,1391]|[100,2,1,...]）：100% 档 → 灼伤(2)；
//     （凰龙息烬 25129 [1058,191,738]|[80,2,2,...]）：80% 档 → 统计带。
//   - a) 100% 档：R1 末对手灼伤；R2 对手攻击照常落地（未触发封攻）。
//   - b) 80% 档 40 局：灼伤存在 ⇔ R2 攻击造成伤害；灼伤缺失 ⇔ 攻击技能被封印（0 伤害）。
//   - c) 对照：R1 不施放 → R2 对手攻击必造成伤害（证明攻击本身有伤害）。
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

constexpr int kPaiDa   = 10004;  // 拍打：物 40（填充刀）
constexpr int kGuang   = 26539;  // [1058,1239,1391]|[100,2,1,2,2,3,2]（100% 灼伤）
constexpr int kHuang   = 25129;  // [1058,191,738]|[80,2,2,4,3,2]（80% 灼伤/封攻）
constexpr int kBurn    = static_cast<int>(AbnormalStatusId::Burn);  // 灼伤 2
constexpr int kSelfHp  = 10000;
constexpr int kFoeHp   = 99999;
constexpr int kFoeAtk  = 50;
constexpr int kTrials  = 40;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_self(int carrier) {
    ElfPet p = make_pet(1, "己方", {carrier, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, /*hp=*/kSelfHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/kFoeAtk, /*hp=*/kFoeHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

void self_cast(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

void foe_attacks(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5560 1058 命中异常 / 未触发封攻 ══
    {
        {   // (a) 100% 档：灼伤落地 → 不封攻
            ScenarioBattle b(make_party(mk_self(kGuang)), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            self_cast(b);
            CHECK(ctx.has_active_abnormal_status(1, kBurn),
                  "T5560.a 1058 100% 档：对手灼伤(2)");
            const int before = ctx.getPet(0).hp;
            foe_attacks(b);
            CHECK(ctx.getPet(0).hp < before,
                  "T5560.a 1058：触发异常后不封攻（R2 对手攻击落地，掉血 " +
                      std::to_string(before - ctx.getPet(0).hp) + "）");
        }
        {   // (c) 对照：R1 不施放 → R2 攻击必落地
            ScenarioBattle b(make_party(mk_self(kGuang)), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int before = ctx.getPet(0).hp;
            foe_attacks(b);
            CHECK(ctx.getPet(0).hp < before,
                  "T5560.c 对照：无 1058 时 R2 攻击照常造成伤害（掉血 " +
                      std::to_string(before - ctx.getPet(0).hp) + "）");
        }
        {   // (b) 80% 档统计：灼伤 ⇔ 攻击落地；无灼伤 ⇔ 封攻 0 伤害
            int hits = 0;
            int misses = 0;
            for (int i = 0; i < kTrials; ++i) {
                ScenarioBattle b(make_party(mk_self(kHuang)), make_party(mk_foe()));
            b.ctx().seed_rng(1 + i * 104729u);  // 每局独立且可复现的抽样序列
                b.start();
                BattleContext& ctx = b.ctx();
                self_cast(b);
                const bool burned = ctx.has_active_abnormal_status(1, kBurn);
                const int before = ctx.getPet(0).hp;
                foe_attacks(b);
                const int loss = before - ctx.getPet(0).hp;
                if (burned) {
                    ++hits;
                    CHECK(loss > 0,
                          "T5560.b 1058 80% 档：灼伤落地 → 未封攻，R2 攻击应有伤害（loss=" +
                              std::to_string(loss) + "）");
                } else {
                    ++misses;
                    CHECK_EQ(loss, 0,
                             "T5560.b 1058 80% 档：未触发异常 → 封攻生效，R2 攻击 0 伤害");
                }
            }
            CHECK(hits >= 22 && hits <= kTrials,
                  "T5560.b 1058 80% 档 40 局灼伤带 [22,40]，实测 " + std::to_string(hits));
            CHECK(misses >= 1,
                  "T5560.b 1058 80% 档：40 局至少出现一次未触发（实测 " +
                      std::to_string(misses) + "）");
            std::cout << "  [T5560] 80% 档 灼伤 " << hits << "/" << kTrials << "、封攻 "
                      << misses << "/" << kTrials << "\n";
        }
        std::cout << "  [T5560] 1058 命中异常 / 未触发封攻 / 对照 通过\n";
    }

    std::cout << "scenario_5560 PASS (J225 命中异常未触发封攻族)" << std::endl;
    return 0;
}
