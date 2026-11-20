// 场景 4910 —— K335 战阶磨血（无下降门）族（655，soul_lib/k335_nodrop_grind.cpp）
//（2026-10-21，worktree ../agents/k335；号段 4910-4919 → T4910-T4911 两块）
//
// 断言口径（655「战斗阶段结束时若对手不处于能力下降状态则每回合减少1/4最大HP
//             （boss无效）」）：
//   - T4910 门开：对手**无能力下降** → 回合末对手掉 1/4 最大体力（99999/4=24999，
//     粉伤 FIXED 不浮动，精确）。
//   - T4911 门闭：对手**有**能力下降（负等级）→ 回合末不掉血。
//
// ⚠️ 场景构造口径：按 **DB args 原样**传 {1,4,1}（不补 {0,0} 前缀）——实现侧走
//   owned_int_args 原序读（K257 实现头注「行参读取路径」记档）。
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
constexpr int kMaxHp = 99999;
constexpr int kGrind = kMaxHp / 4;   // 24999（1/4）

ElfPet mark_pet(std::vector<int> args = {1, 4, 1}) {
    ElfPet p = make_pet(1, "印主", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/300, /*atk=*/1, /*hp=*/kMaxHp,
                        /*soul_mark=*/SoulMark(655, "K335 魂印", "",
                                               EffectArgs(std::move(args))));
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

// 跑一回合，返回对手掉血。drop_level<0 时把对手某槽压成负等级。
int one_round(ScenarioBattle& b, int drop_level) {
    BattleContext& ctx = b.ctx();
    if (drop_level < 0) {
        ctx.ability_levels[1][2] = drop_level;   // 槽 2 = 防御
    }
    const int hp0 = ctx.getPet(1).hp;
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
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

    // ══ T4910 门开：对手无能力下降 → 掉 1/4 最大HP ═══════════════════════════
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        const int loss = one_round(b, /*drop_level=*/0);
        CHECK_EQ(loss, kGrind,
                 "T4910 655：对手不处于能力下降状态 → 回合末减少 1/4 最大体力（24999）");
        std::cout << "  [T4910] 无下降门磨血 通过（loss=" << loss << "）\n";
    }

    // ══ T4911 门闭：对手有负等级 → 不掉血 ═══════════════════════════════════
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        const int loss = one_round(b, /*drop_level=*/-1);
        CHECK_EQ(loss, 0,
                 "T4911 655：对手处于能力下降状态 → 回合末不减少体力（门闭）");
        std::cout << "  [T4911] 门闭对照 通过（loss=" << loss << "）\n";
    }

    // 原序 db[1] 分母：实际宿主在槽1，场下同号零参不能抢参；缺位才回退。
    struct GrindCase {
        std::vector<int> args;
        int loss;
    };
    const std::vector<GrindCase> cases{
        {{1, 3, 1}, 33333}, {{0, 3, 0}, 33333}, {{1, 0, 1}, 0},
        {{}, 24999}, {{1}, 24999},
    };
    for (const auto& c : cases) {
        auto party = make_party(plain_pet());
        party[0] = mark_pet({1, 0, 1});
        party[1] = mark_pet(c.args);
        ScenarioBattle b(party, make_party(plain_pet()));
        b.ctx().on_stage[0] = 1;
        b.start();
        b.ctx().getPet(1).hp = 50000;   // 分母作用于最大HP，不是当前HP
        CHECK_EQ(one_round(b, 0), c.loss, "655：host_slot 原序分母、零关闸、缺失回退");
    }

    // 六槽都属于下降门（含命中）；有正等级不关门，任一负等级就关门。
    for (int slot = 0; slot < BattleContext::kAbilityLevelSlotCount; ++slot) {
        for (const int delta : {-1, 1}) {
            ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
            b.start();
            BattleContext& ctx = b.ctx();
            REQUIRE(stat_change(&ctx, 1, slot, delta) == StatChangeResult::SUCCESS);
            const int before_level = ctx.ability_levels[1][slot];
            REQUIRE(before_level == delta);
            CHECK_EQ(one_round(b, 0), delta < 0 ? 0 : kGrind,
                     "655：任一槽下降关门、仅提升仍磨最大HP的1/4");
            CHECK_EQ(ctx.ability_levels[1][slot], before_level,
                     "655：能力门只读，不消除也不改写等级");
        }
    }
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        REQUIRE(stat_change(&b.ctx(), 1, 0, 2) == StatChangeResult::SUCCESS);
        REQUIRE(stat_change(&b.ctx(), 1, 5, -1) == StatChangeResult::SUCCESS);
        CHECK_EQ(one_round(b, 0), 0, "655：正负混合态仍因命中下降而关门");
    }

    // 回合桶双方都跑也只能每轮磨一次；更新器重注册不会叠条目或只触发首轮。
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        for (int round = 0; round < 3; ++round) {
            CHECK_EQ(one_round(b, 0), kGrind, "655：连续三轮每轮只扣一份 24999");
            CHECK_EQ(b.ctx().getPet(1).hp, kMaxHp - (round + 1) * kGrind,
                     "655：多轮累计恰等于轮数乘单次磨血");
        }
    }
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.ctx().is_boss_challenge = true;
        b.start();
        for (int round = 0; round < 3; ++round) {
            CHECK_EQ(one_round(b, 0), 0, "655：boss旗在首次注册前设置，三轮门开也均无效");
        }
    }

    std::cout << "scenario_4910 PASS (K335 战阶磨血（无下降门）族)" << std::endl;
    return 0;
}
