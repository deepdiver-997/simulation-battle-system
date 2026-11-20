// 场景 4760 —— K320 双防和比例固伤族（694，soul_lib/k320_defsum_fixed.cpp）
//（2026-10-21，worktree ../agents/k320；号段 4760-4769 → T4760-T4762 三块）
//
// 断言口径（694「攻击附加对手双防值之和30%的固定伤害」）：
//   - T4760 **附加固伤生效**：印主用攻击技打中对手 → 对手掉血 = 红伤 + 附加
//     (对手双防和 × 30%)。对手双防由 make_pet 固定（防御 100 + 特防 100 = 200）
//     → 附加 = 200×30% = 60。
//   - T4761 **攻击伤害时点门**：印主**不**出手（对手打）→ 对手不掉血（不构成
//     "自己攻击"）——证明触发面是"自己的攻击"而非"任何伤害"。
//   - T4762 **门闭对照**：印主用**属性技**（非攻击技）→ 不附加（tips"攻击"字面）。
//
// ⚠️ 场景构造口径：按 **DB args 原样**传 {1,30}（不补 {0,0} 前缀）——实现侧走
//   owned_int_args 原序读（K257 起的夜跑口径，见实现头注「行参读取路径」）。
// ⚠️ **对手双防和 = 200**：make_pet 的 numerical_base 固定 DEFENSE=100、
//   SPECIAL_DEFENSE=100 → def_sum = 200 → 附加固伤 = 200×30% = 60。
//   实测用 make_pet 的默认防御（100）——⚠️ 速查 §五："make_pet 的 atk 同时设物攻+
//   特攻，**防御恒 100**"；特防同为 100（numerical_base 默认）。
// ⚠️ 对手掉血 = **红伤带 + 附加固伤**：红伤有 85%~100% 浮动，故用带判据
//   （K233 844 场景 3770 同款口径）。印主 atk 压低（=10）使红伤带尽量小，
//   让"附加 60"成为可分辨的主体。
#include <cstdlib>
#include <iostream>
#include <vector>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kJi = 10001;    // 撞击：物理攻击技
constexpr int kSuoTou = 20004;  // 瞪眼：属性技
constexpr int kMaxHp = 99999;
constexpr int kDefSum = 200;   // 对手 防御100 + 特防100
constexpr int kAttach = 60;    // 200 × 30%

SoulMark make_mark() {
    // DB args 原样 [1,30]：db[1]=30（"30%"，tips 自证）；db[0]=1 记档。
    SoulMark sm(694, "K320 魂印", "test", EffectArgs(std::vector<int>{1, 30}));
    sm.monster_id = 2986;
    return sm;
}

ElfPet mark_pet(int skill0 = kJi) {
    ElfPet p = make_pet(1, "奇拉美", {skill0, kJi, kJi, kJi, kJi},
                        /*speed=*/300, /*atk=*/10, kMaxHp, make_mark());
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/100, /*atk=*/10, kMaxHp);
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

    // ══ T4760 附加固伤生效：对手掉血 ≈ 红伤带 + 60 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        const int foe_hp0 = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        const int foe_delta = foe_hp0 - ctx.getPet(1).hp;
        // 红伤带（印主 atk=10、拍打威力 40、对手防 100 → 红伤极小）+ 附加 60。
        // 判据：掉血量落在 [40, 100]（含红伤浮动 + 附加 60 的合理窗）。
        CHECK(foe_delta >= 40 && foe_delta <= 100,
              "T4760 694：攻击 → 附加对手双防和 200×30% = 60 点固定伤害（对手掉血实测 "
                  + std::to_string(foe_delta) + "，含红伤浮动）");
        std::cout << "  [T4760] 附加固伤生效 通过 (foe_delta=" << foe_delta << ")\n";
    }

    // ══ T4761 不是自己的攻击 → 不附加 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        const int foe_hp0 = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);  // 对手打印主
        b.drive();
        const int foe_delta = foe_hp0 - ctx.getPet(1).hp;
        CHECK_EQ(foe_delta, 0,
                 "T4761 694：不是自己攻击造成的伤害 → 不附加固伤（对手掉血实测 "
                     + std::to_string(foe_delta) + "）");
        std::cout << "  [T4761] 非己方攻击不附加 通过\n";
    }

    // ══ T4762 属性技 → 不附加（tips"攻击"字面）══
    {
        ScenarioBattle b(make_party(mark_pet(/*skill0=*/kSuoTou)), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        const int foe_hp0 = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        const int foe_delta = foe_hp0 - ctx.getPet(1).hp;
        // ⚠️ 属性技（瞪眼）**不造成伤害** → 对手不掉血；本块证明"攻击"字面：
        //   若实现误把属性技也算攻击，固伤会附加 → 对手会掉 60。
        CHECK(foe_delta == 0,
              "T4762 694：用属性技 → 不附加固定伤害（对手掉血实测 "
                  + std::to_string(foe_delta) + "）");
        std::cout << "  [T4762] 属性技不附加 通过\n";
    }

    std::cout << "scenario_4760 PASS (K320 双防和比例固伤族)" << std::endl;
    return 0;
}
