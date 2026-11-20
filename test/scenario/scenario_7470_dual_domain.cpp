// 场景 7470 —— K394 贝狄薇尔双领域魂印（effect 1780=1档 / 1935=2档，soul_lib/k_dual_domain.cpp）
//（2026-10-29，worktree ../agents/k394；号段 7470-7479）
//
// 断言块（单回合/有限步进——本魂印含 CAP/回血语义，禁 while(!FINISHED)，速查 §五）：
//   T1  1780 登场门(对手1000>500→机械) + CAP 60% 精确 600×3 回合 + R2 末无盾 + R3 末
//       ⑤盾500/罩500 + 账本清账 + R4 CAP 到期（盾后损耗带 [574,762]，与钳制残留 100 不相交）
//   T2  1935 同构：CAP 40% 精确 400×3（两行变体 args 驱动档位差）
//   T3  1780 机械先制+1 下回合窗（R1 受击 → R2 ws.preemptive_level==1 + 先手权翻转，等速局）
//   T4  1935 先制+2（level==2）
//   T5  次元封回血：对手体力 400≤500→次元，heal 原语整笔归零；对照局(>500→机械)正常回复
//   T6  次元吸取 1/3 精确值：受击回合末对手 -133（400/3 整除）、自身等量回复（压血带取证）
//   T7  ⑤ boss 无效（is_boss_challenge 前置位）：领域/CAP 照常、盾罩不授
//   T8  ①先出手前对手全技 PP-1/-2（1780 每回合 -1 / 1935 -2）
//   T9  ①后出手前随机 1 技归零（双方都选技——ACTION_START 桶 acted 守门；守恒 38）
//   T10 主动切换登场 5 倍消耗（1780）：切换回合对手用技 -5（印主 CHOOSE_PET 非"出手"，
//       ①不触发——acted 守门口径），次回合印主出手后 ① 恢复且无倍率残留
//   T11 主动切换 20 倍（1935）：切换回合 -20
//   T12 登场门等号边界：对手恰 500 → 次元（账本白盒+封回血）、501 → 机械
//   T13 次元领域 3 回合到期：R1-R3 封回血、R4 恢复
//   T14 697 凭证穿机械 CAP（loss 回裸伤带，与 600 钳制位不相交）
//
// 白盒键（k_dual_domain.cpp 头注）：plugin_storage key = 基座 + owner*10 + 槽位，
//   1780 基座 990178000 / 1935 基座 990193500；槽 0=类别(1机械/2次元) 1=到期回合。
// 伤害带：燕返 atk2500 裸伤 [1074,1262]（K392 同款推导）；atk100 → [44,52]。
#include <algorithm>
#include <any>
#include <iostream>
#include <vector>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kYanFan = 10037;   // 燕返：威力 60 必中（攻击技，priority 0 不污染先制）
constexpr int kGaoSu = 20017;    // 高速移动：属性技（无伤害）
constexpr int kMark1780 = 1780;
constexpr int kMark1935 = 1935;

// 白盒账本读数（与实现槽位编号对齐）。
int stor(const BattleContext& ctx, int base, int owner, int slot) {
    const auto it = ctx.plugin_storage.find(base + owner * 10 + slot);
    if (it == ctx.plugin_storage.end()) {
        return 0;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : 0;
}

SoulMark make_mark(int effect_id, const std::vector<int>& db_args) {
    SoulMark sm(effect_id, "K" + std::to_string(effect_id), "贝狄薇尔双领域",
                EffectArgs(db_args));
    sm.monster_id = 4184;
    return sm;
}

// 印主（side 0 默认）：燕返 + 高速移动×4；行参随宏变体（1780=1档 / 1935=2档）。
ElfPet host_pet(int effect_id, int speed, int hp) {
    const std::vector<int> db_args = (effect_id == kMark1935)
                                         ? std::vector<int>{2, 1, 500, 500, 20, 500, 500, 1}
                                         : std::vector<int>{1, 1, 500, 500, 5, 500, 500, 1};
    return scenario::make_pet(4184, "贝狄薇尔", {kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                              speed, /*atk=*/10, hp, make_mark(effect_id, db_args));
}

// 进攻方（side 1）：atk 2500 → 裸伤 [1074,1262]。
ElfPet foe_pet(int speed, int hp, int atk = 2500) {
    return scenario::make_pet(4185, "对手", {kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                              speed, atk, hp);
}

void idle_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
}

// 对手拍印主一回合（对手燕返 / 印主 NONE），返回印主本回合掉血。
int foe_hit_round(ScenarioBattle& b) {
    const int slot = b.ctx().on_stage[0];
    const int hp0 = b.ctx().seerRobot[0].elfPets[slot].hp;
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    return hp0 - b.ctx().seerRobot[0].elfPets[slot].hp;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== T1 1780：登场门(机械) + CAP 600×3 + R3 末盾罩 + R4 到期 ==========
    {
        ScenarioBattle b(scenario::make_party(host_pet(kMark1780, 200, 1000)),
                         scenario::make_party(foe_pet(90, 1000)));
        BattleContext& ctx = b.ctx();
        b.start();
        CHECK_EQ(stor(ctx, 990178000, 0, 0), 1, "T1 账本：机械领域(对手1000>500)");
        CHECK_EQ(stor(ctx, 990178000, 0, 1), 3, "T1 账本：到期回合=登场回合+3");
        CHECK_EQ(foe_hit_round(b), 600, "T1 R1 CAP 精确 600（max1000×60%）");
        ctx.getPet(0).hp = 1000;
        CHECK_EQ(foe_hit_round(b), 600, "T1 R2 CAP 精确 600");
        CHECK_EQ(ctx.getPet(0).shield_bank_.total(), 0, "T1 R2 末：盾未授（领域未结束）");
        CHECK_EQ(ctx.getPet(0).cover, 0, "T1 R2 末：罩未授");
        ctx.getPet(0).hp = 1000;
        CHECK_EQ(foe_hit_round(b), 600, "T1 R3 CAP 精确 600（本回合伤害先于⑤盾）");
        CHECK_EQ(ctx.getPet(0).shield_bank_.total(), 500, "T1 R3 末：⑤盾 500");
        CHECK_EQ(ctx.getPet(0).cover, 500, "T1 R3 末：⑤罩 500");
        CHECK_EQ(stor(ctx, 990178000, 0, 0), 0, "T1 R3 末：领域账本已清");
        ctx.getPet(0).hp = 1000;
        const int loss4 = foe_hit_round(b);
        CHECK(loss4 >= 574 && loss4 <= 762,
              "T1 R4 CAP 到期（盾吸 500 后损耗 " + std::to_string(loss4) + " ∈[574,762]；"
              "若钳制残留应为 100）");
        std::cout << "  [T1] 1780 机械领域 CAP/盾罩/到期通过\n";
    }

    //========== T2 1935：CAP 40%（两行变体档位差）===============================
    {
        ScenarioBattle b(scenario::make_party(host_pet(kMark1935, 200, 1000)),
                         scenario::make_party(foe_pet(90, 1000)));
        BattleContext& ctx = b.ctx();
        b.start();
        CHECK_EQ(stor(ctx, 990193500, 0, 0), 1, "T2 账本：机械领域");
        CHECK_EQ(foe_hit_round(b), 400, "T2 R1 CAP 精确 400（max1000×40%）");
        ctx.getPet(0).hp = 1000;
        CHECK_EQ(foe_hit_round(b), 400, "T2 R2 CAP 精确 400");
        ctx.getPet(0).hp = 1000;
        CHECK_EQ(foe_hit_round(b), 400, "T2 R3 CAP 精确 400");
        CHECK_EQ(ctx.getPet(0).shield_bank_.total(), 500, "T2 R3 末：⑤盾 500（两行同值）");
        CHECK_EQ(ctx.getPet(0).cover, 500, "T2 R3 末：⑤罩 500");
        ctx.getPet(0).hp = 1000;
        const int loss4 = foe_hit_round(b);
        CHECK(loss4 >= 574 && loss4 <= 762, "T2 R4 CAP 到期（损耗 ∈[574,762]）");
        std::cout << "  [T2] 1935 档位差（400 vs 600）通过\n";
    }

    //========== T3 1780：受击 → 下回合先制+1（等速局，+1 定胜负）=================
    {
        ScenarioBattle b(scenario::make_party(host_pet(kMark1780, 100, 1000)),
                         scenario::make_party(foe_pet(100, 100000)));
        BattleContext& ctx = b.ctx();
        b.start();
        foe_hit_round(b);   // R1 受击 → pending
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(ctx.ws.preemptive_level[0], 1, "T3 R2 先制+1 已消费（白盒）");
        CHECK_EQ(static_cast<int>(ctx.preemptive_right),
                 static_cast<int>(PreemptiveRight::SEER_ROBOT_1),
                 "T3 R2 等速局 +1 翻转先手权");
        std::cout << "  [T3] 机械先制+1 下回合窗通过\n";
    }

    //========== T4 1935：先制+2 ==================================================
    {
        ScenarioBattle b(scenario::make_party(host_pet(kMark1935, 100, 1000)),
                         scenario::make_party(foe_pet(100, 100000)));
        BattleContext& ctx = b.ctx();
        b.start();
        foe_hit_round(b);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(ctx.ws.preemptive_level[0], 2, "T4 R2 先制+2（档位驱动量）");
        CHECK_EQ(static_cast<int>(ctx.preemptive_right),
                 static_cast<int>(PreemptiveRight::SEER_ROBOT_1), "T4 R2 先手权翻转");
        std::cout << "  [T4] 机械先制+2 通过\n";
    }

    //========== T5 次元封回血（对手体力 400≤500）+ 机械对照 ======================
    {
        ScenarioBattle b(scenario::make_party(host_pet(kMark1780, 200, 1000)),
                         scenario::make_party(foe_pet(90, 400)));
        BattleContext& ctx = b.ctx();
        b.start();
        CHECK_EQ(stor(ctx, 990178000, 0, 0), 2, "T5 账本：次元领域(400≤500)");
        ctx.getPet(1).hp = 200;
        heal(&ctx, 1, 2);   // 对手"技能恢复"必经原语（heal_impl=一切恢复入口）
        CHECK_EQ(ctx.ws.last_heal_amount[1], 0, "T5 次元领域：对手回复被整笔封锁");
        CHECK_EQ(ctx.getPet(1).hp, 200, "T5 对手体力未动");
    }
    {
        ScenarioBattle b(scenario::make_party(host_pet(kMark1780, 200, 1000)),
                         scenario::make_party(foe_pet(90, 1000)));
        BattleContext& ctx = b.ctx();
        b.start();
        ctx.getPet(1).hp = 200;
        heal(&ctx, 1, 2);
        CHECK_EQ(ctx.ws.last_heal_amount[1], 500, "T5 机械领域对照：回复正常（封锁仅次元；max/2=500）");
        CHECK_EQ(ctx.getPet(1).hp, 700, "T5 对照局体力已回（200+500）");
        std::cout << "  [T5] 次元封回血 + 对照通过\n";
    }

    //========== T6 次元吸取 1/3 精确值 ===========================================
    {
        ScenarioBattle b(scenario::make_party(host_pet(kMark1780, 200, 10000)),
                         scenario::make_party(foe_pet(90, 400, /*atk=*/100)));
        BattleContext& ctx = b.ctx();
        b.start();
        ctx.getPet(0).hp = 200;   // 压血防回复封顶（SOP §五.2）
        const int slot = ctx.on_stage[0];
        const int hp0 = ctx.seerRobot[0].elfPets[slot].hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 燕返 atk100 → [44,52]
        b.drive();
        CHECK_EQ(400 - ctx.getPet(1).hp, 133, "T6 对手被吸取 400/3=133（精确）");
        const int host_hp = ctx.seerRobot[0].elfPets[slot].hp;
        CHECK(host_hp >= hp0 - 52 + 133 && host_hp <= hp0 - 44 + 133,
              "T6 自身等量回复（净 " + std::to_string(host_hp - hp0) + " ∈[81,89]）");
        std::cout << "  [T6] 次元吸取 1/3 通过\n";
    }

    //========== T7 ⑤ boss 无效（is_boss_challenge 前置位）=======================
    {
        ScenarioBattle b(scenario::make_party(host_pet(kMark1780, 200, 1000)),
                         scenario::make_party(foe_pet(90, 1000)));
        BattleContext& ctx = b.ctx();
        ctx.is_boss_challenge = true;   // 必须前置位（K294 勘误：start 后置位不生效）
        b.start();
        CHECK_EQ(foe_hit_round(b), 600, "T7 boss 局 CAP 照常（⑤才无效）");
        ctx.getPet(0).hp = 1000;
        foe_hit_round(b);
        ctx.getPet(0).hp = 1000;
        foe_hit_round(b);
        CHECK_EQ(ctx.getPet(0).shield_bank_.total(), 0, "T7 boss 局：⑤盾不授");
        CHECK_EQ(ctx.getPet(0).cover, 0, "T7 boss 局：⑤罩不授");
        std::cout << "  [T7] ⑤ boss 无效通过\n";
    }

    //========== T8 ①先出手前对手全技 PP-1/-2 =====================================
    {
        auto make_foe_pp = [](int hp) {
            ElfPet foe = foe_pet(90, hp);
            for (Skills& s : foe.skills) {
                s.pp = 10;
                s.maxPP = 10;
            }
            return foe;
        };
        {
            ScenarioBattle b(scenario::make_party(host_pet(kMark1780, 200, 1000)),
                             scenario::make_party(make_foe_pp(100000)));
            BattleContext& ctx = b.ctx();
            b.start();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
            for (int i = 0; i < 5; ++i) {
                CHECK_EQ(ctx.getPet(1).skills[static_cast<std::size_t>(i)].pp, 9,
                         "T8 1780 R1 对手全技 PP-1（槽 " + std::to_string(i) + "）");
            }
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
            CHECK_EQ(ctx.getPet(1).skills[2].pp, 8, "T8 R2 累计 -2");
        }
        {
            ScenarioBattle b(scenario::make_party(host_pet(kMark1935, 200, 1000)),
                             scenario::make_party(make_foe_pp(100000)));
            BattleContext& ctx = b.ctx();
            b.start();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
            for (int i = 0; i < 5; ++i) {
                CHECK_EQ(ctx.getPet(1).skills[static_cast<std::size_t>(i)].pp, 8,
                         "T8 1935 R1 对手全技 PP-2（槽 " + std::to_string(i) + "）");
            }
            std::cout << "  [T8] 先出手 PP-1/-2 通过\n";
        }
    }

    //========== T9 ①后出手前随机 1 技归零 ========================================
    // （双方都须 SELECT_SKILL——ACTION_START 桶 acted 守门；对手槽 0 只留 1 点 PP 自用，
    //   归零只可能落在槽 1-4（各 10），总量守恒可精确断言。）
    {
        ElfPet foe = foe_pet(200, 100000);   // 对手先手
        for (Skills& s : foe.skills) {
            s.pp = 10;
            s.maxPP = 10;
        }
        foe.skills[0].pp = 1;   // 对手自用技：只留 1 点，消费后为 0（不进归零池）
        ScenarioBattle b(scenario::make_party(host_pet(kMark1780, 90, 1000)),
                         scenario::make_party(foe));
        BattleContext& ctx = b.ctx();
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 后手方也须"出手"
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(1).skills[0].pp, 0, "T9 对手自用技 PP 消耗至 0");
        int zeroed = 0;
        int total = 0;
        for (int i = 1; i < 5; ++i) {
            const int pp = ctx.getPet(1).skills[static_cast<std::size_t>(i)].pp;
            zeroed += (pp == 0) ? 1 : 0;
            total += pp;
        }
        CHECK_EQ(zeroed, 1, "T9 恰随机归零 1 技（槽 1-4 中）");
        CHECK_EQ(total, 30, "T9 其余三技合计 30（50 - 自用1 - 归零10 - 槽0本为1）");
        std::cout << "  [T9] 后出手随机 1 技归零通过\n";
    }

    //========== T10 主动切换登场 5 倍消耗（1780）=================================
    {
        ElfPet foe = foe_pet(90, 1000);
        for (Skills& s : foe.skills) {
            s.pp = 30;
            s.maxPP = 30;
        }
        auto party0 = scenario::make_party(
            scenario::make_pet(4186, "替宠", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                               300, 10, 1000));
        party0[1] = host_pet(kMark1780, 200, 1000);
        ScenarioBattle b(party0, scenario::make_party(foe));
        BattleContext& ctx = b.ctx();
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);   // 主动切换：贝狄薇尔登场
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1); // 对手用高速移动
        b.drive();
        CHECK_EQ(ctx.getPet(1).skills[1].pp, 25,
                 "T10 切换回合对手用技 -5（5倍；印主 CHOOSE_PET 非'出手'→①不触发）");
        CHECK_EQ(ctx.getPet(1).skills[0].pp, 30, "T10 未用技不动");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        CHECK_EQ(ctx.getPet(1).skills[1].pp, 23,
                 "T10 次回合无 5 倍残留（-1(①，印主出手)-1(自用)）");
        CHECK_EQ(ctx.getPet(1).skills[0].pp, 29, "T10 次回合 ①-1 恢复正常触发");
        std::cout << "  [T10] 主动切换 5 倍消耗通过\n";
    }

    //========== T11 主动切换 20 倍（1935）========================================
    {
        ElfPet foe = foe_pet(90, 1000);
        for (Skills& s : foe.skills) {
            s.pp = 30;
            s.maxPP = 30;
        }
        auto party0 = scenario::make_party(
            scenario::make_pet(4186, "替宠", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                               300, 10, 1000));
        party0[1] = host_pet(kMark1935, 200, 1000);
        ScenarioBattle b(party0, scenario::make_party(foe));
        BattleContext& ctx = b.ctx();
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        CHECK_EQ(ctx.getPet(1).skills[1].pp, 10,
                 "T11 切换回合对手用技 -20（20倍；CHOOSE_PET 非'出手'→①不触发）");
        CHECK_EQ(ctx.getPet(1).skills[0].pp, 30, "T11 未用技不动");
        std::cout << "  [T11] 主动切换 20 倍消耗通过\n";
    }

    //========== T12 登场门等号边界（恰 500 → 次元 / 501 → 机械）==================
    {
        {
            ScenarioBattle b(scenario::make_party(host_pet(kMark1780, 200, 1000)),
                             scenario::make_party(foe_pet(90, 500)));
            BattleContext& ctx = b.ctx();
            b.start();
            CHECK_EQ(stor(ctx, 990178000, 0, 0), 2, "T12 恰500：≤500 → 次元领域");
            ctx.getPet(1).hp = 200;
            heal(&ctx, 1, 2);
            CHECK_EQ(ctx.ws.last_heal_amount[1], 0, "T12 恰500：封回血生效");
        }
        {
            ScenarioBattle b(scenario::make_party(host_pet(kMark1780, 200, 1000)),
                             scenario::make_party(foe_pet(90, 501)));
            BattleContext& ctx = b.ctx();
            b.start();
            CHECK_EQ(stor(ctx, 990178000, 0, 0), 1, "T12 501：>500 → 机械领域");
            ctx.getPet(1).hp = 200;
            heal(&ctx, 1, 2);
            CHECK_EQ(ctx.ws.last_heal_amount[1], 250, "T12 501：不封回血（对照）");
        }
        std::cout << "  [T12] 登场门等号边界通过\n";
    }

    //========== T13 次元领域 3 回合到期 ==========================================
    {
        ScenarioBattle b(scenario::make_party(host_pet(kMark1780, 200, 1000)),
                         scenario::make_party(foe_pet(90, 400)));
        BattleContext& ctx = b.ctx();
        b.start();
        ctx.getPet(1).hp = 200;
        heal(&ctx, 1, 2);
        CHECK_EQ(ctx.ws.last_heal_amount[1], 0, "T13 R1 封回血");
        idle_round(b);
        ctx.getPet(1).hp = 200;
        heal(&ctx, 1, 2);
        CHECK_EQ(ctx.ws.last_heal_amount[1], 0, "T13 R2 封回血");
        idle_round(b);
        ctx.getPet(1).hp = 200;
        heal(&ctx, 1, 2);
        CHECK_EQ(ctx.ws.last_heal_amount[1], 0, "T13 R3 封回血（末回合仍在窗）");
        idle_round(b);
        ctx.getPet(1).hp = 200;
        heal(&ctx, 1, 2);
        CHECK_EQ(ctx.ws.last_heal_amount[1], 200, "T13 R4 领域到期：恢复放行");
        std::cout << "  [T13] 次元领域 3 回合到期通过\n";
    }

    //========== T14 697 凭证穿机械 CAP ===========================================
    {
        ScenarioBattle b(scenario::make_party(host_pet(kMark1780, 200, 1000)),
                         scenario::make_party(foe_pet(90, 1000)));
        BattleContext& ctx = b.ctx();
        b.start();
        for (Skills& s : ctx.getPet(1).skills) {
            s.penetration_flags.ignore_damage_limit = true;   // 697"无视伤害限制"
        }
        const int loss = foe_hit_round(b);
        CHECK(loss > 700 && loss < 2000,
              "T14 697 凭证穿锁伤（loss=" + std::to_string(loss) + " 回裸伤带，≠600）");
        std::cout << "  [T14] 697 穿 CAP 通过\n";
    }

    std::cout << "scenario_7470 all passed" << std::endl;
    return 0;
}
