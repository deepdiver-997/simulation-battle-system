// 场景 4140 —— K258 对手无提升→战阶磨血族（359，soul_lib/k258_noboost_grind.cpp）
//（2026-10-21，worktree ../agents/k258；号段 4140-4149 → T4140-T4142 三块）
//
// 断言口径（359「若对手能力不处于提升状态时，则每个战斗阶段结束时减少对手1/5最大HP
//              （boss无效）」）：
//   - T4140 真实行 [2,2,5]：对手无能力提升 → 回合末掉 1/5 最大体力（99999/5=19999，
//     **精确**——粉伤 FIXED 不浮动）。
//   - T4141 门闭：对手处于能力提升（任一槽 > 0）→ 回合末不掉血。
//   - T4142 能力**下降**也算"不处于提升"（负等级不是正等级）→ 仍掉 1/5。
//
// ⚠️ 场景构造口径：按 **DB args 原样**传 {2,2,5}（不补 {0,0} 前缀）——实现侧走
//   db_at 读 owned_int_args 原序，与真实对局语义一致（见 K257 实现头注「行参读取路径」）。
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

constexpr int kPaDa = 10004;    // 拍打：攻击载体（双方都用 NONE，避免额外红伤干扰）
constexpr int kMaxHp = 99999;
constexpr int kGrind = kMaxHp / 5;   // 19999

ElfPet mark_pet() {
    // 行参原序 [2,2,5]（db[2]=分母 5）。
    std::vector<int> args{2, 2, 5};
    ElfPet p = make_pet(1, "印主", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/300, /*atk=*/1, /*hp=*/kMaxHp,
                        /*soul_mark=*/SoulMark(359, "K258 魂印", "",
                                               EffectArgs(std::move(args))));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_pet() {
    ElfPet p = make_pet(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// 双方 NONE 出一整回合（到下一回合开始）。
void one_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T4140 真实行：对手无提升 → 回合末掉 1/5 最大体力 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        const int hp0 = ctx.getPet(1).hp;
        one_round(b);
        CHECK_EQ(ctx.getPet(1).hp, hp0 - kGrind,
                 "T4140 359：对手无能力提升 → 回合末减少 1/5 最大体力（19999）");
        std::cout << "  [T4140] 无提升磨血 通过\n";
    }

    // ══ T4141 门闭：对手处于能力提升 → 不掉血 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][0] = 1;   // 对手攻击 +1（处于能力提升状态）
        const int hp0 = ctx.getPet(1).hp;
        one_round(b);
        CHECK_EQ(ctx.getPet(1).hp, hp0,
                 "T4141 359：对手处于能力提升状态 → 不减少体力（门闭）");
        std::cout << "  [T4141] 门闭对照 通过\n";
    }

    // ══ T4142 能力下降（负等级）也算"不处于提升" → 仍掉 1/5 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][2] = -1;   // 对手防御 -1（下降，不是提升）
        const int hp0 = ctx.getPet(1).hp;
        one_round(b);
        CHECK_EQ(ctx.getPet(1).hp, hp0 - kGrind,
                 "T4142 359：对手能力下降（非提升）→ 仍减少 1/5 最大体力");
        std::cout << "  [T4142] 下降≠提升 对照 通过\n";
    }

    std::cout << "scenario_4140 PASS (K258 对手无提升→战阶磨血族)" << std::endl;
    return 0;
}
