// 场景 5920 —— J261 异常门全额吸血族（575，moves_lib/j_abnormal_full_lifesteal.cpp）
//（2026-10-22，worktree feat/jbatch；号段 5920-5929）
//
// 断言口径（精确 HP 快照 + 实际落血反推）：
//   - 载体 15675 虚空轮回 [575]（纯链、特攻 130、必中）；对照刀 10004 拍打（物攻 40）。
//   - T5920.a：对手麻痹(0) → 用 575 载体打出的**实际落血**全额回复己方体力
//     （hp = 40000 + dealt，逐值精确）；审计 996575100+0 == 1。
//   - T5920.b：对手无异常 → 同一刀照样落地，但己方体力**不变**（绝对值断言，
//     不用"与对照局相等"）；审计 == 0。
//   - T5920.c：对手害怕(6)（另一种异常）→ 同样全额回复，证明"任意异常"门。
//   - T5920.d：R1 麻痹门 + 575 载体回复；R2 重铺麻痹、改用**非 575** 的拍打 →
//     伤害照样落地但不再回血（窗口 = 本回合，registered duration=1 过期）。
//   - T5920.e：异常在**己方**身上（衰弱 11）、对手无异常 → 不回血、审计 0
//     （门判对手而不是自己）。
//   - T5920.f：对手睡眠(8) → **不回血**。睡眠在 ON_SKILL_HIT 即被解除，早于
//     SKILL_EFFECT（battleFsm:231-234），门自然为闭；这是"命中即醒"（K2）在
//     本族的直接体现，锁住"门看的是伤害发生时对手的实际状态"。
//
// 观测面：make_pet 的 hp 同时是最大体力与初始体力（坑集 §4）——统一满血构造后
// 单独压当前 hp 到 40000，留足回血空间，避免上限 clamp 掩盖差异。
#include <any>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <entities/seer-robot.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kCarrier575 = 15675;   // 虚空轮回 [575]：特攻 130，必中
constexpr int kPaiDa      = 10004;   // 拍打：物攻 40（对照刀／R2 非 575 刀）
constexpr int kFengLi     = 20265;   // 锋利：属性技（对手填充，不产生伤害）
constexpr int kMaxHp      = 99999;
constexpr int kStartHp    = 40000;   // 当前体力（满血构造后再压，留回血空间）
constexpr int kAudit575   = 996575100;   // +owner：1=门过 / 0=门闭

constexpr int kSleep     = static_cast<int>(AbnormalStatusId::Sleep);      // 8
constexpr int kParalysis = static_cast<int>(AbnormalStatusId::Paralysis);  // 0
constexpr int kFear      = static_cast<int>(AbnormalStatusId::Fear);       // 6
constexpr int kWeakness  = static_cast<int>(AbnormalStatusId::Weakness);   // 11

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
    ElfPet p = make_pet(2, "对手", {kFengLi, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/kMaxHp);
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
    return p;
}

int audit_value(const BattleContext& ctx, int owner) {
    const auto it = ctx.plugin_storage.find(kAudit575 + owner);
    if (it == ctx.plugin_storage.end()) {
        return -999;
    }
    if (const int* p = std::any_cast<int>(&it->second)) {
        return *p;
    }
    return -998;
}

struct RoundResult {
    int dealt = 0;
    int hp_after = 0;
};

// 跑一回合：owner 用自己技能槽 slot，对手用槽 0（锋利/属性技，不反打）。
RoundResult run_round(ScenarioBattle& b, int slot) {
    BattleContext& ctx = b.ctx();
    const int foe_hp_before = ctx.getPet(1).hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, slot);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    RoundResult r;
    r.dealt = foe_hp_before - ctx.getPet(1).hp;
    r.hp_after = ctx.getPet(0).hp;
    return r;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5920.a 对手麻痹门 → 全额吸血 ══
    {
        ScenarioBattle b(make_party(mk_self(kCarrier575)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = kStartHp;
        ctx.apply_abnormal_status_for_rounds(1, kParalysis, 3);
        const RoundResult r = run_round(b, /*slot=*/0);

        std::cout << "  [T5920.a] paralysis gate -> dealt=" << r.dealt
                  << " hp=" << r.hp_after << " audit=" << audit_value(ctx, 0) << std::endl;
        CHECK(r.dealt > 0, "T5920.a 前提：575 载体打出红伤");
        CHECK_EQ(r.hp_after, kStartHp + r.dealt, "T5920.a 麻痹门 → 实际落血全额回复");
        CHECK_EQ(audit_value(ctx, 0), 1, "T5920.a 审计：门过=1");
    }

    // ══ T5920.b 对手无异常 → 不回血（绝对值断言）══
    {
        ScenarioBattle b(make_party(mk_self(kCarrier575)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = kStartHp;
        const RoundResult r = run_round(b, /*slot=*/0);

        std::cout << "  [T5920.b] no anomaly -> dealt=" << r.dealt
                  << " hp=" << r.hp_after << " audit=" << audit_value(ctx, 0) << std::endl;
        CHECK(r.dealt > 0, "T5920.b 前提：伤害照样落地");
        CHECK_EQ(r.hp_after, kStartHp, "T5920.b 无异常 → 己方体力不变（绝对值）");
        CHECK_EQ(audit_value(ctx, 0), 0, "T5920.b 审计：门闭=0");
    }

    // ══ T5920.c 任意异常（害怕 6）→ 同样全额吸血 ══
    {
        ScenarioBattle b(make_party(mk_self(kCarrier575)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = kStartHp;
        ctx.apply_abnormal_status_for_rounds(1, kFear, 3);
        const RoundResult r = run_round(b, /*slot=*/0);

        std::cout << "  [T5920.c] fear gate -> dealt=" << r.dealt
                  << " hp=" << r.hp_after << " audit=" << audit_value(ctx, 0) << std::endl;
        CHECK(r.dealt > 0, "T5920.c 前提：575 载体打出红伤");
        CHECK_EQ(r.hp_after, kStartHp + r.dealt, "T5920.c 害怕(6) 也属\"任意异常\" → 全额回复");
        CHECK_EQ(audit_value(ctx, 0), 1, "T5920.c 审计：门过=1");
    }

    // ══ T5920.d 本回合窗：R1 回血，R2 换非 575 刀 → 不再回血 ══
    {
        ScenarioBattle b(make_party(mk_self(kCarrier575)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = kStartHp;
        ctx.apply_abnormal_status_for_rounds(1, kParalysis, 3);
        const RoundResult r1 = run_round(b, /*slot=*/0);
        CHECK(r1.dealt > 0, "T5920.d 前提：R1 575 载体打出红伤");
        CHECK_EQ(r1.hp_after, kStartHp + r1.dealt, "T5920.d R1 麻痹门 → 全额回复");
        const int hp_after_r1 = r1.hp_after;

        // R2：重铺麻痹。改用拍打（无 575），伤害照样落地但窗口已过期。
        ctx.apply_abnormal_status_for_rounds(1, kParalysis, 3);
        const RoundResult r2 = run_round(b, /*slot=*/1);

        std::cout << "  [T5920.d] R1 heal=" << r1.dealt << " -> hp=" << hp_after_r1
                  << " | R2 dealt=" << r2.dealt << " hp=" << r2.hp_after << std::endl;
        CHECK(r2.dealt > 0, "T5920.d 前提：R2 非 575 刀打出红伤");
        CHECK_EQ(r2.hp_after, hp_after_r1, "T5920.d 窗口只覆盖施放当回合 → R2 不回血");
    }

    // ══ T5920.e 门判对手：异常在己方身上 → 不回血 ══
    {
        ScenarioBattle b(make_party(mk_self(kCarrier575)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = kStartHp;
        ctx.apply_abnormal_status_for_rounds(0, kWeakness, 3);   // 己方衰弱，对手干净
        const RoundResult r = run_round(b, /*slot=*/0);

        std::cout << "  [T5920.e] self anomaly only -> dealt=" << r.dealt
                  << " hp=" << r.hp_after << " audit=" << audit_value(ctx, 0) << std::endl;
        CHECK(r.dealt > 0, "T5920.e 前提：575 载体照样出手");
        CHECK_EQ(r.hp_after, kStartHp, "T5920.e 己方有异常、对手无异常 → 不回血");
        CHECK_EQ(audit_value(ctx, 0), 0, "T5920.e 审计：门闭=0（门判对手）");
    }

    // ══ T5920.f 睡眠门：ON_SKILL_HIT 已解除睡眠（早于 SKILL_EFFECT）→ 门闭 ══
    {
        ScenarioBattle b(make_party(mk_self(kCarrier575)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = kStartHp;
        ctx.apply_abnormal_status_for_rounds(1, kSleep, 3);
        const RoundResult r = run_round(b, /*slot=*/0);

        std::cout << "  [T5920.f] sleep gate -> dealt=" << r.dealt
                  << " hp=" << r.hp_after << " audit=" << audit_value(ctx, 0) << std::endl;
        CHECK(r.dealt > 0, "T5920.f 前提：575 载体打出红伤");
        CHECK_EQ(r.hp_after, kStartHp, "T5920.f 睡眠在 ON_SKILL_HIT 已解 → 不回血");
        CHECK_EQ(audit_value(ctx, 0), 0, "T5920.f 审计：门闭=0（K2 命中即醒时点）");
    }

    std::cout << "scenario_5920 PASS (575 异常门全额吸血)" << std::endl;
    return 0;
}
