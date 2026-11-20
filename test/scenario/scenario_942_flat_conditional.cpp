#include <utils/sbs_rand.h>
// 场景 942 —— K1 条件点减：斩月双生（effect 329）"本回合未受伤 → 下回合受伤减少100点"
//
// 官方依据（effect_icon.tips，[2673,2693] 两行同文）：args[1,100]——db[1]=点数。
// 机制：TAKE_DAMAGE/TAKE_PINK_DAMAGE 记伤 → ROUND_START 评上回合 → REDUCE_FLAT 消费。
// ⚠️ 第 1 回合无"上回合"——翻旗节点带 saw_full_round 守卫（第 1 回合哪怕没受伤也不减）。
//
// 断言口径：三回合对照局——对照/带魂印两组行动序列逐位一致（rand 对齐），
// 带魂印组第 N 回合损失 = 对照组第 N 回合损失 ∓ 100（整数精确）。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>

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
    return scenario::make_pet(id, "斩月", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                              /*speed=*/100, /*atk=*/10, /*hp=*/1000000, std::move(mark));
}

SoulMark make_mark(int effect_id, std::vector<int> db_args) {
    SoulMark sm(effect_id, "K" + std::to_string(effect_id), "test",
                EffectArgs(std::move(db_args)));
    sm.monster_id = 2673;
    return sm;
}

// 跑 rounds 回合；attacker_moves[i] = 第 i+1 回合攻方是否出手。返回逐回合守方损失。
std::vector<int> probe_losses(int rounds, const std::vector<bool>& attacker_moves,
                              const SoulMark& mark) {
    ScenarioBattle b(scenario::make_party(defender_pet(1, mark)),
                     scenario::make_party(attacker_pet(2)));
    BattleContext& ctx = b.ctx();
    sbs_srand(20260916);   // 对照局对齐：每次探针同一 rand 序
    b.start();
    std::vector<int> losses;
    const int hp0 = ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp;
    int prev = hp0;
    for (int r = 0; r < rounds; ++r) {
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, attacker_moves[r] ? BattleFsm::ActionType::SELECT_SKILL
                                                : BattleFsm::ActionType::NONE,
                           attacker_moves[r] ? 0 : -1);
        b.drive();
        const int now = ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp;
        losses.push_back(prev - now);
        prev = now;
    }
    return losses;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    const std::vector<bool> hit_hit{true, true, true};
    const std::vector<bool> none_hit_hit{false, true, true};

    // ══ A 第 1 回合挨打 → 第 2 回合不减（受过伤不满足条件）══════════════════
    {
        const auto ctrl = probe_losses(2, hit_hit, SoulMark{});
        const auto marked = probe_losses(2, hit_hit, make_mark(329, {1, 100}));
        CHECK_EQ(marked[0], ctrl[0], "A 第 1 回合无减伤（saw_full_round 守卫）");
        CHECK_EQ(marked[1], ctrl[1], "A 第 1 回合受过伤 → 第 2 回合不减");
    }
    // ══ B 第 1 回合没挨打 → 第 2 回合 -100 ══════════════════════════════════
    {
        const auto ctrl = probe_losses(2, none_hit_hit, SoulMark{});
        const auto marked = probe_losses(2, none_hit_hit, make_mark(329, {1, 100}));
        CHECK_EQ(marked[0], 0, "B 第 1 回合攻守皆无动作");
        CHECK_EQ(marked[1], ctrl[1] - 100, "B 第 1 回合未受伤 → 第 2 回合 -100");
    }
    // ══ C 三回合窗口：r1 挨打 / r2 躲过 → r3 减 100（旗子逐回合重评）═════════
    {
        const std::vector<bool> hit_none_hit{true, false, true};
        const auto ctrl = probe_losses(3, hit_none_hit, SoulMark{});
        const auto marked = probe_losses(3, hit_none_hit, make_mark(329, {1, 100}));
        CHECK_EQ(marked[1], ctrl[1], "C 前置：r1 挨打 → r2 不减");
        CHECK_EQ(marked[2], ctrl[2] - 100, "C r2 未受伤 → r3 -100（窗口跨回合重评）");
    }

    std::cout << "scenario_942 PASS (K1 斩月：未受伤窗口旗 + REDUCE_FLAT + 首回合守卫)" << std::endl;
    return 0;
}
