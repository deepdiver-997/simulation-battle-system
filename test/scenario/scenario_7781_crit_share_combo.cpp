// 场景 7781 —— 多段（连击）共用同一次判暴 + 判暴短路零 rand（S17 百科清缴·07 章 §五#5）。
//
// 主张（07-暴击命中先制.md §五#5「多段/变威力共用同一次暴击结果」）——引擎口径钉死：
//   连击是**一次伤害公式 ×N**（battleFsm.cpp:722-737：本系→克制→浮动→暴击抗性→
//   暴击系数→连击次数，×N 排最后、乘在 base 上），不是 N 次独立结算；判暴在
//   query_usage ①.5 掷一次、结算端只消费 `crit_happened`（不再重掷）。
//   官方同源：L101《赛学扩展4—连击计算（新）》公式尾乘 ×连击次数。
//
// 断言口径（同种子对照；rand 消耗逐位对齐是"×2 精确比"成立的前提）：
//   T1/T2 种子 s：rate=0（rate>0 短路、不耗 rand）vs rate=16（rate≥100 短路、不耗 rand）
//     ——两次运行 rand 流完全一致（命中 roll 1 + 连击 roll 1 + 浮动 1，判暴均 0 耗）
//     → D16 == 2×D0 **精确成立**（暴击系数 200 整段放大后 ×N）。
//   T3/T4 换种子 s2 重复 → 比例关系跨种子稳定。
//   若判暴不是"一次使用一次结果"（例如逐段重掷、或 rate=16 走 rand 掷点），rand 流
//   错位 → 浮动/连击取值漂移 → ×2 精确比破裂 → 转红。
//
// 载体：花瓣舞 10044（cat 2 特攻 / power 50 / acc 100 / crit 0 / 纯链 [31]），
// case 31 连击区间 [2,3]（skills.cpp combo_arg_indices 注释记档）。防守方全属性技。
// 全部 submit_operation + 有限步进，绝不 while(!FINISHED)。
#include <iostream>

#include <entities/pet_factory.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kHuaBan = 10044;   // 花瓣舞：特攻 50 / acc 100 / 纯链 [31] 连击 [2,3]
constexpr int kPaiDa  = 10004;   // 拍打：物攻 40（铺位，不被选中）
constexpr int kGaoSu  = 20017;   // 高速移动：属性技（防守方无红伤输出）
constexpr int kMaxHp = 99999;

// 跑一场"攻方用 skill_index 号技能打一回合"，返回守方掉血量（攻方先手）。
int run_one_round(int crit_sixteenths, unsigned seed) {
    sbs_srand(seed);
    auto atk = make_pet(0, "攻方", {kHuaBan, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/300, /*hp=*/kMaxHp);
    auto def = make_pet(1, "守方", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                        /*speed=*/100, /*atk=*/100, /*hp=*/kMaxHp);
    if (crit_sixteenths > 0) {
        set_crit_sixteenths(atk, crit_sixteenths);  // 16 = rate≥100 必暴短路（不耗 rand）
    }
    ScenarioBattle battle(make_party(atk), make_party(def));
    BattleContext& ctx = battle.ctx();
    battle.start();
    const int hp_before = battle.pet(1).hp;
    battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    ctx.debug_step_mode = true;
    int steps = 0;
    while (ctx.currentState != State::OPERATION_CHOOSE_SKILL_MEDICAMENT
           && ctx.currentState != State::FINISHED) {
        battle.step();
        if (++steps > 200) break;
    }
    ctx.debug_step_mode = false;
    const int drop = hp_before - battle.pet(1).hp;
    CHECK(drop > 0, "守方应掉血 >0（连击技命中、非克制归零形态）");
    return drop;
}

void check_pair(unsigned seed) {
    const int d0  = run_one_round(/*crit=*/0,  seed);
    const int d16 = run_one_round(/*crit=*/16, seed);
    CHECK_EQ(d16, 2 * d0,
             "必暴伤害应精确等于 2×无暴伤害（判暴一次共用整段 ×N、rate=16 短路不耗 rand）");
    std::cout << "  [seed " << seed << "] D0=" << d0 << " D16=" << d16
              << "（D16==2×D0 精确）通过" << std::endl;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    check_pair(/*seed=*/20261105);
    check_pair(/*seed=*/777);

    std::cout << "scenario_7781 全部通过" << std::endl;
    return 0;
}
