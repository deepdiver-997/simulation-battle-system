// 场景 1540 —— J67 受击窗族（1053/1226，moves_lib/j_hit_window.cpp）
//（2026-10-03，worktree ../agents/j67；号段 1540-1549 → T1540-T1541 两块）
//
// 断言口径（载体 26699/27619 均属性技：我方攻击不产红/粉，断言干净）：
//   - 1053 对手攻击窗（[3,6,3]：3回合/害怕/回max/3；兄弟 48=自免疫 843=先制，良性）：
//     R1 对手拍打（攻击技）→ 对手中害怕、自身无回血（hp == 999999-红）；
//     R2 对手 NONE → 自身恰回 max/3=6666（hp 20000、对手 atk 30000 开足缺口防钳）。
//   - 1226 扁平减+未受击吸取（[3,300,300]；兄弟 48/861 良性）：
//     R1 对手拍打 atk3000 → GUARD_DETECT 探针抓减前值，final == 减前-300；
//     R2 对手 NONE → 回合末吸取：粉探针恰 1 笔 ==300 且自身 +300；
//     R3 对手再拍打 → 有受击旗标不吸取（R3 无新 300 笔）且再减 300。
#include <any>
#include <iostream>
#include <vector>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

// ── 载体技（切片已逐条与 side_effect/arg 核对）──
constexpr int kPaiDa  = 10004;   // 拍打（对手攻击刀）
constexpr int kChuang = 26699;   // 属性技 [48,1053,843]|[3,3,6,3,2,1]（1053 窗）
constexpr int kBiYe   = 27619;   // 属性技 [48,1226,861]|[4,3,300,300,4,2]（1226 窗）

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300, int atk = 1, int hp = 20000) {
    ElfPet p = make_pet(1, "受击窗", skills, speed, atk, hp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100, int atk = 30000, int hp = 99998) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, hp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

// 粉伤 FINAL 探针（T1491 同款）：收集落在对手身上的每笔粉伤终值。
void install_pink_probe(BattleContext& ctx, std::vector<int>& pinks) {
    ctx.register_pink_damage_effect(
        PinkDamagePhase::FINAL, 1, [&pinks](BattleContext* c, int) {
            const PinkDamageResolved& p = c->resolvedPink;
            if (p.target == 1 && p.final > 0) {
                pinks.push_back(p.final);
            }
        });
}

int count_of(const std::vector<int>& v, int x) {
    int n = 0;
    for (const int e : v) {
        if (e == x) {
            ++n;
        }
    }
    return n;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1540 1053 对手攻击窗（26699 [3,6,3]）══
    {
        ScenarioBattle b(make_party(self_pet({kChuang, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        // R1：对手用攻击技 → 害怕落地、不回血
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 属性技挂窗
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 拍打（攻击技）
        b.drive();
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Fear)),
              "T1540.a 1053：对手用攻击技 → 中害怕");
        const int hp_r1 = ctx.getPet(0).hp;
        CHECK(hp_r1 < 20000, "T1540.a 前置：R1 挨打开缺口");
        // R2：对手 NONE（非攻击技）→ 自身回 max/3 = 6666
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(0).hp - hp_r1, 20000 / 3,
                 "T1540.b 1053：未触发 → 自身回 max/3（实测 +" +
                     std::to_string(ctx.getPet(0).hp - hp_r1) + "）");
        // (c) 被控回合（睡眠，SELECT_SKILL 被跳过）→ "成功使用"不成立 → 主句不发
        //    （2026-10-03 裁定：使用=成功使用出来，被控不算；skill_use_seq 是执行记录）
        ScenarioBattle c(
            make_party(self_pet({kChuang, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
            make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        c.start();
        BattleContext& cctx = c.ctx();
        (void)apply_anomaly(&cctx, 1, static_cast<int>(AbnormalStatusId::Sleep), 9, 0);
        c.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        c.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 会被控跳过
        c.drive();
        CHECK_EQ(cctx.skill_use_seq[1], 0,
                 "T1540.c 前置：被控回合对手 skill_use_seq 不动（实测 " +
                     std::to_string(cctx.skill_use_seq[1]) + "）");
        CHECK(!cctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Fear)),
              "T1540.c 1053：被控回合主句不触发（对手未成功使用攻击技）");
        std::cout << "  [T1540] 1053 对手攻击窗 通过\n";
    }

    // ══ T1541 1226 扁平减+未受击吸取（27619 [3,300,300]）══
    {
        ScenarioBattle b(make_party(self_pet({kBiYe, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                             /*speed=*/300, /*atk=*/1, /*hp=*/99999)),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            /*speed=*/100, /*atk=*/3000)));
        b.start();
        BattleContext& ctx = b.ctx();
        int pre_guard = -1;   // GUARD_DETECT 时点的 final（减前）
        ctx.register_damage_effect(
            DamagePhase::GUARD_DETECT, 0, DamageEffectCategory::DETECT,
            [&pre_guard](BattleContext* c, int) {
                const DamageSnapshot& d = c->resolvedDamage;
                if (d.defenderId == 0 && d.isRed && d.final > 0) {
                    pre_guard = d.final;
                }
            });
        std::vector<int> pinks;
        install_pink_probe(ctx, pinks);
        // R1：挂窗；对手拍打 → 扁平减 300；受击旗标置位 → 无吸取
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(pre_guard > 300, "T1541.a 前置：减前值已抓到");
        CHECK_EQ(pre_guard - ctx.resolvedDamage.final, 300,
                 "T1541.a 1226：受到攻击伤害 -300（pre=" + std::to_string(pre_guard) +
                     "，final=" + std::to_string(ctx.resolvedDamage.final) + "）");
        CHECK_EQ(count_of(pinks, 300), 0, "T1541.a 受击回合不吸取");
        // R2：对手 NONE → 未受击 → 回合末吸取 300
        const int self_hp_r1 = ctx.getPet(0).hp;
        pinks.clear();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(count_of(pinks, 300), 1,
                 "T1541.b 1226：未受击 → 回合末恰 1 笔吸取 300（pinks=" +
                     std::to_string(pinks.size()) + " 笔）");
        CHECK_EQ(ctx.getPet(0).hp - self_hp_r1, 300, "T1541.b 吸取回血 +300");
        // R3：对手再拍打 → 受击旗标置位 → 不吸取，且再减 300
        pinks.clear();
        pre_guard = -1;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(pre_guard > 300, "T1541.c 前置：R3 减前值已抓到");
        CHECK_EQ(pre_guard - ctx.resolvedDamage.final, 300, "T1541.c 窗内 R3 仍 -300");
        CHECK_EQ(count_of(pinks, 300), 0, "T1541.c 受击回合不吸取");
        std::cout << "  [T1541] 1226 扁平减+未受击吸取 通过\n";
    }

    std::cout << "scenario_1540 PASS (J67 受击窗族)" << std::endl;
    return 0;
}
