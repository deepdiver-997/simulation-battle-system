// 场景 6240 —— J291 必暴窗族（623 / 493，moves_lib/j_must_crit_window.cpp）
//（2026-10-22，worktree feat/jbatch；号段 6240-6249）
//
// 断言口径：
//   - 必暴是强制的（无掷点，crit_rate 被 harness 清零）→ 以 `ctx.crit_happened[owner]`
//     行为级断言（e_must_crit 场景 480 同法）；负向断言（窗外不暴）靠 crit_rate=0 可靠。
//   - 623（16288 神光爆裂掌，纯链 [1]）：先给自身塞能力下降（ability_levels 本体，
//     clear_stat_drops 读本体）→ 用技清除成功 → 下 1 回合起必暴；门闭（无下降）
//     审计槽 0、不开窗。
//   - 493（17630 火狮怒爪击，纯链 [2,2]）：窗内对手用攻击技 → 自身下 2 回合必暴；
//     对手用属性技 / 窗外用攻击技均不触发（used_attribute + 回合窗双重门）。
//   - 审计槽 996230000/996230100（623 清除数/开窗数）、994930000/994930100
//     （493 触发/候选数）与行为断言双向核对。
#include <any>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;    // 拍打：物 40（填充刀）
constexpr int kDengYan = 20004;  // 瞪眼：属性技（used_attribute 门）
constexpr int k623Skill = 16288; // 神光爆裂掌 [623]|[1] 物150
constexpr int k493Skill = 17630; // 火狮怒爪击 [493]|[2,2] 物150

constexpr int k623Cleared = 996230000;
constexpr int k623Armed = 996230100;
constexpr int k493Trigger = 994930000;
constexpr int k493Seen = 994930100;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_self(int skill, int speed = 300, int hp = 5000) {
    ElfPet p = make_pet(1, "己方", {skill, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, speed,
                        /*atk=*/1, hp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe(int first_skill = kPaiDa, int speed = 100, int hp = 20000) {
    ElfPet p = make_pet(2, "对手", {first_skill, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, speed,
                        /*atk=*/1, hp);
    accuracy_100(p, /*must_hit=*/true);
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

void run(ScenarioBattle& b, int skill0, BattleFsm::ActionType op1, int skill1) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, skill0);
    b.submit_operation(1, op1, skill1);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T6240 623 清除成功 → 下1回合必暴；门闭不开窗 ══
    {
        {   // (a) 自身有 2 项下降 → 清除成功 → 下回合必暴
            ScenarioBattle b(make_party(mk_self(k623Skill)), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.ability_levels[0][0] = -2;   // 本体能力下降（clear_stat_drops 读它）
            ctx.ability_levels[0][2] = -1;
            run(b, 0, BattleFsm::ActionType::NONE, -1);
            CHECK_EQ(stor(ctx, k623Cleared), 2, "T6240.a 623：清除 2 项弱化");
            CHECK_EQ(ctx.ability_levels[0][0], 0, "T6240.a 623：攻击槽已清零");
            CHECK_EQ(ctx.ability_levels[0][2], 0, "T6240.a 623：防御槽已清零");
            CHECK_EQ(stor(ctx, k623Armed), 1, "T6240.a 623：开窗 1 次");
            CHECK(ctx.crit_happened[0] == false,
                  "T6240.a 623：本回合（NextRounds 窗外）不暴击");

            run(b, 1, BattleFsm::ActionType::NONE, -1);   // R2 拍打
            CHECK(ctx.crit_happened[0] == true,
                  "T6240.a 623：消除成功 → 下1回合直接攻击必定暴击");
            run(b, 1, BattleFsm::ActionType::NONE, -1);   // R3 窗外
            CHECK(ctx.crit_happened[0] == false,
                  "T6240.a 623：窗过期（rounds=1）恢复常规掷点");
        }
        {   // (b) 门闭：无能力下降 → 不开窗
            ScenarioBattle b(make_party(mk_self(k623Skill)), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            run(b, 0, BattleFsm::ActionType::NONE, -1);
            CHECK_EQ(stor(ctx, k623Cleared), 0, "T6240.b 623 门：无弱化 → 清除数 0");
            CHECK_EQ(stor(ctx, k623Armed), 0, "T6240.b 623 门：不开窗");
            run(b, 1, BattleFsm::ActionType::NONE, -1);
            CHECK(ctx.crit_happened[0] == false,
                  "T6240.b 623 门：未消除成功 → 下回合不暴击");
        }
    }

    // ══ T6241 493 窗内对方用攻击技 → 自身下2回合必暴；属性技/窗外不触发 ══
    {
        {   // (a) R1 对方用攻击技 → 触发；R2/R3 必暴，R4 过期
            ScenarioBattle b(make_party(mk_self(k493Skill)), make_party(mk_foe(kPaiDa)));
            b.start();
            BattleContext& ctx = b.ctx();
            run(b, 0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 我方 493，对方拍打
            CHECK_EQ(stor(ctx, k493Trigger), 1, "T6241.a 493：窗内对方攻击技触发 1 次");
            CHECK(ctx.crit_happened[0] == false,
                  "T6241.a 493：本回合（NextRounds 窗外）不暴击");
            run(b, 1, BattleFsm::ActionType::NONE, -1);   // R2
            CHECK(ctx.crit_happened[0] == true,
                  "T6241.a 493：触发后下1回合必暴");
            run(b, 1, BattleFsm::ActionType::NONE, -1);   // R3
            CHECK(ctx.crit_happened[0] == true,
                  "T6241.a 493：触发后下2回合仍必暴（buff=2）");
            run(b, 1, BattleFsm::ActionType::NONE, -1);   // R4
            CHECK(ctx.crit_happened[0] == false,
                  "T6241.a 493：必暴窗过期恢复常规掷点");
        }
        {   // (b) 门闭：对方用属性技（used_attribute）→ 不触发
            ScenarioBattle b(make_party(mk_self(k493Skill)), make_party(mk_foe(kDengYan)));
            b.start();
            BattleContext& ctx = b.ctx();
            run(b, 0, BattleFsm::ActionType::SELECT_SKILL, 0);
            CHECK_EQ(stor(ctx, k493Trigger), 0, "T6241.b 493 门：对方属性技不触发");
            run(b, 1, BattleFsm::ActionType::NONE, -1);
            CHECK(ctx.crit_happened[0] == false,
                  "T6241.b 493 门：属性技不触发 → 下回合不暴击");
        }
        {   // (c) 门闭：对方 R1/R2 不出手，R3（窗外）才用攻击技 → 不触发
            ScenarioBattle b(make_party(mk_self(k493Skill)), make_party(mk_foe(kPaiDa)));
            b.start();
            BattleContext& ctx = b.ctx();
            run(b, 0, BattleFsm::ActionType::NONE, -1);            // R1 对方不出手
            run(b, 1, BattleFsm::ActionType::NONE, -1);            // R2 窗外
            run(b, 1, BattleFsm::ActionType::SELECT_SKILL, 0);     // R3 窗外攻击
            CHECK_EQ(stor(ctx, k493Trigger), 0, "T6241.c 493 门：窗外攻击技不触发");
            run(b, 1, BattleFsm::ActionType::NONE, -1);            // R4
            CHECK(ctx.crit_happened[0] == false,
                  "T6241.c 493 门：窗外触发不生效 → 不暴击");
        }
    }

    std::cout << "scenario_6240_must_crit_window: ALL PASS\n";
    return 0;
}
