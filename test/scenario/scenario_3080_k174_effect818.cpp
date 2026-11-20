#include <utils/sbs_rand.h>
// 场景 3080 —— K174 effect 818（攻击命中回血族）：水中灵/塔琳-MKII
// "攻击技能命中后立即为自身恢复{db[0]}点体力"
//
// 断言口径：
//   A 命中即回：带伤出手命中 → 回 200（水中灵 [200,0]）。
//   B 第二行 args 驱动：塔琳-MKII [400,1] → 回 400。
//   C 属性技：无伤害管线（DETECT 不走）→ 不回。
//   D miss：accuracy=0（无 must_hit）→ 无伤害落地 → 不回。
//   E 上限封顶：贴近满血 → 回血封顶 max。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kYanFan = 10037;   // 燕返：威力 60 必中（直接攻击载体）
constexpr int kGaoSu = 20017;    // 高速移动：属性技

SoulMark make_mark(int monster_id, std::pair<int, int> db_args) {
    SoulMark sm(818, "K818", "test", EffectArgs({db_args.first, db_args.second}));
    sm.monster_id = monster_id;
    return sm;
}

// 0 方（带魂印，hp 压到 cur_hp）第 1 回合用 slot0 技能出手（1 方 NONE），返回出手后自身 hp。
int probe_self_hp(int monster_id, std::pair<int, int> db_args, int skill_slot, int cur_hp,
                  int accuracy = 100) {
    ElfPet holder = scenario::make_pet(1, "水中灵", {kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                       /*speed=*/300, /*atk=*/5000, /*hp=*/99999,
                                       make_mark(monster_id, db_args));
    for (Skills& s : holder.skills) {
        s.must_hit = 0;   // 燕返必中——accuracy 造 miss 需同清 must_hit（J108 记档）
        s.accuracy = 100;
    }
    if (accuracy != 100) {
        holder.skills[0].accuracy = accuracy;
    }
    ScenarioBattle b(scenario::make_party(holder),
                     scenario::make_party(scenario::make_pet(
                         2, "沙包", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                         /*speed=*/100, /*atk=*/10, /*hp=*/1000000)));
    BattleContext& ctx = b.ctx();
    sbs_srand(20261007);
    b.start();
    ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp = cur_hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, skill_slot);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
    return ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ A 命中即回 200（水中灵 3189 [200,0]）══════════════════════════════
    CHECK_EQ(probe_self_hp(3189, {200, 0}, 0, 5000), 5200, "A 命中 → 回 200");

    // ══ B 第二行 args 驱动（塔琳 4705 [400,1]）════════════════════════════
    CHECK_EQ(probe_self_hp(4705, {400, 1}, 0, 5000), 5400, "B 命中 → 回 400（args 驱动）");

    // ══ C 属性技 → 不回 ═══════════════════════════════════════════════════
    CHECK_EQ(probe_self_hp(3189, {200, 0}, 1, 5000), 5000, "C 属性技无伤害管线 → 不回");

    // ══ D miss → 不回 ═════════════════════════════════════════════════════
    CHECK_EQ(probe_self_hp(3189, {200, 0}, 0, 5000, /*accuracy=*/0), 5000,
             "D miss 无伤害落地 → 不回");

    // ══ E 上限封顶（贴满血只回差额）═══════════════════════════════════════
    CHECK_EQ(probe_self_hp(3189, {200, 0}, 0, 99850), 99999, "E 贴满血 → 封顶 max");

    std::cout << "scenario_3080 PASS (K174 818 攻击命中回血)" << std::endl;
    return 0;
}
