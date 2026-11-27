#include <utils/sbs_rand.h>
// 场景 3760 —— K232 睡眠/自强/消回合/吸血复合族（834，soul_lib/k232_sleep_burst_family.cpp）
//（2026-10-08，worktree ../agents/k232；号段 3760-3769 → T3760-T3763 四块）
//
// 断言口径（行1 [35,50,3,1]：35%睡/50%消回合；行2 [40,60,3,1]：40%/60%）：
//   - T3760 睡眠未触发分支（50% 掷点运行期自校准，K12 2920 口径）：攻击技使用 →
//     未触发档 = 对手不睡 + 自身特攻+1、速度+1（消回合 roll 对裸对局无可消除物，无害）；
//   - T3761 睡眠命中分支（自校准）：对手入睡（end_round>0）、自身不强化；
//   - T3762 ③ 命中睡眠对手（确定性——预置睡眠 set_abnormal_status_end_round）：
//     印主攻击命中 → 对手 -33333（99999/3 固定）+1~2 红伤（带宽）；回合末醒睡钩子
//     已清睡眠槽（anomaly_on_attack_hit_hook 在桶后，场景 057 同 API 预置）；
//   - T3763 行2 参数（40/60）：未触发档自身 +1/+1 同形（独立自校准）。
//   ⚠️ 全部纯 drive() 流（跨回合 step 停靠与 FSM 操作队列不合，3720 场景记档）。
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

constexpr int kJi = 10001;   // 撞击：攻击技载体（触发 EVENT_SKILL_USED，声明类型非属性）
constexpr int kMaxHp = 99999;
constexpr int kSleepId = static_cast<int>(AbnormalStatusId::Sleep);

ElfPet mark_pet(const std::vector<int>& row_args) {
    ElfPet p = make_pet(1, "印主", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/300, /*atk=*/1, /*hp=*/kMaxHp,
                        /*soul_mark=*/SoulMark(834, "K232 魂印", "",
                                               EffectArgs(row_args)));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_pet() {
    ElfPet p = make_pet(2, "对手", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/100, /*atk=*/1, /*hp=*/kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

void mark_attacks(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }
    const int sp_atk = 1;   // 特攻槽
    const int sp_spe = 4;   // 速度槽
    const std::vector<int> row1{35, 50, 3, 1};
    const std::vector<int> row2{40, 60, 3, 1};

    // 自校准：找行1 的"未触发"与"睡眠"种子（对手不睡且自身+1/+1 = 未触发）。
    int seed_fail = -1;
    int seed_sleep = -1;
    for (int s = 1; s <= 400 && (seed_fail < 0 || seed_sleep < 0); ++s) {
        ScenarioBattle b(make_party(mark_pet(row1)), make_party(plain_pet()));
        b.start();
        b.ctx().seed_rng(s);
        mark_attacks(b);
        const bool asleep = b.ctx().abnormal_status_end_round[1][kSleepId] > 0;
        if (asleep && seed_sleep < 0) {
            seed_sleep = s;
        }
        if (!asleep && seed_fail < 0) {
            seed_fail = s;
        }
    }
    CHECK(seed_fail > 0, "T3760 834：未触发档种子可达");
    CHECK(seed_sleep > 0, "T3761 834：睡眠档种子可达");
    std::cout << "  [3760] 自校准种子：未触发=" << seed_fail
              << " 睡眠=" << seed_sleep << "\n";

    // ══ T3760 未触发分支：自强 +1/+1、对手不睡 ══
    {
        ScenarioBattle b(make_party(mark_pet(row1)), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.ctx().seed_rng(seed_fail);
        mark_attacks(b);
        CHECK_EQ(ctx.abnormal_status_end_round[1][kSleepId], 0,
                 "T3760 834：未触发档对手不睡");
        CHECK_EQ(ctx.ability_levels[0][sp_atk], 1,
                 "T3760 834：未触发档自身特攻+1");
        CHECK_EQ(ctx.ability_levels[0][sp_spe], 1,
                 "T3760 834：未触发档自身速度+1");
        std::cout << "  [T3760] 未触发自强分支 通过\n";
    }

    // ══ T3761 睡眠分支：对手入睡、自身不强化 ══
    {
        ScenarioBattle b(make_party(mark_pet(row1)), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.ctx().seed_rng(seed_sleep);
        mark_attacks(b);
        CHECK(ctx.abnormal_status_end_round[1][kSleepId] > 0,
              "T3761 834：睡眠档对手入睡");
        CHECK_EQ(ctx.ability_levels[0][sp_atk], 0,
                 "T3761 834：睡眠档自身不强化");
        CHECK_EQ(ctx.ability_levels[0][sp_spe], 0,
                 "T3761 834：睡眠档自身不强化（速）");
        std::cout << "  [T3761] 睡眠分支 通过\n";
    }

    // ══ T3762 ③ 命中睡眠中的对手 → 附加 1/3 固定伤（预置睡眠，确定性）══
    {
        ScenarioBattle b(make_party(mark_pet(row1)), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.set_abnormal_status_end_round(1, kSleepId,
                                          ctx.roundCount + 5);   // 预置睡眠（场景 057 同 API）
        const int foe_hp0 = ctx.getPet(1).hp;
        mark_attacks(b);
        const int loss = foe_hp0 - ctx.getPet(1).hp;
        CHECK(loss >= kMaxHp / 3 + 1 && loss <= kMaxHp / 3 + 2,
              "T3762 834：命中睡眠对手 = 固定 1/3 + 红伤 1~2（实失 " +
                  std::to_string(loss) + "）");
        CHECK_EQ(ctx.abnormal_status_end_round[1][kSleepId], 0,
                 "T3762 834：命中后醒睡钩子已清睡眠槽");
        std::cout << "  [T3762] 命中睡眠附加伤 通过\n";
    }

    // ══ T3763 行2 参数（[40,60,3,1]）：未触发档自强同形（独立自校准）══
    {
        int seed_fail2 = -1;
        for (int s = 1; s <= 400 && seed_fail2 < 0; ++s) {
            ScenarioBattle b(make_party(mark_pet(row2)), make_party(plain_pet()));
            b.start();
            b.ctx().seed_rng(s);
            mark_attacks(b);
            if (b.ctx().abnormal_status_end_round[1][kSleepId] <= 0) {
                seed_fail2 = s;
            }
        }
        CHECK(seed_fail2 > 0, "T3763 834：行2 未触发档种子可达");
        ScenarioBattle b(make_party(mark_pet(row2)), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.ctx().seed_rng(seed_fail2);
        mark_attacks(b);
        CHECK_EQ(ctx.abnormal_status_end_round[1][kSleepId], 0,
                 "T3763 834：行2 未触发档对手不睡");
        CHECK_EQ(ctx.ability_levels[0][sp_atk], 1,
                 "T3763 834：行2 未触发档自身特攻+1");
        CHECK_EQ(ctx.ability_levels[0][sp_spe], 1,
                 "T3763 834：行2 未触发档自身速度+1");
        std::cout << "  [T3763] 行2 参数 通过 (seed " << seed_fail2 << ")\n";
    }

    std::cout << "scenario_3760 PASS (K232 睡眠/自强/消回合/吸血复合族)" << std::endl;
    return 0;
}
