// 场景 961 —— effect 795 累计口径裁定回归（2026-10-01 人工裁定：场上有效、下场失效）
//（载体：怒气勃发 31060 [795] args=[20,100]，纯递增载体）
//
// 裁定：795「每次使用…额外提升{0}%，最高{1}%」的累计计数**场上有效、下场失效**——
// owner 再上场即清零（EVENT_ENTER_STAGE watcher，懒注册一次；下场期间无法用技，
// "再上场清零" ≡ "下场清零"）。例外备忘：2472（蔑天行·戾杀）官方明写"本场"→ 常驻。
//
// 断言组：
//   T1 场上累计：R1 使用 → damage_add_pct=+20（uses=1）；R2 再用 → +40（uses=2，
//      ws 每回合 reset，增量来自 uses 计数爬坡——累计生效的直接证据）。
//   T2 下场清零：R3 换下场 → R4 换回 → R5 再用 → +20（uses 从 1 起，非 3）。
#include <array>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kNuQi = 31060;       // 怒气勃发：[795] args=[20,100]
constexpr int kPaDa = 10004;       // 拍打 filler

ElfPet self_pet(const std::array<int, 5>& skills, int speed, int atk, int hp) {
    ElfPet p = make_pet(1, "己方", skills, speed, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet(const std::array<int, 5>& skills, int speed, int atk, int hp) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

void round_with_self_skill(ScenarioBattle& b, int self_slot) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, self_slot);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

void round_self_switch(ScenarioBattle& b, int to_slot) {
    b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, to_slot);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    ScenarioBattle b(make_party(self_pet({kNuQi, kPaDa, kPaDa, kPaDa, kPaDa},
                                          /*speed=*/300, /*atk=*/1, /*hp=*/5000)),
                     make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                        /*speed=*/100, /*atk=*/1, /*hp=*/100000)));
    b.start();
    BattleContext& ctx = b.ctx();

    round_with_self_skill(b, 0);   // R1：uses=1 → +20
    CHECK_EQ(ctx.ws.damage_add_pct[0], 20, "T1 核心：R1 首用 +20（uses=1）");
    round_with_self_skill(b, 0);   // R2：uses=2 → +40（ws 已 reset，增量来自计数爬坡）
    CHECK_EQ(ctx.ws.damage_add_pct[0], 40, "T1 核心：R2 再用 +40（uses=2，累计生效）");
    std::cout << "  [T1] 795 场上累计 通过\n";

    round_self_switch(b, 1);       // R3：换下场（slot 1 的同模精灵顶上）
    round_self_switch(b, 0);       // R4：换回（再上场 → watcher 清 uses）
    CHECK(!ctx.plugin_storage.count(990179510),
          "T2 结构：再上场 → uses 计数已清（watcher 生效）");
    round_with_self_skill(b, 0);   // R5：再用 → uses=1 → +20（若未清会 +60）
    CHECK_EQ(ctx.ws.damage_add_pct[0], 20,
             "T2 核心：下场清零 → 重计 +20（下场保留则会是 +60）");
    std::cout << "  [T2] 795 下场清零 通过\n";

    std::cout << "scenario_961 PASS (effect 795 场上有效/下场失效 裁定回归)" << std::endl;
    return 0;
}
