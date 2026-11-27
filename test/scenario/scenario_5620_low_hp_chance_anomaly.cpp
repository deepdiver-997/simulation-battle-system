// 场景 5620 —— J231 低体概率异常族（159，moves_lib/j_low_hp_chance_anomaly.cpp）
//（2026-10-22，worktree feat/jbatch；号段 5620-5629）
//
// 断言口径（审计槽分档 + 精确异常位 + 统计带）：
//   - 159 模板：自身体力小于最大值的1/{0}时，{1}%概率令对方{2}。
//   - a) 37655 迟波抚镜 [2,100,6害怕]：自身 4999 < 5000 → 门开、100% 落害怕；
//        审计 pct=100、result=SUCCESS。
//   - b) 37655 边界：自身恰 5000（等于 1/2）→ 门闭、审计 result=-1、无害怕。
//   - c) 36869 幻梦之引 [1,100,8睡眠]：自身 9999 < 10000 → 门开、审计 SUCCESS；
//        自身满 10000 → 门闭 -1（1/1 档：满血才关门）。
//   - d) 12788 绝地影袭 [4,50,6害怕]：自身 2499 < 2500 → 每局审计 pct=50；
//        24 局落地带 [4,20]；SUCCESS 必有害怕、ROLL_FAILED 必无。
//   - e) 12788 边界：自身恰 2500（等于 1/4）→ 门闭、无害怕。
//   - 睡眠的"命中即醒"（K2）不影响本盘：a/b/d 用害怕/疲惫等非睡眠异常，
//     c 只断言审计落地、不断言回合末睡眠残留。
#include <any>
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

constexpr int kPaiDa    = 10004;  // 拍打：物 40（填充刀）
constexpr int kChiBo    = 37655;  // [159,1804]|[2,100,6]（物 150；1804 未实现=空转）
constexpr int kHuanMeng = 36869;  // [159]|[1,100,8]（特 130）
constexpr int kJueDi    = 12788;  // [159]|[4,50,6]（物 80）
constexpr int kPctBase  = 990159000;     // +self*2：本次申报概率
constexpr int kResultBase = 990159001;   // +self*2：ApplyAnomalyResult（门闭 -1）
constexpr int kFear    = static_cast<int>(AbnormalStatusId::Fear);
constexpr int kSelfMax = 10000;
constexpr int kFoeMax  = 99999;
constexpr int kTrials  = 24;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_self(int skill0) {
    ElfPet p = make_pet(1, "己方", {skill0, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
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

struct Outcome {
    int pct;
    int result;
    bool fear;
};

// 单局：铺设 self_hp，施放 159 载体，返回审计 + 害怕位。
Outcome run_one(int skill0, int self_hp, unsigned seed = 1) {
    ScenarioBattle b(make_party(mk_self(skill0)), make_party(mk_foe()));
    b.ctx().seed_rng(seed);
    b.start();
    BattleContext& ctx = b.ctx();
    ctx.getPet(0).hp = self_hp;   // 只压当前体力，最大体力不变
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
    Outcome o;
    o.pct = std::any_cast<int>(ctx.plugin_storage.at(kPctBase + 0));
    o.result = std::any_cast<int>(ctx.plugin_storage.at(kResultBase + 0));
    o.fear = ctx.has_active_abnormal_status(1, kFear);
    return o;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5620.a 37655 [2,100,6]：4999 < 5000 → 100% 害怕落地 ══
    {
        const Outcome o = run_one(kChiBo, 4999);
        CHECK_EQ(o.pct, 100, "T5620.a 审计 pct=100");
        CHECK_EQ(o.result, static_cast<int>(ApplyAnomalyResult::SUCCESS),
                 "T5620.a 100% 档应 SUCCESS（实测 " + std::to_string(o.result) + "）");
        CHECK(o.fear, "T5620.a 门开 → 对手害怕");
        std::cout << "  [T5620.a] 1/2 门开 100% 害怕通过\n";
    }

    // ══ T5620.b 37655 边界：恰 5000 → 门闭 ══
    {
        const Outcome o = run_one(kChiBo, 5000);
        CHECK_EQ(o.result, -1, "T5620.b 边界（等于 1/2）门闭 → result=-1");
        CHECK(!o.fear, "T5620.b 边界门闭 → 无害怕");
        std::cout << "  [T5620.b] 1/2 边界门闭通过\n";
    }

    // ══ T5620.c 36869 [1,100,8]：9999 < 10000 门开；满血门闭 ══
    {
        const Outcome open = run_one(kHuanMeng, 9999);
        CHECK_EQ(open.pct, 100, "T5620.c 审计 pct=100");
        CHECK_EQ(open.result, static_cast<int>(ApplyAnomalyResult::SUCCESS),
                 "T5620.c 1/1 档：非满血 → 门开 SUCCESS（实测 " +
                     std::to_string(open.result) + "）");
        const Outcome full = run_one(kHuanMeng, kSelfMax);
        CHECK_EQ(full.result, -1, "T5620.c 1/1 档：满血 → 门闭 result=-1");
        std::cout << "  [T5620.c] 1/1 档满血门闭通过\n";
    }

    // ══ T5620.d 12788 [4,50,6]：2499 < 2500 → p=50；24 局落地带 [4,20] ══
    {
        int landed = 0;
        for (int i = 0; i < kTrials; ++i) {
            const Outcome o = run_one(kJueDi, 2499, 1 + i * 104729u);
            CHECK_EQ(o.pct, 50, "T5620.d 审计 pct=50");
            if (o.result == static_cast<int>(ApplyAnomalyResult::SUCCESS)) {
                CHECK(o.fear, "T5620.d 掷中（SUCCESS）→ 对手必有害怕");
                ++landed;
            } else if (o.result == static_cast<int>(ApplyAnomalyResult::ROLL_FAILED)) {
                CHECK(!o.fear, "T5620.d 未掷中（ROLL_FAILED）→ 对手无害怕");
            } else {
                CHECK(false, "T5620.d 非预期 ApplyAnomalyResult=" + std::to_string(o.result));
            }
        }
        CHECK(landed >= 4 && landed <= 20,
              "T5620.d 159 p=50 24 局落地带 [4,20]，实测 " + std::to_string(landed));
        std::cout << "  [T5620.d] 1/4 档 p=50 落地 " << landed << "/" << kTrials << " 通过\n";
    }

    // ══ T5620.e 12788 边界：恰 2500（等于 1/4）→ 门闭 ══
    {
        const Outcome o = run_one(kJueDi, 2500);
        CHECK_EQ(o.result, -1, "T5620.e 边界（等于 1/4）门闭 → result=-1");
        CHECK(!o.fear, "T5620.e 边界门闭 → 无害怕");
        std::cout << "  [T5620.e] 1/4 边界门闭通过\n";
    }

    std::cout << "scenario_5620 PASS (J231 低体概率异常族 159)" << std::endl;
    return 0;
}
