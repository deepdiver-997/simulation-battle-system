#include <utils/sbs_rand.h>
// 场景 940 —— K1 减伤族：百分比减伤（effect 3/254/260/280/317，REDUCE_PCT/REDUCE_EXTRA）
//
// 官方依据（effect_icon.tips，行 dump 见 soul_lib/k_guard_family.cpp 头）：
//   3   「受到的所有攻击伤害减少10%/15%/50%」——有参行 db[0]，无参行按 monster 查表
//   254 「受到所有伤害减少5%」——红伤 + **粉伤**双管线（措辞 vs 3 的"攻击伤害"）
//   260 「受到的所有攻击伤害减少10%」（鲍尔）；鲍尔特"10%闪避"行不在表 → 不减伤（记档）
//   280 「受到伤害减少15%+闪避5%」（db[0]/db[1]，只消费 db[0]）/「30%」
//   317 pct 在 db[1]（args[100,10]/[100,25]）
//
// 断言口径：对照组（无魂印）与带魂印组行动序列完全一致 → 引擎 85%~100% 浮动按同
// rand 序逐位对齐 → 减伤后体力损失 = 基线 × (100-pct)/100 **整数精确相等**。
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

// K 宏测试桩：id = 效果号（插件 SOUL_PROGRAM 注册键，构造即解析程序链路）。
SoulMark make_mark(int effect_id, std::vector<int> db_args, int monster_id) {
    SoulMark sm(effect_id, "K" + std::to_string(effect_id), "test",
                EffectArgs(std::move(db_args)));
    sm.monster_id = monster_id;
    return sm;
}

// 一回合：攻方出手、守方 NONE → 守方（0 方）体力损失（红伤）。
int probe_red_loss(int defender_id, const SoulMark& mark) {
    ScenarioBattle b(scenario::make_party(defender_pet(defender_id, mark)),
                     scenario::make_party(attacker_pet(2)));
    BattleContext& ctx = b.ctx();
    ctx.seed_rng(20260916);   // 对照局对齐：每次探针同一 rand 序
    b.start();
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

    const int base = probe_red_loss(1, SoulMark{});
    CHECK(base > 1000, "基线伤害量级合理（atk=5000 vs def=100）");

    // ══ A effect 3：有参行 db[50,0]（森林弓骑兵 50%）═════════════════════════
    CHECK_EQ(probe_red_loss(1, make_mark(3, {50, 0}, 2867)), base / 2,
             "A effect3 有参行 50% 减伤（整数精确）");
    // ══ B effect 3：无参行按 monster 查表（塔西姆 10%）══════════════════════
    CHECK_EQ(probe_red_loss(1, make_mark(3, {}, 2419)), base * 90 / 100,
             "B effect3 无参行 monster=2419 → 10%");
    CHECK_EQ(probe_red_loss(1, make_mark(3, {}, 2438)), base * 85 / 100,
             "B2 effect3 无参行 monster=2438（火舞天尊）→ 15%");
    // ══ C effect 254：红伤 5% + 粉伤 5%（"所有伤害"双管线）══════════════════
    CHECK_EQ(probe_red_loss(1, make_mark(254, {}, 2326)), base * 95 / 100,
             "C effect254 红伤 5%");
    {
        ScenarioBattle b(scenario::make_party(defender_pet(1, make_mark(254, {}, 2326))),
                         scenario::make_party(attacker_pet(2)));
        BattleContext& ctx = b.ctx();
        b.start();
        const int hp0 = ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp;
        deal_pink_damage(&ctx, 0, 200, DamageKind::FIXED, 1);
        CHECK_EQ(hp0 - ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp, 190,
                 "C2 effect254 粉伤 REDUCE_EXTRA 5%（200→190）");
    }
    // ══ D effect 317：pct 在 db[1]（[100,25] → 25%）═════════════════════════
    CHECK_EQ(probe_red_loss(1, make_mark(317, {100, 25}, 2710)), base * 75 / 100,
             "D effect317 db[1]=25 → 25%");
    // ══ E effect 280（monster 3077 → 30%）与 260（monster 2424 → 10%）═══════
    CHECK_EQ(probe_red_loss(1, make_mark(280, {30, 0}, 3077)), base * 70 / 100,
             "E effect280 有参行 db[0]=30 → 30%");
    CHECK_EQ(probe_red_loss(1, make_mark(280, {}, 3077)), base * 70 / 100,
             "E2 effect280 无参行 monster=3077 表查 30%");
    CHECK_EQ(probe_red_loss(1, make_mark(260, {}, 2424)), base * 90 / 100,
             "E3 effect260 monster=2424（鲍尔）→ 10%");
    // ══ F effect 260 鲍尔特闪避行：不在表 → 不注册减伤（记档口径）═══════════
    CHECK_EQ(probe_red_loss(1, make_mark(260, {}, 2425)), base,
             "F effect260 monster=2425（鲍尔特闪避行）无减伤");

    std::cout << "scenario_940 PASS (K1 减伤族：REDUCE_PCT/REDUCE_EXTRA 双管线 + 行≠宏查表)" << std::endl;
    return 0;
}
