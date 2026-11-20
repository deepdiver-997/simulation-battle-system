// 场景 1480 —— J62 防御窗族（2 宏，moves_lib/j_defense_windows.cpp）
//（2026-10-03，worktree ../agents/j62；号段 1480-1489 → T1480-T1481 两块）
//
// 断言口径：
//   - 1023 用 100% 闪避档载体（开溜需趁早 [3,100,3]，链全实现不扰断言）：闪避把
//     hit_chance 压 0 → 对手攻击必 MISS（skills.cpp 命中 roll 消费 ws.dodge_rate）→
//     EVENT_SKILL_INVALID → heal 精确 max/3=1666（预置半血防钳制）；R5 窗过期 →
//     命中落地、不再回血；
//   - 914 用 100% 概率档载体（灼光绮丽 [2,2,100]；链上 191/886 已实现无害、1393
//     未实现哑）：预置对手烧伤 → 状态门过 → SEAL_ALL 2 回合 → R2 对手攻击无效
//     （我方血不动）、R3 窗过命中落地；对照 = 不预置烧伤 → 门关 → R2 命中。
//   - 两载体全属性技（无伤害面）+ 对手 NONE/攻击节奏隔离，rand 消耗仅命中 roll。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa  = 10004;  // 拍打：物40（对手探针攻击）
constexpr int kKaiLiu  = 29272;  // 开溜需趁早 [962,1023,843]|[1,3,100,3,2,1]：属
                                 //（1023 切片=[3,100,3] 100% 闪避 1/3 回血；链全实现且
                                 //  962 全属性+1/843 先制+1 均不扰回血断言） // 冰凌·拥天 [191,1023,1003,2533]|[2,3,100,3,…]：属（100% 闪避载体；
                                 // 链上 191 被动授予无害、1003/2532 未实现哑）
constexpr int kZhuoGuang = 28421; // 灼光绮丽 [191,914,886,1393]|[2,2,100,?,?]：属（封全载体）
constexpr int kMaxHp  = 5000;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_self(std::array<int, 5> skills) {
    ElfPet p = make_pet(1, "己方", skills, 300, /*atk=*/1, kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe(std::array<int, 5> skills) {
    ElfPet p = make_pet(2, "对手", skills, 100, /*atk=*/1, kMaxHp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

// 一回合：己方选技 / 对手 NONE。
void drive_self_skill(ScenarioBattle& b, int self_skill_idx) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, self_skill_idx);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

// 一回合：双方都出招（己方 slot 1 拍打 / 对手 slot 0 拍打）。
// ⚠️ 不要用 "NONE 先提交" 模式——NONE 侧未标记 operation_collected 时 ROUND_START
// 的 preserve 会把后手方 roundChoice 清成 -1 → 行动被整体跳过（T1480 调试实证）。
void drive_both_attack(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1480 1023 闪避窗+MISS 回血（无息之隐 [3,100,3]）══
    {
        ScenarioBattle b(
            make_party(mk_self({kKaiLiu, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
            make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 3000;                      // 预置缺口（防回血钳制）
        // R1：先手挂窗（闪避当回合即生效）+ 同回合对手攻击 → 100% 闪避必 MISS → 回血
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(0).hp, 3000 + kMaxHp / 3,
                 "T1480.a 1023：100% 闪避 → 对手 MISS → 回 max/3（实测 " +
                     std::to_string(ctx.getPet(0).hp) + "）");
        CHECK_EQ(ctx.getPet(1).hp, kMaxHp, "T1480.a 前置：对手未被攻击（载体属性技）");
        // R2..R4 空转；R5 窗过期（[3] → R1..R3）→ 命中落地、不再回血
        drive_self_skill(b, 1);                       // R2
        drive_self_skill(b, 1);                       // R3
        drive_self_skill(b, 1);                       // R4
        const int hp_before = ctx.getPet(0).hp;
        drive_both_attack(b);                         // R5 对手攻击（窗已过）
        CHECK(ctx.getPet(0).hp < hp_before,
              "T1480.b 1023：窗过期 → 命中落地不再回血（" +
                  std::to_string(hp_before) + "→" + std::to_string(ctx.getPet(0).hp) + "）");
        std::cout << "  [T1480] 1023 闪避+MISS回血 通过\n";
    }

    // ══ T1481 914 状态门→封全技能无效（灼光绮丽 [2,2,100]）══
    {
        // a) 对手烧伤 → 门过 → SEAL_ALL 2 回合：R2 攻击无效、R3 落地
        {
            ScenarioBattle b(
                make_party(mk_self({kZhuoGuang, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)apply_anomaly(&ctx, 1, static_cast<int>(AbnormalStatusId::Burn),
                                /*duration=*/5, /*actor=*/0);
            CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Burn)),
                  "T1481.a 前置：对手已烧伤");
            drive_self_skill(b, 0);                   // R1 挂封（对手 NONE）
            drive_both_attack(b);                     // R2 对手攻击 → 无效
            CHECK_EQ(ctx.getPet(0).hp, kMaxHp,
                     "T1481.a 914：封全窗内对手技能无效（我方血实测 " +
                         std::to_string(ctx.getPet(0).hp) + "）");
            drive_both_attack(b);                     // R3 对手攻击 → 窗过落地
            CHECK(ctx.getPet(0).hp < kMaxHp,
                  "T1481.a 914：窗过 → 命中落地（血实测 " +
                      std::to_string(ctx.getPet(0).hp) + "）");
        }
        // b) 对照：无烧伤 → 门关 → R2 命中
        {
            ScenarioBattle b(
                make_party(mk_self({kZhuoGuang, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            drive_self_skill(b, 0);                   // R1（无封——门关）
            drive_both_attack(b);                   // R2 对手攻击 → 命中
            CHECK(ctx.getPet(0).hp < kMaxHp,
                  "T1481.b 914 对照：无状态门关 → 命中（血实测 " +
                      std::to_string(ctx.getPet(0).hp) + "）");
        }
        std::cout << "  [T1481] 914 状态门封全 通过\n";
    }

    std::cout << "scenario_1480 PASS (J62 防御窗族 1023/914)" << std::endl;
    return 0;
}
