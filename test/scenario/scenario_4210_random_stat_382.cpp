// 场景 4210 —— K265 随机属性+1族（382，soul_lib/k265_random_stat.cpp）
//（2026-10-21，worktree ../agents/k265；号段 4210-4219 → T4210-T4212 三块）
//
// 断言口径（382「每次使用攻击技能都能使自身随机属性+1」）：
//   - T4210 自己用**攻击技** → 六槽中**恰有一槽 +1**（总等级和 = 1）——随机槽
//     不可预测，断言"和"比断言"具体槽"稳健（K 系惯例：随机类效果用和/计数断言）。
//   - T4211 用**属性技** → 不触发（攻击技门）。
//   ⚠️ 载体选择坑（2026-10-21 实测）：属性技载体**自带自身能力增益**会污染
//     「等级和」断言——缩头 20001 的 side_effect=[4]（effect 4=改**自身**等级）
//     用它当属性技载体时 sum=1 恒成立 → 门被破坏也不红。故本单用**瞪眼 20004**
//     （effect 5=改**对手**等级，不动自身等级）。
//   - T4212 对手用技 → 印主不触发（自己使用门）。
//
// ⚠️ 场景构造口径：按 **DB args 原样**传 {5,1}（不补 {0,0} 前缀）——实现侧走
//   owned_int_args 原序读（K257 实现头注「行参读取路径」记档）。
#include <array>
#include <cstdlib>
#include <iostream>
#include <vector>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kJi = 10001;     // 撞击：攻击技载体
constexpr int kSuoTou = 20004; // 瞪眼：属性技载体（effect 5=改【对手】等级；
                                  // ⚠️ 勿用缩头 20001：其 side_effect=[4]（改【自身】
                                  // 等级）会污染等级和断言）

ElfPet mark_pet(int skill_id = kJi) {
    std::vector<int> args{5, 1};
    ElfPet p = make_pet(1, "印主", {skill_id, kJi, kJi, kJi, kJi},
                        /*speed=*/300, /*atk=*/1, /*hp=*/99999,
                        /*soul_mark=*/SoulMark(382, "K265 魂印", "",
                                               EffectArgs(std::move(args))));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_pet() {
    ElfPet p = make_pet(2, "对手", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/100, /*atk=*/1, /*hp=*/99999);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// 六槽等级和。
int level_sum(BattleContext& ctx, int side) {
    int sum = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        sum += ctx.ability_levels[side][i];
    }
    return sum;
}

// 恰好一槽 +1（其余为 0）。
bool exactly_one_slot_up(BattleContext& ctx, int side) {
    int ups = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        const int lv = ctx.ability_levels[side][i];
        if (lv == 1) {
            ++ups;
        } else if (lv != 0) {
            return false;
        }
    }
    return ups == 1;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T4210 自己用攻击技 → 恰有一槽 +1 ═══════════════════════════════════
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(level_sum(ctx, 0) == 0);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(level_sum(ctx, 0), 1,
                 "T4210 382：自己用攻击技 → 能力等级总和 +1（随机一槽）");
        CHECK(exactly_one_slot_up(ctx, 0),
              "T4210 382：随机属性 +1 落在**恰一槽**（其余槽为 0）");
        std::cout << "  [T4210] 攻击技触发 通过\n";
    }

    // ══ T4211 属性技 → 不触发 ══════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(mark_pet(kSuoTou)), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        const auto itd = ctx.plugin_storage.find(990266000 + 0);
        std::cout << "  [DIAG] used_attribute_seen="
                  << (itd == ctx.plugin_storage.end() ? -2 : std::any_cast<int>(itd->second)) << "\n";
        std::cout << "  [DIAG] after: sum=" << level_sum(ctx, 0)
                  << " roundChoice0=" << ctx.roundChoice[0][0]
                  << " idx=" << ctx.roundChoice[0][1] << "\n";
        const auto itw = ctx.plugin_storage.find(990269000 + 0);
        std::cout << "  [DIAG] actor_mismatch="
                  << (itw == ctx.plugin_storage.end() ? -2 : std::any_cast<int>(itw->second)) << "\n";
        CHECK_EQ(level_sum(ctx, 0), 0,
                 "T4211 382：用属性技 → 不触发（攻击技门）");
        std::cout << "  [T4211] 属性技门对照 通过\n";
    }

    // ══ T4212 对手用技 → 印主不触发 ════════════════════════════════════════
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(level_sum(ctx, 0), 0,
                 "T4212 382：对手用技 → 印主不触发（自己使用门）");
        std::cout << "  [T4212] 使用者门对照 通过\n";
    }

    std::cout << "scenario_4210 PASS (K265 随机属性+1族)" << std::endl;
    return 0;
}
