// 场景 7680 —— J320 暴击无效窗查证族（675，moves_lib/j_crit_invalid_window.cpp）
//（2026-11-03，worktree ../agents/j320；号段 7680-7689）
//
// 断言口径（J316 7560 直系：被拦 = hp 精确不变（SKILL_INVALID 不进伤害管线）；
// 落地 = 掉血 >0（暴击倍率浮动不进断言，规避带间距问题））：
//   - 675 模板：下{0}回合对手打出致命一击的非必中技能无效。
//   - 判暴确定性：harness 默认清零暴击率；对手刀 set_crit_sixteenths(16)=必定暴击
//     （rate>=100 短路、不耗 sbs_rand）→ crit_happened[1] 恒 true，可作伴随断言。
//   - 载体：36348 百无一失（[675]|[3] **纯链**物攻120，主载体）/
//     35276 异面折光（[675]|[1] **纯链**物攻120，rounds=1 过期档）。
//   - 窗口径 = NextRounds（本回合不算）：R1 先手注册，同回合对手暴击刀照常落地；
//     窗 [R+1, R+N]。审计槽 996750000/996750100（开窗数/tick 重授予数）双向核对。
//   - 豁免三支：必中技（凭证 must_hit，暴击照打不拦）、非暴击技（crit_happened
//     =false 不拦）、属性技（瞪眼 20004=effect5 100% 降对手攻，窗内照常生效——
//     以"效果落地"为观测：若被误拦，SKILL_INVALID 不注册效果、等级不动）。
//   - 必中判定先于暴击判定先于盔判定（query_usage 第0步凭证物化 → ①.5 判暴 →
//     ② notify），两判据在盔层均为现成值——内核钩子查证详见实现头注。
#include <any>
#include <iostream>
#include <string>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa   = 10004;  // 拍打：物 40（对手对照刀）
constexpr int kDengYan = 20004;  // 瞪眼：属性技 [5]|[1,100,-1]（属性技豁免观测刀）
constexpr int k36348   = 36348;  // 百无一失：物攻120 [675]|[3]（纯链）
constexpr int k35276   = 35276;  // 异面折光：物攻120 [675]|[1]（纯链，rounds=1）
constexpr int kArmedAudit = 996750000;      // +owner：开窗数（实现同款）
constexpr int kTickAudit  = 996750100;      // +owner：tick 重授予数
constexpr int kBigHp      = 99999;

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
    return fallback;
}

ElfPet self_pet(int skill0, int speed) {
    ElfPet p = make_pet(1, "己方", {skill0, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        speed, /*atk=*/1, kBigHp);
    accuracy_100(p, /*must_hit=*/false);
    set_crit_sixteenths(p, 0);
    return p;
}

// 对手刀：accuracy 100；must_hit 与暴击率按用例钉（16=必暴确定性档，0=必不暴）。
ElfPet foe_pet(int skill0, bool must_hit, int crit_sixteenths, int speed = 100) {
    ElfPet p = make_pet(2, "对手", {skill0, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        speed, /*atk=*/300, kBigHp);
    accuracy_100(p, must_hit);
    set_crit_sixteenths(p, crit_sixteenths);
    return p;
}

void run_round(ScenarioBattle& b, BattleFsm::ActionType op0, int skill0,
               BattleFsm::ActionType op1, int skill1) {
    b.submit_operation(0, op0, skill0);
    b.submit_operation(1, op1, skill1);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7680.a 主链：R1 先手注册（当回合不算）→ R2-R4 拦 → R5 过期 ════════
    {
        ScenarioBattle b(make_party(self_pet(k36348, /*speed=*/300)),
                         make_party(foe_pet(kPaiDa, /*must_hit=*/false, /*crit=*/16)));
        b.start();
        BattleContext& ctx = b.ctx();
        // R1：己方先手百无一失（开窗 [R2,R3,R4]）；同回合对手必暴拍打照常落地。
        const int self_before_r1 = ctx.getPet(0).hp;
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                  BattleFsm::ActionType::SELECT_SKILL, 0);
        const int taken_r1 = self_before_r1 - ctx.getPet(0).hp;
        CHECK(taken_r1 > 0,
              "T7680.a NextRounds 本回合不算：R1 同回合对手必暴刀落地（实测掉 " +
                  std::to_string(taken_r1) + "）");
        CHECK(ctx.crit_happened[1],
              "T7680.a 前置：对手拍打确打出致命一击（crit 16 确定性档）");
        CHECK_EQ(audit_or(ctx, kArmedAudit + 0, -1), 1,
                 "T7680.a 审计：开窗 1 次");
        CHECK_EQ(audit_or(ctx, kTickAudit + 0, 0), 0,
                 "T7680.a 审计：R1 尚无 tick 重授予（窗从下回合起；槽未写=0）");
        // R2/R3/R4：窗内，对手必暴拍打被拦（hp 精确不变）。
        for (int r = 2; r <= 4; ++r) {
            const int self_before = ctx.getPet(0).hp;
            run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1,
                      BattleFsm::ActionType::SELECT_SKILL, 0);
            CHECK_EQ(ctx.getPet(0).hp, self_before,
                     "T7680.a R" + std::to_string(r) +
                         " 窗内：必暴非必中拍打被拦 → 己方不掉血（实测掉 " +
                         std::to_string(self_before - ctx.getPet(0).hp) + "）");
        }
        CHECK_EQ(audit_or(ctx, kTickAudit + 0, -1), 3,
                 "T7680.a 审计：R2-R4 每回合 tick 重授予 1 次，共 3 次");
        // R5：窗过期（rounds=3），必暴拍打恢复落地。
        const int self_before_r5 = ctx.getPet(0).hp;
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1,
                  BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(self_before_r5 - ctx.getPet(0).hp > 0,
              "T7680.a R5 窗过期：必暴拍打恢复落地（实测掉 " +
                  std::to_string(self_before_r5 - ctx.getPet(0).hp) + "）");
        std::cout << "  [T7680.a] 窗内拦暴击技/当回合不算/3回合过期 ✓\n";
    }

    // ══ T7680.b 必中豁免：窗内必中暴击技不拦（判据=凭证 must_hit）══════════
    {
        ScenarioBattle b(make_party(self_pet(k36348, /*speed=*/300)),
                         make_party(foe_pet(kPaiDa, /*must_hit=*/true, /*crit=*/16)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                  BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 开窗
        CHECK_EQ(audit_or(ctx, kArmedAudit + 0, -1), 1, "T7680.b 前置：窗已开");
        const int self_before_r2 = ctx.getPet(0).hp;
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1,
                  BattleFsm::ActionType::SELECT_SKILL, 0);   // R2 窗内必中暴击刀
        CHECK(ctx.crit_happened[1],
              "T7680.b 前置：对手必中拍打确打出致命一击");
        CHECK(self_before_r2 - ctx.getPet(0).hp > 0,
              "T7680.b 窗内必中技豁免：必中暴击拍打落地不拦（实测掉 " +
                  std::to_string(self_before_r2 - ctx.getPet(0).hp) + "）");
        std::cout << "  [T7680.b] 必中技不受窗影响 ✓\n";
    }

    // ══ T7680.c 非暴击豁免：窗内普通刀不拦（crit_happened=false）═══════════
    {
        ScenarioBattle b(make_party(self_pet(k36348, /*speed=*/300)),
                         make_party(foe_pet(kPaiDa, /*must_hit=*/false, /*crit=*/0)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                  BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 开窗
        const int self_before_r2 = ctx.getPet(0).hp;
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1,
                  BattleFsm::ActionType::SELECT_SKILL, 0);   // R2 窗内普通拍打
        CHECK(!ctx.crit_happened[1],
              "T7680.c 前置：crit=0 → 未打出致命一击");
        CHECK(self_before_r2 - ctx.getPet(0).hp > 0,
              "T7680.c 窗内非暴击技不拦：普通拍打落地（实测掉 " +
                  std::to_string(self_before_r2 - ctx.getPet(0).hp) + "）");
        std::cout << "  [T7680.c] 非暴击技不受窗影响 ✓\n";
    }

    // ══ T7680.d 属性技豁免：窗内瞪眼照常生效（被误拦则效果不注册）══════════
    {
        ScenarioBattle b(make_party(self_pet(k36348, /*speed=*/300)),
                         make_party(foe_pet(kDengYan, /*must_hit=*/false, /*crit=*/0)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                  BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 开窗
        int before[6];
        for (int s = 0; s < 6; ++s) {
            before[s] = ctx.ability_levels[0][s];
        }
        const int self_before_r2 = ctx.getPet(0).hp;
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1,
                  BattleFsm::ActionType::SELECT_SKILL, 0);   // R2 窗内瞪眼
        CHECK_EQ(ctx.getPet(0).hp, self_before_r2,
                 "T7680.d 前置：属性技无伤害阶段，己方不掉血");
        int drops = 0;
        int total_delta = 0;
        for (int s = 0; s < 6; ++s) {
            const int d = before[s] - ctx.ability_levels[0][s];
            if (d != 0) {
                ++drops;
            }
            total_delta += d < 0 ? -d : d;
        }
        CHECK_EQ(drops, 1,
                 "T7680.d 属性技豁免：瞪眼 effect5 恰降 1 槽（双序坑规避——不断言槽位）");
        CHECK_EQ(total_delta, 1,
                 "T7680.d 属性技豁免：降 1 级（被误拦则 effect5 不注册、等级全 0）");
        std::cout << "  [T7680.d] 属性技不受窗影响 ✓\n";
    }

    // ══ T7680.e rounds=1 档（异面折光 [675]|[1]）：R2 拦、R3 过期 ══════════
    {
        ScenarioBattle b(make_party(self_pet(k35276, /*speed=*/300)),
                         make_party(foe_pet(kPaiDa, /*must_hit=*/false, /*crit=*/16)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int self_before_r1 = ctx.getPet(0).hp;
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                  BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 开窗（当回合不算）
        CHECK(self_before_r1 - ctx.getPet(0).hp > 0,
              "T7680.e R1 同回合必暴刀落地（NextRounds 本回合不算）");
        const int self_before_r2 = ctx.getPet(0).hp;
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1,
                  BattleFsm::ActionType::SELECT_SKILL, 0);   // R2 窗内
        CHECK_EQ(ctx.getPet(0).hp, self_before_r2,
                 "T7680.e R2 窗内：必暴拍打被拦（实测掉 " +
                     std::to_string(self_before_r2 - ctx.getPet(0).hp) + "）");
        CHECK_EQ(audit_or(ctx, kTickAudit + 0, -1), 1,
                 "T7680.e 审计：rounds=1 → tick 重授予恰 1 次");
        const int self_before_r3 = ctx.getPet(0).hp;
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1,
                  BattleFsm::ActionType::SELECT_SKILL, 0);   // R3 窗外
        CHECK(self_before_r3 - ctx.getPet(0).hp > 0,
              "T7680.e R3 过期：必暴拍打恢复落地（实测掉 " +
                  std::to_string(self_before_r3 - ctx.getPet(0).hp) + "）");
        std::cout << "  [T7680.e] rounds=1 档 R2 拦 R3 过期 ✓\n";
    }

    // ══ T7680.f 后手口径：后手开窗，次回合起拦（后出手不再额外顺延）═════════
    {
        ScenarioBattle b(make_party(self_pet(k36348, /*speed=*/100)),
                         make_party(foe_pet(kPaiDa, /*must_hit=*/false, /*crit=*/16,
                                            /*speed=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int self_before_r1 = ctx.getPet(0).hp;
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 0,
                  BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 对手先手刀+己方后手开窗
        CHECK(self_before_r1 - ctx.getPet(0).hp > 0,
              "T7680.f R1 对手先手刀在注册前 → 落地（实测掉 " +
                  std::to_string(self_before_r1 - ctx.getPet(0).hp) + "）");
        CHECK_EQ(audit_or(ctx, kArmedAudit + 0, -1), 1,
                 "T7680.f 前置：后手百无一失命中 → 窗已开");
        const int self_before_r2 = ctx.getPet(0).hp;
        run_round(b, BattleFsm::ActionType::SELECT_SKILL, 1,
                  BattleFsm::ActionType::SELECT_SKILL, 0);   // R2 窗内
        CHECK_EQ(ctx.getPet(0).hp, self_before_r2,
                 "T7680.f R2 窗内：必暴拍打被拦（后手 NextRounds 从 R2 起算，不再顺延）");
        std::cout << "  [T7680.f] 后手开窗次回合生效 ✓\n";
    }

    std::cout << "scenario_7680 all green" << std::endl;
    return 0;
}
