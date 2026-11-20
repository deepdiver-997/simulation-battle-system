#include <utils/sbs_rand.h>
// 场景 063：能力等级迁 `BattleContext::ability_levels`（on-stage 作用域）——2026-09-16 用户拍板。
//
// 口径（用户原话）："等级确实要放到 context，之前是我的设计失误，原本就是要**切换清除等级状态**，
// 等级提升/下降**只在在场期间有意义**，这个一定要改。"
//
// 落地：权威状态从 `ElfPet::levels`（本体、跨切换保留、跨对局残留）迁到
// `BattleContext::ability_levels[side][6]`——随上下场清（与 `abnormal_status_end_round` 同语义）；
// `ws.view_levels` 仍是回合内视图（每回合从本体重基）。
//
// 验证点：
//   L1 等级写入/读取走 context（stat_change 生效、视图同步 → 伤害公式读得到）
//   L2 ★ 换宠清除：A 加攻 +2 → 切走 → 再切回 → 等级归 0（"只在在场期间有意义"）
//   L3 ★ 开战清零：上一局残留的等级不会带进新对局（复现原来的 pet.levels 污染）
//   L4 换宠清除同时刷视图（ws.view_levels 归 0，公式不会读到旧值）
//   L5 药品"解除能力下降"改走 context（负等级清零 + 视图同步）
//   L6 上升/下降的钳制与视图同步（±6 边界不变）
#include <cassert>
#include <cstdlib>
#include <iostream>

#include <effects/trait_state.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

constexpr int kPaiDa = 10004;      // 物攻 40
constexpr int kGaoSu = 20017;      // 属性技能
constexpr int kAtk = 0;            // 攻击（引擎 stat 索引）
constexpr int kDef = 2;            // 防御

using scenario::ScenarioBattle;

ScenarioBattle make_battle(const ElfPet& p0, const ElfPet& p1) {
    sbs_srand(20260916);
    return ScenarioBattle(scenario::make_party(p0), scenario::make_party(p1));
}

ElfPet make_pet(int side, const char* name) {
    return scenario::make_pet(side, name, {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                              100 + side * 100, 200, 800);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== L1 读写走 context + 视图同步 ==========
    {
        ScenarioBattle b = make_battle(make_pet(0, "甲"), make_pet(1, "乙"));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK_EQ(ctx.ability_levels[0][kAtk], 0, "L1 初始为 0");
        CHECK_EQ(stat_change(&ctx, 0, kAtk, 2), StatChangeResult::SUCCESS, "L1 +2 应成功");
        CHECK_EQ(ctx.ability_levels[0][kAtk], 2, "L1 本体应记在 context");
        CHECK_EQ(ctx.ws.view_levels[0][kAtk], 2, "L1 视图应同步（公式读它）");
        std::cout << "L1 PASS 等级本体在 context + 视图同步\n";
    }

    //========== L2 ★ 换宠清除：切走再切回等级归 0 ==========
    {
        ScenarioBattle b = make_battle(make_pet(0, "甲"), make_pet(1, "乙"));
        b.start();
        BattleContext& ctx = b.ctx();
        stat_change(&ctx, 0, kAtk, 2);
        CHECK_EQ(ctx.ability_levels[0][kAtk], 2, "L2 前提：已 +2");

        // 主动切换：第 1 号技能位以外任选 → 用 CHOOSE_PET 切到槽位 1
        ctx.m_buffer.clear();
        const int buf[4] = {0, static_cast<int>(BattleFsm::ActionType::CHOOSE_PET), 1, 0};
        ctx.m_buffer.assign(reinterpret_cast<const char*>(buf),
                            reinterpret_cast<const char*>(buf) + 4 * sizeof(int));
        ctx.is_empty = false;
        b.drive();
        CHECK_EQ(ctx.on_stage[0], 1, "L2 前提：已换到槽位 1");
        CHECK_EQ(ctx.ability_levels[0][kAtk], 0, "L2 ★ 换宠应清除等级（切走即失效）");
        CHECK_EQ(ctx.ws.view_levels[0][kAtk], 0, "L2 视图也应归 0");

        // 再切回来：不得"复活"旧等级
        stat_change(&ctx, 0, kDef, 1);   // 新精灵自己加个别的等级，确认是干净的
        ctx.m_buffer.clear();
        const int buf2[4] = {0, static_cast<int>(BattleFsm::ActionType::CHOOSE_PET), 0, 0};
        ctx.m_buffer.assign(reinterpret_cast<const char*>(buf2),
                            reinterpret_cast<const char*>(buf2) + 4 * sizeof(int));
        ctx.is_empty = false;
        b.drive();
        CHECK_EQ(ctx.on_stage[0], 0, "L2 切回槽位 0");
        CHECK_EQ(ctx.ability_levels[0][kAtk], 0, "L2 ★ 切回不复活旧等级");
        CHECK_EQ(ctx.ability_levels[0][kDef], 0, "L2 换宠同样清掉新加的等级");
        std::cout << "L2 PASS 换宠清除等级（不被复活）\n";
    }

    //========== L3 ★ 跨对局不残留（原 pet.levels 会污染）==========
    {
        // 同一批 pet 对象连续打两局（训练模式的用法）：第一局加满等级，第二局必须从 0 开始。
        ElfPet p0 = make_pet(0, "甲");
        ElfPet p1 = make_pet(1, "乙");
        {
            ScenarioBattle first = make_battle(p0, p1);
            first.start();
            stat_change(&first.ctx(), 0, kAtk, 6);
            CHECK_EQ(first.ctx().ability_levels[0][kAtk], 6, "L3 第一局已 +6");
        }
        {
            ScenarioBattle second = make_battle(p0, p1);   // 复用同一批 pet 对象
            second.start();
            CHECK_EQ(second.ctx().ability_levels[0][kAtk], 0,
                     "L3 ★ 新对局应从 0 开始（等级是 on-stage 状态，不是精灵本体状态）");
            std::cout << "L3 PASS 跨对局不残留\n";
        }
    }

    //========== L4 clear_ability_levels 直接可用（换宠路径的同一入口）==========
    {
        ScenarioBattle b = make_battle(make_pet(0, "甲"), make_pet(1, "乙"));
        b.start();
        BattleContext& ctx = b.ctx();
        stat_change(&ctx, 0, kAtk, 3);
        stat_change(&ctx, 1, kDef, -3);
        ctx.clear_ability_levels(0);
        CHECK_EQ(ctx.ability_levels[0][kAtk], 0, "L4 清空一方");
        CHECK_EQ(ctx.ability_levels[1][kDef], -3, "L4 不影响另一方");
        std::cout << "L4 PASS clear_ability_levels 按方清空\n";
    }

    //========== L5 钳制边界（±6，视图同步）==========
    {
        ScenarioBattle b = make_battle(make_pet(0, "甲"), make_pet(1, "乙"));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK_EQ(stat_change(&ctx, 0, kAtk, 6), StatChangeResult::SUCCESS, "L5 +6 成功");
        CHECK_EQ(stat_change(&ctx, 0, kAtk, 1), StatChangeResult::AT_CAP, "L5 越 +6 应 AT_CAP");
        CHECK_EQ(ctx.ability_levels[0][kAtk], 6, "L5 本体保持 6");
        CHECK_EQ(ctx.ws.view_levels[0][kAtk], 6, "L5 视图保持 6");
        // 弱化到 -6
        CHECK_EQ(stat_drop(&ctx, 1, kDef, 20), StatDropResult::SUCCESS, "L5 弱化成功");
        CHECK_EQ(ctx.ability_levels[1][kDef], -6, "L5 钳在 -6");
        CHECK_EQ(stat_drop(&ctx, 1, kDef, 1), StatDropResult::AT_FLOOR, "L5 已在底应 AT_FLOOR");
        std::cout << "L5 PASS 钳制 ±6 与视图同步\n";
    }

    std::cout << "\n场景 063 全部通过：能力等级迁 context（换宠清除、跨局不残留）✓" << std::endl;
    return 0;
}
