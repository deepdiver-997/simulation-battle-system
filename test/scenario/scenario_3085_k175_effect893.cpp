#include <utils/sbs_rand.h>
// 场景 3085 —— K175 effect 893（攻击后随机降属族）：德罗西尼/波波鱼
// "使用攻击技能后令对手随机{db[0]}项能力-{db[1]}"
//
// 断言与选槽无关（不依赖随机序列）：总降幅 = 项数×幅度、落在档的槽位数 = 项数。
//   A 德罗西尼 [2,2,1]：攻击后 → 总 -4、恰 2 槽各 -2。
//   B 波波鱼 [3,1,1]：攻击后 → 总 -3、恰 3 槽各 -1。
//   C 属性技 → 不降（used_attribute 门）。
//   D miss 也触发（「使用后」口径，与 818「命中后」族相反——E13 锚点先于可用性判定）。
//   E 对手出手 → 不降（own 门）。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kYanFan = 10037;   // 燕返：威力 60（直接攻击载体）
constexpr int kGaoSu = 20017;    // 高速移动：属性技
constexpr int kSlots = 6;        // 等级槽：0攻 1特攻 2防 3特防 4速 5命中

SoulMark make_mark(int monster_id, const int db[3]) {
    SoulMark sm(893, "K893", "test", EffectArgs({db[0], db[1], db[2]}));
    sm.monster_id = monster_id;
    return sm;
}

// 0 方带魂印第 1 回合出手（1 方 NONE），返回 1 方（对手）六槽等级快照。
std::array<int, kSlots> probe_opp_levels(int monster_id, const int db[3], int skill_slot,
                                         int accuracy = 100) {
    ElfPet holder = scenario::make_pet(1, "德罗西尼", {kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                       /*speed=*/300, /*atk=*/5000, /*hp=*/1000000,
                                       make_mark(monster_id, db));
    for (Skills& s : holder.skills) {
        s.must_hit = 0;   // accuracy 造 miss 需同清 must_hit（J108 记档）
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
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, skill_slot);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
    std::array<int, kSlots> levels{};
    int sum = 0;
    for (int i = 0; i < kSlots; ++i) {
        levels[i] = ctx.ability_levels[1][i];
        sum += levels[i];
    }
    levels[0] = sum;   // [0] 复用为总降幅
    return levels;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ A 德罗西尼 [2,2,1]：随机 2 项各 -2 ════════════════════════════════
    {
        const int db[3] = {2, 2, 1};
        const auto lv = probe_opp_levels(3280, db, 0);
        CHECK_EQ(lv[0], -4, "A 总降幅 -4（2项×-2）");
        int hit = 0;
        for (int i = 1; i < kSlots; ++i) {
            if (lv[i] == -2) {
                ++hit;
            } else {
                CHECK_EQ(lv[i], 0, "A 未选中槽保持 0");
            }
        }
        CHECK_EQ(hit, 2, "A 恰 2 槽 -2");
    }

    // ══ B 波波鱼 [3,1,1]：随机 3 项各 -1 ══════════════════════════════════
    {
        const int db[3] = {3, 1, 1};
        const auto lv = probe_opp_levels(4703, db, 0);
        CHECK_EQ(lv[0], -3, "B 总降幅 -3（3项×-1）");
        int hit = 0;
        for (int i = 1; i < kSlots; ++i) {
            if (lv[i] == -1) {
                ++hit;
            }
        }
        CHECK_EQ(hit, 3, "B 恰 3 槽 -1");
    }

    // ══ C 属性技 → 不降 ═══════════════════════════════════════════════════
    {
        const int db[3] = {2, 2, 1};
        const auto lv = probe_opp_levels(3280, db, 1);
        CHECK_EQ(lv[0], 0, "C 属性技（used_attribute）→ 不降");
    }

    // ══ D miss 也触发（「使用后」口径）════════════════════════════════════
    {
        const int db[3] = {2, 2, 1};
        const auto lv = probe_opp_levels(3280, db, 0, /*accuracy=*/0);
        CHECK_EQ(lv[0], -4, "D miss（accuracy0 无必中）→ 仍随机 2 项 -2");
    }

    // ══ E 对手出手 → 不降 ═════════════════════════════════════════════════
    {
        const int db[3] = {2, 2, 1};
        ElfPet holder = scenario::make_pet(
            1, "德罗西尼", {kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
            /*speed=*/100, /*atk=*/10, /*hp=*/1000000, make_mark(3280, db));
        ElfPet foe = scenario::make_pet(2, "对手", {kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                        /*speed=*/300, /*atk=*/10, /*hp=*/1000000);
        ScenarioBattle b(scenario::make_party(holder), scenario::make_party(foe));
        BattleContext& ctx = b.ctx();
        sbs_srand(20261007);
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        int sum = 0;
        for (int i = 0; i < kSlots; ++i) {
            sum += ctx.ability_levels[1][i];
        }
        CHECK_EQ(sum, 0, "E 对手出手 → 对手自身等级不动");
    }

    std::cout << "scenario_3085 PASS (K175 893 攻击后随机降属)" << std::endl;
    return 0;
}
