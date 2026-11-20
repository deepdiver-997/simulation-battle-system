// 场景 163：斯布林蒂 4600 技能组缺口（二）—— 大数回廊三件套
//（moves_lib/batch_c_skills.cpp，2026-09-27）
//
// 覆盖效果（大数回廊 28586 = [1754, 1388, 2109, 1909]，属性、必中）：
//   1754 获得 300 护盾，护盾消失时附加 300 固定伤害 + 自身下 1 次攻击伤害翻倍
//        （破盾 → ROUND_END 统一结算；授予在 ROUND_END、天然晚于自己这次攻击）
//   1388 获得 300 护罩，护罩消失时使对手 1 回合攻击技能无效（SEAL_ATTACK 票）
//   2109 免疫下 1 次异常，免疫成功后消除对手回合类效果与能力提升状态
//        （1993 真伤窗做"回合类效果"金丝雀：破窗后每回合 300 真伤停止）
//   1909 非缺口（shengguang_monika_skills.cpp 已注册 effect_1909_reset_hp，数据驱动生效）
//
// 确定性纪律：红伤公式带 (217+rand%39)/255 随机因子 → 红伤值一律不断言 exact；
// 粉伤/固伤/真伤/能力等级/授予结构是确定值，才进 CHECK_EQ。
#include <cstdlib>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kDashu = 28586;      // 大数回廊：1754+1388+2109+1909（属性、必中）
constexpr int kCanary1993 = 28327; // 玲·沧吟星海（1993 窗口金丝雀：每回合用技附加 300 真伤）
constexpr int kFiller190 = 22638;  // 虚空之遗（属性、必中；190 窗对无提升方是 no-op）
constexpr int kHit = 10004;
constexpr int kHarmless = 10938;

ElfPet sibu(std::array<int, 5> skills, int speed = 200, int hp = 100000,
            int atk = 100) {
    ElfPet p = scenario::make_pet(0, "斯布林蒂", skills, speed, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe(std::array<int, 5> skills, int atk = 10, int hp = 100000) {
    ElfPet p = scenario::make_pet(1, "对手", skills, /*speed=*/100, atk, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

bool has_seal_entry(const BattleContext& ctx, int effect_id) {
    for (const RuleTicket& t : ctx.rule_center_.entries()) {
        if (t.source_effect_id == effect_id) {
            return true;
        }
    }
    return false;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== T1 1754：破盾 → ROUND_END 附加 300 固伤 + 下次攻击伤害翻倍 ==========
    {
        ScenarioBattle b(scenario::make_party(sibu({kDashu, kHit, kHarmless, kHarmless,
                                                    kHarmless})),
                         scenario::make_party(foe({kHit, kHarmless, kHarmless, kHarmless,
                                                   kHarmless}, /*atk=*/2000)));
        b.start();
        BattleContext& ctx = b.ctx();
        // R1：我先手武装（盾 300/罩 300/免异常 1 次）；对手后手大红伤（≥570）一击爆盾
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(0).shield_bank_.quantity_of(1754), 0,
                 "T1 前置：对手红伤把 1754 护盾打爆");
        CHECK_EQ(ctx.getPet(1).hp, 100000 - 300,
                 "T1 核心：破盾当回合 ROUND_END 附加 300 固定伤害（对手本回合没挨别的伤）");
        CHECK_EQ(ctx.attack_boost_grants[0].size(), 1u, "T1 核心：授予 1 条攻击增伤");
        CHECK_EQ(ctx.attack_boost_grants[0][0].source_effect_id, 1754, "T1 来源 = 1754");
        CHECK_EQ(ctx.attack_boost_grants[0][0].remaining, 1, "T1 次数 = 1");
        CHECK_EQ(ctx.attack_boost_grants[0][0].pct, 100, "T1 翻倍 = +100%");
        // R2：我的 kHit 消费翻倍增伤（若授予被自己这次攻击提前吃掉，这里就是空的）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        CHECK_EQ(ctx.attack_boost_grants[0].size(), 0u,
                 "T1 顺序：增伤被我的下一次攻击消费（下 1 次语义）");
        std::cout << "  [T1] 1754 破盾固伤+翻倍 通过\n";
    }

    //========== T2 1388：护罩消失 → 对手 1 回合攻击技能无效 ==========
    {
        ScenarioBattle b(scenario::make_party(sibu({kDashu, kHit, kHarmless, kHarmless,
                                                    kHarmless})),
                         scenario::make_party(foe({kHit, kFiller190, kHarmless, kHarmless,
                                                   kHarmless})));
        b.start();
        BattleContext& ctx = b.ctx();
        // R1：双方都无红伤（我武装、对手 filler）→ 我满血，罩 300 在
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        CHECK_EQ(ctx.getPet(0).hood_bank_.quantity_of(1388), 300, "T2 前置：护罩 300 已挂");
        // 粉伤只吃护罩不吃护盾（官方盾/罩分离）→ 300 点数粉伤恰好拿空护罩
        (void)deal_pink_damage(&ctx, 0, 300, DamageKind::FIXED, 1);
        ctx.event_center_.drain(&ctx, ctx.roundCount);   // 直调后手动投递（破盾事件晚 drain）
        CHECK_EQ(ctx.getPet(0).hood_bank_.quantity_of(1388), 0, "T2 前置：护罩被粉伤拿空");
        CHECK_EQ(ctx.getPet(0).hp, 100000, "T2 前置：粉伤被罩全吸收（本体无伤）");
        CHECK(has_seal_entry(ctx, 1388), "T2 核心：护罩消失 → SEAL_ATTACK 票已授");
        // drive 后 advanceRound 已推进（当前=第 2 回合选择期），票 rounds=1 覆盖本回合
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(0).hp, 100000,
                 "T2 核心：护罩消失当回合起对手攻击技能无效（0 伤落地）");
        std::cout << "  [T2] 1388 护罩消失封攻击 通过\n";
    }

    //========== T3 2109：免疫 1 次异常 → 免疫成功消除对手回合类效果 + 能力提升 ==========
    {
        // ★ 先手权由技能优先级决定：大数回廊 priority=1 → 我必先手；对手（后手）的
        //   1993 InRounds 窗按官方口径顺延到 R2 起。因此金丝雀观察点 = "R2 不结算"：
        //   免疫成功 → break_round_effects 拆窗 → R2 末没有那 300 真伤。
        ScenarioBattle b(scenario::make_party(sibu({kDashu, kHit, kHarmless, kHarmless,
                                                    kHarmless})),
                        scenario::make_party(foe({kCanary1993, kFiller190, kHarmless,
                                                  kHarmless, kHarmless})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][0] = 2;   // 对手攻击+2（待清除的提升）
        // R1：我武装（含 2109 次免 1 次）；对手用 1993 金丝雀（窗口已挂、R2 起结算）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(ctx.has_round_effects(1), "T3 前置：对手 1993 回合窗已挂（R2 起结算）");
        // 直调申报原语：100% 烧伤打我 → 被次免票挡下
        const ApplyAnomalyResult r = apply_anomaly(
            &ctx, 0, static_cast<int>(AbnormalStatusId::Burn), 3, /*actor=*/1, 100,
            ChanceSource::Skill);
        ctx.event_center_.drain(&ctx, ctx.roundCount);   // 免疫成功事件晚 drain（驱动 2109 清除）
        CHECK(r != ApplyAnomalyResult::SUCCESS && r != ApplyAnomalyResult::REPLACED_EXISTING
              && r != ApplyAnomalyResult::DURATION_EXTENDED,
              "T3 核心：异常被次免票挡下");
        CHECK(!ctx.has_active_abnormal_status(0, 2), "T3 核心：烧伤未落地");
        CHECK_EQ(ctx.ability_levels[1][0], 0,
                 "T3 核心：免疫成功 → 对手能力提升被消除");
        CHECK(!ctx.has_round_effects(1), "T3 核心：免疫成功 → 对手回合类效果被消除");
        // R2：金丝雀窗已被 break_round_effects 拆除 → R2 末不再扣 300 真伤
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 2);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        CHECK_EQ(ctx.getPet(0).hp, 100000,
                 "T3 核心：对手回合类效果被消除（1993 真伤窗 R2 未结算）");
        std::cout << "  [T3] 2109 免疫+消回合+消提升 通过\n";
    }

    std::cout << "scenario_163 all passed" << std::endl;
    return 0;
}
