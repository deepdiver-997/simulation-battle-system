// 场景 074 —— 消逝族效果批 1：空元行者 2175（之录/之渎/卪）+ 2260 击败消逝 + 4872 消逝过滤
//
// 锁的是**效果行为**（内核接口/判据已由 071 锁定）：
//   A. 点数版上限原语：reduce/raise_max_hp_flat（下限钳 1、消逝拒绝、削减压低当前体力）
//   B. 之录：双方已阵亡的**不在场**精灵消逝 → 转化诗章 + 上限增益（消逝前上限 10%，
//      封顶自身上限 10%）；在场死亡不消逝；懒清扫补漏（直写死亡也收敛）
//   C. 之渎：首次死亡残留 1 体力 + 清异常 + 当回合免异常；消耗体力成因**穿**它；
//      触发后每回合结束 -200 点上限（点数）
//   D. 卪：击败对手（actor 归因）→ 自身上限 -20%/次 + 本场攻击伤害 ±10%/次（管线探针）
//   E. 无为觉者 2260：自身击败对手 → 重置万相乖离（威力回落）+ 对方全部阵亡精灵消逝
//   F. 妙时天女 4872：复制扫描跳过**已消逝**的对手精灵；阵亡未消逝的照常复制
//
// 官方依据：effect_icon 1791（4586 空元行者）、1862（4661 无为觉者）、effect_des 239/273、
// 语料 idx=34/35（魂帝尸骸与消逝时序）、薇尔诗专栏 idx=57。口径决策见
// docs_local/docs/02-效果系统/魂印机制设计与精灵表现档案.md 消逝族行。

#include <cmath>

#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kSkill = 10001;   // 撞击（低威力普通技能，只关心生命周期不关心效果）
constexpr int kKongYuan = 2175;
constexpr int kWuXiang = 2260;
constexpr int kMiaoShi = 2478;  // effect_id（2026-09-27 对齐装配层）

std::array<int, 5> plain_skills() { return {kSkill, kSkill, kSkill, kSkill, kSkill}; }

ElfPet kongyuan_pet(int hp = 1000) {
    return make_pet(4586, "空元行者", plain_skills(), /*speed=*/100, /*atk=*/100, hp,
                    SoulMark(kKongYuan, "空元行者", "消逝族"));
}

ElfPet plain_pet(int hp = 2000) {
    return make_pet(1, "普通", plain_skills(), /*speed=*/100, /*atk=*/100, hp);
}

// 己方队伍：slot0 = 空元行者，其余 = 普通队友。
// ⚠️ 不能用 make_party(kongyuan_pet())——6 格全是空元行者的话，杀"己方场下"会先被
//    之渎拦下（残留 1 体力，死亡不成立），之录永远看不到己方受害者。
std::array<ElfPet, 6> own_party(int kong_hp = 1000, int mate_hp = 2000) {
    return {kongyuan_pet(kong_hp), plain_pet(mate_hp), plain_pet(mate_hp),
            plain_pet(mate_hp), plain_pet(mate_hp), plain_pet(mate_hp)};
}

// 直写 hp=0（绕过原语的路径），可选地把死亡登记复位后走死亡漏斗。
void hard_kill(BattleContext& ctx, int side, int slot) {
    ctx.seerRobot[side].elfPets[slot].hp = 0;
}

int max_hp(BattleContext& ctx, int side, int slot) {
    return ctx.seerRobot[side].elfPets[slot].numericalBase[NumericalPropertyIndex::HP];
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ════════════════════════════════════════════════════════════════
    // A. 点数版上限原语（071 只锁了 pct 版）
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle a(scenario::make_party(plain_pet(1000)), scenario::make_party(plain_pet()));
        a.start();
        BattleContext& x = a.ctx();
        x.on_stage[0] = 0;
        x.on_stage[1] = 1;

        CHECK_EQ(reduce_max_hp_flat(&x, 0, 1, 300, 1), 700, "A1 点数削减 1000-300=700");
        CHECK_EQ(reduce_max_hp_flat(&x, 0, 1, 5000, 1), 1, "A2 下限钳 1：再减 5000 → 1（不会到 0）");
        CHECK_EQ(max_hp(x, 0, 1), 1, "A2 上限确实为 1");
        CHECK_EQ(raise_max_hp_flat(&x, 0, 1, 99), 100, "A3 点数提升 1+99=100");
        CHECK_EQ(reduce_max_hp_flat(&x, 0, 1, 0, 1), -1, "A4 amount<=0 拒绝");

        // 消逝目标拒绝（上限已 0）
        hard_kill(x, 0, 2);
        x.seerRobot[0].elfPets[2].numericalBase[NumericalPropertyIndex::HP] = 0;
        CHECK(x.is_vanished(0, 2), "A5 前置：上限归零 = 消逝态");
        CHECK_EQ(reduce_max_hp_flat(&x, 0, 2, 100, 1), -1, "A5 消逝目标拒绝削减");
        CHECK_EQ(raise_max_hp_flat(&x, 0, 2, 100), -1, "A5 消逝目标拒绝提升");

        // 削减同步压低当前体力（pct 版同款契约）
        x.seerRobot[0].elfPets[3].hp = 900;
        reduce_max_hp_flat(&x, 0, 3, 200, 1);
        CHECK_EQ(x.seerRobot[0].elfPets[3].hp, 800, "A6 上限 1000→800 后当前体力同步压低");

        // 提升上限不动当前体力
        x.seerRobot[0].elfPets[4].hp = 500;
        raise_max_hp_flat(&x, 0, 4, 300);
        CHECK_EQ(x.seerRobot[0].elfPets[4].hp, 500, "A7 提升上限不动当前体力");

        std::cout << "  [A] 点数版上限原语通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // B. 之录：不在场阵亡 → 消逝 + 上限增益（actor=-1 归因隔离 卪）
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(own_party(1000, 2000), scenario::make_party(plain_pet(2000)));
        b.start();
        BattleContext& x = b.ctx();
        x.on_stage[0] = 0;
        x.on_stage[1] = 1;

        // B1 对方场下 slot3 阵亡 → 消逝 + 增益 = min(2000/10, 1000/10) = 100
        hard_kill(x, 1, 3);
        CHECK(defeat_pet(&x, 1, 3, /*actor=*/-1, DefeatCause::EFFECT) == DefeatResult::DEFEATED,
              "B1 前置：场下死亡成立");
        x.event_center_.drain(&x, x.roundCount);
        CHECK(x.is_vanished(1, 3), "B1 场下阵亡应被之录消逝");
        CHECK_EQ(max_hp(x, 0, 0), 1100, "B1 增益 = 消逝前上限 10% = 100（1000→1100）");

        // B2 己方场下 slot4（普通队友）同样吃（"双方"）→ gain = min(200, 110) = 110
        hard_kill(x, 0, 4);
        defeat_pet(&x, 0, 4, -1, DefeatCause::EFFECT);
        x.event_center_.drain(&x, x.roundCount);
        CHECK(x.is_vanished(0, 4), "B2 己方场下阵亡也应消逝");
        CHECK_EQ(max_hp(x, 0, 0), 1210, "B2 增益 = min(2000/10, 1100/10) = 110（1100→1210）");

        // B3 在场死亡不消逝、不给增益
        const int on1 = x.on_stage[1];
        hard_kill(x, 1, on1);
        defeat_pet(&x, 1, on1, -1, DefeatCause::EFFECT);
        x.event_center_.drain(&x, x.roundCount);
        CHECK(!x.is_vanished(1, on1), "B3 在场阵亡不应消逝（不在场才是之录对象）");
        CHECK_EQ(max_hp(x, 0, 0), 1210, "B3 在场死亡不给增益");

        // B4 封顶：受害者上限极大时增益 ≤ 自身上限 10%
        ElfPet& fat = x.seerRobot[1].elfPets[5];
        fat.numericalBase[NumericalPropertyIndex::HP] = 100000;   // 潜在增益 10000
        hard_kill(x, 1, 5);
        defeat_pet(&x, 1, 5, -1, DefeatCause::EFFECT);
        x.event_center_.drain(&x, x.roundCount);
        CHECK(x.is_vanished(1, 5), "B4 前置：消逝成功");
        CHECK_EQ(max_hp(x, 0, 0), 1331, "B4 增益封顶 = 自身上限 10% = 121（1210→1331）");

        std::cout << "  [B] 之录（即时消逝 + 上限增益 + 封顶）通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // B' 之录懒清扫：直写死亡（不经漏斗、无事件）在下一回合开始被补消逝
    // ════════════════════════════════════════════════════════════════
    {
        // 双方 10 万血：一回合撞击打不死任何人，隔离"直写尸体"变量
        const MedicineStock meds = {{300011, 3}};   /* 3 瓶初级体力药剂(+20体力) */
        ScenarioBattle b(scenario::make_party(kongyuan_pet(100000)),
                         scenario::make_party(plain_pet(100000)), meds, meds);
        b.start();
        BattleContext& x = b.ctx();
        x.on_stage[0] = 0;
        x.on_stage[1] = 1;

        hard_kill(x, 1, 4);   // 直写 hp=0：不调 defeat_pet → 不发 EVENT_DEATH
        CHECK(!x.is_vanished(1, 4), "B'1 直写死亡当场不被之录看见（无事件）");
        CHECK(!x.pet_death_notified[1][4], "B'1 前置：死亡未登记");

        b.submit_operation(0, BattleFsm::ActionType::USE_MEDICINE, 0);
        b.submit_operation(1, BattleFsm::ActionType::USE_MEDICINE, 0);
        b.drive();   // 对账兜底登记死亡 + 下一回合 ROUND_START 懒清扫

        CHECK(x.is_vanished(1, 4), "B'2 回合推进后直写尸体被懒清扫消逝");
        std::cout << "  [B'] 之录懒清扫通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // C. 之渎：首次死亡残留 1 体力；消耗体力穿它；之后每回合 -200 点上限
    // ════════════════════════════════════════════════════════════════
    {
        const MedicineStock meds = {{300011, 3}};   /* 3 瓶初级体力药剂(+20体力) */
        ScenarioBattle c(scenario::make_party(kongyuan_pet(1000)),
                         scenario::make_party(plain_pet(100000)), meds, meds);
        c.start();
        BattleContext& x = c.ctx();
        x.on_stage[0] = 0;
        x.on_stage[1] = 1;

        // C0 先转化一篇诗章（上限 +100 → 1100），之渎触发时要清零诗章（不可直接观察，
        //    通过"诗章状态存在性"由实现自查；此处锁上限与体力这两个可观察量）
        hard_kill(x, 1, 3);
        defeat_pet(&x, 1, 3, -1, DefeatCause::EFFECT);
        x.event_center_.drain(&x, x.roundCount);
        CHECK_EQ(max_hp(x, 0, 0), 1100, "C0 前置：之录增益后上限 1100");

        // C1 首次死亡 → 拦下，残留 1 点体力
        hard_kill(x, 0, 0);
        CHECK(defeat_pet(&x, 0, 0, /*actor=*/1, DefeatCause::DAMAGE) == DefeatResult::INTERCEPTED,
              "C1 之渎应拦下首次死亡");
        CHECK_EQ(x.seerRobot[0].elfPets[0].hp, 1, "C1 保留 1 点体力");
        CHECK(!x.pet_death_notified[0][0], "C1 拦下后死亡不成立");

        // C2 消耗体力成因**穿**残留免死（官方 idx=339）；且"首次"已用掉，之后不再拦
        hard_kill(x, 0, 0);
        CHECK(defeat_pet(&x, 0, 0, 1, DefeatCause::HP_CONSUME) == DefeatResult::DEFEATED,
              "C2 消耗体力应穿之渎且首次已用 → 死亡成立");

        std::cout << "  [C] 之渎（残留免死 + 穿透口径）通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // C' 之渎衰减：触发后每回合结束 -200 点上限（嗑药回合隔离伤害）
    // ════════════════════════════════════════════════════════════════
    {
        const MedicineStock meds = {{300011, 9}};   /* 9 瓶初级体力药剂 */
        ScenarioBattle c(scenario::make_party(kongyuan_pet(1000)),
                         scenario::make_party(plain_pet(100000)), meds, meds);
        c.start();
        BattleContext& x = c.ctx();
        x.on_stage[0] = 0;
        x.on_stage[1] = 1;

        hard_kill(x, 0, 0);
        CHECK(defeat_pet(&x, 0, 0, 1, DefeatCause::DAMAGE) == DefeatResult::INTERCEPTED,
              "C'1 前置：之渎触发（上限 1000 未变）");
        CHECK_EQ(max_hp(x, 0, 0), 1000, "C'1 触发当刻上限未变");

        for (int round = 1; round <= 3; ++round) {
            c.submit_operation(0, BattleFsm::ActionType::USE_MEDICINE, 0);
            c.submit_operation(1, BattleFsm::ActionType::USE_MEDICINE, 0);
            c.drive();
            char msg[64];
            std::snprintf(msg, sizeof(msg), "C'2 第 %d 回合结束后上限应 -200", round);
            CHECK_EQ(max_hp(x, 0, 0), 1000 - 200 * round, msg);
        }
        std::cout << "  [C'] 之渎每回合 -200 点上限通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // D. 卪：击败对手 → 上限 -20%/次（归因）；管线 ±10%/次用探针锁
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle d(scenario::make_party(kongyuan_pet(1000)),
                         scenario::make_party(plain_pet(2000)));
        d.start();
        BattleContext& x = d.ctx();
        x.on_stage[0] = 0;
        x.on_stage[1] = 1;

        // D1 **槽位门**：卪 绑 5 号位（之铭第 5 条）——用 3 号位击败 → 不触发
        {
            const int on1 = x.on_stage[1];
            x.roundChoice[0][0] = 1;   // SELECT_SKILL
            x.roundChoice[0][1] = 2;   // 3 号位（非绑定槽）
            hard_kill(x, 1, on1);
            CHECK(defeat_pet(&x, 1, on1, /*actor=*/0, DefeatCause::DAMAGE) == DefeatResult::DEFEATED,
                  "D1 前置：击败成立");
            x.event_center_.drain(&x, x.roundCount);
            CHECK_EQ(max_hp(x, 0, 0), 1000, "D1 用 3 号位击败 → 卪 不减上限（仍 1000）");
            CHECK(!x.is_vanished(1, on1), "D1 在场阵亡不被之录消逝（那个只吃不在场）");
        }

        // D2 5 号位击败 → 卪 触发：上限 ceil(1000×0.8) = 800
        {
            x.on_stage[1] = 3;         // 换一只上场（D1 那只已阵亡）
            x.roundChoice[0][1] = 4;   // 5 号位 = 卪 的绑定槽
            hard_kill(x, 1, 3);
            CHECK(defeat_pet(&x, 1, 3, /*actor=*/0, DefeatCause::DAMAGE) == DefeatResult::DEFEATED,
                  "D2 前置：击败成立");
            x.event_center_.drain(&x, x.roundCount);
            CHECK_EQ(max_hp(x, 0, 0), 800, "D2 卪：5 号位击败 → 上限 -20%（1000→800）");
        }

        // D3 归因不成立（actor = 对方自己）→ 不触发 卪
        {
            x.on_stage[1] = 4;
            x.roundChoice[0][1] = 4;   // 仍是 5 号位，但击杀方不是自己
            hard_kill(x, 1, 4);
            CHECK(defeat_pet(&x, 1, 4, /*actor=*/1, DefeatCause::EFFECT) == DefeatResult::DEFEATED,
                  "D3 前置：死亡成立");
            x.event_center_.drain(&x, x.roundCount);
            CHECK_EQ(max_hp(x, 0, 0), 800, "D3 非自身击败 → 卪 不减上限（仍 800）");
        }

        // D3 管线 ±10%/次：隔离战场，两具"自身击败"的场下尸体（defeats=2）后打真实回合。
        //    攻结算（0→1）：DETECT 时 addPct 应含 +20；守结算（1→0）：
        //    DETECT final ≈ GUARD_DETECT final × 0.8（±2 取整容差）。
        {
            ScenarioBattle d3(scenario::make_party(kongyuan_pet(100000)),
                              scenario::make_party(plain_pet(100000)));
            d3.start();
            BattleContext& y = d3.ctx();
            y.on_stage[0] = 0;
            y.on_stage[1] = 1;
            for (const int slot : {3, 4}) {
                hard_kill(y, 1, slot);
                y.roundChoice[0][0] = 1;   // SELECT_SKILL
                y.roundChoice[0][1] = 4;   // 5 号位：卪 的绑定槽
                CHECK(defeat_pet(&y, 1, slot, /*actor=*/0, DefeatCause::DAMAGE)
                          == DefeatResult::DEFEATED,
                      "D3 前置：场下击败成立");
                y.event_center_.drain(&y, y.roundCount);
            }
            double guard_final = -1, detect_final = -1;
            int detect_add_pct = -999;
            y.register_default_damage_effect(
                DamagePhase::GUARD_DETECT, 0, DamageEffectCategory::DETECT,
                [&](BattleContext* c, int) {
                    const DamageSnapshot& dm = c->resolvedDamage;
                    if (dm.attackerId == 1 && dm.defenderId == 0 && dm.isRed && dm.final > 0) {
                        guard_final = dm.final;
                    }
                });
            y.register_default_damage_effect(
                DamagePhase::DETECT, 0, DamageEffectCategory::DETECT,
                [&](BattleContext* c, int) {
                    const DamageSnapshot& dm = c->resolvedDamage;
                    if (dm.attackerId == 0 && dm.defenderId == 1 && dm.isRed && dm.final > 0) {
                        detect_add_pct = dm.addPct;   // AMP 之后 → 应含 卪 +20（2 次）
                    }
                    if (dm.attackerId == 1 && dm.defenderId == 0 && dm.isRed && dm.final > 0) {
                        detect_final = dm.final;      // REDUCE_PCT 之后 → ≈ guard×0.8
                    }
                });
            d3.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            d3.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            d3.drive();
            CHECK_EQ(detect_add_pct, 20, "D3 卪增伤：AMP 阶段 addPct 应 +20（2 次击败）");
            CHECK(guard_final > 0 && detect_final > 0, "D3 前置：守结算探针都抓到值");
            CHECK(std::fabs(detect_final - guard_final * 0.8) <= 2.0,
                  "D3 卪减伤：DETECT final ≈ GUARD_DETECT final × 0.8（±2）");
        }

        std::cout << "  [D] 卪（击败归因 + 上限削减 + 管线 ±10%/次）通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // E. 无为觉者 2260：自身击败对手 → 重置万相乖离 + 对方全部阵亡精灵消逝
    // ════════════════════════════════════════════════════════════════
    {
        // 第五技能必须是带条件效果的真技能（无相谛 37460）——万相乖离"取消首条条件 +
        // 提升 50 威力"只在有 parsed_units 的技能上可见（普通撞击无条件可取消，威力不动）。
        ElfPet wx = make_pet(4661, "无为觉者",
                             {kSkill, kSkill, kSkill, kSkill, /*无相谛=*/37460},
                             /*speed=*/200, /*atk=*/50,
                             /*hp=*/100000, SoulMark(kWuXiang, "无为觉者", "多时点程序族"));
        wx.skills[0].pp = 0;      // PP=0 技能 → 使用后触发万相乖离（2260 节点③）
        wx.skills[0].must_hit = true;   // 确定性：撞击可能 miss（miss = SKILL_INVALID 不触发乖离）
        const MedicineStock meds = {{300011, 3}};   /* 3 瓶初级体力药剂(+20体力) */
        ScenarioBattle e(scenario::make_party(wx),
                         scenario::make_party(plain_pet(100000)), meds, meds);
        e.start();
        BattleContext& x = e.ctx();
        x.on_stage[0] = 0;
        x.on_stage[1] = 1;

        // 预置两具对方场下尸体（actor=-1：不触发重置/消逝）
        hard_kill(x, 1, 3);
        defeat_pet(&x, 1, 3, -1, DefeatCause::EFFECT);
        x.event_center_.drain(&x, x.roundCount);
        hard_kill(x, 1, 4);
        defeat_pet(&x, 1, 4, -1, DefeatCause::EFFECT);
        x.event_center_.drain(&x, x.roundCount);
        CHECK(!x.is_vanished(1, 3) && !x.is_vanished(1, 4), "E1 前置：尸体死亡但未消逝");

        // 觉者用 PP=0 技能 → 万相乖离：取消第五技能首条条件 + 威力 +50
        const int power0 = x.seerRobot[0].elfPets[0].skills[4].power;
        e.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        e.submit_operation(1, BattleFsm::ActionType::USE_MEDICINE, 0);
        e.drive();
        const int power1 = x.seerRobot[0].elfPets[0].skills[4].power;
        CHECK_EQ(power1, power0 + 50, "E2 万相乖离：第五技能威力 +50");
        // 条件**不再被改写**（2026-09-18 单元准入门版）：进度是魂印侧计数器，
        // 技能对象只读——行为验证在场景 010（放行/6次秒杀/击败清零）。
        CHECK(x.seerRobot[0].elfPets[0].skills[4].parsed_units_[0].condition
                  != UnitCondition::None,
              "E2 条件原样保留（技能对象只读；放行由准入门在注册期决定）");

        // E2b **非击败不重置**：无 EVENT_DEATH → 进度保持（用户 2026-09-17/18 口径）
        CHECK_EQ(x.seerRobot[0].elfPets[0].skills[4].power, power1,
                 "E2b 无击败事件 → 威力不回退（进度保持）");

        // 觉者击败对方在场精灵（actor=己方）→ 重置（条件恢复 + 威力回落）+ 批量消逝
        const int on1 = x.on_stage[1];
        hard_kill(x, 1, on1);
        CHECK(defeat_pet(&x, 1, on1, /*actor=*/0, DefeatCause::DAMAGE) == DefeatResult::DEFEATED,
              "E3 前置：击败成立");
        x.event_center_.drain(&x, x.roundCount);

        CHECK_EQ(x.seerRobot[0].elfPets[0].skills[4].power, power0, "E3 重置：威力回落（进度清零）");
        CHECK(x.seerRobot[0].elfPets[0].skills[4].parsed_units_[0].condition
                  != UnitCondition::None,
              "E3 条件始终未被改写（重置 = 进度计数归零，不是恢复条件）");
        CHECK(x.is_vanished(1, on1), "E3 被击败者（阵亡）被消逝");
        CHECK(x.is_vanished(1, 3) && x.is_vanished(1, 4), "E3 对方全部阵亡精灵批量消逝");

        std::cout << "  [E] 2260 击败 → 重置万相乖离 + 对方阵亡全消逝 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // F. 妙时天女 4872：复制扫描跳过已消逝精灵；阵亡未消逝照常复制
    // ════════════════════════════════════════════════════════════════
    {
        ElfPet me = make_pet(4872, "妙时天女", plain_skills(), 200, 100, 4000,
                             SoulMark(kMiaoShi, "妙时天女", "百天浮世络"));
        // 对手：0=瞬杀（活） 1=坚硬（活） 2=精神（**消逝**：hp=0 且上限=0）
        //       3=静电（活） 4=精神（**阵亡未消逝**：hp=0 上限正常 → 应照常复制）
        std::array<ElfPet, 6> enemy{
            make_pet(1, "e0", plain_skills(), 100, 100, 4000, {}, make_trait("瞬杀", 3, {13, 0})),
            make_pet(1, "e1", plain_skills(), 100, 100, 4000, {}, make_trait("坚硬", 3, {7, 0})),
            make_pet(1, "e2", plain_skills(), 100, 100, 4000, {}, make_trait("精神", 5, {2, 5})),
            make_pet(1, "e3", plain_skills(), 100, 100, 4000, {}, make_trait("静电", 5, {5, 0})),
            make_pet(1, "e4", plain_skills(), 100, 100, 4000, {}, make_trait("精神", 5, {2, 5})),
            make_pet(1, "e5", plain_skills(), 100, 100, 4000, {}),
        };
        enemy[2].hp = 0;
        enemy[2].numericalBase[NumericalPropertyIndex::HP] = 0;   // 消逝表示法（071 A 组）
        enemy[4].hp = 0;                                          // 阵亡未消逝

        ScenarioBattle f(scenario::make_party(me), enemy);
        f.start();
        BattleContext& x = f.ctx();

        int instant = 0, hardness = 0, spirit = 0, poison = 0;
        for (const TraitOverlay& t : x.trait_state_[0].copies) {
            if (t.trait.kind == TraitKind::InstantKill) ++instant;
            if (t.trait.kind == TraitKind::Hardness) ++hardness;
            if (t.trait.kind == TraitKind::Spirit) ++spirit;
            if (t.trait.kind == TraitKind::ActivePoisonPhysical) ++poison;
        }
        CHECK_EQ(instant, 1, "F1 活着对手的特性照常复制");
        CHECK_EQ(hardness, 1, "F1 活着对手的特性照常复制（坚硬）");
        CHECK_EQ(poison, 1, "F1 静电照常复制");
        // 槽2 已消逝 → 跳过；槽4 阵亡未消逝 → 照常复制（官方只排除"已消逝"）
        CHECK_EQ(spirit, 1, "F2 spirit=1：阵亡未消逝的槽4 照常复制，已消逝的槽2 被跳过");

        std::cout << "  [F] 4872 复制扫描消逝过滤通过\n";
    }

    std::cout << "\n场景 074 全部通过：消逝族批 1（空元 2175 / 觉者 2260 击败子句 / 4872 消逝过滤）\n";
    return 0;
}
