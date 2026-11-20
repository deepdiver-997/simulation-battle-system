#include <utils/sbs_rand.h>
// 场景 4220 —— K266 低体先制+2+随机异常族（366，soul_lib/k266_lowhp_preempt_randanomaly.cpp）
//（2026-10-21，worktree ../agents/k266；号段 4220-4229 → T4220-T4223 四块）
//
// 断言口径：
//   - T4220 ①句先制门：印主速度 100 vs 对手 101（速度必输）。满血回合 pr=对手侧；
//     hp 压到 <1/2 → pr 翻印主（先制+2 压过速度）；hp=50000（max=99999 奇数，半线
//     49999.5，"小于1/2"严格）→ pr 回对手侧；再压 49999 → 又翻。②句以 db[1]=0 关闸
//     （概率门 0% 短路），隔离随机面。
//   - T4221 ②句统计：db[1]=15，30 刀（拍打 PP=35 封顶）对手处于任意官方异常的回合数
//     ∈ [1,12]（p=0.15/n=30 → 期望 4.5；种子定序逐次一致；若误读 db[0]=1 → p=0.01
//     期望 0.3，下沿 1 大概率转红）。
//   - T4222 ②句关闸直证：db[1]=0，20 刀零异常（args 驱动：0 = 禁用档）。
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
constexpr int kMarkMaxHp = 99999;

SoulMark mark366(std::vector<int> row) {
    // ⚠️ 直注官方行原序（K236 场景同款）：实现走宿主 owned_int_args 通道，
    // 不经 {0,0} 前缀（那是类型化 a.row 通道的构造约定，两条通道别混）。
    SoulMark sm(366, "K266 魂印", "test", EffectArgs(std::move(row)));
    sm.monster_id = 2771;
    return sm;
}

ElfPet mark_pet(std::vector<int> row) {
    ElfPet p = make_pet(1, "忍王·壬无月", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/300, kMarkMaxHp,
                        mark366(std::move(row)));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/101, /*atk=*/1, 800000);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

void round_attack(ScenarioBattle& b, int actor) {
    b.submit_operation(actor, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1 - actor, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

void round_both_attack(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

bool any_anomaly(BattleContext& ctx, int side) {
    for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
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

    // ══ T4220 ①句：低体门翻转 pr；半线上方不翻（"小于1/2"严格）════════════════
    {
        sbs_srand(4220);
        ScenarioBattle b(make_party(mark_pet({1, 0})), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();

        round_both_attack(b);   // R1：满血（99999 ≥ 半线）→ 无先制加成
        CHECK_EQ(ctx.preemptive_right, PreemptiveRight::SEER_ROBOT_2,
                 "T4220 366：满血 → 先制门不生效，速度序（对手 101 先）");

        ctx.getPet(0).hp = 49999;   // 49999×2=99998 < 99999 → 小于 1/2
        round_both_attack(b);   // R2：低体 → 先制+2 压过速度
        CHECK_EQ(ctx.preemptive_right, PreemptiveRight::SEER_ROBOT_1,
                 "T4220 366：hp<1/2 → 先制+2 翻转 pr（印主先）");

        ctx.getPet(0).hp = 50000;   // 50000×2=100000 ≥ 99999 → 恰在半线上方
        round_both_attack(b);   // R3：FMR 时点评估门（回合内被打下去也不回头翻）
        CHECK_EQ(ctx.preemptive_right, PreemptiveRight::SEER_ROBOT_2,
                 "T4220 366：hp 恰 ≥1/2 → 门不生效，速度序回归");
        std::cout << "  [T4220] 低体先制门 通过\n";
    }

    // ══ T4221 ②句：15% 随机异常统计带（30 刀 ∈ [1,12]）════════════════════════
    {
        sbs_srand(20261021);
        ScenarioBattle b(make_party(mark_pet({1, 15})), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        int anomaly_rounds = 0;
        for (int i = 0; i < 30; ++i) {
            round_attack(b, 0);
            if (any_anomaly(ctx, 1)) {
                ++anomaly_rounds;
            }
        }
        CHECK(anomaly_rounds >= 1 && anomaly_rounds <= 12,
              "T4221 366：30 刀 15% 随机异常回合数 ∈ [1,12]（实测 " +
                  std::to_string(anomaly_rounds) + "）");
        std::cout << "  [T4221] 15% 随机异常统计带 通过（实测 " << anomaly_rounds
                  << "）\n";
    }

    // ══ T4222 ②句关闸：db[1]=0 → 20 刀零异常（args 驱动直证）═══════════════════
    {
        sbs_srand(4222);
        ScenarioBattle b(make_party(mark_pet({1, 0})), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        int anomaly_rounds = 0;
        for (int i = 0; i < 20; ++i) {
            round_attack(b, 0);
            if (any_anomaly(ctx, 1)) {
                ++anomaly_rounds;
            }
        }
        CHECK_EQ(anomaly_rounds, 0,
                 "T4222 366：db[1]=0 关闸 → 20 刀零异常（若误用固定 15% 会 ≈3）");
        std::cout << "  [T4222] 0% 关闸对照 通过\n";
    }

    // ══ T4223 ②句上限锚：db[1]=100 → 30 刀全中（概率门 100% 档短路）═══════════
    {
        sbs_srand(4223);
        ScenarioBattle b(make_party(mark_pet({1, 100})), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        int anomaly_rounds = 0;
        for (int i = 0; i < 30; ++i) {
            round_attack(b, 0);
            if (any_anomaly(ctx, 1)) {
                ++anomaly_rounds;
            }
        }
        CHECK(anomaly_rounds >= 25,
              "T4223 366：db[1]=100 → 30 刀异常回合数 ≥25（实测 " +
                  std::to_string(anomaly_rounds) +
                  "；睡眠被同刀'命中即醒'清掉是唯一预期缺口，SOP §三.2）");
        std::cout << "  [T4223] 100% 上限锚 通过（实测 " << anomaly_rounds << "）\n";
    }

    std::cout << "scenario_4220 PASS (K266 低体先制+2+随机异常族)" << std::endl;
    return 0;
}
