// 场景 886 —— 终局即停：最后一只死亡后**不会开始下一个回合**（2026-10-01 用户加测）
//
// 官方依据链：薇尔诗黄金万象词条（effect_des 523，monster=4000）把"视为存活"锚在
// **胜负结算**层，而非"回合继续"——终局不需要当回合跑完，需要的是结算口径
// （快照 + 视为存活加账，见场景 887）；重生之翼实测（效果结算了一瞬、随即终止）
// 与之拼合 → 定档：终局旗立起后，当回合剩余时点不再推进，下一回合永不开始。
//
// 本场景锁定的边界（三道）：
//   ① 死亡时点的亡语照常结算（"一瞬"）——EVENT_DEATH watcher 恰好执行 1 次；
//   ② 当回合不再推进——ROUND_COMPLETION 桶 0 次（advanceRound 在其中，roundCount 冻结）；
//   ③ 下一回合永不开始——ROUND_START 桶恰好 1 次（只有终局前那一回合的自己）。
#include <iostream>

#include <effects/continuousEffect.h>
#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

// 桶执行计数器（EffectFn 是裸函数指针，捕获不了 lambda → TU 级静态，单线程测试安全）。
int g_round_start_runs = 0;
int g_round_completion_runs = 0;
int g_death_watch_runs = 0;

EffectResult count_round_start(BattleContext*, const EffectArgs&) {
    ++g_round_start_runs;
    return EffectResult::kOk;
}
EffectResult count_round_completion(BattleContext*, const EffectArgs&) {
    ++g_round_completion_runs;
    return EffectResult::kOk;
}

// 往 skills_effects 桶挂一个常驻计数节点（moves_lib register_round_effect 同款构造）。
void register_bucket_counter(BattleContext* ctx, State trigger, int effect_id, EffectFn fn) {
    Effect e(effect_id, 0, /*owner=*/0, /*left_round=*/-1, EffectArgs(), fn);
    auto ce = std::make_unique<ContinuousEffect>(e, trigger, /*owner=*/0, /*duration=*/-1,
                                                 ctx->roundCount);
    ce->source_id_ = effect_id;
    ctx->register_skill_effect(trigger, 0, std::move(ce));
}

constexpr int kGaoSu = 20017;    // 高速移动：属性技（无伤害噪声）
constexpr int kYanFan = 10037;   // 燕返：威力 60 必中

ElfPet victim_pet(int id) {
    return scenario::make_pet(id, "败方", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                              /*speed=*/100, /*atk=*/10, /*hp=*/100);
}
ElfPet killer_pet(int id) {
    return scenario::make_pet(id, "击杀方", {kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                              /*speed=*/300, /*atk=*/5000, /*hp=*/100000);
}

void zero_bench(BattleContext& ctx, int side) {
    for (int i = 1; i < 6; ++i) {
        ctx.seerRobot[side].elfPets[i].hp = 0;
    }
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    ScenarioBattle b(scenario::make_party(victim_pet(1)),
                     scenario::make_party(killer_pet(2)));
    BattleContext& ctx = b.ctx();
    b.start();
    zero_bench(ctx, 0);
    register_bucket_counter(&ctx, State::BATTLE_ROUND_START, 88201, &count_round_start);
    register_bucket_counter(&ctx, State::BATTLE_ROUND_COMPLETION, 88202,
                            &count_round_completion);
    // 亡语观测：死亡时点的 drain 里执行（"结算了一瞬"的证据面）。
    ctx.event_center_.register_watcher(
        EventType::EVENT_DEATH, /*owner=*/0, ctx.roundCount, /*duration=*/0, /*once=*/false,
        [](BattleContext*, const BattleEvent& ev) {
            if (ev.target == 0) {
                ++g_death_watch_runs;
            }
        },
        WatcherScope::TEAM);

    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 先手燕返一击杀
    b.drive();

    CHECK_EQ(static_cast<int>(ctx.currentState), static_cast<int>(State::FINISHED),
             "终局直达 FINISHED");
    CHECK(ctx.finish_flag, "结束标志已立");
    // ① 亡语结算了一瞬：
    CHECK_EQ(g_death_watch_runs, 1, "死亡时点的亡语 watcher 恰好结算 1 次");
    // ② 当回合不再推进：
    CHECK_EQ(g_round_completion_runs, 0, "ROUND_COMPLETION 不再执行（当回合不跑完）");
    CHECK_EQ(ctx.roundCount, 0, "roundCount 冻结（advanceRound 在 ROUND_COMPLETION 里）");
    // ③ 下一回合永不开始：
    CHECK_EQ(g_round_start_runs, 1,
             "ROUND_START 恰好 1 次 = 终局前那一回合的自己；第 2 回合的 ROUND_START 不存在");
    std::cout << "  [882] 最后死亡 → 亡语一瞬 ✓ 当回合不再推进 ✓ 下一回合永不开始 ✓"
              << std::endl;

    std::cout << "scenario_882 PASS (终局即停：不开始下一回合)" << std::endl;
    return 0;
}
