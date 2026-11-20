// 场景 7620 —— J318 消除成功条件族（effect 655，moves_lib/j_dispel_success_cond.cpp）
//（2026-11-02，a01-jnight4；号段 7620-7629 一文件覆盖 8 载体）
//
// 机制：655 = 「消除{0}状态，消除成功则{1}的{2}」（模板有损，八参实解）：
//   args[0] 消除对象（0=对手提升→clear_stat_boosts 查 STAT_CLEAR；1=自身下降→
//           clear_stat_drops 不查免疫）；args[1] 后缀目标（0=对手/1=自身）；
//   args[2..7] 六槽增量官方语义序（攻,防,特攻,特防,速,命中）→ 引擎槽 {0,2,1,3,4,5}；
//   负增量→stat_drop（吃免弱）、正增量→stat_change。
//
// 断言口径（790 消强成功族场景同范式）：
//   - 前置态直写 ability_levels 本体（730/650/790 同款）；载体必中（消 miss 噪声）；
//   - 载体红伤/链上固伤兄弟（19615 的 943、31450 的 497 均已实现）只落 hp，
//     不污染等级断言；等级断言全精确值（无随机）；
//   - 双序反证：16399「双攻-1」语义 idx2（特攻）必须落引擎槽 1，防槽（槽 2）恒 0
//     ——按引擎序直读的实现此处必炸；
//   - 次序钉死：16426 自身特防 -1 → 先清（0）后升（+1）终值 +1；
//   - 免弱路：对手挂 harness 免弱魂印（STAT_DROP 票）→ 消强仍成功（clear 查的是
//     STAT_CLEAR）但 stat_drop 全 IMMUNE；
//   - 免消除强化路：对手挂 STAT_CLEAR 票 → clear_stat_boosts 返回 0 → 后缀不接。
// 无 while(!FINISHED) 步进：全部 submit_operation + drive 单回合驱动。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kHeJin   = 16399;  // 合金烈爪：[655] → [1,0,-1,0,-1,0,0,0]（解除自身弱化，双攻-1）
constexpr int kGangYi  = 16426;  // 钢翼切割：[655] → [1,1,0,0,0,1,0,0]（解除自身弱化，自身特防+1）
constexpr int kFengLin = 17018;  // 风凛一击：[655] → [1,0,0,0,0,0,-2,0]（解除自身下降，对手速-2）
constexpr int kHuiMie  = 17101;  // 毁灭进攻：[655] → [0,0,0,0,0,0,-2,-2]（消对手强化，速/命中-2）
constexpr int kJueXing = 17153;  // 觉醒粒子：[655] → [0,1,0,0,0,0,1,0]（消对手提升，自身速+1）
constexpr int kKuangYe = 19615;  // 狂野搏击：[655,943] → 655=[0,1,0,1,0,1,0,0]、943=[30]
constexpr int kQiangLi = 19785;  // 强力箭：[655] → [0,1,1,1,1,1,1,1]（消对手提升，自身全+1）
constexpr int kBiShui  = 31450;  // 碧水弦音：[655,497] → 655=[0,1,0,0,1,0,1,0]、497=[100,50,200]
constexpr int kPaDa    = 10004;  // 拍打：filler

// 引擎等级槽（0攻 1特攻 2防 3特防 4速 5命中）
constexpr int kAtkSlot = 0, kSpAtkSlot = 1, kDefSlot = 2, kSpDefSlot = 3, kSpdSlot = 4,
              kAccSlot = 5;

void always_hit(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
}

ElfPet self_pet(std::array<int, 5> skills, int atk = 300, int hp = 99999) {
    ElfPet p = make_pet(1, "己方", skills, /*speed=*/300, atk, hp);
    always_hit(p);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, SoulMark mark = {}) {
    ElfPet p = make_pet(2, "对手", skills, /*speed=*/100, /*atk=*/1, /*hp=*/99999, mark);
    always_hit(p);
    return p;
}

void play_round(ScenarioBattle& b, int self_slot) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, self_slot);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

int lvl(BattleContext& ctx, int side, int slot) {
    return ctx.ability_levels[side][slot];
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7620 16399 合金烈爪 成功路：解除自身弱化 → 对手双攻-1（双序反证钉死）══
    {
        ScenarioBattle b(make_party(self_pet({kHeJin, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[0][kAtkSlot] = -2;   // 自身预置攻-2（解除门成立）
        play_round(b, 0);
        CHECK_EQ(lvl(ctx, 0, kAtkSlot), 0, "T7620 核心：自身弱化被解除（-2 → 0）");
        CHECK_EQ(lvl(ctx, 1, kAtkSlot), -1, "T7620 核心：对手攻-1（语义 idx0 → 引擎槽0）");
        CHECK_EQ(lvl(ctx, 1, kSpAtkSlot), -1, "T7620 核心：对手特攻-1（语义 idx2 → 引擎槽1）");
        CHECK_EQ(lvl(ctx, 1, kDefSlot), 0, "T7620 双序反证：防槽（引擎2）必须为 0——按引擎序直读会错落此槽");
        CHECK_EQ(lvl(ctx, 1, kSpDefSlot), 0, "T7620 对照：对手特防槽 0");
        CHECK_EQ(lvl(ctx, 1, kSpdSlot), 0, "T7620 对照：对手速槽 0");
        CHECK_EQ(lvl(ctx, 1, kAccSlot), 0, "T7620 对照：对手命中槽 0");
        std::cout << "  [T7620] 655 合金烈爪 解除自身弱化→双攻-1 通过\n";
    }

    // ══ T7621 17018 风凛一击 成功路：解除自身下降 → 对手速-2 ══
    {
        ScenarioBattle b(make_party(self_pet({kFengLin, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[0][kSpdSlot] = -1;   // 自身预置速-1
        play_round(b, 0);
        CHECK_EQ(lvl(ctx, 0, kSpdSlot), 0, "T7621 核心：自身下降被解除");
        CHECK_EQ(lvl(ctx, 1, kSpdSlot), -2, "T7621 核心：对手速-2（语义 idx4 → 引擎槽4）");
        CHECK_EQ(lvl(ctx, 1, kAtkSlot), 0, "T7621 对照：对手攻槽 0");
        std::cout << "  [T7621] 655 风凛一击 解除自身下降→对手速-2 通过\n";
    }

    // ══ T7622 17101 毁灭进攻 成功路：消除对手强化 → 对手速-2、命中-2 ══
    {
        ScenarioBattle b(make_party(self_pet({kHuiMie, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][kAtkSlot] = 2;    // 对手预置攻+2（消强门成立）
        play_round(b, 0);
        CHECK_EQ(lvl(ctx, 1, kAtkSlot), 0, "T7622 核心：对手强化被清零");
        CHECK_EQ(lvl(ctx, 1, kSpdSlot), -2, "T7622 核心：对手速-2（语义 idx4）");
        CHECK_EQ(lvl(ctx, 1, kAccSlot), -2, "T7622 核心：对手命中-2（语义 idx5 → 引擎槽5）");
        CHECK_EQ(lvl(ctx, 0, kSpdSlot), 0, "T7622 对照：自身速槽 0（后缀目标=对手）");
        std::cout << "  [T7622] 655 毁灭进攻 消强→速/命中-2 通过\n";
    }

    // ══ T7623 17153 觉醒粒子 成功路：消除对手提升 → 自身速+1 ══
    {
        ScenarioBattle b(make_party(self_pet({kJueXing, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][kSpDefSlot] = 1;  // 对手预置特防+1（任意正等级保证消强成功）
        play_round(b, 0);
        CHECK_EQ(lvl(ctx, 1, kSpDefSlot), 0, "T7623 核心：对手提升被清零");
        CHECK_EQ(lvl(ctx, 0, kSpdSlot), 1, "T7623 核心：自身速+1（后缀目标=自身）");
        CHECK_EQ(lvl(ctx, 1, kSpdSlot), 0, "T7623 对照：对手速槽 0");
        std::cout << "  [T7623] 655 觉醒粒子 消强→自身速+1 通过\n";
    }

    // ══ T7624 16426 钢翼切割 成功路：先清（自身-1→0）后升（特防+1）——次序钉死 ══
    {
        ScenarioBattle b(make_party(self_pet({kGangYi, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[0][kSpDefSlot] = -1;  // 自身预置特防-1
        play_round(b, 0);
        CHECK_EQ(lvl(ctx, 0, kSpDefSlot), 1,
                 "T7624 核心：特防终值 +1（先清后升；若先升后清会被清回 0——语序钉死）");
        CHECK_EQ(lvl(ctx, 1, kSpDefSlot), 0, "T7624 对照：对手特防槽 0（后缀目标=自身）");
        CHECK_EQ(lvl(ctx, 0, kAtkSlot), 0, "T7624 对照：自身攻槽 0");
        std::cout << "  [T7624] 655 钢翼切割 先清后升次序 通过\n";
    }

    // ══ T7625 19785 强力箭 成功路：消除对手提升 → 自身全六槽+1 ══
    {
        ScenarioBattle b(make_party(self_pet({kQiangLi, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][kAtkSlot] = 1;    // 对手预置攻+1
        play_round(b, 0);
        CHECK_EQ(lvl(ctx, 1, kAtkSlot), 0, "T7625 核心：对手强化被清零");
        for (int slot = 0; slot < 6; ++slot) {
            CHECK_EQ(lvl(ctx, 0, slot), 1, "T7625 核心：自身六槽全 +1（[1,1,1,1,1,1]）");
        }
        std::cout << "  [T7625] 655 强力箭 消强→自身全属性+1 通过\n";
    }

    // ══ T7626 失败路双门：对手无提升 / 自身无下降 → 后缀全不接 ══
    {
        // (a) 觉醒粒子：对手无提升 → 消强未成功 → 自身速 0
        {
            ScenarioBattle b(make_party(self_pet({kJueXing, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            play_round(b, 0);
            CHECK_EQ(lvl(ctx, 0, kSpdSlot), 0,
                     "T7626a 核心：对手无提升 → 消除未成功 → 自身不提升");
        }
        // (b) 钢翼切割：自身无下降 → 解除未成功 → 自身特防 0
        {
            ScenarioBattle b(make_party(self_pet({kGangYi, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            play_round(b, 0);
            CHECK_EQ(lvl(ctx, 0, kSpDefSlot), 0,
                     "T7626b 核心：自身无下降 → 解除未成功 → 不接特防+1");
        }
        std::cout << "  [T7626] 655 失败路双门（消强/解除）通过\n";
    }

    // ══ T7627 免弱路：消强仍成功（clear 查 STAT_CLEAR 且**清所有正等级**），
    //    速/命中的 -2 降被免弱挡下（对照 T7622 无免弱时终值 -2）══
    {
        ScenarioBattle b(make_party(self_pet({kHuiMie, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            make_stat_drop_immunity_soul_mark())));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][kAtkSlot] = 2;    // 对手预置攻+2（消强门成立）
        ctx.ability_levels[1][kSpdSlot] = 3;    // 对手预置速+3（先被消强门清回 0，-2 再被免弱挡）
        play_round(b, 0);
        CHECK_EQ(lvl(ctx, 1, kAtkSlot), 0, "T7627 核心：消强不受免弱影响（查的是 STAT_CLEAR）→ 攻清零");
        CHECK_EQ(lvl(ctx, 1, kSpdSlot), 0,
                 "T7627 核心：速=0（消强门先清 +3；-2 被免弱挡下——无免弱对照 T7622 终值 -2）");
        CHECK_EQ(lvl(ctx, 1, kAccSlot), 0,
                 "T7627 核心：命中=0（-2 被免弱挡下，本体原值 0）");
        std::cout << "  [T7627] 655 免弱路（清强照常/降被免）通过\n";
    }

    // ══ T7628 免消除强化路：对手 STAT_CLEAR 票 → clear=0 → 消除未成功 → 后缀不接 ══
    {
        ScenarioBattle b(make_party(self_pet({kJueXing, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][kAtkSlot] = 2;    // 对手预置攻+2（有强化但被免消除保护）
        ctx.grant_immunity(1, ImmunityType::STAT_CLEAR, coverage_all(), /*anomaly_mask=*/0,
                           /*duration_rounds=*/9, /*source_id=*/9996, /*soul_immunity=*/false,
                           EffectScope::ON_STAGE, /*counts=*/0, /*source_effect_id=*/9996);
        play_round(b, 0);
        CHECK_EQ(lvl(ctx, 1, kAtkSlot), 2, "T7628 核心：免消除强化 → 对手攻+2 保持（clear 返回 0）");
        CHECK_EQ(lvl(ctx, 0, kSpdSlot), 0, "T7628 核心：消除未成功 → 自身速不提升");
        std::cout << "  [T7628] 655 免消除强化路（门槛 0）通过\n";
    }

    // ══ T7629 链切片双载体：arg_count 切片（655 取前 8 参、兄弟效果续切）══
    {
        // (a) 19615 狂野搏击 [655,943]：655=[0,1,0,1,0,1,0,0] → 自身防+1、特防+1
        {
            ScenarioBattle b(make_party(self_pet({kKuangYe, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.ability_levels[1][kSpdSlot] = 1;   // 对手预置速+1（消强门成立）
            play_round(b, 0);
            CHECK_EQ(lvl(ctx, 1, kSpdSlot), 0, "T7629a 核心：对手速+1 被清零");
            CHECK_EQ(lvl(ctx, 0, kDefSlot), 1, "T7629a 核心：自身防+1（语义 idx1 → 引擎槽2）");
            CHECK_EQ(lvl(ctx, 0, kSpDefSlot), 1, "T7629a 核心：自身特防+1（语义 idx3 → 引擎槽3）");
            CHECK_EQ(lvl(ctx, 0, kSpAtkSlot), 0, "T7629a 反证：特攻槽 0——切片第 9 参（30）不属 655");
        }
        // (b) 31450 碧水弦音 [655,497]：655=[0,1,0,0,1,0,1,0] → 自身特攻+1、速+1
        {
            ScenarioBattle b(make_party(self_pet({kBiShui, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.ability_levels[1][kAtkSlot] = 1;   // 对手预置攻+1（消强门成立）
            play_round(b, 0);
            CHECK_EQ(lvl(ctx, 1, kAtkSlot), 0, "T7629b 核心：对手攻+1 被清零");
            CHECK_EQ(lvl(ctx, 0, kSpAtkSlot), 1, "T7629b 核心：自身特攻+1（语义 idx2 → 引擎槽1）");
            CHECK_EQ(lvl(ctx, 0, kSpdSlot), 1, "T7629b 核心：自身速+1（语义 idx4 → 引擎槽4）");
            CHECK_EQ(lvl(ctx, 0, kDefSlot), 0, "T7629b 反证：防槽 0——497 的 [100,50,200] 不混入 655 切片");
        }
        std::cout << "  [T7629] 655 链切片双载体（19615/31450）通过\n";
    }

    std::cout << "scenario_7620 ALL PASS" << std::endl;
    return 0;
}
