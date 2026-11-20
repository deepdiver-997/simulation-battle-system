// 场景 5470 —— J218 概率异常低体翻倍族（1210，moves_lib/j_prob_anomaly_low_hp.cpp）
//（2026-10-22，worktree feat/jbatch；号段 5470-5479）
//
// 断言口径（审计槽 + 精确异常位 + 统计带）：
//   - 1210（冥土掩埋 37041 纯链 [1210]|[25,9]）：{0}=25%、{1}=石化(Petrify=9)。
//   - a) 自身满体 10000 > 对手 5000 → 不翻倍：每局审计申报 p=25；24 局落地带 [1,14]。
//   - b) 自身 4000 < 对手 5000 → 翻倍：每局审计申报 p=50；24 局落地带 [4,20]。
//   - 每局独立（避免 PP 10 上限）：掷中（SUCCESS）时对手必有石化；ROLL_FAILED 时必无。
//   - 体力比较是 SKILL_EFFECT 时点（本刀伤害前）的当前体力，故 a/b 条件在提交前铺设。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa     = 10004;  // 拍打：物 40（填充刀）
constexpr int kMingTu    = 37041;  // [1210]|[25,9]（纯链；25% 石化）
constexpr int kPetrify   = static_cast<int>(AbnormalStatusId::Petrify);  // 石化 9
constexpr int kPctBase   = 990121000;     // +self*2：本次申报概率
constexpr int kResultBase = 990121001;    // +self*2：ApplyAnomalyResult 整数
constexpr int kTrials    = 24;
constexpr int kSelfMax   = 10000;
constexpr int kFoeMax    = 5000;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_self() {
    ElfPet p = make_pet(1, "己方", {kMingTu, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, /*hp=*/kSelfMax);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/kFoeMax);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

// 单局：铺设 self_hp，施放 1210，返回审计落地数（0/1）。
int run_one(int self_hp, int expected_pct) {
    ScenarioBattle b(make_party(mk_self()), make_party(mk_foe()));
    b.start();
    BattleContext& ctx = b.ctx();
    ctx.getPet(0).hp = self_hp;   // 只压当前体力，最大体力不变
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();

    CHECK_EQ(std::any_cast<int>(ctx.plugin_storage.at(kPctBase + 0)), expected_pct,
             "T5470 1210：申报概率应 " + std::to_string(expected_pct) + "（self.hp=" +
                 std::to_string(self_hp) + "，opp.hp=" + std::to_string(ctx.getPet(1).hp) +
                 "）");
    const int r = std::any_cast<int>(ctx.plugin_storage.at(kResultBase + 0));
    const bool anomaly = ctx.has_active_abnormal_status(1, kPetrify);
    if (r == static_cast<int>(ApplyAnomalyResult::SUCCESS)) {
        CHECK(anomaly, "T5470 1210：掷中（SUCCESS）→ 对手必有石化");
        return 1;
    }
    if (r == static_cast<int>(ApplyAnomalyResult::ROLL_FAILED)) {
        CHECK(!anomaly, "T5470 1210：未掷中（ROLL_FAILED）→ 对手无石化");
        return 0;
    }
    // 其余档（免疫/抗性/替换/延长）在本干净场景不应出现。
    CHECK(false, "T5470 1210：非预期 ApplyAnomalyResult=" + std::to_string(r));
    return 0;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5470 1210 低体概率翻倍 ══
    {
        int landed25 = 0;
        for (int i = 0; i < kTrials; ++i) {
            landed25 += run_one(/*self_hp=*/kSelfMax, /*expected_pct=*/25);   // 10000 > 5000
        }
        CHECK(landed25 >= 1 && landed25 <= 14,
              "T5470.a 1210 p=25 24 局落地带 [1,14]，实测 " + std::to_string(landed25));

        int landed50 = 0;
        for (int i = 0; i < kTrials; ++i) {
            landed50 += run_one(/*self_hp=*/4000, /*expected_pct=*/50);       // 4000 < 5000
        }
        CHECK(landed50 >= 4 && landed50 <= 20,
              "T5470.b 1210 p=50 24 局落地带 [4,20]，实测 " + std::to_string(landed50));

        CHECK(landed50 > landed25,
              "T5470.c 1210 翻倍侧落地率应更高（p25=" + std::to_string(landed25) +
                  "，p50=" + std::to_string(landed50) + "）");

        std::cout << "  [T5470] 1210 p25 落地 " << landed25 << "/" << kTrials
                  << "、p50 落地 " << landed50 << "/" << kTrials << " 通过\n";
    }

    std::cout << "scenario_5470 PASS (J218 概率异常低体翻倍族)" << std::endl;
    return 0;
}
