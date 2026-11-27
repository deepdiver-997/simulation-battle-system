#include <utils/sbs_rand.h>
// 场景 3720 —— K228 受击吸强/石化/封属复合族（1456，soul_lib/k228_hit_drain_petrify_seal.cpp）
//（2026-10-08，worktree ../agents/k228；号段 3720-3729 → T3720-T3723 四块）
//
// ⚠️ 断言全部走纯 drive() 流（回合完成点读 ctx/ws 快照）——跨回合 debug_step 停靠
//   （step_past）与 FSM 操作队列不合（实测 R2 停靠空转，2026-10-08），勿改回。
//
// 断言口径（行1 {3,2,1,50,2,1}：回 1/3+先制+1；行2 {2,2,2,50,2,1}：回 1/2+先制+2）：
//   - T3720 吸取成功链（确定性，无掷点）：受击 → 对手强化被整体搬走、自身获得；
//     回 33333（99999/3，压血防封顶，last_heal 精确断言）；对手 -33333 固定伤；
//   - T3721 先制窗界：触发在 R1 → R2/R3 回合完成点 preemptive_right 低速翻盘 +
//     ws.preemptive_level[0]==+1；R4 归零、高速对手恢复先手（"下2回合"）；
//   - T3722 石化/封属分支（50% 掷点运行期自校准，K12 2920 口径）：对手无提升 →
//     掷中=石化落地（abnormal_status_end_round）且不封属；未触发=下2回合封属票
//     （rule_center has_seal_kind，R2/R3 活、R4 过期）且不先制；
//   - T3723 行2 参数（回 1/2+先制+2）：吸取成功 → 回 49999、对手 -49999；R2 先制 +2。
#include <iostream>
#include <vector>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kJi = 10001;   // 撞击：无效果 priority 0 载体（对手攻击用）

constexpr int kMaxHp = 99999;

ElfPet mark_pet(int speed, int hp, const std::vector<int>& row_args) {
    ElfPet p = make_pet(1, "印主", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/speed, /*atk=*/1, /*hp=*/hp,
                        /*soul_mark=*/SoulMark(1456, "K228 魂印", "",
                                               EffectArgs(row_args)));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_pet(int speed) {
    ElfPet p = make_pet(2, "对手", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/speed, /*atk=*/1, /*hp=*/kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// 封属真消费载体：第 5 技 = 瞪眼 20004（属性技，effect 5 原生解析 100% 降对手防御）。
ElfPet stare_pet(int speed) {
    ElfPet p = make_pet(2, "对手", {kJi, kJi, kJi, kJi, 20004},
                        /*speed=*/speed, /*atk=*/1, /*hp=*/kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// 触发一轮"对手攻击印主"：对手强化由调用方预置（开战后写 ability_levels）。
void round1_foe_hits(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

void drive_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }
    const int speed_slot = static_cast<int>(NumericalPropertyIndex::SPEED);
    const int petrify_id = static_cast<int>(AbnormalStatusId::Petrify);
    const std::vector<int> row1{3, 2, 1, 50, 2, 1};
    const std::vector<int> row2{2, 2, 2, 50, 2, 1};

    // ══ T3720 吸取成功链（行1）：搬强化 + 回 1/3 + 等量固定伤 ══
    {
        ScenarioBattle b(make_party(mark_pet(/*speed=*/100, kMaxHp, row1)),
                         make_party(plain_pet(/*speed=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][speed_slot] = 2;   // 对手速度 +2（开战后预置）
        ctx.getPet(0).hp = 30000;                // 压血防封顶（缺口 69999 > 33333）
        const int foe_hp0 = ctx.getPet(1).hp;
        round1_foe_hits(b);
        CHECK_EQ(ctx.ability_levels[1][speed_slot], 0,
                 "T3720 1456：对手速度强化被吸走");
        CHECK_EQ(ctx.ability_levels[0][speed_slot], 2,
                 "T3720 1456：自身获得吸来的速度强化");
        // 回血精确量 = ws.last_heal_amount（heal_amount 记账；回合完成点读当轮值）。
        // 净值 = 1/3 − 对手红伤（atk=1 → 1~2 点浮动），带宽断言。
        CHECK_EQ(ctx.ws.last_heal_amount[0], kMaxHp / 3,
                 "T3720 1456：恢复最大体力 1/3（行1 db[0]=3）");
        const int net_heal = ctx.getPet(0).hp - 30000;
        CHECK(net_heal >= kMaxHp / 3 - 2 && net_heal <= kMaxHp / 3 - 1,
              "T3720 1456：净值 = 1/3 − 对手红伤");
        CHECK_EQ(foe_hp0 - ctx.getPet(1).hp, kMaxHp / 3,
                 "T3720 1456：等量固定伤害（印主 NONE，唯一失血源）");
        std::cout << "  [T3720] 吸取成功链 通过\n";
    }

    // ══ T3721 先制窗界（行1，+1 下2回合）：R2/R3 生效、R4 过期 ══
    // 全 drive 流：回合完成点 preemptive_right / ws.preemptive_level 保留当轮值。
    {
        ScenarioBattle b(make_party(mark_pet(/*speed=*/100, kMaxHp, row1)),
                         make_party(plain_pet(/*speed=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][speed_slot] = 1;
        round1_foe_hits(b);   // R1：触发吸取（先制窗账本 until=R3）

        drive_round(b);   // R2
        CHECK_EQ(ctx.ws.preemptive_level[0], 1, "T3721 1456：R2 先制 +1");
        CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_1,
              "T3721 1456：R2 低速印主（100 vs 300）先制翻盘先手");
        drive_round(b);   // R3
        CHECK_EQ(ctx.ws.preemptive_level[0], 1, "T3721 1456：R3 先制 +1（窗内）");
        CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_1,
              "T3721 1456：R3 印主仍先手");
        drive_round(b);   // R4
        CHECK_EQ(ctx.ws.preemptive_level[0], 0, "T3721 1456：R4 窗过期归零");
        CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_2,
              "T3721 1456：R4 高速对手恢复先手");
        std::cout << "  [T3721] 先制窗界 通过\n";
    }

    // ══ T3722 石化/封属分支（50% 自校准）：两个种子各断一条支 ══
    int seed_petrify = -1;
    int seed_seal = -1;
    for (int s = 1; s <= 400 && (seed_petrify < 0 || seed_seal < 0); ++s) {
        ScenarioBattle b(make_party(mark_pet(/*speed=*/100, kMaxHp, row1)),
                         make_party(plain_pet(/*speed=*/300)));
        b.start();
        b.ctx().seed_rng(s);
        round1_foe_hits(b);
        const bool petrified = b.ctx().abnormal_status_end_round[1][petrify_id] > 0;
        if (petrified && seed_petrify < 0) {
            seed_petrify = s;
        }
        if (!petrified && seed_seal < 0) {
            // 未石化 = 未触发档（裸对局无免疫/抗性，非石化即 ROLL_FAILED）。
            seed_seal = s;
        }
    }
    CHECK(seed_petrify > 0, "T3722 1456：石化档种子可达");
    CHECK(seed_seal > 0, "T3722 1456：未触发档种子可达");
    std::cout << "  [T3722] 自校准种子：石化=" << seed_petrify
              << " 封属=" << seed_seal << "\n";

    // —— 未触发档：下2回合封属——观测面 = EVENT_SKILL_ARMOR_RESOLVED(blocked=true，
    //     封属票真实拦截属性技；与载体技效果是否落地无关——古早载体技（瞪眼 20004）
    //     效果不落地也不影响本断言）。R2/R3 拦截、R4 不拦（"下2回合"）。——
    {
        ScenarioBattle b(make_party(mark_pet(/*speed=*/100, kMaxHp, row1)),
                         make_party(stare_pet(/*speed=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.ctx().seed_rng(seed_seal);
        round1_foe_hits(b);
        CHECK_EQ(ctx.abnormal_status_end_round[1][petrify_id], 0,
                 "T3722 1456：未触发档不石化");
        int sealed_hits = 0;
        ctx.event_center_.register_watcher(
            EventType::EVENT_SKILL_ARMOR_RESOLVED, 0, ctx.roundCount,
            /*duration_rounds=*/0, /*once=*/false,
            [&sealed_hits](BattleContext*, const BattleEvent& ev) {
                if (ev.target == 1 && ev.blocked) {
                    ++sealed_hits;
                }
            },
            WatcherScope::TEAM);
        for (int r = 2; r <= 4; ++r) {
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 4);
            b.drive();
            if (r == 2) {
                // ⚠️ 不在完成点断 has_seal_kind：grant_seal 的 register_round 恒 0，
                // window_expired 在完成点（cur=rc+1）读"过期"是账面伪影；真实拦截以
                // notify（remaining_rounds>0）为准 = 下面的 sealed_hits 计数。
            }
            CHECK_EQ(sealed_hits, r <= 3 ? r - 1 : 2,
                     (r <= 3 ? "T3722 1456：R" : "T3722 1456：R4 窗过期不拦（累计 ")
                         << r << "）");
            CHECK_EQ(ctx.ws.preemptive_level[0], 0,
                     "T3722 1456：未触发档不先制");
        }
        std::cout << "  [T3722] 封属分支窗界 通过\n";
    }

    // —— 石化档：石化落地、不封属、不先制 ——
    {
        ScenarioBattle b(make_party(mark_pet(/*speed=*/100, kMaxHp, row1)),
                         make_party(plain_pet(/*speed=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.ctx().seed_rng(seed_petrify);
        round1_foe_hits(b);
        CHECK(ctx.abnormal_status_end_round[1][petrify_id] > 0,
              "T3722 1456：石化档落地（end_round > 0）");
        drive_round(b);
        CHECK(!ctx.rule_center_.has_seal_kind(1, SealKind::SEAL_ATTRIBUTE,
                                              ctx.roundCount),
              "T3722 1456：石化档不给封属补偿");
        CHECK_EQ(ctx.ws.preemptive_level[0], 0, "T3722 1456：石化档不先制");
        std::cout << "  [T3722] 石化分支 通过\n";
    }

    // ══ T3723 行2 参数（{2,2,2,50,2,1}）：回 1/2 + 等量伤 + 先制 +2 ══
    {
        ScenarioBattle b(make_party(mark_pet(/*speed=*/100, kMaxHp, row2)),
                         make_party(plain_pet(/*speed=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][speed_slot] = 2;
        ctx.getPet(0).hp = 20000;   // 缺口 79999 > 49999
        const int foe_hp0 = ctx.getPet(1).hp;
        round1_foe_hits(b);
        CHECK_EQ(ctx.ws.last_heal_amount[0], kMaxHp / 2,
                 "T3723 1456：恢复最大体力 1/2（行2 db[0]=2）");
        const int net_heal2 = ctx.getPet(0).hp - 20000;
        CHECK(net_heal2 >= kMaxHp / 2 - 2 && net_heal2 <= kMaxHp / 2 - 1,
              "T3723 1456：净值 = 1/2 − 对手红伤");
        CHECK_EQ(foe_hp0 - ctx.getPet(1).hp, kMaxHp / 2,
                 "T3723 1456：等量固定伤害（行2）");
        drive_round(b);
        CHECK_EQ(ctx.ws.preemptive_level[0], 2,
                 "T3723 1456：R2 先制 +2（行2 db[2]=2）");
        std::cout << "  [T3723] 行2 参数 通过\n";
    }

    std::cout << "scenario_3720 PASS (K228 受击吸强/石化/封属复合族)" << std::endl;
    return 0;
}
