// 场景 7450 —— K392 锁伤族 CAP 二批（effect 928/1191/1938，soul_lib/k_damage_cap2.cpp）
//（2026-10-26，worktree ../agents/k392；号段 7450-7459）
//
// 官方依据（effect_icon.tips，行≠宏）：
//   928  茉蕊儿 3322 args[2,500,1,1]：攻击技分支→当回合受攻击伤害 ≤ maxHP/2（db[0]=2）；
//        属性技分支→当回合免疫异常（工单行内两分支；死亡继承/解除降态子句记档跳过）。
//   1191 芳馨·茉蕊儿 3613 args[2,1,500,500,1]：对手选择期选中攻击技→对手随机 db[1]=1 个
//        技能 PP 归零 + 当回合受攻击伤害 ≤ maxHP/2（db[0]=2）。
//   1938 狂沙之主·萨特 4354 args[1,2,2,2,2,1,1]：回合开始 hp>max/2（db[1]=2）→当回合
//        受攻击伤害 ≤ max/2（db[2]=2）。
//
// 断言口径：锁伤断言用**精确值**（loss == maxHP/2，钳制后随机浮动带被剪掉）；
//   697 凭证穿透用**区间**（loss > cap 且存活）；1938 体力门负位用「回合中回血」构造
//   （ROUND_START 后直写 hp——门在回合开始 latch，伤害时点回血不应回头开门）；
//   免疫分支用 try_apply_anomaly 直铺通道（K171 3050 同款）在 ROUND_START 后、出手前取证。
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <vector>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kYanFan = 10037;   // 燕返：威力 60 必中（攻击技）
constexpr int kGaoSu = 20017;    // 高速移动：属性技（无伤害噪声）
constexpr int kParalysis = static_cast<int>(AbnormalStatusId::Paralysis);

SoulMark make_mark(int effect_id, std::vector<int> db_args, int monster_id) {
    SoulMark sm(effect_id, "K" + std::to_string(effect_id), "test",
                EffectArgs(std::move(db_args)));
    sm.monster_id = monster_id;
    return sm;
}

// 进攻方（side 0）：atk 2500、燕返威力 60 → 公式 (0.84·25·60+2)·[217..255]/255
// = 裸伤 [1074,1262]（若吃本系 ×1.5 则 [1611,1893]）——两读法都落在 (1000,2000)，
// 对 hp=2000/cap=1000 的穿透位存活、对 cap=500 的锁伤位必超。
ElfPet attacker_pet() {
    return scenario::make_pet(2, "进攻方", {kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                              /*speed=*/300, /*atk=*/2500, /*hp=*/100000);
}

// 提交第二手操作但**不跑完回合**：submit 会把整个回合一口气跑完（run 循环到下一个
// 等输入点）→ 需要回合内取证（免疫票/体力门 latch）的用例，先置 debug_step_mode
// 让第二手只推一个状态，再 step_past 走到回合开始后。
void submit_second_stepped(ScenarioBattle& b, BattleContext& ctx,
                           BattleFsm::ActionType action, int index) {
    ctx.debug_step_mode = true;
    b.submit_operation(1, action, index);   // 单步：只执行收操作那一个状态
    ctx.debug_step_mode = false;
}

// 锁伤宿主（side 1，防守方）。
ElfPet holder_pet(int effect_id, std::vector<int> db_args, int monster_id, int hp) {
    return scenario::make_pet(1, "印主", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                              /*speed=*/100, /*atk=*/1, hp,
                              make_mark(effect_id, std::move(db_args), monster_id));
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ 928 茉蕊儿（3322）═══════════════════════════════════════════════
    // T0 攻击技分支：对手用燕返 → 当回合锁 maxHP/2（精确 500）
    {
        ScenarioBattle b(scenario::make_party(attacker_pet()),
                         scenario::make_party(holder_pet(928, {2, 500, 1, 1}, 3322, 1000)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int slot = ctx.on_stage[1];
        const int hp0 = ctx.seerRobot[1].elfPets[slot].hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 燕返
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        const int loss = hp0 - ctx.seerRobot[1].elfPets[slot].hp;
        CHECK_EQ(loss, 500, "T0 928：对手攻击技 → 锁 maxHP/2（精确）");
    }
    // T1 属性技分支：对手用高速移动 → 当回合免异常（直铺被拒）
    {
        ScenarioBattle b(scenario::make_party(attacker_pet()),
                         scenario::make_party(holder_pet(928, {2, 500, 1, 1}, 3322, 1000)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // 高速移动
        submit_second_stepped(b, ctx, BattleFsm::ActionType::NONE, -1);
        b.step_past(State::BATTLE_ROUND_START);   // 票已挂、出手未发生
        CHECK(!try_apply_anomaly(&ctx, 1, kParalysis, 3, /*actor=*/0),
              "T1 928：对手选属性技 → 当回合免异常（麻痹直铺被拒）");
        CHECK(!ctx.has_active_abnormal_status(1, kParalysis), "T1 928：无麻痹残留");
        b.drive();
    }
    // T2 免疫分支负位：对手选攻击技 → 免疫票不响应（异常照常落地）
    {
        ScenarioBattle b(scenario::make_party(attacker_pet()),
                         scenario::make_party(holder_pet(928, {2, 500, 1, 1}, 3322, 1000)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 燕返
        submit_second_stepped(b, ctx, BattleFsm::ActionType::NONE, -1);
        b.step_past(State::BATTLE_ROUND_START);
        CHECK(try_apply_anomaly(&ctx, 1, kParalysis, 3, /*actor=*/0),
              "T2 928：对手选攻击技 → 不免异常（麻痹落地）");
        CHECK(ctx.has_active_abnormal_status(1, kParalysis), "T2 928：麻痹在身");
        b.drive();
    }
    // T3 697 凭证穿透：燕返带 ignore_damage_limit → 锁伤失效（loss>cap 且存活）
    {
        ScenarioBattle b(scenario::make_party(attacker_pet()),
                         scenario::make_party(holder_pet(928, {2, 500, 1, 1}, 3322, 2000)));
        b.start();
        BattleContext& ctx = b.ctx();
        for (Skills& s : ctx.getPet(0).skills) {
            s.penetration_flags.ignore_damage_limit = true;   // 697 凭证（078 I1 同款）
        }
        const int slot = ctx.on_stage[1];
        const int hp0 = ctx.seerRobot[1].elfPets[slot].hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        const int loss = hp0 - ctx.seerRobot[1].elfPets[slot].hp;
        CHECK(loss > 1000 && loss < 2000,
              "T3 928：697 凭证穿锁伤（loss=" + std::to_string(loss) + " > cap=1000 且存活）");
    }

    // ══ 1191 芳馨·茉蕊儿（3613）════════════════════════════════════════
    // T4 对手选攻击技：随机 1 技 PP 归零（唯一 pp>0 槽=所选招，确定性）+ 锁 500
    {
        ElfPet foe = attacker_pet();
        for (int i = 1; i < 5; ++i) {
            foe.skills[static_cast<std::size_t>(i)].pp = 0;   // 只留槽 0 有 PP
        }
        ScenarioBattle b(scenario::make_party(foe),
                         scenario::make_party(holder_pet(1191, {2, 1, 500, 500, 1}, 3613, 1000)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int slot = ctx.on_stage[1];
        const int hp0 = ctx.seerRobot[1].elfPets[slot].hp;
        const int pp0 = ctx.getPet(0).skills[0].pp;
        CHECK(pp0 > 0, "T4 前置：燕返初始 PP>0");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 燕返（攻击技）
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(ctx.getPet(0).skills[0].pp, 0, "T4 1191：对手选攻击技 → 随机 1 技 PP 归零");
        CHECK_EQ(hp0 - ctx.seerRobot[1].elfPets[slot].hp, 500,
                 "T4 1191：对手选攻击技 → 当回合锁 maxHP/2（精确）");
    }
    // T5 负位：对手选属性技 → 不 PP 归零（全员 PP>0）、无伤害
    {
        ElfPet foe = attacker_pet();   // 槽 1 = 高速移动（属性技）出手
        for (int i = 0; i < 5; ++i) {
            if (foe.skills[static_cast<std::size_t>(i)].pp < 0) {
                foe.skills[static_cast<std::size_t>(i)].pp = 5;   // 防 -1 无限 PP 干扰
            }
        }
        ScenarioBattle b(scenario::make_party(foe),
                         scenario::make_party(holder_pet(1191, {2, 1, 500, 500, 1}, 3613, 1000)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int slot = ctx.on_stage[1];
        const int hp0 = ctx.seerRobot[1].elfPets[slot].hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // 高速移动（属性技）
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        int zeroed = 0;
        int min_pp = 1 << 30;
        for (const Skills& s : ctx.getPet(0).skills) {
            zeroed += (s.pp == 0) ? 1 : 0;
            min_pp = std::min(min_pp, s.pp);
        }
        CHECK(zeroed == 0 && min_pp > 0, "T5 1191：对手选属性技 → 无 PP 归零");
        CHECK_EQ(hp0 - ctx.seerRobot[1].elfPets[slot].hp, 0, "T5 1191：属性技无伤害");
    }
    // T6 697 凭证穿透（1191 同 928 链路）
    {
        ScenarioBattle b(scenario::make_party(attacker_pet()),
                         scenario::make_party(holder_pet(1191, {2, 1, 500, 500, 1}, 3613, 2000)));
        b.start();
        BattleContext& ctx = b.ctx();
        for (Skills& s : ctx.getPet(0).skills) {
            s.penetration_flags.ignore_damage_limit = true;
        }
        const int slot = ctx.on_stage[1];
        const int hp0 = ctx.seerRobot[1].elfPets[slot].hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        const int loss = hp0 - ctx.seerRobot[1].elfPets[slot].hp;
        CHECK(loss > 1000 && loss < 2000,
              "T6 1191：697 凭证穿锁伤（loss=" + std::to_string(loss) + "）");
    }

    // ══ 1938 狂沙之主·萨特（4354）══════════════════════════════════════
    // T7 体力门开：满血（>max/2）→ 锁 500（精确）
    {
        ScenarioBattle b(scenario::make_party(attacker_pet()),
                         scenario::make_party(holder_pet(1938, {1, 2, 2, 2, 2, 1, 1}, 4354, 1000)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int slot = ctx.on_stage[1];
        const int hp0 = ctx.seerRobot[1].elfPets[slot].hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(hp0 - ctx.seerRobot[1].elfPets[slot].hp, 500,
                 "T7 1938：回合开始 hp>max/2 → 锁 maxHP/2（精确）");
    }
    // T8 体力门负位 + latch 语义：回合开始 hp=490（≤max/2，门关）→ 回合中回血到 900
    //    （ROUND_START 后直写 hp 模拟）→ 大伤落地不锁 → 阵亡（若门误开/实时判则存活 400）
    {
        ScenarioBattle b(scenario::make_party(attacker_pet()),
                         scenario::make_party(holder_pet(1938, {1, 2, 2, 2, 2, 1, 1}, 4354, 1000)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int slot = ctx.on_stage[1];
        ctx.seerRobot[1].elfPets[slot].hp = 490;   // 回合开始前压线（<500）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        submit_second_stepped(b, ctx, BattleFsm::ActionType::NONE, -1);
        b.step_past(State::BATTLE_ROUND_START);    // 门已按 490 评过（关）
        ctx.seerRobot[1].elfPets[slot].hp = 900;   // 回合中回血越过 1/2 —— 不回开门
        b.drive();
        CHECK_EQ(ctx.seerRobot[1].elfPets[slot].hp, 0,
                 "T8 1938：门 latch 在回合开始（490≤1/2 关）→ 回血不开门 → 不锁阵亡");
    }
    // T9 697 凭证穿透（1938 同链路）
    {
        ScenarioBattle b(scenario::make_party(attacker_pet()),
                         scenario::make_party(holder_pet(1938, {1, 2, 2, 2, 2, 1, 1}, 4354, 2000)));
        b.start();
        BattleContext& ctx = b.ctx();
        for (Skills& s : ctx.getPet(0).skills) {
            s.penetration_flags.ignore_damage_limit = true;
        }
        const int slot = ctx.on_stage[1];
        const int hp0 = ctx.seerRobot[1].elfPets[slot].hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        const int loss = hp0 - ctx.seerRobot[1].elfPets[slot].hp;
        CHECK(loss > 1000 && loss < 2000,
              "T9 1938：697 凭证穿锁伤（loss=" + std::to_string(loss) + "）");
    }

    std::cout << "scenario_7450 PASS (K392 锁伤CAP二批 928/1191/1938)" << std::endl;
    return 0;
}
