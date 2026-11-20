#include <utils/sbs_rand.h>
// 场景 5230 —— K2536 库拉塔克·吸取能力族（2152，soul_lib/k2536_kulatake_drain.cpp）
//（2026-10-21，worktree feat/k2536；号段 5230-5239 → T5230-T5233 四块）
//
// 断言口径（2152「自身攻击技能命中时吸取对手4项能力1级，自身属性技能命中时吸取对手
//             第五技能2点PP值；|战斗阶段结束时，若本回合自身未受到攻击伤害则下回合
//             所有技能先制+1，若自身受到了攻击伤害则下回合使用技能后附加300点固定
//             伤害（boss无效）」）：
//   - T5230 子句①门开：攻击技命中 → 对手 4 槽各 -1、自己同样 4 槽各 +1（**随机
//     槽不可预测** → 断言"降/升的槽集合相同 + 数量为 4"，不钉具体槽）。
//   - T5231 子句①**免弱对照**（用户口径 2026-10-21）：对手挂 STAT_DROP 免疫 →
//     stat_drop 返回 IMMUNE → **对手等级一点不动、自己也不加提升**。
//   - T5232 子句①**满级槽对照**（用户口径 2026-10-21）：自己某槽已 +6（满上限）
//     → 随机仍会随机到它、仍调用 stat_change → 原语内钳 AT_CAP → 该槽**不升**，
//     其余 3 槽正常升（"看起来没发生什么"= 不报错、不卡流程）。
//   - T5233 子句②：属性技命中 → 对手**第五技能** PP -2（精确）+ 其余技能 PP 不动。
//
// ⚠️ 场景构造口径：按 **DB args 原样**传 {4,1,2,1,300,1}（不补 {0,0} 占位）——
//   实现侧走 db_at 读 owned_int_args 原序（方案 B，K257 实现头注记档）。
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <set>
#include <vector>

#include <abnormal-system/abnormal-types.h>
#include <effects/rule_center.h>
#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kJi = 10001;     // 撞击：攻击技载体
constexpr int kSuoTou = 20004; // 瞪眼：属性技载体（effect 5=改对手等级，不动自身）
constexpr int kMaxHp = 99999;

// 印主：可带/不带魂印。
ElfPet mark_pet(bool with_mark) {
    SoulMark sm;
    if (with_mark) {
        sm = SoulMark(2152, "K2536 魂印", "",
                      EffectArgs(std::vector<int>{4, 1, 2, 1, 300, 1}));
    }
    ElfPet p = make_pet(1, "印主", {kJi, kSuoTou, kJi, kJi, kJi},
                        /*speed=*/300, /*atk=*/300, /*hp=*/kMaxHp, std::move(sm));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_pet() {
    ElfPet p = make_pet(2, "对手", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/100, /*atk=*/1, /*hp=*/kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// 某方等级为负的槽集合。
std::set<int> negative_slots(BattleContext& ctx, int side) {
    std::set<int> out;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx.ability_levels[side][i] < 0) {
            out.insert(i);
        }
    }
    return out;
}

// 某方等级为正的槽集合。
std::set<int> positive_slots(BattleContext& ctx, int side) {
    std::set<int> out;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx.ability_levels[side][i] > 0) {
            out.insert(i);
        }
    }
    return out;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5230 子句①：攻击技命中 → 对手 4 槽 -1 / 自己同样 4 槽 +1 ══════════
    {
        ScenarioBattle b(make_party(mark_pet(true)), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        sbs_srand(20261021);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        const std::set<int> foe_neg = negative_slots(ctx, 1);
        const std::set<int> self_pos = positive_slots(ctx, 0);
        CHECK_EQ(static_cast<int>(foe_neg.size()), 4,
                 "T5230 2152：攻击技命中 → 对手 4 项能力 -1");
        CHECK(foe_neg == self_pos,
              "T5230 2152：自己 +1 的槽集合与对手 -1 的槽集合**相同**（同一批随机槽）");
        std::cout << "  [T5230] 随机吸取 通过（槽数=" << foe_neg.size() << "）\n";
    }

    // ══ T5231 子句①免弱对照：对手免弱 → 不降、也不给自己加 ═════════════════
    {
        ScenarioBattle b(make_party(mark_pet(true)), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        // 对手挂 STAT_DROP 免疫（免弱）——覆盖整场。
        ctx.grant_immunity(1, ImmunityType::STAT_DROP, coverage_all(),
                           /*anomaly_mask=*/0, /*duration_rounds=*/0,
                           /*source_id=*/0, /*soul_immunity=*/false,
                           EffectScope::ON_STAGE, /*counts=*/0);
        sbs_srand(20261021);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK(negative_slots(ctx, 1).empty(),
              "T5231 2152：对手免弱 → 对手能力等级一点不动");
        CHECK(positive_slots(ctx, 0).empty(),
              "T5231 2152：对手免弱 → **自己也不附加提升**（用户口径 2026-10-21）");
        std::cout << "  [T5231] 免弱对照 通过\n";
    }

    // ══ T5232 子句①满级槽对照：自己某槽已 +6 → 该槽不升、其余正常升 ═══════
    {
        ScenarioBattle b(make_party(mark_pet(true)), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        // ⚠️ 满级槽必须设在**本次随机确实会选中的槽**上，否则本用例测不到
        //   "随机到满级槽"这条路径（seed 20261021 选中的是槽 1/2/3/5 → 取槽 1）。
        //   满级 +6 = 等级上限；随机到它时 stat_change 内钳 AT_CAP → 该槽不升。
        for (int i = 0; i < 6; ++i) {
            stat_change(&ctx, 0, 1, 1);
        }
        REQUIRE(ctx.ability_levels[0][1] == 6);
        sbs_srand(20261021);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        const std::set<int> foe_neg = negative_slots(ctx, 1);
        const std::set<int> self_pos = positive_slots(ctx, 0);
        // 审计槽：本次随机选中的槽掩码（bit i = 槽 i 被选中）。
        const auto it = ctx.plugin_storage.find(992152400 + 0);
        const int pick_mask = (it == ctx.plugin_storage.end()) ? -1 : std::any_cast<int>(it->second);
        CHECK(pick_mask >= 0, "T5232 2152：审计槽记录了本次随机选中的槽");
        CHECK((pick_mask & 2) != 0,
              "T5232 2152：本次随机确实选中了满级槽(槽1)——否则本用例无效");
        CHECK_EQ(ctx.ability_levels[0][1], 6,
                 "T5232 2152：自己满级槽仍为 6（原语内钳 AT_CAP，不越界、不报错）");
        CHECK_EQ(static_cast<int>(foe_neg.size()), 4,
                 "T5232 2152：对手仍被随机降 4 项（**不因自己满级而跳过**）");
        // 满级槽没升（仍 6，仍是正槽）→ 自己正槽 = 满级槽 + 其余 3 个被选中的
        //   非满级槽 = 4 个；被选中的 4 槽里只有 3 槽真的升了。
        CHECK_EQ(static_cast<int>(self_pos.size()), 4,
                 "T5232 2152：满级槽不升（仍 +6）、其余 3 槽升到 +1");
        CHECK_EQ(static_cast<int>(foe_neg.size()), 4,
                 "T5232 2152：对手被降的仍是 4 项（含满级对应那项——对手无满级概念）");
        std::cout << "  [T5232] 满级槽对照（pick_mask=" << pick_mask
                  << " 自己正槽数=" << self_pos.size() << "）\n";
    }

    // ══ T5233 子句②：属性技命中 → 对手第五技能 PP -2 ════════════════════════
    {
        ScenarioBattle b(make_party(mark_pet(true)), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        const int pp5_before = ctx.getPet(1).skills[4].pp;
        const int pp1_before = ctx.getPet(1).skills[0].pp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // 槽 1 = 瞪头(属性技)
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(ctx.getPet(1).skills[4].pp, pp5_before - 2,
                 "T5233 2152：属性技命中 → 对手第五技能 PP -2");
        CHECK_EQ(ctx.getPet(1).skills[0].pp, pp1_before,
                 "T5233 2152：只吸第五技能 → 其余技能 PP 不动");
        std::cout << "  [T5233] 吸取第五技能PP 通过（pp " << pp5_before
                  << "→" << ctx.getPet(1).skills[4].pp << "）\n";
    }

    std::cout << "scenario_5230 PASS (K2536 库拉塔克·吸取能力族)" << std::endl;
    return 0;
}
