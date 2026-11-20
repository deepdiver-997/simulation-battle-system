#include <utils/sbs_rand.h>
// 场景 881 —— X2 effect 1248 异常码错参修正回归（custom_effect_programs 数据修复）
//（2026-10-01，worktree ../agents/x-fix-1；小修批工单-2026-09-30 X2，段 881）
//
// 缺陷与修法：1248 官方模板 = "对手处于异常状态时{0}%令对手{1}"（{0}=概率、{1}=异常码，
// 全库 22 载体同构）。custom_effect_programs 第 5 行（无相谛 1248 迁移行）的 unit_json
// 写成 param0={"arg":0}——把 {0} 概率（无相谛=100）填进了 Anomaly 异常码槽，且概率
// 参数未接线（<100% 载体变必发、异常码恒错）。执行器链（effect_unit.cpp run_primitive
// Anomaly → apply_anomaly(param0)）实证 p0=100 p1=2（工单 ASAN 快照）。
// 修法 = 纯数据行修正（引擎零改动），两处：
//   ① param0={"arg":1}（31=臣服 AbnormalStatusId::Submission）；
//   ② 概率接线用 chance_value={"arg":0}（装载期对 record.args 求值）——⚠️ 首修踩坑记档：
//      写成 chance_arg:0 不动（chance_arg 是**运行时**下标、指向带 owner/target 前缀的
//      EffectArgs.int_args，[0]=owner 恒 0 → 概率恒 0 → 单元永 Never，skill load 正常、
//      静默不触发）；JSON 里 {"arg":n} 全族都是装载期对 record.args（无前缀）求值。
//
// 断言组：
//   881a 主路径：无相谛（37460，1248 切片=[100,31]）打**已有异常**的对手 →
//        对手获得臣服(31)（修复前：异常码槽=100 越界 → 子句空转，无臣服）。
//   881b 互斥对照：同载体打**无异常**的对手 → 1257（无异常吸取 1/3）走吸取、
//        1248 前置条件不成立 → 无臣服（条件对依然成立，不是"永远上臣服"）。
//   881c 第二载体：黑月悬空（32982 [1248] 切片=[50,7]，官方"对手处于异常状态时50%
//        令对手疲惫"）→ 先挂毒造前置，40 种子扫描触发数 >0，且触发即疲惫(7) 无码错位
//        （修复前：异常码=50 越界全空转，触发数恒 0——修复后必 >0）。
//
// 链伴生自查（无相谛六单元，self 后手局）：179 同系威力（异系 no-op；同系也只加威力，
// atk=1 方向断言不受影响）；700 先手降 PP（self 后手 no-op）；1083 后手消对手回合类
// （对手无回合类 no-op）；1257 无异常吸取（881a 对手有毒 → no-op，881b 成立=对照）；
// 456 体力<300 秒杀（对手 hp 充裕 no-op）。对手 NONE 出手保稳定。
// ⚠️ 无相谛 DB max_pp=0（第五技能官方无 PP 列，4399 文本 pp="--"）→ 场景手术
//    skills[0].pp=3，否则"Skill not usable"整场空转（首跑踩坑记档）。
#include <array>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kWuXiangDi = 37460;   // 无相谛：[179,700,1083,1257,1248,456] | [170,1,3,100,31,300]
constexpr int kHeiYue = 32982;      // 黑月悬空：[1248] | [50,7] → 异常态对手 50% 疲惫(7)
constexpr int kPaDa = 10004;        // 拍打：物攻40 无效果（filler）

constexpr int kSubmissionId = static_cast<int>(AbnormalStatusId::Submission);   // 31
constexpr int kPoisonId = static_cast<int>(AbnormalStatusId::Poison);           // 1
constexpr int kWearinessId = 7;                                                 // 疲惫

ElfPet self_pet(const std::array<int, 5>& skills, int speed, int atk, int hp) {
    ElfPet p = make_pet(1, "己方", skills, speed, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet(const std::array<int, 5>& skills, int speed, int atk, int hp) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

void play_round_foe_none(ScenarioBattle& b, int self_slot) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, self_slot);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

bool has_anomaly(BattleContext& ctx, int side, int id) {
    return ctx.has_active_abnormal_status(side, id);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ════════════════════════════════════════════════════════════════
    // 881a 主路径：无相谛打异常态对手 → 臣服(31)
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kWuXiangDi, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/100, /*atk=*/1, /*hp=*/5000)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*speed=*/300, /*atk=*/1, /*hp=*/100000)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).skills[0].pp = 3;   // 无相谛 max_pp=0（第五技能），手术给 PP
        CHECK(try_apply_anomaly(&ctx, 1, kPoisonId, /*duration=*/5, /*actor=*/0),
              "881a 前提：对手挂毒（TargetHasAnomaly 前置）");
        play_round_foe_none(b, 0);   // R1：self 后手无相谛 → 1248 单元 100% 臣服
        CHECK(has_anomaly(ctx, 1, kSubmissionId),
              "881a 核心：异常态对手被打出臣服(31)——{1} 落位正确（修复前异常码槽=100 空转）");
        std::cout << "  [881a] 1248 异常码落位 主路径 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // 881b 互斥对照：打无异常对手 → 1257 吸取、1248 不触发
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kWuXiangDi, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/100, /*atk=*/1, /*hp=*/5000)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*speed=*/300, /*atk=*/1, /*hp=*/100000)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).skills[0].pp = 3;   // PP 手术（同 881a）
        const int foe_hp_before = ctx.getPet(1).hp;
        play_round_foe_none(b, 0);   // R1：对手无异常 → 1257 吸取最大体力 1/3、1248 不成立
        CHECK(!has_anomaly(ctx, 1, kSubmissionId),
              "881b 核心：无异常对手不上臣服（TargetHasAnomaly 前置互斥仍成立）");
        CHECK(ctx.getPet(1).hp < foe_hp_before,
              "881b 对照：1257 无异常吸取照常（1/3 百分比伤，方向断言）");
        std::cout << "  [881b] 1248/1257 条件互斥对照 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // 881c 第二载体：黑月悬空 50% 疲惫——概率接线 + 异常码双双验证
    // ════════════════════════════════════════════════════════════════
    {
        int fired = 0;
        bool wrong_code = false;
        for (int seed = 1; seed <= 40; ++seed) {
            sbs_srand(seed);
            ScenarioBattle b(make_party(self_pet({kHeiYue, kPaDa, kPaDa, kPaDa, kPaDa},
                                                  /*speed=*/100, /*atk=*/1, /*hp=*/5000)),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                                /*speed=*/300, /*atk=*/1, /*hp=*/100000)));
            b.start();
            // 1248 全系带 TargetHasAnomaly 前置（官方"对手处于异常状态时"）：先挂毒。
            try_apply_anomaly(&b.ctx(), 1, kPoisonId, /*duration=*/5, /*actor=*/0);
            play_round_foe_none(b, 0);   // 1248 前置成立、50% 疲惫
            if (has_anomaly(b.ctx(), 1, kWearinessId)) {
                ++fired;
            } else {
                // 毒没了但不是疲惫 = 顶替/施加了错码
                wrong_code = wrong_code || !has_anomaly(b.ctx(), 1, kPoisonId);
            }
        }
        CHECK(fired > 0,
              "881c 核心：50% 档 40 种子全空转（概率≈1e-12）→ 概率/异常码仍有一个没接对");
        CHECK(!wrong_code, "881c 核心：触发即疲惫(7)，未触发则毒仍在——无码错位");
        std::cout << "  [881c] 1248 第二载体 50% 疲惫 通过（40 种子触发 " << fired
                  << " 次）\n";
    }

    std::cout << "scenario_881 PASS (X2 effect 1248 异常码/概率接线修正回归，段 881)"
              << std::endl;
    return 0;
}
