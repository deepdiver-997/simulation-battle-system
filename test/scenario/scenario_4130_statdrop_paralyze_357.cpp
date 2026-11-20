// 场景 4130 —— K257 对手能力下降→攻击麻痹族（357，soul_lib/k257_statdrop_paralyze.cpp）
//（2026-10-21，worktree ../agents/k257；号段 4130-4139 → T4130-T4133 四块）
//
// 断言口径（357「对手处于能力下降状态时，所有攻击技能有30%概率使对手麻痹（boss免疫)」）：
//   - 审计：真实行 [2,1,1,30] → 审计槽申报值 = 30（**无随机精确断言**，F9 口径）。
//   - 合成行 [2,1,1,100] → 100% 麻痹必落地（确定性档）。
//   - 合成行 [2,1,1,0] → 不申报（审计槽 -1）。
//   - 门闭：对手无能力下降 → 不申报；属性技 → 不申报。
//
// ⚠️ 场景构造口径：按 **DB args 原样**传 {2,1,1,30}（不补 {0,0} 前缀）——
//   实现侧走 db_at 读 owned_int_args 原序，与真实对局语义一致。
//   （存量场景按 {0,0,...行参} 构造 + 实现走 a.row_or，两边都"错"但互相抵消；
//    本单两边都"对"——见实现头注「行参读取路径」记档。）
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

constexpr int kJi = 10001;     // 撞击：物理攻击技载体
constexpr int kSuoTou = 20001; // 缩头：属性技载体
constexpr int kParalysis = static_cast<int>(AbnormalStatusId::Paralysis);  // 0
constexpr int kChanceAuditBase = 991457000;   // +owner：本次申报概率%（审计槽）

ElfPet mark_pet(const std::vector<int>& row, int attack_skill = kJi) {
    ElfPet p = make_pet(1, "印主", {attack_skill, kJi, kJi, kJi, kJi},
                        /*speed=*/300, /*atk=*/300, /*hp=*/99999,
                        /*soul_mark=*/SoulMark(357, "K257 魂印", "",
                                               EffectArgs(row)));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_pet() {
    ElfPet p = make_pet(2, "对手", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/100, /*atk=*/1, /*hp=*/99999);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

int audit_int(BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    return it == ctx.plugin_storage.end() ? -1 : std::any_cast<int>(it->second);
}

// 印主出招（对手 NONE）。
void my_strike(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T4130 真实行：审计槽申报值 = 30 ══
    {
        ScenarioBattle b(make_party(mark_pet({2, 1, 1, 30})), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        // 对手能力下降（槽 2 = 防御 -1）
        ctx.ability_levels[1][2] = -1;
        my_strike(b);
        CHECK_EQ(audit_int(ctx, kChanceAuditBase + 0), 30,
                 "T4130 357：真实行申报值 = db[3] = 30");
        std::cout << "  [T4130] 真实行审计 通过\n";
    }

    // ══ T4131 合成行 100%：麻痹必落地 ══
    {
        ScenarioBattle b(make_party(mark_pet({2, 1, 1, 100})), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][2] = -1;
        my_strike(b);
        CHECK_EQ(audit_int(ctx, kChanceAuditBase + 0), 100,
                 "T4131 357：合成行申报值 = 100");
        CHECK(ctx.has_active_abnormal_status(1, kParalysis),
              "T4131 357：100% 档 → 对手被麻痹");
        std::cout << "  [T4131] 100% 落地 通过\n";
    }

    // ══ T4132 门闭：对手无能力下降 → 不申报；属性技 → 不申报 ══
    {
        ScenarioBattle b(make_party(mark_pet({2, 1, 1, 100})), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        // 不压等级 → 对手无能力下降 → 门闭
        my_strike(b);
        CHECK_EQ(audit_int(ctx, kChanceAuditBase + 0), -1,
                 "T4132 357：对手无能力下降 → 不申报（门闭）");
        CHECK(!ctx.has_active_abnormal_status(1, kParalysis),
              "T4132 357：门闭 → 对手不被麻痹");
        std::cout << "  [T4132] 门闭对照 通过\n";
    }
    {
        ScenarioBattle b(make_party(mark_pet({2, 1, 1, 100}, kSuoTou)),
                         make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][2] = -1;
        my_strike(b);
        CHECK_EQ(audit_int(ctx, kChanceAuditBase + 0), -1,
                 "T4132 357：印主用属性技 → 不申报（攻击技门）");
        CHECK(!ctx.has_active_abnormal_status(1, kParalysis),
              "T4132 357：属性技 → 对手不被麻痹");
        std::cout << "  [T4132b] 属性技门对照 通过\n";
    }

    // ══ T4133 0% 档：申报但不生效 ══
    {
        ScenarioBattle b(make_party(mark_pet({2, 1, 1, 0})), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][2] = -1;
        my_strike(b);
        CHECK(!ctx.has_active_abnormal_status(1, kParalysis),
              "T4133 357：0% 档 → 对手不被麻痹");
        std::cout << "  [T4133] 0% 档 通过\n";
    }

    std::cout << "scenario_4130 PASS (K257 对手能力下降→攻击麻痹族)" << std::endl;
    return 0;
}
