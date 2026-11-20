// 场景 878 —— 终局标志位模型（D4/D5，2026-10-01《终局与死切状态机改造单》）
//
// 官方实测口径（用户，重生之翼案例）：一方最后一只精灵死亡 → 对局**直接结束**；
// 但动画里死亡效果（复活计数 1→2）结算了一瞬。定档 = 结束标志位：
//   死亡漏斗完成即评估终局（在场宠死亡已登记 + 6 槽无存活）→ 立旗；
//   其后亡语/复活 watcher 允许在当前时点结算完，但流程见旗即走 FINISHED，**不回滚**。
//
// 引擎落点（本场景锁定）：
//   · funnel_on_stage_deaths 尾部 evaluate_battle_end（漏斗完成即判）；
//   · finish_battle()：立旗 + 跳 FINISHED + on_finish 单次回调；
//   · runInternal 检查点：旗立后任何后续时点不进入（handler 内的 FINISHED 被
//     "跳 OLD_ROUND_END_1"覆盖也能兜回）。
//
// 断言组：
//   A 全灭直达 FINISH：先手方一击打掉对方最后一血 → 不进死切、不等输入，旗立 + 回调一次。
//   B 亡语不救终局：EVENT_DEATH watcher 在登记后复活该宠（重生之翼式"计数跳动"）→
//     复活真实落地（hp>0、登记复位）但对局仍 FINISHED——终局不因亡语复活回滚。
//   C 被打方全灭对称：败侧是后手方同样直达 FINISH（评估不挑先手）。
//
// 确定性纪律：击杀走真实攻击（燕返必中、atk 压到 hp 的数倍，±浮动不影响致死）；
// 替补"早已倒下"用停等期直写（PROTECTION_1 的流程保护只拉**场上**，不碰场下）。
#include <iostream>

#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kGaoSu = 20017;    // 高速移动：属性技（无伤害噪声）
constexpr int kYanFan = 10037;   // 燕返：威力 60 必中

// 败方桩：低血，被任何一击打穿。
ElfPet victim_pet(int id, const char* name) {
    return scenario::make_pet(id, name, {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                              /*speed=*/100, /*atk=*/10, /*hp=*/100);
}

// 击杀方桩：高速（先手）+ 大攻，燕返一击必杀。
ElfPet killer_pet(int id, int speed) {
    return scenario::make_pet(id, "击杀方", {kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                              speed, /*atk=*/5000, /*hp=*/100000);
}

// 把 side 的 1..5 槽直写归零（"早就倒下的替补"）。停等期直写：不触发漏斗/登记，
// 全灭判定只看"场上已登记死亡 + 6 槽无 hp>0"。
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

    // ══ A 全灭直达 FINISH（先手方击杀）══════════════════════════════════════
    {
        ScenarioBattle b(scenario::make_party(victim_pet(1, "败方")),
                         scenario::make_party(killer_pet(2, /*speed=*/300)));
        BattleContext& ctx = b.ctx();
        b.start();
        zero_bench(ctx, 0);
        int finish_calls = 0;
        ctx.on_finish = [&](BattleContext& c) {
            ++finish_calls;
            CHECK(c.finish_flag, "A on_finish 回调时旗已立");
        };
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 燕返
        b.drive();
        CHECK_EQ(static_cast<int>(ctx.currentState), static_cast<int>(State::FINISHED),
                 "A 全灭直达 FINISHED（不再进死切等输入）");
        CHECK(ctx.finish_flag, "A 结束标志已立");
        CHECK_EQ(finish_calls, 1, "A on_finish 恰好一次");
        CHECK_EQ(ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp, 0, "A 最后一血被击倒");
        CHECK(ctx.pet_death_notified[0][ctx.on_stage[0]], "A 死亡已登记");
        int alive0 = 0;
        for (int i = 0; i < 6; ++i) {
            if (ctx.seerRobot[0].elfPets[i].hp > 0) {
                ++alive0;
            }
        }
        CHECK_EQ(alive0, 0, "A 败方 6 槽无存活（与 battle_over 胜负口径一致）");
        std::cout << "  [A] 全灭直达 FINISH：旗立 + on_finish 一次 ✓" << std::endl;
    }

    // ══ B 亡语不救终局（重生之翼口径）═══════════════════════════════════════
    {
        ScenarioBattle b(scenario::make_party(victim_pet(1, "败方")),
                         scenario::make_party(killer_pet(2, /*speed=*/300)));
        BattleContext& ctx = b.ctx();
        b.start();
        zero_bench(ctx, 0);
        int finish_calls = 0;
        ctx.on_finish = [&](BattleContext&) { ++finish_calls; };
        // 重生之翼式亡语：死亡登记后（EVENT_DEATH 在 defeat_pet 登记尾 emit）触发复活。
        ctx.event_center_.register_watcher(
            EventType::EVENT_DEATH, /*owner=*/0, ctx.roundCount, /*duration=*/0,
            /*once=*/false,
            [](BattleContext* wctx, const BattleEvent& ev) {
                if (ev.target == 0) {
                    // "复活计数跳动"：登记后的宠物被真实复活（hp>0、登记复位）
                    revive_pet(wctx, ev.target, ev.slot, 50);
                }
            },
            WatcherScope::TEAM);
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const int slot = ctx.on_stage[0];
        // 终局不回滚：
        CHECK_EQ(static_cast<int>(ctx.currentState), static_cast<int>(State::FINISHED),
                 "B 亡语复活后对局仍 FINISHED（不回滚终局）");
        CHECK(ctx.finish_flag, "B 结束标志保持立起");
        CHECK_EQ(finish_calls, 1, "B on_finish 仍恰好一次");
        // 效果确实结算了一瞬（=官方动画计数 1→2）：
        CHECK_EQ(ctx.seerRobot[0].elfPets[slot].hp, 50, "B 亡语复活真实落地（hp>0）");
        CHECK(!ctx.pet_death_notified[0][slot], "B 复活复位死亡登记（revive 原语语义）");
        std::cout << "  [B] 亡语结算了一瞬（复活落地）但终局不回滚 ✓" << std::endl;
    }

    // ══ C 后手方全灭对称 ═════════════════════════════════════════════════════
    {
        // 败方高速先手（打不死对方），击杀方慢速后手一击收尾。
        ScenarioBattle b(scenario::make_party(killer_pet(1, /*speed=*/50)),
                         scenario::make_party(victim_pet(2, "败方")));
        BattleContext& ctx = b.ctx();
        b.start();
        zero_bench(ctx, 1);
        int finish_calls = 0;
        ctx.on_finish = [&](BattleContext&) { ++finish_calls; };
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 燕返（后手）
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(static_cast<int>(ctx.currentState), static_cast<int>(State::FINISHED),
                 "C 后手方全灭同样直达 FINISHED");
        CHECK(ctx.finish_flag, "C 结束标志已立");
        CHECK_EQ(finish_calls, 1, "C on_finish 恰好一次");
        CHECK(ctx.pet_death_notified[1][ctx.on_stage[1]], "C 败侧死亡已登记");
        std::cout << "  [C] 后手方全灭对称直达 FINISH ✓" << std::endl;
    }

    std::cout << "scenario_878 PASS (终局标志位：全灭直达 FINISH；亡语结算不回滚终局)"
              << std::endl;
    return 0;
}
