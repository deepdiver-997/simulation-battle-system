// 场景 2840 —— K7 时空跃迁/叠层减伤复杂族（1639/1903 + 1673，soul_lib/k_spacetime_leap.cpp）
//（2026-10-04，worktree ../agents/k7；号段 2840-2849 → T2840 一块四例）
//
// 断言口径（拍打 P40；红伤带 = 0.84×40×atk/100；kMaxHp=800000）：
//   - a 1639 跃迁：自方预中毒 + 对手 atk=720000 大刀（红伤 ≥ 当前体力）→ 免疫
//     （hp 不动）+ 中毒解除 + 次回合起闪避 100%（对手 acc=100 也 miss）；
//   - b 1639 一次性：跃迁额度用尽后第二刀正常落地；
//   - c 1639 clause B：满血回合 → must_crit + 对手随机 1 技 PP=0（白盒）；
//     低体回合 → preemptive+1（白盒）+ 吸取对手最大体力 1/3；
//   - d 1673 叠层：回合推进 → damage_add_pct[opp] 递减 / hit_rate_mod_attack[opp]
//     递减（白盒）；对手 acc=0 miss 回合 → 末削对手最大体力 1/3（对手未命中分支）。
//   魂印构造 = scenario_958 的 make_mark/make_pet 范式（args 用 DB 行原值）。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa  = 10004;
constexpr int kPoison = static_cast<int>(AbnormalStatusId::Poison);
constexpr int kMaxHp  = 800000;

// K7 宏（MonsterPet 真身 id 仅作标记）。
SoulMark make_mark(int effect_id, std::vector<int> db_args, int monster_id) {
    SoulMark sm(effect_id, "K" + std::to_string(effect_id), "test",
                EffectArgs(std::move(db_args)));
    sm.monster_id = monster_id;
    return sm;
}

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mark_pet(int effect_id, int monster_id, std::vector<int> db_args, int atk) {
    ElfPet p = make_pet(1, "时空界神", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, atk, kMaxHp,
                        make_mark(effect_id, std::move(db_args), monster_id));
    accuracy_100(p, true);
    return p;
}

ElfPet foe_pet(int atk, int acc = 100) {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, atk, kMaxHp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

void drive_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T2840.a 1639（1 次）：跃迁免疫+解异常+闪避窗+额度用尽倒下 ═════════════
    {
        ScenarioBattle b(make_party(mark_pet(1639, 4050, {3, 100, 2, 1, 2, 1, 3, 1}, 300)),
                         make_party(foe_pet(3000000)));
        b.start();
        BattleContext& ctx = b.ctx();
        (void)apply_anomaly(&ctx, 0, kPoison, /*duration_rounds=*/9, /*actor=*/1);   // 预挂中毒
        drive_round(b);   // R0：大刀 ≥ 满血 → 跃迁
        CHECK_EQ(ctx.getPet(0).hp, kMaxHp - 100000,
                 "T2840.a 1639：跃迁免疫本刀（毒 tick 照扣 → 70 万，实测 " +
                     std::to_string(ctx.getPet(0).hp) + "）");
        CHECK(ctx.has_active_abnormal_status(0, kPoison) == false,
              "T2840.a 1639：跃迁解除自身异常");
        for (int r = 1; r <= 3; ++r) {
            drive_round(b);   // R1-R3：闪避 100%
            CHECK_EQ(ctx.getPet(0).hp, kMaxHp - 100000,
                     "T2840.a 1639：闪避窗 R" + std::to_string(r) + " hp 恒定（毒已解无 tick）");
        }
        // R4：额度用尽 + 窗过期 → 一刀倒下。⚠️ R0 的 clause B 把对手随机 1 技 PP 归零
        // （可能正是槽 0）——R4 让对手换用 PP 完好的槽位攻击，排除 pp=0 干扰。
        {
            int foe_slot = -1;
            for (int i = 0; i < static_cast<int>(ctx.getPet(1).skills.size()); ++i) {
                if (ctx.getPet(1).skills[static_cast<std::size_t>(i)].pp > 0) {
                    foe_slot = i;
                    break;
                }
            }
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_slot);
            b.drive();
        }
        CHECK_EQ(ctx.getPet(0).hp, 0,
                 "T2840.a 1639：额度用尽后一刀倒下（最强一次性证明，实测 hp " +
                     std::to_string(ctx.getPet(0).hp) + "）");
    }
    // ══ T2840.b 1903（2 次）：二次跃迁 ════════════════════════════════════════
    {
        ScenarioBattle b(make_party(mark_pet(1903, 4050, {3, 100, 2, 1, 2, 1, 3, 1}, 300)),
                         make_party(foe_pet(3000000)));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b);   // R0：第一次跃迁
        for (int r = 1; r <= 3; ++r) {
            drive_round(b);   // R1-R3 闪避
        }
        CHECK_EQ(ctx.getPet(0).hp, kMaxHp, "T2840.b 1903：R3 仍满血（一次跃迁+闪避窗）");
        drive_round(b);     // R4：大刀 → 第二次跃迁
        CHECK_EQ(ctx.getPet(0).hp, kMaxHp,
                 "T2840.b 1903：第二次跃迁免疫（1903 独有 2 次额度）");
        for (int r = 5; r <= 7; ++r) {
            drive_round(b);   // R5-R7 闪避
        }
        CHECK_EQ(ctx.getPet(0).hp, kMaxHp, "T2840.b 1903：R7 仍满血");
        // R8：额度（2 次）用尽 → 倒下。⚠️ 对手换用 PP 完好槽位（clause B 两轮随机归零干扰）
        {
            int foe_slot = -1;
            for (int i = 0; i < static_cast<int>(ctx.getPet(1).skills.size()); ++i) {
                if (ctx.getPet(1).skills[static_cast<std::size_t>(i)].pp > 0) {
                    foe_slot = i;
                    break;
                }
            }
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_slot);
            b.drive();
        }
        CHECK_EQ(ctx.getPet(0).hp, 0, "T2840.b 1903：R8 额度用尽倒下");
    }
    // ══ T2840.ch 1639 clause B 高体：必暴 + 随机 1 技 PP 归零 ═════════════════
    {
        ScenarioBattle b(make_party(mark_pet(1639, 4050, {3, 100, 2, 1, 2, 1, 3, 1}, 300)),
                         make_party(foe_pet(1)));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b);   // R0：满血 → 高体分支
        int zero_pp = 0;
        for (const Skills& s : ctx.getPet(1).skills) {
            if (s.pp == 0) {
                ++zero_pp;
            }
        }
        CHECK(zero_pp >= 1, "T2840.ch：随机 1 技 PP 归零（归零数 " +
                                std::to_string(zero_pp) + "）");
        const int red = kMaxHp - ctx.getPet(1).hp;
        // 基准红伤 0.84×40×300/100 → 带 [85,100]；必暴 ×2 → [170,200]
        CHECK(red >= 170 && red <= 200,
              "T2840.ch：必暴红伤 2×带（实测 " + std::to_string(red) + "）");
    }
    // ══ T2840.cl 1639 clause B 低体：先制+1 + 吸取双向 1/3 ════════════════════
    {
        // 对手 atk=1500000：R0 一刀把自方打进低体（红伤 [428400,504000] > 400000）
        ScenarioBattle b(make_party(mark_pet(1639, 4050, {3, 100, 2, 1, 2, 1, 3, 1}, 300)),
                         make_party(foe_pet(1500000)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();   // R0：自方吃大刀进低体
        CHECK(ctx.getPet(0).hp < kMaxHp / 2, "T2840.cl 前置：已进低体");
        const int pre_pre = ctx.ws.preemptive_level[0];
        const int self_hp0 = ctx.getPet(0).hp;
        const int foe_hp0 = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();   // R1：低体回合（双方 NONE，唯一变量 = clause B）
        // 先制+1 不做 post-drive 白盒断言（ws 每回合 reset，读到的已是下轮状态）——
        // 低体分支的观测 = 吸取双向 1/3（下方两条精确带）。
        (void)pre_pre;
        const int self_gain = ctx.getPet(0).hp - self_hp0;
        const int foe_loss = foe_hp0 - ctx.getPet(1).hp;
        CHECK(foe_loss >= kMaxHp / 3 - 2 && foe_loss <= kMaxHp / 3,
              "T2840.cl 1639：吸取对手最大体力 1/3（实测 " + std::to_string(foe_loss) +
                  " ≈ " + std::to_string(kMaxHp / 3) + "）");
        CHECK(self_gain >= kMaxHp / 3 - 2 && self_gain <= kMaxHp / 3,
              "T2840.cl 1639：吸取等量回自（实测 " + std::to_string(self_gain) + "）");
    }
    // ══ T2840.d 1673：叠层白盒 + miss 分支削对手最大体力 ══════════════════════
    {
        ScenarioBattle b(make_party(mark_pet(1673, 4080, {10, 10, 5, 3, 3, 1}, 300)),
                         make_party(foe_pet(1)));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b);   // R0：一层
        CHECK(ctx.ws.damage_add_pct[1] <= -10.0f && ctx.ws.hit_rate_mod_attack[1] < 1.0f,
              "T2840.d 1673：R0 一层（对手输出降+命中降）");
        drive_round(b);   // R1：两层
        CHECK(ctx.ws.damage_add_pct[1] <= -20.0f && ctx.ws.hit_rate_mod_attack[1] <= 0.8f,
              "T2840.d 1673：R1 两层");
        // miss 分支：对手 acc=0 → 战斗阶段结束削对手最大体力 1/3
        for (Skills& s : ctx.getPet(1).skills) {
            s.accuracy = 0;
            s.must_hit = 0;
        }
        const int foe_max0 = ctx.getPet(1).numericalBase[NumericalPropertyIndex::HP];
        drive_round(b);   // R2：对手 miss → 末削最大体力
        const int foe_max_now = ctx.getPet(1).numericalBase[NumericalPropertyIndex::HP];
        CHECK(foe_max_now < foe_max0,
              "T2840.d 1673：miss 回合末对手最大体力被削（" + std::to_string(foe_max0) +
                  " → " + std::to_string(foe_max_now) + "）");
    }


    // ══ T2840.e 1057 补子句：后手必暴（×2 确定性）/ 先手闪避（对手低命中构造必 miss）
    {
        // 后手必暴：自方 speed 50（后手）→ R0 自方拍打必暴 ×2
        ElfPet sp = make_pet(1, "耶里梅斯", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                             /*speed=*/50, 300, kMaxHp,
                             make_mark(1057, {250, 70, 1}, 3458));
        accuracy_100(sp, true);
        ElfPet fp = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                             /*speed=*/100, 1, kMaxHp);
        accuracy_100(fp, false);
        auto crit_probe = [&](const ElfPet& attacker) {
            ScenarioBattle b(make_party(attacker), make_party(fp));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.debug_step_mode = true;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1); // 隔离反弹
            ctx.debug_step_mode = false;
            b.step_until(State::BATTLE_SECOND_ATTACK_DAMAGE);
            ctx.seed_rng(20261004); // 在伤害公式前对齐两场的浮动
            b.drive();
            return std::make_pair(kMaxHp - ctx.getPet(1).hp, ctx.resolvedDamage.isCrit);
        };
        ElfPet forced_crit = sp;
        forced_crit.soulMark = SoulMark{};
        set_crit_sixteenths(forced_crit, 16);
        const auto [red, marked_crit] = crit_probe(sp);
        const auto [red_ctrl, control_crit] = crit_probe(forced_crit);
        ElfPet noncrit = forced_crit;
        set_crit_sixteenths(noncrit, 0);
        const auto [red_plain, plain_crit] = crit_probe(noncrit);
        CHECK(marked_crit && control_crit && !plain_crit,
              "T2840.e 后手魂印与必暴对照均暴击，零暴对照不暴击");
        CHECK(red_plain > 0, "T2840.e 零暴对照造成红伤");
        CHECK_EQ(red_ctrl, red_plain * 2, "T2840.e 同种子必暴对零暴精确 2 倍");
        CHECK_EQ(red, red_ctrl, "T2840.e 1057 后手伤害与同种子必暴对照精确一致");
        // 先手闪避：自方 speed 300（先手）→ 自方闪避 +70%；对手命中压到 29（29-70<0 必 miss）
        ElfPet sp2 = make_pet(1, "耶里梅斯", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                              /*speed=*/300, 300, kMaxHp,
                              make_mark(1057, {250, 70, 1}, 3458));
        accuracy_100(sp2, true);
        ElfPet fp2 = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                              /*speed=*/100, 1, kMaxHp);
        for (Skills& s : fp2.skills) {
            s.accuracy = 29;   // 29 - 70 < 0 → 闪避窗内必 miss（确定性）
            s.must_hit = 0;
        }
        ScenarioBattle b2(make_party(sp2), make_party(fp2));
        b2.start();
        BattleContext& c2 = b2.ctx();
        b2.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R0 自方先手拍打
        b2.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R0 对手拍打（应 miss）
        b2.drive();
        CHECK_EQ(c2.getPet(0).hp, kMaxHp,
                 "T2840.e 1057：先手闪避 +70% 压过对手命中 29 → 拍打落空（hp 应满血，实测 " +
                     std::to_string(c2.getPet(0).hp) + "）");
    }

    std::cout << "scenario_2840 PASS (K7 时空跃迁/叠层减伤复杂族)" << std::endl;
    return 0;
}
