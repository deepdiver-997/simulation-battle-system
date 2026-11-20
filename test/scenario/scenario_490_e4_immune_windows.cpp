#include <utils/sbs_rand.h>
// 场景 490：E4 减伤/免疫窗口族（44/50/127/576/741）
// （2026-09-29，E4 / D 组转 E 组首任务；实现 resources/moves_lib/e_immune_window.cpp）
//
// T1 50 物理减半：R1 物理攻击减半（loss == raw/2）、R2 到期全额；特攻不减半（过滤）。
// T2 44 特攻减半：镜像 T1。
// T3 127 概率窗：100% 载体（光羽断空闪 [100,2]）→ R1/R2 全源减半、R3 到期全额。
// T4 576 低于阈值免疫：raw < 200 → 免疫（loss==0）；raw ≥ 200 → 全额落地。
// T5 741 大于阈值免疫：raw > 280 → 免疫（loss==0）；raw < 280 → 全额落地。
//
// raw 取 ws.raw_attack_damage[攻方]（330 同款读法）；合成宠无特性/暴击（harness 默认
// 清零 crit_rate）、技能全部 must_hit → 伤害确定。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/state.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kHit = 10001;        // 撞击：无副作用物理攻击（power 35）
constexpr int kWaterGun = 10013;   // 水枪：无副作用特殊攻击（power 40）
constexpr int kFiller = 10001;

// 窗口持有方（先手）：slot0 = 载体技，slot1+ = 普通攻击（到期断言用，避免续窗）。
ElfPet window_holder(int carrier) {
    ElfPet p = scenario::make_pet(1, "持窗方", {carrier, kFiller, kFiller, kFiller, kFiller},
                                  /*speed=*/200, /*atk=*/20, 1000000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// 攻击方（后手）：全部 slot 同一种普攻。
ElfPet striker(int attack_skill, int atk) {
    ElfPet p = scenario::make_pet(2, "攻方", {attack_skill, attack_skill, attack_skill,
                                              attack_skill, attack_skill},
                                  /*speed=*/100, atk, 1000000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// R1：持窗方先手用载体（idx0）开窗 → 攻方反击。返回本回合攻方攻击对持窗方造成的
// 实际掉血与裸伤（raw = ws.raw_attack_damage[1]）。
struct Round1 {
    int loss = 0;
    int raw = 0;
};

Round1 drive_round1(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 载体：开窗
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 攻方普攻
    b.drive();
    BattleContext& ctx = b.ctx();
    Round1 r;
    r.raw = ctx.ws.raw_attack_damage[1];
    r.loss = 1000000 - ctx.getPet(0).hp;
    return r;
}

// 续回合：持窗方改用普攻（不续窗）→ 攻方普攻。
void drive_next_round(ScenarioBattle& b, int holder_slot = 1) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, holder_slot);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

}  // namespace

int main() {
    sbs_srand(20260929);
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== T1 50 物理减半：物理减半 / 到期恢复 / 特攻不滤 ==========
    {
        sbs_srand(20260929);
        ScenarioBattle b(scenario::make_party(window_holder(11268 /*雷电冲撞 [50]*/)),
                         scenario::make_party(striker(kHit, /*atk=*/100)));
        BattleContext& ctx = b.ctx();
        b.start();
        const Round1 r1 = drive_round1(b);
        CHECK(r1.raw > 0, "T1 前置：攻方造成了裸伤");
        CHECK_EQ(r1.loss, r1.raw / 2, "T1 核心：物理攻击伤害减半（乘算槽 50）");
        CHECK_EQ(ctx.ws.damage_reduce_mul[0][0], 50, "T1 通道：reduce_mul 首槽写 50");
        drive_next_round(b);
        const int loss2 = 1000000 - ctx.getPet(0).hp - r1.loss;
        const int raw2 = ctx.ws.raw_attack_damage[1];
        CHECK_EQ(loss2, raw2, "T1 到期：rounds=1 窗口结束，R2 全额受伤");
        std::cout << "  [T1] 50 物理减半 + 到期 通过\n";
    }
    {
        sbs_srand(20260930);
        ScenarioBattle b(scenario::make_party(window_holder(11268)),
                         scenario::make_party(striker(kWaterGun, /*atk=*/100)));
        BattleContext& ctx = b.ctx();
        b.start();
        const Round1 r1 = drive_round1(b);
        CHECK(r1.raw > 0, "T1b 前置：攻方造成了裸伤");
        CHECK_EQ(r1.loss, r1.raw, "T1b 过滤：50 只减物理，特殊攻击全额");
        std::cout << "  [T1b] 50 特攻不滤 通过\n";
    }

    //========== T2 44 特攻减半：特攻减半 / 物理不滤 ==========
    {
        sbs_srand(20260931);
        ScenarioBattle b(scenario::make_party(window_holder(11854 /*水流冲刷 [44]*/)),
                         scenario::make_party(striker(kWaterGun, /*atk=*/100)));
        BattleContext& ctx = b.ctx();
        b.start();
        const Round1 r1 = drive_round1(b);
        CHECK(r1.raw > 0, "T2 前置：攻方造成了裸伤");
        CHECK_EQ(r1.loss, r1.raw / 2, "T2 核心：特殊攻击伤害减半");
        std::cout << "  [T2] 44 特攻减半 通过\n";
    }
    {
        sbs_srand(20260932);
        ScenarioBattle b(scenario::make_party(window_holder(11854)),
                         scenario::make_party(striker(kHit, /*atk=*/100)));
        BattleContext& ctx = b.ctx();
        b.start();
        const Round1 r1 = drive_round1(b);
        CHECK(r1.raw > 0, "T2b 前置：攻方造成了裸伤");
        CHECK_EQ(r1.loss, r1.raw, "T2b 过滤：44 只减特殊，物理攻击全额");
        std::cout << "  [T2b] 44 物理不滤 通过\n";
    }

    //========== T3 127 概率窗：100% 载体，2 回合全源减半，R3 到期 ==========
    {
        sbs_srand(20260933);
        ScenarioBattle b(scenario::make_party(window_holder(14739 /*光羽断空闪 [100,2]*/)),
                         scenario::make_party(striker(kHit, /*atk=*/100)));
        BattleContext& ctx = b.ctx();
        b.start();
        const Round1 r1 = drive_round1(b);
        CHECK(r1.raw > 0, "T3 前置：攻方造成了裸伤");
        CHECK_EQ(r1.loss, r1.raw / 2, "T3 核心：100% 概率命中，R1 全源减半");
        drive_next_round(b);
        const int loss2 = 1000000 - ctx.getPet(0).hp - r1.loss;
        CHECK_EQ(loss2, ctx.ws.raw_attack_damage[1] / 2, "T3 核心：R2 仍在窗内减半");
        drive_next_round(b);
        const int loss3 = 1000000 - ctx.getPet(0).hp - r1.loss - loss2;
        CHECK_EQ(loss3, ctx.ws.raw_attack_damage[1], "T3 到期：rounds=2，R3 全额");
        std::cout << "  [T3] 127 概率减半窗 + 到期 通过\n";
    }

    //========== T4 576 低于阈值免疫：raw<200 免疫 / raw≥200 落地 ==========
    {
        sbs_srand(20260934);
        ScenarioBattle b(scenario::make_party(window_holder(16147 /*圣甲地裂崩 [2,200]*/)),
                         scenario::make_party(striker(kHit, /*atk=*/5)));
        BattleContext& ctx = b.ctx();
        b.start();
        const Round1 r1 = drive_round1(b);
        CHECK(r1.raw > 0 && r1.raw < 200, "T4 前置：裸伤在阈值之下");
        CHECK_EQ(r1.loss, 0, "T4 核心：低于 200 的攻击伤害免疫");
        std::cout << "  [T4a] 576 低于阈值免疫 通过\n";
    }
    {
        sbs_srand(20260935);
        ScenarioBattle b(scenario::make_party(window_holder(16147)),
                         scenario::make_party(striker(kHit, /*atk=*/3000)));
        BattleContext& ctx = b.ctx();
        b.start();
        const Round1 r1 = drive_round1(b);
        CHECK(r1.raw >= 200, "T4b 前置：裸伤在阈值之上");
        CHECK_EQ(r1.loss, r1.raw, "T4b 核心：高于阈值照常落地");
        std::cout << "  [T4b] 576 高于阈值落地 通过\n";
    }

    //========== T5 741 大于阈值免疫：raw>280 免疫 / raw<280 落地 ==========
    {
        sbs_srand(20260936);
        ScenarioBattle b(scenario::make_party(window_holder(26341 /*荧光守护 [2,280]*/)),
                         scenario::make_party(striker(kHit, /*atk=*/3000)));
        BattleContext& ctx = b.ctx();
        b.start();
        const Round1 r1 = drive_round1(b);
        CHECK(r1.raw > 280, "T5 前置：裸伤在阈值之上");
        CHECK_EQ(r1.loss, 0, "T5 核心：大于 280 的攻击伤害免疫");
        std::cout << "  [T5a] 741 大于阈值免疫 通过\n";
    }
    {
        sbs_srand(20260937);
        ScenarioBattle b(scenario::make_party(window_holder(26341)),
                         scenario::make_party(striker(kHit, /*atk=*/100)));
        BattleContext& ctx = b.ctx();
        b.start();
        const Round1 r1 = drive_round1(b);
        CHECK(r1.raw > 0 && r1.raw < 280, "T5b 前置：裸伤在阈值之下");
        CHECK_EQ(r1.loss, r1.raw, "T5b 核心：低于阈值照常落地");
        std::cout << "  [T5b] 741 低于阈值落地 通过\n";
    }

    std::cout << "scenario_490 all passed" << std::endl;
    return 0;
}
