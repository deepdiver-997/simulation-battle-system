// 场景 057：异常施加双通道 + 免疫层级（2026-09-16 用户拍板口径）。
//
// 口径来源：
//   - 数据库验证（详见 docs_local/docs/06-交接/魂印与技能机制-交接.md 2026-09-16 段）：
//     古早施加 = effect 10/11/12/14/15"命中后{0}%令对方XX"（瞬雷天闪 10176 = effect 10、5% 麻痹）
//     + 特性接触施加（Eid 6/66/67）；现代施加 = 参数化族"{0}%令对手{1}"（月下伏 37526 =
//     effect 2189、100% 焚烬）。免疫侧：古代层 = effect 48（意志模组·怒 29025，纯免不反弹）；
//     现代弹控 = effect 191（1131 个技能）。异常码 = battle_effects 命名空间（引擎 AbnormalStatusId）。
//   - 语料《浅谈主动毒与魂印免控判定》（reference/scrape/out/all_posts_final.jsonl）：
//     主动毒（古早施加）无视一切无补丁免疫（= Modern 层）、**全部弹控**、异常抗性、转化异常；
//     只有老免控（= Ancient 层，如 effect 48）与老魂免（Mark 0 兜底）能挡。
//
// 拦截矩阵（本场景逐格验证）：
//                  | Modern 层免疫 | Ancient 层免疫 | REFLECT | 抗性 | 转化 |
//   现代施加(apply)|     挡✓       |      挡✓      |  反弹✓  | 抗✓  | 转✓ |
//   古早施加(ancient)|    穿✓       |      挡✓      |  穿✓    | 穿✓  | 穿✓ |
//
// 验证点：
//   C1 无免疫对照：双通道都 SUCCESS 落地
//   C2 Ancient 层票：双通道都 TARGET_IMMUNE（古代层=高级免，两种控都挡）
//   C3 Modern 层次数票：古早穿透**且不消耗次数**；现代被挡且消耗（"免下N次"暂按 Modern）
//   C4 REFLECT（光之结界 22240 真件 [191]）：现代施加 → REFLECTED 反弹给施放方；
//      古早施加 → 穿透落地、不反弹（"技能弹控不能免疫主动毒，魂印弹控也不能"）
//   C5 抗性（100% 控制类抗性槽）：现代 → RESISTED_BY_RESISTANCE；古早 → 无视落地
//   C6 转化（麻痹→烧伤）：现代 → CONVERTED 落地烧伤；古早 → 无视转化落地麻痹
//   C7 Mark 0 老魂免兜底：古早也被挡（Mark 0 属古代层）
//   S8 effect 48 真件（星光护体 21420 = [4,4,48]）：授予的 ANOMALY 票 tier=Ancient，
//      古早施加被挡（意志模组·怒式纯免语义）
//   S9 effect 2189 真件（月下伏 37526）：PP 总和高于对手时 100% 令对手焚烬（现代通道）
#include <cassert>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <effects/rule_center.h>
#include <entities/mark.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

constexpr int kShunLei = 10176;   // 瞬雷天闪：[10] args 5（5% 麻痹，古早施加族代表）
constexpr int kGuangZhiJie = 22240;  // 光之结界：[191] args 3（弹控真件）
constexpr int kXingGuang = 21420;    // 星光护体：[4,4,48] args [1,100,1,3,100,1,?]（48 真件）
constexpr int kYueXiaFu = 37526;     // 月下伏：[2189,1159,1108] args [100,22,1,1,1]（现代参数化真件）

using scenario::ScenarioBattle;

ElfPet attacker() {
    return scenario::make_pet(0, "攻方", {kShunLei, kShunLei, kShunLei, kShunLei, kShunLei},
                              200, 100, 400);
}
ElfPet defender() {
    return scenario::make_pet(1, "守方", {kShunLei, kShunLei, kShunLei, kShunLei, kShunLei},
                              100, 100, 400);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== C1 无免疫对照：双通道都落地 ==========
    {
        ScenarioBattle b({scenario::make_party(attacker())},
                         {scenario::make_party(defender())});
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK_EQ(apply_anomaly(&ctx, 1, abnormal_status_id(AbnormalStatusId::Paralysis),
                               2, 0),
                 ApplyAnomalyResult::SUCCESS, "C1 现代通道应落地麻痹");
        CHECK(ctx.has_active_abnormal_status(1, abnormal_status_id(AbnormalStatusId::Paralysis)),
              "C1 守方应处于麻痹");
        CHECK_EQ(apply_anomaly_ancient(&ctx, 1, abnormal_status_id(AbnormalStatusId::Poison),
                                       2, 0),
                 ApplyAnomalyResult::SUCCESS, "C1 古早通道应落地中毒");
        CHECK(ctx.has_active_abnormal_status(1, abnormal_status_id(AbnormalStatusId::Poison)),
              "C1 守方应处于中毒");
        std::cout << "C1 PASS 无免疫对照\n";
    }

    //========== C2 Ancient 层票：双通道都挡 ==========
    {
        ScenarioBattle b({scenario::make_party(attacker())},
                         {scenario::make_party(defender())});
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.grant_immunity(1, ImmunityType::ANOMALY, coverage_all(), 0, 3, /*source_id=*/991,
                           false, EffectScope::ON_STAGE, 0, /*source_effect_id=*/991,
                           ImmunityTier::Ancient);
        const int paralysis = abnormal_status_id(AbnormalStatusId::Paralysis);
        CHECK_EQ(apply_anomaly(&ctx, 1, paralysis, 2, 0),
                 ApplyAnomalyResult::TARGET_IMMUNE, "C2 现代通道被古代层挡");
        CHECK_EQ(apply_anomaly_ancient(&ctx, 1, paralysis, 2, 0),
                 ApplyAnomalyResult::TARGET_IMMUNE, "C2 古早通道被古代层挡");
        CHECK(!ctx.has_active_abnormal_status(1, paralysis), "C2 守方不应有异常");
        std::cout << "C2 PASS 古代层免疫挡双通道\n";
    }

    //========== C3 Modern 层次数票：古早穿透且不消耗；现代被挡且消耗 ==========
    {
        ScenarioBattle b({scenario::make_party(attacker())},
                         {scenario::make_party(defender())});
        b.start();
        BattleContext& ctx = b.ctx();
        // 模拟官方次数型"免下N次"（928/1159/1440 一族，暂按 Modern——用户 2026-09-16 拍板）
        const int sid = ctx.grant_immunity(1, ImmunityType::ANOMALY, coverage_all(), 0, 0,
                                           /*source_id=*/992, false, EffectScope::ON_STAGE,
                                           /*counts=*/1, /*source_effect_id=*/992,
                                           ImmunityTier::Modern);
        const int paralysis = abnormal_status_id(AbnormalStatusId::Paralysis);
        CHECK_EQ(apply_anomaly_ancient(&ctx, 1, paralysis, 2, 0),
                 ApplyAnomalyResult::SUCCESS, "C3 古早通道应穿透 Modern 层次数票");
        CHECK(ctx.has_active_abnormal_status(1, paralysis), "C3 麻痹应落地");
        // 穿透**不消耗**：现代施加仍被挡（票还在）
        CHECK(ctx.is_immune_effect(1, ImmunityType::ANOMALY, ctx.currentState, paralysis),
              "C3 古早穿透不应消耗 Modern 次数票");
        CHECK_EQ(apply_anomaly(&ctx, 1, abnormal_status_id(AbnormalStatusId::Burn), 2, 0),
                 ApplyAnomalyResult::TARGET_IMMUNE, "C3 现代通道被 Modern 次数票挡");
        // 消费后票没了：再施加落地
        CHECK_EQ(apply_anomaly(&ctx, 1, abnormal_status_id(AbnormalStatusId::Burn), 2, 0),
                 ApplyAnomalyResult::SUCCESS, "C3 次数票消费后再施加应落地");
        (void)sid;
        std::cout << "C3 PASS Modern 次数票对古早不可见\n";
    }

    //========== C4 REFLECT 真件（光之结界 [191]）：现代反弹 / 古早穿透 ==========
    {
        ElfPet def = scenario::make_pet(1, "守方",
                                        {kGuangZhiJie, kShunLei, kShunLei, kShunLei, kShunLei},
                                        100, 100, 400);
        ScenarioBattle b({scenario::make_party(attacker())}, {scenario::make_party(def)});
        b.start();
        BattleContext& ctx = b.ctx();
        // 守方使用光之结界（属性技能）：3 回合免疫并反弹
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(ctx.rule_center_.has_reflect(1), "C4 光之结界后守方应有 REFLECT 条目");
        const int paralysis = abnormal_status_id(AbnormalStatusId::Paralysis);
        // 现代施加 → 被反弹：施放方(0)中招，守方(1)干净
        CHECK_EQ(apply_anomaly(&ctx, 1, paralysis, 2, 0),
                 ApplyAnomalyResult::REFLECTED, "C4 现代施加应被弹控反弹");
        CHECK(ctx.has_active_abnormal_status(0, paralysis), "C4 施放方应被弹上麻痹");
        CHECK(!ctx.has_active_abnormal_status(1, paralysis), "C4 守方不应有麻痹");
        // 古早施加 → 穿透弹控：守方中招，施放方无事
        CHECK_EQ(apply_anomaly_ancient(&ctx, 1, abnormal_status_id(AbnormalStatusId::Poison),
                                       2, 0),
                 ApplyAnomalyResult::SUCCESS, "C4 古早施加应穿透弹控落地");
        CHECK(ctx.has_active_abnormal_status(1, abnormal_status_id(AbnormalStatusId::Poison)),
              "C4 守方应被古早施加挂上中毒");
        CHECK(!ctx.has_active_abnormal_status(0, abnormal_status_id(AbnormalStatusId::Poison)),
              "C4 古早施加不应反弹给施放方");
        std::cout << "C4 PASS 弹控真件：现代反弹/古早穿透\n";
    }

    //========== C5 抗性：现代被抗 / 古早无视 ==========
    {
        ScenarioBattle b({scenario::make_party(attacker())},
                         {scenario::make_party(defender())});
        b.start();
        BattleContext& ctx = b.ctx();
        const int paralysis = abnormal_status_id(AbnormalStatusId::Paralysis);
        std::pair<int, int> slots[kResistanceSlotCount] = {
            {paralysis, 100}, {0, 0}, {0, 0}};  // 100% 麻痹抗性（确定性）
        CHECK(b.pet(1).resistance.setControlResistance(slots), "C5 抗性槽应设置成功");
        CHECK_EQ(apply_anomaly(&ctx, 1, paralysis, 2, 0),
                 ApplyAnomalyResult::RESISTED_BY_RESISTANCE, "C5 现代施加应被抗性抵抗");
        CHECK_EQ(apply_anomaly_ancient(&ctx, 1, paralysis, 2, 0),
                 ApplyAnomalyResult::SUCCESS, "C5 古早施加应无视抗性落地");
        CHECK(ctx.has_active_abnormal_status(1, paralysis), "C5 麻痹应落地");
        std::cout << "C5 PASS 抗性：现代被抗/古早无视\n";
    }

    //========== C6 转化：现代被转化 / 古早无视 ==========
    {
        ScenarioBattle b({scenario::make_party(attacker())},
                         {scenario::make_party(defender())});
        b.start();
        BattleContext& ctx = b.ctx();
        const int paralysis = abnormal_status_id(AbnormalStatusId::Paralysis);
        const int burn = abnormal_status_id(AbnormalStatusId::Burn);
        ctx.anomaly_conversion[1][paralysis] = burn;  // 麻痹→烧伤（单跳）
        CHECK_EQ(apply_anomaly(&ctx, 1, paralysis, 2, 0),
                 ApplyAnomalyResult::CONVERTED, "C6 现代施加应被转化");
        CHECK(ctx.has_active_abnormal_status(1, burn), "C6 应落地烧伤");
        CHECK(!ctx.has_active_abnormal_status(1, paralysis), "C6 不应落地麻痹");
        ctx.set_abnormal_status_end_round(1, burn, 0);  // 清掉烧伤，隔离断言
        CHECK_EQ(apply_anomaly_ancient(&ctx, 1, paralysis, 2, 0),
                 ApplyAnomalyResult::SUCCESS, "C6 古早施加应无视转化落地");
        CHECK(ctx.has_active_abnormal_status(1, paralysis), "C6 古早应落地麻痹（非转化）");
        std::cout << "C6 PASS 转化：现代被转/古早无视\n";
    }

    //========== C7 Mark 0 老魂免兜底：古早也被挡 ==========
    {
        ScenarioBattle b({scenario::make_party(attacker())},
                         {scenario::make_party(defender())});
        b.start();
        BattleContext& ctx = b.ctx();
        b.pet(1).marks.push_back(Mark(0, "老魂免兜底"));
        const int paralysis = abnormal_status_id(AbnormalStatusId::Paralysis);
        CHECK_EQ(apply_anomaly_ancient(&ctx, 1, paralysis, 2, 0),
                 ApplyAnomalyResult::TARGET_IMMUNE, "C7 Mark 0 应挡古早施加");
        CHECK_EQ(apply_anomaly(&ctx, 1, paralysis, 2, 0),
                 ApplyAnomalyResult::TARGET_IMMUNE, "C7 Mark 0 应挡现代施加");
        std::cout << "C7 PASS Mark 0 老魂免挡双通道\n";
    }

    //========== S8 effect 48 真件（星光护体）：Ancient 层授予 + 挡古早 ==========
    {
        ElfPet def = defender();
        // 守方换成带星光护体的技能组（属性技能，无 flake）
        def = scenario::make_pet(1, "守方", {kXingGuang, kShunLei, kShunLei, kShunLei, kShunLei},
                                 100, 100, 400);
        ScenarioBattle b({scenario::make_party(attacker())}, {scenario::make_party(def)});
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);  // 星光护体
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);  // 瞬雷天闪（5% 麻痹不断言）
        b.drive();
        // 找到 effect 48 授予的 ANOMALY 票并验证层级
        bool found_ancient_ticket = false;
        for (const RuleTicket& t : ctx.rule_center_.entries()) {
            if (t.category == RuleCategory::IMMUNE && t.subtype == static_cast<int>(ImmunityType::ANOMALY)
                && t.source_effect_id == 48 && t.target == 1) {
                found_ancient_ticket = (t.tier == static_cast<int>(ImmunityTier::Ancient));
            }
        }
        CHECK(found_ancient_ticket, "S8 星光护体应授予 effect 48 的 Ancient 层 ANOMALY 票");
        CHECK_EQ(apply_anomaly_ancient(&ctx, 1, abnormal_status_id(AbnormalStatusId::Poison),
                                       2, 0),
                 ApplyAnomalyResult::TARGET_IMMUNE, "S8 古早施加应被 48 纯免挡下");
        CHECK_EQ(apply_anomaly(&ctx, 1, abnormal_status_id(AbnormalStatusId::Poison), 2, 0),
                 ApplyAnomalyResult::TARGET_IMMUNE, "S8 现代施加也应被 48 挡下");
        std::cout << "S8 PASS effect 48 真件：Ancient 层纯免\n";
    }

    //========== S9 effect 2189 真件（月下伏）：PP 高时 100% 令对手焚烬（现代通道）==========
    {
        ElfPet atk = scenario::make_pet(0, "攻方", {kYueXiaFu, kYueXiaFu, kYueXiaFu, kYueXiaFu, kYueXiaFu},
                                        200, 100, 400);
        atk.skills[0].accuracy = 100;  // 月下伏 97 命中 → 测试拉满去 flake
        ScenarioBattle b({scenario::make_party(atk)}, {scenario::make_party(defender())});
        b.start();
        BattleContext& ctx = b.ctx();
        b.pet(1).skills[0].pp = 1;  // 守方 PP 总和被压低 → 攻方 PP 总和高于对手
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);  // 月下伏（100% 焚烬）
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const int incinerate = abnormal_status_id(AbnormalStatusId::Incinerate);  // 22 = 焚烬
        CHECK(ctx.has_active_abnormal_status(1, incinerate),
              "S9 月下伏应令守方焚烬（图鉴口径：{1}=22 → battle_effects 焚烬）");
        std::cout << "S9 PASS effect 2189 真件：PP 高时 100% 焚烬\n";
    }

    std::cout << "\n场景 057 全部通过：异常双通道 + 免疫层级拦截矩阵 ✓" << std::endl;
    return 0;
}
