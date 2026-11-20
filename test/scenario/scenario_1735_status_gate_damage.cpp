// 场景 1735 —— J86 状态门伤害/回血族（2 宏，moves_lib/j_status_gate_damage.cpp）
//（2026-10-03，worktree ../agents/j86；号段 1730-1739 → 场景号 1735（1730 已被 J83
//  场景占用，同段错开）→ T1735 两块）
//
// 断言口径：
//   - 552 = 对手预挂麻痹（apply_anomaly 直授，953 同款；麻痹无自伤 tick 不污染承伤带）→ 破冰狂击怒 19684 [100,300]
//     100% 档附加 300 固伤：对手掉 = 特150 红伤带 [216,254] + 300 = [516,554]；
//     负例（对手干净）= 红伤带 [216,254]；100% 档无掷点随机；
//   - 194 = 寒光冰魄 13177 [6,5,3]：台账 raw 直读（AFTER_ACTION，内核 case 194 归位）——
//     基础回 raw/6；对手冻伤(5) → 升档 raw/3（断言按 raw 换算精确值，无带宽）；
//   - 冻伤(5)=Crippled（J85 勘误：勿用 Paralysis=0 麻痹）；冻伤无行动封锁、自伤 tick
//     不碰对手 hp 断言面；
//   - 载体双纯链（[552]/[194] 单效果）。
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

constexpr int kPaiDa  = 10004;   // 拍打
constexpr int kPoBing = 19684;   // 破冰狂击怒[552]|[100,300]：特150（对手异常→100%附加300固伤）
constexpr int kHanGuang = 13177; // 寒光冰魄[194]|[6,5,3]：特150（1/6 回血·对手冻伤升档 1/3）

constexpr int kFrost = static_cast<int>(AbnormalStatusId::Frostbite);  // 冻伤=5（Frostbite；Crippled=19 是瘫痪——J85 同款坑二度）
constexpr int kParal = static_cast<int>(AbnormalStatusId::Paralysis); // 麻痹=0（无自伤 tick，J16 选型；对手 NONE 不受行动封锁影响）
constexpr int kMaxHp = 99999;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills) {
    ElfPet p = make_pet(1, "己方", skills, 300, 200, kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills) {
    ElfPet p = make_pet(2, "对手", skills, 100, 1, kMaxHp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1735 552 对手异常 → 100% 附加 300 固伤（破冰狂击怒 [100,300]）══
    {
        // a) 正例：对手中毒 → 红伤带 + 300
        {
            ScenarioBattle b(make_party(self_pet({kPoBing, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)apply_anomaly(&ctx, 1, kParal, /*duration_rounds=*/9, /*actor=*/0);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int foe_loss = kMaxHp - ctx.getPet(1).hp;
            CHECK(foe_loss >= 516 && foe_loss <= 554,
                  "T1735.a 552：异常门 → 红伤带+300（实测 " + std::to_string(foe_loss) +
                      "，带 [516,554]）");
        }
        // b) 负例：对手干净 → 仅红伤带
        {
            ScenarioBattle b(make_party(self_pet({kPoBing, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int foe_loss = kMaxHp - ctx.getPet(1).hp;
            CHECK(foe_loss >= 216 && foe_loss <= 254,
                  "T1735.b 552 门：对手干净仅红伤（实测 " + std::to_string(foe_loss) +
                      "，带 [216,254]）");
        }
        std::cout << "  [T1735] 552 异常门固伤 通过\n";
    }

    // ══ T1735b 194 台账回血·对手冻伤升档（寒光冰魄 [6,5,3]）══
    {
        // a) 升档：对手冻伤 → 回 raw/3（按台账精确断言）
        {
            ScenarioBattle b(make_party(self_pet({kHanGuang, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)apply_anomaly(&ctx, 1, kFrost, /*duration_rounds=*/9, /*actor=*/0);
            ctx.getPet(0).hp = kMaxHp / 2;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int raw = ctx.ws.raw_attack_damage[0];
            const int gain = ctx.getPet(0).hp - kMaxHp / 2;
            CHECK(raw > 0, "T1735b.a 194：台账已落地（raw=" + std::to_string(raw) + "）");
            CHECK_EQ(gain, raw / 3,
                     "T1735b.a 194：对手冻伤升档回 raw/3（实测回 " + std::to_string(gain) +
                         "，期望 " + std::to_string(raw / 3) + "）");
        }
        // b) 基础档：对手干净 → 回 raw/6
        {
            ScenarioBattle b(make_party(self_pet({kHanGuang, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = kMaxHp / 2;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int raw = ctx.ws.raw_attack_damage[0];
            const int gain = ctx.getPet(0).hp - kMaxHp / 2;
            CHECK_EQ(gain, raw / 6,
                     "T1735b.b 194：基础档回 raw/6（实测回 " + std::to_string(gain) +
                         "，期望 " + std::to_string(raw / 6) + "）");
        }
        std::cout << "  [T1735b] 194 台账回血升档 通过\n";
    }

    std::cout << "scenario_1735 PASS (J86 状态门伤害/回血族 552/194)" << std::endl;
    return 0;
}
