// 场景 6190 —— J286 对手攻击未命中异常窗族（1466，moves_lib/j_miss_anomaly_window.cpp）
//（2026-10-22，worktree feat/jbatch；号段 6190-6199 → T6190.a-e 五块）
//
// 断言口径（沿用 J62 场景 1480 的窗式驱动 + 双局绝对值对照）：
//   - 载体 怒灵咆哮 27209 [1283,1466,576,783]|[3,100,6] → {0}=3 回合、{1}=100、{2}=6 害怕；
//     链上 1283 自身全属性+、576 低伤免疫、783 被消强反伤，均为自身面，不扰对手异常断言。
//   - 对手攻击技能 accuracy=0 → hit_chance=0 → 必 MISS → EVENT_SKILL_INVALID（actor=对手）。
//   - a) 同回合触发：R1 己方挂窗 → 对手攻击 MISS → 对手害怕(6) + 审计 991466200+0 == 1。
//   - b) 命中对照：对手 accuracy=100 → 命中落地 → 无害怕 + 审计槽缺席（绝对值）。
//   - c) 攻击技过滤：对手用属性技（瞪眼 20004）accuracy=0 → 同样 SKILL_INVALID，
//        但属性技的无效不算 → 无害怕 + 审计缺席。
//   - d) 窗持续性：R1 对手命中（不触发），R2 对手 MISS → 仍触发（窗 [3] 覆盖 R2）。
//   - e) 窗过期：R1 命中，R2-R4 空转，R5 对手 MISS → 不再触发（绝对值）。
//   - 变异判据：拆攻击技过滤 → c 转红；拆窗时长（永久）→ e 转红；拆概率/apply → a/d 转红。
#include <any>
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;    // 拍打：物 40
constexpr int kDengYan = 20004;  // 瞪眼：属性技（改对手防御）
constexpr int kNuLing = 27209;   // 怒灵咆哮 [1283,1466,576,783]|[3,100,6]

constexpr int kAudit = 991466200;   // +self：累计异常落地次数
constexpr int kFear = static_cast<int>(AbnormalStatusId::Fear);   // 6

int storage_int(BattleContext& ctx, int key, int fallback) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return fallback;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : fallback;
}

void set_accuracy(ElfPet& p, int acc) {
    for (Skills& s : p.skills) {
        s.accuracy = acc;
        s.must_hit = 0;
    }
}

ElfPet mk_self() {
    ElfPet p = make_pet(1, "己方", {kNuLing, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, /*hp=*/50000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;   // 己方命中不影响断言（对手才是 miss 方）
        s.accuracy = 100;
    }
    return p;
}

ElfPet mk_foe(int first_skill) {
    ElfPet p = make_pet(2, "对手", {first_skill, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/50000);
    set_accuracy(p, 100);
    return p;
}

// R：己方挂窗（属性技）/ 对手出招（槽 0）。
void drive_carrier_and_foe(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

// R：己方拍打 / 对手出招（槽 0）。
void drive_attack_and_foe(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

// R：己方拍打 / 对手 NONE（空转推进回合）。
void drive_idle(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T6190.a 同回合：对手攻击 MISS → 害怕 ══
    {
        ScenarioBattle b(make_party(mk_self()), make_party(mk_foe(kPaiDa)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_accuracy(ctx.getPet(1), 0);   // 对手必 MISS
        drive_carrier_and_foe(b);
        CHECK(ctx.has_active_abnormal_status(1, kFear),
              "T6190.a 1466：对手攻击 MISS → 害怕(6)在身");
        CHECK_EQ(storage_int(ctx, kAudit + 0, -1), 1,
                 "T6190.a 1466：审计落地次数=1");
        std::cout << "  [T6190.a] 1466 同回合未命中→害怕 通过\n";
    }

    // ══ T6190.b 命中对照：不触发（绝对值）══
    {
        ScenarioBattle b(make_party(mk_self()), make_party(mk_foe(kPaiDa)));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_carrier_and_foe(b);   // 对手 accuracy=100 → 命中
        CHECK(!ctx.has_active_abnormal_status(1, kFear),
              "T6190.b 1466 对照：命中 → 不附加害怕（绝对值）");
        CHECK_EQ(storage_int(ctx, kAudit + 0, -1), -1,
                 "T6190.b 1466 对照：审计槽缺席");
        std::cout << "  [T6190.b] 1466 命中对照 通过\n";
    }

    // ══ T6190.c 攻击技过滤：属性技的 SKILL_INVALID 不算 ══
    {
        ScenarioBattle b(make_party(mk_self()), make_party(mk_foe(kDengYan)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_accuracy(ctx.getPet(1), 0);   // 属性技也 MISS
        drive_carrier_and_foe(b);
        CHECK(!ctx.has_active_abnormal_status(1, kFear),
              "T6190.c 1466：属性技无效 → 不附加害怕（攻击技过滤）");
        CHECK_EQ(storage_int(ctx, kAudit + 0, -1), -1,
                 "T6190.c 1466：审计槽缺席");
        std::cout << "  [T6190.c] 1466 属性技过滤 通过\n";
    }

    // ══ T6190.d 窗持续：R1 命中不触发，R2 MISS 仍触发 ══
    {
        ScenarioBattle b(make_party(mk_self()), make_party(mk_foe(kPaiDa)));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_carrier_and_foe(b);         // R1：对手命中
        CHECK(!ctx.has_active_abnormal_status(1, kFear), "T6190.d 前置：R1 未触发");
        set_accuracy(ctx.getPet(1), 0);   // R2 起必 MISS
        drive_attack_and_foe(b);          // R2
        CHECK(ctx.has_active_abnormal_status(1, kFear),
              "T6190.d 1466：R2 未命中仍在窗内 → 害怕(6)");
        CHECK_EQ(storage_int(ctx, kAudit + 0, -1), 1,
                 "T6190.d 1466：审计落地次数=1");
        std::cout << "  [T6190.d] 1466 窗持续性 通过\n";
    }

    // ══ T6190.e 窗过期：R5 MISS 不再触发（绝对值）══
    {
        ScenarioBattle b(make_party(mk_self()), make_party(mk_foe(kPaiDa)));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_carrier_and_foe(b);   // R1：命中（挂窗）
        drive_idle(b);              // R2
        drive_idle(b);              // R3
        drive_idle(b);              // R4
        set_accuracy(ctx.getPet(1), 0);
        drive_attack_and_foe(b);    // R5：MISS，但窗已过
        CHECK(!ctx.has_active_abnormal_status(1, kFear),
              "T6190.e 1466：窗过期 → MISS 不再附加害怕（绝对值）");
        CHECK_EQ(storage_int(ctx, kAudit + 0, -1), -1,
                 "T6190.e 1466：审计槽缺席");
        std::cout << "  [T6190.e] 1466 窗过期 通过\n";
    }

    std::cout << "scenario_6190 PASS (J286 对手攻击未命中异常窗族)" << std::endl;
    return 0;
}
