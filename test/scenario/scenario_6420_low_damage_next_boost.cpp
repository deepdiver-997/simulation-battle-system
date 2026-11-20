// 场景 6420 —— J309 伤害不足→下N次增伤族（918/1016，
// moves_lib/j_low_damage_next_boost.cpp）（2026-10-22，worktree feat/jbatch；号段 6420-6429）
//
// 断言口径（审计槽 + 绝对量带 + 同局比值三重锁）：
//   - 触发面 = 伤害管线 DETECT 相位读 `resolvedDamage.final`：
//     **不足{0}** = 严格小于阈值（等值不触发）；只认**落地红伤**（isRed && final>0）。
//   - 审计槽 991918100/991016100 = 触发那一发的 final（-999 = 未触发）；
//     991918101/991016101 = 授予的 count（= 次数+1，消费点补偿）。
//   - 增伤面 = `grant_attack_boost` 次数型票：消费在"成功使用攻击技后"、伤害结算**之前**；
//     本次施放的消费早于授予 → 授 次数+1 才命中 次数 次（1037/1850 同款补偿）。
//   - 伤害带按官方公式反推：damage = (0.84×攻/防×威力+2)×克制1.0×(217+rand%39)/255
//     → 拍打(威力40)@攻100 = 35.6×[0.851,1.0] → [30,35]；+100% → [60,70]；+50% → [45,52]。
//     克制=1.0（守方系别 0 无克制行）、本系加成不触发（宠系别 {0,0} vs 技能系别≠0）。
//   - **同局比值**锁"增伤真落地"：同局 R2(吃增伤)/R3(不吃) 的比值窗口
//     = 增伤倍数×[0.851,1/0.851]（+100% → [1.70,2.35]；+50% → [1.28,1.76]），
//     比绝对带更抗浮动（相邻档位带间距 < 波动幅度时的标准手法）。
//   - 触发那一发**自己不吃增伤**（授予发生在 DETECT，本次伤害已算完）→ R1 绝对带即锁。
#include <iostream>
#include <any>
#include <string>
#include <vector>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <plugin/core_api.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;      // 拍打：物 40（填充技，无被测效果）
constexpr int k918Pure = 19605;    // 梦之无解念 [918]|[300,1,100]（纯链，物 150）
constexpr int k1016Pure = 31718;   // 烟花易冷 [1043,1016]|[300,1,100]（特 150）
constexpr int k1016Half = 38332;   // 初诞·始源胎动 [990,1016]|[300,1,50]（特 160）
constexpr int k1016Twice = 38459;  // 沧海铭刻 [1219,2303,1016]|[300,2,50]（特 160，次数=2）

constexpr int k918Audit = 991918100;
constexpr int k918Grant = 991918101;
constexpr int k1016Audit = 991016100;
constexpr int k1016Grant = 991016101;

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
}

ElfPet mk_owner(int carrier, int atk, int hp = 99999) {
    ElfPet p = make_pet(1, "己方", {carrier, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/atk, /*hp=*/hp);
    accuracy_100(p);
    return p;
}

ElfPet mk_foe(int hp = 99999) {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/hp);
    accuracy_100(p);
    return p;
}

int audit(BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return -999;
    }
    if (const int* p = std::any_cast<int>(&it->second)) {
        return *p;
    }
    return -999;
}

// 一局：R1 载体技 → R2..R(n+1) 拍打（填充）。
struct BoostTrial {
    int carrier_damage = 0;     // R1 载体技打出的伤害
    std::vector<int> follow;    // R2.. 的拍打伤害
    int audit = -999;            // 审计槽：触发那一发的 final（-999 = 未触发）
    int grant = -999;            // 审计槽：授予的 count
};

BoostTrial run_trial(int carrier, int atk, int follow_rounds, int audit_key, int grant_key) {
    ScenarioBattle b(make_party(mk_owner(carrier, atk)), make_party(mk_foe()));
    b.start();
    BattleContext& ctx = b.ctx();
    BoostTrial t;
    int before = ctx.getPet(1).hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 载体技
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
    t.carrier_damage = before - ctx.getPet(1).hp;
    t.audit = audit(ctx, audit_key);
    t.grant = audit(ctx, grant_key);
    for (int r = 0; r < follow_rounds; ++r) {
        before = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // 拍打
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        t.follow.push_back(before - ctx.getPet(1).hp);
    }
    return t;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T6420.a 918 触发（伤害<300）→ 下 1 次攻击 +100%；再下一发恢复原值 ══
    {
        const BoostTrial t = run_trial(k918Pure, /*atk=*/100, /*follow_rounds=*/2,
                                       k918Audit, k918Grant);
        std::cout << "  [T6420.a] carrier=" << t.carrier_damage
                  << " follow=" << (t.follow.size() > 0 ? t.follow[0] : -1) << ","
                  << (t.follow.size() > 1 ? t.follow[1] : -1)
                  << " audit=" << t.audit << " grant=" << t.grant << std::endl;
        CHECK(t.carrier_damage >= 105 && t.carrier_damage <= 132,
              "T6420.a 载体伤害落在未增伤带 [108,128]（实测 " +
                  std::to_string(t.carrier_damage) + "）——触发那发自己不吃增伤");
        CHECK(t.carrier_damage < 300, "T6420.a 载体伤害 < 300（触发前提，实测 " +
                                           std::to_string(t.carrier_damage) + "）");
        CHECK_EQ(t.audit, t.carrier_damage, "T6420.a 审计槽=触发那一发的 final");
        CHECK_EQ(t.grant, 2, "T6420.a 授予 count=次数+1=2（消费点补偿）");
        CHECK(t.follow.size() == 2, "T6420.a 后续两发都取到");
        CHECK(t.follow[0] >= 58 && t.follow[0] <= 72,
              "T6420.a R2 吃 +100% 增伤（带 [60,70]，实测 " +
                  std::to_string(t.follow[0]) + "）");
        CHECK(t.follow[1] >= 28 && t.follow[1] <= 37,
              "T6420.a R3 次数用尽 → 恢复原带（实测 " + std::to_string(t.follow[1]) + "）");
        CHECK(t.follow[0] > t.follow[1] * 3 / 2,
              "T6420.a 同局比值 R2/R3 > 1.5（实测 " + std::to_string(t.follow[0]) + "/"
                  + std::to_string(t.follow[1]) + "）");
        CHECK(t.follow[0] < t.follow[1] * 5 / 2,
              "T6420.a 同局比值 R2/R3 < 2.5（+100% 理论窗 [1.70,2.35]）");
    }

    // ══ T6420.b 918 门闭（伤害≥300）→ 不触发、不增伤 ══
    {
        const BoostTrial t = run_trial(k918Pure, /*atk=*/1000, /*follow_rounds=*/2,
                                       k918Audit, k918Grant);
        std::cout << "  [T6420.b] carrier=" << t.carrier_damage
                  << " follow=" << (t.follow.size() > 0 ? t.follow[0] : -1) << ","
                  << (t.follow.size() > 1 ? t.follow[1] : -1)
                  << " audit=" << t.audit << " grant=" << t.grant << std::endl;
        CHECK(t.carrier_damage >= 1000, "T6420.b 载体伤害 ≥1000（门闭前提，实测 " +
                                            std::to_string(t.carrier_damage) + "）");
        CHECK_EQ(t.audit, -999, "T6420.b 门闭 → 审计未写（未触发）");
        CHECK_EQ(t.grant, -999, "T6420.b 门闭 → 未授予增伤票");
        CHECK(t.follow.size() == 2, "T6420.b 后续两发都取到");
        CHECK(t.follow[0] >= 280 && t.follow[0] <= 350,
              "T6420.b R2 无增伤（带 [287,338]，实测 " + std::to_string(t.follow[0]) + "）");
        CHECK(t.follow[1] >= 280 && t.follow[1] <= 350,
              "T6420.b R3 无增伤（实测 " + std::to_string(t.follow[1]) + "）");
    }

    // ══ T6420.c 1016 触发（特攻载体）→ 下 1 次攻击 +100% ══
    {
        const BoostTrial t = run_trial(k1016Pure, /*atk=*/100, /*follow_rounds=*/2,
                                       k1016Audit, k1016Grant);
        std::cout << "  [T6420.c] carrier=" << t.carrier_damage
                  << " follow=" << (t.follow.size() > 0 ? t.follow[0] : -1) << ","
                  << (t.follow.size() > 1 ? t.follow[1] : -1)
                  << " audit=" << t.audit << " grant=" << t.grant << std::endl;
        CHECK(t.carrier_damage >= 105 && t.carrier_damage <= 132,
              "T6420.c 1016 载体伤害落在未增伤带（实测 " +
                  std::to_string(t.carrier_damage) + "）");
        CHECK_EQ(t.audit, t.carrier_damage, "T6420.c 审计槽=触发那一发的 final");
        CHECK_EQ(t.grant, 2, "T6420.c 授予 count=2");
        CHECK(t.follow.size() == 2, "T6420.c 后续两发都取到");
        CHECK(t.follow[0] >= 58 && t.follow[0] <= 72,
              "T6420.c R2 吃 +100% 增伤（实测 " + std::to_string(t.follow[0]) + "）");
        CHECK(t.follow[1] >= 28 && t.follow[1] <= 37,
              "T6420.c R3 次数用尽（实测 " + std::to_string(t.follow[1]) + "）");
    }

    // ══ T6420.d 1016 半额档（提升 50%）→ 下 1 次攻击 ×1.5 ══
    {
        const BoostTrial t = run_trial(k1016Half, /*atk=*/100, /*follow_rounds=*/2,
                                       k1016Audit, k1016Grant);
        std::cout << "  [T6420.d] carrier=" << t.carrier_damage
                  << " follow=" << (t.follow.size() > 0 ? t.follow[0] : -1) << ","
                  << (t.follow.size() > 1 ? t.follow[1] : -1)
                  << " audit=" << t.audit << " grant=" << t.grant << std::endl;
        CHECK(t.carrier_damage >= 110 && t.carrier_damage <= 142,
              "T6420.d 载体（威力160）伤害落在未增伤带 [116,136]（实测 " +
                  std::to_string(t.carrier_damage) + "）");
        CHECK_EQ(t.audit, t.carrier_damage, "T6420.d 审计槽=触发那一发的 final");
        CHECK_EQ(t.grant, 2, "T6420.d 授予 count=2");
        CHECK(t.follow.size() == 2, "T6420.d 后续两发都取到");
        CHECK(t.follow[0] >= 42 && t.follow[0] <= 56,
              "T6420.d R2 吃 +50% 增伤（带 [45,52]，实测 " +
                  std::to_string(t.follow[0]) + "）");
        CHECK(t.follow[1] >= 28 && t.follow[1] <= 37,
              "T6420.d R3 次数用尽（实测 " + std::to_string(t.follow[1]) + "）");
        CHECK(t.follow[0] > t.follow[1],
              "T6420.d 同局比值 R2 > R3（半额增伤，实测 " + std::to_string(t.follow[0]) +
                  " vs " + std::to_string(t.follow[1]) + "）");
    }

    // ══ T6420.e 1016 次数=2 档 → 连 2 次攻击吃 +50%，第 3 次恢复 ══
    {
        const BoostTrial t = run_trial(k1016Twice, /*atk=*/100, /*follow_rounds=*/3,
                                       k1016Audit, k1016Grant);
        std::cout << "  [T6420.e] carrier=" << t.carrier_damage
                  << " follow=" << (t.follow.size() > 0 ? t.follow[0] : -1) << ","
                  << (t.follow.size() > 1 ? t.follow[1] : -1) << ","
                  << (t.follow.size() > 2 ? t.follow[2] : -1)
                  << " audit=" << t.audit << " grant=" << t.grant << std::endl;
        CHECK(t.carrier_damage >= 110 && t.carrier_damage <= 142,
              "T6420.e 载体（威力160）伤害落在未增伤带（实测 " +
                  std::to_string(t.carrier_damage) + "）");
        CHECK_EQ(t.audit, t.carrier_damage, "T6420.e 审计槽=触发那一发的 final");
        CHECK_EQ(t.grant, 3, "T6420.e 授予 count=次数+1=3");
        CHECK(t.follow.size() == 3, "T6420.e 后续三发都取到");
        CHECK(t.follow[0] >= 42 && t.follow[0] <= 56,
              "T6420.e R2 第 1 次增伤（实测 " + std::to_string(t.follow[0]) + "）");
        CHECK(t.follow[1] >= 42 && t.follow[1] <= 56,
              "T6420.e R3 第 2 次增伤（实测 " + std::to_string(t.follow[1]) + "）");
        CHECK(t.follow[2] >= 28 && t.follow[2] <= 37,
              "T6420.e R4 次数用尽 → 恢复原带（实测 " + std::to_string(t.follow[2]) + "）");
    }

    std::cout << "scenario_6420 PASS (J309 伤害不足→下N次增伤族)" << std::endl;
    return 0;
}
