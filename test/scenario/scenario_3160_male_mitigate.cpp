// 场景 3160 —— K184 雄性来源减伤族（272，soul_lib/k184_male_mitigate.cpp）
//（2026-10-07，worktree ../agents/k184；号段 3160-3169 → T3160-T3162 三块）
//
// 断言口径（raw 台账恒等式，单局精确、免对照局）：
//   减伤票在 REDUCE_PCT 相位做 d.final×(100-pct)/100；raw_attack_damage 在管线前
//   记录 → 无其他票据时 loss == raw×(100-pct)/100 逐位成立（含整除截断）。
//   性别门三向：MALE → 减伤生效；FEMALE / NONE → 原伤（门闭）。
#include <array>
#include <iostream>
#include <vector>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <entities/elf-pet.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kJi = 10001;   // 撞击：物理攻击载体（无附加）

// 印主（被攻打方，speed 低让对手先手无所谓——NONE 收尾）。
template <int MarkId>
ElfPet mark_pet(const std::vector<int>& row) {
    std::vector<int> args = row;   // 方案B：按 DB args 原样构造
    ElfPet p = make_pet(1, "印主", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/100, /*atk=*/1, /*hp=*/99999,
                        /*soul_mark=*/SoulMark(MarkId, "K184 魂印", "",
                                               EffectArgs(std::move(args))));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// 攻击方（性别显式置位；harness make_pet 恒 NONE）。
ElfPet attacker_pet(Gender gender) {
    ElfPet p = make_pet(2, "攻方", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/300, /*atk=*/50, /*hp=*/99999);
    p.gender = gender;
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

void foe_hits(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    const std::vector<int> row_20{20, 0};
    const std::vector<int> row_25{25, 0};

    // ══ T3160 [20]：雄性来源 → 减 20%（raw×80/100 恒等）══
    {
        ScenarioBattle b(make_party(mark_pet<272>(row_20)),
                         make_party(attacker_pet(Gender::MALE)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int my0 = ctx.getPet(0).hp;
        foe_hits(b);
        const int loss = my0 - ctx.getPet(0).hp;
        CHECK_EQ(loss, ctx.ws.raw_attack_damage[1] * 80 / 100,
                 "T3160 272：雄性来源 → 攻伤 -20%（raw×80/100 逐位）");
        std::cout << "  [T3160] 雄性门减伤 通过\n";
    }

    // ══ T3161 [20]：雌性 / 无性别来源 → 门闭原伤 ══
    {
        ScenarioBattle b(make_party(mark_pet<272>(row_20)),
                         make_party(attacker_pet(Gender::FEMALE)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int my0 = ctx.getPet(0).hp;
        foe_hits(b);
        CHECK_EQ(my0 - ctx.getPet(0).hp, ctx.ws.raw_attack_damage[1],
                 "T3161 272：雌性来源 → 原伤（门闭）");
        std::cout << "  [T3161a] 雌性门闭 通过\n";
    }
    {
        ScenarioBattle b(make_party(mark_pet<272>(row_20)),
                         make_party(attacker_pet(Gender::NONE)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int my0 = ctx.getPet(0).hp;
        foe_hits(b);
        CHECK_EQ(my0 - ctx.getPet(0).hp, ctx.ws.raw_attack_damage[1],
                 "T3161 272：无性别来源 → 原伤（门闭）");
        std::cout << "  [T3161b] 无性别门闭 通过\n";
    }

    // ══ T3162 进化行 [25]：雄性来源 → 减 25%（db[0] 透传互锚）══
    {
        ScenarioBattle b(make_party(mark_pet<272>(row_25)),
                         make_party(attacker_pet(Gender::MALE)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int my0 = ctx.getPet(0).hp;
        foe_hits(b);
        CHECK_EQ(my0 - ctx.getPet(0).hp, ctx.ws.raw_attack_damage[1] * 75 / 100,
                 "T3162 272 进化行：雄性来源 → 攻伤 -25%");
        std::cout << "  [T3162] 进化行互锚 通过\n";
    }

    std::cout << "scenario_3160 PASS (K184 雄性来源减伤族)" << std::endl;
    return 0;
}
