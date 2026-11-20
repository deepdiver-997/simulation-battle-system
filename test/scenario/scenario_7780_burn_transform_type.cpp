// 场景 7780 —— 变型技 × 烧伤减半：烧伤减半读**静态 type**、物化时判（S17 百科清缴·07 章 §五#2）。
//
// 主张（07-暴击命中先制.md §五#2「变型技+烧伤减半组合未验证」）——引擎口径钉死：
//   烧伤(2)「攻击技能的威力会减少50%」的减半判据是 `skill.type != SkillType::Attribute`
//   （**静态 category**）且发生在威力**物化点**（ON_SKILL_HIT，battleFsm.cpp:498-511）；
//   变型技（天启星魂 2210）在 **SKILL_EFFECT 时点**（更晚）才把 type_view 改写为
//   物理/特殊、威力视图**赋值** 180（tianqi_skills.cpp effect_2210_transform）——
//   赋值发生在减半之后 → **烧伤不减半变型后的攻击威力**。
//
// 断言口径（全同种子对照，种子内 rand 流逐位对齐；max_hp 大额防 773 互换体力门）：
//   T1 变型+烧伤  vs  T2 变型无烧伤：伤害**精确相等**（减半被静态属性技门挡掉 +
//      2210 赋值 180 无视烧伤；rand 流一致——烧伤 tick=TruePercent 定值、减半短路不耗 rand）；
//   T3 拍打无烧伤  vs  T4 拍打+烧伤：静态攻击技照样减半——D_burn < D_plain 且
//      2×D_burn ≥ D_plain − 2（威力公式常数项 +2 使减半不恰为 1/2，带宽 ±2 容地板）。
//
// 全部 submit_operation + 有限步进（每步一个状态、到下一 CHOOSE/FINISHED 即停），
// 绝不 while(!FINISHED)。
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kTianwu   = 28810;  // 魂·天忤太虚：静态属性技（cat 4 / power 0 / must_hit=1），
                                  // 链 [976,935,773,2210,2218×2]——2210 星赐独处→物攻转化
constexpr int kPaiDa    = 10004;  // 拍打：静态物理攻击（cat 1 / power 40 / acc 100 / crit 0）
constexpr int kGaoSu    = 20017;  // 高速移动：属性技（防守方无红伤输出）
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);            // 2
constexpr int kStar = static_cast<int>(AbnormalStatusId::StarBlessing);    // 33 星赐
constexpr int kHpAtk = 99999;     // 攻方大额 HP：烧伤 1/8 tick 后仍高于守方（防 773 互换体力）
constexpr int kHpDef = 80000;     // 守方 HP 低于攻方 tick 后残量 → 935 束缚门开（无害）/773 门闭

// 跑一场"攻方用 index 号技能打一回合"，返回守方掉血量（攻方速度 300 恒先手）。
int run_one_round(int skill_index, bool with_burn, bool with_star, unsigned seed) {
    sbs_srand(seed);
    auto atk = make_pet(0, "攻方", {kTianwu, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/300, /*hp=*/kHpAtk);
    auto def = make_pet(1, "守方", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                        /*speed=*/100, /*atk=*/100, /*hp=*/kHpDef);
    ScenarioBattle battle(make_party(atk), make_party(def));
    BattleContext& ctx = battle.ctx();
    battle.start();  // 停在 CHOOSE（roundCount 尚未推进）
    if (with_burn) {
        REQUIRE(apply_anomaly(&ctx, /*target=*/0, kBurn, /*rounds=*/9, /*actor=*/1)
                == ApplyAnomalyResult::SUCCESS);
    }
    if (with_star) {
        REQUIRE(apply_anomaly(&ctx, 0, kStar, 9, 0) == ApplyAnomalyResult::SUCCESS);
    }
    const int hp_before = battle.pet(1).hp;
    battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, skill_index);
    battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    // 有限步进：跑到下一输入点 / FINISHED（单回合状态数有限，防御性上限）
    ctx.debug_step_mode = true;
    int steps = 0;
    while (ctx.currentState != State::OPERATION_CHOOSE_SKILL_MEDICAMENT
           && ctx.currentState != State::FINISHED) {
        battle.step();
        if (++steps > 200) break;
    }
    ctx.debug_step_mode = false;
    const int drop = hp_before - battle.pet(1).hp;
    CHECK(drop > 0, "守方应掉血 >0（攻方先手、属性技防守方无输出面）");
    return drop;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== T1/T2 变型技：烧伤不减半转化后的攻击（同种子伤害精确相等）==========
    constexpr unsigned kSeed = 20261105;
    const int d_burn  = run_one_round(/*skill_index=*/0, /*with_burn=*/true,  /*with_star=*/true, kSeed);
    const int d_plain = run_one_round(/*skill_index=*/0, /*with_burn=*/false, /*with_star=*/true, kSeed);
    CHECK_EQ(d_burn, d_plain,
             "T1/T2 变型技伤害应精确相等（减半判据=静态 type=属性技被挡 + 2210 赋值 180 无视烧伤）");
    std::cout << "  [T1/T2] 变型技 烧伤=" << d_burn << " 无烧伤=" << d_plain
              << "（精确相等，烧伤未减半）通过" << std::endl;

    //========== T3/T4 静态攻击技：烧伤照样减半（同种子，非精确 1/2 的带宽）==========
    const int d_c = run_one_round(/*skill_index=*/1, /*with_burn=*/false, /*with_star=*/false, kSeed);
    const int d_d = run_one_round(/*skill_index=*/1, /*with_burn=*/true,  /*with_star=*/false, kSeed);
    CHECK(d_d < d_c, "T4 拍打+烧伤应严格低于 T3 无烧伤（减半生效）");
    CHECK(2 * d_d >= d_c - 2, "T4 减半幅度应约 1/2（公式常数项 +2 使减半不恰为一半，带宽容地板）");
    std::cout << "  [T3/T4] 拍打 无烧伤=" << d_c << " 烧伤=" << d_d
              << "（减半生效，2×D_burn≥D_plain−2）通过" << std::endl;

    std::cout << "scenario_7780 全部通过" << std::endl;
    return 0;
}
