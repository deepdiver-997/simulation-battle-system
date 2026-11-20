// 场景 4410 —— K285 烧伤门回血/固伤族（618，soul_lib/k285_burngate_heal_fixed.cpp）
//（2026-10-21，worktree ../agents/k285；号段 4410-4419 → T4410-T4411 两块）
//
// 断言口径（618「若对手处于烧伤状态时则每回合恢复200HP；若对手不处于烧伤状态则
//             每回合附加200点固定伤害」）：
//   - T4410 分支①：对手**烧伤** → 印主每回合回血 200（压血到 5000 防满血封顶）。
//   - T4411 分支②：对手**不烧伤** → 对手每回合掉 200 点固定伤害（粉伤 FIXED，
//     不浮动，精确）；印主不回血。
//
// ⚠️ 场景构造口径：按 **DB args 原样**传 {2,200,200}（不补 {0,0} 前缀）——实现侧
//   走 owned_int_args 原序读（K257 实现头注「行参读取路径」记档）。
#include <iostream>
#include <vector>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kJi = 10001;     // 撞击：攻击载体
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);   // 2
constexpr int kMaxHp = 99999;

ElfPet mark_pet(std::vector<int> args = {2, 200, 200}) {
    ElfPet p = make_pet(1, "印主", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/300, /*atk=*/1, /*hp=*/kMaxHp,
                        /*soul_mark=*/SoulMark(618, "K285 魂印", "",
                                               EffectArgs(std::move(args))));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// 对照组印主（无魂印）——用于扣除"烧伤自身每回合扣血"的基线。
ElfPet plain_mark_pet() {
    ElfPet p = make_pet(1, "印主", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/300, /*atk=*/1, /*hp=*/kMaxHp);
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

// 跑一回合：可选给对手预铺烧伤 + 压印主血量；返回（印主回血, 对手掉血）。
struct RoundDelta {
    int self_heal = 0;
    int foe_loss = 0;
};

RoundDelta one_round(ScenarioBattle& b, bool burn_foe, int self_hp = 0) {
    BattleContext& ctx = b.ctx();
    if (self_hp > 0) {
        ctx.getPet(0).hp = self_hp;   // 压体力防满血封顶
    }
    if (burn_foe) {
        apply_anomaly(&ctx, 1, kBurn, 3, /*actor=*/0);
        REQUIRE(ctx.has_active_abnormal_status(1, kBurn));
    }
    const int self0 = ctx.getPet(0).hp;
    const int foe0 = ctx.getPet(1).hp;
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
    RoundDelta d;
    d.self_heal = ctx.getPet(0).hp - self0;
    d.foe_loss = foe0 - ctx.getPet(1).hp;
    return d;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T4410 分支①：对手烧伤 → 自己回血 200 + 不附加固伤 ══════════════════
    // ⚠️ 对手掉血含**烧伤自身**每回合扣最大体力 1/8（≈12500）→ 固伤分支用
    //    **双局差值**判定（对照组无魂印、同样烧伤 → 基线只有烧伤扣血）。
    {
        int ctrl_foe_loss = 0;
        {
            ScenarioBattle b(make_party(plain_mark_pet()), make_party(plain_pet()));
            b.start();
            ctrl_foe_loss = one_round(b, /*burn_foe=*/true, /*self_hp=*/5000).foe_loss;
        }
        int mark_foe_loss = 0;
        int self_heal = 0;
        {
            ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
            b.start();
            const RoundDelta d = one_round(b, /*burn_foe=*/true, /*self_hp=*/5000);
            mark_foe_loss = d.foe_loss;
            self_heal = d.self_heal;
        }
        CHECK_EQ(self_heal, 200,
                 "T4410 618：对手处于烧伤状态 → 自身每回合恢复 200 HP");
        CHECK_EQ(mark_foe_loss, ctrl_foe_loss,
                 "T4410 618：对手烧伤分支 → 不附加固定伤害（与无印对照组掉血相同）");
        std::cout << "  [T4410] 烧伤门回血 通过（heal=" << self_heal
                  << " foe=" << mark_foe_loss << " vs ctrl=" << ctrl_foe_loss << "）\n";
    }

    // ══ T4411 分支②：对手不烧伤 → 对手掉 200 固定伤 ═════════════════════════
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        const RoundDelta d = one_round(b, /*burn_foe=*/false, /*self_hp=*/5000);
        CHECK_EQ(d.foe_loss, 200,
                 "T4411 618：对手不处于烧伤状态 → 对手每回合附加 200 点固定伤害");
        CHECK_EQ(d.self_heal, 0,
                 "T4411 618：非烧伤分支 → 自身不恢复体力");
        std::cout << "  [T4411] 非烧伤门固伤 通过（foe_loss=" << d.foe_loss << "）\n";
    }

    // 两个金额独立消费；缺失才用 200，显式零不得被 >0 读取守卫吞掉。
    struct AmountCase {
        std::vector<int> args;
        int heal;
        int fixed;
    };
    const std::vector<AmountCase> cases{
        {{2, 137, 283}, 137, 283}, {{0, 137, 283}, 137, 283},
        {{2, 0, 283}, 0, 283}, {{2, 137, 0}, 137, 0},
        {{}, 200, 200}, {{2}, 200, 200}, {{2, 137}, 137, 200},
    };
    int burn_baseline = 0;
    {
        ScenarioBattle b(make_party(plain_mark_pet()), make_party(plain_pet()));
        b.start();
        burn_baseline = one_round(b, true, 5000).foe_loss;
    }
    for (const auto& c : cases) {
        for (const bool burned : {false, true}) {
            // 同号场下行设为零：只读实际 host_slot=1，不能 find 到槽 0 就拿错参。
            auto party = make_party(plain_mark_pet());
            party[0] = mark_pet({2, 0, 0});
            party[1] = mark_pet(c.args);
            ScenarioBattle b(party, make_party(plain_pet()));
            b.ctx().on_stage[0] = 1;
            b.start();
            const RoundDelta d = one_round(b, burned, 5000);
            CHECK_EQ(d.self_heal, burned ? c.heal : 0,
                     "618：烧伤分支只恢复原序 db[1]，含零和缺失");
            CHECK_EQ(d.foe_loss, burned ? burn_baseline : c.fixed,
                     "618：非烧伤只扣原序 db[2]，烧伤只扣异常自身基线");
            if (!burned && c.fixed > 0) {
                const auto& hit = b.ctx().last_landed_hit[1][b.ctx().on_stage[1]];
                CHECK(hit.valid && hit.kind == static_cast<int>(DamageKind::FIXED),
                      "618：观测粉伤真正落体力的记录，而非其他伤害通道的死亡归因槽");
                CHECK_EQ(hit.actor, 0, "618：固定伤害落血记录保留印主 actor");
                CHECK_EQ(hit.amount, c.fixed, "618：来源记录对应本次固定伤害");
            }
        }
    }

    // 原语约束：免粉、部分护罩吸收、封回血，均用真实 ROUND_START 节点驱动。
    {
        ScenarioBattle b(make_party(mark_pet({2, 137, 283})), make_party(plain_pet()));
        b.start();
        b.ctx().grant_immunity(1, ImmunityType::PINK_DAMAGE, coverage_all(), 0, 0, 0);
        const RoundDelta d = one_round(b, false, 5000);
        CHECK_EQ(d.foe_loss, 0, "618：免粉挡住固伤");
        CHECK_EQ(d.self_heal, 0, "618：固伤被免不改走回血分支");
    }
    {
        ScenarioBattle b(make_party(mark_pet({2, 137, 283})), make_party(plain_pet()));
        b.start();
        b.ctx().getPet(1).hood_bank_.add(0, 100, 618, 0);
        const RoundDelta d = one_round(b, false, 5000);
        CHECK_EQ(d.foe_loss, 183, "618：283 固伤先消费 100 护罩再扣 183 HP");
        CHECK_EQ(b.ctx().getPet(1).hood_bank_.total(), 0, "618：护罩确实被消费");
    }
    {
        ScenarioBattle b(make_party(mark_pet({2, 137, 283})), make_party(plain_pet()));
        b.start();
        b.ctx().grant_immunity(0, ImmunityType::HEAL_BLOCK, coverage_all(), 0, 0, 0);
        const RoundDelta d = one_round(b, true, 5000);
        CHECK_EQ(d.self_heal, 0, "618：烧伤分支回血受封回血限制");
        CHECK_EQ(d.foe_loss, burn_baseline, "618：封回血不让互斥分支串成固伤");
    }

    // 臣服的粉伤来源守卫依赖 actor；改回 fixed_damage(actor=-1) 必须被本断言捕获。
    {
        ScenarioBattle b(make_party(mark_pet({2, 137, 283})), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        constexpr int submission = static_cast<int>(AbnormalStatusId::Submission);
        apply_anomaly(&ctx, 0, submission, 3, 1);
        REQUIRE(ctx.has_active_abnormal_status(0, submission));
        const RoundDelta d = one_round(b, false, 5000);
        CHECK_EQ(d.foe_loss, 0, "618：印主臣服时有来源粉伤被挡，不得绕成无源固伤");
        CHECK_EQ(d.self_heal, 0, "618：来源守卫挡伤不会切到回血分支");
    }

    std::cout << "scenario_4410 PASS (K285 烧伤门回血/固伤族)" << std::endl;
    return 0;
}
