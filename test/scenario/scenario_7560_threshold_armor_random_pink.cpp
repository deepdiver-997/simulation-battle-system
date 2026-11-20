// 场景 7560 —— J316 门槛盔与随机固伤族（1664+1526，moves_lib/j_threshold_armor_random_pink.cpp）
//（2026-11-02，worktree ../agents/j316；号段 7560-7569）
//
// 断言口径（046 直系伤害公式 + 审计槽精确 + 被拦=0 伤恒等）：
//   - 1664 模板：当回合若自身先出手则令对手使用的威力低于{0}的攻击技能无效，
//     若击败对手则攻击无效效果延续至下回合。
//     载体：29051 千劫战意（[191,47,1664]|门槛160，属性技 p=1——链 191/47 自体防御窗，
//     不碰对手，良性）/ 34881 暴龙怒吼（[1061,1664]|门槛100，特攻 power85 p=3——链 1061
//     反转对手强化，对手无强化=消除空操作，良性）/ 38278 绝招·马踏飞燕（[1777,1664]
//     |门槛150，物攻 power160 p=0——链 1777 概率异常只在对手无回合类效果时掷骰，
//     本场景 R1 即断言或目标已死，不扰后续）。
//   - 1526 模板：随机附加{0}-{1}点固定伤害（后缀减PP子句内核缺口记档停，见实现头注）。
//     载体：35732 智眼解析（[1526]|[200,400] 纯链物攻）/ 26986 菌丝体期（[1526]|[100,400,1]
//     纯链属性技——第三参 1 为官方冗余，arg_count=2 只消费前两位）。
//   - 单段红伤理论区间（restraint 钉 1.0、无暴击、无增减伤，046 直系）：
//     (0.84 × atk/100 × 威力 + 2) × 浮动[217,255]/255。
//   - 1526 固伤 = 审计槽精确值；掉血恒等式 = 红伤带 + 审计值。
//   - 1664 被拦 = 对手 hp 精确不变（SKILL_INVALID 不进伤害管线）。
//   - 1526 审计键 = 990152600+owner（实现同款常量）。
#include <any>
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kQianJie    = 29051;  // 千劫战意：属性技 [191,47,1664]|[5,5,160]
constexpr int kLongNuHou  = 34881;  // 暴龙怒吼：特攻85 [1061,1664]|[100]
constexpr int kMaTaFeiYan = 38278;  // 绝招·马踏飞燕：物攻160 [1777,1664]|[80,6,150]
constexpr int kZhiYan     = 35732;  // 智眼解析：物攻130 [1526]|[200,400]
constexpr int kJunSi      = 26986;  // 菌丝体期：属性技 [1526]|[100,400,1]
constexpr int kPaiDa      = 10004;  // 拍打：物 40（低威力对照刀）
constexpr int kZhaoLong   = 10376;  // 爪龙连牙弹：特 110（门槛外对照刀，side_effect=[]）
constexpr int kAuditBase  = 990152600;  // 1526 审计槽（实现同款）
constexpr int kBigHp      = 99999;

// ── 046 直系：单段红伤理论区间（def 恒 100）──
void expect_range(int atk, int power, int* lo, int* hi) {
    const double raw = 0.84 * atk / 100.0 * power + 2.0;
    *lo = static_cast<int>(raw * 217.0 / 255.0) - 2;
    *hi = static_cast<int>(raw) + 2;
}

// 双侧钉 restraint=1.0（ROUND_START 桶每回合重写——ws.reset() memset 会冲掉，046 同款）。
EffectResult pin_restraint_tick(BattleContext* ctx, const EffectArgs&) {
    if (ctx) {
        ctx->ws.restraint_view[0] = 1.0;
        ctx->ws.restraint_view[1] = 1.0;
    }
    return EffectResult::kOk;
}

void register_restraint_pin(BattleContext& ctx, int owner) {
    Effect e(9997560, /*priority=*/0, owner, /*left_round=*/1,
             EffectArgs(std::vector<int>{owner, 1 - owner}), &pin_restraint_tick);
    auto ce = std::make_unique<ContinuousEffect>(e, State::BATTLE_ROUND_START, owner,
                                                 /*duration=*/9, ctx.roundCount);
    ce->source_id_ = 9997560;
    ctx.register_skill_effect(State::BATTLE_ROUND_START, owner, std::move(ce));
}

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

int audit_or(BattleContext& ctx, int key, int fallback) {
    auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return fallback;
    }
    if (const int* p = std::any_cast<int>(&it->second)) {
        return *p;
    }
    return fallback;   // 类型不符按缺失语义回退（5530 同款，不抛）
}

ElfPet self_pet(int skill0, int speed, int atk, int hp) {
    ElfPet p = make_pet(1, "己方", {skill0, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        speed, atk, hp);
    accuracy_100(p, /*must_hit=*/true);
    set_crit_sixteenths(p, 0);
    return p;
}

ElfPet foe_pet(int atk, int hp, int speed = 100) {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kZhaoLong, kPaiDa, kPaiDa, kPaiDa},
                        speed, atk, hp);
    accuracy_100(p, /*must_hit=*/true);
    set_crit_sixteenths(p, 0);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7560.a 1664 基础拦截：先手属性技载体 → 对手低威力攻击技无效 ════════
    {
        // 千劫战意 p=1（先制>0 先手）；对手拍打 40 < 门槛160 → 被拦。
        ScenarioBattle b(make_party(self_pet(kQianJie, /*speed=*/300, /*atk=*/1, kBigHp)),
                         make_party(foe_pet(/*atk=*/300, kBigHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        register_restraint_pin(ctx, 0);
        register_restraint_pin(ctx, 1);
        const int foe_before = ctx.getPet(1).hp;
        const int self_before = ctx.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 千劫战意
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 拍打
        b.drive();
        CHECK_EQ(ctx.getPet(1).hp, foe_before,
                 "T7560.a 1664：对手拍打(40<160)被拦 → 对手不掉血（实测 " +
                     std::to_string(ctx.getPet(1).hp) + "，前 " + std::to_string(foe_before) + "）");
        CHECK_EQ(ctx.getPet(0).hp, self_before,
                 "T7560.a 属性技无伤害阶段 + 拦截生效 → 己方不掉血（实测 " +
                     std::to_string(ctx.getPet(0).hp) + "）");
        std::cout << "  [T7560.a] 先手门槛盔拦截低威力攻击 ✓\n";
    }

    // ══ T7560.b 1664 门槛边界：威力 ≥ 门槛的攻击技不被拦（面板威力口径）══════
    {
        // 暴龙怒吼 p=3 先手（特攻85 红伤）+ 注册门槛100盔；对手爪龙连牙弹 110>100 不拦。
        ScenarioBattle b(make_party(self_pet(kLongNuHou, /*speed=*/300, /*atk=*/300, kBigHp)),
                         make_party(foe_pet(/*atk=*/300, kBigHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        register_restraint_pin(ctx, 0);
        register_restraint_pin(ctx, 1);
        const int foe_before = ctx.getPet(1).hp;
        const int self_before = ctx.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 暴龙怒吼
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);   // 爪龙连牙弹 110
        b.drive();
        int lo = 0, hi = 0;
        expect_range(300, 110, &lo, &hi);
        const int taken = self_before - ctx.getPet(0).hp;
        CHECK(taken >= lo && taken <= hi,
              "T7560.b 门槛外(110>100)不被拦 → 己方吃满红伤带 [" + std::to_string(lo) + "," +
                  std::to_string(hi) + "]（实测 " + std::to_string(taken) + "）");
        expect_range(300, 85, &lo, &hi);
        const int dealt = foe_before - ctx.getPet(1).hp;
        CHECK(dealt >= lo && dealt <= hi,
              "T7560.b 暴龙怒吼红伤带 [" + std::to_string(lo) + "," + std::to_string(hi) +
                  "]（实测 " + std::to_string(dealt) + "）");
        std::cout << "  [T7560.b] 门槛外攻击不被拦（110>100）+ 载体红伤带 ✓\n";
    }

    // ══ T7560.c 1664 后手：对手先手刀不被拦（发生在注册时点之前）════════════
    {
        // 马踏飞燕 p=0；己方 speed100 vs 对手 speed300 → 对手先手拍打（40<150 但盔不存在）。
        ScenarioBattle b(make_party(self_pet(kMaTaFeiYan, /*speed=*/100, /*atk=*/300, kBigHp)),
                         make_party(foe_pet(/*atk=*/300, kBigHp, /*speed=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        register_restraint_pin(ctx, 0);
        register_restraint_pin(ctx, 1);
        const int foe_before = ctx.getPet(1).hp;
        const int self_before = ctx.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 马踏飞燕
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 拍打
        b.drive();
        int lo = 0, hi = 0;
        expect_range(300, 40, &lo, &hi);
        const int taken = self_before - ctx.getPet(0).hp;
        CHECK(taken >= lo && taken <= hi,
              "T7560.c 后手：对手先手刀不被拦 → 己方吃带 [" + std::to_string(lo) + "," +
                  std::to_string(hi) + "]（实测 " + std::to_string(taken) + "）");
        expect_range(300, 160, &lo, &hi);
        const int dealt = foe_before - ctx.getPet(1).hp;
        CHECK(dealt >= lo && dealt <= hi,
              "T7560.c 马踏飞燕红伤带 [" + std::to_string(lo) + "," + std::to_string(hi) +
                  "]（实测 " + std::to_string(dealt) + "）");
        std::cout << "  [T7560.c] 后手不注册，先手刀照常命中 ✓\n";
    }

    // ══ T7560.d 1664 后手击败：无延续盔（「先出手」门的可观测后果）══════════
    {
        // 对手 hp=80：R1 对手先手拍打 → 我后手马踏飞燕红伤(>80)击败；
        // R2 对手第二只拍打 → 无延续盔（后手未注册）→ 照常吃带。
        ScenarioBattle b(make_party(self_pet(kMaTaFeiYan, /*speed=*/100, /*atk=*/300, kBigHp)),
                         make_party(foe_pet(/*atk=*/300, /*hp=*/80, /*speed=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        register_restraint_pin(ctx, 0);
        register_restraint_pin(ctx, 1);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(ctx.getPet(1).hp < 80,
              "T7560.d 前置：R1 已击败对手第一只（实测 hp " + std::to_string(ctx.getPet(1).hp) + "）");
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);   // 死后补位第二只
        b.drive();   // 补位 + 推进到 R2 选择期
        const int self_before_r2 = ctx.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R2 新宠拍打
        b.drive();
        int lo = 0, hi = 0;
        expect_range(300, 40, &lo, &hi);
        const int taken_r2 = self_before_r2 - ctx.getPet(0).hp;
        CHECK(taken_r2 >= lo && taken_r2 <= hi,
              "T7560.d 后手击败无延续 → R2 拍打照常吃带 [" + std::to_string(lo) + "," +
                  std::to_string(hi) + "]（实测 " + std::to_string(taken_r2) + "）");
        std::cout << "  [T7560.d] 后手击败不延续 ✓\n";
    }

    // ══ T7560.e 1664 先手击败延续：延续盔管到下回合、第三回合过期 ═══════════
    {
        // 暴龙怒吼 p=3 先手红伤(>80)击败对手 hp80 → EVENT_DEATH → 票刷新 rounds=2；
        // R2 对手新宠拍打(40<100)被拦（延续）；R3 拍打照常（下回合末已过期）。
        ScenarioBattle b(make_party(self_pet(kLongNuHou, /*speed=*/300, /*atk=*/300, kBigHp)),
                         make_party(foe_pet(/*atk=*/300, /*hp=*/80)));
        b.start();
        BattleContext& ctx = b.ctx();
        register_restraint_pin(ctx, 0);
        register_restraint_pin(ctx, 1);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 暴龙怒吼（击败）
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(ctx.getPet(1).hp < 80,
              "T7560.e 前置：R1 已击败对手第一只（实测 hp " + std::to_string(ctx.getPet(1).hp) + "）");
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);   // 死后补位第二只
        b.drive();   // 补位 + 推进到 R2 选择期
        const int self_before_r2 = ctx.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R2 拍打
        b.drive();
        CHECK_EQ(ctx.getPet(0).hp, self_before_r2,
                 "T7560.e 延续盔：R2 拍打(40<100)被拦 → 己方不掉血（实测掉 " +
                     std::to_string(self_before_r2 - ctx.getPet(0).hp) + "）");
        const int self_before_r3 = ctx.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R3 拍打
        b.drive();
        int lo = 0, hi = 0;
        expect_range(300, 40, &lo, &hi);
        const int taken_r3 = self_before_r3 - ctx.getPet(0).hp;
        CHECK(taken_r3 >= lo && taken_r3 <= hi,
              "T7560.e 延续盔下回合末过期 → R3 拍打照常吃带 [" + std::to_string(lo) + "," +
                  std::to_string(hi) + "]（实测 " + std::to_string(taken_r3) + "）");
        std::cout << "  [T7560.e] 先手击败 → 延续至下回合 → 第三回合过期 ✓\n";
    }

    // ══ T7560.f 1526 攻击技载体：红伤 + 随机固伤（审计槽精确恒等）═══════════
    {
        // 智眼解析 [1526]|[200,400] 纯链物攻130：掉血 = 红伤带 + 审计 roll。
        ScenarioBattle b(make_party(self_pet(kZhiYan, /*speed=*/300, /*atk=*/300, kBigHp)),
                         make_party(foe_pet(/*atk=*/1, kBigHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        register_restraint_pin(ctx, 0);
        register_restraint_pin(ctx, 1);
        const int foe_before = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 智眼解析
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        const int amount = audit_or(ctx, kAuditBase + 0, -1);
        CHECK(amount >= 200 && amount <= 400,
              "T7560.f 1526 审计抽取 ∈ [200,400]（实测 " + std::to_string(amount) + "）");
        int lo = 0, hi = 0;
        expect_range(300, 130, &lo, &hi);
        const int red_part = (foe_before - ctx.getPet(1).hp) - amount;
        CHECK(red_part >= lo && red_part <= hi,
              "T7560.f 掉血-固伤 == 红伤带 [" + std::to_string(lo) + "," + std::to_string(hi) +
                  "]（实测红份 " + std::to_string(red_part) + "，总掉 " +
                  std::to_string(foe_before - ctx.getPet(1).hp) + "，审计 " +
                  std::to_string(amount) + "）");
        std::cout << "  [T7560.f] 固伤 roll=" << amount << " + 红伤恒等 ✓\n";
    }

    // ══ T7560.g 1526 属性技载体：无红伤，掉血 == 审计 roll（第三参冗余记档）═══
    {
        // 菌丝体期 [1526]|[100,400,1]：arg_count=2 只消费 [100,400]；属性技无伤害阶段。
        ScenarioBattle b(make_party(self_pet(kJunSi, /*speed=*/300, /*atk=*/1, kBigHp)),
                         make_party(foe_pet(/*atk=*/1, kBigHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        register_restraint_pin(ctx, 0);
        register_restraint_pin(ctx, 1);
        const int foe_before = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 菌丝体期
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        const int amount = audit_or(ctx, kAuditBase + 0, -1);
        CHECK(amount >= 100 && amount <= 400,
              "T7560.g 1526 审计抽取 ∈ [100,400]（实测 " + std::to_string(amount) + "）");
        CHECK_EQ(ctx.getPet(1).hp, foe_before - amount,
                 "T7560.g 属性技掉血 == 审计抽取（实测 " + std::to_string(ctx.getPet(1).hp) +
                     "，应 " + std::to_string(foe_before - amount) + "）");
        std::cout << "  [T7560.g] 属性技纯固伤 roll=" << amount << " ✓\n";
    }

    std::cout << "scenario_7560 all green" << std::endl;
    return 0;
}
