// 场景 630 —— F3 吸取成功族二批（985 / 1062 / 1214，moves_lib/f_drain_family.cpp）
//（2026-09-29，worktree ../agents/f3；号段 630-639 一文件覆盖三效果）
//
// 家族门槛 = **吸取成功**：transfer_stat_boosts(ctx, opp, self) > 0（1123 直系同款——
// 对手无提升 / 被免消除强化挡下 → 返回 0 → 后续子句全不给）。
//
// 断言组：
//   T1 985（挟天震世令 19746，切片 [1]）：对手有提升 → 提升被吸走 + 下1回合对手属性
//      技能无效（R2 高速移动探针被拦、R3 恢复 → 窗口恰 1 回合）；
//      对照：对手无提升 → 无封属（R2 探针照常落地）。
//   T2 1062（海海之心 28415，切片 [4]）：吸取成功 → 对手掉最大体力 1/4（FIXED 精确值）
//      + 自身回等量；对照：无提升 → 不吸（属性技能无红伤，hp 双向不变）。
//   T3 1214（蜃影幽茫剑 30978，切片 [300]）：对手处于任意异常（麻痹注入）→ 吸 300 点
//      + 自身回 300；对照：无异常 → 不吸不回（红伤只剩余量）。
//
// 随机性口径：FIXED 粉伤 / heal 无浮动 → 精确等式；红伤 85%~100% 浮动 → 上下界断言。
// 探针/载体 all must_hit；暴击率由 harness 清零。麻痹(0) 无伤害档案（不污染 hp 断言）。
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kXietian = 19746;   // 挟天震世令：物理 160 [985,986]，985 切片 [1]
constexpr int kHaihai = 28415;    // 海海之心：属性 0 [1062,1068,1089]，1062 切片 [4] → 1/4
constexpr int kShenying = 30978;  // 蜃影幽茫剑：特殊 150 [769,1214]，1214 切片 [300]
constexpr int kZhuangJi = 10001;  // 撞击：无效果 priority 0（普攻 filler）
constexpr int kGaosu = 20017;     // 高速移动：属性 [4]/[4,100,2] → 自身速度+2（封属探针）
constexpr int kSpeedSlot = 4;     // 能力等级槽：0攻 1特攻 2防 3特防 4速 5命中
constexpr int kDrain300 = 300;    // 1214 的 {0}

ElfPet self_pet(std::array<int, 5> skills, int hp = 300000) {
    ElfPet p = make_pet(1, "己方", skills, /*speed=*/100, /*atk=*/1, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills) {
    ElfPet p = make_pet(2, "对手", skills, /*speed=*/300, /*atk=*/200, /*hp=*/100000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

void play_round(ScenarioBattle& b, BattleFsm::ActionType self_act, int self_slot,
                BattleFsm::ActionType foe_act, int foe_slot) {
    b.submit_operation(0, self_act, self_slot);
    b.submit_operation(1, foe_act, foe_slot);
    b.drive();
}

void grant_foe_boosts(BattleContext& ctx, int levels) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        ctx.ability_levels[1][i] = levels;   // 开战后写入（on-stage，470-T1 同款）
    }
}

bool all_levels_zero(const BattleContext& ctx, int side) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx.ability_levels[side][i] != 0) {
            return false;
        }
    }
    return true;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ════════════════════════════════════════════════════════════════
    // T1a 985：吸取成功 → 提升被吸走 + 下1回合对手属性技能无效（探针被拦）
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kXietian, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})),
                         make_party(foe_pet({kZhuangJi, kGaosu, kZhuangJi, kZhuangJi, kZhuangJi})));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_foe_boosts(ctx, 2);
        // R1：己方挟天震世令（吸取提升成功 → 预约下1回合封属）
        play_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                   BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(all_levels_zero(ctx, 1), "T1a 核心：对手能力提升被吸走（transfer 成功门槛）");
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            CHECK_EQ(ctx.ability_levels[0][i], 2, "T1a：提升整体搬到自身（+2）");
        }
        // R2：对手用属性技能探针（高速移动）→ 被封属拦下，效果不落地
        play_round(b, BattleFsm::ActionType::NONE, 0,
                   BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(ctx.ability_levels[1][kSpeedSlot], 0,
                 "T1a 核心：下1回合对手属性技能无效（速度+2 探针被拦）");
        // R3：窗口恰 1 回合 → 探针恢复落地
        play_round(b, BattleFsm::ActionType::NONE, 0,
                   BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(ctx.ability_levels[1][kSpeedSlot], 2,
                 "T1a：封属窗口恰 1 回合（R3 探针照常 +2）");
        std::cout << "  [T1a] 985 吸取成功→下1回合封属 通过\n";
    }
    // ════════════════════════════════════════════════════════════════
    // T1b 985 对照：对手无提升 → 吸取失败 → 不封属
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kXietian, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})),
                         make_party(foe_pet({kZhuangJi, kGaosu, kZhuangJi, kZhuangJi, kZhuangJi})));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                   BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(all_levels_zero(ctx, 0), "T1b 前提：无提升可吸（自身未获得提升）");
        play_round(b, BattleFsm::ActionType::NONE, 0,
                   BattleFsm::ActionType::SELECT_SKILL, 1);
        CHECK_EQ(ctx.ability_levels[1][kSpeedSlot], 2,
                 "T1b 核心：吸取失败 → 不封属（探针照常 +2）");
        std::cout << "  [T1b] 985 门槛对照 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // T2a 1062：吸取成功 → 对手掉最大体力 1/4（FIXED 精确）+ 自身回等量
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kHaihai, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_foe_boosts(ctx, 2);
        // 自身压到 260000（017 直写同款）：缺口 40000 > 回复量，断言不被 max_hp 钳制污染
        ctx.getPet(0).hp = 260000;
        // 测试轮：己方海海之心（属性技能，无红伤）——吸取成功 → 吸对手最大体力 1/4 = 25000
        play_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                   BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.getPet(1).hp, 100000 - 25000,
                 "T2a 核心：吸取对手最大体力的1/4（100000/4=25000，FIXED 精确值）");
        CHECK_EQ(ctx.getPet(0).hp, 260000 + 25000,
                 "T2a 核心：自身回复等量体力（+25000）");
        CHECK(all_levels_zero(ctx, 1), "T2a：提升同时被吸走（transfer 门槛同族）");
        std::cout << "  [T2a] 1062 吸取成功→吸最大体力1/4 通过\n";
    }
    // ════════════════════════════════════════════════════════════════
    // T2b 1062 对照：对手无提升 → 不吸（属性技能无红伤，hp 双向不变）
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kHaihai, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})));
        b.start();
        BattleContext& ctx = b.ctx();
        // 自身压到 260000（直写同 T2a）：不吸时 hp 应原样不动
        ctx.getPet(0).hp = 260000;
        play_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                   BattleFsm::ActionType::NONE, 0);
        CHECK_EQ(ctx.getPet(1).hp, 100000, "T2b 核心：吸取失败 → 对手不掉血");
        CHECK_EQ(ctx.getPet(0).hp, 260000, "T2b 核心：吸取失败 → 自身不回复");
        std::cout << "  [T2b] 1062 门槛对照 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // T3a 1214：对手处于任意异常 → 吸 300 点 + 自身回 300
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kShenying, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})));
        b.start();
        BattleContext& ctx = b.ctx();
        // 自身压到 299000（直写同 T2a）：缺口 1000 > 回复量 300，断言不被钳制污染
        ctx.getPet(0).hp = 299000;
        // 注入麻痹（0，无伤害档案）——"对手处于异常状态"门槛料（081 同款直调）
        apply_anomaly(&ctx, 1, static_cast<int>(AbnormalStatusId::Paralysis),
                      /*duration_rounds=*/3, /*actor=*/0);
        REQUIRE(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Paralysis)));
        // 测试轮：己方蜃影幽茫剑（atk=1 红伤只剩量）→ 吸 300 + 回 300
        play_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                   BattleFsm::ActionType::NONE, 0);
        const int foe_loss = 100000 - ctx.getPet(1).hp;
        CHECK(foe_loss >= 300 + 1, "T3a 核心：吸取 300 点落地（对手掉血 ≥ 301）");
        CHECK(foe_loss <= 400, "T3a：红伤只剩量（atk=1，浮动有界）");
        CHECK_EQ(ctx.getPet(0).hp, 299000 + kDrain300,
                 "T3a 核心：自身回复吸取量（+300 精确）");
        std::cout << "  [T3a] 1214 对手异常→吸取300点 通过\n";
    }
    // ════════════════════════════════════════════════════════════════
    // T3b 1214 对照：对手无异常 → 不吸不回（红伤只剩余量）
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kShenying, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})));
        b.start();
        BattleContext& ctx = b.ctx();
        // 自身压到 299000（直写同 T2a）：不吸时 hp 应原样不动
        ctx.getPet(0).hp = 299000;
        play_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                   BattleFsm::ActionType::NONE, 0);
        const int foe_loss = 100000 - ctx.getPet(1).hp;
        CHECK(foe_loss <= 100, "T3b 核心：对手无异常 → 不吸 300（红伤只剩余量）");
        CHECK_EQ(ctx.getPet(0).hp, 299000, "T3b 核心：无异常 → 自身不回复");
        std::cout << "  [T3b] 1214 门槛对照 通过\n";
    }

    std::cout << "scenario_630 PASS (F3 吸取成功族二批 985/1062/1214)" << std::endl;
    return 0;
}
