// 场景 887 —— 胜负结算口径：终局快照 + "视为存活"加账（2026-10-01 用户加测）
//
// 官方依据：
//   · "己方压轴击杀对手压轴重生也算自己赢"（用户实测口径）——胜负在终局立旗瞬间
//     定档：finish_alive_snapshot 按当时 hp>0 清点，其后亡语复活只结算、不改判；
//   · 薇尔诗黄金万象（effect_des 523，monster=4000）："己方的薇尔诗在胜负结算中
//     视为存活"——结算存活数在快照之上加回 treated-alive 槽（settlement_treated_alive，
//     汇报时点读取——她死亡触发的亡语置位晚于立旗也赶得上）。
//
// 断言组：
//   A 压轴重生不翻盘：败方最后一血被击倒 → 亡语复活真实落地（hp>0）→ 快照仍 0，
//     结算存活数仍 0（对方满编 → 判负），胜负不因晚到复活改成平局。
//   B 黄金万象加账（模拟 2513-② 的结算置位）：双方互灭（快照 0/0 = 平局基准）→
//     置 treated-alive 后结算存活数 1/0 → 胜负翻为置位方——"强制在结束后改变
//     存活精灵数，以此改变战局结果"的引擎形态。
#include <iostream>

#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kGaoSu = 20017;    // 高速移动：属性技（无伤害噪声）
constexpr int kYanFan = 10037;   // 燕返：威力 60 必中

ElfPet victim_pet(int id) {
    return scenario::make_pet(id, "败方", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                              /*speed=*/100, /*atk=*/10, /*hp=*/100);
}
ElfPet killer_pet(int id, int speed) {
    return scenario::make_pet(id, "击杀方", {kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                              speed, /*atk=*/5000, /*hp=*/100000);
}
ElfPet tank_pet(int id) {
    return scenario::make_pet(id, "耐久桩", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                              /*speed=*/100, /*atk=*/1, /*hp=*/1000000);
}

void zero_bench(BattleContext& ctx, int side) {
    for (int i = 1; i < 6; ++i) {
        ctx.seerRobot[side].elfPets[i].hp = 0;
    }
}

// 处刑魂印：ROUND_START 时点把双方场上宠归零（秒杀通道→同归保护不救，构造互灭）。
// 时点/一次性双守卫见场景 885 的注释（登场激活会立即执行一次 + PROTECTION_1 会救回）。
constexpr int kExecuteOnceKey = 9996001;
EffectResult execute_both_soul_effect(BattleContext* ctx, const EffectArgs&) {
    if (!ctx || ctx->currentState != State::BATTLE_ROUND_START
        || ctx->plugin_storage.contains(kExecuteOnceKey)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage[kExecuteOnceKey] = 1;
    for (int side = 0; side < 2; ++side) {
        const int slot = ctx->on_stage[side];
        if (slot >= 0 && slot < 6 && ctx->seerRobot[side].elfPets[slot].hp > 0) {
            force_hp_to_zero(ctx, side, /*actor=*/-1);
        }
    }
    return EffectResult::kOk;
}

SoulMark make_execute_soul_mark() {
    SoulMark sm;
    sm.id = 9995;
    sm.name = "处刑";
    sm.description = "回合开始把双方场上宠体力归零（测试桩）";
    sm.effect = &execute_both_soul_effect;
    sm.args = EffectArgs(std::vector<int>{0, 1});
    return sm;
}

// 与 battle_over_to_json 相同的胜负判定（读 settlement_alive_count）。
int settlement_winner(const BattleContext& ctx) {
    const int alive0 = ctx.settlement_alive_count(0);
    const int alive1 = ctx.settlement_alive_count(1);
    if (alive0 > 0 && alive1 == 0) {
        return 0;
    }
    if (alive1 > 0 && alive0 == 0) {
        return 1;
    }
    return -1;   // 平局/未分
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ A 压轴重生不翻盘 ═════════════════════════════════════════════════════
    {
        ScenarioBattle b(scenario::make_party(victim_pet(1)),
                         scenario::make_party(killer_pet(2, /*speed=*/300)));
        BattleContext& ctx = b.ctx();
        b.start();
        zero_bench(ctx, 0);
        // 败方亡语：死亡登记后把自己复活（重生之翼式"计数跳动"——效果确实结算了）。
        ctx.event_center_.register_watcher(
            EventType::EVENT_DEATH, /*owner=*/0, ctx.roundCount, /*duration=*/0,
            /*once=*/false,
            [](BattleContext* wctx, const BattleEvent& ev) {
                if (ev.target == 0) {
                    revive_pet(wctx, ev.target, ev.slot, 50);
                }
            },
            WatcherScope::TEAM);
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const int slot = ctx.on_stage[0];
        CHECK_EQ(static_cast<int>(ctx.currentState), static_cast<int>(State::FINISHED),
                 "A 终局成立");
        CHECK_EQ(ctx.seerRobot[0].elfPets[slot].hp, 50, "A 亡语复活真实落地（hp>0）");
        CHECK_EQ(ctx.finish_alive_snapshot[0], 0,
                 "A 快照定档在立旗瞬间：败方 0 活口（晚到复活不进快照）");
        CHECK_EQ(ctx.finish_alive_snapshot[1], 6, "A 胜方 6 活口");
        CHECK_EQ(ctx.settlement_alive_count(0), 0,
                 "A 结算存活数仍 0——压轴重生不改判（官方口径）");
        CHECK_EQ(settlement_winner(ctx), 1, "A 判负不翻成平局");
        std::cout << "  [A] 压轴重生：效果落地但胜负定档不翻盘 ✓" << std::endl;
    }

    // ══ B 黄金万象加账：互灭平局 → 视为存活翻胜局 ════════════════════════════
    {
        // 双方都只剩场上那只（替补直写归零），处刑魂印同回合互灭 → 快照 0/0 平局基准。
        ElfPet side0_pet = tank_pet(1);
        side0_pet.soulMark = make_execute_soul_mark();
        ScenarioBattle b(scenario::make_party(side0_pet),
                         scenario::make_party(tank_pet(2)));
        BattleContext& ctx = b.ctx();
        b.start();
        zero_bench(ctx, 0);
        zero_bench(ctx, 1);
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(static_cast<int>(ctx.currentState), static_cast<int>(State::FINISHED),
                 "B 双方互灭 → 终局成立");
        CHECK(ctx.finish_flag, "B 结束标志已立");
        CHECK_EQ(ctx.finish_alive_snapshot[0], 0, "B 快照：0 方 0 活口");
        CHECK_EQ(ctx.finish_alive_snapshot[1], 0, "B 快照：1 方 0 活口");
        CHECK_EQ(settlement_winner(ctx), -1, "B 无加账时判平（draw）");

        // 模拟薇尔诗 2513-② 的结算置位（她死亡触发的亡语晚于立旗，汇报时点读取赶得上）：
        // "己方的薇尔诗在胜负结算中视为存活"（effect_des 523）。
        ctx.settlement_treated_alive[0][ctx.on_stage[0]] = true;
        CHECK_EQ(ctx.settlement_alive_count(0), 1, "B 加账后 0 方结算存活数 = 1");
        CHECK_EQ(ctx.settlement_alive_count(1), 0, "B 1 方仍 0");
        CHECK_EQ(settlement_winner(ctx), 0, "B 胜负翻为 0 方——结算口径改变战局结果");
        std::cout << "  [B] 视为存活加账：平局 → 置位方胜 ✓" << std::endl;
    }

    std::cout << "scenario_883 PASS (胜负结算口径：终局快照定档 + 视为存活加账)" << std::endl;
    return 0;
}
