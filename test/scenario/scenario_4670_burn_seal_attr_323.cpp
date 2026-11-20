// 场景 4670 —— K311 烧伤门属性技失效族（323，soul_lib/k311_burn_seal_attr.cpp）
//（2026-10-21，worktree ../agents/k311；号段 4670-4679 → T4670-T4672 三块）
//
// 断言口径（323「若对手处于烧伤状态，则对手释放的所有属性技能失效」）：
//   - T4670 **封属生效**：对手处于烧伤 → 对手用属性技（高速移动）→ 属性技**无效**
//     （速度等级不变）；对照：对手无烧伤 → 属性技正常生效。
//   - T4671 **门闭对照**：对手**不**处于烧伤 → 属性技正常生效（不封）。
//   - T4672 **条件持续性**：对手烧伤期间每回合都封（对手的烧伤持续 → 后续回合仍封）；
//     对手烧伤**解除**后的下一回合 → 不再封（属性技恢复生效）。
//
// ⚠️ 场景构造口径：按 **DB args 原样**传 {2,2,4}（不补 {0,0} 前缀）——实现侧
//   **不消费任何下标**（异常码按 tips 字面取官方码 Burn=2；db[0] 位置存疑记档）。
// ⚠️ **"属性技被封"的行为观测**（scenario_039 Z10 同款）：高速移动（属性技）
//   生效会改自身速度等级 → 读 `ctx.ability_levels[1][4]`：
//   被封 → 等级不变（技能无效）；未被封 → 等级 +1。
//   ⚠️ 高速移动是**属性技**（SkillType::Attribute）——正是本单封的那一类。
// ⚠️ 印主是 0 方（带 323 魂印），对手是 1 方（被封方）。
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

constexpr int kGaoSu = 20017;   // 高速移动：属性技（生效则自身速度 +1）
constexpr int kPaDa = 10004;    // 拍打：攻击技（filler）
constexpr int kMaxHp = 99999;
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);   // 2
constexpr int kSpeedSlot = 4;

SoulMark make_mark() {
    // DB args 原样 {2,2,4}；实现不消费任何下标（按 tips 字面定值）。
    SoulMark sm(323, "K311 魂印", "test", EffectArgs(std::vector<int>{2, 2, 4}));
    sm.monster_id = 2666;
    return sm;
}

ElfPet mark_pet() {
    ElfPet p = make_pet(1, "神炎", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/300, /*atk=*/10, kMaxHp, make_mark());
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// 对手：槽 0 = 高速移动（属性技）。
ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kGaoSu, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/100, /*atk=*/10, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// 对手出一记属性技（高速移动）→ 返回对手速度等级。
int foe_uses_attr_skill(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    return b.ctx().ability_levels[1][kSpeedSlot];
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T4670 封属生效：对手烧伤 → 属性技无效 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        apply_anomaly(&ctx, 1, kBurn, /*duration_rounds=*/9, /*actor=*/0);
        REQUIRE(ctx.has_active_abnormal_status(1, kBurn));
        const int speed = foe_uses_attr_skill(b);
        CHECK_EQ(speed, 0,
                 "T4670 323：对手处于烧伤状态 → 对手释放的所有属性技能失效"
                 "（高速移动未生效，速度等级实测 " + std::to_string(speed) + "）");
        std::cout << "  [T4670] 封属生效 通过\n";
    }

    // ══ T4671 门闭对照：对手无烧伤 → 属性技正常生效 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        const int speed = foe_uses_attr_skill(b);
        CHECK(speed > 0,
              "T4671 323：对手不处于烧伤状态 → 属性技正常生效（速度等级实测 "
                  + std::to_string(speed) + "）");
        std::cout << "  [T4671] 门闭对照 通过\n";
    }

    // ══ T4672 条件持续性：烧伤期间每回合都封；烧伤到期后恢复 ══
    // ⚠️ 烧伤**到期**用 set_abnormal_status_end_round（battleContext.h:697）——
    //   没有"立即清除异常"的原语（clear_stat_drops 只清能力下降，不适用）。
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        apply_anomaly(&ctx, 1, kBurn, /*duration_rounds=*/9, /*actor=*/0);
        const int s1 = foe_uses_attr_skill(b);   // 烧伤中 → 被封
        const int s2 = foe_uses_attr_skill(b);   // 烧伤仍在（第 2 回合）→ 仍被封
        CHECK_EQ(s1, 0, "T4672 323：烧伤期间属性技被封（第 1 回合速度等级实测 "
                            + std::to_string(s1) + "）");
        CHECK_EQ(s2, 0, "T4672 323：烧伤持续期间每回合都封（第 2 回合速度等级实测 "
                            + std::to_string(s2) + "）");
        // 让烧伤本回合结束 → 下一回合不再封
        ctx.set_abnormal_status_end_round(1, kBurn, ctx.roundCount);
        const int s3 = foe_uses_attr_skill(b);   // 烧伤已到期 → 不再封
        CHECK(s3 > 0,
              "T4672 323：烧伤到期后的回合属性技恢复生效（速度等级实测 "
                  + std::to_string(s3) + "）");
        std::cout << "  [T4672] 条件持续性 通过\n";
    }

    std::cout << "scenario_4670 PASS (K311 烧伤门属性技失效族)" << std::endl;
    return 0;
}
