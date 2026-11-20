// 场景 6280 —— J295 最大体力差护盾族（1871，moves_lib/j_maxhp_diff_shield.cpp）
//（2026-10-22，worktree feat/jbatch；号段 6280-6289）
//
// 断言口径：
//   - 1871（28499 地动检索，纯链 0 参）：门 = 自身**当前体力严格低于**对手；
//     量 = |双方最大体力差|；落点 = shield_bank_（普通盾，priority 0）。
//   - 守卫式断言：门闭/相等/零差值三档都必须**不授予**（bank 绝对量为 0）。
//   - 行为侧证：盾 3000 吸收一次 atk300 红伤（[87,102]）→ 自身 hp 不变、
//     bank 减少等量（验证盾真实进了伤害链，而不是只写了审计槽）。
#include <iostream>
#include <string>
#include <any>

#include <entities/numerical-properties.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kShield1871 = 28499;   // 地动检索 [1871]（0 参纯链，属性技）
constexpr int kPaiDa = 10004;        // 拍打：物 40（填充刀）

constexpr int kAmountKey = 991871100;   // +owner：差值/授予量
constexpr int kGateKey = 991871200;     // +owner：门 1/0

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
}

// max_hp 同时是初始体力；cur_hp 覆盖当前体力（max 保持 numericalBase）。
ElfPet mk_self(int max_hp, int cur_hp, int atk = 1) {
    ElfPet p = make_pet(1, "己方", {kShield1871, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, atk, max_hp);
    p.hp = cur_hp;
    accuracy_100(p);
    return p;
}

ElfPet mk_foe(int max_hp, int cur_hp, int atk = 1) {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, atk, max_hp);
    p.hp = cur_hp;
    accuracy_100(p);
    return p;
}

int stor(const BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return 0;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : 0;
}

int shield_total(BattleContext& ctx, int side) {
    return ctx.getPet(side).shield_bank_.total();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T6280.a 门开：max 5000/8000、当前 3000<8000 → 盾 = 3000 ══
    {
        ScenarioBattle b(make_party(mk_self(5000, 3000)), make_party(mk_foe(8000, 8000)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(stor(ctx, kGateKey + 0), 1, "T6280.a 门开（3000 < 8000）");
        CHECK_EQ(stor(ctx, kAmountKey + 0), 3000, "T6280.a |5000-8000| = 3000");
        CHECK_EQ(shield_total(ctx, 0), 3000, "T6280.a 护盾槽 = 3000");
        CHECK_EQ(ctx.getPet(0).hp, 3000, "T6280.a 授予盾本身不改体力");
    }

    // ══ T6280.b 门闭：当前体力 9000 > 8000 → 不授予 ══
    {
        ScenarioBattle b(make_party(mk_self(9000, 9000)), make_party(mk_foe(8000, 8000)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(stor(ctx, kGateKey + 0), 0, "T6280.b 门闭（9000 ≥ 8000）");
        CHECK_EQ(stor(ctx, kAmountKey + 0), 0, "T6280.b 差值审计 0");
        CHECK_EQ(shield_total(ctx, 0), 0, "T6280.b 不授予护盾");
    }

    // ══ T6280.c 相等档：当前体力相同 → 严格低于不成立 → 不授予 ══
    {
        ScenarioBattle b(make_party(mk_self(8000, 8000)), make_party(mk_foe(8000, 8000)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(stor(ctx, kGateKey + 0), 0, "T6280.c 相等不授予（严格低于）");
        CHECK_EQ(shield_total(ctx, 0), 0, "T6280.c 护盾槽 0");
    }

    // ══ T6280.d 绝对差：自身 max 4000 < 对手 max 9000 → 盾 5000 ══
    {
        ScenarioBattle b(make_party(mk_self(4000, 2000)), make_party(mk_foe(9000, 9000)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(stor(ctx, kAmountKey + 0), 5000, "T6280.d |4000-9000| = 5000（绝对差）");
        CHECK_EQ(shield_total(ctx, 0), 5000, "T6280.d 护盾槽 = 5000");
    }

    // ══ T6280.e 零差值：max 相同（8000/8000），门开但无盾可给 ══
    {
        ScenarioBattle b(make_party(mk_self(8000, 3000)), make_party(mk_foe(8000, 8000)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(stor(ctx, kGateKey + 0), 1, "T6280.e 门开（3000 < 8000）");
        CHECK_EQ(stor(ctx, kAmountKey + 0), 0, "T6280.e 最大体力差 0");
        CHECK_EQ(shield_total(ctx, 0), 0, "T6280.e 差值 0 → 不给盾");
    }

    // ══ T6280.f 行为侧证：盾 3000 吸收 atk300 红伤 → hp 不变、盾减少 ══
    {
        ScenarioBattle b(make_party(mk_self(5000, 3000)), make_party(mk_foe(8000, 8000, 300)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 上盾
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(shield_total(ctx, 0), 3000, "T6280.f 前置：盾 3000");
        const int hp_before = ctx.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);           // R2 挨打
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 对手拍打
        b.drive();
        const int loss = hp_before - ctx.getPet(0).hp;
        CHECK_EQ(loss, 0, "T6280.f 盾完全吸收红伤（hp 不变，实测掉 " +
                              std::to_string(loss) + "）");
        const int remain = shield_total(ctx, 0);
        CHECK(remain >= 2896 && remain <= 2915,
              "T6280.f 盾减少等量红伤（atk300 raw∈[87,102]，余 " +
                  std::to_string(remain) + "）");
    }

    std::cout << "scenario 6280 OK" << std::endl;
    return 0;
}
