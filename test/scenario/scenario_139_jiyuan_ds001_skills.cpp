// 场景 139：极渊DS-001 4797 技能组（moves_lib/jiyuan_ds001_skills.cpp，2026-09-27）
//
// 重点锁两条用户口径（2026-09-27）：
//   T1 荡世"自身每处于一种能力提升状态，此技能威力提升20"是**变威力**：1012 吸强
//      （moves.side_effect 列序在 882 之前）→ 882 写威力视图 → 引擎
//      apply_variable_power_recalc 第二次结算。吸强把对手 特攻+1/速度+1 搬过来
//      （2 种提升 → 威力 140）；选这两槽是因为它们不进物理伤害公式（攻 vs 防），
//      伤害比值断言只反映威力 100→140，不被防御视图同步噪声污染。
//   T2 1679"造成的伤害高于400则100%令对手流血"的施加时点在**免死检测之后**：
//      顽强（范围即时型免死，管线内存活钩，比死亡漏斗更早）被荡世致死打穿 →
//      抬回 1 血 → OLD_ROUND_END_1（两条死亡流程后必经、必在漏斗之后的统一时点）
//      才施流血 → 免死存活者带流血。若时点提前（SKILL_EFFECT/AFTER_ACTION 同步施加），
//      hp≤0 的存活门会拒收 → 本断言失败。
// 其余：T3 真死不挂流血（引擎存活门，与游戏"尸体挂标志"的表现差异，插件头注释记档）；
//   T4 阈值未到 → 吸取 1/3；T5 堕煞 2117 流血→诅咒 + 867 消回合成功→下回合先制-3；
//   T6 凋尸 2303 两分支 + 2245 阵亡盾罩；T7 渎神 2322 算力限制→红伤归零但效果照常
//   （1237/43/1814，渎神原文无"命中效果失效"子句）+ 对照不受限；T8 帝丧 962 翻倍 smoke。
//
// 确定性：DS-001 全技能 must_hit（凭证短路，不掷命中）、暴击 0（harness）、
// 顽强 1000‰（trait_proc_roll ≥1000 必发不耗 rand）、流血/臣服 100%（roll_percent ≥100
// 不掷）、异常时长常量 2。唯一 rand 消费者是 2117 的诅咒衍化（回合扣减点，T5 断言
// 已按衍化结果族放宽，不影响判定）。
#include <cmath>
#include <cstdlib>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kDangshi = 38019;   // 极渊·荡世
constexpr int kDuosha = 38018;    // 极渊·堕煞
constexpr int kDushen = 38020;    // 极渊·渎神
constexpr int kDiaoshi = 29107;   // 极渊·凋尸
constexpr int kDisang = 29108;    // 极渊·帝丧
constexpr int kHit = 10004;       // 拍打：物攻 40
constexpr int kBuff = 20265;      // 锋利：属性技能（自身攻击+1，无伤害）

constexpr int kSuanliRestrictedKey = 2414 * 100 + 2;   // soul_lib 算力限制键（注册键 2414 基，2026-09-27 对齐）

constexpr int kBleed = static_cast<int>(AbnormalStatusId::Bleed);          // 16
constexpr int kCurse = static_cast<int>(AbnormalStatusId::Curse);          // 23
constexpr int kSubmission = static_cast<int>(AbnormalStatusId::Submission);// 31

// 算力限制标志现由 2322 直接消费（本场景手工种标志，不挂魂印插件，隔离技能侧行为）。
ElfPet ds001(int atk, int hp, std::array<int, 5> skills) {
    ElfPet p = scenario::make_pet(0, "极渊DS-001", skills, /*speed=*/200, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;      // 凭证短路：必中不掷命中
        s.accuracy = 100;
    }
    return p;
}

ElfPet buff_pet(int hp, int speed = 100) {
    return scenario::make_pet(1, "对手", {kBuff, kBuff, kBuff, kBuff, kBuff}, speed,
                              /*atk=*/100, hp);
}

bool has_curse_family(const BattleContext& ctx, int side) {
    for (const int id : {kCurse,
                         static_cast<int>(AbnormalStatusId::FlameCurse),
                         static_cast<int>(AbnormalStatusId::DeathCurse),
                         static_cast<int>(AbnormalStatusId::WeaknessCurse)}) {
        if (ctx.has_active_abnormal_status(side, id)) {
            return true;
        }
    }
    return false;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== T1 变威力：吸强（1012）→ 每种提升威力+20（882）→ 伤害重算 ==========
    {
        // 对照场：对手无提升 → 吸强失败（无护盾）、威力视图保持 100
        int d_plain = 0;
        {
            ScenarioBattle b(scenario::make_party(ds001(300, 3000, {kDangshi, kHit, kHit, kHit, kHit})),
                             scenario::make_party(buff_pet(4000)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            d_plain = ctx.ws.raw_attack_damage[0];
            CHECK_EQ(ctx.ws.skill_power_view[0].value, 100, "T1 对照：无提升威力视图保持 100");
            CHECK_EQ(ctx.getPet(0).shield_bank_.total(), 0, "T1 对照：吸强失败不附护盾");
        }
        // 提升场：对手 特攻+1/速度+1 → 全部被吸走（2 种 → +40）→ 威力 140 重算
        int d_boost = 0;
        {
            ScenarioBattle b(scenario::make_party(ds001(300, 3000, {kDangshi, kHit, kHit, kHit, kHit})),
                             scenario::make_party(buff_pet(4000)));
            b.start();
            BattleContext& ctx = b.ctx();
            REQUIRE(stat_change(&ctx, 1, /*特攻=*/1, 1) == StatChangeResult::SUCCESS);
            REQUIRE(stat_change(&ctx, 1, /*速度=*/4, 1) == StatChangeResult::SUCCESS);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            d_boost = ctx.ws.raw_attack_damage[0];
            CHECK_EQ(ctx.ws.skill_power_view[0].value, 140, "T1 提升场：威力视图 100+20×2=140（吸到的提升当场计数）");
            CHECK_EQ(ctx.getPet(0).shield_bank_.total(), 300, "T1 提升场：吸强成功附 300 护盾");
            CHECK_EQ(ctx.ability_level(0, 1), 1, "T1 吸强搬入：自身特攻+1");
            CHECK_EQ(ctx.ability_level(0, 4), 1, "T1 吸强搬入：自身速度+1");
            CHECK_EQ(ctx.ability_level(1, 1), 0, "T1 吸强搬出：对手特攻清零");
            CHECK_EQ(ctx.ability_level(1, 4), 0, "T1 吸强搬出：对手速度清零");
            // 变威力重算：伤害按 140 威力重出（两槽不进物理公式 → 比值即威力比；伤害含
            // 随机浮动 → 比值容差放宽到 ±25%，重算本身由威力视图 140 + d_boost>d_plain 锁）
            std::cout << "  [T1] d_plain=" << d_plain << " d_boost=" << d_boost
                      << " ratio=" << (d_plain > 0 ? static_cast<double>(d_boost) / d_plain : 0.0) << "\n";
            CHECK(d_boost > d_plain, "T1 伤害重算：威力 140 > 100 → 提升场伤害更高");
            CHECK(std::fabs(static_cast<double>(d_boost) / d_plain - 1.4) <= 0.25 * 1.4,
                  "T1 伤害比值在 1.4±25%（浮动带宽内）");
            std::cout << "  [T1] 变威力重算 通过（对照 " << d_plain << " vs 提升 " << d_boost << "）\n";
        }
    }

    //========== T2 流血在免死检测之后：顽强免死存活 → OLD_ROUND_END_1 施流血 ==========
    {
        // 顽强 5 星 {1000,1}：致死必发、残留 1 点（范围即时型，管线内存活钩）
        ElfPet foe = buff_pet(/*hp=*/500);
        foe.commonTrait = scenario::make_trait("顽强", 5, {1000, 1});
        ScenarioBattle b(scenario::make_party(ds001(2000, 3000, {kDangshi, kHit, kHit, kHit, kHit})),
                         scenario::make_party(foe));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const int dealt = ctx.ws.raw_attack_damage[0];
        CHECK(dealt > 400, "T2 前提：荡世伤害高于 400 阈值");
        CHECK_EQ(ctx.getPet(1).hp, 1, "T2 顽强免死：致死打穿后残留 1 点（免死检测已跑）");
        CHECK(ctx.has_active_abnormal_status(1, kBleed),
              "T2 核心：流血落在免死检测之后——免死存活者带流血"
              "（若提前施加，hp≤0 存活门会拒收，本断言即失败）");
        std::cout << "  [T2] 流血时点在免死检测之后 通过（伤害 " << dealt << "）\n";
    }

    //========== T3 真死：不挂流血（存活门，偏差记档）、不吸取 ==========
    {
        ScenarioBattle b(scenario::make_party(ds001(2000, 3000, {kDangshi, kHit, kHit, kHit, kHit})),
                         scenario::make_party(buff_pet(/*hp=*/500)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();   // 致死 → FIRST_MOVER_DEATH 漏斗 → 跳 OLD_ROUND_END_1（流血延迟器在此跑、被拒收）
        CHECK(ctx.getPet(1).hp <= 0, "T3 无免死对手被击杀");
        CHECK(!ctx.has_active_abnormal_status(1, kBleed),
              "T3 真死不挂流血：引擎存活门拒收（游戏尸体会挂标志，表现差异记档）");
        std::cout << "  [T3] 真死分支 通过\n";
    }

    //========== T4 阈值未到：吸取对手最大体力 1/3 ==========
    {
        ScenarioBattle b(scenario::make_party(ds001(/*atk=*/50, 3000, {kDangshi, kHit, kHit, kHit, kHit})),
                         scenario::make_party(buff_pet(/*hp=*/2000)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 2000;   // 预留受伤空间，让吸取回血可见
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const int dealt = ctx.ws.raw_attack_damage[0];
        CHECK(dealt <= 400, "T4 前提：伤害未过阈值（atk=50）");
        CHECK_EQ(ctx.getPet(1).hp, 2000 - dealt - 2000 / 3,
                 "T4 未触发→吸取：目标掉 红伤+最大体力1/3");
        CHECK_EQ(ctx.getPet(0).hp, 2000 + 2000 / 3, "T4 吸取回血：自身恢复等量（clamp 前）");
        CHECK(!ctx.has_active_abnormal_status(1, kBleed), "T4 未过阈值不流血");
        std::cout << "  [T4] 阈值未到吸取分支 通过\n";
    }

    //========== T5a 堕煞 2117：流血→诅咒 ==========
    {
        ScenarioBattle b(scenario::make_party(ds001(300, 3000, {kDuosha, kHit, kHit, kHit, kHit})),
                         scenario::make_party(buff_pet(2000)));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(apply_anomaly(&ctx, 1, kBleed, 2, 0) == ApplyAnomalyResult::SUCCESS);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(!ctx.has_active_abnormal_status(1, kBleed), "T5a 转化后流血消失");
        CHECK(has_curse_family(ctx, 1),
              "T5a 流血→诅咒（衍化族断言：诅咒在回合扣减点可能衍化为烈焰/致命/虚弱）");
        std::cout << "  [T5a] 堕煞 2117 流血→诅咒 通过\n";
    }

    //========== T5b 堕煞 867：消回合类成功 → 对手下回合先制-3 ==========
    {
        ScenarioBattle b(scenario::make_party(ds001(300, 3000, {kDuosha, kHit, kHit, kHit, kHit})),
                         scenario::make_party(buff_pet(2000)));
        b.start();
        BattleContext& ctx = b.ctx();
        // 对手挂回合类效果（弹控票即回合类；1808 同款 has_round_type 判定面）
        ctx.rule_center_.grant_reflect(1, /*source_slot=*/-1, /*source_effect_id=*/999,
                                       /*target=*/1, /*rounds=*/3);
        CHECK(ctx.rule_center_.has_round_type(1), "T5b 前提：对手处于回合类效果");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(!ctx.rule_center_.has_round_type(1), "T5b 消除成功：对手回合类效果被清");
        // R2：MOVE_RIGHT 时点窗口 tick 写 preemptive_level（夹在两次 submit 之间 step）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.step_past(State::BATTLE_FIRST_MOVE_RIGHT);
        ctx.debug_step_mode = false;
        CHECK_EQ(ctx.ws.preemptive_level[1], -3, "T5b 下回合对手先制-3（MOVE_RIGHT 窗口 tick）");
        b.drive();
        std::cout << "  [T5b] 堕煞 867 消回合→先制-3 通过\n";
    }

    //========== T6a 凋尸 2303 满体力分支：消耗 1/2 + 令对手流血 ==========
    {
        ScenarioBattle b(scenario::make_party(ds001(300, 3000, {kDiaoshi, kHit, kHit, kHit, kHit})),
                         scenario::make_party(buff_pet(2000)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(0).hp, 1500, "T6a 满体力：消耗自身最大体力 1/2");
        CHECK(ctx.has_active_abnormal_status(1, kBleed), "T6a 满体力：令对手流血");
        CHECK(ctx.rule_center_.has_round_type(0), "T6a 191 免疫反弹（回合类）在身");
        std::cout << "  [T6a] 凋尸满体力分支 通过\n";
    }

    //========== T6b 凋尸 2245：每阵亡队友 +125 盾 +125 罩 ==========
    {
        ScenarioBattle b(scenario::make_party(ds001(300, 3000, {kDiaoshi, kHit, kHit, kHit, kHit})),
                         scenario::make_party(buff_pet(2000)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[1].hp = 0;   // 板凳阵亡一名（funnel 只扫在场，安全）
        ctx.seerRobot[0].elfPets[2].hp = 0;   // 两名 → 250/250
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(0).shield_bank_.total(), 250, "T6b 两名阵亡 → 250 护盾");
        CHECK_EQ(ctx.getPet(0).hood_bank_.total(), 250, "T6b 两名阵亡 → 250 护罩");
        std::cout << "  [T6b] 凋尸 2245 阵亡盾罩 通过\n";
    }

    //========== T6c 凋尸 2303 非满体力分支：吸取 1/3 + 对手 PP-2 ==========
    {
        ScenarioBattle b(scenario::make_party(ds001(300, 3000, {kDiaoshi, kHit, kHit, kHit, kHit})),
                         scenario::make_party(buff_pet(2000)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 1500;   // 非满体力
        const int foe_pp_before = ctx.getPet(1).skills[1].pp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(1).hp, 2000 - 2000 / 3, "T6c 非满体力：吸取对手最大体力 1/3（属性技无红伤）");
        CHECK_EQ(ctx.getPet(0).hp, 1500 + 2000 / 3, "T6c 吸取回血");
        CHECK_EQ(ctx.getPet(1).skills[1].pp, foe_pp_before - 2,
                 "T6c 对手未选用槽 PP-2（所有技能 PP 值-2）");
        std::cout << "  [T6c] 凋尸非满体力分支 通过\n";
    }

    //========== T7a 渎神 2322：算力限制 → 红伤归零，但解异常/1237/43/1814 照常 ==========
    {
        ScenarioBattle b(scenario::make_party(ds001(300, 3000, {kDushen, kHit, kHit, kHit, kHit})),
                         scenario::make_party(buff_pet(2000)));
        b.start();
        BattleContext& ctx = b.ctx();
        // 手工种算力限制（技能侧只读该标志；不挂魂印插件以隔离被测面）。
        // 跨 dylib 的 std::any 惯例只走 int（kongyuan_runtime.h），种 1 而非 bool。
        ctx.getPet(0).on_stage_storage[kSuanliRestrictedKey] = 1;
        REQUIRE(apply_anomaly(&ctx, 0, kBleed, 2, 1) == ApplyAnomalyResult::SUCCESS);
        ctx.getPet(0).hp = 2200;   // 留出 43 满回复的可见空间
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(!ctx.has_active_abnormal_status(0, kBleed), "T7a 使用时解除自身异常（无条件半支）");
        // 白板标记的归零点在伤害管线之后 → 裸伤台账仍记公式值（与"台账=公式值、
        // 挡伤改不了"的既有口径一致），可观察口径是目标不掉血。
        CHECK_EQ(ctx.getPet(1).hp, 2000, "T7a 算力限制：目标不掉血（红伤归零）");
        CHECK_EQ(ctx.getPet(0).hp, 3000, "T7a 43 恢复 1/1 照常（效果不失效口径）");
        CHECK_EQ(ctx.getPet(1).skills[1].pp, 0, "T7a 1237 双方 PP 归零照常");
        CHECK(ctx.has_active_abnormal_status(1, kSubmission),
              "T7a 1814 窗口首回合照常：100% 令对手臣服");
        std::cout << "  [T7a] 渎神算力限制分支 通过\n";
    }

    //========== T7b 渎神对照：无算力限制 → 红伤照常 ==========
    {
        ScenarioBattle b(scenario::make_party(ds001(300, 3000, {kDushen, kHit, kHit, kHit, kHit})),
                         scenario::make_party(buff_pet(2000)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(ctx.ws.raw_attack_damage[0] > 0, "T7b 无限制：300 威力红伤照常");
        CHECK(ctx.getPet(1).hp < 2000, "T7b 目标掉血");
        std::cout << "  [T7b] 渎神无限制对照 通过\n";
    }

    //========== T8 帝丧 962：无提升时翻倍 smoke ==========
    {
        ScenarioBattle b(scenario::make_party(ds001(300, 3000, {kDisang, kHit, kHit, kHit, kHit})),
                         scenario::make_party(buff_pet(2000)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            CHECK_EQ(ctx.ability_level(0, i), 2, "T8 962 全属性+1 且无提升翻倍 → 各槽 +2");
        }
        std::cout << "  [T8] 帝丧 962 smoke 通过\n";
    }

    std::cout << "scenario_139 all passed" << std::endl;
    return 0;
}
