#include <utils/sbs_rand.h>
// 场景 103 —— 概率闸门管道 + 亮节（"改概率"印记族的地基，2026-09-22）
//
// 锁的是"概率闸门"这一层的契约（设计口径见 include/effects/rule_center.h 的
// ChanceTag / ChanceSource / RuleCategory::CHANCE_GATE 三段注释，与 resources/chance_gate_lib.h）：
//
//   A 闸门语义（纯查询，无随机）：阈值边界 / 来源掩码过滤 / 多票按授予序折叠 / 未申报不介入
//   B 原语集成：apply_anomaly 申报概率 → **掷骰之前**过闸门 → 没过门返回 ROLL_FAILED
//   C 生命周期：回合窗口到期 / ON_STAGE 换宠清 vs TEAM 保留
//   D 随机异常附加的**整子句门**也过闸门（它绕过 apply_anomaly，必须单独接）
//   E 端到端·特性主动毒（common_trait 的 Ancient 通道迁移）
//   F 端到端·特性被动毒（common_trait 的 Raw 通道迁移——亮节原文点名覆盖它）
//   G 端到端·解析器模板（effect_unit 的 roll_chance 迁移）+ **未触发补偿不受影响**
//
// 官方依据：
//   · 亮节（无极圣武 4800 魂印 effect 2436 / 帝皇侠复用）："精灵持有时，套装、特性、
//     魂印、技能中不高于 50% 的附加异常效果全部降低为 0%（直接针对控场宝石、主动毒、被动毒）"
//   · 口径「**以实际概率为准**」（语料《机制解析—无极圣武》：001 的战栗特性被提升到
//     100% 就不受影响；克罗首次 40% 会变 0%、之后超过 50% 的不受影响）
//   · 梅赫维特《机制解析》："不高于 50% 控场效果本质上是个将概率降低至 0%，
//     **不影响控场未触发补偿**" ← G1 锁的就是这句
//
// ⚠️ 本场景**不锁**"概率挡伤/概率强化"等其它概率类别：亮节只管异常附加（ChanceTag
//    只有一个值），那些类别**不走闸门**——这是刻意的按类切分，不是漏做。
#include <cstdlib>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <effects/rule_center.h>
#include <effects/trait_state.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "../../resources/chance_gate_lib.h"
#include "scenario_harness.h"

namespace {

constexpr int kPaiDa = 10004;      // 拍打：物攻 40 必中无效果
constexpr int kGaoSu = 20017;      // 高速移动：属性技能（不打伤害）
constexpr int kHongYan = 19531;    // 红炎冲击波：特攻 150，effect 916「{0}%使对手{1}，
                                   // 未触发则恢复自身全部体力」→ 走**解析器**（无 C++ 函数）

using scenario::ScenarioBattle;

// 攻方/守方：atk 压低、hp 拉高，保证多回合驱动不因死亡中断。
ElfPet plain_pet(int id, const char* name, int skill, int speed,
                 CommonTrait trait = {}) {
    return scenario::make_pet(id, name, {skill, skill, skill, skill, skill},
                              speed, /*atk=*/1, /*hp=*/3000, {}, trait);
}

void strike(ScenarioBattle& b, int s0 = 0, int s1 = 0) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, s0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, s1);
    b.drive();
}

// 逐回合驱动并记录"是否**曾经**中过某异常"。
// ⚠️ 不能在末尾只查一次：异常 2~3 回合自然到期，早期中的那一次末尾早没了
//    （H2/E3/G2 都踩过——40% 打 20 回合，末尾 2 回合都没中的概率仍有 36%）。
bool ever_gets_status(ScenarioBattle& b, int side, int status, int rounds) {
    for (int r = 0; r < rounds && b.state() != State::FINISHED; ++r) {
        strike(b);
        if (b.ctx().has_active_abnormal_status(side, status)) {
            return true;
        }
    }
    return false;
}

// 直投一次现代异常施加（走申报通道），返回原语结果。
ApplyAnomalyResult try_anomaly(BattleContext& x, int chance_pct) {
    return apply_anomaly(&x, /*target=*/1, static_cast<int>(AbnormalStatusId::Paralysis),
                         /*duration_rounds=*/-1, /*actor=*/0, chance_pct,
                         ChanceSource::Skill);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }
    sbs_srand(20260922);   // 固定序列：需要掷骰的组可复现
    const int paralysis = abnormal_status_id(AbnormalStatusId::Paralysis);
    const int burn = abnormal_status_id(AbnormalStatusId::Burn);

    // ════════════════════════════════════════════════════════════════
    // A. 闸门语义（纯查询，无随机）
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle a(scenario::make_party(plain_pet(0, "攻", kPaiDa, 200)),
                         scenario::make_party(plain_pet(1, "守", kPaiDa, 100)));
        a.start();
        BattleContext& x = a.ctx();

        // A1 无票：原样返回；未申报（<0）也原样返回（结构性：闸门不介入）
        CHECK_EQ(x.rewrite_anomaly_chance(1, 0, 30, ChanceSource::Skill), 30, "A1 无票不改写");
        CHECK_EQ(x.rewrite_anomaly_chance(1, 0, -1, ChanceSource::Skill), -1,
                 "A1 未申报原样返回");

        // A2 亮节在身：≤50 → 0；>50 不改写（亮节只压不抬）
        chance_gate::grant_liangjie(&x, /*side=*/1, /*rounds=*/0);
        CHECK_EQ(x.rewrite_anomaly_chance(1, 0, 30, ChanceSource::Skill), 0, "A2 30→0");
        CHECK_EQ(x.rewrite_anomaly_chance(1, 0, 50, ChanceSource::Skill), 0,
                 "A2 边界：50 属『不高于50%』→0");
        CHECK_EQ(x.rewrite_anomaly_chance(1, 0, 51, ChanceSource::Skill), 51, "A2 51 不改写");
        CHECK_EQ(x.rewrite_anomaly_chance(1, 0, 100, ChanceSource::Skill), 100, "A2 100 不改写");
        // 归属：护的是**持有方自己**——0 号位（没亮节）问闸门时不受影响
        CHECK_EQ(x.rewrite_anomaly_chance(0, 1, 30, ChanceSource::Skill), 30,
                 "A2 闸门只护持有方（0 号位不受 1 号位的亮节影响）");
        std::cout << "  [A1/A2] 阈值与边界通过\n";

        // A3 来源掩码过滤：只含 Skill 的票，对 SoulMark / Trait 来源不生效
        ScenarioBattle a3(scenario::make_party(plain_pet(0, "攻", kPaiDa, 200)),
                          scenario::make_party(plain_pet(1, "守", kPaiDa, 100)));
        a3.start();
        BattleContext& y = a3.ctx();
        y.grant_chance_gate(1, 9001, ChanceTag::AnomalyAttach, 50, 0, -1,
                            chance_source_bit(ChanceSource::Skill), /*rounds=*/0,
                            EffectScope::ON_STAGE);
        CHECK_EQ(y.rewrite_anomaly_chance(1, 0, 30, ChanceSource::Skill), 0, "A3 Skill 被管");
        CHECK_EQ(y.rewrite_anomaly_chance(1, 0, 30, ChanceSource::SoulMark), 30,
                 "A3 SoulMark 不在掩码内 → 不改写");
        CHECK_EQ(y.rewrite_anomaly_chance(1, 0, 30, ChanceSource::Trait), 30,
                 "A3 Trait 不在掩码内 → 不改写");
        std::cout << "  [A3] 来源掩码过滤通过\n";

        // A4 多票按**授予顺序折叠**：亮节(≤50→0) + 抬档票(>50→100)
        ScenarioBattle a4(scenario::make_party(plain_pet(0, "攻", kPaiDa, 200)),
                          scenario::make_party(plain_pet(1, "守", kPaiDa, 100)));
        a4.start();
        BattleContext& z = a4.ctx();
        chance_gate::grant_liangjie(&z, 1, 0);
        z.grant_chance_gate(1, 9002, ChanceTag::AnomalyAttach, 50, /*below=*/-1, /*above=*/100,
                            kChanceSourceAll, 0, EffectScope::ON_STAGE);
        CHECK_EQ(z.rewrite_anomaly_chance(1, 0, 30, ChanceSource::Skill), 0,
                 "A4 折叠：30 → 亮节压成 0 → 抬档票看 0≤50 → 不管");
        CHECK_EQ(z.rewrite_anomaly_chance(1, 0, 60, ChanceSource::Skill), 100,
                 "A4 折叠：60 → 亮节不管 → 抬档票抬到 100");
        std::cout << "  [A4] 多票折叠通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // B. 原语集成：申报概率 → 掷骰之前过闸门 → ROLL_FAILED
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(scenario::make_party(plain_pet(0, "攻", kPaiDa, 200)),
                         scenario::make_party(plain_pet(1, "守", kPaiDa, 100)));
        b.start();
        BattleContext& x = b.ctx();

        // B1 未申报（-1）+ 无票 → 必定落地；B2 申报 0 → 必定不落地
        CHECK_EQ(try_anomaly(x, -1), ApplyAnomalyResult::SUCCESS, "B1 未申报 = 必定");
        CHECK_EQ(try_anomaly(x, 0), ApplyAnomalyResult::ROLL_FAILED, "B2 申报 0 → ROLL_FAILED");
        std::cout << "  [B1/B2] 申报语义通过\n";
    }
    {
        ScenarioBattle b(scenario::make_party(plain_pet(0, "攻", kPaiDa, 200)),
                         scenario::make_party(plain_pet(1, "守", kPaiDa, 100)));
        b.start();
        BattleContext& x = b.ctx();
        chance_gate::grant_liangjie(&x, 1, 0);
        const ApplyAnomalyResult r = try_anomaly(x, -1);
        CHECK(r == ApplyAnomalyResult::SUCCESS || r == ApplyAnomalyResult::DURATION_EXTENDED,
              "B3 未申报 + 亮节 → 闸门不介入，照常落地");
        std::cout << "  [B3] 未申报不被闸门介入\n";
    }
    {
        ScenarioBattle b(scenario::make_party(plain_pet(0, "攻", kPaiDa, 200)),
                         scenario::make_party(plain_pet(1, "守", kPaiDa, 100)));
        b.start();
        BattleContext& x = b.ctx();
        chance_gate::grant_liangjie(&x, 1, 0);
        // B4 亮节 + 申报 30 → **确定性** ROLL_FAILED（闸门把它压成 0）
        CHECK_EQ(try_anomaly(x, 30), ApplyAnomalyResult::ROLL_FAILED,
                 "B4 亮节把 30 压成 0 → 必定 ROLL_FAILED");
        // B5 亮节 + 申报 60 → >50 不受影响，多次尝试必然中过（确定性档见文件头）
        int landed = 0;
        for (int i = 0; i < 30; ++i) {
            if (try_anomaly(x, 60) != ApplyAnomalyResult::ROLL_FAILED) {
                ++landed;
            }
        }
        CHECK(landed > 0, "B5 60% > 阈值 → 亮节不压制，30 次里应至少落地一次");
        std::cout << "  [B4/B5] 阈值两侧行为通过（60% 落地 " << landed << "/30）\n";
    }

    // ════════════════════════════════════════════════════════════════
    // C. 生命周期：回合窗口到期 / ON_STAGE 换宠清
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle c(scenario::make_party(plain_pet(0, "攻", kPaiDa, 200)),
                         scenario::make_party(plain_pet(1, "守", kPaiDa, 100)));
        c.start();
        BattleContext& x = c.ctx();
        chance_gate::grant_liangjie(&x, 1, /*rounds=*/2);
        CHECK_EQ(x.rewrite_anomaly_chance(1, 0, 30, ChanceSource::Skill), 0, "C1 窗口内生效");
        x.roundCount += 2;   // 过窗口（register_round + 2）
        CHECK_EQ(x.rewrite_anomaly_chance(1, 0, 30, ChanceSource::Skill), 30,
                 "C1 窗口过后不再改写（亮节 3 回合到期就没了）");

        ScenarioBattle c2(scenario::make_party(plain_pet(0, "攻", kPaiDa, 200)),
                          scenario::make_party(plain_pet(1, "守", kPaiDa, 100)));
        c2.start();
        BattleContext& y = c2.ctx();
        chance_gate::grant_liangjie(&y, 1, 0);   // ON_STAGE
        y.grant_chance_gate(1, 9003, ChanceTag::AnomalyAttach, 50, 0, -1, kChanceSourceAll, 0,
                            EffectScope::TEAM);
        y.rule_center_.clear_on_stage(1, -1);    // 换宠
        CHECK_EQ(y.rewrite_anomaly_chance(1, 0, 30, ChanceSource::Skill), 0,
                 "C2 换宠只清 ON_STAGE 票；TEAM 票（永沐式『位于出战背包』）保留");
        std::cout << "  [C1/C2] 生命周期通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // D. 随机异常附加的整子句门（绕过 apply_anomaly，必须单独接闸门）
    // ════════════════════════════════════════════════════════════════
    {
        const std::vector<int> pool = {static_cast<int>(AbnormalStatusId::Paralysis),
                                       static_cast<int>(AbnormalStatusId::Sleep)};
        ScenarioBattle d(scenario::make_party(plain_pet(0, "攻", kPaiDa, 200)),
                         scenario::make_party(plain_pet(1, "守", kPaiDa, 100)));
        d.start();
        BattleContext& x = d.ctx();
        chance_gate::grant_liangjie(&x, 1, 0);
        // D1 申报 40（≤50）→ 整子句门被压成 0 → 一次都没掷
        const RandomAnomalyResult r1 =
            attach_random_anomalies(&x, 1, pool, /*count=*/1, /*probability_pct=*/40,
                                    /*actor=*/0, ChanceSource::Skill);
        CHECK_EQ(r1.rolled, 0, "D1 整子句门被亮节压成 0 → rolled=0");
        // D2 申报 100（>50）→ 不受影响
        const RandomAnomalyResult r2 =
            attach_random_anomalies(&x, 1, pool, /*count=*/1, /*probability_pct=*/100,
                                    /*actor=*/0, ChanceSource::Skill);
        CHECK_EQ(r2.rolled, 1, "D2 100% > 阈值 → 不受亮节影响");
        CHECK_EQ(r2.landed, 1, "D2 落地 1 只");
        std::cout << "  [D1/D2] 随机异常整子句门通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // E. 端到端·特性主动毒（Ancient 通道；args[0] 是百分点）
    // ════════════════════════════════════════════════════════════════
    {
        // E1 静电 100% + 守方亮节 → 100 > 50 不受影响（亮节不会过度封锁）
        ScenarioBattle e1(
            scenario::make_party(plain_pet(0, "攻", kPaiDa, 200,
                                           scenario::make_trait("静电", 5, {100, 0}))),
            scenario::make_party(plain_pet(1, "守", kPaiDa, 100)));
        e1.start();
        chance_gate::grant_liangjie(&e1.ctx(), 1, 0);
        strike(e1);
        CHECK(e1.ctx().has_active_abnormal_status(1, paralysis),
              "E1 静电 100% > 阈值 → 亮节不压制，照常施加");

        // E2 静电 40% + 守方亮节 → 被压成 0 → 多回合必然一次都不中
        ScenarioBattle e2(
            scenario::make_party(plain_pet(0, "攻", kPaiDa, 200,
                                           scenario::make_trait("静电", 5, {40, 0}))),
            scenario::make_party(plain_pet(1, "守", kPaiDa, 100)));
        e2.start();
        chance_gate::grant_liangjie(&e2.ctx(), 1, 0);
        for (int r = 0; r < 12 && e2.state() != State::FINISHED; ++r) {
            strike(e2);
        }
        CHECK(!e2.ctx().has_active_abnormal_status(1, paralysis),
              "E2 亮节把 40% 压成 0 → 12 回合一次都不中");

        // E3 对照：同样的 40% 静电，**无亮节** → 12 回合里应至少中一次
        ScenarioBattle e3(
            scenario::make_party(plain_pet(0, "攻", kPaiDa, 200,
                                           scenario::make_trait("静电", 5, {40, 0}))),
            scenario::make_party(plain_pet(1, "守", kPaiDa, 100)));
        e3.start();
        CHECK(ever_gets_status(e3, 1, paralysis, 12),
              "E3 对照：无亮节时 40% 静电 12 回合内应至少中一次（证明 E2 的『不中』是闸门造成的）");
        std::cout << "  [E1/E2/E3] 特性主动毒（Ancient 通道）端到端通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // F. 端到端·特性被动毒（Raw 通道；亮节原文点名覆盖它）
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle f(
            scenario::make_party(plain_pet(0, "攻", kPaiDa, 200)),
            scenario::make_party(plain_pet(1, "守", kPaiDa, 100,
                                           scenario::make_trait("带电", 5, {40, 0}))));
        f.start();
        // 被动毒弹给**攻方** → 闸门护的也是攻方（谁挨打谁戴亮节）
        chance_gate::grant_liangjie(&f.ctx(), 0, 0);
        for (int r = 0; r < 12 && f.state() != State::FINISHED; ++r) {
            strike(f);
        }
        CHECK(!f.ctx().has_active_abnormal_status(0, paralysis),
              "F1 攻方戴亮节 → 守方 40% 带电（Raw 通道）被压成 0，12 回合一次都不中");
        std::cout << "  [F1] 特性被动毒（Raw 通道）端到端通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // G. 端到端·解析器模板（effect_unit::roll_chance）+ 未触发补偿不受影响
    //    19531 红炎冲击波：effect 916「命中后{0}%使对手{1}，若未触发则恢复自身全部体力」
    //    args=[50, 2] → 50% 令对手烧伤，未触发则自身回满
    // ════════════════════════════════════════════════════════════════
    {
        // G1 守方戴亮节 → 50（≤50）被压成 0 → 不烧伤 + **兜底回血照常**
        ScenarioBattle g1(scenario::make_party(plain_pet(0, "攻", kHongYan, 200)),
                          scenario::make_party(plain_pet(1, "守", kGaoSu, 100)));
        g1.start();
        chance_gate::grant_liangjie(&g1.ctx(), 1, 0);
        ElfPet& self = g1.ctx().getPet(0);
        const int max_hp = static_cast<int>(self.numericalBase[NumericalPropertyIndex::HP]);
        self.hp = max_hp / 2;   // 先扣血，好让"未触发则恢复自身全部体力"可观测
        strike(g1);
        CHECK(!g1.ctx().has_active_abnormal_status(1, burn),
              "G1 亮节把 50% 压成 0 → 守方不烧伤");
        CHECK_EQ(g1.ctx().getPet(0).hp, max_hp,
                 "G1 **未触发补偿不受影响**：没触发 → 兜底恢复自身全部体力照常执行");

        // G2 对照：无亮节 → 多回合内应至少烧伤一次
        ScenarioBattle g2(scenario::make_party(plain_pet(0, "攻", kHongYan, 200)),
                          scenario::make_party(plain_pet(1, "守", kGaoSu, 100)));
        g2.start();
        CHECK(ever_gets_status(g2, 1, burn, 12),
              "G2 对照：无亮节时 50% 模板 12 回合内应至少烧伤一次");
        std::cout << "  [G1/G2] 解析器模板 + 未触发补偿通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // H. 端到端·古早施加族共用体（apply_ancient_hit_status；effects 10/11/12/14/15/16/114，
    //    176 个技能在用）。13875 龙之怒吼：effect 15「命中后{0}%令对方害怕」，args=[40]
    // ════════════════════════════════════════════════════════════════
    {
        constexpr int kLongNuHou = 13875;
        const int fear = abnormal_status_id(AbnormalStatusId::Fear);

        // H1 守方戴亮节 → 40（≤50）被压成 0 → 多回合必然一次都不中
        ScenarioBattle h1(scenario::make_party(plain_pet(0, "攻", kLongNuHou, 200)),
                          scenario::make_party(plain_pet(1, "守", kPaiDa, 100)));
        h1.start();
        chance_gate::grant_liangjie(&h1.ctx(), 1, 0);
        CHECK(!ever_gets_status(h1, 1, fear, 20), "H1 亮节把 40% 压成 0 → 20 回合一次都不中");

        // H2 对照：无亮节 → 20 回合里至少中一次
        ScenarioBattle h2(scenario::make_party(plain_pet(0, "攻", kLongNuHou, 200)),
                          scenario::make_party(plain_pet(1, "守", kPaiDa, 100)));
        h2.start();
        CHECK(ever_gets_status(h2, 1, fear, 20),
              "H2 对照：无亮节时 40% 古早施加族 20 回合内应至少中一次");
        std::cout << "  [H1/H2] 古早施加族共用体端到端通过\n";
    }

    std::cout << "场景 103（概率闸门 + 亮节）全部通过\n";
    return 0;
}
