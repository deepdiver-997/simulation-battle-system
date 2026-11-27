// 场景 8020：J333 效果 1296「{0}回合内受到攻击{1}%进入自然庇佑」——装配/窗界/资格/生命周期
//（resources/moves_lib/j_nature_blessing.cpp，2026-11-06；设计权威 S25 调研 §4.3/§4.4）
//
// 覆盖：
//   T1  37119 根系纯链 [5,100]：受击窗建立 + 合格受击 → 三档回血 + 审计链完整
//       （P=100 入口不耗抽取 rg=-1；伤害浮动只吃前 1~2 抽，档位抽取 = 末抽）
//   T2  属性技不合格（无伤害阶段 → 无受击事件 → 回调不进）
//   T3  粉伤不合格（FIXED kind 过滤 → 拒绝原因 3）
//   T4  窗过期（N=5：roundCount=5 受击 → 拒绝原因 2；末回合 roundCount=4 仍治疗）
//   T5  后手顺延（起点 = 使用回合+1；对手先手时使用当回合受击在观察者注册前
//       已投递 → 天然不入窗；次回合起算窗内受击治疗）
//   T6  刷新不叠观察者（重放使用：guard 旗不变，一次受击恰一次治疗尝试）
//   T7  实时 P 换载体（P100 → P80 后写生效，掷点用新 P；窗界按新 N 刷新）
//   T8  双方镜像（两窗按各自先/后手时点独立运转，审计键按 owner×16 隔离）
//
// 确定性纪律：概率类效果**不做随机断言**——审计槽（1012960000 段）读数精确断言；
// 档位/掷点边界用钉种子（MINSTD 闭式解：seed = 目标态 × 16807^-k mod 2^31-1；
// 实证 424242 链 (53,17,93) 与探针 state 全等）。power-0 保底伤 ∈ {0,1}，
// 涉 hp 的断言以二值集合表达、以审计 act 精确锁定治疗量。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>
#include <utils/sbs_rand.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kGen = 37119;      // 根系：纯链 [1296]，[5,100]，特殊 45
constexpr int kAnxiang = 31893;  // 暗香疏影阵：[766,1296]，[3,80]（766 对手无提升可消→空转）
constexpr int kHit = 10004;      // 拍打（物攻 40）
constexpr int kZero = 10938;     // 本能一击（power 0：受击事件，伤害 0~1）
constexpr int kAttr = 22638;     // 虚空之遗（属性技，无伤害阶段 → 无受击事件）

// 审计槽读数（键段与插件头注一致：1012960000 + owner*16 + slot）
constexpr int kAuditBase = 1012960000;
int aint(BattleContext& ctx, int owner, int slot, int def = 0) {
    const auto it = ctx.plugin_storage.find(kAuditBase + owner * 16 + slot);
    if (it == ctx.plugin_storage.end()) {
        return def;
    }
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : def;
}

ElfPet wei(std::array<int, 5> skills, int speed = 200, int atk = 100, int hp = 100000) {
    ElfPet p = scenario::make_pet(0, "庇佑方", skills, speed, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe(std::array<int, 5> skills, int speed = 100, int atk = 10, int hp = 100000) {
    ElfPet p = scenario::make_pet(1, "对手", skills, speed, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== T1 受击窗建立 + 合格受击回血（钉种子 424242：档位抽取=93→400 档）==========
    {
        ScenarioBattle b(scenario::make_party(wei({kGen, kHit, kZero, kZero, kZero})),
                        scenario::make_party(foe({kZero, kZero, kZero, kZero, kZero})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 1000;   // 缺口 99000 > 400 → 实际治疗 == 名义
        ctx.seed_rng(424242);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        // 窗界：InRounds 先手 [0,5)，P/guard 申报
        CHECK_EQ(aint(ctx, 0, 0, -1), 0, "T1 窗起点=使用回合（先手不顺延）");
        CHECK_EQ(aint(ctx, 0, 1, -1), 5, "T1 窋终点=起点+N（exclusive）");
        CHECK_EQ(aint(ctx, 0, 2, -1), 100, "T1 申报 P=100");
        CHECK_EQ(aint(ctx, 0, 3, -1), 1, "T1 watcher 守卫旗置位");
        // 合格受击 + 审计链
        CHECK_EQ(aint(ctx, 0, 4), 1, "T1 合格事件数=1");
        CHECK_EQ(aint(ctx, 0, 5), 1, "T1 入口成功数=1");
        CHECK_EQ(aint(ctx, 0, 13, -1), -1, "T1 P>=100 入口不耗抽取（rg=-1）");
        CHECK_EQ(aint(ctx, 0, 6, -1), 93, "T1 选档原始抽取 r=93（钉种子 424242 第 3 抽）");
        CHECK_EQ(aint(ctx, 0, 7, -1), 2, "T1 档位=2（r>=80 → 400 档）");
        CHECK_EQ(aint(ctx, 0, 8), 400, "T1 名义治疗=400");
        CHECK_EQ(aint(ctx, 0, 9), 400, "T1 实际治疗=400（缺口充足不钳制）");
        CHECK_EQ(aint(ctx, 0, 10, -1), 0, "T1 无拒绝（rej=0）");
        CHECK_EQ(aint(ctx, 0, 11), 1, "T1 受击事件回合+1=1（roundCount 0）");
        CHECK_EQ(aint(ctx, 0, 12), 1, "T1 治疗尝试数=1");
        CHECK_EQ(ctx.getPet(0).hp, 1399, "T1 hp=1000-1(power0 保底伤,钉种子)+400");
        std::cout << "  [T1] 37119 窗建立+回血+审计链 通过\n";
    }

    //========== T2 属性技不合格：无伤害事件 → 回调不进 ==========
    {
        ScenarioBattle b(scenario::make_party(wei({kGen, kHit, kZero, kZero, kZero})),
                        scenario::make_party(foe({kAttr, kZero, kZero, kZero, kZero})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seed_rng(424242);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(aint(ctx, 0, 4), 0, "T2 合格事件数=0（属性技无伤害阶段）");
        CHECK_EQ(aint(ctx, 0, 11), 0, "T2 无受击事件记录（回调未进）");
        CHECK_EQ(ctx.getPet(0).hp, 100000, "T2 我方零掉血（属性技不进伤害管线）");
        std::cout << "  [T2] 属性技不合格 通过\n";
    }

    //========== T3 粉伤不合格：FIXED kind 过滤（拒绝原因 3）==========
    {
        ScenarioBattle b(scenario::make_party(wei({kGen, kHit, kZero, kZero, kZero})),
                        scenario::make_party(foe({kAttr, kZero, kZero, kZero, kZero})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seed_rng(424242);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();   // R0：武装窗 [0,5)（对手属性技，无事件）
        CHECK_EQ(ctx.getPet(0).hp, 100000, "T3 前置：R0 无掉血");
        // R1 前直调粉伤（deal_damage FIXED，同 pink 路径 emit 点，actor=对手）→ drain 投递
        deal_damage(&ctx, 0, 100, DamageKind::FIXED, 1);
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(aint(ctx, 0, 10, -1), 3, "T3 拒绝原因=3（非红伤）");
        CHECK_EQ(aint(ctx, 0, 4), 0, "T3 合格事件数=0（粉伤不算「受到攻击伤害」）");
        CHECK_EQ(aint(ctx, 0, 12), 0, "T3 无治疗尝试");
        CHECK_EQ(ctx.getPet(0).hp, 99900, "T3 hp=100000-100（粉伤落地但不触发回血）");
        std::cout << "  [T3] 粉伤不合格 通过\n";
    }

    //========== T4 窗过期：N=5 末回合（roundCount=4）仍治疗、roundCount=5 拒绝 ==========
    {
        ScenarioBattle b(scenario::make_party(wei({kGen, kHit, kZero, kZero, kZero})),
                        scenario::make_party(foe({kAttr, kZero, kZero, kZero, kZero})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seed_rng(424242);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();   // R0：武装窗 [0,5)（对手属性技，无事件）
        CHECK_EQ(aint(ctx, 0, 1, -1), 5, "T4 前置：窗终点=5");
        ctx.getPet(0).hp = 1000;   // 压低血量：缺口 > 400 → 治疗不被满血钳制
        ctx.seed_rng(7);
        for (int r = 0; r < 3; ++r) {   // R1..R3：双方 NONE（零抽取空转回合）
            b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
        }
        CHECK_EQ(ctx.roundCount, 4, "T4 前置：3 个空转回合后 roundCount=4（窗内末回合）");
        CHECK_EQ(aint(ctx, 0, 4), 0, "T4 空转回合零事件（不合格不误触发）");
        // R5（roundCount=4）：窗内末回合受击 → 仍治疗（钉种子 1463021119：hit-only draw2=17→200）
        ctx.seed_rng(1463021119);
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        CHECK_EQ(aint(ctx, 0, 5), 1, "T4 末回合（4<5）仍合格");
        CHECK_EQ(aint(ctx, 0, 7, -1), 0, "T4 末回合档位=0（r=17<50）");
        CHECK_EQ(aint(ctx, 0, 9), 200, "T4 末回合治疗 200");
        const int hp_last = ctx.getPet(0).hp;
        CHECK(hp_last >= 1199 && hp_last <= 1200,
              "T4 hp=1000-伤害(0~1)+200");
        // R6（roundCount=5）：窗过期受击 → 拒绝原因 2、零治疗（钉种子 424242：hit-only 伤害=1）
        ctx.seed_rng(424242);
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        CHECK_EQ(aint(ctx, 0, 10, -1), 2, "T6 拒绝原因=2（窗已过期）");
        CHECK_EQ(aint(ctx, 0, 4), 1, "T4 合格事件数仍=1（过期事件不合格）");
        CHECK_EQ(ctx.getPet(0).hp, hp_last - 1, "T4 过期后受击零治疗（再掉 1 点保底伤）");
        std::cout << "  [T4] 窗过期 通过\n";
    }

    //========== T5 后手顺延：起点=使用回合+1；对手先手时当回合受击不入窗 ==========
    {
        ScenarioBattle b(scenario::make_party(wei({kGen, kHit, kZero, kZero, kZero},
                                                  /*speed=*/100)),
                        scenario::make_party(foe({kZero, kZero, kZero, kZero, kZero},
                                                 /*speed=*/200)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 1000;
        ctx.seed_rng(424242);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(aint(ctx, 0, 0, -1), 1, "T5 后手窗起点顺延=使用回合+1");
        CHECK_EQ(aint(ctx, 0, 1, -1), 6, "T5 窋终点=1+5");
        CHECK_EQ(aint(ctx, 0, 4), 0, "T5 当回合受击不合格（对手先手：事件先于观察者注册投递）");
        CHECK_EQ(aint(ctx, 0, 11), 0, "T5 该事件未进回调（evr=0）");
        CHECK_EQ(ctx.getPet(0).hp, 999, "T5 R1 掉 1 点、无治疗");
        // R2（roundCount=1 = 窗起点）：窗内首回合受击 → 治疗（钉种子 424242：hit-only draw2=17→200）
        ctx.seed_rng(424242);
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(aint(ctx, 0, 4), 1, "T5 R2 合格");
        CHECK_EQ(aint(ctx, 0, 7, -1), 0, "T5 R2 档位=0（r=17<50）");
        CHECK_EQ(aint(ctx, 0, 9), 200, "T5 R2 治疗 200");
        CHECK_EQ(ctx.getPet(0).hp, 1198, "T5 hp=999-1+200（钉种子伤害=1）");
        std::cout << "  [T5] 后手顺延 通过\n";
    }

    //========== T6 刷新不叠观察者：重放使用单 watcher、一次受击恰一次治疗 ==========
    {
        ScenarioBattle b(scenario::make_party(wei({kGen, kHit, kZero, kZero, kZero})),
                        scenario::make_party(foe({kAttr, kZero, kZero, kZero, kZero})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 1000;
        ctx.seed_rng(424242);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();   // R0：武装 [0,5)
        CHECK_EQ(aint(ctx, 0, 0, -1), 0, "T6 R0 窗起点=0");
        ctx.seed_rng(7);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();   // R1：重放使用（刷新窗界；对手属性技无事件）
        CHECK_EQ(aint(ctx, 0, 3, -1), 1, "T6 守卫旗不变（未重复注册观察者）");
        CHECK_EQ(aint(ctx, 0, 0, -1), 1, "T6 窋起点刷新为 R1");
        CHECK_EQ(aint(ctx, 0, 1, -1), 6, "T6 窋终点刷新为 1+5");
        // R2：受击一次（钉种子 765971402：hit-only draw2=50→300 档）
        ctx.seed_rng(765971402);
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        CHECK_EQ(aint(ctx, 0, 12), 1, "T6 治疗尝试数=1（一次受击恰一次治疗——双观察者会=2）");
        CHECK_EQ(aint(ctx, 0, 8), 300, "T6 名义 300（r=50）");
        CHECK(aint(ctx, 0, 9) == 300 && ctx.getPet(0).hp >= 1299 && ctx.getPet(0).hp <= 1300,
              "T6 单次治疗落地（1000-伤害(0~1)+300）");
        std::cout << "  [T6] 刷新不叠观察者 通过\n";
    }

    //========== T7 实时 P 换载体：P100 → P80 后写生效（窗界按新 N 刷新）==========
    {
        ScenarioBattle b(scenario::make_party(wei({kGen, kAnxiang, kZero, kZero, kZero})),
                        scenario::make_party(foe({kAttr, kZero, kZero, kZero, kZero})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 1000;
        ctx.seed_rng(424242);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();   // R0：根系 P=100 N=5
        CHECK_EQ(aint(ctx, 0, 2, -1), 100, "T7 R0 申报 P=100");
        ctx.seed_rng(7);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();   // R1：暗香疏影阵 P=80 N=3（刷新窗界+P）
        CHECK_EQ(aint(ctx, 0, 2, -1), 80, "T7 R1 申报 P 刷新为 80");
        CHECK_EQ(aint(ctx, 0, 0, -1), 1, "T7 窋起点刷新为 R1");
        CHECK_EQ(aint(ctx, 0, 1, -1), 4, "T7 窋终点按 31893 的 N=3 刷新（1+3）");
        // R2：受击（钉种子 608939394：hit-only draw2=79<80 过闸 → draw3=53→300 档）
        ctx.seed_rng(608939394);
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        CHECK_EQ(aint(ctx, 0, 13, -1), 79, "T7 入口掷点 r=79（钉种子）");
        CHECK_EQ(aint(ctx, 0, 5), 1, "T7 79<80 过闸");
        CHECK_EQ(aint(ctx, 0, 7, -1), 1, "T7 档位=1（r=53）");
        CHECK_EQ(aint(ctx, 0, 9), 300, "T7 治疗 300（按刷新后的 P=80 掷点）");
        std::cout << "  [T7] 实时 P 换载体 通过\n";
    }

    //========== T8 双方镜像：两窗按各自先/后手时点独立运转、审计键隔离 ==========
    {
        // 双方都持根系（P=100 N=5）；我方先手。R0 双方武装（后手窗顺延至 [1,6)）；
        // 对手先开火？不——我方先手先行动：我的根系先打对手（事件在对手武装前投递→对手不入窗），
        // 对手的根系后打我（我窗已建→我入窗治疗）。R1 双方 power-0 互拍 → 双窗各合格一次。
        ScenarioBattle b(scenario::make_party(wei({kGen, kZero, kHit, kZero, kZero})),
                        scenario::make_party(foe({kGen, kZero, kZero, kZero, kZero},
                                                 /*speed=*/100, /*atk=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 1000;
        ctx.getPet(1).hp = 1000;
        ctx.seed_rng(424242);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();   // R0：我先武装+先出手（根系 45 威力真伤对手），对手后武装+后出手（0~1 伤我）
        CHECK_EQ(aint(ctx, 0, 3, -1), 1, "T8 己方守卫旗");
        CHECK_EQ(aint(ctx, 1, 3, -1), 1, "T8 对方守卫旗（owner=1 独立键段）");
        CHECK_EQ(aint(ctx, 0, 4), 1, "T8 R0 己方合格=1（对手后手攻击进我窗）");
        CHECK_EQ(aint(ctx, 1, 4), 0, "T8 R0 对方不合格（我先手攻击在其武装前已投递）");
        CHECK_EQ(aint(ctx, 1, 0, -1), 1, "T8 对方后手窗起点顺延=1");
        CHECK_EQ(aint(ctx, 0, 8), 400, "T8 R0 己方名义 400（钉种子 424242 第 3 抽=93）");
        // 对手 R0 的武装出手（根系 45 威力 atk100）对我 ~37 实伤（钉种子确定性），
        // hp0 = 1000 - 37 + 400 = 1363
        CHECK_EQ(ctx.getPet(0).hp, 1363, "T8 R0 己方 hp=1000-37(对手根系实伤,钉种子)+400");
        // R1：双方 power-0 互拍 → 两窗各自合格、各自掷档（己方第 2 次、对方第 1 次）
        ctx.seed_rng(424242);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        CHECK_EQ(aint(ctx, 0, 4), 2, "T8 R1 己方合格=2");
        CHECK_EQ(aint(ctx, 1, 4), 1, "T8 R1 对方合格=1（窗起点 roundCount=1 起）");
        // 事件在各行动方 AFTER_ACTION 即投递（K1）：我方 kZero 事件先投递 → 对方档=第 2 抽（17）；
        // 对方 kZero 事件后投递 → 己方档=第 4 抽（70）
        CHECK_EQ(aint(ctx, 1, 8), 200, "T8 R1 对方名义 200（第 2 抽 r=17）");
        CHECK_EQ(aint(ctx, 1, 9), 200, "T8 R1 对方实际=名义");
        CHECK_EQ(aint(ctx, 0, 8), 300, "T8 R1 己方名义 300（第 4 抽 r=70）");
        CHECK_EQ(aint(ctx, 0, 9), 300, "T8 R1 己方实际=名义");
        CHECK(ctx.getPet(1).hp >= 1159 && ctx.getPet(1).hp <= 1170,
              "T8 对方 hp=1000-我方根系实伤(35±浮动)-power0伤(0~1)+200（治疗量已由审计 act 精确锁定）");
        CHECK(ctx.getPet(0).hp >= 1662 && ctx.getPet(0).hp <= 1663,
              "T8 己方 hp=1363-power0伤(0~1)+300");
        std::cout << "  [T8] 双方镜像隔离 通过\n";
    }

    std::cout << "scenario_8020 all passed" << std::endl;
    return 0;
}
