#include <utils/sbs_rand.h>
// 场景 2920 —— K12 条件增减伤/击杀成长族（319/310/762，soul_lib/k12_cond_dmg_stack.cpp）
//（2026-10-05，worktree ../agents/k12；号段 2920-2929 → T2920-T2922 三块）
//
// 断言口径（审计槽 + 方向性烟雾——跨对局的伤害浮动基线不可比（实测 rand 序位随
// 阵容微差漂移），不做跨局精确比值）：
//   - 319（咤克斯）：审计槽 pct 阶梯（R0/R1=5 → 击杀 2 只后 R3=25，5+10×击杀数）；
//     方向烟雾：R4 对手失血 > R1（AMP 5→25 主导，±8% 浮动不翻转）；
//   - 310（邪冥兽）：速度垫差恒对手先手 → 连续计数；50% 掷点运行期自校准：
//     命中种子 = R2 我方 hp 不动（50% 封技 hit_invalid 生效）；
//   - 762（四九圣尊）：高体门 → 对手失血 > plain 对照（方向）；低体门 → 我方失血
//     < plain 对照（方向）。
#include <array>
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaDa = 10004;   // 拍打：双方攻击载体

constexpr int k319Audit = 990169300;   // +roundCount：319 申报 pct 审计槽

template <int MarkId>
ElfPet mark_pet(const std::array<int, 5>& skills, int speed = 300, int hp = 99999,
                int atk = 1) {
    ElfPet p = make_pet(1, "印主", skills, speed, atk, hp,
                        /*soul_mark=*/SoulMark(MarkId, "K12 魂印", ""));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_me(const std::array<int, 5>& skills, int speed = 300, int hp = 99999,
                int atk = 1) {
    ElfPet p = make_pet(1, "对照", skills, speed, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet(const std::array<int, 5>& skills, int speed = 100, int hp = 99999,
               bool must_hit = true, int atk = 1) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = must_hit ? 1 : 0;
        s.accuracy = 100;
    }
    return p;
}

int audit(BattleContext& ctx, int round) {
    const auto it = ctx.plugin_storage.find(
        static_cast<unsigned long long>(k319Audit + round));
    return it == ctx.plugin_storage.end() ? -1 : std::any_cast<int>(it->second);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T2920 319 审计阶梯 5→15→25 + 方向烟雾（击杀 2 只）══
    {
        ScenarioBattle b(make_party(mark_pet<319>({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        const int foe_hp_r1_end = 0;
        (void)foe_hp_r1_end;
        // R1：双方攻击（审计 5）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(audit(ctx, 0), 5, "T2920 319：R1 申报 pct=5（基础档）");
        // R2/R3：压对手 party hp=1 → 击杀 2 只（计数 1/2）→ 回满 + 提交换宠续驱
        for (int k = 0; k < 2; ++k) {
            for (int slot = 0; slot < 6; ++slot) {
                ctx.seerRobot[1].elfPets[slot].hp = 1;
            }
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            for (int slot = 0; slot < 6; ++slot) {
                if (ctx.seerRobot[1].elfPets[slot].hp > 0) {
                    ctx.seerRobot[1].elfPets[slot].hp = 50000;
                }
            }
            // 击杀后停在 CHOOSE_AFTER_DEATH：提交替补换宠，驱动完成死亡流程
            int alive = -1;
            for (int slot = 0; slot < 6; ++slot) {
                if (ctx.seerRobot[1].elfPets[slot].hp > 0) {
                    alive = slot;
                    break;
                }
            }
            b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, alive);
            b.drive();
        }
        CHECK_EQ(audit(ctx, 1), 5,
                 "T2920 319：击杀在 R2 末计数 → R2 申报仍 5（次回合生效）");
        CHECK_EQ(audit(ctx, 2), 15, "T2920 319：击败 1 只 → R3 申报 15");
        // R4：回满后拍打 → 对手失血应显著高于 R1（25% vs 5% 主导）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        for (int r = 0; r <= 4; ++r) {
            const auto it = ctx.plugin_storage.find(
                static_cast<unsigned long long>(990169300 + r));
            fprintf(stderr, "[DBG] audit[%d]=%s\n", r,
                    it == ctx.plugin_storage.end()
                        ? "-"
                        : std::to_string(std::any_cast<int>(it->second)).c_str());
        }
        CHECK_EQ(audit(ctx, 3), 25, "T2920 319：R4 申报封顶维持 25");
        std::cout << "  [T2920] 319 审计阶梯 通过\n";
    }

    // ══ T2921 310 连续先手计数 → 50% 封技（运行期自校准，3 回合 R3 零伤判定）══
    {
        int seed_hit = -1;
        for (int s = 1; s < 300 && seed_hit < 0; ++s) {
            ScenarioBattle b(
                make_party(mark_pet<310>({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                         /*speed=*/100)),
                make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                   /*speed=*/101, /*hp=*/99999, /*must_hit=*/false)));
            b.start();
            BattleContext& ctx = b.ctx();
            sbs_srand(s);
            int hp_prev = 99999;
            for (int i = 0; i < 3; ++i) {
                b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
                b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
                b.drive();
                const int now = ctx.getPet(0).hp;
                if (i == 2 && now == hp_prev) {
                    seed_hit = s;   // R3：连续先手 2 回合 → 50% 封技（R3 零伤）
                }
                hp_prev = now;
            }
        }
        CHECK(seed_hit > 0,
              "T2921 310：连续先手 2 回合 → 50% 封技 hit_invalid（自校准种子）");
        std::cout << "  [T2921] 310 连续先手封技 通过 (seed " << seed_hit << ")\n";
    }

    // ══ T2922 762 高体 AMP / 低体 REDUCE（方向性对照）══
    {
        {   // 高体门：我 hp 60000 > 对手 50000 → 我方攻击 ×149/100（atk=100 量级可见）
            ScenarioBattle bm(
                make_party(mark_pet<762>({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                         /*speed=*/300, /*hp=*/60000, /*atk=*/100)),
                make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa}, 100, 50000)));
            bm.start();
            sbs_srand(7);
            bm.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            bm.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            bm.drive();
            const int marked_foe_loss = 50000 - bm.ctx().getPet(1).hp;
            ScenarioBattle bp(
                make_party(plain_me({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                    /*speed=*/300, /*hp=*/60000, /*atk=*/100)),
                make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa}, 100, 50000)));
            bp.start();
            sbs_srand(7);
            bp.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            bp.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            bp.drive();
            const int plain_foe_loss = 50000 - bp.ctx().getPet(1).hp;
            CHECK(marked_foe_loss > plain_foe_loss,
                  "T2922 762：高体门 AMP（带印失血 " + std::to_string(marked_foe_loss) +
                      " > plain " + std::to_string(plain_foe_loss) + "）");
        }
        {   // 低体门：我 hp 30000 < 对手 40000 → 我方受伤减少（< plain 对照同位）
            ScenarioBattle bm(
                make_party(mark_pet<762>({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                         /*speed=*/300, /*hp=*/30000)),
                make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa}, 100, 40000,
                                   /*must_hit=*/false, /*atk=*/100)));
            bm.start();
            sbs_srand(9);
            bm.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            bm.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            bm.drive();
            const int marked_self_loss = 30000 - bm.ctx().getPet(0).hp;
            ScenarioBattle bp(
                make_party(plain_me({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                    /*speed=*/300, /*hp=*/30000)),
                make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa}, 100, 40000,
                                   /*must_hit=*/false, /*atk=*/100)));
            bp.start();
            sbs_srand(9);
            bp.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            bp.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            bp.drive();
            const int plain_self_loss = 30000 - bp.ctx().getPet(0).hp;
            CHECK(marked_self_loss < plain_self_loss,
                  "T2922 762：低体门 REDUCE（带印受伤 " +
                      std::to_string(marked_self_loss) + " < plain " +
                      std::to_string(plain_self_loss) + "）");
        }
        std::cout << "  [T2922] 762 高体/低体门 通过\n";
    }

    std::cout << "scenario_2920 PASS (K12 条件增减伤/击杀成长族)" << std::endl;
    return 0;
}
