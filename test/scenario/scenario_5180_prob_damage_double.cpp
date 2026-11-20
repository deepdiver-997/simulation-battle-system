// 场景 5180 —— J196 概率伤害翻倍族（1481，moves_lib/j_prob_damage_double.cpp）
//（2026-10-22，worktree ../agents/j196；号段 5180-5189）
//
// 断言口径：
//   - 1481（未来·音速火拳 33423 链 [880,1481]|[3,50]，物 85）：
//     · (a) 对手预先 -1 攻（能力下降）→ 概率翻倍 50→100，必翻倍：
//       审计 = 1000+100；伤害带 = 基础 [184,216] ×2 = [368,432]。
//     · (b) 对手无降态 → 概率 50：审计 = rolled*1000+50；rolled=1 时伤害 [368,432]，
//       rolled=0 时 [184,216]（按审计自洽断言，不猜随机分支）。
//   - 基础带推导：(0.84×300/100×85+2)=216.2 ×[217,255]/255 → [184,216]（速查 §五锚）。
//   - 对照：不进管线（不施放载体）时同技能不存在——本族用 (b) 的 rolled=0 分支兼作
//     "未翻倍"实证；伤害带绝对量断言。
#include <iostream>
#include <any>

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
constexpr int kYinSu   = 33423;  // [880,1481]|[3,50]（50% 伤害翻倍；对手降态翻倍概率）
constexpr int kAuditKey = 990148100;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_attacker() {
    ElfPet p = make_pet(1, "己方", {kYinSu, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/300, /*hp=*/5000);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/99999);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

int audit(const BattleContext& ctx) {
    const auto it = ctx.plugin_storage.find(kAuditKey);
    if (it == ctx.plugin_storage.end()) {
        return -1;
    }
    if (const int* p = std::any_cast<int>(&it->second)) {
        return *p;
    }
    return -1;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5180 1481 概率伤害翻倍 ══
    {
        {   // (a) 对手 -1 攻 → 概率 50→100 → 必翻倍
            ScenarioBattle b(make_party(mk_attacker()), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)stat_drop(&ctx, /*target=*/1, /*stat=*/0, /*amount=*/1);
            const int before = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = before - ctx.getPet(1).hp;
            CHECK_EQ(audit(ctx), 1100,
                     "T5180.a 1481：对手降态 → 概率翻倍 100 且必命中（审计 " +
                         std::to_string(audit(ctx)) + "）");
            CHECK(loss >= 368 && loss <= 432,
                  "T5180.a 1481：伤害翻倍到 [368,432]（实测 " + std::to_string(loss) + "）");
        }
        {   // (b) 对手无降态 → 概率 50；按审计自洽断言伤害带
            ScenarioBattle b(make_party(mk_attacker()), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            const int before = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = before - ctx.getPet(1).hp;
            const int a = audit(ctx);
            const int p = a % 1000;
            const bool rolled = (a / 1000) == 1;
            CHECK_EQ(p, 50, "T5180.b 1481：无降态 → 申报概率 50（实测 " +
                                std::to_string(p) + "）");
            if (rolled) {
                CHECK(loss >= 368 && loss <= 432,
                      "T5180.b 1481：本次命中翻倍 → [368,432]（实测 " +
                          std::to_string(loss) + "）");
            } else {
                CHECK(loss >= 184 && loss <= 216,
                      "T5180.b 1481：本次未命中 → 基础 [184,216]（实测 " +
                          std::to_string(loss) + "）");
            }
        }
        std::cout << "  [T5180] 1481 概率伤害翻倍 + 降态概率翻倍 通过\n";
    }

    std::cout << "scenario_5180 PASS (J196 概率伤害翻倍族)" << std::endl;
    return 0;
}
