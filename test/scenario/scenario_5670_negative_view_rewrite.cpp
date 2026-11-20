// 场景 5670 —— J236 自身负等级视图改写族（1750/1874，moves_lib/j_negative_view_rewrite.cpp）
//（2026-10-22，worktree feat/jbatch；号段 5670-5679）
//
// 断言口径（精确槽位 + 效果前后快照 + 伤害比值）：
//   - 本体等级设在 ctx.ability_levels[0] = {-2, +1, 0, -3, 0, 0}（0攻 1特攻 2防 3特防
//     4速 5命中）；ROUND_START 同步进 ws.view_levels。
//   - 1750（37097 闪光一勺，纯链 [1750]）：SKILL_EFFECT 后视图负槽归 0
//     {0,+1,0,0,0,0}；AFTER_ACTION 还原为 {-2,+1,0,-3,0,0}。
//     伤害比值（负攻 vs 无等级）应落在 1× 带（>0.7），若门失效会掉到 0.5× 带。
//   - 1874（37235 兵者诡道，纯链 [1874]）：SKILL_EFFECT 后视图负槽镜像为同幅正等级
//     {+2,+1,0,+3,0,0}；AFTER_ACTION 还原。伤害比值应落在 2× 带（>1.5）。
//   - 审计槽（991750200/991874200 + owner）记录实际改写掩码，断言 bit0|bit3=0x9，
//     确保"本次确实有负槽被改写"而不是用例没走到目标分支。
//   - 伤害带留足 85%~100% 浮动余量（比值带见《场景断言口径坑集》§2 绝对量口径）；
//     载体 accuracy=95 → 场景内统一改 100 + must_hit，排除 miss 干扰。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <entities/seer-robot.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <fsm/state.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa       = 10004;   // 拍打：物 40（填充刀）
constexpr int kIgnoreDrop  = 37097;   // [1750] 闪光一勺：物 140（纯链）
constexpr int kDropsAsUp   = 37235;   // [1874] 兵者诡道：物 130（纯链）
constexpr int kMaxHp       = 99999;

ElfPet mk_self(int carrier) {
    ElfPet p = make_pet(1, "己方", {carrier, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/300, /*hp=*/kMaxHp);
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
    return p;
}

ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/kMaxHp);
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
    return p;
}

// 敌方负等级设置：攻 -2、特攻 +1、防 0、特防 -3、速/命中 0。
void set_negative_levels(BattleContext& ctx) {
    ctx.ability_levels[0][0] = -2;
    ctx.ability_levels[0][1] = 1;
    ctx.ability_levels[0][2] = 0;
    ctx.ability_levels[0][3] = -3;
    ctx.ability_levels[0][4] = 0;
    ctx.ability_levels[0][5] = 0;
}

int audit_mask(BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return 0;
    }
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : 0;
}

// 无等级、完整一回合：返回对手掉血（伤害比值基线）。
int neutral_damage(int carrier) {
    ScenarioBattle b(make_party(mk_self(carrier)), make_party(mk_foe()));
    b.start();
    BattleContext& ctx = b.ctx();
    const int before = ctx.getPet(1).hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    return before - ctx.getPet(1).hp;
}

// 期望"生效后视图"：由调用方给出 6 槽，并断言效果前后与还原。
void run_view_case(int carrier, int save_audit_key, const int expect[6],
                   const char* label, int* damage_out) {
    ScenarioBattle b(make_party(mk_self(carrier)), make_party(mk_foe()));
    b.start();
    BattleContext& ctx = b.ctx();
    set_negative_levels(ctx);
    const int foe_before = ctx.getPet(1).hp;

    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 自身（先手）
    ctx.debug_step_mode = true;
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 对手（后手）
    b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);                   // 技能效果已结算
    ctx.debug_step_mode = false;

    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        CHECK_EQ(ctx.ws.view_levels[0][i], expect[i],
                 std::string(label) + " 生效后视图槽 " + std::to_string(i));
    }
    CHECK_EQ(audit_mask(ctx, save_audit_key), 0x9,
             std::string(label) + " 审计掩码 = 0x9（攻+特防两个负槽被改写）");

    b.drive();   // 跑完本回合（AFTER_ACTION 还原）
    CHECK_EQ(ctx.ws.view_levels[0][0], -2, std::string(label) + " 行动后攻槽还原 -2");
    CHECK_EQ(ctx.ws.view_levels[0][1], 1, std::string(label) + " 行动后特攻正槽保持 +1");
    CHECK_EQ(ctx.ws.view_levels[0][3], -3, std::string(label) + " 行动后特防槽还原 -3");
    *damage_out = foe_before - ctx.getPet(1).hp;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5670.a/b/c 1750 无视自身能力下降 ══
    {
        const int expect[6] = {0, 1, 0, 0, 0, 0};
        int dmg_neg = 0;
        run_view_case(kIgnoreDrop, 991750200, expect, "T5670.a 1750", &dmg_neg);
        const int dmg_neutral = neutral_damage(kIgnoreDrop);
        std::cout << "  [T5670] 1750 damage: neutral=" << dmg_neutral
                  << " neg(with 1750)=" << dmg_neg << std::endl;
        // 生效后攻等=0 → 与无等级同带（1×）。门失效时攻等=-2 → 0.5× 带。
        CHECK(dmg_neg > dmg_neutral * 7 / 10,
              "T5670.b 1750 负攻被无视 → 伤害保持 1× 带（实测 neg=" +
                  std::to_string(dmg_neg) + " neutral=" + std::to_string(dmg_neutral) + "）");
        CHECK(dmg_neg < dmg_neutral * 14 / 10,
              "T5670.c 1750 未出现异常增伤（实测 neg=" + std::to_string(dmg_neg) + "）");
    }

    // ══ T5670.d/e/f 1874 负等级视为同幅提升 ══
    {
        const int expect[6] = {2, 1, 0, 3, 0, 0};
        int dmg_neg = 0;
        run_view_case(kDropsAsUp, 991874200, expect, "T5670.d 1874", &dmg_neg);
        const int dmg_neutral = neutral_damage(kDropsAsUp);
        std::cout << "  [T5670] 1874 damage: neutral=" << dmg_neutral
                  << " neg(as +2)=" << dmg_neg << std::endl;
        // 攻等 -2 → 视为 +2 = 2×；门失效时是 0.5× 带。
        CHECK(dmg_neg > dmg_neutral * 15 / 10,
              "T5670.e 1874 负攻镜像为 +2 → 伤害 2× 带（实测 neg=" +
                  std::to_string(dmg_neg) + " neutral=" + std::to_string(dmg_neutral) + "）");
    }

    std::cout << "scenario_5670 PASS (1750/1874 自身负等级视图改写)" << std::endl;
    return 0;
}
