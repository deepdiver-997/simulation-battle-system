#include <utils/sbs_rand.h>
// 场景 7150 —— K370 免疫降属+条件烧伤+PP分档族（1231，soul_lib/k370_immune_condburn_pp.cpp）
//（2026-10-24，worktree ../agents/k370；号段 7150-7159 → T7150.a-h 八块）
//
// 断言口径（1231「天生免疫能力下降；回合开始对手不处于能力下降→当回合所有技能
// P%令对手烧伤；体力分档对手全技能 PP-1/-2（boss无效）」）：
//   T7150.a 免疫能力下降：持印者 stat_drop 不落地；无印对照正常落地。
//   T7150.b/c/d PP 分档：满血→-1；1/4 血→-2；恰 1/2 血→无消耗（严格档无归属）。
//   T7150.e 门开→烧伤：对手无降属 + 100% 档（row1478 实战行）→ 用技即烧伤（确定性）。
//   T7150.f 门闭→不烧：对手预置降属 → 用技不烧。
//   T7150.g 跨回合翻转：R1 门闭不烧 → 抬回等级 → R2 门开烧（回合键旗标每回合重评）。
//   T7150.h 50% 档统计：30 局独立种子（row944），落地数带 [4,26]（期望 15）。
// ⚠️ max HP=100000（偶数）：保证"恰 1/2"档可精确构造。
// ⚠️ 烧伤观测面 = ctx.has_active_abnormal_status(1, 2)。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaDa = 10004;   // 拍打：物攻 40，max_pp=35（atk=10 打 def100 仅蹭血）
constexpr int kMaxHp = 100000;

SoulMark make_mark(int pct) {
    // DB args 原序：[概率%, 烧伤码2, 低体PP2, 高体PP1, （尾段记档 2,2,1）]
    SoulMark sm(1231, "K370 英普瑞斯", "test",
                EffectArgs(std::vector<int>{pct, 2, 2, 1, 2, 2, 1}));
    sm.monster_id = 3657;
    return sm;
}

ElfPet mark_pet(int pct) {
    ElfPet p = make_pet(1, "英普瑞斯", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/300, /*atk=*/10, kMaxHp, make_mark(pct));
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
    return p;
}

ElfPet plain_pet(bool with_mark = false) {
    ElfPet p = make_pet(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/100, /*atk=*/10, kMaxHp);
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
    return p;
}

void mark_skill_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

void both_none(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

void check_all_pp(BattleContext& ctx, int side, int expect, const std::string& tag) {
    for (std::size_t i = 0; i < ctx.getPet(side).skills.size(); ++i) {
        CHECK_EQ(ctx.getPet(side).skills[i].pp, expect,
                 tag + "：side" + std::to_string(side) + " 技能 " +
                     std::to_string(i) + " PP 应为 " + std::to_string(expect));
    }
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7150.a 免疫能力下降 + 无印对照 ══
    {
        {
            ScenarioBattle b(make_party(mark_pet(100)), make_party(plain_pet()));
            b.start();
            BattleContext& ctx = b.ctx();
            const StatDropResult r = stat_drop(&ctx, /*target=*/0, /*攻击=*/0,
                                               /*amount=*/2, /*actor=*/1);
            CHECK(r != StatDropResult::SUCCESS,
                  "T7150.a 持印者免能力下降（STAT_DROP 票拦下）");
            CHECK_EQ(ctx.ability_level(0, 0), 0, "T7150.a 攻击等级未被压低");
        }
        {
            ScenarioBattle b(make_party(mark_pet(100)), make_party(plain_pet()));
            b.start();
            // 对照：给对手侧造同款免票环境不存在——直接验无票可落：对对手 stat_drop
            ScenarioBattle c(make_party(mark_pet(100)), make_party(plain_pet()));
            c.start();
            BattleContext& ctx = c.ctx();
            const StatDropResult r = stat_drop(&ctx, /*target=*/1, /*攻击=*/0,
                                               /*amount=*/2, /*actor=*/0);
            CHECK_EQ(r, StatDropResult::SUCCESS, "T7150.a 对照：无印者正常被降");
            CHECK_EQ(ctx.ability_level(1, 0), -2, "T7150.a 对照：攻击 -2 落地");
        }
        std::cout << "  [T7150.a] 免疫能力下降+对照 通过" << std::endl;
    }

    // ══ T7150.b PP 高体档：满血 → 对手全技能 PP-1 ══
    {
        ScenarioBattle b(make_party(mark_pet(100)), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        const int pp0 = ctx.getPet(1).skills[0].pp;
        both_none(b);
        check_all_pp(ctx, 1, pp0 - 1, "T7150.b");
        std::cout << "  [T7150.b] 高体档 PP-1 通过" << std::endl;
    }

    // ══ T7150.c PP 低体档：1/4 血 → 对手全技能 PP-2 ══
    {
        ScenarioBattle b(make_party(mark_pet(100)), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = kMaxHp / 4;
        const int pp0 = ctx.getPet(1).skills[0].pp;
        both_none(b);
        check_all_pp(ctx, 1, pp0 - 2, "T7150.c");
        std::cout << "  [T7150.c] 低体档 PP-2 通过" << std::endl;
    }

    // ══ T7150.d PP 等号档：恰 1/2 血 → 无消耗（严格档无归属）══
    {
        ScenarioBattle b(make_party(mark_pet(100)), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = kMaxHp / 2;
        const int pp0 = ctx.getPet(1).skills[0].pp;
        both_none(b);
        check_all_pp(ctx, 1, pp0, "T7150.d");
        std::cout << "  [T7150.d] 等号档无归属 通过" << std::endl;
    }

    // ══ T7150.e 门开→烧伤：对手无降属 + 100% 档 ══
    {
        ScenarioBattle b(make_party(mark_pet(100)), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK(!ctx.has_active_abnormal_status(1, 2), "T7150.e 前置：对手无烧伤");
        mark_skill_round(b);
        CHECK(ctx.has_active_abnormal_status(1, 2),
              "T7150.e 门开 + 100% 档：用技即烧伤（实测 has_burn=" +
                  std::string(ctx.has_active_abnormal_status(1, 2) ? "1" : "0") + "）");
        std::cout << "  [T7150.e] 门开烧伤 通过" << std::endl;
    }

    // ══ T7150.f 门闭→不烧：对手预置降属 ══
    {
        ScenarioBattle b(make_party(mark_pet(100)), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        const StatDropResult r = stat_drop(&ctx, 1, 0, 2, 0);   // 对手攻击 -2
        CHECK_EQ(r, StatDropResult::SUCCESS, "T7150.f 前置：对手被降");
        mark_skill_round(b);
        CHECK(!ctx.has_active_abnormal_status(1, 2),
              "T7150.f 门闭：对手处于能力下降 → 用技不烧");
        std::cout << "  [T7150.f] 门闭不烧 通过" << std::endl;
    }

    // ══ T7150.g 跨回合翻转：R1 门闭不烧 → 抬回 → R2 门开烧 ══
    {
        ScenarioBattle b(make_party(mark_pet(100)), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        (void)stat_drop(&ctx, 1, 0, 2, 0);      // R1：对手降属 → 门闭
        mark_skill_round(b);
        CHECK(!ctx.has_active_abnormal_status(1, 2), "T7150.g R1 门闭不烧");
        CHECK_EQ(stat_change(&ctx, 1, 0, 2), StatChangeResult::SUCCESS,
                 "T7150.g 抬回攻击等级");       // 等级回 0 → R2 门开
        CHECK_EQ(ctx.ability_level(1, 0), 0, "T7150.g 等级已清");
        mark_skill_round(b);
        CHECK(ctx.has_active_abnormal_status(1, 2), "T7150.g R2 门开烧（回合键旗标重评）");
        std::cout << "  [T7150.g] 跨回合翻转 通过" << std::endl;
    }

    // ══ T7150.h 50% 档统计：30 局独立种子，落地数带 [4,26]（期望 15）══
    {
        int burned = 0;
        for (int i = 0; i < 30; ++i) {
            sbs_srand(7150 + i);
            ScenarioBattle b(make_party(mark_pet(50)), make_party(plain_pet()));
            b.start();
            mark_skill_round(b);
            if (b.ctx().has_active_abnormal_status(1, 2)) {
                ++burned;
            }
        }
        CHECK(burned >= 4 && burned <= 26,
              "T7150.h 50% 档统计带落地 " + std::to_string(burned) +
                  "/30（期望≈15，带 [4,26]）");
        std::cout << "  [T7150.h] 50% 档统计带 通过（" << burned << "/30）" << std::endl;
    }

    std::cout << "scenario_7150 PASS (1231 免疫降属+条件烧伤+PP分档 2 行)" << std::endl;
    return 0;
}
