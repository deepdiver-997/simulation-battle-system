// 场景 5540 —— J224 固定伤害未落地异常族（1895，moves_lib/j_drain_fixed_fallback_anomaly.cpp）
//（2026-10-22，worktree feat/jbatch；号段 5540-5549）
//
// 断言口径（精确回血量 + 异常位 + 双分支）：
//   - 1895（彼空·真理重塑 36302 [1895,1896,984,1811]|[300,100,6,2,3]）：
//     {0}=300、{1}=100%、{2}=害怕(6)；链兄弟 1896/984/1811 均无 hp/异常面。
//   - a) 正常：对手受 300 固定伤害 → 自身回 300（半血观测，避开 max_hp clamp）；
//        对手 hp 损失 = 300 + 本刀红伤（atk=1 小值带）；不得附加异常。
//   - b) 免粉：对手 PINK_DAMAGE 免疫票 → 固伤不落地 → 自身**不回血**、
//        且 100% 申报害怕（6）——"未受到固定伤害"兜底分支。
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
constexpr int kBiKong  = 36302;  // [1895,1896,984,1811]|[300,100,6,2,3]
constexpr int kFear    = static_cast<int>(AbnormalStatusId::Fear);  // 害怕 6
constexpr int kMaxHp   = 99999;
constexpr int kSelfHp  = 5000;   // 半血观测（避免回血被 max_hp clamp）
constexpr int kDrain   = 300;    // {0}

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_self() {
    ElfPet p = make_pet(1, "己方", {kBiKong, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, /*hp=*/kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/kMaxHp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

void run_skill(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5540 1895 吸取 300 / 免粉未落地 → 害怕兜底 ══
    {
        {   // (a) 正常：固伤落地 → 回 300；无异常
            ScenarioBattle b(make_party(mk_self()), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = kSelfHp;   // 只压当前体力，最大体力不变
            run_skill(b);

            CHECK_EQ(ctx.getPet(0).hp, kSelfHp + kDrain,
                     "T5540.a 1895：固伤落地 → 自身回 300（实测 " +
                         std::to_string(ctx.getPet(0).hp) + "，应 5300）");
            const int foe_loss = kMaxHp - ctx.getPet(1).hp;
            CHECK(foe_loss >= kDrain && foe_loss <= kDrain + 30,
                  "T5540.a 1895：对手损失 = 300 + 红伤带（实测 " +
                      std::to_string(foe_loss) + "）");
            CHECK(!ctx.has_active_abnormal_status(1, kFear),
                  "T5540.a 1895：固伤落地 → 不申报兜底异常（无害怕）");
        }
        {   // (b) 免粉：固伤不落地 → 不回血；100% 害怕
            ScenarioBattle b(make_party(mk_self()), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = kSelfHp;
            ctx.grant_immunity(1, ImmunityType::PINK_DAMAGE, coverage_all(),
                               /*anomaly_mask=*/0, /*duration_rounds=*/9);
            run_skill(b);

            CHECK_EQ(ctx.getPet(0).hp, kSelfHp,
                     "T5540.b 1895：固伤被免 → 自身不回血（实测 " +
                         std::to_string(ctx.getPet(0).hp) + "，应 5000）");
            const int foe_loss = kMaxHp - ctx.getPet(1).hp;
            CHECK(foe_loss >= 0 && foe_loss <= 30,
                  "T5540.b 1895：免粉后对手只吃本刀红伤（实测 " +
                      std::to_string(foe_loss) + "）");
            CHECK(ctx.has_active_abnormal_status(1, kFear),
                  "T5540.b 1895：未受到固定伤害 → 100% 申报害怕(6)");
        }
        std::cout << "  [T5540] 1895 固伤落地回血 / 免粉未落地害怕兜底 通过\n";
    }

    std::cout << "scenario_5540 PASS (J224 固定伤害未落地异常族)" << std::endl;
    return 0;
}
