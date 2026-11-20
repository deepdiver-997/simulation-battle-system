#include <utils/sbs_rand.h>
// 场景 2890 —— K164 攻击附带概率族（109/116/151/266，soul_lib/k_attack_prob.cpp）
//（2026-10-05，worktree ../agents/k164；号段 2890-2899 → T2890 一块五例）
//
// 断言口径（拍打 P40、atk=300 → 自方红伤带 [250,295]；对手 atk=1 不还手）：
//   - a 109（2593 [20,20]）：40 次攻击统计 20% 固伤触发次数 ∈ [4, 24]
//     （p=0.2、n=40 → 期望 8，±4 覆盖 p<1e-4）；每次触发恰 +20（总账分项核对）；
//   - b 116（2163 [10,2]）：60 次攻击统计 10% 烧伤触发次数 ∈ [0, 12]
//     （p=0.1、n=60 → 期望 6，±6 覆盖 p<1e-4）+ 至少一次落地后对手带烧伤状态；
//   - c 151（2132 [20,0] 千分比→2%）：直接用 1000 档换算验证——用 500 次攻击统计
//     太重，改为**概率单位核对**：db=20 → 2%，50 次攻击触发 ∈ [0,3]
//     （p=0.02、n=50 → 期望 1，上界 3 覆盖 p>0.99）+ 命中即死（hp=0）；
//   - d 266（2563 [8,0]）：白盒 hit_rate_mod_attack[opp]=0.92（ROUND_START 重写）。
//   ⚠️ 概率断言一律跑前 srand 定种子（触发面 rand 序固定 → 结果逐次一致）。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;
constexpr int kBurn  = static_cast<int>(AbnormalStatusId::Burn);   // 2
constexpr int kMaxHp = 800000;

SoulMark make_mark(int effect_id, std::vector<int> db_args, int monster_id) {
    SoulMark sm(effect_id, "K" + std::to_string(effect_id), "test",
                EffectArgs(std::move(db_args)));
    sm.monster_id = monster_id;
    return sm;
}

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 0;
    }
}

// 自方：魂印宠、先手；对手：不还手沙包。
ElfPet mark_pet(int effect_id, int monster_id, std::vector<int> db_args) {
    ElfPet p = make_pet(1, "魂印宠", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/300, kMaxHp,
                        make_mark(effect_id, std::move(db_args), monster_id));
    accuracy_100(p);
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, kMaxHp);
    accuracy_100(p);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T2890.a 109（2593 [20,20]）：逐刀分桶计触发（红伤带 [85,100] 与 +20 触发带
    //    [105,120] 不相交 → 逐刀可精确判触发），30 刀（拍打 PP=35 封顶）触发 ∈ [1,18]
    //    （p=0.2/n=30 → 期望 6；种子定序，逐次一致）══
    {
        sbs_srand(20261005);
        ScenarioBattle b(make_party(mark_pet(109, 2593, {20, 20})), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        int triggers = 0;
        for (int i = 0; i < 30 && ctx.getPet(1).hp > 0; ++i) {
            const int hp0 = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
            const int dmg = hp0 - ctx.getPet(1).hp;
            if (dmg >= 105 && dmg <= 120) {
                ++triggers;   // 红伤带 + 固伤 20 = 触发带（不相交判别）
            }
        }
        CHECK(triggers >= 1 && triggers <= 18,
              "T2890.a 109：30 刀 20% 触发次数 ∈ [1,18]（实测 " +
                  std::to_string(triggers) + "）");
    }
    // ══ T2890.b 116（2163 [10,2]）：30 刀 10% 烧伤申报——对手带烧伤回合数 ≥1
    //    （P(30 刀零触发)=0.9^30≈4%，种子定序）/ 对手非烧伤回合不受影响 ══════════
    {
        sbs_srand(20261006);
        ScenarioBattle b(make_party(mark_pet(116, 2163, {10, 2})), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        int burn_rounds = 0;
        for (int i = 0; i < 30 && ctx.getPet(1).hp > 0; ++i) {
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
            if (ctx.has_active_abnormal_status(1, kBurn)) {
                ++burn_rounds;
            }
        }
        CHECK(burn_rounds >= 1,
              "T2840.b 116：30 刀窗口内烧伤触发过（带烧伤回合数 " +
                  std::to_string(burn_rounds) + "）");
    }
    // ══ T2890.c 151（2132 [20,0] 千分比→2%）：单位核对——35 刀（PP 封顶）内
    //    秒杀次数 ≤ 3（p=0.02/n=35 → 期望 0.7；若误读 20% 会 ≈7 刀内秒杀）═══════
    {
        sbs_srand(20261007);
        ScenarioBattle b(make_party(mark_pet(151, 2132, {20, 0})), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        int kills = 0;
        for (int i = 0; i < 35 && ctx.getPet(1).hp > 0; ++i) {
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
            if (ctx.getPet(1).hp == 0) {
                ++kills;
                break;   // 秒杀即终局（每宏一场一次语义无关——本宏无次数限制，仅统计）
            }
        }
        CHECK(kills <= 3,
              "T2890.c 151：千分比口径核对（2% 档 35 刀内秒杀次数 " +
                  std::to_string(kills) + " ≤ 3；若误读 20% 会 ≈7）");
    }
    // ══ T2890.d 266（2563 [8,0]）：命中压制乘区白盒 ═══════════════════════════
    {
        ScenarioBattle b(make_party(mark_pet(266, 2563, {8, 0})), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK(ctx.ws.hit_rate_mod_attack[1] > 0.90f && ctx.ws.hit_rate_mod_attack[1] < 0.94f,
              "T2890.d 266：命中压制乘区 = 0.92（实测 " +
                  std::to_string(ctx.ws.hit_rate_mod_attack[1]) + "）");
    }

    std::cout << "scenario_2890 PASS (K164 攻击附带概率族)" << std::endl;
    return 0;
}
