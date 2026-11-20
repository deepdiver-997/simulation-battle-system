#include <utils/sbs_rand.h>
// 场景 990 —— J14 概率未触发族三（966/1274/1248/1488/1024，moves_lib/j_prob_fallback3.cpp）
//（2026-10-02，worktree ../agents/j14；号段 990-999 → T990-T994 五块）
//
// 断言口径（J3 场景 900 同款）：
//   - 主干 chance=100 载体必施加；"未触发"用对手 ANOMALY 次免票强制（!anomaly_landed
//     统一口径），兜底全确定性断言；
//   - 1274 概率翻倍走 **审计槽**（plugin_storage[990127410+owner]，F9 782 口径）无随机
//     断言申报值；翻倍后 eff=100 → 落地本身也是确定性断言；
//   - 1024 的 50% 掷点用 **种子校准** 钉死两半：首 sbs_rand()%100 ≥50 的种子=必歪（兜底粉伤
//     落地）、<50 的种子=必中（免攻票）。载体 25053 是属性技（无伤害相位）、同链 191/478
//     无 rand 消费 → 掷点前 rand 序列干净。若引擎改动插入 rand 消费者，此块会确定性
//     失败（非假绿），届时按偏移重校准。
//   - 1488 天敌门槛 = 水(2) vs 草(1) 铺设（scenario_540 同款）。
#include <any>
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

// ── 载体技（切片已逐条与 side_effect/arg 核对）──
constexpr int kPaiDa   = 10004;   // 拍打：物40（对攻/填充）
constexpr int kJiangWan = 28015;   // 酱丸启航 [966]|[100,2,1]      属（966 chance=100）
constexpr int kChangAn = 28395;   // 常黯之空 [1274]|[50,6]        属（1274 chance=50）
constexpr int kGuangYao = 28676;  // 光耀坚铠 [1248]|[100,6]       属（1248 chance=100）
constexpr int kXueMei  = 35196;   // 雪梅之傲 [1488]|[100,5]       物140（1488 chance=100）
constexpr int kMingJie = 25053;   // 冥界王权 [191,1024,478]|[4,3,50,100,2] 属（1024 [3,50,100]）

constexpr int kBurn      = static_cast<int>(AbnormalStatusId::Burn);       // 2
constexpr int kFear      = static_cast<int>(AbnormalStatusId::Fear);       // 6
constexpr int kFrostbite = static_cast<int>(AbnormalStatusId::Frostbite);  // 5
constexpr int kPoison    = static_cast<int>(AbnormalStatusId::Poison);     // 1

constexpr int kWater = 2;   // 水（被草克 → 遇草为天敌）
constexpr int kGrass = 1;   // 草（克水）

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300, int atk = 1, int hp = 99999) {
    ElfPet p = make_pet(1, "掷点者", skills, speed, atk, hp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100, int atk = 1, int hp = 99999) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, hp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

int level_sum(BattleContext& ctx, int side) {
    int sum = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        sum += ctx.ability_levels[side][i];
    }
    return sum;
}

int audit_1274(BattleContext& ctx, int owner) {
    const auto it = ctx.plugin_storage.find(990127410 + owner);
    return it == ctx.plugin_storage.end() ? -1 : std::any_cast<int>(it->second);
}

// setup 直授：对手 ANOMALY 次免票（counts）——"未触发"强制器（场景 900 同款）。
void grant_foe_anomaly_counts_ticket(BattleContext& ctx, int counts) {
    ctx.grant_immunity(1, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                       /*duration_rounds=*/0, /*source_id=*/9901,
                       /*soul_immunity=*/false, EffectScope::ON_STAGE,
                       /*counts=*/counts, /*source_effect_id=*/9901);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T990 966 主干/兜底（28015 酱丸启航 [100,2,1]）══
    {
        {   // (a) chance=100 主干落地，兜底不触发
            ScenarioBattle b(make_party(self_pet({kJiangWan, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 28015
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(ctx.has_active_abnormal_status(1, kBurn),
                  "T990.a 966 主干：100% 烧伤落地");
            CHECK_EQ(level_sum(ctx, 0), 0, "T990.a 966：主干触发则兜底（全属性+1）不触发");
        }
        {   // (b) 对手 ANOMALY 次免票 → 未触发 → 兜底自身全属性+1
            ScenarioBattle b(make_party(self_pet({kJiangWan, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            grant_foe_anomaly_counts_ticket(ctx, /*counts=*/1);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(!ctx.has_active_abnormal_status(1, kBurn),
                  "T990.b 966：主干被次免票挡下");
            CHECK_EQ(level_sum(ctx, 0), 6, "T990.b 966 兜底：自身六槽全 +1（含命中）");
        }
        std::cout << "  [T990] 966 主干/兜底 通过\n";
    }

    // ══ T991 1274 翻倍（28395 常黯之空 [50,6]，审计槽口径）══
    {
        {   // (a) 对手无下降：申报 50（不判落地——50% 非确定）
            ScenarioBattle b(make_party(self_pet({kChangAn, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(audit_1274(ctx, 0), 50, "T991.a 1274 审计槽：无下降申报原值 50");
        }
        {   // (b) 对手有下降：翻倍 → 100 → 害怕必落地
            ScenarioBattle b(make_party(self_pet({kChangAn, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            REQUIRE(stat_change(&ctx, 1, 0, -1) == StatChangeResult::SUCCESS);   // 对手攻-1
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(audit_1274(ctx, 0), 100, "T991.b 1274 审计槽：有下降翻倍 100");
            CHECK(ctx.has_active_abnormal_status(1, kFear),
                  "T991.b 1274：翻倍至 100% → 害怕必落地");
        }
        std::cout << "  [T991] 1274 翻倍+审计槽 通过\n";
    }

    // ══ T992 1248 异常门槛（28676 光耀坚铠 [100,6]）══
    {
        {   // (a) 对手无异常 → 门槛不进（chance=100 也不施加）
            ScenarioBattle b(make_party(self_pet({kGuangYao, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(!ctx.has_active_abnormal_status(1, kFear),
                  "T992.a 1248 门槛：对手无异常不施加");
        }
        {   // (b) 对手中毒（setup 直铺）→ 100% 害怕
            ScenarioBattle b(make_party(self_pet({kGuangYao, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            REQUIRE(try_apply_anomaly(&ctx, 1, kPoison, /*duration=*/3, /*actor=*/0));
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(ctx.has_active_abnormal_status(1, kFear),
                  "T992.b 1248：对手处于异常 → 100% 害怕");
        }
        std::cout << "  [T992] 1248 异常门槛 通过\n";
    }

    // ══ T993 1488 天敌门槛（35196 雪梅之傲 [100,5]，水 vs 草铺设）══
    {
        {   // (a) 非天敌（双方无系）→ 不施加
            ScenarioBattle b(make_party(self_pet({kXueMei, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(!ctx.has_active_abnormal_status(1, kFrostbite),
                  "T993.a 1488 门槛：非天敌不施加");
        }
        {   // (b) 天敌（我水、对手草）→ 100% 冻伤
            ElfPet me = self_pet({kXueMei, kPaiDa, kPaiDa, kPaiDa, kPaiDa});
            me.elementalAttributes[0] = kWater;
            me.elementalAttributes[1] = 0;
            ElfPet foe = foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa});
            foe.elementalAttributes[0] = kGrass;
            foe.elementalAttributes[1] = 0;
            ScenarioBattle b(make_party(std::move(me)), make_party(std::move(foe)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(ctx.has_active_abnormal_status(1, kFrostbite),
                  "T993.b 1488：遇天敌 → 100% 冻伤");
        }
        std::cout << "  [T993] 1488 天敌门槛 通过\n";
    }

    // ══ T994 1024 免疫窗两半（25053 冥界王权 → 1024=[3,50,100]）══
    // 种子校准：载体属性技 + 同链无 rand 消费 → 掷点=校准后首个 sbs_rand()。
    {
        int seed_fail = -1;
        int seed_hit = -1;
        for (int s = 1; s < 100000 && (seed_fail < 0 || seed_hit < 0); ++s) {
            sbs_srand(s);
            const int r = sbs_rand() % 100;
            if (seed_fail < 0 && r >= 50) {
                seed_fail = s;
            }
            if (seed_hit < 0 && r < 50) {
                seed_hit = s;
            }
        }
        REQUIRE(seed_fail > 0);
        REQUIRE(seed_hit > 0);
        {   // (a) 掷歪：受伤照常，ROUND_END 兜底 = 100% × 受伤量 FIXED 反打对手
            ScenarioBattle b(make_party(self_pet({kMingJie, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*speed=*/100, /*atk=*/3000)));
            b.start();
            BattleContext& ctx = b.ctx();
            sbs_srand(seed_fail);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 25053
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 拍打
            b.drive();
            const int self_loss = 99999 - ctx.getPet(0).hp;
            const int foe_loss = 99999 - ctx.getPet(1).hp;
            CHECK(self_loss > 0, "T994.a 前置：掷歪回合对手攻击落地（受伤 " +
                                     std::to_string(self_loss) + "）");
            CHECK_EQ(foe_loss, self_loss,
                     "T994.a 1024 兜底：ROUND_END 反打 100%×受伤量（对手失 " +
                         std::to_string(foe_loss) + " vs 自身失 " +
                         std::to_string(self_loss) + "）");
        }
        {   // (b) 掷中：免攻票覆盖当回合 → 不掉血、无兜底
            ScenarioBattle b(make_party(self_pet({kMingJie, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*speed=*/100, /*atk=*/3000)));
            b.start();
            BattleContext& ctx = b.ctx();
            sbs_srand(seed_hit);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, 99999, "T994.b 1024：掷中免攻（对手攻击被免疫）");
            CHECK_EQ(ctx.getPet(1).hp, 99999, "T994.b 1024：掷中回合无兜底粉伤");
        }
        std::cout << "  [T994] 1024 免疫窗掷点两半 通过 (seed " << seed_fail << "/"
                  << seed_hit << ")\n";
    }

    std::cout << "scenario_990 PASS (J14 概率未触发族三)" << std::endl;
    return 0;
}
