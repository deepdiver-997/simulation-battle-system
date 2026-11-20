#include <utils/sbs_rand.h>
// 场景 7090 —— K364 对手PP消耗+免弱族（639，soul_lib/k364_foe_pp_drain.cpp）
//（2026-10-24，worktree feat/k364；号段 7090-7099）
//
// 断言口径（639「每回合，对手所有技能PP减少N点（boss无效）;免疫能力下降」）：
//   T7090.a row315 [1,1]：R1 结束对手全技能 PP 恰 -1（双方 NONE，排除出手消耗混淆）。
//   T7090.b row438 [2,1]：PP 恰 -2（行参驱动数值）。
//   T7090.c 免疫能力下降：持印者 stat_drop 不落地（等级不变）；无印对照正常落地。
//   T7090.d 累计窗：R1/R2 各 -1（无"每回合重置"，累计消耗）。
//   T7090.e 只消耗对手：持印者自身 PP 不动。
//   T7090.f 钳 0 + 无限 PP 不参与：压到低 PP 的技能钳 0（pp=max(0, pp-N)）。
//
// ⚠️ SoulMark args 按 DB 原样 {1,1}/{2,1}（db_at 原序读，K257 起夜跑口径）。
// ⚠️ PP 观测面 = ctx.getPet(side).skills[i].pp 直读（pp_reduce 原语落点）。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaDa = 10004;   // 拍打：物攻 40，max_pp=35
constexpr int kMaxHp = 99999;

SoulMark make_mark(int drain) {
    // DB args 原序：[每回合对手 PP 消耗, 免疫能力下降旗]
    SoulMark sm(639, "K364 英普力亚", "test", EffectArgs(std::vector<int>{drain, 1}));
    sm.monster_id = 2898;
    return sm;
}

ElfPet mark_pet(int drain, bool with_mark = true) {
    ElfPet p = with_mark
        ? make_pet(1, "英普力亚", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                   /*speed=*/300, /*atk=*/10, kMaxHp, make_mark(drain))
        : make_pet(1, "裸英普力亚", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                   /*speed=*/300, /*atk=*/10, kMaxHp);
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
    return p;
}

ElfPet plain_pet() {
    ElfPet p = make_pet(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/100, /*atk=*/10, kMaxHp);
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
    return p;
}

void both_none(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

void check_all_pp(BattleContext& ctx, int side, int expect,
                  const std::string& tag) {
    for (std::size_t i = 0; i < ctx.getPet(side).skills.size(); ++i) {
        CHECK_EQ(ctx.getPet(side).skills[i].pp, expect,
                 tag + "：side" + std::to_string(side) + " 技能 " +
                     std::to_string(i) + " PP 应为 " + std::to_string(expect));
    }
}

}  // namespace

int main() {
    sbs_srand(20261024);
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7090.a row315 [1,1]：R1 对手全技能 PP -1 ══
    {
        ScenarioBattle b(make_party(mark_pet(1)), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        const int pp0 = ctx.getPet(1).skills[0].pp;
        both_none(b);
        check_all_pp(ctx, 1, pp0 - 1, "T7090.a");
        std::cout << "  [T7090.a] row315 PP-1 通过（基线 " << pp0 << "）" << std::endl;
    }

    // ══ T7090.b row438 [2,1]：R1 对手全技能 PP -2（行参驱动）══
    {
        ScenarioBattle b(make_party(mark_pet(2)), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        const int pp0 = ctx.getPet(1).skills[0].pp;
        both_none(b);
        check_all_pp(ctx, 1, pp0 - 2, "T7090.b");
        std::cout << "  [T7090.b] row438 PP-2 通过" << std::endl;
    }

    // ══ T7090.c 免疫能力下降 ══
    {
        // 持印者：stat_drop 被免（等级不变）
        {
            ScenarioBattle b(make_party(mark_pet(1)), make_party(plain_pet()));
            b.start();
            BattleContext& ctx = b.ctx();
            const StatDropResult r = stat_drop(&ctx, /*target=*/0,
                                               /*攻击=*/0, /*delta=*/2, /*actor=*/1);
            CHECK(r != StatDropResult::SUCCESS,
                  "T7090.c 持印者免能力下降（STAT_DROP 票拦下）");
            CHECK_EQ(ctx.ability_level(0, 0), 0, "T7090.c 攻击等级未被压低");
        }
        // 无印对照：正常落地
        {
            ScenarioBattle b(make_party(mark_pet(1, /*with_mark=*/false)),
                             make_party(plain_pet()));
            b.start();
            BattleContext& ctx = b.ctx();
            const StatDropResult r = stat_drop(&ctx, 0, 0, 2, 1);
            CHECK_EQ(r, StatDropResult::SUCCESS, "T7090.c 对照：无印正常被降");
            CHECK_EQ(ctx.ability_level(0, 0), -2, "T7090.c 对照：攻击 -2 落地");
        }
        std::cout << "  [T7090.c] 免疫能力下降+对照 通过" << std::endl;
    }

    // ══ T7090.d 累计窗：R1/R2 各 -1 ══
    {
        ScenarioBattle b(make_party(mark_pet(1)), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        const int pp0 = ctx.getPet(1).skills[0].pp;
        both_none(b);
        check_all_pp(ctx, 1, pp0 - 1, "T7090.d R1");
        both_none(b);
        check_all_pp(ctx, 1, pp0 - 2, "T7090.d R2");
        std::cout << "  [T7090.d] 每回合累计消耗 通过" << std::endl;
    }

    // ══ T7090.e 只消耗对手：持印者自身 PP 不动 ══
    {
        ScenarioBattle b(make_party(mark_pet(1)), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        const int self_pp0 = ctx.getPet(0).skills[0].pp;
        const int foe_pp0 = ctx.getPet(1).skills[0].pp;
        both_none(b);
        CHECK_EQ(ctx.getPet(0).skills[0].pp, self_pp0, "T7090.e 持印者自身 PP 不动");
        CHECK_EQ(ctx.getPet(1).skills[0].pp, foe_pp0 - 1, "T7090.e 对手 PP -1");
        std::cout << "  [T7090.e] 只消耗对手 通过" << std::endl;
    }

    // ══ T7090.f 钳 0：低 PP 技能耗到 0 不穿 ══
    {
        ScenarioBattle b(make_party(mark_pet(2)), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(1).skills[0].pp = 1;    // 低 PP：-2 后钳 0
        ctx.getPet(1).skills[1].pp = 0;    // 已 0：不再变动
        ctx.getPet(1).skills[2].pp = -1;   // 无限 PP：不参与
        both_none(b);
        CHECK_EQ(ctx.getPet(1).skills[0].pp, 0, "T7090.f 低 PP 钳 0");
        CHECK_EQ(ctx.getPet(1).skills[1].pp, 0, "T7090.f 已 0 不变");
        CHECK_EQ(ctx.getPet(1).skills[2].pp, -1, "T7090.f 无限 PP 不参与");
        std::cout << "  [T7090.f] 钳0/无限PP边界 通过" << std::endl;
    }

    std::cout << "scenario_7090 PASS (639 对手PP消耗+免弱 2 行)" << std::endl;
    return 0;
}
