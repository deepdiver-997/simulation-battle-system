// 场景 8050：K422 效果 917「命中后{0}%使对手{1}，未触发则附加自身最大体力值1/{2}的
// 百分比伤害」——概率参数化施加 + 未触发粉伤兜底
//（resources/moves_lib/k422_gailv_fenhong_pool.cpp，2026-11-07；号段 8050-8059）
//
// 覆盖：
//   T1  [50,6,2] 29084 属性技 P=50 过闸侧（钉 r=49）：害怕落地、零粉伤、hp 零损
//   T2  同载体拒绝侧（钉 r=50）：ROLL_FAILED、兜底恰 max/2=150000（属性技零红伤 → hp 精确）
//   T3  [35,0,4] 31240 P=35 过闸侧（钉 r=34）：麻痹落地
//   T4  同载体拒绝侧（钉 r=35）：兜底恰 max/4=75000（红伤带 + 粉伤带断言）
//   T5  掷点下界（钉 r=0，seed=100）：0<35 过闸（r%100==0 可达边界）
//   T6  [100,2,3] 26443 属性技 P=100 短路：不耗抽取恒落地、烧伤 tick 恰 1/8=100000
//   T7  免疫兜底（31934 烧伤载体 + Modern 免疫窗）：掷点过闸仍 TARGET_IMMUNE →
//       兜底粉伤恰 max/3=100000（"掷歪/免疫都算未触发"，J129/J165 同口径）
//   T8  闸门改写（亮节族 CHANCE_GATE below→100）：申报 50 被改写为 100 → r=50 种子
//       反而落地、审计 slot3 记改写后实际概率（闸门管辖实证，T2 同种子无闸被拒互证）
//   T9  [917,1000] 37377 混链：P=100 寄生落地 + 审计在位（废弃 parser 半解析清账实证——
//       parser 驱动时无审计槽；skills.cpp:671 只在无插件函数时走 parse_effect_unit）
//   T10 双方镜像（两侧 19513）：审计键 owner×16 隔离、独立抽取（#1 vs #3）
//   T11 确定性（同种子两次对局审计链全等）
//   T12 固定 20 种子扫描：闭式 r=(seed×16807 mod M)%100 与 landed 逐种子一致 +
//       过闸率侧证带（P=50）
//   T13 miss 门（31934 accuracy=0）：HIT 分支不注册 → 全链不触发、审计键缺席
//
// 确定性纪律：917 为默认路由 SKILL_EFFECT 桶（伤害管线前）→ 申报制掷点 = 第 1 抽
//（0<final<100 恰耗一抽；<=0/>=100 不耗，battle_primitives.h:90 契约）；MINSTD 闭式
// 钉种子 seed ≡ 目标态×16807^-1 (mod 2^31-1)。审计槽 1009170000 段（owner×16+slot）。
// 红伤带 = 0.84~1.0 × power×atk/def（atk=1/def=100 → 单值域 [1,2]，带放宽 [1,10]）。
#include <cstdlib>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <effects/rule_center.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>
#include <utils/sbs_rand.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kLianShen = 19513;   // 炼神乾坤灾 [917] [50,7,3]：物理150 必中（疲惫7 不扣血）
constexpr int kJuShou   = 29084;   // 巨兽蔑视 [191,917,1236] [4, 50,6,2, 3,3]：属性技（害怕6）
constexpr int kMoFa     = 31240;   // 魔法仆从 [1055,917] [1, 35,0,4]：特85（麻痹0）
constexpr int kShenYuan = 26443;   // 深渊之炎 [917,693,843] [100,2,3,…]：属性技（烧伤2）
constexpr int kBaoLie   = 31934;   // 爆裂篝火 [917] [50,2,3]：物理150（烧伤2）
constexpr int kFenMo    = 37377;   // 星光·粉末击打 [917,1000] [100,4,4, 100,1,200]：特130
constexpr int kAttr     = 22638;   // 虚空之遗（属性技，190 窗无害——8030 同款对手填充）
constexpr int kZero     = 10938;   // 本能一击（power 0，无效果填充）

constexpr int kAuditBase = 1009170000;
int aint(BattleContext& ctx, int owner, int slot, int def = 0) {
    const auto it = ctx.plugin_storage.find(kAuditBase + owner * 16 + slot);
    if (it == ctx.plugin_storage.end()) {
        return def;
    }
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : def;
}
bool ahas(BattleContext& ctx, int owner, int slot) {
    return ctx.plugin_storage.count(kAuditBase + owner * 16 + slot) > 0;
}

constexpr int kMyMax = 300000;    // 施加方最大体力：粉伤 /2=150000 /3=100000 /4=75000 全整除
constexpr int kFoeMax = 800000;   // 对手最大体力：异常 tick 1/8=100000 整除

ElfPet caster(std::array<int, 5> skills, int speed = 300) {
    ElfPet p = scenario::make_pet(0, "施加方", skills, speed, /*atk=*/1, kMyMax);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe(std::array<int, 5> skills, int speed = 100) {
    ElfPet p = scenario::make_pet(1, "对手", skills, speed, /*atk=*/1, kFoeMax);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// 标准局骨架：我方 speed 300 先手出载体，对手属性技空转（我方零受击）。
void run_cast(ScenarioBattle& b, int skill_index, int seed) {
    b.ctx().seed_rng(static_cast<unsigned>(seed));
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, skill_index);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

// MINSTD 闭式：srand(seed) 后第 k 抽 = seed×16807^k mod (2^31-1)。
std::uint64_t minstd_draw(int seed, int k) {
    std::uint64_t v = static_cast<std::uint64_t>(seed) % 2147483647ULL;
    for (int i = 0; i < k; ++i) {
        v = (v * 16807ULL) % 2147483647ULL;
    }
    return v;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== T1 [50,6,2] 29084 属性技 P=50 过闸侧（r=49，钉种子 256696296）==========
    {
        ScenarioBattle b(scenario::make_party(caster({kJuShou, kZero, kZero, kZero, kZero})),
                        scenario::make_party(foe({kAttr, kZero, kZero, kZero, kZero})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 256696296);
        CHECK_EQ(aint(ctx, 0, 0, -1), 50, "T1 申报 P=50");
        CHECK_EQ(aint(ctx, 0, 1, -1), 6, "T1 申报异常码=6（害怕）");
        CHECK_EQ(aint(ctx, 0, 2, -1), 2, "T1 申报分母=2（191/1236 的参不串位）");
        CHECK_EQ(aint(ctx, 0, 3), 50, "T1 无闸门 → 改写后实际概率==申报值");
        CHECK_EQ(aint(ctx, 0, 4, -1), static_cast<int>(ApplyAnomalyResult::SUCCESS),
                 "T1 施加结果=SUCCESS");
        CHECK_EQ(aint(ctx, 0, 5), 1, "T1 landed=1");
        CHECK_EQ(aint(ctx, 0, 6), 0, "T1 兜底量=0（触发与兜底互斥）");
        CHECK_EQ(aint(ctx, 0, 7), 1, "T1 landed 计数=1");
        CHECK_EQ(aint(ctx, 0, 8), 0, "T1 兜底计数=0");
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Fear)),
              "T1 害怕落地");
        CHECK_EQ(ctx.getPet(1).hp, kFoeMax,
                 "T1 属性技零红伤 + 害怕零 tick + 零粉伤 → hp 精确无损");
        std::cout << "  [T1] P=50 过闸侧（属性技 hp 精确）通过\n";
    }

    //========== T2 同载体拒绝侧（r=50，钉种子 1664373296）==========
    {
        ScenarioBattle b(scenario::make_party(caster({kJuShou, kZero, kZero, kZero, kZero})),
                        scenario::make_party(foe({kAttr, kZero, kZero, kZero, kZero})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 1664373296);
        CHECK_EQ(aint(ctx, 0, 3), 50, "T2 闸门不改写（50）");
        CHECK_EQ(aint(ctx, 0, 4, -1), static_cast<int>(ApplyAnomalyResult::ROLL_FAILED),
                 "T2 施加结果=ROLL_FAILED（r=50 ≥ P=50 拒）");
        CHECK_EQ(aint(ctx, 0, 5), 0, "T2 landed=0");
        CHECK_EQ(aint(ctx, 0, 6), 150000, "T2 兜底量=自身最大体力/2=150000（整除精确）");
        CHECK_EQ(aint(ctx, 0, 7), 0, "T2 landed 计数=0");
        CHECK_EQ(aint(ctx, 0, 8), 1, "T2 兜底计数=1");
        CHECK_EQ(aint(ctx, 0, 9), 0, "T2 拒绝原因=0（非目标已倒）");
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Fear)) == false,
              "T2 害怕未落地（触发与兜底互斥的另一侧）");
        CHECK_EQ(ctx.getPet(1).hp, kFoeMax - 150000,
                 "T2 属性技零红伤 → hp 损失恰=兜底粉伤 150000（粉伤数值精确）");
        std::cout << "  [T2] P=50 拒绝侧（兜底恰 150000）通过\n";
    }

    //========== T3/T4 [35,0,4] 31240 P=35 边界对（34 过 / 35 拒）==========
    for (int cs = 0; cs < 2; ++cs) {
        const int seed = (cs == 0) ? 616377766 : 2024054766;   // r=34 过 / r=35 拒
        const bool expect_pass = (cs == 0);
        ScenarioBattle b(scenario::make_party(caster({kMoFa, kZero, kZero, kZero, kZero})),
                        scenario::make_party(foe({kAttr, kZero, kZero, kZero, kZero})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, seed);
        CHECK_EQ(aint(ctx, 0, 0, -1), 35, "T3/T4 申报 P=35（1055 的参不串位）");
        CHECK_EQ(aint(ctx, 0, 1, -1), 0, "T3/T4 申报异常码=0（麻痹）");
        CHECK_EQ(aint(ctx, 0, 2, -1), 4, "T3/T4 申报分母=4");
        CHECK_EQ(aint(ctx, 0, 5), expect_pass ? 1 : 0, "T3/T4 landed（35<35 不成立=拒）");
        CHECK_EQ(aint(ctx, 0, 4, -1),
                 expect_pass ? static_cast<int>(ApplyAnomalyResult::SUCCESS)
                             : static_cast<int>(ApplyAnomalyResult::ROLL_FAILED),
                 "T3/T4 施加结果码");
        if (expect_pass) {
            CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Paralysis)),
                  "T3 麻痹落地");
            CHECK_EQ(aint(ctx, 0, 6), 0, "T3 兜底量=0");
            const int loss = kFoeMax - ctx.getPet(1).hp;
            CHECK(loss >= 0 && loss <= 10,
                  "T3 过闸侧 hp 损失=红伤带 [0,10]（atk=1，无粉伤混杂）实测 " +
                      std::to_string(loss));
        } else {
            CHECK_EQ(aint(ctx, 0, 6), 75000, "T4 兜底量=自身最大体力/4=75000");
            CHECK_EQ(aint(ctx, 0, 8), 1, "T4 兜底计数=1");
            const int loss = kFoeMax - ctx.getPet(1).hp;
            CHECK(loss >= 75000 && loss <= 75010,
                  "T4 hp 损失=75000+红伤带 [0,10]（实测 " + std::to_string(loss) + "）");
        }
    }
    std::cout << "  [T3/T4] P=35 边界对 通过\n";

    //========== T5 掷点下界（r=0，seed=100：1680700%100==0）==========
    {
        ScenarioBattle b(scenario::make_party(caster({kMoFa, kZero, kZero, kZero, kZero})),
                        scenario::make_party(foe({kAttr, kZero, kZero, kZero, kZero})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 100);
        CHECK_EQ(static_cast<int>(minstd_draw(100, 1) % 100), 0, "T5 闭式核验 r=0");
        CHECK_EQ(aint(ctx, 0, 5), 1, "T5 r=0 < 35 → landed=1（掷点下界过闸）");
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Paralysis)),
              "T5 麻痹落地");
        std::cout << "  [T5] 掷点下界 r=0 通过\n";
    }

    //========== T6 [100,2,3] 26443 属性技 P=100 短路（不耗抽取恒落地）==========
    {
        ScenarioBattle b(scenario::make_party(caster({kShenYuan, kZero, kZero, kZero, kZero})),
                        scenario::make_party(foe({kAttr, kZero, kZero, kZero, kZero})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 42);   // P=100 不掷：任意种子恒落地
        CHECK_EQ(aint(ctx, 0, 0, -1), 100, "T6 申报 P=100（693/843 的参不串位）");
        CHECK_EQ(aint(ctx, 0, 1, -1), 2, "T6 申报异常码=2（烧伤）");
        CHECK_EQ(aint(ctx, 0, 2, -1), 3, "T6 申报分母=3");
        CHECK_EQ(aint(ctx, 0, 3), 100, "T6 闸门不改写（100）");
        CHECK_EQ(aint(ctx, 0, 4, -1), static_cast<int>(ApplyAnomalyResult::SUCCESS),
                 "T6 P=100 短路恒 SUCCESS");
        CHECK_EQ(aint(ctx, 0, 5), 1, "T6 landed=1");
        CHECK_EQ(aint(ctx, 0, 6), 0, "T6 兜底量=0（100% 档无兜底）");
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Burn)),
              "T6 烧伤落地");
        CHECK_EQ(ctx.getPet(1).hp, kFoeMax - kFoeMax / 8,
                 "T6 属性技零红伤 → hp 损失恰=烧伤 tick 1/8=100000");
        std::cout << "  [T6] P=100 短路 通过\n";
    }

    //========== T7 免疫兜底（31934 + Modern 免疫窗：掷歪/免疫都算未触发）==========
    {
        ScenarioBattle b(scenario::make_party(caster({kBaoLie, kZero, kZero, kZero, kZero})),
                        scenario::make_party(foe({kAttr, kZero, kZero, kZero, kZero})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.grant_immunity(1, ImmunityType::ANOMALY, coverage_all(),
                           /*anomaly_mask=*/0, /*duration_rounds=*/9, /*source_id=*/8001,
                           /*soul_immunity=*/false, EffectScope::ON_STAGE,
                           /*counts=*/0, /*source_effect_id=*/8001, ImmunityTier::Modern);
        run_cast(b, 0, 256696296);   // r=49：掷点过闸、免疫另挡
        CHECK_EQ(aint(ctx, 0, 4, -1), static_cast<int>(ApplyAnomalyResult::TARGET_IMMUNE),
                 "T7 施加结果=TARGET_IMMUNE（掷点过闸但被免疫挡）");
        CHECK_EQ(aint(ctx, 0, 5), 0, "T7 landed=0");
        CHECK_EQ(aint(ctx, 0, 6), 100000, "T7 兜底量=自身最大体力/3=100000");
        CHECK_EQ(aint(ctx, 0, 8), 1, "T7 兜底计数=1");
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Burn)) == false,
              "T7 烧伤未落地");
        const int loss = kFoeMax - ctx.getPet(1).hp;
        CHECK(loss >= 100000 && loss <= 100010,
              "T7 hp 损失=100000+红伤带 [0,10]（实测 " + std::to_string(loss) + "）");
        std::cout << "  [T7] 免疫兜底 通过\n";
    }

    //========== T8 闸门改写（CHANCE_GATE below→100：闸门在掷骰前管辖）==========
    {
        ScenarioBattle b(scenario::make_party(caster({kLianShen, kZero, kZero, kZero, kZero})),
                        scenario::make_party(foe({kAttr, kZero, kZero, kZero, kZero})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.grant_chance_gate(1, 8002, ChanceTag::AnomalyAttach, /*threshold_pct=*/50,
                              /*below_result=*/100, /*above_result=*/-1,
                              kChanceSourceAll, /*rounds=*/9);
        run_cast(b, 0, 1664373296);   // r=50：无闸时被拒（T2 同种子互证）→ 改写 100 后落地
        CHECK_EQ(aint(ctx, 0, 0, -1), 50, "T8 申报 P=50");
        CHECK_EQ(aint(ctx, 0, 3), 100, "T8 审计记改写后实际概率=100（闸门管辖）");
        CHECK_EQ(aint(ctx, 0, 4, -1), static_cast<int>(ApplyAnomalyResult::SUCCESS),
                 "T8 改写后 >=100 短路恒 SUCCESS");
        CHECK_EQ(aint(ctx, 0, 5), 1, "T8 landed=1");
        CHECK_EQ(aint(ctx, 0, 6), 0, "T8 兜底量=0");
        CHECK_EQ(aint(ctx, 0, 8), 0, "T8 兜底计数=0");
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Fatigue)),
              "T8 疲惫落地（闸门在掷骰前改写——r=50 原本被拒）");
        std::cout << "  [T8] 闸门改写 通过\n";
    }

    //========== T9 [917,1000] 37377 混链：寄生落地 + 审计在位（parser 半解析清账）==========
    {
        ScenarioBattle b(scenario::make_party(caster({kFenMo, kZero, kZero, kZero, kZero})),
                        scenario::make_party(foe({kAttr, kZero, kZero, kZero, kZero})));
        b.start();
        BattleContext& ctx = b.ctx();
        run_cast(b, 0, 42);
        CHECK_EQ(aint(ctx, 0, 0, -1), 100, "T9 混链 917 切片申报 P=100");
        CHECK_EQ(aint(ctx, 0, 1, -1), 4, "T9 申报异常码=4（寄生）");
        CHECK_EQ(aint(ctx, 0, 2, -1), 4, "T9 申报分母=4（1000 的参不串位）");
        CHECK_EQ(aint(ctx, 0, 5), 1, "T9 landed=1");
        CHECK_EQ(aint(ctx, 0, 7), 1, "T9 landed 计数=1");
        CHECK_EQ(aint(ctx, 0, 6), 0, "T9 兜底量=0");
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Parasite)),
              "T9 寄生落地（J129 记档的 parser 100% 上寄生行为，如今由插件正规驱动）");
        CHECK(ahas(ctx, 0, 0),
              "T9 审计槽在位=插件驱动（parser 半解析无审计——skills.cpp:671 只在无插件函数时走）");
        std::cout << "  [T9] 37377 混链（parser 清账）通过\n";
    }

    //========== T10 双方镜像（两侧 19513；d1=51 拒 / d3=17 过）==========
    {
        ScenarioBattle b(scenario::make_party(caster({kLianShen, kZero, kZero, kZero, kZero})),
                        scenario::make_party(foe({kLianShen, kZero, kZero, kZero, kZero},
                                                 /*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        // 我方 roll=第 1 抽（51 拒）→ 红 float 第 2 抽；对手 roll=第 3 抽（17 过）→ 第 4 抽
        ctx.seed_rng(924566649);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(static_cast<int>(minstd_draw(924566649, 1) % 100), 51, "T10 闭式核验 d1=51");
        CHECK_EQ(static_cast<int>(minstd_draw(924566649, 3) % 100), 17, "T10 闭式核验 d3=17");
        CHECK_EQ(aint(ctx, 0, 5), 0, "T10 己方 r=51 拒 → landed=0");
        CHECK_EQ(aint(ctx, 0, 6), 100000, "T10 己方兜底=150威力载体 max/3=100000（denom=3）");
        CHECK_EQ(aint(ctx, 0, 8), 1, "T10 己方兜底计数=1");
        CHECK_EQ(aint(ctx, 1, 5), 1, "T10 对方 r=17 过 → landed=1（独立键段独立抽取）");
        CHECK_EQ(aint(ctx, 1, 7), 1, "T10 对方 landed 计数=1");
        CHECK_EQ(aint(ctx, 1, 6), 0, "T10 对方兜底量=0");
        CHECK(ctx.has_active_abnormal_status(0, static_cast<int>(AbnormalStatusId::Fatigue)),
              "T10 疲惫落在我方（对手 917 触发；19513 载体码=7）");
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Fatigue)) == false,
              "T10 对方无疲惫（己方 917 未触发）");
        const int loss1 = kFoeMax - ctx.getPet(1).hp;
        CHECK(loss1 >= 100000 && loss1 <= 100010,
              "T10 对方 hp 损失=100000+红伤带 [0,10]（实测 " + std::to_string(loss1) + "）");
        std::cout << "  [T10] 双方镜像隔离 通过\n";
    }

    //========== T11 确定性（同种子两次对局审计链全等）==========
    {
        std::tuple<int, int, int, int, int, int> first;
        for (int rep = 0; rep < 2; ++rep) {
            ScenarioBattle b(scenario::make_party(caster({kMoFa, kZero, kZero, kZero, kZero})),
                            scenario::make_party(foe({kAttr, kZero, kZero, kZero, kZero})));
            b.start();
            BattleContext& ctx = b.ctx();
            run_cast(b, 0, 616377766);
            auto cur = std::make_tuple(aint(ctx, 0, 3), aint(ctx, 0, 4, -1), aint(ctx, 0, 5),
                                       aint(ctx, 0, 6), aint(ctx, 0, 7),
                                       ctx.getPet(1).hp);
            if (rep == 0) {
                first = cur;
            } else {
                CHECK(first == cur, "T11 同种子两次对局审计链逐项全等");
            }
        }
        std::cout << "  [T11] 确定性 通过\n";
    }

    //========== T12 固定 20 种子扫描：逐种子 landed==(r<50) 一致 + 过闸率侧证 ==========
    {
        int passes = 0;
        for (int s = 0; s < 20; ++s) {
            const int seed = 3000 + s * 7919;
            ScenarioBattle b(scenario::make_party(caster({kLianShen, kZero, kZero, kZero, kZero})),
                            scenario::make_party(foe({kAttr, kZero, kZero, kZero, kZero})));
            b.start();
            BattleContext& ctx = b.ctx();
            run_cast(b, 0, seed);
            const int r = static_cast<int>(minstd_draw(seed, 1) % 100);
            const int landed = aint(ctx, 0, 5);
            CHECK_EQ(landed, (r < 50) ? 1 : 0,
                     "T12 逐种子 landed==(闭式 r<50)（seed=" + std::to_string(seed) +
                         " r=" + std::to_string(r) + "）");
            if (landed == 1) {
                ++passes;
                CHECK_EQ(aint(ctx, 0, 6), 0, "T12 过闸种子零兜底");
                CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Fatigue)),
                      "T12 过闸种子疲惫在位");
            } else {
                CHECK_EQ(aint(ctx, 0, 6), 100000, "T12 拒绝种子兜底=19513 max/3=100000（denom=3）");
                CHECK_EQ(aint(ctx, 0, 4, -1),
                         static_cast<int>(ApplyAnomalyResult::ROLL_FAILED),
                         "T12 拒绝种子结果码=ROLL_FAILED");
            }
        }
        CHECK(passes >= 1 && passes <= 19,
              "T12 固定 20 种子过闸计数在 50% 侧证带内（实测确定值，非随机断言）");
        std::cout << "  [T12] 种子扫描通过（过闸 " << passes << "/20）\n";
    }

    //========== T13 miss 门（31934 accuracy=0 → HIT 分支不注册）==========
    {
        ScenarioBattle b(scenario::make_party(caster({kBaoLie, kZero, kZero, kZero, kZero})),
                        scenario::make_party(foe({kAttr, kZero, kZero, kZero, kZero})));
        b.start();
        BattleContext& ctx = b.ctx();
        for (Skills& s : ctx.getPet(0).skills) {
            s.must_hit = 0;
            s.accuracy = 0;
        }
        run_cast(b, 0, 256696296);
        CHECK(ahas(ctx, 0, 0) == false, "T13 审计键缺席（SKILL_EFFECT 桶未跑）");
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Burn)) == false,
              "T13 miss 不施异常");
        CHECK_EQ(ctx.getPet(1).hp, kFoeMax, "T13 miss 无红伤无粉伤");
        std::cout << "  [T13] miss 门 通过\n";
    }

    std::cout << "scenario_8050 all passed" << std::endl;
    return 0;
}
