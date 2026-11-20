#include <utils/sbs_rand.h>
// 场景 066：技能批 1 —— 原语直通族 + 古早族补员（2026-09-17）。
//
// 口径来源：effect_info 模板原文（数据库为准）+ 交接文档批量实现作战计划：
//   - 31  连击族（乱突/疯狂乱抓 [2,5]）：无 effect_info 行的裸连击模板，次数由
//         loadSkills 的 combo_arg_indices 消化，效果本体 no-op（审计消音用）。
//   - 47  {0}回合内免疫能力下降状态：窗口型免弱票（grant_immunity STAT_DROP）。
//   - 180 消除对手回合类效果：break_round_effects 直通（吃免断）。
//   - 447 造成的伤害不少于{0}：保底伤害 = 管线 FLOOR 阶段（减伤区后、锁伤前）。
//   - 521 反转自身能力下降状态：stat_reversal 直通，AFTER_ACTION（同 1221 口径：
//         先结算伤害再反转，反转不参与本次伤害）。
//   - 16  命中后{0}%令对手睡眠：古早施加族补员（apply_anomaly_ancient 主动毒通道）。
//   - 456 若对手体力不足{0}则直接秒杀：运行时解析器 TargetHpBelow+Kill 已覆盖；
//         Kill 标签走 force_hp_to_zero（秒杀体系：免疫票/短路转化/事件）。
//
// 验证点：
//   E1 31：静态基数 2~5 + 端到端 combo_view 落在区间、伤害 ≈ N 段
//   E2 47：免弱窗口内 stat_drop → IMMUNE；窗口过期后 → 成功弱化
//   E3 180：清掉守方回合类效果；守方免断 → 清不掉
//   E4 447：弱攻（裸伤 < 保底）→ 伤害恰好 = 保底值；强攻（裸伤 > 保底）→ 不受影响
//   E5 521：带 -2 攻时伤害按 -2 结算（反转不参与本次伤害）、行动后等级反转为 +2
//   E6 16：100% 睡眠落地；Modern 层免疫+弹控不挡（古早通道）；Ancient 层免疫挡住
//   E7 456：hp<阈值 → 秒杀；hp>阈值 → 正常伤害；hp_zero_converted 短路 → 存活
#include <cassert>
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

constexpr int kPaiDa = 10004;       // 拍打：物攻 40（accuracy 100）
constexpr int kLuanTu = 10022;      // 乱突：物攻 15，effect 31 [2,5]
constexpr int kFengKuang = 10029;   // 疯狂乱抓：物攻 25，effect 31 [2,5]
constexpr int kMianRou = 34187;     // 绵柔冲击：物攻 130，effect 447 [240]（acc 97）
constexpr int kZhenWu = 16474;      // 真武精神：物攻 140，effect 521（acc 95）
constexpr int kHuanHua = 20486;     // 幻化之火：属性，effect 16 [100]（acc 90）
constexpr int kWuMing = 16341;      // 无铭业火：特攻 85，effect 456 [250]（acc 100）

const int kSleep = abnormal_status_id(AbnormalStatusId::Sleep);

using scenario::ScenarioBattle;

ScenarioBattle make_battle(const ElfPet& p0, const ElfPet& p1) {
    sbs_srand(20260916);
    return ScenarioBattle(scenario::make_party(p0), scenario::make_party(p1));
}

void round_of(ScenarioBattle& b, int idx0 = 0, int idx1 = 0) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, idx0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, idx1);
    b.drive();
}

// 圆石（速度 200 必先手）+ 圆石守方。skill 数组同技能填满。
ElfPet attacker(int skill, int atk = 200, int hp = 4000) {
    return scenario::make_pet(0, "攻方", {skill, skill, skill, skill, skill},
                              200, atk, hp);
}
ElfPet defender(int skill = kPaiDa, int hp = 4000) {
    return scenario::make_pet(1, "守方", {skill, skill, skill, skill, skill},
                              100, 100, hp);
}
int strike(ScenarioBattle& b) {
    const int before = b.pet(1).hp;
    round_of(b);
    return before - b.pet(1).hp;
}

// 044 同款：给 owner 预挂一条"回合类效果"（ROUND_START 桶、存活 3 回合），供 180 测试清。
EffectResult marker_effect(BattleContext* ctx, const EffectArgs& args) {
    if (ctx && args.int_args && args.int_count >= 1) {
        ctx->plugin_storage[999066] = true;   // 标记：回合类效果存在过
    }
    return EffectResult::kOk;
}
void register_round_marker(BattleContext& ctx, int owner) {
    Effect e(999066, 0, owner, 3, EffectArgs(std::vector<int>{owner, 1 - owner}), &marker_effect);
    auto ce = std::make_unique<ContinuousEffect>(e, State::BATTLE_ROUND_START, owner,
                                                 /*duration=*/3, ctx.roundCount);
    ce->source_id_ = 999066;
    ctx.register_skill_effect(State::BATTLE_ROUND_START, owner, std::move(ce));
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== E1 effect 31：静态基数 + 端到端连击 ==========
    {
        Skills lt(kLuanTu);      // 乱突 [2,5]
        CHECK_EQ(lt.combo_min, 2, "E1) 乱突 args[0]=2 → 连击下限 2");
        CHECK_EQ(lt.combo_max, 5, "E1) 乱突 args[1]=5 → 连击上限 5");
        Skills fk(kFengKuang);   // 疯狂乱抓 [2,5]
        CHECK_EQ(fk.combo_min, 2, "E1) 疯狂乱抓 下限 2");
        CHECK_EQ(fk.combo_max, 5, "E1) 疯狂乱抓 上限 5");
        std::cout << "E1a PASS effect 31 静态基数 2~5\n";

        ScenarioBattle b = make_battle(attacker(kFengKuang, 200), defender());
        b.start();
        const int dmg = strike(b);
        const int n = b.ctx().ws.combo_view[0];
        CHECK(n >= 2 && n <= 5, "E1) combo_view 应落在 [2,5]（实际 " + std::to_string(n) + "）");
        // 单段裸伤 ≈ 0.84*25*200/100 + 2 = 44；×N 段 → 总伤 ≥ 2 段下界（浮动向下的 85%）
        CHECK(dmg >= 2 * 44 * 85 / 100, "E1) 多段伤害应 ≥ 2 段下界");
        std::cout << "E1b PASS 疯狂乱抓端到端：combo=" << n << "，伤害 " << dmg << "\n";
    }

    //========== E2 effect 47：免弱窗口 ==========
    {
        // 第 1 回合用 15255 授予窗口；后两回合改用拍打（不再刷新票）→ 第 3 回合窗口应过期。
        ScenarioBattle b = make_battle(
            scenario::make_pet(0, "攻方", {15255, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 200, 200, 4000),
            defender());
        b.start();
        round_of(b, /*idx0=*/0);   // 离子光速拳 [2] → 攻方自身获 2 回合免弱（同 48 的自身保护语义）
        auto& ctx = b.ctx();
        const StatDropResult r1 = stat_drop(&ctx, /*target=*/0, /*stat=*/0, /*amount=*/2);
        CHECK_EQ(r1 == StatDropResult::IMMUNE, true, "E2) 免弱窗口内 stat_drop 应 IMMUNE");
        CHECK_EQ(ctx.ability_levels[0][0], 0, "E2) 免弱期间等级一点不动");
        std::cout << "E2a PASS 窗口内免弱（IMMUNE，等级不变）\n";

        // 推进到窗口外（授于回合 R，in_rounds 覆盖 R..R+1）→ 第 3 回合应过期
        round_of(b, /*idx0=*/1);   // 拍打：不刷新免弱票
        round_of(b, /*idx0=*/1);
        const StatDropResult r2 = stat_drop(&ctx, 0, 0, 2);
        CHECK_EQ(r2 == StatDropResult::SUCCESS, true, "E2) 窗口过期后 stat_drop 应成功");
        CHECK_EQ(ctx.ability_levels[0][0], -2, "E2) 过期后弱化落地（攻击 -2）");
        std::cout << "E2b PASS 窗口过期（round " << ctx.roundCount << " 后弱化成功）\n";
    }

    //========== E3 effect 180：消回合 + 免断 ==========
    {
        ScenarioBattle b = make_battle(attacker(16621), defender());  // 迷踪击 [180]
        b.start();
        register_round_marker(b.ctx(), /*owner=*/1);
        CHECK(b.ctx().has_round_effects(1), "E3) 前提：守方已有回合类效果");
        strike(b);
        CHECK(!b.ctx().has_round_effects(1), "E3) 迷踪击应清掉守方回合类效果");
        std::cout << "E3a PASS 消回合清掉守方回合类效果\n";

        ScenarioBattle b2 = make_battle(attacker(16621), defender());
        b2.start();
        register_round_marker(b2.ctx(), 1);
        b2.ctx().grant_immunity(/*owner=*/1, ImmunityType::BREAK, coverage_all(),
                                /*anomaly_mask=*/0, /*duration_rounds=*/3,
                                /*source_id=*/999066, /*soul_immunity=*/false,
                                EffectScope::ON_STAGE, /*counts=*/0, /*source_effect_id=*/999066);
        strike(b2);
        CHECK(b2.ctx().has_round_effects(1), "E3) 守方免断时消回合应无效");
        std::cout << "E3b PASS 免断挡住消回合\n";
    }

    //========== E4 effect 447：保底伤害 ==========
    {
        // 弱攻：atk 30 → 单段裸伤 ≈ 0.84*130*30/100 + 2 ≈ 34.8 < 240 → 保底抬到 240
        ScenarioBattle b = make_battle(attacker(kMianRou, /*atk=*/30), defender());
        b.start();
        const int dmg_weak = strike(b);
        CHECK_EQ(dmg_weak, 240, "E4) 裸伤 < 保底时应恰好打出保底 240");
        std::cout << "E4a PASS 保底伤害：弱攻打出 240\n";

        // 强攻：atk 500 → 裸伤 ≈ 0.84*130*500/100 + 2 ≈ 548 > 240 → 不受影响
        ScenarioBattle b2 = make_battle(attacker(kMianRou, /*atk=*/500), defender());
        b2.start();
        const int dmg = strike(b2);
        CHECK(dmg > 240, "E4) 裸伤 > 保底时保底不应生效（实际 " + std::to_string(dmg) + "）");
        std::cout << "E4b PASS 裸伤高于保底不受影响（" << dmg << "）\n";
    }

    //========== E5 effect 521：反转自身下降（先伤害后反转）==========
    {
        // 对照：无下降 → D0
        ScenarioBattle ctrl = make_battle(attacker(kZhenWu), defender());
        ctrl.start();
        const int d0 = strike(ctrl);
        CHECK(d0 > 0, "E5) 前提：对照有伤害");

        // 带 -2 攻：伤害应按 -2 结算（< D0），行动结束后反转为 +2
        ScenarioBattle b = make_battle(attacker(kZhenWu), defender());
        b.start();
        b.ctx().ability_levels[0][0] = -2;
        b.ctx().ws.view_levels[0][0] = -2;
        const int dmg = strike(b);
        CHECK(dmg < d0, "E5) 反转不参与本次伤害：伤害应按 -2 结算（"
              + std::to_string(dmg) + " vs 对照 " + std::to_string(d0) + "）");
        CHECK_EQ(b.ctx().ability_levels[0][0], 2, "E5) 行动结束后 -2 应反转为 +2");
        std::cout << "E5 PASS 反转自身下降（先伤害 " << dmg << " < " << d0
                  << "，后反转 +2）\n";

        // 无下降可反转：NOTHING → 等级保持 0
        ScenarioBattle b2 = make_battle(attacker(kZhenWu), defender());
        b2.start();
        strike(b2);
        CHECK_EQ(b2.ctx().ability_levels[0][0], 0, "E5) 无下降时反转无事发生");
        std::cout << "E5b PASS 无下降时反转无事发生\n";
    }

    //========== E6 effect 16：古早睡眠（通道语义）==========
    {
        // S1：100% 睡眠落地
        ScenarioBattle b = make_battle(attacker(kHuanHua), defender());
        b.start();
        strike(b);
        CHECK(b.ctx().has_active_abnormal_status(1, kSleep), "E6) 幻化之火应令守方睡眠");
        std::cout << "E6a PASS 古早睡眠落地\n";

        // S2：Modern 层免疫 + 弹控 → 古早通道不可见，睡眠照常落地
        ScenarioBattle b2 = make_battle(attacker(kHuanHua), defender());
        b2.start();
        b2.ctx().grant_immunity(1, ImmunityType::ANOMALY, coverage_all(), 0, 3,
                                /*source_id=*/999066, false, EffectScope::ON_STAGE,
                                0, 999066, ImmunityTier::Modern);
        b2.ctx().rule_center_.grant_reflect(1, -1, 999066, 1, 3);
        strike(b2);
        CHECK(b2.ctx().has_active_abnormal_status(1, kSleep),
              "E6) 古早施加应穿透 Modern 层免疫/弹控");
        std::cout << "E6b PASS 穿透 Modern 免疫与弹控\n";

        // S3：Ancient 层免疫 → 挡住
        ScenarioBattle b3 = make_battle(attacker(kHuanHua), defender());
        b3.start();
        b3.ctx().grant_immunity(1, ImmunityType::ANOMALY, coverage_all(), 0, 3,
                                /*source_id=*/999067, false, EffectScope::ON_STAGE,
                                0, 999067, ImmunityTier::Ancient);
        strike(b3);
        CHECK(!b3.ctx().has_active_abnormal_status(1, kSleep),
              "E6) Ancient 层免疫应挡住古早睡眠");
        std::cout << "E6c PASS Ancient 层免疫挡住\n";
    }

    //========== E7 effect 456：条件秒杀（解析器 TargetHpBelow + Kill→force_hp_to_zero）==========
    {
        // 无铭业火 16341：args[0]=200 → "若对手体力不足 200 则直接秒杀"（严格小于）。
        // T1：hp 150 < 200 → 秒杀（hp 归 0）
        ScenarioBattle b = make_battle(attacker(kWuMing), defender(kPaiDa, /*hp=*/150));
        b.start();
        strike(b);
        CHECK_EQ(b.pet(1).hp, 0, "E7) 体力不足 200 应被直接秒杀");
        std::cout << "E7a PASS 条件秒杀（150 < 200 → 0）\n";

        // T2：hp 4000 > 200 → 正常伤害，不秒杀
        ScenarioBattle b2 = make_battle(attacker(kWuMing), defender());
        b2.start();
        const int dmg = strike(b2);
        CHECK(b2.pet(1).hp > 0, "E7) 体力充足不应被秒杀");
        CHECK(dmg > 0, "E7) 正常攻击伤害应 > 0");
        std::cout << "E7b PASS 体力充足不秒杀（伤害 " << dmg << "）\n";

        // T3：秒杀短路（hp_zero_converted）→ 存活，只吃正常伤害
        ScenarioBattle b3 = make_battle(attacker(kWuMing), defender(kPaiDa, /*hp=*/150));
        b3.start();
        b3.ctx().hp_zero_converted[1] = true;
        strike(b3);
        CHECK(b3.pet(1).hp > 0, "E7) 秒杀被短路后应存活");
        std::cout << "E7c PASS 秒杀短路存活（剩 " << b3.pet(1).hp << "）\n";
    }

    std::cout << "\n场景 066 全部通过：技能批 1（31/47/180/447/521/16/456）✓" << std::endl;
    return 0;
}
