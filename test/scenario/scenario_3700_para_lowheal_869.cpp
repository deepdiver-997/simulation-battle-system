// 场景 3700 —— K226 攻击瘫痪概率+低血恢复族（869，soul_lib/k226_para_lowhp_heal.cpp）
//（2026-10-08，day 专项 worktree ../agents/k226；号段 3700-3709 → T3700 一块四例）
//
// 断言口径（2890 攻击窗同款驱动 + F9 审计槽无随机断言 + 3050 直铺手法）：
//   - a 概率梯度（审计槽精确断言）：满血攻击申报=30；掉到半血申报=30+50=80
//     （已损体力百分比附加，662 爬坡同族）——审计槽在掷骰前写，无随机；
//   - b 100% 档落地：血量压到 30%（30+70=100）→ 攻击后对手必瘫痪
//     （apply_anomaly_checked 100 档确定性，0/100 短路不耗 rand）；
//   - c 回合末恢复正例：血量 1/3 + 直铺提升 +2 → 过完回合回 1/3（精确等式）；
//   - d 负例：无提升不恢复 / 恰在半血门限（hp*2 ≥ max）不恢复（严格小于口径）。
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaDa = 10004;   // 拍打：filler
constexpr int kCrippled = static_cast<int>(AbnormalStatusId::Crippled);   // 瘫痪=19
constexpr int kMaxHp = 99999;
constexpr int kAuditBase = 990869000;   // +owner：申报概率审计槽（实现文件同锚）

SoulMark make_mark(const std::vector<int>& row_args) {
    SoulMark sm(869, "K226 魂印", "test", EffectArgs(row_args));
    sm.monster_id = 3255;   // 圣辉·达赫迪
    return sm;
}

// 自方：魂印宠、先手、攻击手；对手：不还手沙包。
ElfPet mark_pet() {
    ElfPet p = make_pet(1, "印主", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/300, /*atk=*/300, kMaxHp, make_mark({30, 2, 3, 1}));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/100, /*atk=*/1, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

int audit_pct(BattleContext& ctx) {
    const auto it = ctx.plugin_storage.find(kAuditBase + 0);
    return it == ctx.plugin_storage.end() ? -1 : std::any_cast<int>(it->second);
}

void one_round(ScenarioBattle& b, BattleFsm::ActionType act, int idx) {
    b.submit_operation(0, act, idx);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T3700.a 概率梯度（审计槽）：满血 30 → 半血 80 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        one_round(b, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(audit_pct(ctx), 30, "T3700.a 满血攻击申报 = 基础 30%");
        ctx.seerRobot[0].elfPets[0].hp = kMaxHp / 2;   // 已损 50% → 30+50
        one_round(b, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(audit_pct(ctx), 80, "T3700.a 半血申报 = 30 + 已损50%");
        std::cout << "  [T3700.a] 概率梯度 30/80 通过\n";
    }

    // ══ T3700.b 100% 档瘫痪落地（血量 30% → 30+70=100 必瘫）══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = kMaxHp * 3 / 10;
        one_round(b, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(audit_pct(ctx), 100, "T3700.b 申报 30+70=100");
        CHECK(ctx.has_active_abnormal_status(1, kCrippled),
              "T3700.b 100% 档 → 对手瘫痪落地");
        std::cout << "  [T3700.b] 100% 档瘫痪落地 通过\n";
    }

    // ══ T3700.c 回合末恢复正例：1/3 血 + 有提升 → 回 1/3（精确等式）══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = kMaxHp / 3;
        ctx.ability_levels[0][0] = 2;   // 直铺提升（3052 同手法）
        one_round(b, BattleFsm::ActionType::NONE, -1);
        CHECK_EQ(ctx.getPet(0).hp, kMaxHp / 3 * 2,
                 "T3700.c 低于半血且有提升 → 回合末回 max/3");
        std::cout << "  [T3700.c] 回合末恢复 通过\n";
    }

    // ══ T3700.d 负例：无提升 / 恰在门限 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = kMaxHp / 3;   // 低于门限但无提升
        one_round(b, BattleFsm::ActionType::NONE, -1);
        CHECK_EQ(ctx.getPet(0).hp, kMaxHp / 3, "T3700.d1 无能力提升不恢复");
    }
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = kMaxHp / 2 + 1;   // hp*2 > max：恰在门限外
        ctx.ability_levels[0][0] = 2;
        one_round(b, BattleFsm::ActionType::NONE, -1);
        CHECK_EQ(ctx.getPet(0).hp, kMaxHp / 2 + 1, "T3700.d2 恰在半血门限外不恢复（严格小于）");
        std::cout << "  [T3700.d] 负例（无提升/门限外）通过\n";
    }

    std::cout << "scenario_3700 PASS (K226 攻击瘫痪概率+低血恢复族)" << std::endl;
    return 0;
}
