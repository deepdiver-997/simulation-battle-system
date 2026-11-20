// 场景 103 —— 空元之铭①~④（仐 / 叵 / 乂 / 卌）端到端
//
// 官方 effect_icon 1791（pet 4586）tips（与《官方名词解释》reference/scrape/out/all_posts.jsonl
// idx=127 逐字同文）：
//   「空元之铭：空元行者的技能被刻入了铭文，1~5号位的技能额外获得以下效果；
//     仐：先制+1并无视对手的能力提升状态、自身的能力下降状态；
//     叵：不受PP值限制且命中时吸取对手所有技能1点PP值；
//     乂：回合开始时解除自身所处的异常状态；
//     卌：使用后吸取对手最大体力的40%且减少对手下次攻击40%的伤害；
//     卪：击败对手后减少自身体力上限的20%…」      ← 卪 已由场景 074 锁定
//
// 本场景锁**绑定语义 + 四条各自的时点行为**：
//   · 绑定 = **面板槽位**（下标 0..4 即"1~5号位"）：门读 `roundChoice[owner][1]`。
//     故全部用例都用**同一把技能**（10162 飞空俯冲，必中、无 side_effect）铺满 5 个槽位——
//     行为差异只能来自**槽位**。这正是"按槽绑定、不认技能名"的判据：哪天有人把门改成
//     按技能名/按"是不是第五技能"，这些用例会当场红。
//   · 作用域 **STAGE**：铭文绑的是**她的**技能，人不在场就没有"她的技能"→ D4 锁"下场即撤"。
//
// 组：
//   A 仐：A1 用 1 号位 → 先制+1（对手速度更高也先手）；A1' 用 3 号位 → 后手（槽位门）；
//        A2 能力视图钳制 + **成对还原**（本体不动）
//   B 叵：B1 2 号位 PP=0 仍可选、1 号位 PP=0 被拒（证明是**按技能**修饰，不是按方旗标）；
//        B2 命中吸取对手**所有技能** 1 点 PP（同槽位回 1）
//   C 乂：C1 用 3 号位 → 回合开始解自身异常；C2 用 2 号位 → 异常保留（槽位门）
//   D 卌：D1 吸取对手最大体力的 40% 并按**实际减少量**回血；D2 下回合 ROUND_START 重写减伤槽；
//        D3 对手本次攻击伤害 ≈ 未减伤时的 60%（同局内比，规避 rand 浮动）+ pending 被消费；
//        D4 下场即撤：换下再换回，减伤**不复活**（官方「之后」的默认结束点）

#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kHit = 10162;     // 飞空俯冲（普通系物攻 90、必中、无 side_effect）
constexpr int kKongYuan = 2175;

constexpr int kCurse = static_cast<int>(AbnormalStatusId::Curse);
constexpr int kAtkStat = static_cast<int>(NumericalPropertyIndex::PHYSICAL_ATTACK);
constexpr int kDefStat = static_cast<int>(NumericalPropertyIndex::DEFENSE);

std::array<int, 5> plain_skills() {
    return {kHit, kHit, kHit, kHit, kHit};
}

// 空元行者：hp 与上限分开给（D1 要观测"按实际减少量回血"）
ElfPet kongyuan(int speed, int hp, int max_hp) {
    ElfPet p = make_pet(4586, "空元行者", plain_skills(), speed, /*atk=*/100, hp,
                        SoulMark(kKongYuan, "空元行者", "消逝族"));
    p.numericalBase[NumericalPropertyIndex::HP] = max_hp;
    return p;
}

ElfPet foe_pet(int speed, int hp, int max_hp = 0) {
    ElfPet p = make_pet(1, "守方", plain_skills(), speed, /*atk=*/100, hp);
    if (max_hp > 0) {
        p.numericalBase[NumericalPropertyIndex::HP] = max_hp;
    }
    return p;
}

// 己方队伍：slot0 = 空元行者（面板 1 号位），其余是普通队友（供 D4 换人）。
std::array<ElfPet, 6> kong_party(int speed = 100, int hp = 5000, int max_hp = 9000) {
    std::array<ElfPet, 6> party = make_party(foe_pet(100, 4000));
    party[0] = kongyuan(speed, hp, max_hp);
    return party;
}

// 双方各出一手（同一把技能，差异只在**槽位**）。
void play_round(ScenarioBattle& b, int kong_slot, int foe_slot = 0) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, kong_slot);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_slot);
    b.drive();
}

// 从"收下第二个操作"起单步推进到指定时点**之后**（debug_step_mode 的标准用法）
void play_round_step(ScenarioBattle& b, int kong_slot, int foe_slot, State target) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, kong_slot);
    b.ctx().debug_step_mode = true;
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_slot);
    b.step_past(target);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ════════════════════════════════════════════════════════════════
    // A 仐（1号位）：先制+1 并无视对手能力提升 / 自身能力下降
    // ════════════════════════════════════════════════════════════════
    {
        // A1 对手速度 200 > 空元 100，双方技能先制皆 0 → 只有 仐 的 +1 能改判
        ScenarioBattle b(kong_party(/*speed=*/100), make_party(foe_pet(/*speed=*/200, 4000)));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round_step(b, /*kong_slot=*/0, /*foe_slot=*/0, State::BATTLE_FIRST_MOVE_RIGHT);
        CHECK_EQ(ctx.ws.preemptive_level[0], 1, "A1) 仐：1 号位技能先制 +1");
        CHECK_EQ(ctx.ws.preemptive_level[1], 0, "A1) 对手无先制加成");
        CHECK(ctx.ws.preemptive_right == PreemptiveRight::SEER_ROBOT_1,
              "A1) ★ 速度劣势下仍先手（先制+1 真的参与了先手权比较）");
        b.drive();
        std::cout << "  [A1] 仐·先制+1：速度 100 vs 200 仍先手 通过\n";
    }
    {
        // A1' 同一把技能、同一速度差，改用 3 号位 → 没有 +1 → 后手
        ScenarioBattle b(kong_party(/*speed=*/100), make_party(foe_pet(/*speed=*/200, 4000)));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round_step(b, /*kong_slot=*/2, /*foe_slot=*/0, State::BATTLE_FIRST_MOVE_RIGHT);
        CHECK_EQ(ctx.ws.preemptive_level[0], 0, "A1') 非 1 号位：无先制加成");
        CHECK(ctx.ws.preemptive_right == PreemptiveRight::SEER_ROBOT_2,
              "A1') ★ 槽位门：速度慢者后手");
        b.drive();
        std::cout << "  [A1'] 槽位门（3 号位不加先制）通过\n";
    }
    {
        // A2 对手物防 +2、自身物攻 -2 → 伤害视图钳到 0；回合走完后**成对还原**
        ScenarioBattle b(kong_party(/*speed=*/100), make_party(foe_pet(/*speed=*/50, 4000)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][kDefStat] = 2;     // 对手能力提升
        ctx.ws.view_levels[1][kDefStat] = 2;
        ctx.ability_levels[0][kAtkStat] = -2;    // 自身能力下降
        ctx.ws.view_levels[0][kAtkStat] = -2;

        play_round_step(b, /*kong_slot=*/0, /*foe_slot=*/0, State::BATTLE_FIRST_SKILL_EFFECT);
        // 此刻 SKILL_EFFECT 已跑（钳制生效）、AFTER_ACTION 未跑（还原未发生）
        CHECK_EQ(ctx.ws.view_levels[1][kDefStat], 0,
                 "A2) ★ 无视对手能力提升：伤害视图钳到 0");
        CHECK_EQ(ctx.ws.view_levels[0][kAtkStat], 0,
                 "A2) ★ 无视自身能力下降：伤害视图钳到 0");
        CHECK_EQ(ctx.ability_levels[1][kDefStat], 2,
                 "A2) 本体**不动**（官方 idx=127「能力提升效果失效」：能力等级不再参与能力值"
                 "计算，但正常参与'处于能力提升'等结算）");
        b.drive();
        CHECK_EQ(ctx.ws.view_levels[1][kDefStat], 2, "A2) ★ 成对还原：对手防御视图回 2");
        CHECK_EQ(ctx.ws.view_levels[0][kAtkStat], -2, "A2) ★ 成对还原：自身物攻视图回 -2");
        std::cout << "  [A2] 仐·能力视图钳制 + 成对还原 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // B 叵（2号位）：不受PP值限制 + 命中吸取对手所有技能1点PP
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(kong_party(), make_party(foe_pet(100, 4000)));
        b.start();
        BattleContext& ctx = b.ctx();

        // B1：2 号位（叵）PP=0 仍可选；1 号位 PP=0 被拒
        // → 证明这是**按技能**的修饰（`Skills::usabilityEffects`），而不是
        //   `ctx->ignore_pp` 那种"整方全技能放行"的旗标。
        ctx.getPet(0).skills[1].pp = 0;
        ctx.getPet(0).skills[0].pp = 0;
        CHECK(ctx.getPet(0).skills[1].query_selectable(&ctx, 0)
                  == SkillSelectionResult::SELECTABLE,
              "B1) ★ 2 号位 PP=0 仍可选（叵）");
        CHECK(ctx.getPet(0).skills[0].query_selectable(&ctx, 0)
                  == SkillSelectionResult::PP_EMPTY,
              "B1) 1 号位 PP=0 被拒（不是整方 ignore_pp）");
        CHECK(ctx.getPet(0).skills[1].skill_usable(&ctx, 0), "B1) skill_usable 同判据");
        CHECK(!ctx.ignore_pp[0], "B1) 佐证：`ctx->ignore_pp` 全程未被置位");

        // B2：命中时吸取对手**所有技能** 1 点 PP（★ 2026-09-23 纠错：**只扣不给**——
        // 官方名词解释「叵：不受PP值限制且命中时吸取对手所有技能1点PP值」，无"自身同槽位
        // +1"的半边；语料带受益方的吸取必写去向，如孤烛灯使「…恢复至己方上只精灵」）。
        // 自身 PP 压到 5 供"吸取不带自我回复"的对照；对手压到满值供"所有技能都 -1"。
        for (Skills& s : ctx.getPet(0).skills) {
            s.pp = 5;
        }
        for (Skills& s : ctx.getPet(1).skills) {
            s.pp = s.maxPP;
        }
        const int foe_pp_before = ctx.getPet(1).skills[0].pp;
        play_round(b, /*kong_slot=*/1);   // 2 号位 = 叵
        // slot0 是守方本回合**自己用掉**的那一槽：额外 -1（用技能本身扣 1 点 PP）
        CHECK_EQ(ctx.getPet(1).skills[0].pp, foe_pp_before - 2,
                 "B2) ★ 对手被用的那一槽 -1（吸取）+ -1（使用）");
        for (int i = 1; i < 5; ++i) {
            CHECK_EQ(ctx.getPet(1).skills[i].pp, foe_pp_before - 1,
                     "B2) ★ 对手没用的槽位也 -1 PP（'所有技能'）");
        }
        // 自身：**原封不动**——2 号位只吃用技的 -1，其余槽位不回任何 PP
        CHECK_EQ(ctx.getPet(0).skills[1].pp, 4,
                 "B2) ★ 自身 2 号位：-1（使用），吸取无自我回复半边");
        for (int i = 0; i < 5; ++i) {
            if (i == 1) {
                continue;
            }
            CHECK_EQ(ctx.getPet(0).skills[i].pp, 5,
                     "B2) ★ 自身其余槽位不动（'只扣不给'）");
        }
        std::cout << "  [B] 叵·按技能 PP 放行 + 命中吸取（只扣不给） 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // C 乂（3号位）：回合开始时解除自身所处的异常状态
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(kong_party(), make_party(foe_pet(100, 4000)));
        b.start();
        BattleContext& ctx = b.ctx();
        // 选择期之前先挂一个异常 → 回合开始时应被解掉（异常槽归 0）
        CHECK_EQ(static_cast<int>(apply_anomaly(&ctx, 0, kCurse, 3, 1)),
                 static_cast<int>(ApplyAnomalyResult::SUCCESS), "C1) 前置：自身被附加诅咒");
        CHECK(ctx.abnormal_status_end_round[0][kCurse] != 0, "C1) 前置：异常确实在身");
        play_round(b, /*kong_slot=*/2);   // 3 号位 = 乂
        CHECK_EQ(ctx.abnormal_status_end_round[0][kCurse], 0,
                 "C1) ★ 回合开始时解除自身所处的异常状态");
        std::cout << "  [C1] 乂·回合开始解异常 通过\n";
    }
    {
        ScenarioBattle b(kong_party(), make_party(foe_pet(100, 4000)));
        b.start();
        BattleContext& ctx = b.ctx();
        apply_anomaly(&ctx, 0, kCurse, 3, 1);
        play_round(b, /*kong_slot=*/1);   // 2 号位 = 叵（不是乂）
        CHECK(ctx.abnormal_status_end_round[0][kCurse] != 0,
              "C2) ★ 槽位门：非 3 号位不解异常");
        std::cout << "  [C2] 槽位门（2 号位不解异常）通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // D 卌（4号位）：吸取对手最大体力的40% + 减少对手下次攻击40%的伤害
    // ════════════════════════════════════════════════════════════════
    {
        // 对手速度更高 → 每回合先手砍一刀。第 1 回合那一刀（d1）无减伤，
        // 第 2 回合那一刀（d2）吃到 -40% → 同局内比 d2/d1 ≈ 0.6。
        ScenarioBattle b(kong_party(/*speed=*/100, /*hp=*/5000, /*max_hp=*/9000),
                         make_party(foe_pet(/*speed=*/200, /*hp=*/1000, /*max_hp=*/1000)));
        b.start();
        BattleContext& ctx = b.ctx();

        // 第 1 回合：先走到**对手（先手方）行动后** —— 空元已挨刀（d1），还没出手
        play_round_step(b, /*kong_slot=*/3, /*foe_slot=*/0, State::BATTLE_FIRST_AFTER_ACTION);
        const int d1 = 5000 - ctx.getPet(0).hp;
        CHECK(d1 > 0, "D1) 前置：对手先手攻击已造成伤害");

        // 再走到"空元打完伤害、**尚未**结算'使用后'"（后手方的 ATTACK_DAMAGE 之后）
        b.step_past(State::BATTLE_SECOND_ATTACK_DAMAGE);
        const int foe_hp_before_drain = ctx.getPet(1).hp;
        const int kong_hp_before_drain = ctx.getPet(0).hp;
        b.step_past(State::BATTLE_SECOND_AFTER_ACTION);   // ★ 卌 的"使用后"时点
        CHECK_EQ(foe_hp_before_drain - ctx.getPet(1).hp, 400,
                 "D1) ★ 吸取对手**最大体力**的 40%（1000 → 减 400）");
        CHECK_EQ(ctx.getPet(0).hp - kong_hp_before_drain, 400,
                 "D1) ★ 按**实际减少量**回血（自身 +400）");
        b.drive();   // 跑完第 1 回合

        // 第 2 回合：先确认减伤槽已由 ROUND_START 重写（ws 每回合 reset → 必须重写）
        play_round_step(b, /*kong_slot=*/1, /*foe_slot=*/0, State::BATTLE_ROUND_START);
        int slot_pct = 0;
        for (int i = 0; i < 4; ++i) {
            slot_pct = std::max(slot_pct, ctx.ws.damage_reduce_add[0][i]);
        }
        CHECK_EQ(slot_pct, 40, "D2) ★ 下回合 ROUND_START 重写加算减伤槽 = 40");

        // 同回合继续推进到对手（先手方）打完这一刀
        const int hp_before = ctx.getPet(0).hp;
        b.step_past(State::BATTLE_FIRST_AFTER_ACTION);
        const int d2 = hp_before - ctx.getPet(0).hp;
        CHECK(d2 > d1 * 45 / 100 && d2 < d1 * 80 / 100,
              "D3) ★ 本次攻击伤害约为未减伤时的 60%（d1=" + std::to_string(d1)
                  + " d2=" + std::to_string(d2) + "）");
        int slot_after = 0;
        for (int i = 0; i < 4; ++i) {
            slot_after = std::max(slot_after, ctx.ws.damage_reduce_add[0][i]);
        }
        CHECK_EQ(slot_after, 0, "D3) ★ pending 被消费：减伤槽清 0（'**下次**攻击'只用一次）");
        b.drive();
        std::cout << "  [D1~D3] 卌·吸取 40% + 回血 + 下次攻击 -40% 通过\n";
    }
    {
        // D4 "下场即撤"：官方《名词解释》idx=127「之后」——"未提及结束时点时，
        //     默认**持续至精灵下场为止**" → 换下去再换回来，减伤**不复活**
        ScenarioBattle b(kong_party(), make_party(foe_pet(/*speed=*/50, 4000)));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, /*kong_slot=*/3);              // 第 1 回合：用 4 号位 → 挂 pending
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);   // 第 2 回合：换下
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.on_stage[0], 1, "D4) 前置：空元行者已下场");
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 0);   // 第 3 回合：换回
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.on_stage[0], 0, "D4) 前置：空元行者已回到场上");
        // 第 4 回合：pending 若未撤销，ROUND_START 会重新写出 40
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.step_past(State::BATTLE_ROUND_START);
        int slot_pct = 0;
        for (int i = 0; i < 4; ++i) {
            slot_pct = std::max(slot_pct, ctx.ws.damage_reduce_add[0][i]);
        }
        CHECK_EQ(slot_pct, 0, "D4) ★ 下场即撤：换回来也不再有'下次攻击 -40%'");
        b.drive();
        std::cout << "  [D4] 卌·下场即撤（不复活）通过\n";
    }

    std::cout << "scenario_103 PASS (空元之铭①~④：仐/叵/乂/卌)" << std::endl;
    return 0;
}
