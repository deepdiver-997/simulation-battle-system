// 场景 2180 —— J124 条件输出族二（890/488/987，moves_lib/j_cond_output2.cpp）
//（2026-10-03，worktree ../agents/j124；号段 2180-2189 → T2180 一块五例）
//
// 断言口径（890/488 双局对照：比值 = 倍率 × [0.85,1/0.85] 随机带——J23 1080 同式）：
//   - 890 翻倍带 [170,235]%；提升用速槽+1（不碰攻防面板）；反向局 = 仅负槽
//     （特攻-1——载体是特殊技吃槽 1 → 红伤 ×2/3）：正确实现 d_neg≈0.67×norm，
//     若误把负槽当"提升"翻倍则 ≈1.33×norm——d_neg < d_norm 即判别（带不相交）；
//   - 488 增伤带 1.3×[0.85,1/0.85]=[110,153]%；门槛局 foe max=200<300（cast 时
//     满血即满足），atk=100 → 红伤 [100,117]，×1.3 后 [130,152] < 200 不致死；
//   - 987 精确差分：atk=1 红伤 1~3 + 附加 200 → [201,203]；烧伤局附加翻倍 400，
//     烧伤 tick = max/8（kMaxHp=800000 → 100000 整除，扣减法隔离——J23 97 同式）
//     → loss_total − 100000 ∈ [401,403]；载体纯链 30577（工单指定，避开 958 链）。
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

constexpr int kPaiDa  = 10004;   // 拍打：填充位
constexpr int kYueLou = 30239;   // 岳楼波动咒 [890]：特150 纯链（890 零参）
constexpr int kCaiLing = 33884;  // 彩铃幻音 [488]|[300,30]：特140 纯链
constexpr int kYanHuo = 30577;   // 炎火重击 [987]|[200,2]：物130 纯链（烧伤翻倍）
constexpr int kBurn   = static_cast<int>(AbnormalStatusId::Burn);          // 2
constexpr int kMaxHp  = 800000;  // 烧伤 tick = max/8 = 100000 整除（扣减法用）

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(int carrier, int atk) {
    ElfPet p = make_pet(1, "己方", {carrier, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, atk, kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(int hp = kMaxHp) {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, hp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

// 跑一局：self 用载体、对手 NONE，返回对手总掉血（按对手实际 max，兼容低血门槛局）。
int probe(int carrier, int atk, int foe_hp = kMaxHp) {
    ScenarioBattle b(make_party(self_pet(carrier, atk)), make_party(foe_pet(foe_hp)));
    b.start();
    BattleContext& ctx = b.ctx();
    const int hp0 = ctx.getPet(1).hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
    return hp0 - ctx.getPet(1).hp;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T2180.a 890 自身有提升→伤害翻倍（岳楼波动咒，双局对照）══════════════
    {
        const int d_norm = probe(kYueLou, 300);
        CHECK(d_norm > 50, "T2180.a 前置：无提升局正伤（实测 " + std::to_string(d_norm) + "）");
        int d_boost = 0;
        {
            ScenarioBattle b(make_party(self_pet(kYueLou, 300)), make_party(foe_pet()));
            b.start();
            BattleContext& ctx = b.ctx();
            REQUIRE(stat_change(&ctx, 0, 4, +1) == StatChangeResult::SUCCESS);  // 速+1（任一正槽）
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
            d_boost = kMaxHp - ctx.getPet(1).hp;
        }
        const int ratio_x100 = d_boost * 100 / d_norm;
        CHECK(ratio_x100 >= 170 && ratio_x100 <= 235,
              "T2180.a 890：有提升局翻倍（比值 " + std::to_string(ratio_x100) +
                  "%，带 170~235；无提升=" + std::to_string(d_norm) +
                  " 有提升=" + std::to_string(d_boost) + "）");
    }
    // ══ T2180.b 890 仅负槽不触发（特攻-1：载体是特殊技吃槽 1，伤害降但不得翻倍）══
    {
        const int d_norm = probe(kYueLou, 300);
        int d_neg = 0;
        {
            ScenarioBattle b(make_party(self_pet(kYueLou, 300)), make_party(foe_pet()));
            b.start();
            BattleContext& ctx = b.ctx();
            REQUIRE(stat_change(&ctx, 0, 1, -1) == StatChangeResult::SUCCESS);  // 特攻-1（负槽）
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
            d_neg = kMaxHp - ctx.getPet(1).hp;
        }
        CHECK(d_neg < d_norm,
              "T2180.b 890：仅负槽不翻倍（负槽局 " + std::to_string(d_neg) + " < 无提升局 " +
                  std::to_string(d_norm) + "；若误翻倍会 ≈1.33×norm）");
    }
    // ══ T2180.c 488 对手 hp<300 → +30%（彩铃幻音，双局对照）═════════════════
    {
        const int d_norm = probe(kCaiLing, 100);
        CHECK(d_norm > 50, "T2180.c 前置：满血局正伤（实测 " + std::to_string(d_norm) + "）");
        const int d_low = probe(kCaiLing, 100, /*foe_hp=*/200);
        const int ratio_x100 = d_low * 100 / d_norm;
        CHECK(ratio_x100 >= 110 && ratio_x100 <= 153,
              "T2180.c 488：低血局 +30%（比值 " + std::to_string(ratio_x100) +
                  "%，带 110~153；满血=" + std::to_string(d_norm) +
                  " 低血=" + std::to_string(d_low) + "）");
    }
    // ══ T2180.d 987 无异常：附加 200 固伤（炎火重击，atk=1 红伤 1~2）═════════
    {
        const int loss = probe(kYanHuo, 1);
        CHECK(loss >= 201 && loss <= 203,
              "T2180.d 987：红伤 1~3 + 附加 200（实测 " + std::to_string(loss) + "）");
    }
    // ══ T2180.e 987 对手烧伤：附加翻倍 400（烧伤 tick 扣减法隔离）════════════
    {
        ScenarioBattle b(make_party(self_pet(kYanHuo, 1)), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        (void)apply_anomaly(&ctx, 1, kBurn, /*duration_rounds=*/9, /*actor=*/0);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        const int loss_total = kMaxHp - ctx.getPet(1).hp;
        const int hit = loss_total - kMaxHp / 8;   // 减去烧伤 tick（=100000）
        CHECK(hit >= 401 && hit <= 403,
              "T2180.e 987：烧伤局附加翻倍 400（扣 tick 后实测 " + std::to_string(hit) + "）");
    }

    std::cout << "scenario_2180 PASS (J124 条件输出族二 890/488/987)" << std::endl;
    return 0;
}
