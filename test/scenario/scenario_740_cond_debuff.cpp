// 场景 740 —— G4 异常/降级条件族（745 / 167，moves_lib/g_cond_debuff.cpp）
//（2026-09-29，worktree ../agents/g4；号段 740-749 一文件覆盖两效果、四载体）
//
// 机制：
//   745  使对手{0}（六槽降级表·**官方语义序** 攻,防,特攻,特防,速,命中 → 引擎槽
//        {0,2,1,3,4,5}），对手处于**任意**异常状态 → 全表幅度 ×2（施加动作内判，
//        1214 遍历判据）——T1/T2 火舞枫叶（特防/命中 -1→-2）、T3/T4 炼狱环境
//        （双攻+命中 -1→-2；[-1,0,-1,0,0,-1] 的语义序读法把 防/特防 钉死——
//        按引擎序直读会落 攻+防，断言必炸）；
//   167  若对手六槽能力等级任一 <0（1244 同判据）→ 附加 {0} 点 FIXED 粉伤
//        （先判后加：无下降 → 0）——T5/T6 深海狂啮 [100]（负例/正例）、
//        T7/T8 残影叶刃闪 [30]（命中槽 -2 也判/无下降负例）、T9 镜像方向
//        （粉伤落在出手侧账本，方向纪律钉子）。
//
// 断言口径（常识速查 §五）：
//   - 745 直接断言 ability_levels 六槽权威态（stat_drop 落点，630 直写同源）；
//   - 167 粉伤 FIXED 精确分量：loss == raw ± {0}（raw = ws 伤害台账，85%~100%
//     红伤浮动被台账吸收，等式两侧同源浮动 → 恒成立）；
//   - 异常前置用 apply_anomaly 全检查链直注（730 T10 同款，Burn 恒 SUCCESS；
//     烧伤跳伤只动 hp、不碰能力槽 → 与 745 断言正交）；
//   - 探针/载体 all must_hit（消 miss 噪声）；对手/filler 用 ActionType::NONE
//     （威力 0 有 1 点保底伤，精确 hp 断言一律绕开保底路径）。
#include <algorithm>
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kHuoWuFengYe   = 17361;  // 火舞枫叶：特攻85 先制+2 [745][0,0,0,-1,0,-1] → 特防-1、命中-1
constexpr int kLianYuHuanJing = 23934; // 炼狱环境：属性技0 [745,48][-1,0,-1,0,0,-1,3] → 双攻-1、命中-1（链尾 48=3回合免异常，已实现不在本件断言）
constexpr int kShenHaiKongNie = 12887; // 深海狂啮：物攻150 [167][100]
constexpr int kCanYingYeRen   = 13423; // 残影叶刃闪：特攻150 [167][30]
constexpr int kPaDa           = 10004; // 拍打：物攻40 无效果（filler，730 同款）
constexpr int kAtkSlot   = 0;          // 等级槽 0攻
constexpr int kSpAtkSlot = 1;          // 1特攻
constexpr int kDefSlot   = 2;          // 2防
constexpr int kSpDefSlot = 3;          // 3特防
constexpr int kSpdSlot   = 4;          // 4速
constexpr int kAccSlot   = 5;          // 5命中
constexpr int kBurnId    = static_cast<int>(AbnormalStatusId::Burn);  // 2 烧伤

void always_hit(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
}

ElfPet self_pet(std::array<int, 5> skills, int atk, int hp, int speed = 300) {
    ElfPet p = make_pet(1, "己方", skills, speed, atk, hp);
    always_hit(p);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int atk = 1, int hp = 1000000, int speed = 100) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, hp);
    always_hit(p);
    return p;
}

// 己方出技能 + 对手 NONE（跨回合无 watcher，无额外 rand 消费者）。
void play_round_self_acts(ScenarioBattle& b, int self_slot) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, self_slot);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

// 对手出技能 + 己方 NONE（镜像方向：167 粉伤应落在出手侧=对手）。
void play_round_foe_acts(ScenarioBattle& b, int foe_slot) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_slot);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ════════════════════════════════════════════════════════════════
    // 745 火舞枫叶 [0,0,0,-1,0,-1]：特防/命中两槽
    // ════════════════════════════════════════════════════════════════

    // T1 对手无异常 → 基础幅度（特防-1、命中-1，其余槽不动）
    {
        ScenarioBattle b(make_party(self_pet({kHuoWuFengYe, kPaDa, kPaDa, kPaDa, kPaDa},
                                             /*atk=*/1, /*hp=*/99999)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round_self_acts(b, 0);
        CHECK_EQ(ctx.ability_levels[1][kSpDefSlot], -1,
                 "T1 核心：语义序 idx3=特防 → 槽3 -1");
        CHECK_EQ(ctx.ability_levels[1][kAccSlot], -1,
                 "T1 核心：语义序 idx5=命中 → 槽5 -1");
        CHECK_EQ(ctx.ability_levels[1][kAtkSlot], 0, "T1：攻槽不动");
        CHECK_EQ(ctx.ability_levels[1][kSpAtkSlot], 0, "T1：特攻槽不动");
        CHECK_EQ(ctx.ability_levels[1][kDefSlot], 0, "T1：防槽不动");
        CHECK_EQ(ctx.ability_levels[1][kSpdSlot], 0, "T1：速槽不动");
        std::cout << "  [T1] 745 火舞枫叶 无异常基础档（特防/命中 -1）通过\n";
    }

    // T2 对手烧伤 → 幅度翻倍（特防-2、命中-2）
    {
        ScenarioBattle b(make_party(self_pet({kHuoWuFengYe, kPaDa, kPaDa, kPaDa, kPaDa},
                                             /*atk=*/1, /*hp=*/99999)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        const ApplyAnomalyResult r =
            apply_anomaly(&ctx, 1, kBurnId, /*duration_rounds=*/3, /*actor=*/0);
        REQUIRE(r == ApplyAnomalyResult::SUCCESS);
        play_round_self_acts(b, 0);
        CHECK_EQ(ctx.ability_levels[1][kSpDefSlot], -2,
                 "T2 核心：对手处于异常 → 弱化翻倍（特防 -1×2）");
        CHECK_EQ(ctx.ability_levels[1][kAccSlot], -2,
                 "T2 核心：弱化翻倍（命中 -1×2）");
        std::cout << "  [T2] 745 火舞枫叶 异常翻倍档（特防/命中 -2）通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // 745 炼狱环境 [-1,0,-1,0,0,-1]：语义序钉子（双攻+命中；防/特防不动）
    //   若误按引擎序读（0攻1特攻2防…）→ 落点是 攻+防，本组断言必炸。
    // ════════════════════════════════════════════════════════════════

    // T3 对手无异常 → 攻/特攻/命中 -1，防/特防/速不动（语义序钉子）
    {
        ScenarioBattle b(make_party(self_pet({kLianYuHuanJing, kPaDa, kPaDa, kPaDa, kPaDa},
                                             /*atk=*/1, /*hp=*/99999)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round_self_acts(b, 0);
        CHECK_EQ(ctx.ability_levels[1][kAtkSlot], -1, "T3 核心：语义序 idx0=攻 → 槽0 -1");
        CHECK_EQ(ctx.ability_levels[1][kSpAtkSlot], -1,
                 "T3 核心：语义序 idx2=特攻 → 槽1 -1（引擎序误读会落防槽，必炸）");
        CHECK_EQ(ctx.ability_levels[1][kAccSlot], -1, "T3 核心：语义序 idx5=命中 → 槽5 -1");
        CHECK_EQ(ctx.ability_levels[1][kDefSlot], 0, "T3：防槽不动（idx1=0）");
        CHECK_EQ(ctx.ability_levels[1][kSpDefSlot], 0, "T3：特防槽不动（idx3=0）");
        CHECK_EQ(ctx.ability_levels[1][kSpdSlot], 0, "T3：速槽不动（idx4=0）");
        std::cout << "  [T3] 745 炼狱环境 语义序钉子（双攻+命中 -1）通过\n";
    }

    // T4 对手烧伤 → 双攻/命中 -2（翻倍 × 语义序双钉）
    {
        ScenarioBattle b(make_party(self_pet({kLianYuHuanJing, kPaDa, kPaDa, kPaDa, kPaDa},
                                             /*atk=*/1, /*hp=*/99999)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        const ApplyAnomalyResult r =
            apply_anomaly(&ctx, 1, kBurnId, /*duration_rounds=*/3, /*actor=*/0);
        REQUIRE(r == ApplyAnomalyResult::SUCCESS);
        play_round_self_acts(b, 0);
        CHECK_EQ(ctx.ability_levels[1][kAtkSlot], -2, "T4 核心：攻 -1×2");
        CHECK_EQ(ctx.ability_levels[1][kSpAtkSlot], -2, "T4 核心：特攻 -1×2");
        CHECK_EQ(ctx.ability_levels[1][kAccSlot], -2, "T4 核心：命中 -1×2");
        CHECK_EQ(ctx.ability_levels[1][kDefSlot], 0, "T4：防仍不动");
        std::cout << "  [T4] 745 炼狱环境 异常翻倍档（双攻+命中 -2）通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // 167 深海狂啮 [100] / 残影叶刃闪 [30]：对手六槽任一负等级 → 附加 FIXED 粉伤
    //   loss - raw = 粉伤精确分量（红伤浮动被台账同源吸收）。
    // ════════════════════════════════════════════════════════════════

    // T5 深海狂啮：对手满等级 → 不附加（loss == raw）
    {
        ScenarioBattle b(make_party(self_pet({kShenHaiKongNie, kPaDa, kPaDa, kPaDa, kPaDa},
                                             /*atk=*/2000, /*hp=*/99999)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round_self_acts(b, 0);
        const int raw = ctx.ws.raw_attack_damage[0];
        const int loss = 1000000 - ctx.getPet(1).hp;
        CHECK(raw > 0, "T5 前提：红伤台账非零");
        CHECK_EQ(loss, raw, "T5 核心：对手无能力下降 → 不附加（loss == raw）");
        std::cout << "  [T5] 167 深海狂啮 无下降负例 通过（raw " << raw << "）\n";
    }

    // T6 深海狂啮：对手攻槽 -1 → 附加恰 100（loss == raw + 100）
    {
        ScenarioBattle b(make_party(self_pet({kShenHaiKongNie, kPaDa, kPaDa, kPaDa, kPaDa},
                                             /*atk=*/2000, /*hp=*/99999)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][kAtkSlot] = -1;   // 六槽权威态直写（680 T5b 同款）
        play_round_self_acts(b, 0);
        const int raw = ctx.ws.raw_attack_damage[0];
        const int loss = 1000000 - ctx.getPet(1).hp;
        CHECK_EQ(loss, raw + 100, "T6 核心：对手有下降 → 附加 {0}=100 FIXED");
        std::cout << "  [T6] 167 深海狂啮 附加 100 通过\n";
    }

    // T7 残影叶刃闪：对手**命中槽** -2（非攻槽也判 → 任意槽遍历判据钉子）→ 附加恰 30
    {
        ScenarioBattle b(make_party(self_pet({kCanYingYeRen, kPaDa, kPaDa, kPaDa, kPaDa},
                                             /*atk=*/2000, /*hp=*/99999)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][kAccSlot] = -2;
        play_round_self_acts(b, 0);
        const int raw = ctx.ws.raw_attack_damage[0];
        const int loss = 1000000 - ctx.getPet(1).hp;
        CHECK_EQ(loss, raw + 30,
                 "T7 核心：命中槽下降也判 → 附加 {0}=30（第二载体 + 任意槽判据）");
        std::cout << "  [T7] 167 残影叶刃闪 附加 30（命中槽下降）通过\n";
    }

    // T8 残影叶刃闪：对手满等级 → 不附加（第二载体负例）
    {
        ScenarioBattle b(make_party(self_pet({kCanYingYeRen, kPaDa, kPaDa, kPaDa, kPaDa},
                                             /*atk=*/2000, /*hp=*/99999)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round_self_acts(b, 0);
        const int raw = ctx.ws.raw_attack_damage[0];
        const int loss = 1000000 - ctx.getPet(1).hp;
        CHECK_EQ(loss, raw, "T8 核心：第二载体无下降 → 不附加");
        std::cout << "  [T8] 167 残影叶刃闪 无下降负例 通过\n";
    }

    // T9 镜像方向：对手用深海狂啮、己方速槽 -1 → 粉伤落在**出手侧**（对手）账本，
    //    己方掉血 == raw1 + 100（方向纪律钉子：owner/target 不反转）。
    {
        ScenarioBattle b(make_party(self_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                             /*atk=*/1, /*hp=*/999999, /*speed=*/100)),
                         make_party(foe_pet({kShenHaiKongNie, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*atk=*/1, /*hp=*/1000000, /*speed=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[0][kSpdSlot] = -1;   // 己方有下降 → 对手的 167 判据成立
        play_round_foe_acts(b, 0);
        const int raw1 = ctx.ws.raw_attack_damage[1];
        const int loss = 999999 - ctx.getPet(0).hp;
        CHECK(raw1 > 0, "T9 前提：对手红伤台账非零");
        CHECK_EQ(loss, raw1 + 100,
                 "T9 核心：167 落在出手侧槽 → 己方承伤 = 红伤 + 100 粉伤");
        std::cout << "  [T9] 167 镜像方向（对手出手 +100）通过\n";
    }

    std::cout << "scenario_740_cond_debuff PASS (G4 异常/降级条件族 745/167)" << std::endl;
    return 0;
}
