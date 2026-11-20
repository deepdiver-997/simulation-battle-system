// 场景 5890 —— J258 随机 PP 归零+受击加罚族（1126，moves_lib/j_pp_zero_on_hit.cpp）
//（2026-10-22，worktree feat/jbatch；号段 5890-5899）
//
// 断言口径（PP 槽位逐槽快照，非"正 PP 和"）：
//   - 对手槽 0 = 本回合出手槽（正常消耗 1 点）；断言按 used_slot 特判，
//     把"效果归零"与"正常技能消耗"分离。
//   - 快照不变量：每个槽 after == before 或 after == 0（出手槽允许 -1）；
//     新归零槽数精确等于 {0}+{1}（受击档）/ {0}（未受击档）。
//   - 立即段用 step_past(FIRST_SKILL_EFFECT) 在对手行动前读：审计=立即归零数；
//     完整回合再读额外段审计。
//   - 单发消费：R2 对手再次攻击 → 额外段审计不再增长、PP 不再归零（窗过期+token）。
//   - 无正 PP 候选：默认段/额外段都安全归零 0 项（审计 0/0，不崩）。
#include <array>
#include <iostream>
#include <any>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <plugin/core_api.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;   // 拍打：物攻 40（对手攻击载体）
constexpr int k1126a = 35282;   // 骇影投射 纯链 [1,1]
constexpr int k1126b = 25617;   // 灵鱼祈盼 [2,1]（伴生 585/877/48，对 PP 断言良性）

constexpr int kAuditBase = 996260100;
constexpr int kExtraBase = 996260200;

constexpr int kSlots = 5;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_owner(int carrier) {
    ElfPet p = make_pet(1, "己方", {carrier, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/300, /*hp=*/99999);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

// 对手 5 槽均正 PP；槽 0 是本回合出手槽（正常消耗 1 点，断言按 used_slot 特判）。
ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/300, /*hp=*/99999);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

using Snap = std::array<int, kSlots>;

Snap snap(BattleContext& ctx, int side) {
    Snap s{};
    for (int i = 0; i < kSlots; ++i) {
        s[static_cast<std::size_t>(i)] =
            ctx.getPet(side).skills[static_cast<std::size_t>(i)].pp;
    }
    return s;
}

// 新归零槽数：before>0 且 after==0。
int newly_zeroed(const Snap& before, const Snap& after) {
    int n = 0;
    for (int i = 0; i < kSlots; ++i) {
        if (before[static_cast<std::size_t>(i)] > 0 && after[static_cast<std::size_t>(i)] == 0) {
            ++n;
        }
    }
    return n;
}

// 槽位不变量：每槽 either 不变 or 归零；出手槽（used_slot）允许正常消耗 1 点。
bool only_unchanged_or_zero(const Snap& before, const Snap& after, int used_slot = -1) {
    for (int i = 0; i < kSlots; ++i) {
        const int b = before[static_cast<std::size_t>(i)];
        const int a = after[static_cast<std::size_t>(i)];
        if (a == b || a == 0) {
            continue;
        }
        if (i == used_slot && b > 0 && a == b - 1) {
            continue;   // 本回合正常出手消耗
        }
        return false;
    }
    return true;
}

int audit(BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return -999;
    }
    if (const int* p = std::any_cast<int>(&it->second)) {
        return *p;
    }
    return -999;
}

void both_act(ScenarioBattle& b, int self_slot, int foe_slot) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, self_slot);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_slot);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5890.a 35282 [1,1]：立即段在对手行动前已归零 1 项 ══
    {
        ScenarioBattle b(make_party(mk_owner(k1126a)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        const Snap before = snap(ctx, 1);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        ctx.debug_step_mode = true;   // 5840 field_trial 同款：第二次提交起单步
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);
        const Snap mid = snap(ctx, 1);
        CHECK_EQ(audit(ctx, kAuditBase + 0), 1, "a1 立即段审计=1");
        CHECK_EQ(newly_zeroed(before, mid), 1, "a2 立即段精确归零 1 槽");
        CHECK(only_unchanged_or_zero(before, mid), "a3 槽位只允许不变或归零");
        std::cout << "  [a] 立即段：1 槽归零（对手行动前）\n";
    }

    // ══ T5890.b 35282 [1,1]：完整回合，受击后额外归零 1 项 ══
    {
        ScenarioBattle b(make_party(mk_owner(k1126a)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        const Snap before = snap(ctx, 1);
        const int hp_before = ctx.getPet(0).hp;
        both_act(b, 0, 0);   // 己方 1126；对手拍打（正常消耗 1 点）
        const Snap after = snap(ctx, 1);
        CHECK(ctx.getPet(0).hp < hp_before, "b1 对手攻击实际命中（受击条件成立）");
        CHECK_EQ(audit(ctx, kAuditBase + 0), 1, "b2 立即段审计=1");
        CHECK_EQ(audit(ctx, kExtraBase + 0), 1, "b3 额外段审计=1");
        CHECK_EQ(newly_zeroed(before, after), 2, "b4 立即+额外精确归零 2 槽");
        CHECK(only_unchanged_or_zero(before, after, /*used_slot=*/0), "b5 槽位只允许不变/归零/出手槽-1");
        CHECK(after[0] == before[0] || after[0] == before[0] - 1 || after[0] == 0, "b6 出手槽仅正常消耗或被归零");
        std::cout << "  [b] 完整回合：立即 1 + 受击额外 1 = 2 槽归零\n";
    }

    // ══ T5890.c 35282 [1,1]：对手不攻击 → 只有立即段 ══
    {
        ScenarioBattle b(make_party(mk_owner(k1126a)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        const Snap before = snap(ctx, 1);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        const Snap after = snap(ctx, 1);
        CHECK_EQ(audit(ctx, kAuditBase + 0), 1, "c1 立即段审计=1");
        CHECK_EQ(audit(ctx, kExtraBase + 0), 0, "c2 未受击额外段审计=0");
        CHECK_EQ(newly_zeroed(before, after), 1, "c3 仅立即段归零 1 槽");
        std::cout << "  [c] 未受击：仅立即 1 槽\n";
    }

    // ══ T5890.d 25617 [2,1]：参数驱动（立即 2 + 额外 1）══
    {
        ScenarioBattle b(make_party(mk_owner(k1126b)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        const Snap before = snap(ctx, 1);
        both_act(b, 0, 0);
        const Snap after = snap(ctx, 1);
        CHECK_EQ(audit(ctx, kAuditBase + 0), 2, "d1 立即段审计=2");
        CHECK_EQ(audit(ctx, kExtraBase + 0), 1, "d2 额外段审计=1");
        CHECK_EQ(newly_zeroed(before, after), 3, "d3 精确归零 3 槽");
        CHECK(only_unchanged_or_zero(before, after, /*used_slot=*/0), "d4 槽位只允许不变/归零/出手槽-1");
        std::cout << "  [d] [2,1] 参数：立即 2 + 额外 1 = 3 槽\n";
    }

    // ══ T5890.e 窗过期 + 单发消费：R2 再受击不再归零 ══
    {
        ScenarioBattle b(make_party(mk_owner(k1126a)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        both_act(b, 0, 0);                          // R1：立即+额外各 1
        const Snap after_r1 = snap(ctx, 1);
        const int extra_r1 = audit(ctx, kExtraBase + 0);
        const int hp_before = ctx.getPet(0).hp;
        both_act(b, 1, 0);                          // R2：填充技，对手再攻击
        const Snap after_r2 = snap(ctx, 1);
        CHECK(ctx.getPet(0).hp < hp_before, "e1 R2 对手攻击命中");
        CHECK_EQ(audit(ctx, kExtraBase + 0), extra_r1, "e2 R2 额外段审计不增长");
        CHECK_EQ(newly_zeroed(after_r1, after_r2), 0, "e3 R2 无新归零槽");
        std::cout << "  [e] 窗过期/单发消费：R2 不再加罚\n";
    }

    // ══ T5890.f 无正 PP 候选：两段都安全归零 0 项 ══
    {
        ScenarioBattle b(make_party(mk_owner(k1126a)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        for (int i = 0; i < kSlots; ++i) {          // 对手全部 PP=0：无候选
            ctx.getPet(1).skills[static_cast<std::size_t>(i)].pp = 0;
        }
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(audit(ctx, kAuditBase + 0), 0, "f1 立即段无候选归零 0");
        CHECK_EQ(audit(ctx, kExtraBase + 0), 0, "f2 额外段无候选归零 0");
        CHECK_EQ(ctx.getPet(1).skills[0].pp, 0, "f3 对手 PP 全 0 保持");
        std::cout << "  [f] 无正 PP 候选：0/0 安全\n";
    }

    return 0;
}
