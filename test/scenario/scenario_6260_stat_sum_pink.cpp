// 场景 6260 —— J293 属性总和/速度段粉伤族（876 / 812，moves_lib/j_stat_sum_pink.cpp）
//（2026-10-22，worktree feat/jbatch；号段 6260-6269）
//
// 断言口径：
//   - 876（33412 自然莲印，纯链 [20]）：粉伤值确定性，审计槽精确断言
//     sum=DEF+SDEF+SPEED 视图值、pink=sum×20%；再用"对手掉血 = 红伤 1~2 + 粉伤"
//     的行为带侧证。能力等级档单独验"视图值吃等级"（速度 +2 → sum 500→800）。
//   - 812（34211 飞矢流影，纯链 [6]）：用技后挂常驻 ROUND_END；每回合末按
//     k=max(0,速度等级) 段 × 1/6 削对手 max HP（一次 reduce，pct=k*100/6，上取整）。
//     审计槽精确断言 (原始等级, 有效段, pct, tick 次数) + max HP 精确值：
//       k=2：60000 → 40200 → 26934；k=0/负等级不动；k=6（pct=100）→ floor 1。
//   - 812 连续两回合再使用：恒"每回合一次"（source_id 同源去重，不叠加）——
//     k=1 两跳应为 60000→50400→42336（叠加形会是 35563）。
#include <iostream>
#include <string>
#include <any>

#include <entities/numerical-properties.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;      // 拍打：物 40（填充刀）
constexpr int k876Skill = 33412;   // 自然莲印 [876]|[20] 物 120（纯链）
constexpr int k812Skill = 34211;   // 飞矢流影 [812]|[6] 物 150（纯链）

constexpr int k876Pink = 998760000;
constexpr int k876Sum = 998760100;
constexpr int k812Stage = 998120000;
constexpr int k812Pct = 998120100;
constexpr int k812Ticks = 998120200;
constexpr int k812RawStage = 998120300;

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
}

ElfPet mk_self(int skill, int speed = 300, int hp = 5000) {
    ElfPet p = make_pet(1, "己方", {skill, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, speed,
                        /*atk=*/1, hp);
    accuracy_100(p);
    return p;
}

ElfPet mk_foe(int hp = 20000, int speed = 100) {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, speed,
                        /*atk=*/1, hp);
    accuracy_100(p);
    return p;
}

int stor(const BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return 0;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : 0;
}

int max_hp(BattleContext& ctx, int side) {
    return ctx.getPet(side).numericalBase[NumericalPropertyIndex::HP];
}

// 设定速度能力等级（本体 + 回合视图双写；render_by_sync 每回合从本体重基）。
void set_speed_stage(BattleContext& ctx, int side, int stage) {
    ctx.ability_levels[side][4] = stage;
    ctx.ws.view_levels[side][4] = stage;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T6260 876 —— 附加 防御+特防+速度 总和{0}% 粉伤 ══
    {
        {   // (a) 基础档：sum=100+100+300=500 → pink=100
            ScenarioBattle b(make_party(mk_self(k876Skill)), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            const int hp_before = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
            CHECK_EQ(stor(ctx, k876Sum), 500, "T6260.a 876：三属性视图总和 500");
            CHECK_EQ(stor(ctx, k876Pink), 100, "T6260.a 876：500×20% = 100 粉伤");
            const int loss = hp_before - ctx.getPet(1).hp;
            CHECK(loss >= 100 && loss <= 105,
                  "T6260.a 876：对手掉血 = 粉伤100 + 红伤1~2，实测 " + std::to_string(loss));
        }
        {   // (b) 视图值吃能力等级：速度+2 → 300×2=600 → sum=800 → pink=160
            ScenarioBattle b(make_party(mk_self(k876Skill)), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            set_speed_stage(ctx, 0, 2);
            const int hp_before = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
            CHECK_EQ(stor(ctx, k876Sum), 800, "T6260.b 876：速度+2 → 总和 800");
            CHECK_EQ(stor(ctx, k876Pink), 160, "T6260.b 876：800×20% = 160 粉伤");
            const int loss = hp_before - ctx.getPet(1).hp;
            CHECK(loss >= 160 && loss <= 165,
                  "T6260.b 876：对手掉血 = 粉伤160 + 红伤1~2，实测 " + std::to_string(loss));
        }
    }

    // ══ T6261 812 —— 速度每+1段，回合末削对手 1/{0} 最大体力 ══
    {
        {   // (a) k=2：R1/R2 各一跳，pct=33；精确 max HP 链
            ScenarioBattle b(make_party(mk_self(k812Skill)), make_party(mk_foe(60000)));
            b.start();
            BattleContext& ctx = b.ctx();
            CHECK_EQ(max_hp(ctx, 1), 60000, "T6261.a 前置：对手最大体力 60000");
            set_speed_stage(ctx, 0, 2);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();   // 施放当回合回合末就跳（常驻 ROUND_END）
            CHECK_EQ(stor(ctx, k812RawStage), 2, "T6261.a 812：原始速度等级 2");
            CHECK_EQ(stor(ctx, k812Stage), 2, "T6261.a 812：有效段 2");
            CHECK_EQ(stor(ctx, k812Pct), 33, "T6261.a 812：2 段 × 100/6 = 33");
            CHECK_EQ(stor(ctx, k812Ticks), 1, "T6261.a 812：R1 跳 1 次");
            CHECK_EQ(max_hp(ctx, 1), 40200,
                     "T6261.a 812：60000 削 33% 上取整 → 40200");
            CHECK_EQ(ctx.getPet(1).hp, 40200, "T6261.a 812：当前体力同步压低到新上限");

            b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();   // R2 再跳一次
            CHECK_EQ(stor(ctx, k812Ticks), 2, "T6261.a 812：R2 跳 2 次");
            CHECK_EQ(max_hp(ctx, 1), 26934,
                     "T6261.a 812：40200 再削 33% → 26934（按当前上限乘算）");
        }
        {   // (b) 无提升 / 负等级 → 不削（原始等级仍留痕）
            ScenarioBattle b(make_party(mk_self(k812Skill)), make_party(mk_foe(60000)));
            b.start();
            BattleContext& ctx = b.ctx();
            set_speed_stage(ctx, 0, 0);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
            CHECK_EQ(stor(ctx, k812RawStage), 0, "T6261.b 812：0 级 → 原始等级 0");
            CHECK_EQ(stor(ctx, k812Stage), 0, "T6261.b 812：有效段 0");
            CHECK_EQ(stor(ctx, k812Pct), 0, "T6261.b 812：pct 0");
            CHECK_EQ(stor(ctx, k812Ticks), 1, "T6261.b 812：仍照常 tick（审计）");
            CHECK_EQ(max_hp(ctx, 1), 60000, "T6261.b 812：0 段不削");

            set_speed_stage(ctx, 0, -2);
            b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
            CHECK_EQ(stor(ctx, k812RawStage), -2, "T6261.b 812：负等级原始值 -2");
            CHECK_EQ(stor(ctx, k812Stage), 0, "T6261.b 812：负等级钳 0");
            CHECK_EQ(max_hp(ctx, 1), 60000, "T6261.b 812：负等级不削");
        }
        {   // (c) 连续两回合重复使用：source_id 同源去重 → 每回合仍只一跳（不叠加）
            ScenarioBattle b(make_party(mk_self(k812Skill)), make_party(mk_foe(60000)));
            b.start();
            BattleContext& ctx = b.ctx();
            set_speed_stage(ctx, 0, 1);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();   // R1：pct=16 → 50400
            CHECK_EQ(max_hp(ctx, 1), 50400, "T6261.c 812：R1 16% → 50400");
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 再次使用
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();   // R2：仍只一跳 → 42336（叠加形会是 35563）
            CHECK_EQ(stor(ctx, k812Ticks), 2, "T6261.c 812：两回合共 2 跳（非 3 跳）");
            CHECK_EQ(max_hp(ctx, 1), 42336,
                     "T6261.c 812：重复使用不叠加（50400 再削 16% = 42336）");
        }
        {   // (d) +6 满段 → pct=100，上限钳 floor=1（"上限==0 ⇔ 消逝"不变量）
            ScenarioBattle b(make_party(mk_self(k812Skill)), make_party(mk_foe(99999)));
            b.start();
            BattleContext& ctx = b.ctx();
            set_speed_stage(ctx, 0, 6);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
            CHECK_EQ(stor(ctx, k812Pct), 100, "T6261.d 812：6 段 × 100/6 = 100");
            CHECK_EQ(max_hp(ctx, 1), 1, "T6261.d 812：100% 削到 floor 1");
            CHECK_EQ(ctx.getPet(1).hp, 1, "T6261.d 812：当前体力同步到 1");
        }
    }

    std::cout << "scenario_6260_stat_sum_pink: ALL PASS\n";
    return 0;
}
