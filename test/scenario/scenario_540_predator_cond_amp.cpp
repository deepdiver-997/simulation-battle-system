#include <utils/sbs_rand.h>
// 场景 540：E9 天敌/条件增伤族 —— effect 428（天敌附加固伤）+ 188（对手异常威力翻倍）
// + 1020（全属性+N，对手无提升翻倍）（moves_lib/e_predator_cond_amp.cpp，2026-09-29）
//
// 覆盖：
//   428  银河星爆 13534（[50]，特攻 140）：非天敌不附加 / 天敌（水 vs 草）附加 50 固伤
//        ——对拍口径同 scenario_220（foe hp = max − raw − rider，raw 读 ws 裸伤台账）。
//   188  极限冰结 13068（[]，特攻 70）：对手异常 → 威力翻倍 → 同种子对拍伤害恰 2 倍
//        （伤害对威力线性，其余随机同源同位）；对手无异常 = 控制组。
//        （消除子句跳过：per-stat 消除原语内核缺口，实现文件头记档。）
//   1020 笑傲光阴 25044（[1020,532,843]/[1,...]）：对手无提升 → 全属性 +2（翻倍）；
//        对手有提升 → +1（532/843 不触碰能力等级，断言不受污染）。
#include <cstdlib>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kYinhe = 13534;   // 银河星爆：428 [50]（特攻 140）
constexpr int kJixian = 13068;  // 极限冰结：188 []（特攻 70）
constexpr int kXiaoao = 25044;  // 笑傲光阴：1020 [1]（属性技）
constexpr int kHit = 10004;
constexpr int kWater = 2;       // 水（被草克 → 遇草为天敌）
constexpr int kGrass = 1;       // 草（克水）

ElfPet attacker(std::array<int, 5> skills, int atk, int element = -1) {
    ElfPet p = scenario::make_pet(0, "测试甲", skills, /*speed=*/200, atk, /*hp=*/5000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    if (element >= 0) {
        p.elementalAttributes[0] = element;
        p.elementalAttributes[1] = 0;
    }
    return p;
}

ElfPet foe(int element = -1, int hp = 1000000) {
    ElfPet p = scenario::make_pet(1, "对手", {kHit, kHit, kHit, kHit, kHit},
                                  /*speed=*/100, /*atk=*/10, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    if (element >= 0) {
        p.elementalAttributes[0] = element;
        p.elementalAttributes[1] = 0;
    }
    return p;
}

void run_round(ScenarioBattle& b, int skill0, int skill1) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, skill0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, skill1);
    b.drive();
}

// 同种子单回合 188 伤害采样：give_anomaly 决定对手是否带异常。
int sample_188_damage(bool give_anomaly, unsigned seed) {
    sbs_srand(seed);
    ScenarioBattle b(scenario::make_party(attacker({kJixian, kHit, kHit, kHit, kHit},
                                                   /*atk=*/1000)),
                     scenario::make_party(foe()));
    BattleContext& ctx = b.ctx();
    b.start();
    if (give_anomaly) {
        REQUIRE(apply_anomaly(&ctx, /*target=*/1, /*anomaly_id=*/0)
                == ApplyAnomalyResult::SUCCESS);
        REQUIRE(ctx.has_any_active_abnormal_status(1));
    }
    run_round(b, 0, 0);
    return 1000000 - ctx.getPet(1).hp;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== T1 428 非天敌：不附加 ==========
    {
        sbs_srand(7);
        ScenarioBattle b(scenario::make_party(attacker({kYinhe, kHit, kHit, kHit, kHit},
                                                       /*atk=*/500)),
                         scenario::make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, 0, 0);
        const int raw = ctx.ws.raw_attack_damage[0];
        CHECK(raw > 0, "T1 前提：攻击伤害落地");
        CHECK_EQ(ctx.getPet(1).hp, 1000000 - raw,
                 "T1 核心：非天敌（{0,0} vs {0,0} 克制 1.0）→ 无附加固伤");
        std::cout << "  [T1] 428 非天敌不附加 通过\n";
    }

    //========== T2 428 天敌：附加 50 固伤 ==========
    {
        sbs_srand(7);
        ScenarioBattle b(scenario::make_party(attacker({kYinhe, kHit, kHit, kHit, kHit},
                                                       /*atk=*/500, kWater)),
                         scenario::make_party(foe(kGrass)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, 0, 0);
        const int raw = ctx.ws.raw_attack_damage[0];
        CHECK(raw > 0, "T2 前提：攻击伤害落地");
        CHECK_EQ(ctx.getPet(1).hp, 1000000 - raw - 50,
                 "T2 核心：水遇草（天敌）→ 附加 50 点固定伤害");
        std::cout << "  [T2] 428 天敌附加 50 固伤 通过\n";
    }

    //========== T3 188 对手异常 → 威力翻倍（同种子倍率区间对拍）==========
    {
        // ⚠️ 不做"恰 2 倍"等式断言：apply_anomaly 内部抗性/持续掷点会消耗共享随机流，
        // 两组采样的伤害浮动 roll 不同位（实测 d_anomaly/d_clean=1131/559≈2.02）。
        // 威力翻倍在伤害对威力线性、浮动 ∈[0.85,1.0] 下的理论倍率区间 = [1.7, 2/0.85]；
        // 若翻倍未生效（比值≈1±0.18）或误三倍（≈3）都会落到区间外被抓住。
        const int d_clean = sample_188_damage(/*give_anomaly=*/false, 7);
        const int d_anomaly = sample_188_damage(/*give_anomaly=*/true, 7);
        std::cout << "  [T3] 采样 d_clean=" << d_clean << " d_anomaly=" << d_anomaly << "\n";
        CHECK(d_clean > 0, "T3 前提：控制组伤害落地");
        CHECK(d_anomaly * 1000 > d_clean * 1700,
              "T3 核心：对手异常 → 威力翻倍（倍率下界 1.7）");
        CHECK(d_anomaly * 1000 < d_clean * 2353,
              "T3 核心：倍率上界 2/0.85≈2.353（排除三倍/复合放大）");
        std::cout << "  [T3] 188 异常威力翻倍 通过\n";
    }

    //========== T4/T5 1020 全属性+N（对手无提升翻倍 / 有提升不翻倍）==========
    {
        sbs_srand(7);
        ScenarioBattle b(scenario::make_party(attacker({kXiaoao, kHit, kHit, kHit, kHit},
                                                       /*atk=*/10)),
                         scenario::make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, 0, 0);   // 对手无提升 → 翻倍 +2
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            CHECK_EQ(ctx.ability_levels[0][i], 2, "T4 核心：对手无提升 → 全属性+1 翻倍为 +2");
        }
        std::cout << "  [T4] 1020 无提升翻倍（+2） 通过\n";
    }
    {
        sbs_srand(7);
        ScenarioBattle b(scenario::make_party(attacker({kXiaoao, kHit, kHit, kHit, kHit},
                                                       /*atk=*/10)),
                         scenario::make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(stat_change(&ctx, /*target=*/1, /*stat=*/0, /*delta=*/2)
                == StatChangeResult::SUCCESS);   // 对手攻击 +2（处于提升状态）
        run_round(b, 0, 0);
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            CHECK_EQ(ctx.ability_levels[0][i], 1, "T5 核心：对手有提升 → 不翻倍，全属性+1");
        }
        std::cout << "  [T5] 1020 有提升不翻倍（+1） 通过\n";
    }

    std::cout << "scenario_540 全部通过" << std::endl;
    return 0;
}
