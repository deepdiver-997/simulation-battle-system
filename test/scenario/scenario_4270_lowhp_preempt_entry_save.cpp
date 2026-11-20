#include <utils/sbs_rand.h>
// 场景 4270 —— K271 低体下回合先制+首战免死族（347，soul_lib/k271_lowhp_preempt_entry_save.cpp）
//（2026-10-21，worktree ../agents/k271；号段 4270-4279 → T4270-T4272 三块）
//
// 断言口径（印主速 100 vs 对手速 101 速度必输；印主用精准（属性技）避免蹭伤扰动
// 双方血量序，对手拍打 atk=1 挠印主）：
//   - T4270 子句①账本时序：R1 末血量低于对手 → 账本=1 → R2 FMR 先制+2 翻转 pr；
//     R3（对手血压到 500000 后不再记账）→ R4 无燃回归速度序（"下回合"一次性钉死）。
//   - T4271 子句②首战免死：登场回合（R1）致死 → 残留 1 点；R2 再致死（hp=1）→
//     非首回合不救 → 死亡成立（窗口=首回合时限钉死）。
//   - T4272 无印对照：致死直接死亡。
#include <cstdlib>
#include <iostream>
#include <vector>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;
constexpr int kZhunQue = 20138;   // 精准：属性技（无伤害，保持血量序可控）
constexpr int kHp = 1000000;
constexpr int kFatalHp = 60;      // < 红伤带下限 87 → 对手一刀必致死

SoulMark make_mark() {
    // DB args 原序 [2,0]：db[0]=2（先制量）、db[1]=0 废位。
    SoulMark sm(347, "K271 魂印", "test", EffectArgs(std::vector<int>{2, 0}));
    sm.monster_id = 2713;
    return sm;
}

ElfPet mark_pet(bool with_mark = true, int hp = kHp) {
    ElfPet p = with_mark
        ? make_pet(1, "神·哈迪斯", {kZhunQue, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                   /*speed=*/100, /*atk=*/1, hp, make_mark())
        : make_pet(1, "裸哈迪斯", {kZhunQue, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                   /*speed=*/100, /*atk=*/1, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet(int atk = 1) {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/101, atk, kHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

void round_ops(ScenarioBattle& b, int mark_skill) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, mark_skill);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T4270 子句①：回合末低体 → 次回合先制+2；再燃与衰减 ════════════════════
    {
        sbs_srand(4270);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        round_ops(b, 0);   // R1：印主精准（无伤害），对手挠 → 末盘 hp0 < hp1 记账
        CHECK_EQ(ctx.preemptive_right, PreemptiveRight::SEER_ROBOT_2,
                 "T4270 347：R1 尚无账本 → 速度序（对手 101 先）");
        round_ops(b, 0);   // R2：上轮账本燃 → 先制+2 翻转
        CHECK_EQ(ctx.preemptive_right, PreemptiveRight::SEER_ROBOT_1,
                 "T4270 347：R2 FMR 先制+2 翻转 pr（印主先）");
        ctx.getPet(1).hp = 500000;   // R3 前压对手血 → R3 末不再记账（血序反转）
        round_ops(b, 0);   // R3：仍燃 R2 账本；末盘 hp0(≈999995) < 500000 不成立 → 不记账
        CHECK_EQ(ctx.preemptive_right, PreemptiveRight::SEER_ROBOT_1,
                 "T4270 347：R3 仍燃 R2 账本（先制+2）");
        round_ops(b, 0);   // R4：无账可燃 → 速度序回归（"下回合"一次性）
        CHECK_EQ(ctx.preemptive_right, PreemptiveRight::SEER_ROBOT_2,
                 "T4270 347：R4 无燃回归速度序（下回合一次性语义）");
        std::cout << "  [T4270] 低体下回合先制 通过\n";
    }

    // ══ T4271 子句②：登场首回合免死；次回合不救 ═══════════════════════════════
    {
        sbs_srand(4271);
        ScenarioBattle b(make_party(mark_pet(/*with_mark=*/true, kFatalHp)),
                         make_party(foe_pet(/*atk=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        round_ops(b, 1);   // R1（登场回合）：致死 → 保留 1 点
        CHECK_EQ(ctx.getPet(0).hp, 1,
                 "T4271 347：登场首回合致死 → 残留 1 点（实测 hp=" +
                     std::to_string(ctx.getPet(0).hp) + "）");
        round_ops(b, 1);   // R2：再致死 → 非首回合不救
        CHECK_EQ(ctx.getPet(0).hp, 0,
                 "T4271 347：次回合致死 → 免死窗口已过，死亡成立（实测 hp=" +
                     std::to_string(ctx.getPet(0).hp) + "）");
        std::cout << "  [T4271] 首战免死窗口 通过\n";
    }

    // ══ T4272 无印对照：致死直接死亡 ═══════════════════════════════════════════
    {
        sbs_srand(4272);
        ScenarioBattle b(make_party(mark_pet(/*with_mark=*/false, kFatalHp)),
                         make_party(foe_pet(/*atk=*/300)));
        b.start();
        round_ops(b, 1);
        CHECK_EQ(b.ctx().getPet(0).hp, 0,
                 "T4272 347：无印对照 → 致死直接死亡");
        std::cout << "  [T4272] 无印对照 通过\n";
    }

    std::cout << "scenario_4270 PASS (K271 低体下回合先制+首战免死族)" << std::endl;
    return 0;
}
