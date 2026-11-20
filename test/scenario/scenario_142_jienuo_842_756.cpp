#include <utils/sbs_rand.h>
// 场景 142：杰诺 3408 技能组缺口 —— 效果 842（致命爆破 30956）+ 756（铁甲游骑兵 30960）
//（moves_lib/batch_a_skills.cpp，2026-09-27）
//
// 842 官方模板："若自身处于能力提升状态则造成的攻击伤害额外提升{0}%"
//（致命爆破 args=[50]，链=[485(消强回满·已有), 842]）。
// 实现：SKILL_EFFECT 判"自身处于能力提升状态"（6 槽正等级、962 同判据）→
// 写 ws.damage_add_extra_mul（**非通用增伤·乘法**通道——官方"额外提升"措辞）。
//
// 756 官方模板："命中后{0}%令对手{1}"（铁甲游骑兵 args=[50,19]：50% 瘫痪；
// 链=[756, 1048(对手异常→额外增伤100%·已有)]）。
// 实现：SKILL_EFFECT 掷点 → apply_anomaly 现代通道（{1} 是 battle_effects 异常码）。
//
// 断言组：
//   T1 842 主路径（确定性）：自提**速度**（不进物理伤害公式）→ 威力外伤害 ×1.5
//      （AMP_EXTRA 乘法通道）；无提升对照 = 基线。同种子 → 浮动系数一致 → 比值精确 1.5。
//      ★ 顺序敏感：提**攻击**不作为对照项——公式本体也吃攻击等级，会把 842 的份额淹没；
//      提速度把"条件成立"与"公式变化"解耦，比值断言只反映 842。
//   T2 756 主路径 + 链序：50% 瘫痪落地（种子 SB）→ 后继 1048 **当场看见新异常** →
//      伤害高于未触发对照（种子 SC）——756 在 side_effect 列里先于 1048，
//      数据层序=执行序（附录二），同一击内"附加异常→异常增伤"要能接上。
//
// 确定性：全技能强制必中、暴击 0。T2 rand 序：rand#1=756 的 50%、rand#2=伤害浮动。
#include <cmath>
#include <cstdlib>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kZhiming = 30956;   // 致命爆破：物攻 85、先 3、[485, 842]
constexpr int kTiejia = 30960;    // 铁甲游骑兵：物攻 150、先 1、[756, 1048]
constexpr int kHarmless = 10938;  // 本能一击：威力 0
constexpr int kCrippled = static_cast<int>(AbnormalStatusId::Crippled);  // 19 瘫痪

ElfPet jieno(std::array<int, 5> skills) {
    ElfPet p = scenario::make_pet(0, "杰诺", skills, /*speed=*/200, /*atk=*/300, /*hp=*/2000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe(int hp) {
    ElfPet p = scenario::make_pet(1, "对手", {kHarmless, kHarmless, kHarmless, kHarmless, kHarmless},
                                  /*speed=*/100, /*atk=*/100, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== T1 842：有提升 → ×1.5（AMP_EXTRA 乘法），无提升 → 基线 ==========
    {
        int d_boost = 0;
        {
            sbs_srand(777);
            ScenarioBattle b(scenario::make_party(jieno({kZhiming, kHarmless, kHarmless, kHarmless, kHarmless})),
                             scenario::make_party(foe(/*hp=*/100000)));
            b.start();
            BattleContext& ctx = b.ctx();
            REQUIRE(stat_change(&ctx, 0, /*速度=*/4, 1) == StatChangeResult::SUCCESS);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 致命爆破
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            d_boost = 100000 - ctx.getPet(1).hp;   // 对手掉血=该击最终伤害（AMP_EXTRA 在管线内，raw 台账看不到）
            CHECK_EQ(ctx.ability_level(0, 0), 0, "T1 攻击等级未动（排除公式噪声）");
        }
        int d_plain = 0;
        {
            sbs_srand(777);   // 同种子：伤害浮动系数一致
            ScenarioBattle b(scenario::make_party(jieno({kZhiming, kHarmless, kHarmless, kHarmless, kHarmless})),
                             scenario::make_party(foe(/*hp=*/100000)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            d_plain = 100000 - ctx.getPet(1).hp;
        }
        CHECK(d_boost > d_plain, "T1 核心：处于能力提升状态 → 伤害提升");
        const double ratio = static_cast<double>(d_boost) / d_plain;
        CHECK(std::fabs(ratio - 1.5) < 0.02,
              "T1 非通用增伤·乘法通道：比值应为 1.5（同种子浮动一致，实测除整舍入误差）");
        std::cout << "  [T1] 842 提升增伤 通过（基线 " << d_plain << " → 提升 " << d_boost
                  << "，比值 " << ratio << "）\n";
    }

    //========== T2 756：50% 瘫痪 + 链序（1048 当场看见新异常 → 增伤） ==========
    {
        // 触发分支：种子 SB 使 rand#1 < 50 → 瘫痪落地
        int d_hit = 0;
        bool paralyzed = false;
        {
            sbs_srand(4);   // 首掷=28 < 50 → 瘫痪落地（探针验证）
            ScenarioBattle b(scenario::make_party(jieno({kTiejia, kHarmless, kHarmless, kHarmless, kHarmless})),
                             scenario::make_party(foe(/*hp=*/100000)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 铁甲游骑兵
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            paralyzed = ctx.has_active_abnormal_status(1, kCrippled);
            d_hit = 100000 - ctx.getPet(1).hp;
            CHECK(paralyzed, "T2 核心：50% 瘫痪落地（种子触发分支）");
        }
        // 未触发对照：种子 SC 使 rand#1 >= 50 → 无瘫痪、无 1048 增伤
        int d_miss = 0;
        {
            sbs_srand(8);   // 首掷=56 ≥ 50 → 未触发（探针验证）
            ScenarioBattle b(scenario::make_party(jieno({kTiejia, kHarmless, kHarmless, kHarmless, kHarmless})),
                             scenario::make_party(foe(/*hp=*/100000)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(!ctx.has_active_abnormal_status(1, kCrippled), "T2 对照：未触发无瘫痪");
            d_miss = 100000 - ctx.getPet(1).hp;
        }
        // 1048（已有实现）："对手处于异常状态→额外提升100%"——756 在列首先施加，
        // 同一击的 1048 判定必须看见新异常（数据层序=执行序）→ 伤害 ≈2 倍。
        CHECK(d_hit > d_miss * 1.2,
              "T2 链序：756 新落的瘫痪被后继 1048 当场看见 → 额外增伤 100% 生效");
        std::cout << "  [T2] 756 瘫痪+1048 链序增伤 通过（未触发 " << d_miss
                  << " → 触发 " << d_hit << "）\n";
    }

    std::cout << "scenario_142 all passed" << std::endl;
    return 0;
}
