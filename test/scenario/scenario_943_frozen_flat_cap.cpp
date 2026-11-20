#include <utils/sbs_rand.h>
// 场景 943 —— K1 冻伤条件点减（effect 337）+ 伤害锁定 CAP（effect 766 上半）
//
// 官方依据（effect_icon.tips）：
//   337 埃尔文达 args[2,5,100]「若对手处于冻伤状态，则自身本回合受到的伤害减少100点」
//       —— db[1]=5（官方异常码冻伤 AbnormalStatusId::Frostbite）、db[2]=点数。
//   766 格罗姆特 args[260,1,100,50]「自身体力低于对手时，受到的伤害不超过260」
//       —— 受伤当下比血量（文本无"回合开始时"前缀）；> 260 的伤害被锁到 260。
// 机制：766 = 引擎 CAP 阶段**首批消费者**；697"无视伤害限制"凭证穿锁伤（实现内守卫）。
//
// 断言口径：对照/带魂印两组 rand 对齐；CAP 用"伤害 >> 上限"把损失精确钉在上限值。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kYanFan = 10037;   // 燕返：威力 60 必中
constexpr int kGaoSu = 20017;    // 高速移动：属性技（无伤害噪声）

ElfPet attacker_pet(int id) {
    return scenario::make_pet(id, "进攻方", {kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                              /*speed=*/300, /*atk=*/5000, /*hp=*/100000);
}
ElfPet defender_pet(int id, SoulMark mark) {
    return scenario::make_pet(id, "守方", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                              /*speed=*/100, /*atk=*/10, /*hp=*/1000000, std::move(mark));
}

SoulMark make_mark(int effect_id, std::vector<int> db_args, int monster_id) {
    SoulMark sm(effect_id, "K" + std::to_string(effect_id), "test",
                EffectArgs(std::move(db_args)));
    sm.monster_id = monster_id;
    return sm;
}

int probe_loss(const SoulMark& mark, bool freeze_foe_first) {
    ScenarioBattle b(scenario::make_party(defender_pet(1, mark)),
                     scenario::make_party(attacker_pet(2)));
    BattleContext& ctx = b.ctx();
    sbs_srand(20260916);   // 对照局对齐：每次探针同一 rand 序
    b.start();
    if (freeze_foe_first) {
        ctx.apply_abnormal_status_for_rounds(1, static_cast<int>(AbnormalStatusId::Frostbite),
                                             /*durationRounds=*/9);
    }
    const int hp0 = ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp;
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    return hp0 - ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    const SoulMark no_mark{};
    const int base = probe_loss(no_mark, false);
    CHECK(base > 1000, "基线伤害量级合理");

    // ══ A effect 337：对手冻伤 → -100 ═══════════════════════════════════════
    CHECK_EQ(probe_loss(make_mark(337, {2, 5, 100}, 2683), true), base - 100,
             "A 对手冻伤（状态 5）→ 受伤 -100");
    // ══ B effect 337：对手没冻伤 → 全额 ═════════════════════════════════════
    CHECK_EQ(probe_loss(make_mark(337, {2, 5, 100}, 2683), false), base,
             "B 对手未冻伤 → 不减");

    // ══ C effect 766 格罗姆特：血量低于对手 → 锁 260 ════════════════════════
    {
        // 守方 hp=1000（< 攻方 100000）→ 基线伤害 >> 260 且不致死，损失恰好钉在 260。
        ElfPet low = scenario::make_pet(1, "格罗姆特", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                        /*speed=*/100, /*atk=*/10, /*hp=*/1000,
                                        make_mark(766, {260, 1, 100, 50}, 3107));
        ScenarioBattle b(scenario::make_party(low), scenario::make_party(attacker_pet(2)));
        BattleContext& ctx = b.ctx();
        b.start();
        const int hp0 = ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const ElfPet& after = ctx.seerRobot[0].elfPets[ctx.on_stage[0]];
        CHECK_EQ(hp0 - after.hp, 260, "C 血量低于对手 → 受伤锁定 260");
        CHECK_EQ(after.hp, 740, "C 锁伤后仍存活");
    }

    std::cout << "scenario_943 PASS (K1 冻伤点减 + 766 CAP 锁伤)" << std::endl;
    return 0;
}
