// 场景 5870 —— J256 条件自强化族（763/1425，moves_lib/j_cond_self_boost.cpp）
//（2026-10-22，worktree feat/jbatch；号段 5870-5879）
//
// 断言口径（逐槽快照，不看"等级和"；坑集 §1）：
//   - 参数序 = **官方码序 攻/防/特攻/特防/速/命中**，落引擎槽需经
//     {0,2,1,3,4,5}（0攻/1特攻/2防/3特防/4速/5命中；防与特攻互换——477/501 同款，
//     直接按引擎序读会把防/特攻读反，见常识速查"属性排列双序"）。
//   - 763（28536 午夜帷幕，纯链属性技，参数 [1,1,2,1,2,0] 官方序）：
//       无正等级 → 引擎槽 [1,2,1,1,2,0]（攻+1/特攻+2/防+1/特防+1/速+2，逐槽精确）；
//       已有正等级（攻+1）→ 门闭，逐槽不变（绝对值）+ 审计 -1；
//       只有负等级（攻-2/防-1）→ 仍算"不处于提升" → 攻=-1、防=0、特攻=2（原值+增量）。
//   - 1425（33757 裂魂践踏，参数 [200,1,0,0,0,1,0]）：
//       低伤（atk=1，伤害 1~2 <200）→ 攻+1/速+1；审计=触发 final；
//       高伤（atk=1000，伤害 ~700 >200）→ 门闭，逐槽不变（绝对值）+ 审计 -1。
//   - 等级断言全部按槽读取 `ctx.ability_levels[0][slot]`（本体等级；stat_change 同步视图）。
#include <iostream>
#include <any>
#include <array>
#include <string>
#include <vector>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <plugin/core_api.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;   // 拍打：物 40（填充）
constexpr int k763   = 28536;   // 午夜帷幕 [763]|[1,1,2,1,2,0]（纯链，属性）
constexpr int k1425  = 33757;   // 裂魂践踏 [868,1425]|[1, 200,1,0,0,0,1,0]
constexpr int k1425b = 34397;   // 净尘法球 [1061,1425]|[250,0,1,0,1,0,0]（特攻向）

constexpr int k763Audit = 990763100;
constexpr int k1425Audit = 991425100;

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
}

ElfPet mk_owner(int carrier, int atk) {
    ElfPet p = make_pet(1, "己方", {carrier, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/atk, /*hp=*/99999);
    accuracy_100(p);
    return p;
}

ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/99999);
    accuracy_100(p);
    return p;
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

std::array<int, 6> levels(BattleContext& ctx, int side) {
    std::array<int, 6> out{};
    for (int i = 0; i < 6; ++i) {
        out[static_cast<std::size_t>(i)] = ctx.ability_levels[side][i];
    }
    return out;
}

struct StatTrial {
    std::array<int, 6> before{};
    std::array<int, 6> after{};
    int audit = -999;
    int dealt = 0;
};

// carrier 用 0 号技能；预设等级由调用方在 start 后写入 ctx.ability_levels。
StatTrial run_trial(int carrier, int atk, const std::array<int, 6>& preset) {
    ScenarioBattle b(make_party(mk_owner(carrier, atk)), make_party(mk_foe()));
    b.start();
    BattleContext& ctx = b.ctx();
    for (int i = 0; i < 6; ++i) {
        ctx.ability_levels[0][i] = preset[static_cast<std::size_t>(i)];
    }
    StatTrial t;
    const int foe_before = ctx.getPet(1).hp;
    t.before = levels(ctx, 0);
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
    t.after = levels(ctx, 0);
    t.dealt = foe_before - ctx.getPet(1).hp;
    t.audit = audit(ctx, carrier == k763 ? k763Audit : k1425Audit);
    return t;
}

std::string fmt(const std::array<int, 6>& a) {
    std::string s = "[";
    for (int i = 0; i < 6; ++i) {
        s += std::to_string(a[static_cast<std::size_t>(i)]);
        if (i != 5) s += ",";
    }
    return s + "]";
}

// 逐槽断言（CHECK_EQ 只吃整数；数组需要逐槽，正好符合"精确槽位"口径）。
void check_slots(const std::array<int, 6>& got, const std::array<int, 6>& want,
                 const std::string& tag) {
    for (int i = 0; i < 6; ++i) {
        CHECK_EQ(got[static_cast<std::size_t>(i)], want[static_cast<std::size_t>(i)],
                 tag + " slot " + std::to_string(i));
    }
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5870.a 763：无正等级 → 六槽精确增量；有正等级 → 门闭 ══
    {
        const std::array<int, 6> zero{0, 0, 0, 0, 0, 0};
        const StatTrial open = run_trial(k763, /*atk=*/100, zero);
        // 28536 [1,1,2,1,2,0] 官方序（攻,防,特攻,特防,速,命中）
        // → 引擎槽（攻,特攻,防,特防,速,命中）= [1,2,1,1,2,0]
        const std::array<int, 6> want{1, 2, 1, 1, 2, 0};
        std::cout << "  [T5870.a] open before=" << fmt(open.before) << " after=" << fmt(open.after)
                  << " audit=" << open.audit << " | want=" << fmt(want) << std::endl;
        check_slots(open.after, want,
                    "T5870.a 无正等级 → 官方序 [1,1,2,1,2,0] 落引擎槽 [1,2,1,1,2,0]");
        CHECK_EQ(open.audit, 31, "T5870.a 应用参数位掩码=0b011111=31");

        std::array<int, 6> boosted{0, 0, 0, 0, 0, 0};
        boosted[0] = 2;   // 攻 +2：已有正等级
        const StatTrial closed = run_trial(k763, /*atk=*/100, boosted);
        std::cout << "  [T5870.a] closed before=" << fmt(closed.before)
                  << " after=" << fmt(closed.after) << " audit=" << closed.audit << std::endl;
        check_slots(closed.after, boosted, "T5870.a 已有正等级 → 门闭，逐槽不变");
        CHECK_EQ(closed.audit, -1, "T5870.a 门闭审计=-1");
    }

    // ══ T5870.b 763：只有负等级也算"不处于提升" → 增量叠加在原值上 ══
    {
        std::array<int, 6> neg{0, 0, 0, 0, 0, 0};
        neg[0] = -2;   // 攻 -2
        neg[2] = -1;   // 防 -1
        const StatTrial t = run_trial(k763, /*atk=*/100, neg);
        // 官方序增量 [1,1,2,1,2,0] → 引擎槽 [0]+1、[2]+1、[1]+2、[3]+1、[4]+2：
        // 攻 -2+1=-1、防 -1+1=0、特攻 0+2=2、特防 +1、速 +2。
        const std::array<int, 6> want{-1, 2, 0, 1, 2, 0};
        std::cout << "  [T5870.b] before=" << fmt(t.before) << " after=" << fmt(t.after)
                  << " audit=" << t.audit << " | want=" << fmt(want) << std::endl;
        check_slots(t.after, want,
                    "T5870.b 负等级 + 官方序增量叠加（攻-2+1=-1、防-1+1=0、特攻+2）");
        CHECK_EQ(t.audit, 31, "T5870.b 门开掩码=31");
    }

    // ══ T5870.c 1425：伤害 <200 → 攻+1/速+1；伤害 >200 → 门闭 ══
    {
        const std::array<int, 6> zero{0, 0, 0, 0, 0, 0};
        const StatTrial low = run_trial(k1425, /*atk=*/1, zero);
        const std::array<int, 6> want{1, 0, 0, 0, 1, 0};
        std::cout << "  [T5870.c] low dealt=" << low.dealt << " after=" << fmt(low.after)
                  << " audit=" << low.audit << " | want=" << fmt(want) << std::endl;
        CHECK(low.dealt > 0 && low.dealt < 200,
              "T5870.c 低伤构造伤害 <200（实测 " + std::to_string(low.dealt) + "）");
        check_slots(low.after, want, "T5870.c 低伤 → 攻+1/速+1（逐槽）");
        CHECK_EQ(low.audit, low.dealt, "T5870.c 审计=触发 final");

        const StatTrial high = run_trial(k1425, /*atk=*/1000, zero);
        std::cout << "  [T5870.c] high dealt=" << high.dealt << " after=" << fmt(high.after)
                  << " audit=" << high.audit << std::endl;
        CHECK(high.dealt >= 200, "T5870.c 高伤构造伤害 >=200（实测 " +
                                     std::to_string(high.dealt) + "）");
        check_slots(high.after, zero, "T5870.c 高伤门闭 → 逐槽不变（绝对值）");
        CHECK_EQ(high.audit, -1, "T5870.c 高伤门闭审计=-1");
    }

    // ══ T5870.d 1425 官方序映射：特攻向载体 [250,0,1,0,1,0,0] → 防+1/特防+1 ══
    // 若按引擎序直读会错落成 特攻+1/特防+1（槽 1 而非槽 2）——本用例锁死防/特攻互换。
    {
        const std::array<int, 6> zero{0, 0, 0, 0, 0, 0};
        const StatTrial t = run_trial(k1425b, /*atk=*/1, zero);
        const std::array<int, 6> want{0, 0, 1, 1, 0, 0};
        std::cout << "  [T5870.d] dealt=" << t.dealt << " after=" << fmt(t.after)
                  << " audit=" << t.audit << " | want=" << fmt(want) << std::endl;
        CHECK(t.dealt > 0 && t.dealt < 250,
              "T5870.d 低伤构造伤害 <250（实测 " + std::to_string(t.dealt) + "）");
        check_slots(t.after, want, "T5870.d 官方序 [0,1,0,1,0,0] → 防+1/特防+1（槽 2/3）");
        CHECK_EQ(t.audit, t.dealt, "T5870.d 审计=触发 final");
    }

    std::cout << "scenario_5870 PASS (J256 条件自强化族)" << std::endl;
    return 0;
}
