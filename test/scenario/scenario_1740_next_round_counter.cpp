// 场景 1740 —— J84 下回合反击族（2 宏，moves_lib/j_next_round_counter.cpp）
//（2026-10-03，worktree ../agents/j84；号段 1740-1749 → T1740-T1741 两块）
//
// 断言口径：
//   - 560（天地乖离剑 [120] 物150 纯链）：R1 载体攻击（"其他"=当次不附加，对手掉血
//     仅红伤~3）；R2 我方拍打（攻击技）→ +120 精确（fixed 无浮动，窄带含拍打~2）；
//     R2 改用属性技（高速移动 20017）对照 → 不附加（对手掉血 0）；R3 窗过期 → 不附加；
//   - 715（阴冥雷障 [2,1] 物140 纯链）：R1 挂窗；R2 对手攻击命中我方（双方出招模式）
//     → 一次性触发 → 对手恰好 2 槽 -1（负槽计数=2、负槽总和=-2，洗牌不赖具体槽）；
//     对照 = 对手不动（我方未受击）→ 槽位全 0；当回合受击不算（R1 内打不触发）。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa  = 10004;  // 拍打：物40
constexpr int kTianDi = 17304;  // 天地乖离剑 [560]|[120]：物150（下回合附加载体）
constexpr int kYinMing = 35763; // 阴冥雷障 [715]|[2,1]：物140（受击反击载体）
constexpr int kGaoSu  = 20017;  // 高速移动 [4]|[4,100,2]：属（攻击技门对照）
constexpr int kMaxHp  = 5000;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_self(std::array<int, 5> skills) {
    ElfPet p = make_pet(1, "己方", skills, 300, /*atk=*/1, kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe(std::array<int, 5> skills) {
    ElfPet p = make_pet(2, "对手", skills, 100, /*atk=*/1, 99999);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

int foe_negative_slots(const BattleContext& ctx) {
    int cnt = 0, sum = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx.ability_levels[1][i] < 0) {
            ++cnt;
            sum += ctx.ability_levels[1][i];
        }
    }
    return cnt * 100 + (-sum);   // 编码：负槽计数与总和一起回传
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1740 560 下回合攻击技附加固伤（天地乖离剑 [120]）══
    {
        // a) R1 载体（当次不附加）；R2 拍打 → +120
        {
            ScenarioBattle b(
                make_party(mk_self({kTianDi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 载体
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int h1 = ctx.getPet(1).hp;
            CHECK(99999 - h1 <= 5,
                  "T1740.a 560：载体当次不附加（'其他'，实测掉 " +
                      std::to_string(99999 - h1) + "）");
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 拍打
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int r2_loss = h1 - ctx.getPet(1).hp;
            CHECK(r2_loss >= 121 && r2_loss <= 123,
                  "T1740.a 560：下回合攻击技 → +120 固伤（实测 " +
                      std::to_string(r2_loss) + "）");
        }
        // b) 攻击技门：R2 用属性技 → 不附加
        {
            ScenarioBattle b(
                make_party(mk_self({kTianDi, kGaoSu, kPaiDa, kPaiDa, kPaiDa})),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 载体
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int h1 = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 属性技
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(h1 - ctx.getPet(1).hp, 0,
                  "T1740.b 560 对照：属性技不附加（实测掉 " +
                      std::to_string(h1 - ctx.getPet(1).hp) + "）");
        }
        // c) 窗过期：R3 拍打 → 不附加（窗仅 1 回合）
        {
            ScenarioBattle b(
                make_party(mk_self({kTianDi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2（窗内空转）
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int h2 = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R3 拍打
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int r3_loss = h2 - ctx.getPet(1).hp;
            CHECK(r3_loss <= 5,
                  "T1740.c 560：窗过期不附加（实测掉 " + std::to_string(r3_loss) + "）");
        }
        std::cout << "  [T1740] 560 下回合附加 通过\n";
    }

    // ══ T1741 715 受击→对手随机 2 槽 -1（阴冥雷障 [2,1]）══
    {
        // a) R1 挂窗（载体攻击，自己不受击）；R2 对手攻击命中 → 触发
        {
            ScenarioBattle b(
                make_party(mk_self({kYinMing, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 载体
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            // R2：我方拍打 + 对手拍打（我方受击 → 触发）
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            const int code = foe_negative_slots(ctx);
            const int cnt = code / 100, sum = code % 100;
            CHECK(cnt == 2 && sum == 2,
                  "T1741.a 715：受击 → 对手随机 2 槽 -1（负槽 " + std::to_string(cnt) +
                      " 个，总降 " + std::to_string(sum) + "）");
        }
        // b) 对照：我方未受击 → 不触发
        {
            ScenarioBattle b(
                make_party(mk_self({kYinMing, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 载体
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 拍打
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int code = foe_negative_slots(ctx);
            CHECK(code == 0,
                  "T1741.b 715 对照：未受击不触发（编码 " + std::to_string(code) + "）");
        }
        std::cout << "  [T1741] 715 受击反击 通过\n";
    }

    std::cout << "scenario_1740 PASS (J84 下回合反击族 560/715)" << std::endl;
    return 0;
}
