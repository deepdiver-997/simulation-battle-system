// 场景 6130 —— J280 闪避窗伤害延长族（1832，moves_lib/j_dodge_window_extend.cpp）
//（2026-10-22，worktree feat/jbatch；号段 6130-6139 → T6130.a-b）
//
// 断言口径（坑集 §0/§2：门闭用绝对量，不用"与对照局相等"）：
//   - 载体 冰莹洁光破 35993 [881,1167,1832]|[1,100,400,2]（1832=[1,100,400,2]；
//     881/1167 在无能力下降时均为空操作）。
//   - 伤害锚（速查 §五）：atk300/def100 的 raw 带 [87,102] @power40 → power150 时
//     atk600 ≈ [644,758]（>400 门外）、atk50 ≈ [55,65]（<400 门内）。
//   - T6130.a 高伤（atk600）：回合 0 自身闪避→hp 不变、对手掉血 ∈[600,800]、
//     审计 damage=掉血、extend=1；回合 1/2 闪避延续（hp 不变）；**回合 3 恢复掉血**
//     ——负向控制锁"恰好延长 2 回合"。
//   - T6130.b 低伤（atk50）：技能伤害 < 阈值 → extend=0；回合 0 闪避、回合 1 即掉血。
#include <any>
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kCarrier = 35993;   // 冰莹洁光破：1832=[1,100,400,2]
constexpr int kFiller  = 10004;   // 拍打
constexpr int kMaxHp   = 99999;

constexpr int kExtendAuditBase = 991832000;
constexpr int kDamageAuditBase = 991832100;

int audit_value(BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return -2;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : -2;
}

ElfPet make_self(int atk, int accuracy, int must_hit) {
    ElfPet p = make_pet(1, "己方", {kCarrier, kFiller, kFiller, kFiller, kFiller},
                        /*speed=*/300, atk, kMaxHp);
    p.skills[0].must_hit = must_hit;
    p.skills[0].accuracy = accuracy;
    return p;
}

ElfPet make_foe() {
    ElfPet p = make_pet(2, "对手", {kFiller, kFiller, kFiller, kFiller, kFiller},
                        /*speed=*/100, /*atk=*/1, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;   // 对手攻击可被闪避
        s.accuracy = 100;
    }
    return p;
}

// self_slot = -1 → 本回合己方 NONE（不出手）；否则用该槽技能。
void one_round(ScenarioBattle& b, int self_slot) {
    if (self_slot >= 0) {
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, self_slot);
    } else {
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    }
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T6130.a 高伤触发延长：闪避 0/1/2 回合，回合 3 恢复掉血 ══
    {
        ScenarioBattle b(make_party(make_self(/*atk=*/600, /*accuracy=*/100, /*must_hit=*/1)),
                         make_party(make_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        const int self_hp0 = ctx.getPet(0).hp;
        const int foe_hp0 = ctx.getPet(1).hp;
        one_round(b, 0);
        const int dealt = foe_hp0 - ctx.getPet(1).hp;
        CHECK(dealt >= 600 && dealt <= 800,
              "T6130.a 1832：本技能红伤 ∈[600,800]（实测 " + std::to_string(dealt) + "）");
        CHECK_EQ(audit_value(ctx, kDamageAuditBase + 0), dealt,
                 "T6130.a 1832：EVENT_TAKE_DAMAGE 观测值 = 实际掉血");
        CHECK_EQ(audit_value(ctx, kExtendAuditBase + 0), 1,
                 "T6130.a 1832：伤害 > 400 → 延长触发");
        CHECK_EQ(ctx.getPet(0).hp, self_hp0, "T6130.a 回合0 闪避 → 自身 hp 不变");

        one_round(b, -1);
        CHECK_EQ(ctx.getPet(0).hp, self_hp0, "T6130.a 回合1 延长闪避 → hp 不变");
        one_round(b, -1);
        CHECK_EQ(ctx.getPet(0).hp, self_hp0, "T6130.a 回合2 延长闪避 → hp 不变");
        one_round(b, -1);
        CHECK(ctx.getPet(0).hp < self_hp0, "T6130.a 回合3 窗过期 → 恢复掉血");
        // 延长观察者只认"本次技能"：后续对手攻击（actor=对手）不得再次延长/覆盖观测。
        CHECK_EQ(audit_value(ctx, kExtendAuditBase + 0), 1,
                 "T6130.a 1832：延长整场只触发一次");
        CHECK_EQ(audit_value(ctx, kDamageAuditBase + 0), dealt,
                 "T6130.a 1832：后续受击不覆盖本技能红伤观测值");
        std::cout << "  [T6130.a] 高伤延长 通过 (dealt=" << dealt
                  << ", 回合3 hp=" << ctx.getPet(0).hp << ")\n";
    }

    // ══ T6130.b 低伤不延长：回合 0 闪避、回合 1 即掉血 ══
    {
        ScenarioBattle b(make_party(make_self(/*atk=*/50, /*accuracy=*/100, /*must_hit=*/1)),
                         make_party(make_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        const int self_hp0 = ctx.getPet(0).hp;
        const int foe_hp0 = ctx.getPet(1).hp;
        one_round(b, 0);
        const int dealt = foe_hp0 - ctx.getPet(1).hp;
        CHECK(dealt > 0 && dealt < 200,
              "T6130.b 1832：本技能红伤 < 400（实测 " + std::to_string(dealt) + "）");
        CHECK_EQ(audit_value(ctx, kExtendAuditBase + 0), 0,
                 "T6130.b 1832：伤害未过阈 → 不延长");
        CHECK_EQ(ctx.getPet(0).hp, self_hp0, "T6130.b 回合0 闪避 → hp 不变");
        one_round(b, -1);
        CHECK(ctx.getPet(0).hp < self_hp0, "T6130.b 回合1 无延长 → 恢复掉血");
        std::cout << "  [T6130.b] 低伤不延长 通过 (dealt=" << dealt << ")\n";
    }

    std::cout << "scenario_6130 PASS (J280 闪避窗伤害延长族)" << std::endl;
    return 0;
}
