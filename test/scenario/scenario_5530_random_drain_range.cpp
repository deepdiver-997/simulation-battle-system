// 场景 5530 —— J223 随机吸取区间+强化门族（1694，moves_lib/j_random_drain_range.cpp）
//（2026-10-22，worktree feat/jbatch；号段 5530-5539）
//
// 断言口径（审计槽精确分档 + 绝对体力双面；随机值本身由审计槽给出，断言区间与血量恒等式）：
//   - 1694 模板：随机吸取对手{0}-{1}点体力，若自身处于能力提升状态则效果转变为{2}-{3}点。
//   - a) 门闭（28981 盘根错节，属性技；链兄弟 191/576 只挂窗、1769 未注册惰性）：
//       审计门=0，抽取 ∈ [200,300]（{0}-{1} 档），对手 hp == 5000-抽取、自身 hp ==
//       2000+抽取（按实际扣减回血）。
//   - b) 门开（29029 妙手童话，属性技；链首 946 全属性+{0} 先给强化）：审计门=1，
//       抽取 ∈ [300,400]（{2}-{3} 档），血量恒等式同上。
//   - c) 对照（瞪眼 20004，无 1694）：审计双键缺席，双方体力不变。
//   - 半血构造避免回血被 max 钳住（坑集 §4/§9）；起始 2000 << max 99999。
#include <any>
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa      = 10004;  // 拍打：物 40（填充刀）
constexpr int kDengYan    = 20004;  // 瞪眼：属性技（对照，无 1694）
constexpr int kPanGen     = 28981;  // 盘根错节：属性技 [191,576,1769,1694]|1694=[200,300,300,350]
constexpr int kMiaoShou   = 29029;  // 妙手童话：属性技 [946,1694,524]|1694=[200,300,300,400]
constexpr int kAuditBase  = 990169400;  // + owner*2：抽取点数；+1：强化门
constexpr int kSelfMax    = 99999;
constexpr int kFoeMax     = 5000;
constexpr int kSelfStart  = 2000;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(int skill0) {
    ElfPet p = make_pet(1, "己方", {skill0, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, /*hp=*/kSelfMax);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/kFoeMax);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

int audit_or(BattleContext& ctx, int key, int fallback) {
    auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return fallback;
    }
    return std::any_cast<int>(it->second);
}

// 1694 单次使用：自身半血、对手满血，返回审计抽取点数。
// 恒等式在调用方断言（对手掉血 = 抽取；自身回血 = 实际扣减 = 抽取）。
struct DrainRun {
    int amount = 0;
    int gate = -1;
};

DrainRun run_1694(int skill0) {
    ScenarioBattle b(make_party(self_pet(skill0)), make_party(foe_pet()));
    b.start();
    BattleContext& ctx = b.ctx();
    ctx.getPet(0).hp = kSelfStart;
    const int self_before = ctx.getPet(0).hp;
    const int foe_before = ctx.getPet(1).hp;
    DrainRun r;
    r.amount = audit_or(ctx, kAuditBase, 0);
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
    r.amount = audit_or(ctx, kAuditBase, -1);
    r.gate = audit_or(ctx, kAuditBase + 1, -1);
    CHECK_EQ(ctx.getPet(1).hp, foe_before - r.amount,
             "1694：对手掉血 == 审计抽取量（foe hp 实测 " +
                 std::to_string(ctx.getPet(1).hp) + "，应 " + std::to_string(foe_before - r.amount) +
                 "；审计 " + std::to_string(r.amount) + "）");
    CHECK_EQ(ctx.getPet(0).hp, self_before + r.amount,
             "1694：自身回血 == 实际扣减（self hp 实测 " +
                 std::to_string(ctx.getPet(0).hp) + "，应 " +
                 std::to_string(self_before + r.amount) + "）");
    return r;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5530 1694 随机吸取区间 + 强化门 ══
    {
        {   // (a) 门闭：28981，无强化 → [200,300] 档
            DrainRun r = run_1694(kPanGen);
            CHECK_EQ(r.gate, 0, "T5530.a 门闭：审计门应为 0（实测 " + std::to_string(r.gate) + "）");
            CHECK(r.amount >= 200 && r.amount <= 300,
                  "T5530.a 门闭：抽取应在 [200,300]（实测 " + std::to_string(r.amount) + "）");
            std::cout << "  [T5530.a] 门闭抽取=" << r.amount << "（[200,300]）\n";
        }
        {   // (b) 门开：29029，链首 946 已给全属性+ → [300,400] 档
            ScenarioBattle b(make_party(self_pet(kMiaoShou)), make_party(foe_pet()));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = kSelfStart;
            const int self_before = ctx.getPet(0).hp;
            const int foe_before = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int amount = audit_or(ctx, kAuditBase, -1);
            const int gate = audit_or(ctx, kAuditBase + 1, -1);
            // 前提侧证：946 确实先给了自身强化（门开不是空判）。
            int boost_slots = 0;
            for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
                if (ctx.ability_levels[0][i] > 0) {
                    ++boost_slots;
                }
            }
            CHECK(boost_slots > 0,
                  "T5530.b 前提：946 先手强化确实落（正等级槽数 " +
                      std::to_string(boost_slots) + "）");
            CHECK_EQ(gate, 1, "T5530.b 门开：审计门应为 1（实测 " + std::to_string(gate) + "）");
            CHECK(amount >= 300 && amount <= 400,
                  "T5530.b 门开：抽取应在 [300,400]（实测 " + std::to_string(amount) + "）");
            std::cout << "  [T5530.b] 门开抽取=" << amount << "（[300,400]），强化槽数="
                      << boost_slots << "\n";
            CHECK_EQ(ctx.getPet(1).hp, foe_before - amount,
                     "T5530.b 对手掉血 == 抽取（实测 " +
                         std::to_string(ctx.getPet(1).hp) + "，应 " +
                         std::to_string(foe_before - amount) + "）");
            CHECK_EQ(ctx.getPet(0).hp, self_before + amount,
                     "T5530.b 自身回血 == 抽取（实测 " +
                         std::to_string(ctx.getPet(0).hp) + "，应 " +
                         std::to_string(self_before + amount) + "）");
        }
        {   // (c) 对照：无 1694 → 审计双键缺席、双方体力不变
            ScenarioBattle b(make_party(self_pet(kDengYan)), make_party(foe_pet()));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = kSelfStart;
            const int self_before = ctx.getPet(0).hp;
            const int foe_before = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.plugin_storage.count(kAuditBase) + ctx.plugin_storage.count(kAuditBase + 1),
                     0,
                     "T5530.c 对照：1694 审计键缺席");
            CHECK_EQ(ctx.getPet(0).hp, self_before,
                     "T5530.c 对照：自身体力不变（实测 " +
                         std::to_string(ctx.getPet(0).hp) + "）");
            CHECK_EQ(ctx.getPet(1).hp, foe_before,
                     "T5530.c 对照：对手体力不变（实测 " +
                         std::to_string(ctx.getPet(1).hp) + "）");
        }
        std::cout << "  [T5530] 1694 门闭/门开/对照 通过\n";
    }

    std::cout << "scenario_5530 PASS (J223 随机吸取区间+强化门族)" << std::endl;
    return 0;
}
