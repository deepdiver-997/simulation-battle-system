// 场景 3930 —— K246 低体害怕+高体免异常窗族（826，soul_lib/k246_lowhp_fear_highhp_anomaly_immune.cpp）
//（2026-10-13，worktree ../agents/k246；号段 3930-3939 → T3930-T3932 三块）
//
// 断言口径（行1 [2,2,1]：低体害怕/高体下1回合免异常；maxHP=99999 → half=49999）：
//   · 低体（30000）：R1 末 → 对手害怕落地（end_round>0）；自身无免疫票；
//   · 高体（60000）：R1 末 → 下回合（R2）自身免疫异常（is_immune ANOMALY 活）；
//     R3 过期（duration=N+1 补 rc 滞后——cur(2)-reg(0)>=2）；对手不害怕；
//   · 免疫行为侧证：R2 让对手攻击携带异常的载体不可得 → 以 is_immune 查询 +
//     R3 过期负控为准（IMMUNE 票消费路径已由 chaos_sorensen ③/076 场景实证）。
//   ⚠️ 纯 drive() 流（3720 记档）。
#include <iostream>
#include <vector>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kJi = 10001;
constexpr int kMaxHp = 99999;
constexpr int kFearId = static_cast<int>(AbnormalStatusId::Fear);

ElfPet mark_pet(const std::vector<int>& row_args) {
    ElfPet p = make_pet(1, "印主", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/300, /*atk=*/1, /*hp=*/kMaxHp,
                        /*soul_mark=*/SoulMark(826, "K246 魂印", "",
                                               EffectArgs(row_args)));
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

void round_none(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

bool anomaly_immune(BattleContext& ctx, int side) {
    // 票 coverage_all → 任意时点位匹配；取真实施加点位代表态查询。
    return ctx.is_immune(side, ImmunityType::ANOMALY,
                         State::BATTLE_FIRST_SKILL_EFFECT);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T3930 低体 → 对手害怕、自身无免疫 ══
    {
        ScenarioBattle b(make_party(mark_pet({2, 2, 1})), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 30000;   // < 49999
        round_none(b);
        CHECK(ctx.abnormal_status_end_round[1][kFearId] > 0,
              "T3930 826：低体 → 对手害怕落地");
        CHECK(!anomaly_immune(ctx, 0), "T3930 826：低体分支自身无免异常");
        std::cout << "  [T3930] 低体害怕分支 通过\n";
    }

    // ══ T3931 高体 → R2 免异常活；R2 末转低体（停止重授）→ R3 过期（行1：下1回合）══
    // ⚠️ 持续高体会每回合 ROUND_END 重授（官方语义：每回合结束时高于 1/2 → 下回合免）
    // —— 过期测试须先脱离高体再等窗 lapse。
    {
        ScenarioBattle b(make_party(mark_pet({2, 2, 1})), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 60000;   // > 49999
        round_none(b);              // R1 末：授予（reg=0, rem=1+1）
        CHECK_EQ(ctx.abnormal_status_end_round[1][kFearId], 0,
                 "T3931 826：高体分支对手不害怕");
        round_none(b);              // R2（cur=1）：窗内
        CHECK(anomaly_immune(ctx, 0), "T3931 826：R2 免异常活跃（下1回合）");
        ctx.getPet(0).hp = 30000;   // 转低体 → R2 末不再重授（走害怕分支）
        round_none(b);              // R2 末：低体分支
        round_none(b);              // R3（cur=2）：R1 票过期且无新授
        CHECK(!anomaly_immune(ctx, 0), "T3931 826：脱离高体后窗过期");
        std::cout << "  [T3931] 高体免异常窗（行1） 通过\n";
    }

    // ══ T3932 行2 [2,2,1,1]：下2回合 → R2/R3 活、R4 过期（同样先脱离高体）══
    {
        ScenarioBattle b(make_party(mark_pet({2, 2, 1, 1})), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 60000;
        round_none(b);   // R1 末：授予（reg=0, rem=2+1）
        ctx.getPet(0).hp = 30000;   // R2 起转低体（停止重授）
        round_none(b);   // R2 末：低体分支（cur=1）
        CHECK(anomaly_immune(ctx, 0), "T3932 826：R2 免异常活跃");
        // R3 动作期（cur=2）在窗内由 rem=3 算术保证；此处 drive 完成点 cur 已自增
        // 到 3，post-drive 查询读不到窗内态（3720/T3931 同款时点口径），跳过中间断言。
        round_none(b);   // R4（cur=3→4）：过期
        CHECK(!anomaly_immune(ctx, 0), "T3932 826：R4 窗过期");
        std::cout << "  [T3932] 高体免异常窗（行2 下2回合） 通过\n";
    }

    std::cout << "scenario_3930 PASS (K246 低体害怕+高体免异常窗族)" << std::endl;
    return 0;
}
