// 场景 880 —— X1 effect 919 守卫反置修正回归（moves_lib/burn_trio_skills.cpp）
//（2026-10-01，worktree ../agents/x-fix-1；小修批工单-2026-09-30 X1，段 880）
//
// 缺陷与修法（插件文件头已记档）：919 官方 = "后出手时令对手下{0}回合属性技能无效"
//（effect_info args_num=1，忷魔涸焦血 38283 等全库 40 载体）；落地守卫写成
// !acting_side_is_first 早退 = **先手才挂**，与官方相反。修法 = 判据一刀翻转
//（acting_side_is_first 早退；同库 d5_D2.cpp 明写"919 同款判据"的极性即正）。
//
// 断言组（一文件三块）：
//   880a 后手主路径：己方慢速 38283（919 切片[2]）→ R1 对手（快速）先动高速移动
//        照常落地（授票在 SKILL_EFFECT 时点、晚于先手方行动——引擎时点事实，非缺陷）；
//        票在（回合中段查，step_past(BATTLE_SECOND_SKILL_EFFECT)——ON_SKILL_HIT(34)
//        在 SKILL_EFFECT(35) 之前，720 的落点对 SKILL_EFFECT 时点授票不适用）；
//        R2 对手属性技被拦（速度+2 不落地）；R3 票逝（duration=2 = 注册回合+下1回合）
//        速度+2 再落地。
//   880b 先手门槛对照：己方快速同一载体 → 919 不挂票、对手高速移动照常落地；
//        同链 909（先出手 100% 烧伤）照常触发——同一技能两条先后手效果极性互为对照
//        （若 919 守卫再反置：880b 的票存在断言 + 高速移动落地断言双双转红）。
//   880c 窗口内只封属性不封攻击：后手授票后的下一回合，对手攻击技照常掉血
//       （seal attribute=true attack=false 的半边）。
//
// 设计口径（720/800 同款）：先手/后手对照 speed 300 vs 100，载体 priority=0；
// 必中全开；atk=1 保方向断言。伴生效果自查：909 后手局门槛不成立 no-op（880a/c）、
// 先手局 100% 烧伤（880b，正交——烧伤不动能力等级）；2105 未击败 no-op（foe hp 充裕）；
// 1268 需"上回合击败"旗 no-op。
#include <array>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kXiongMo = 38283;   // 忷魔涸焦血：物攻150 priority0 [909,919,2105,1268] → 919=[2]
constexpr int kGaoSu = 20017;     // 高速移动：属性 [4]|[4,100,2] 速度+2（封属观测量）
constexpr int kPaDa = 10004;      // 拍打：物攻40 无效果（filler）

ElfPet self_pet(const std::array<int, 5>& skills, int speed, int atk, int hp) {
    ElfPet p = make_pet(1, "己方", skills, speed, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet(const std::array<int, 5>& skills, int speed, int atk, int hp) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

void play_round(ScenarioBattle& b, int self_slot, int foe_slot) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, self_slot);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_slot);
    b.drive();
}

// 推进到"后手方 SKILL_EFFECT 桶已执行、回合扣减点未到"——SKILL_EFFECT 时点授票的
// 回合中段结构断言窗口（720 的 play_round_to_mid 停在 ON_SKILL_HIT，早于本时点）。
void play_round_to_effect_done(ScenarioBattle& b, int self_slot, int foe_slot) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, self_slot);
    b.ctx().debug_step_mode = true;
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_slot);
    b.ctx().debug_step_mode = false;
    b.step_past(State::BATTLE_SECOND_SKILL_EFFECT);
}

// 结构断言用：直接扫 RuleCenter 票条目（720 同款存活口径：remaining_rounds>0）。
bool seal_ticket_alive(BattleContext& ctx, int target, SealKind kind) {
    for (const RuleTicket& t : ctx.rule_center_.entries()) {
        if (t.category == RuleCategory::SEAL && t.target == target
            && static_cast<SealKind>(t.subtype) == kind && t.remaining_rounds > 0) {
            return true;
        }
    }
    return false;
}

bool has_anomaly(BattleContext& ctx, int side, AbnormalStatusId id) {
    return ctx.has_active_abnormal_status(side, static_cast<int>(id));
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ════════════════════════════════════════════════════════════════
    // 880a 后手主路径：慢速 38283 → 挂 2 回合封属，R2 拦对手属性技
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kXiongMo, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/100, /*atk=*/1, /*hp=*/5000)),
                         make_party(foe_pet({kGaoSu, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*speed=*/300, /*atk=*/1, /*hp=*/100000)));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round_to_effect_done(b, 0, 0);   // R1：对手先手高速移动（已落地）→ 己方后手 919 授票
        CHECK_EQ(ctx.ability_levels[1][4], 2,
                 "880a 时点：R1 对手先动，封属授票晚于其行动 → 高速移动照常落地（速度+2）");
        CHECK(seal_ticket_alive(ctx, 1, SealKind::SEAL_ATTRIBUTE),
              "880a 结构：后手授票（SEAL_ATTRIBUTE 票在，回合中段查）");
        b.drive();   // R1 收尾：duration=2 扣到 1（覆盖 R2）
        play_round(b, 1, 0);   // R2：己方改拍打（不刷新票）→ 对手高速移动被拦
        CHECK_EQ(ctx.ability_levels[1][4], 2, "880a 核心：R2 封属生效（对手属性技无效）");
        play_round(b, 1, 0);   // R3：票已逝 → 高速移动落地
        CHECK(!seal_ticket_alive(ctx, 1, SealKind::SEAL_ATTRIBUTE),
              "880a 结构：R3 票已逝（duration=2 = 注册回合+下1回合）");
        CHECK_EQ(ctx.ability_levels[1][4], 4, "880a 核心：R3 过期 → 对手属性技恢复（速度再+2）");
        std::cout << "  [880a] 919 后手封属主路径 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // 880b 先手门槛对照：快速 38283 → 不挂票；同链 909（先手烧伤）照常
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kXiongMo, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/300, /*atk=*/1, /*hp=*/5000)),
                         make_party(foe_pet({kGaoSu, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*speed=*/100, /*atk=*/1, /*hp=*/100000)));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, 0, 0);   // R1：己方先手 38283 → 909 烧伤落地；919 门槛短路不挂票
        CHECK(!seal_ticket_alive(ctx, 1, SealKind::SEAL_ATTRIBUTE),
              "880b 核心：先手不挂封属票（整回合后查——若误挂 duration=2 票此时仍存活）");
        CHECK_EQ(ctx.ability_levels[1][4], 2, "880b 核心：先手不封 → 对手高速移动照常落地");
        CHECK(has_anomaly(ctx, 1, AbnormalStatusId::Burn),
              "880b 对照：同链 909（先出手 100% 烧伤）门槛极性相反、照常触发");
        std::cout << "  [880b] 919 先手门槛对照（909 反向对照）通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // 880c 窗口内只封属性不封攻击：对手攻击技照常掉血
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kXiongMo, kPaDa, kPaDa, kPaDa, kPaDa},
                                              /*speed=*/100, /*atk=*/1, /*hp=*/5000)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                            /*speed=*/300, /*atk=*/10, /*hp=*/100000)));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, 0, 0);   // R1：对手先手拍打 → 己方后手 38283 授票
        CHECK(seal_ticket_alive(ctx, 1, SealKind::SEAL_ATTRIBUTE), "880c 前提：票已挂");
        const int hp_after_r1 = ctx.getPet(0).hp;
        play_round_to_effect_done(b, 1, 0);   // R2 中段：票仍在窗口内（未刷新、未扣减）
        CHECK(seal_ticket_alive(ctx, 1, SealKind::SEAL_ATTRIBUTE), "880c 前提：R2 票在窗口内");
        b.drive();
        CHECK(ctx.getPet(0).hp < hp_after_r1,
              "880c 核心：封属窗口内对手攻击技照常落地（919 只封属性技）");
        std::cout << "  [880c] 919 窗口内攻击技不封 通过\n";
    }

    std::cout << "scenario_880 PASS (X1 effect 919 后手守卫修正回归，段 880)" << std::endl;
    return 0;
}
