// 场景 038：回合数窗口 —— 先出手 / 后出手（官方通用规则，用户 2026-09-13 定）。
//
// 规则（见 docs/02-效果系统/官方机制理解与引擎缺口对照.md §五.0 + docs/03-数据与数据库/认证数据层.md §3.1）：
//   InRounds（"{N}回合内"，默认）：先出手 → R..R+N-1（本回合就结算一次）
//                                后出手 → R+1..R+N（顺延一回合，N≥2）
//                                N==1（"本回合内"）**不顺延**（挪到下一回合反而是错的）
//   NextRounds（"下{N}回合"）：先/后出手**都** R+1..R+N（本回合本就不算，后出手不再额外顺延）
//   家族声明 = 认证数据层 custom_effect_overrides(override_type='window')；
//   折算入口 = BattleContext::round_effect_start_round(owner, duration, kind)。
//
// 验证点：
//   W1 真值表：五种组合的窗口起点（含 N==1 不顺延、NextRounds 不叠加顺延）
//   W2 端到端：认证层声明 843（下N回合先制）后，窗口起点 = R+1 且真的覆盖 N 回合
//   W3 端到端：未声明（默认 in_rounds）的效果，先出手起点 = R
#include <cassert>
#include <iostream>

#include <effects/effect.h>
#include <effects/effect_meta.h>
#include <entities/pet_factory.h>
#include <fsm/state.h>

#include "scenario_harness.h"

namespace {

constexpr int kZhuangJi = 10001;
constexpr int kWuJi = 20355;   // 狙击战术（本场景只用来凑一个普通技能回合）

// 用 dodge 魂印只为构造两个可区分的回合（本场景不关心其效果）。
scenario::ScenarioBattle make_battle(int speed0, int speed1) {
    auto p0 = scenario::make_pet(0, "先手方", {kWuJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                 speed0, /*atk=*/100, /*hp=*/3000);
    auto p1 = scenario::make_pet(1, "后手方", {kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                 speed1, /*atk=*/100, /*hp=*/3000);
    return scenario::ScenarioBattle(scenario::make_party(p0), scenario::make_party(p1));
}

// 跑一回合（双方都出招），停在回合末之前。
void play_round(scenario::ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== W1 真值表 ==========
    {
        // 快慢固定：0 号速度 300 → 先出手；1 号速度 100 → 后出手
        scenario::ScenarioBattle b = make_battle(/*speed0=*/300, /*speed1=*/100);
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b);  // 先手权已结算（ws.preemptive_right 就位）
        const int R = ctx.roundCount;
        CHECK(ctx.is_second_mover(1), "W1) 前置：1 号应为后出手");
        CHECK(!ctx.is_second_mover(0), "W1) 前置：0 号应为先出手");

        const auto In = EffectWindowKind::InRounds;
        const auto Next = EffectWindowKind::NextRounds;

        CHECK_EQ(ctx.round_effect_start_round(/*owner=*/0, /*duration=*/4, In), R,
                 "W1) N回合内 + 先出手 → 起点 = R");
        CHECK_EQ(ctx.round_effect_start_round(/*owner=*/1, /*duration=*/4, In), R + 1,
                 "W1) N回合内 + 后出手 → 起点 = R+1（顺延）");
        CHECK_EQ(ctx.round_effect_start_round(/*owner=*/1, /*duration=*/1, In), R,
                 "W1) N==1（本回合内）+ 后出手 → **不**顺延，仍为 R");
        CHECK_EQ(ctx.round_effect_start_round(/*owner=*/0, /*duration=*/2, Next), R + 1,
                 "W1) 下N回合 + 先出手 → 起点 = R+1（本回合不算）");
        CHECK_EQ(ctx.round_effect_start_round(/*owner=*/1, /*duration=*/2, Next), R + 1,
                 "W1) 下N回合 + 后出手 → 仍 R+1（不叠加顺延）");
        std::cout << "  [W1] 真值表: 内/先=R、内/后=R+1、内1/后=R、下/先=R+1、下/后=R+1\n";
    }

    //========== W2 端到端：843「下{N}回合先制+N」窗口从 R+1 起算，且真的覆盖 N 回合 ==========
    {
        constexpr int kFengHuo = 24772;  // 烽火蔓延：effect 843（下2回合自身所有技能先制+2）
        const EffectMeta* meta = EffectMetaCatalog::instance().find(843);
        CHECK(meta != nullptr, "W2) effect 843 应在 meta 目录中");
        CHECK(meta->window == EffectWindowKind::NextRounds,
              "W2) 认证数据层应已声明 effect 843 = next_rounds");

        auto p0 = scenario::make_pet(0, "先制方", {kFengHuo, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                     /*speed=*/300, /*atk=*/100, /*hp=*/3000);
        p0.skills[0].must_hit = true;
        auto p1 = scenario::make_pet(1, "对手", {kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                     /*speed=*/100, /*atk=*/100, /*hp=*/5000);
        scenario::ScenarioBattle b(scenario::make_party(p0), scenario::make_party(p1));
        b.start();
        BattleContext& ctx = b.ctx();
        const int R = ctx.roundCount;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();  // 回合 R 用完烽火蔓延

        // 桶里的 MOVE_RIGHT 条目：窗口起点应为 R+1（"下N回合"）
        int checked = 0;
        for (const auto& [key, eff] : ctx.skills_effects.at(State::BATTLE_FIRST_MOVE_RIGHT, 0)) {
            (void)key;
            if (eff->getEffectId() == 843) {
                CHECK_EQ(eff->getRegisteredRound(), R + 1,
                         "W2) 「下N回合」：窗口起点应为 R+1（本回合不算）");
                CHECK(!eff->isExpired(R + 2), "W2) 第 2 个回合仍应生效（R+2）");
                CHECK(eff->isExpired(R + 3), "W2) 第 3 个回合应已过期（窗口 = R+1..R+2，共 N=2）");
                ++checked;
            }
        }
        CHECK_EQ(checked, 1, "W2) 应能取到 843 注册的 MOVE_RIGHT 条目");

        // 行为断言：R+1 与 R+2 的先手权里能看到 +2 先制；R+3 没有
        auto priority_of = [&](int expect_round) {
            while (ctx.roundCount < expect_round) {
                b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);  // 换 1 号技能避免重复加
                b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
                b.drive();
            }
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
            ctx.debug_step_mode = true;
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.step_past(State::BATTLE_FIRST_MOVE_RIGHT);
            const int lv = ctx.ws.preemptive_level[0];
            b.drive();
            return lv;
        };
        const int lv_r1 = priority_of(R + 1);
        const int lv_r2 = priority_of(R + 2);
        CHECK(lv_r1 >= 2, "W2) R+1 应有 843 的 +2 先制");
        CHECK(lv_r2 >= 2, "W2) R+2 仍应有 +2 先制（窗口覆盖 N=2 回合）");
        std::cout << "  [W2] 843(下2回合先制+2): 窗口起点 R+1；R+1 先制=" << lv_r1
                  << "、R+2 先制=" << lv_r2 << "（R=" << R << "）\n";
    }

    //========= W3 对照：未声明的效果默认 in_rounds，先出手起点 = R =========
    {
        // effect 497（落芳天华固伤递增）未在认证层声明 → 默认 in_rounds
        const EffectMeta* meta = EffectMetaCatalog::instance().find(497);
        if (meta != nullptr) {
            CHECK(meta->window == EffectWindowKind::InRounds,
                  "W3) 未声明的效果应默认 in_rounds");
        }
        scenario::ScenarioBattle b = make_battle(/*speed0=*/300, /*speed1=*/100);
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b);
        // 先出手 + 未声明(in_rounds) → 起点就是当前回合（不挪）
        CHECK_EQ(ctx.round_effect_start_round(/*owner=*/0, /*duration=*/3, EffectWindowKind::InRounds),
                 ctx.roundCount, "W3) 未声明 + 先出手 → 起点 = 当前回合（默认档）");
        std::cout << "  [W3] 对照(未声明=in_rounds): 先出手窗口起点 = 当前回合（R=" << ctx.roundCount << "）\n";
    }

    std::cout << "\n场景 038 通过：N回合内(先/后出手)、下N回合(两种出手同规则)、N==1不顺延；"
                 "家族声明走认证数据层。\n";
    return 0;
}
