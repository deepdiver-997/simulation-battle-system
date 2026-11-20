// 场景 885 —— 死亡裁决封闭（D2）+ 双侧同倒轮转（D4 配套，2026-10-01 改造单）
//
// D2 模型：死亡漏斗登记（pet_death_notified）= 裁决成立。之后"奶活"不可逆转——
// heal 对已登记宠 no-op（fuzz seed 11263 活锁的根修半边：等待死切期间死亡被治疗
// 翻转 → need_input 前提消失）。复活是独立通道：revive_pet 原语真实复位登记
//（重生之翼"复活背包内已倒下队友"走登记后 EVENT_DEATH watcher，D5 明文允许结算），
// 复活后治疗恢复可用。终局方向由结束标志位兜住（场景 878）。
//
// D4 配套：双方场上同回合倒下、双方都有替补 → 先倒方补位后**原地轮转**问另一方
//（原先只问 set_current_player 选中的一方，另一方拖着尸体进下一回合）。
//
// 断言组：
//   A 治疗封闭：登记后 heal_amount/heal 全 no-op（hp 不动、last_heal_amount=0）；
//     revive_pet 复活后治疗恢复可用。
//   B 双侧轮转：双方场上同倒 → 死切先问 0 方；0 方补位后原地转问 1 方；
//     1 方补位后对局推进，双方替补在场、未终局。
#include <iostream>

#include <primitives/battle_primitives.h>

#include <entities/pet_factory.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kGaoSu = 20017;   // 高速移动：属性技（无伤害噪声）

ElfPet pet(int id, const char* name) {
    return scenario::make_pet(id, name, {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                              /*speed=*/100, /*atk=*/10, /*hp=*/5000);
}

// 处刑魂印：回合开始时点把双方场上宠体力归零（秒杀通道——不经伤害管线、
// 不置 first_to_exhaust_foe → 同归保护不救，构造"双方同倒"）。
// 挂在 0 方精灵上（老式单效果魂印注册在 BATTLE_ROUND_START 桶，见 soul_mark_manager）。
// ⚠️ 两个守卫缺一不可：
//   ① 时点守卫（currentState==ROUND_START）：单效果魂印在**登场激活时会立即执行一次**
//     （activate_soul_mark"战斗开始即生效"语义）——那时杀的尸体会被 PROTECTION_1 的
//     流程保护（场上 hp<=0 → 拉回 1）当场救回，白杀；
//   ② 一次性守卫：不守卫会把死切换上场的替补当场处刑（登场激活再执行一次）。
constexpr int kExecuteOnceKey = 9996000;
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
    sm.id = 9996;
    sm.name = "处刑";
    sm.description = "回合开始把双方场上宠体力归零（测试桩）";
    sm.effect = &execute_both_soul_effect;
    sm.args = EffectArgs(std::vector<int>{0, 1});
    return sm;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ A 治疗封闭（原语级）══════════════════════════════════════════════════
    {
        ScenarioBattle b(scenario::make_party(pet(1, "甲")),
                         scenario::make_party(pet(2, "乙")));
        BattleContext& ctx = b.ctx();
        b.start();
        ElfPet& corpse = ctx.seerRobot[0].elfPets[ctx.on_stage[0]];
        corpse.hp = 0;
        REQUIRE(defeat_pet(&ctx, 0, ctx.on_stage[0], /*actor=*/1, DefeatCause::EFFECT)
                == DefeatResult::DEFEATED);
        CHECK(ctx.pet_death_notified[0][ctx.on_stage[0]], "A 裁决登记成立");

        const HealResult r1 = heal_amount(&ctx, 0, 100);
        CHECK_EQ(ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp, 0,
                 "A 登记后 heal_amount 无法奶活");
        CHECK_EQ(ctx.ws.last_heal_amount[0], 0, "A last_heal_amount 归零（监听方不误触发）");
        CHECK(r1 == HealResult::SUCCESS, "A 调用本身合法（参数无错，效果被封闭）");
        heal(&ctx, 0, 4);
        CHECK_EQ(ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp, 0,
                 "A 比例恢复（heal）同样封闭");

        // 复活通道独立：真实复位（D5 允许亡语复活结算的载体）。
        REQUIRE(revive_pet(&ctx, 0, ctx.on_stage[0], 50) == 50);
        CHECK(!ctx.pet_death_notified[0][ctx.on_stage[0]], "A 复活复位死亡登记");
        const HealResult r2 = heal_amount(&ctx, 0, 10);
        CHECK_EQ(ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp, 60, "A 复活后治疗恢复可用");
        CHECK_EQ(ctx.ws.last_heal_amount[0], 10, "A 复活后 last_heal_amount 记实际量");
        CHECK(r2 == HealResult::SUCCESS, "A 复活后 heal_amount 正常返回");
        std::cout << "  [A] 登记后治疗封闭；复活复位后恢复可用 ✓" << std::endl;
    }

    // ══ B 双侧同倒轮转 ═══════════════════════════════════════════════════════
    {
        // 0 方带"处刑"魂印：本回合 ROUND_START 双方场上归零（回合内时点，PROTECTION_1
        // 的流程保护已过）→ 两具尸体都在本回合进漏斗裁决。
        ElfPet side0_pet = pet(1, "甲");
        side0_pet.soulMark = make_execute_soul_mark();
        ScenarioBattle b(scenario::make_party(side0_pet),
                         scenario::make_party(pet(2, "乙")));
        BattleContext& ctx = b.ctx();
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();   // 回合内 ROUND_START 处刑（PROTECTION_1 之后）→ 漏斗登记 → 死切
        CHECK_EQ(static_cast<int>(ctx.currentState),
                 static_cast<int>(State::CHOOSE_AFTER_DEATH), "B 停在死切等待");
        CHECK_EQ(ctx.current_player_id_, 0, "B 先问 0 方");
        CHECK(ctx.pet_death_notified[0][0] && ctx.pet_death_notified[1][0],
              "B 双方场上死亡都已登记");
        CHECK(!ctx.finish_flag, "B 双方都有替补：不触发全灭终局");

        // 0 方补位 → 原地轮转问 1 方（不推进状态）。
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        CHECK_EQ(ctx.on_stage[0], 1, "B 0 方替补登场");
        CHECK_EQ(static_cast<int>(ctx.currentState),
                 static_cast<int>(State::CHOOSE_AFTER_DEATH), "B 仍在死切（轮转）");
        CHECK_EQ(ctx.current_player_id_, 1, "B 原地转问 1 方");

        // 1 方补位 → 双方就位，对局推进到下一回合。
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.drive();
        CHECK_EQ(ctx.on_stage[0], 1, "B 0 方替补在场");
        CHECK_EQ(ctx.on_stage[1], 1, "B 1 方替补在场");
        CHECK(ctx.seerRobot[0].elfPets[1].hp > 0 && ctx.seerRobot[1].elfPets[1].hp > 0,
              "B 双方替补活体在场");
        CHECK(ctx.currentState == State::OPERATION_CHOOSE_SKILL_MEDICAMENT,
              "B 推进到下一回合选择期");
        CHECK(!ctx.finish_flag, "B 未终局");
        std::cout << "  [B] 双侧同倒：先倒先补、原地轮转、双双就位续跑 ✓" << std::endl;
    }

    std::cout << "scenario_881 PASS (D2 治疗封闭 + D4 双侧同倒轮转)" << std::endl;
    return 0;
}
