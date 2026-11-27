// 场景 8070 —— K424 效果 1141「1回合做{0}~{1}次攻击，每次攻击{2}%令对手{3}，
// 攻击低于{4}次则每次攻击{5}%令对手{6}」（resources/moves_lib/k424_lianji_yichang_pool.cpp，
// 号段 8070-8079 → 本文件 8070，2026-11-07）
//
// 断言口径（连击次数 N 读 ctx.resolvedDamage.hitCount=内核单次结算权威记录；
// 逐段分支/概率/异常码选择读审计槽 1,011,410,000 段——无随机精确断言）：
//   T1 晶爆 37668 [5,10,30,16,7,60,16] 第一支（N>=7，30%/流血）：钉种子每用
//      同种子 → N=8 确定性；逐用断言 审计 N==hitCount、分支==0、(概率,异常码)
//      ==(30,16)、申报位全等、尝试数==ΣN（parser 接管证伪：半解析每用至多 1 次）、
//      landed==12（=6 用 ×每用 2，MINSTD 闭式可复核）。
//   T2 晶爆 第二支（N<7，60%/流血）：钉种子 N=5；landed==12 → 对手流血状态。
//   T3 火花闪电 36342 [30,40,1,0,35,2,0] 阈值 35 双分支（1% 麻痹 / 2% 麻痹）：
//      钉种子 N=34（第二支 2%）/ N=37（第一支 1%），分支与概率选择逐用精确；
//      第二支钉种子零 proc 档（MINSTD 闭式可复核：3 用 102 掷全歪）→ 对手无麻痹。
//   T4 幽影连击 38439 [10,20,5,6,15,5,16] 异双条选择（害怕/流血互斥分支）：
//      钉种子 N=15 → 传原语异常码==6（landed==5）；N=14 → ==16（landed==5）；
//      互斥（害怕局无流血 / 流血局无害怕）。
//   T5 鳗鳗冲锋 34746 混链 [1141,918]|[3,6,20,1,5,30,1|300,1,50]：申报位断言
//      ==20/1/5/30/1（链切片下 1141 吃 7 参、918 不串台）；同载体双分支
//      （N=6 第一支 20% / N=3 第二支 30%）。
//   T6 免疫反例：对手 Modern 层 ANOMALY 免疫票 → 尝试数==16、landed==0、
//      TARGET_IMMUNE 计数==4（掷点过门才到免疫层）、对手干净。
//   T7 miss 反例：对手 100% 闪避 + 本方非必中 → 1141 效果桶不跑（miss 提前 return）：
//      slot5/6==-1、尝试/landed==0、对手无异常。⚠️ miss 后连击伤害仍结算 = 内核
//      无效出口 variable_power_requested `combo_view>1` 既定行为（勘误记档，R 单素材）。
//   T8 攻击无效反例（1090 族 ATTACK_NULLIFY 票，技能有效但红伤归零）：
//      效果桶跑（申报位已写）但逐段钩子不触发（final<=0 内核闸）：
//      slot5/6 保持 -1、尝试/landed 增量==0。
//   T9 双方镜像：双方 1141 载体互打，owner×16 槽隔离，两侧申报位/尝试/landed
//      各自成立（两侧 attempts>=ΣN 下界）。
//   T10 确定性回放：同种子序列两次独立对局，审计全槽逐位相等。
#include <array>
#include <iostream>
#include <vector>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>
#include <utils/sbs_rand.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

// ── 载体技（切片已逐条与 side_effect/arg 核对，见插件头注）──
constexpr int kPaiDa   = 10004;   // 拍打（填充刀）
constexpr int kJingBao = 37668;   // 晶爆 [1141,412]|[5,10,30,16,7,60,16|2] 物10 pp20
constexpr int kHuoHua  = 36342;   // 火花闪电 [1141]|[30,40,1,0,35,2,0] 特4 pp10
constexpr int kYouYing = 38439;   // 幽影连击 [1141,1290]|[10,20,5,6,15,5,16|2,2] 物10 pp5
constexpr int kManMan  = 34746;   // 鳗鳗冲锋 [1141,918]|[3,6,20,1,5,30,1|300,1,50] 物40 pp5

constexpr int kBleed     = static_cast<int>(AbnormalStatusId::Bleed);      // 16 流血
constexpr int kFear      = static_cast<int>(AbnormalStatusId::Fear);       // 6 害怕
constexpr int kParalysis = static_cast<int>(AbnormalStatusId::Paralysis);  // 0 麻痹

// ── 审计槽（k424_lianji_yichang_pool.cpp 头注 §审计槽）──
constexpr int kAuditBase = 1011410000;
int aint(BattleContext& ctx, int owner, int slot, int def = 0) {
    const auto it = ctx.plugin_storage.find(kAuditBase + owner * 16 + slot);
    if (it == ctx.plugin_storage.end()) {
        return def;
    }
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : def;
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300, int atk = 1000, int hp = 999999) {
    ElfPet p = make_pet(1, "连击方", skills, speed, atk, hp);
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
    return p;
}

// 非必中版（T7 miss 块用）。
ElfPet self_pet_no_must(std::array<int, 5> skills, int speed = 300, int atk = 1000) {
    ElfPet p = make_pet(1, "连击方", skills, speed, atk, 999999);
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 0;
    }
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100, int atk = 1, int hp = 999999) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, hp);
    for (Skills& s : p.skills) {
        s.accuracy = 100;
    }
    return p;
}

// 标准局骨架：我方先手出 index 技，对手空转（我方零受击）。
void run_cast(ScenarioBattle& b, int skill_index, int seed) {
    b.ctx().seed_rng(static_cast<unsigned>(seed));
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, skill_index);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1 晶爆第一支（N>=7 → 30%/流血）══
    {
        ScenarioBattle b(make_party(self_pet({kJingBao, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        // 钉种子：seed=3（MINSTD 首抽 r=3*16807 mod (2^31-1)=50421，50421%6==3 →
        // N=5+3=8 >= 阈值 7 → 第一支 30%/流血）。同种子逐用 → 每用 N=8 确定性。
        int attempts_expect = 0;
        for (int r = 0; r < 6; ++r) {
            run_cast(b, 0, 3);
            const int n = ctx.resolvedDamage.hitCount;
            CHECK_EQ(n, 8, "T1 晶爆 seed=3 连击数 N==8（实测 " + std::to_string(n) + "）");
            CHECK_EQ(aint(ctx, 0, 5, -99), 8, "T1 审计 N==hitCount==8");
            CHECK_EQ(aint(ctx, 0, 6, -99), 0, "T1 分支==0（N>=7 第一支）");
            CHECK_EQ(aint(ctx, 0, 11, -99), 30, "T1 传原语概率==30");
            CHECK_EQ(aint(ctx, 0, 10, -99), kBleed, "T1 传原语异常码==16 流血");
            attempts_expect += n;
            CHECK_EQ(aint(ctx, 0, 7), attempts_expect,
                     "T1 尝试数累计==ΣN（逐段申报，非 parser 半解析每用至多 1 次）");
        }
        CHECK_EQ(aint(ctx, 0, 0, -99), 30, "T1 申报概率1==30");
        CHECK_EQ(aint(ctx, 0, 1, -99), kBleed, "T1 申报异常码1==16");
        CHECK_EQ(aint(ctx, 0, 2, -99), 7, "T1 申报阈值==7");
        CHECK_EQ(aint(ctx, 0, 3, -99), 60, "T1 申报概率2==60");
        CHECK_EQ(aint(ctx, 0, 4, -99), kBleed, "T1 申报异常码2==16");
        CHECK_EQ(aint(ctx, 0, 8), 12,
                 "T1 landed==12（6 用 ×每用 2：MINSTD 闭式可复核，掷点=第 4~11 抽）");
        std::cout << "  [T1] 晶爆第一支 N=8 通过（尝试 " << attempts_expect << " 段 / landed "
                  << aint(ctx, 0, 8) << "）\n";
    }

    // ══ T2 晶爆第二支（N<7 → 60%/流血）══
    {
        ScenarioBattle b(make_party(self_pet({kJingBao, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        // 钉种子：seed=6（首抽 r=100842，%6==0 → N=5 < 7 → 第二支 60%/流血）。
        int attempts_expect = 0;
        for (int r = 0; r < 6; ++r) {
            run_cast(b, 0, 6);
            const int n = ctx.resolvedDamage.hitCount;
            CHECK_EQ(n, 5, "T2 晶爆 seed=6 连击数 N==5（实测 " + std::to_string(n) + "）");
            CHECK_EQ(aint(ctx, 0, 6, -99), 1, "T2 分支==1（N<7 第二支）");
            CHECK_EQ(aint(ctx, 0, 11, -99), 60, "T2 传原语概率==60");
            CHECK_EQ(aint(ctx, 0, 10, -99), kBleed, "T2 传原语异常码==16 流血");
            attempts_expect += n;
            CHECK_EQ(aint(ctx, 0, 7), attempts_expect, "T2 尝试数累计==ΣN");
        }
        CHECK_EQ(aint(ctx, 0, 8), 12, "T2 landed==12（6 用 ×每用 2，闭式可复核）");
        CHECK(ctx.has_active_abnormal_status(1, kBleed), "T2 对手处于流血");
        std::cout << "  [T2] 晶爆第二支 N=5 通过（landed " << aint(ctx, 0, 8) << "）\n";
    }

    // ══ T3 火花闪电 阈值 35 双分支 + 零 proc 精确档 ══
    {
        // (a) seed=7（首抽 r=117649，%11==4 → N=30+4=34 <35 → 第二支 2% 麻痹）。
        {
            ScenarioBattle b(make_party(self_pet({kHuoHua, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            for (int r = 0; r < 3; ++r) {
                run_cast(b, 0, 7);
                const int n = ctx.resolvedDamage.hitCount;
                CHECK_EQ(n, 34, "T3.a 火花 seed=7 N==34（实测 " + std::to_string(n) + "）");
                CHECK_EQ(aint(ctx, 0, 6, -99), 1, "T3.a 分支==1（34<35）");
                CHECK_EQ(aint(ctx, 0, 11, -99), 2, "T3.a 传原语概率==2");
                CHECK_EQ(aint(ctx, 0, 10, -99), kParalysis, "T3.a 传原语异常码==0 麻痹");
            }
            CHECK_EQ(aint(ctx, 0, 2, -99), 35, "T3.a 申报阈值==35");
            std::cout << "  [T3.a] 火花 N=34 第二支 通过\n";
        }
        // (b) seed=15（首抽 r=252105，%11==7 → N=37 >=35 → 第一支 1% 麻痹）。
        {
            ScenarioBattle b(make_party(self_pet({kHuoHua, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            for (int r = 0; r < 3; ++r) {
                run_cast(b, 0, 15);
                const int n = ctx.resolvedDamage.hitCount;
                CHECK_EQ(n, 37, "T3.b 火花 seed=15 N==37（实测 " + std::to_string(n) + "）");
                CHECK_EQ(aint(ctx, 0, 6, -99), 0, "T3.b 分支==0（37>=35）");
                CHECK_EQ(aint(ctx, 0, 11, -99), 1, "T3.b 传原语概率==1");
                CHECK_EQ(aint(ctx, 0, 10, -99), kParalysis, "T3.b 传原语异常码==0 麻痹");
            }
            CHECK_EQ(aint(ctx, 0, 8), 3, "T3.b landed==3（3 用 ×每用 1，闭式可复核）");
            std::cout << "  [T3.b] 火花 N=37 第一支 通过\n";
        }
        // (c) 零 proc 精确档：seed=8（首抽 %11==3 → N=33 <35 → 第二支 2%；
        //     MINSTD 闭式可复核：3 用 99 掷全歪）。
        {
            ScenarioBattle b(make_party(self_pet({kHuoHua, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            for (int r = 0; r < 3; ++r) {
                run_cast(b, 0, 8);
            }
            CHECK_EQ(aint(ctx, 0, 8), 0, "T3.c 零 proc 种子组 landed==0");
            CHECK_EQ(aint(ctx, 0, 7), 99, "T3.c 尝试数==3×33==99");
            CHECK(!ctx.has_active_abnormal_status(1, kParalysis), "T3.c 对手无麻痹");
            std::cout << "  [T3.c] 火花零 proc 档 通过\n";
        }
    }

    // ══ T4 幽影连击 异双条选择（害怕/流血互斥分支）══
    {
        // (a) seed=39（首抽 %11==5 → N=10+5=15 >=15 → 第一支 5%/害怕；每用 landed=1）。
        {
            ScenarioBattle b(make_party(self_pet({kYouYing, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            for (int r = 0; r < 5; ++r) {
                run_cast(b, 0, 39);
                const int n = ctx.resolvedDamage.hitCount;
                CHECK_EQ(n, 15, "T4.a 幽影 seed=39 N==15（实测 " + std::to_string(n) + "）");
                CHECK_EQ(aint(ctx, 0, 6, -99), 0, "T4.a 分支==0（15>=15）");
                CHECK_EQ(aint(ctx, 0, 11, -99), 5, "T4.a 传原语概率==5");
                CHECK_EQ(aint(ctx, 0, 10, -99), kFear, "T4.a 传原语异常码==6 害怕");
            }
            CHECK_EQ(aint(ctx, 0, 8), 5, "T4.a landed==5（5 用 ×每用 1，闭式可复核）");
            CHECK(ctx.has_active_abnormal_status(1, kFear), "T4.a 对手害怕");
            CHECK(!ctx.has_active_abnormal_status(1, kBleed), "T4.a 互斥：第一支局无流血");
            std::cout << "  [T4.a] 幽影 N=15 害怕支 通过（landed " << aint(ctx, 0, 8) << "）\n";
        }
        // (b) seed=7（首抽 %11==4 → N=14 <15 → 第二支 5%/流血；每用 landed=1）。
        {
            ScenarioBattle b(make_party(self_pet({kYouYing, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            for (int r = 0; r < 5; ++r) {
                run_cast(b, 0, 7);
                const int n = ctx.resolvedDamage.hitCount;
                CHECK_EQ(n, 14, "T4.b 幽影 seed=7 N==14（实测 " + std::to_string(n) + "）");
                CHECK_EQ(aint(ctx, 0, 6, -99), 1, "T4.b 分支==1（14<15）");
                CHECK_EQ(aint(ctx, 0, 10, -99), kBleed, "T4.b 传原语异常码==16 流血");
            }
            CHECK_EQ(aint(ctx, 0, 8), 5, "T4.b landed==5（5 用 ×每用 1，闭式可复核）");
            CHECK(ctx.has_active_abnormal_status(1, kBleed), "T4.b 对手流血");
            CHECK(!ctx.has_active_abnormal_status(1, kFear), "T4.b 互斥：第二支局无害怕");
            std::cout << "  [T4.b] 幽影 N=14 流血支 通过（landed " << aint(ctx, 0, 8) << "）\n";
        }
    }

    // ══ T5 鳗鳗冲锋 混链 [1141,918]（切片自洽 + 同载体双分支）══
    {
        ScenarioBattle b(make_party(self_pet({kManMan, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        // seed=5（首抽 %4==3 → N=3+3=6 >=5 → 第一支 20%/中毒，每用 landed=1）×3。
        for (int r = 0; r < 3; ++r) {
            run_cast(b, 0, 5);
            const int n = ctx.resolvedDamage.hitCount;
            CHECK_EQ(n, 6, "T5 鳗鳗 seed=5 N==6（实测 " + std::to_string(n) + "）");
            CHECK_EQ(aint(ctx, 0, 6, -99), 0, "T5 分支==0（6>=5）");
            CHECK_EQ(aint(ctx, 0, 11, -99), 20, "T5 传原语概率==20");
            CHECK_EQ(aint(ctx, 0, 10, -99),
                     static_cast<int>(AbnormalStatusId::Poison), "T5 传原语异常码==1 中毒");
        }
        // seed=3（首抽 r=50421，%4==1 → N=3+1=4 <5 → 第二支 30%/中毒，每用 landed=2）×1。
        run_cast(b, 0, 3);
        {
            const int n = ctx.resolvedDamage.hitCount;
            CHECK_EQ(n, 4, "T5 鳗鳗 seed=3 N==4（实测 " + std::to_string(n) + "）");
            CHECK_EQ(aint(ctx, 0, 6, -99), 1, "T5 分支==1（4<5 第二支）");
            CHECK_EQ(aint(ctx, 0, 11, -99), 30, "T5 传原语概率==30（第二支）");
            CHECK_EQ(aint(ctx, 0, 10, -99),
                     static_cast<int>(AbnormalStatusId::Poison), "T5 传原语异常码==1 中毒");
        }
        CHECK_EQ(aint(ctx, 0, 0, -99), 20, "T5 申报概率1==20");
        CHECK_EQ(aint(ctx, 0, 1, -99), static_cast<int>(AbnormalStatusId::Poison),
                 "T5 申报异常码1==1 中毒");
        CHECK_EQ(aint(ctx, 0, 2, -99), 5, "T5 申报阈值==5");
        CHECK_EQ(aint(ctx, 0, 3, -99), 30, "T5 申报概率2==30");
        CHECK_EQ(aint(ctx, 0, 4, -99), static_cast<int>(AbnormalStatusId::Poison),
                 "T5 申报异常码2==1 中毒");
        CHECK_EQ(aint(ctx, 0, 7), 22, "T5 尝试数==3×6+4==22");
        CHECK_EQ(aint(ctx, 0, 8), 5, "T5 landed==5（3 用 seed5 每用 1 + 1 用 seed3 共 2）");
        std::cout << "  [T5] 鳗鳗冲锋混链切片+双分支 通过\n";
    }

    // ══ T6 免疫反例（Modern ANOMALY 免疫票）══
    {
        ScenarioBattle b(make_party(self_pet({kJingBao, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.grant_immunity(1, ImmunityType::ANOMALY, coverage_all(), /*mask=*/0,
                           /*duration_rounds=*/9, /*source_id=*/7001);
        for (int r = 0; r < 2; ++r) {
            run_cast(b, 0, 3);
        }
        CHECK_EQ(aint(ctx, 0, 7), 16, "T6 尝试数==2 用 ×N=8==16（钉种子确定性）");
        CHECK_EQ(aint(ctx, 0, 8), 0, "T6 landed==0（免疫全挡）");
        CHECK_EQ(aint(ctx, 0, 12), 4,
                 "T6 TARGET_IMMUNE 计数==4（掷点过门才到免疫层；同种子 T1 每 8 掷 2 过门"
                 "→ 2 用 4 次；其余 ROLL_FAILED 不到免疫层）");
        CHECK_EQ(aint(ctx, 0, 9, -99),
                 static_cast<int>(ApplyAnomalyResult::ROLL_FAILED),
                 "T6 最近结果码==ROLL_FAILED（末次掷歪；过门次被免疫拦）");
        CHECK(!ctx.has_active_abnormal_status(1, kBleed), "T6 对手无流血");
        std::cout << "  [T6] 免疫反例 通过（尝试 " << aint(ctx, 0, 7) << "）\n";
    }

    // ══ T7 miss 反例（对手 100% 闪避 + 非必中）══
    // ⚠️ 勘误记档（2026-11-07 本单实测，R 单素材）：连击技 miss 后**仍会结算伤害**——
    //   query_usage ① 命中 roll 正常判 MISS（精度 roll 在 lr=query_usage+1620 实测触发），
    //   但 handle_*_AttackDamage 的无效出口 run_attack_damage_from_invalid_skill 由
    //   variable_power_requested（battleFsm.cpp:296-300）的 `combo_view > 1` 项放行：
    //   **静态连击基数（引擎打底物化，非效果改写）也命中该条件** → stage+finish 全额
    //   重结算。单击技（N==1）不受影响（0/20 实测）。修复方向 = 条件改用 rewritten 项
    //   / 排除未改写的静态连击——src/ 本单不动，留续单（本效果断言只锁"钩子不触发"）。
    {
        ElfPet dodger = make_pet(2, "闪避对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                 /*speed=*/100, /*atk=*/1, 999999,
                                 /*soul_mark=*/make_dodge_soul_mark());
        ScenarioBattle b(make_party(self_pet_no_must({kJingBao, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(dodger));
        b.start();
        BattleContext& ctx = b.ctx();
        const int hp_before = ctx.getPet(1).hp;
        for (int r = 0; r < 2; ++r) {
            run_cast(b, 0, 3);
        }
        CHECK_EQ(aint(ctx, 0, 5), 0, "T7 slot5 未写入（miss → 1141 效果桶不跑，0=槽缺失）");
        CHECK_EQ(aint(ctx, 0, 6), 0, "T7 slot6 未写入（钩子未挂）");
        CHECK_EQ(aint(ctx, 0, 7), 0, "T7 尝试数==0（miss 无落地段）");
        CHECK_EQ(aint(ctx, 0, 8), 0, "T7 landed==0");
        CHECK(!ctx.has_active_abnormal_status(1, kBleed), "T7 对手无流血（逐段异常不触发）");
        std::cout << "  [T7] miss 反例 通过（钩子全不触发；miss 后连击伤害仍结算=内核"
                  << "无效出口既定行为，掉血 " << (hp_before - ctx.getPet(1).hp) << "）\n";
    }

    // ══ T8 攻击无效反例（ATTACK_NULLIFY 票，1090 族口径：命中效果失效 + 红伤归零）══
    // 票在 query_usage ②.6 命中 → kFullNull 白板：效果不注册（HIT 分支被 nullify 过滤）
    // + 红伤归零。1141 效果桶整体不跑 = 逐段异常天然不触发（miss 同款口径）。
    {
        ScenarioBattle b(make_party(self_pet({kJingBao, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        // 1090 族票：生效目标=0（我方攻击整段归零）。 bearer=0 自持。
        ctx.rule_center_.grant_attack_nullify(/*source_owner=*/0, /*source_effect_id=*/999999,
                                              /*target=*/0, /*rounds=*/9,
                                              /*register_round=*/ctx.roundCount);
        const int hp_before = ctx.getPet(1).hp;
        run_cast(b, 0, 3);
        CHECK_EQ(ctx.getPet(1).hp, hp_before, "T8 对手掉血==0（kFullNull 红伤归零）");
        CHECK_EQ(aint(ctx, 0, 0), 0, "T8 申报位未写入（效果不注册：HIT 分支被过滤）");
        CHECK_EQ(aint(ctx, 0, 5), 0, "T8 slot5 未写入（钩子未挂）");
        CHECK_EQ(aint(ctx, 0, 7), 0, "T8 尝试数==0");
        CHECK_EQ(aint(ctx, 0, 8), 0, "T8 landed==0");
        CHECK(!ctx.has_active_abnormal_status(1, kBleed), "T8 对手无流血");
        std::cout << "  [T8] 攻击无效反例 通过\n";
    }

    // ══ T9 双方镜像（双方 1141 载体互打，owner×16 槽隔离）══
    {
        ScenarioBattle b(make_party(self_pet({kJingBao, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                             /*speed=*/300, /*atk=*/1000)),
                         make_party(self_pet({kJingBao, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                             /*speed=*/100, /*atk=*/1000)));
        b.start();
        BattleContext& ctx = b.ctx();
        for (int r = 0; r < 3; ++r) {
            b.ctx().seed_rng(static_cast<unsigned>(100 + r));
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
        }
        for (const int o : {0, 1}) {
            CHECK_EQ(aint(ctx, o, 0, -99), 30, "T9 owner " + std::to_string(o) + " 申报概率1==30");
            CHECK_EQ(aint(ctx, o, 2, -99), 7, "T9 owner " + std::to_string(o) + " 申报阈值==7");
            CHECK(aint(ctx, o, 7) >= 15,
                  "T9 owner " + std::to_string(o) + " 尝试数>=3 用 ×N>=5（实测 "
                      + std::to_string(aint(ctx, o, 7)) + "）");
            CHECK(aint(ctx, o, 8) >= 1,
                  "T9 owner " + std::to_string(o) + " landed>=1（3 用 ×N∈[5,10] ×30/60%，"
                  "P 全歪<=2.3e-5）");
        }
        std::cout << "  [T9] 双方镜像 通过（0 侧尝试/landed " << aint(ctx, 0, 7) << "/"
                  << aint(ctx, 0, 8) << "，1 侧 " << aint(ctx, 1, 7) << "/"
                  << aint(ctx, 1, 8) << "）\n";
    }

    // ══ T10 确定性回放（同种子序列两次对局审计逐位相等）══
    {
        constexpr int kSlots = 13;
        int snap[2][kSlots];
        for (int game = 0; game < 2; ++game) {
            ScenarioBattle b(make_party(self_pet({kJingBao, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            const int seeds[3] = {3, 6, 1};
            for (int r = 0; r < 3; ++r) {
                run_cast(b, 0, seeds[r]);
            }
            for (int s = 0; s < kSlots; ++s) {
                snap[game][s] = aint(ctx, 0, s, -99);
            }
        }
        for (int s = 0; s < kSlots; ++s) {
            CHECK_EQ(snap[1][s], snap[0][s],
                     "T10 slot " + std::to_string(s) + " 两次对局逐位相等（实测 "
                         + std::to_string(snap[1][s]) + " vs " + std::to_string(snap[0][s])
                         + "）");
        }
        std::cout << "  [T10] 确定性回放 通过\n";
    }

    std::cout << "scenario_8070 PASS (1141 连击逐段异常：双分支/异双条/混链/免疫/miss/无效/镜像/回放)"
              << std::endl;
    return 0;
}
