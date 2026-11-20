#include <utils/sbs_rand.h>
// 场景 082：37 砥砺 + 41 狂信（异常线收口：44 槽全部接入）
//
// 口径来源（官方 effect_des，均已核对原文）：
//   37 砥砺（id=300, kind=2, icon=37）
//     「附属类异常状态，处于该状态的精灵**受到真实伤害后**，若**本回合未执行过附加异常状态的效果**，
//       则**增加此伤害值 80%** 的体力」
//     语料（idx=157/165/90）：「每次受到真伤后触发**实时**结算的增加体力，体力为0则不触发」；
//     「可以被免疫效果响应」；「执行异常效果的来源**必须是对方**，如果通过**第三方效果**是不算进去的
//       （比如圣武砥砺、涂碟黑幕），另外**主动毒和被动毒也是不算进去**」
//   41 狂信（id=515, kind=2, icon=41）
//     「控制类异常状态，该状态下**对手为信仰对象时**自身无法行动；**进入狂信状态时**，
//       **若自身没有信仰对象**，则**本次狂信状态的来源**成为自身信仰对象」
//   信仰对象（id=517, kind=6 —— 独立的**规则词条**，不是异常状态）
//     「**战斗中每只精灵恒常存在的规则属性**，初始为无对象」
//     生命期三条互证（见开工文档 §十）：517「恒常存在」+ 同族 536 明写"未注明时默认下场清"（517 没写）
//     + 语料「哪怕是**阵亡精灵**也得信仰教皇，除非该精灵**消逝**」
//     + `effect_info` 2544「**自身位于场下时**，为自身的信仰对象提供支援」→ **pet 上、下场保留**
//
// 验证点：
//   P 砥砺
//     P1 受真伤 → 回该伤害值 80% 的体力（实时）
//     P2 本回合**对方执行过附加异常效果** → 不回血（条件位）
//     P3 属性直伤（ATTRIBUTE）不是真伤 → 不触发
//     P4 回血走 heal 通道 → 被封回血挡下（"可以被免疫效果响应"）
//   Q 狂信
//     Q1 进入狂信时，若自身没有信仰对象 → 来源（对面在场那只）成为信仰对象
//     Q2 对手**就是**信仰对象 → 禁行动
//     Q3 对手切换成**非**信仰对象的那只 → 挂着狂信**依然能行动**（两段式语义）
//     Q4 已有信仰对象时不被覆盖（条件写）
//     Q5 免疫挡下（未进入）→ 不设信仰对象
#include <array>
#include <cassert>
#include <cstdlib>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <effects/rule_center.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kGaoSu = 20017;   // 高速移动：属性技能、必中、威力 0
constexpr int kPaiDa = 10004;   // 拍打：物理、必中、威力 40
constexpr int kHp = 5000;

constexpr int kResolution = static_cast<int>(AbnormalStatusId::Resolution);     // 37 砥砺
constexpr int kFanaticism = static_cast<int>(AbnormalStatusId::Fanaticism);     // 41 狂信

using SkillArr = std::array<int, 5>;
const SkillArr all_gaosu() { return {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu}; }

ElfPet attacker(const SkillArr& s) {
    return scenario::make_pet(0, "攻方", s, /*speed=*/200, /*atk=*/100, kHp);
}
ElfPet defender(const SkillArr& s) {
    return scenario::make_pet(1, "守方", s, /*speed=*/100, /*atk=*/100, kHp);
}
ScenarioBattle make_battle(const SkillArr& s0 = all_gaosu(), const SkillArr& s1 = all_gaosu()) {
    return ScenarioBattle({scenario::make_party(attacker(s0))}, {scenario::make_party(defender(s1))});
}
void run_round(ScenarioBattle& b, int s0 = 0, int s1 = 0) {
    sbs_srand(20260918u);
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, s0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, s1);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //==================== P 砥砺 ====================
    // P1 受真伤 → 回该伤害值 80%（实时）
    {
        auto b = make_battle();
        b.start();
        BattleContext& ctx = b.ctx();
        apply_anomaly(&ctx, 1, kResolution, /*duration_rounds=*/3, /*actor=*/0);
        const int hp0 = ctx.getPet(1).hp;
        deal_true_damage(&ctx, 1, 1000, /*actor=*/0);
        // 掉 1000、回 800 → 净 -200
        CHECK_EQ(hp0 - ctx.getPet(1).hp, 1000 - 800,
                 "P1 砥砺应令受真伤者回复该伤害值 80% 的体力（实时）");
        std::cout << "P1 PASS 砥砺受真伤 1000 → 回 800（净 -200）\n";
    }

    // P2 本回合**对方执行过附加异常效果** → 不回血
    //    ⚠️ 注意顺序：条件位在"对方执行附加异常"时置位，之后本回合内受真伤就不回血。
    {
        auto b = make_battle();
        b.start();
        BattleContext& ctx = b.ctx();
        apply_anomaly(&ctx, 1, kResolution, 3, 0);          // 砥砺本身也是"附加异常"→ 会置位！
        // 所以先清掉这次置位，模拟"砥砺是上一回合挂的、本回合还没人附加异常"
        ctx.ws.anomaly_applied_by_opponent[1] = false;
        const int hp0 = ctx.getPet(1).hp;
        deal_true_damage(&ctx, 1, 1000, 0);
        CHECK_EQ(hp0 - ctx.getPet(1).hp, 200, "P2 前置：无附加异常历史时应回血");

        // 现在让"对方"执行一次附加异常 → 条件位置起 → 再受真伤不回血
        apply_anomaly(&ctx, 1, static_cast<int>(AbnormalStatusId::Burn), 2, /*actor=*/0);
        const int hp1 = ctx.getPet(1).hp;
        deal_true_damage(&ctx, 1, 1000, 0);
        CHECK_EQ(hp1 - ctx.getPet(1).hp, 1000,
                 "P2 本回合对方执行过附加异常效果 → 砥砺不回血（条件位生效）");
        std::cout << "P2 PASS 本回合有对面附加异常则不回血\n";
    }
    // P2b 主动毒/被动毒**不算**"执行附加异常效果"（语料明确排除）
    {
        auto b = make_battle();
        b.start();
        BattleContext& ctx = b.ctx();
        apply_anomaly(&ctx, 1, kResolution, 3, 0);
        ctx.ws.anomaly_applied_by_opponent[1] = false;
        apply_anomaly_ancient(&ctx, 1, static_cast<int>(AbnormalStatusId::Burn), 2, 0);  // 主动毒
        apply_anomaly_raw(&ctx, 1, static_cast<int>(AbnormalStatusId::Poison), 2, 0);   // 被动毒
        CHECK(!ctx.ws.anomaly_applied_by_opponent[1],
              "P2b 主动毒/被动毒不应置起砥砺的条件位（语料 idx=90/157 明确排除）");
        const int hp0 = ctx.getPet(1).hp;
        deal_true_damage(&ctx, 1, 1000, 0);
        CHECK_EQ(hp0 - ctx.getPet(1).hp, 200, "P2b 故此刻受真伤仍应回血 80%");
        std::cout << "P2b PASS 主动毒/被动毒不算附加异常效果 → 照常回血\n";
    }

    // P3 属性直伤（ATTRIBUTE）不是真伤 → 不触发砥砺
    {
        auto b = make_battle();
        b.start();
        BattleContext& ctx = b.ctx();
        apply_anomaly(&ctx, 1, kResolution, 3, 0);
        ctx.ws.anomaly_applied_by_opponent[1] = false;
        const int hp0 = ctx.getPet(1).hp;
        const int kElement[2] = {3, 3};
        deal_attribute_damage(&ctx, 1, 1000, kElement, /*actor=*/0);
        CHECK_EQ(hp0 - ctx.getPet(1).hp, 1000,
                 "P3 属性直伤不是真伤 → 砥砺不触发（官方只写「真实伤害」）");
        std::cout << "P3 PASS 属性直伤不触发砥砺\n";
    }

    // P4 回血走 heal 通道 → 被封回血挡下（"可以被免疫效果响应"）
    {
        auto b = make_battle();
        b.start();
        BattleContext& ctx = b.ctx();
        apply_anomaly(&ctx, 1, kResolution, 3, 0);
        ctx.ws.anomaly_applied_by_opponent[1] = false;
        ctx.grant_immunity(1, ImmunityType::HEAL_BLOCK, coverage_all(), 0,
                           /*duration_rounds=*/3, /*source_id=*/9001);
        const int hp0 = ctx.getPet(1).hp;
        deal_true_damage(&ctx, 1, 1000, 0);
        CHECK_EQ(hp0 - ctx.getPet(1).hp, 1000, "P4 封回血应挡下砥砺的回复（走 heal 通道）");
        std::cout << "P4 PASS 砥砺回血吃封回血（可被免疫效果响应）\n";
    }

    //==================== Q 狂信 ====================
    // Q1/Q2 进入狂信 → 来源成为信仰对象；对手就是信仰对象 → 禁行动
    {
        auto b = make_battle();
        b.start();
        BattleContext& ctx = b.ctx();
        const ApplyAnomalyResult r = apply_anomaly(&ctx, 1, kFanaticism, /*duration_rounds=*/3,
                                                  /*actor=*/0);
        CHECK(r == ApplyAnomalyResult::SUCCESS, "Q1 狂信应成功落地");
        const FaithTarget t = ctx.faith_target(1);
        CHECK_EQ(t.side, 0, "Q1 信仰对象应为**来源方**(0)");
        CHECK_EQ(t.slot, ctx.on_stage[0], "Q1 信仰对象的槽位应为来源方**此刻在场那只**");
        CHECK(ctx.opponent_is_faith_target(1), "Q1 此刻在场的对手就是信仰对象");

        // Q2 禁行动的**行为观测**：被禁行动时整段主流程被跳走（`should_skip_action_flow`），
        //    因此 `AFTER_ACTION` 的 `consume_selected_skill_pp` **不会**执行 → 所选技能的 PP 不变。
        //    这是"真的没出手"的可观测证据（比只问谓词强）。
        Skills& sk = ctx.getPet(1).skills[2];
        const int pp_before = sk.pp;
        run_round(b, /*s0=*/0, /*s1=*/2);
        CHECK_EQ(ctx.getPet(1).skills[2].pp, pp_before,
                 "Q2 狂信（对手即信仰对象）应令守方**无法行动**（所选技能 PP 未被消耗）");
        std::cout << "Q1/Q2 PASS 狂信落地、信仰对象=来源方在场精灵，且守方无法行动（PP 未消耗）\n";
    }

    // Q3 **两段式**：对手切换成非信仰对象的那只 → 挂着狂信依然能行动（语料原句）
    {
        auto b = make_battle();
        b.start();
        BattleContext& ctx = b.ctx();
        apply_anomaly(&ctx, 1, kFanaticism, /*duration_rounds=*/9, /*actor=*/0);
        CHECK_EQ(ctx.faith_target(1).slot, 0, "Q3 前置：信仰对象是 0 号槽");
        // 让 0 方中切到 1 号槽（信仰对象仍是 0 号槽 → 不再是"在场那只"）
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.on_stage[0], 1, "Q3 攻方应已切到 1 号槽");
        CHECK_EQ(ctx.faith_target(1).slot, 0, "Q3 信仰对象**仍是** 0 号槽（存的是引用，不随切换重算）");
        CHECK(!ctx.opponent_is_faith_target(1),
              "Q3 此刻在场的对手(1 号槽)不是信仰对象 → 狂信**不再**禁行动");
        // 行为观测：这时守方**能**出手了 → 下一回合它选用的技能 PP 应被消耗
        Skills& sk = ctx.getPet(1).skills[2];
        const int pp_before = sk.pp;
        run_round(b, /*s0=*/0, /*s1=*/2);
        CHECK_EQ(ctx.getPet(1).skills[2].pp, pp_before - 1,
                 "Q3 对照：对手不是信仰对象时守方**能行动**（所选技能 PP 被消耗 1）");
        std::cout << "Q3 PASS 信仰对象是存下来的引用；对手换成非信仰对象则狂信不禁行动"
                     "（PP 被消耗 → 真的出手了）\n";
    }

    // Q4 已有信仰对象 → 狂信进入时**不覆盖**（条件写）
    {
        auto b = make_battle();
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.set_faith_target(1, /*target_side=*/0, /*target_slot=*/4);   // 预置一个"非在场"的对象
        apply_anomaly(&ctx, 1, kFanaticism, 3, /*actor=*/0);
        const FaithTarget t = ctx.faith_target(1);
        CHECK_EQ(t.slot, 4, "Q4 已有信仰对象时狂信不应覆盖（官方「若自身没有信仰对象」）");
        // 强制改写入口（教皇魂印"改为其"、索伦森"改为索伦森"用）→ 可以改
        ctx.set_faith_target(1, 0, 0);
        CHECK_EQ(ctx.faith_target(1).slot, 0, "Q4 强制改写入口应能覆盖（供魂印用）");
        std::cout << "Q4 PASS 狂信是条件写、不覆盖已有信仰对象；强制改写入口可覆盖\n";
    }

    // Q5 免疫挡下（未进入）→ 不设信仰对象
    {
        auto b = make_battle();
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.grant_immunity(1, ImmunityType::ANOMALY, coverage_all(), 0, 0, /*source_id=*/9002,
                           /*soul_immunity=*/false);
        CHECK(apply_anomaly(&ctx, 1, kFanaticism, 3, 0) == ApplyAnomalyResult::TARGET_IMMUNE,
              "Q5 前置：应被免疫挡下");
        CHECK_EQ(ctx.faith_target(1).side, -1,
                 "Q5 未真正进入狂信 → 不设信仰对象（语料「免疫或者抵抗都是没有信仰对象」）");
        std::cout << "Q5 PASS 未进入狂信则不设信仰对象\n";
    }

    // Q6 信仰对象存在 pet 的 soulmark_storage（下场保留）——换宠后原宠的信仰对象仍在
    {
        auto b = make_battle();
        b.start();
        BattleContext& ctx = b.ctx();
        apply_anomaly(&ctx, 1, kFanaticism, 9, /*actor=*/0);
        CHECK(ctx.getPet(1).soulmark_storage.count(kFaithTargetStorageKey) == 1,
              "Q6 信仰对象应存在 pet 的 soulmark_storage（下场保留槽）");
        CHECK(ctx.getPet(1).on_stage_storage.count(kFaithTargetStorageKey) == 0,
              "Q6 不应存在 on_stage_storage（那是下场清的槽）");
        std::cout << "Q6 PASS 信仰对象落在 soulmark_storage（下场保留）\n";
    }

    std::cout << "\n场景 082 全部通过（砥砺：真伤实时回血+条件位；狂信：信仰对象建模+两段式禁行动）\n";
    return 0;
}
