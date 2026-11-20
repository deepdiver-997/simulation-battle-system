#include <utils/sbs_rand.h>
// 场景 5000 —— K344 闪避+受伤增幅族（352，soul_lib/k344_dodge_hitmod.cpp）
//（2026-10-21，worktree ../agents/k344；号段 5000-5009 → T5000-T5001 两块）
//
// 断言口径（对手拍打 atk=300 逐刀攻击印主；miss 判别 = 印主 hp 零变化）：
//   - T5000 白盒：ws.hit_rate_mod_attack[对手] = 0.5；30 刀 miss ∈ [6,24]
//     （p=0.5/n=30 → 期望 15）+ damage_reduce_add[0] 含 -20 槽（增幅白盒）；
//     累计落刀伤害 D1/L（L = 落刀数）。
//   - T5001 无印对照：30 刀零 miss（accuracy 100），累计伤害 D0/30。
//   - T5002 刀均增幅比 = (D1/L)/(D0/30) ∈ [1.05,1.35]（期望 ×1.2——±8% 单刀浮动
//     在 ≥6 样本均值下收窄，带宽 ~4σ 防假绿/假红双保险）。
//   （受伤增幅半边 2026-10-21 白班补齐：加算减伤槽负值 = 增幅乘区，见实现头注②。）
#include <cstdlib>
#include <iostream>
#include <vector>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;
constexpr int kMaxHp = 800000;

SoulMark make_mark() {
    // DB args 原序 [500,1,200]：db[0]=500 闪避千分比（db[2]=200 增幅半边记档跳过）。
    SoulMark sm(352, "K344 魂印", "test", EffectArgs(std::vector<int>{500, 1, 200}));
    sm.monster_id = 2728;
    return sm;
}

ElfPet mark_pet(bool with_mark = true) {
    ElfPet p = with_mark
        ? make_pet(1, "噬神狂天", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                   /*speed=*/300, /*atk=*/1, kMaxHp, make_mark())
        : make_pet(1, "裸噬神", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                   /*speed=*/300, /*atk=*/1, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/300, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5000 白盒 + 30 刀闪避统计 ═════════════════════════════════════════════
    //（白盒在首轮 drive 后读——start() 不触发 ROUND_START，K342 同款轮序教训。）
    long long dmg_total = 0;   // 落刀伤害累计（T5002 增幅比分子素材）
    int t5000_misses = 0;
    {
        sbs_srand(5000);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        int misses = 0;
        for (int i = 0; i < 30; ++i) {
            const int hp0 = ctx.getPet(0).hp;
            b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            if (i == 0) {
                CHECK(ctx.ws.hit_rate_mod_attack[1] > 0.49f &&
                          ctx.ws.hit_rate_mod_attack[1] < 0.51f,
                      "T5000 352：白盒——对手命中乘区 = 0.5（实测 " +
                          std::to_string(ctx.ws.hit_rate_mod_attack[1]) + "）");
                bool amp_slot_found = false;
                for (int s = 0; s < 4; ++s) {
                    amp_slot_found = amp_slot_found ||
                                     ctx.ws.damage_reduce_add[0][s] == -20;
                }
                CHECK(amp_slot_found,
                      "T5000 352：白盒——damage_reduce_add[0] 含 -20 增幅槽（实测 [" +
                          std::to_string(ctx.ws.damage_reduce_add[0][0]) + "," +
                          std::to_string(ctx.ws.damage_reduce_add[0][1]) + "," +
                          std::to_string(ctx.ws.damage_reduce_add[0][2]) + "," +
                          std::to_string(ctx.ws.damage_reduce_add[0][3]) + "]）");
            }
            const long long delta = hp0 - ctx.getPet(0).hp;
            if (delta == 0) {
                ++misses;   // 零变化 = 闪避
            } else {
                dmg_total += delta;
            }
        }
        CHECK(misses >= 6 && misses <= 24,
              "T5000 352：30 刀 50% 闪避 ∈ [6,24]（实测 " + std::to_string(misses) +
                  "）");
        t5000_misses = misses;
        std::cout << "  [T5000] 闪避乘区/统计 通过\n";
    }

    // ══ T5001 无印对照：零闪避 ═════════════════════════════════════════════════
    {
        sbs_srand(5001);
        ScenarioBattle b(make_party(mark_pet(/*with_mark=*/false)),
                         make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        int misses = 0;
        long long ctrl_total = 0;   // 对照累计（T5002 分母素材）
        for (int i = 0; i < 30; ++i) {
            const int hp0 = ctx.getPet(0).hp;
            b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            const long long delta = hp0 - ctx.getPet(0).hp;
            if (delta == 0) {
                ++misses;
            } else {
                ctrl_total += delta;
            }
        }
        CHECK_EQ(misses, 0,
                 "T5001 352：无印对照 → 30 刀零闪避（实测 " +
                     std::to_string(misses) + "）");
        std::cout << "  [T5001] 无印对照 通过\n";

        // ══ T5002 刀均增幅比（期望 ×1.2）══════════════════════════════════════
        const int landed = 30 - t5000_misses;
        const double mark_mean = static_cast<double>(dmg_total) / landed;
        const double ctrl_mean = static_cast<double>(ctrl_total) / 30;
        const double ratio = mark_mean / ctrl_mean;
        CHECK(ratio > 1.05 && ratio < 1.35,
              "T5002 352：受伤增幅刀均比 ∈ [1.05,1.35]（实测 " +
                  std::to_string(ratio) + " = " + std::to_string(mark_mean) +
                  "/" + std::to_string(ctrl_mean) + "）");
        std::cout << "  [T5002] 受伤增幅比 通过\n";
    }

    std::cout << "scenario_5000 PASS (K344 闪避+受伤增幅族)" << std::endl;
    return 0;
}
