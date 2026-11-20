#include <utils/sbs_rand.h>
// 场景 7070 —— K362 暴击免疫反弹+瘫痪分支族（786，soul_lib/k362_crit_immune_reflect_paralysis.cpp）
//（2026-10-24，worktree feat/k362；号段 7070-7079）
//
// 断言口径（786「免疫并反弹致命一击伤害，触发后自身恢复N点体力且下回合自身先制+M；
// 能力提升状态被消除则50%令对手瘫痪，未触发瘫痪则消除对手回合类效果」）：
//   T7070.a 必暴打持印者 → 免疫（持印者不掉血）+ 恰好回 N 体力 + 对手挨等额反弹（>0）。
//   T7070.b 非暴击门闭 → 正常受伤、不回血、不反弹（门是"致命一击"）。
//   T7070.c 无魂印对照 → 必暴全额落地（免疫确来自本魂印）。
//   T7070.d 下回合先制+M：R1 触发（对手速快先手）→ R2 持印者反抢先手（preemptive_right）。
//   T7070.e row582 args[600,3,...]：回血 600（行参驱动数值）。
//   T7070.f 分支门确定性对照：被**吸取**（kind=1）不触发；无提升可清（不发事件）不触发
//           ——两种情形对手票都保留、不瘫痪。
//   T7070.g 50% 分支统计带：40 回合独立对局（每局重种票+重给提升+消强），
//           票被断数 ∈ [8,32]；且"票保留 ⇒ 对手处于瘫痪(19)"（分支互斥完备）。
//
// ⚠️ 场景构造口径：SoulMark args 按 **DB 原样** {500,2,50,19,1}/{600,3,50,19,1}
//   （不补 {0,0} 前缀；实现侧 db_at 原序读，K257 起夜跑口径）。
// ⚠️ 暴击制造：set_crit_sixteenths(foe, 16)=必暴；持印者出 NONE/拍打，不动对手 HP 面。
// ⚠️ 回合类票种植：seal_skill 直调（3090 惯用法，回合型 duration>0——break_round_effects
//   只清回合类）；断票观测面 = ctx.rule_center_.empty()。
#include <cstdlib>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <plugin/core_api.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaDa = 10004;   // 拍打：物攻 40（填充）
constexpr int kMaxHp = 99999;
constexpr int kCrippled = static_cast<int>(AbnormalStatusId::Crippled);   // 瘫痪=19

SoulMark make_mark(int heal, int prio) {
    // DB args 原序：[N回血, M先制, 概率, 瘫痪码, boss旗]
    SoulMark sm(786, "K362 兰斯洛特", "test",
                EffectArgs(std::vector<int>{heal, prio, 50, kCrippled, 1}));
    sm.monster_id = 3134;
    return sm;
}

// 持印者：速度 100（慢，让对手先手打暴击）。
ElfPet mark_pet(int heal = 500, int prio = 2, bool with_mark = true) {
    ElfPet p = with_mark
        ? make_pet(1, "兰斯洛特", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                   /*speed=*/100, /*atk=*/10, kMaxHp, make_mark(heal, prio))
        : make_pet(1, "裸兰斯洛特", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                   /*speed=*/100, /*atk=*/10, kMaxHp);
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
    return p;
}

// 对手：速度快（先手），必暴制造致命一击。
ElfPet crit_foe(int atk = 300) {
    ElfPet p = make_pet(2, "暴击手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/200, atk, kMaxHp);
    set_crit_sixteenths(p, 16);
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
    return p;
}

}  // namespace

int main() {
    sbs_srand(20261024);
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7070.a 必暴 → 免疫 + 回 500 + 反弹 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(crit_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = 1000;   // 压低留回血空间
        const int h0 = ctx.getPet(0).hp;
        const int f0 = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(ctx.crit_happened[1], "T7070.a 前置：对手本次确实暴击");
        CHECK_EQ(ctx.getPet(0).hp, h0 + 500,
                 "T7070.a 免疫（不掉血）+ 触发回 500 体力（实测 "
                     + std::to_string(ctx.getPet(0).hp - h0) + "）");
        CHECK(ctx.getPet(1).hp < f0, "T7070.a 攻击者挨等额反弹（FIXED 粉伤）");
        std::cout << "  [T7070.a] 免疫+回血+反弹 通过（对手损 "
                  << f0 - ctx.getPet(1).hp << "）" << std::endl;
    }

    // ══ T7070.b 非暴击门闭 ══
    {
        ElfPet foe = crit_foe();
        set_crit_sixteenths(foe, 0);   // 永不必暴
        ScenarioBattle b(make_party(mark_pet()), make_party(foe));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = 1000;
        const int h0 = ctx.getPet(0).hp;
        const int f0 = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(!ctx.crit_happened[1], "T7070.b 前置：本次非暴击");
        CHECK(ctx.getPet(0).hp < h0, "T7070.b 非暴击正常受伤（免疫门闭）");
        CHECK(ctx.getPet(0).hp != h0 + 500, "T7070.b 不回血（触发面未发生）");
        CHECK_EQ(ctx.getPet(1).hp, f0, "T7070.b 不反弹");
        std::cout << "  [T7070.b] 非暴击门闭 通过" << std::endl;
    }

    // ══ T7070.c 无魂印对照 ══
    {
        ScenarioBattle b(make_party(mark_pet(500, 2, /*with_mark=*/false)),
                         make_party(crit_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = 1000;
        const int h0 = ctx.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(ctx.crit_happened[1], "T7070.c 前置：对手暴击");
        CHECK(ctx.getPet(0).hp < h0, "T7070.c 无魂印：暴击全额落地（免疫确来自本魂印）");
        std::cout << "  [T7070.c] 无魂印对照 通过" << std::endl;
    }

    // ══ T7070.d 下回合先制+2：R1 对手先手触发 → R2 持印者反抢 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(crit_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(ctx.crit_happened[1], "T7070.d 前置：R1 对手暴击触发免疫");
        CHECK_EQ(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_2, true,
                 "T7070.d R1 对照：速度快者先手（无先制加成）");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_1, true,
                 "T7070.d R2：触发后下回合先制+2 → 慢者反抢先手");
        std::cout << "  [T7070.d] 下回合先制+2 通过" << std::endl;
    }

    // ══ T7070.e row582 args[600,3,...]：回血 600（行参驱动）══
    {
        ScenarioBattle b(make_party(mark_pet(/*heal=*/600, /*prio=*/3)),
                         make_party(crit_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = 1000;
        const int h0 = ctx.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(ctx.crit_happened[1], "T7070.e 前置：对手暴击");
        CHECK_EQ(ctx.getPet(0).hp, h0 + 600,
                 "T7070.e 进化行 db[0]=600：回血量随行参（实测 "
                     + std::to_string(ctx.getPet(0).hp - h0) + "）");
        std::cout << "  [T7070.e] row582 回血 600 通过" << std::endl;
    }

    // ══ T7070.f 分支门确定性对照（不触发路径）══
    {
        // f1：提升被**吸取**（transfer_stat_boosts → stat_loss_kind=1）→ 不触发瘫痪/断票
        {
            ScenarioBattle b(make_party(mark_pet()), make_party(crit_foe(/*atk=*/1)));
            b.start();
            BattleContext& ctx = b.ctx();
            // 票 source=1（对手的回合类效果）——break_round_effects(对手) 清的就是它
            //（3090 口径：断回合按**来源方**清回合类；source=0 的票 break(1) 清不掉）。
            seal_skill(&ctx, /*source=*/1, /*target=*/0, /*effect_id=*/70991,
                       /*attribute=*/false, /*attack=*/true, /*count=*/0,
                       /*duration_rounds=*/2, /*penetrable=*/false);
            ctx.ability_levels[0][static_cast<int>(NumericalPropertyIndex::PHYSICAL_ATTACK)] = 2;
            REQUIRE(transfer_stat_boosts(&ctx, /*from=*/0, /*to=*/1) > 0);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK_EQ(ctx.rule_center_.empty(), false,
                     "T7070.f1 被吸取（kind=1）不算被消除 → 不消除对手回合类");
            CHECK(!core_api().has_active_anomaly(&ctx, 1, kCrippled),
                  "T7070.f1 被吸取 → 不瘫痪对手");
        }
        // f2：无提升可清（clear=0 不发事件）→ 不触发
        {
            ScenarioBattle b(make_party(mark_pet()), make_party(crit_foe(/*atk=*/1)));
            b.start();
            BattleContext& ctx = b.ctx();
            seal_skill(&ctx, 1, 0, 70991, false, true, 0, 2, false);   // 对手来源的回合类票
            REQUIRE(clear_stat_boosts(&ctx, 0) == 0);   // 本就无提升
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK_EQ(ctx.rule_center_.empty(), false,
                     "T7070.f2 无提升被消除（无事件）→ 对手票保留");
            CHECK(!core_api().has_active_anomaly(&ctx, 1, kCrippled),
                  "T7070.f2 无事件 → 不瘫痪对手");
        }
        std::cout << "  [T7070.f] 分支门确定性对照 通过" << std::endl;
    }

    // ══ T7070.g 50% 分支统计带（40 局独立对局）══
    {
        int cleared = 0, combo_a = 0, combo_b = 0, combo_c = 0, combo_d = 0;
        for (int i = 0; i < 40; ++i) {
            ScenarioBattle b(make_party(mark_pet()), make_party(crit_foe(/*atk=*/1)));
            b.start();
            BattleContext& ctx = b.ctx();
            seal_skill(&ctx, 1, 0, 70991, false, true, 0, 2, false);   // 对手来源的回合类票
            ctx.ability_levels[0][static_cast<int>(NumericalPropertyIndex::PHYSICAL_ATTACK)] = 2;
            REQUIRE(clear_stat_boosts(&ctx, 0) > 0);   // 触发 EVENT_STAT_BOOSTS_LOST
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            const bool paralyzed = core_api().has_active_anomaly(&ctx, 1, kCrippled);
            const bool ticket_gone = ctx.rule_center_.empty();
            if (ticket_gone && !paralyzed) ++combo_a;
            if (!ticket_gone && paralyzed) ++combo_b;
            if (ticket_gone && paralyzed) ++combo_c;
            if (!ticket_gone && !paralyzed) ++combo_d;
            if (ticket_gone) {
                ++cleared;
            }
        }
        std::cout << "  [T7070.g] 40 局断票数=" << cleared << "/40（期望 ~20）；组合 断票-only="
                  << combo_a << " 瘫痪-only=" << combo_b << " 双真=" << combo_c
                  << " 双假=" << combo_d << std::endl;
        CHECK_EQ(combo_c, 0, "T7070.g 无双真（瘫痪+断票并现）");
        CHECK_EQ(combo_d, 0, "T7070.g 无双假（分支皆未走）");
        CHECK(cleared >= 8 && cleared <= 32,
              "T7070.g 50% 档统计带 [8,32]（实测 " + std::to_string(cleared) + "/40）");
    }

    std::cout << "scenario_7070 PASS (786 暴击免疫反弹+瘫痪分支 2 行)" << std::endl;
    return 0;
}
