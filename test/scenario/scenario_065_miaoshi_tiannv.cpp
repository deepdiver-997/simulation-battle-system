#include <utils/sbs_rand.h>
// 场景 065：妙时天女 4872 ——「百天浮世络」+ 复制对方通用特性（effect_icon 2478）。
//
// 口径（图鉴全文 reference/scrape_4399/out/hunyin.jsonl + 专栏补充）：
//   「登场时触发 3 回合内百天浮世络，效果消失的当回合结束后，若期间自身受到的伤害之和
//     小于自身最大体力的 54% → 怀生菩怜态；不小于 54% → 拒赦怒威态」
//   百天浮世络：① 免疫并反弹受到的异常状态；② 免疫受到的能力下降状态；
//     ③ **自身拥有对方的通用特性**——以此法获得的特性**触发概率提升至 99%**、
//        **取消攻击技能类型要求**；专栏：默认 **5 星**、不被对手检测、同种只触发 1 次、
//        **无法获得对方已消逝精灵的特性**（消逝未接入 → 当前全量扫描）。
//
// 验证点：
//   T1 登场授予：ANOMALY（Modern 层）+ STAT_DROP 票 + REFLECT 条目都在
//   T2 ★ 复制对方 6 只的通用特性：条数正确、且被覆写为 5 星 / 99% / 取消物特区分
//   T3 ★ 复制来的"毒"标了 modern_anomaly_channel（走现代施加，不再是主动毒）
//   T4 同种特性去重（对方多只带同一特性 → 只复制 1 份）
//   T5 未知/无特性不被复制（kind == None 跳过）
//   T6 免弱生效（STAT_DROP 票挡住对手的弱化）
//   T7 异常免疫 + 反弹（Modern 层：现代施加被弹回；主动毒穿透——层级口径不变）
//   T8 两态判定：期间受伤 < 54% → 怀生菩怜态；≥ 54% → 拒赦怒威态（用 window 倒数 + 累计伤害）
#include <cassert>
#include <cstdlib>
#include <iostream>

#include <effects/rule_center.h>
#include <effects/trait_state.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

constexpr int kPaiDa = 10004;      // 物攻 40
constexpr int kMiaoShi = 2478;  // 魂印注册键 = effect_id（2026-09-27 对齐；4872=monster id）
constexpr int kAtk = 0;

using scenario::ScenarioBattle;

ScenarioBattle make_battle(const ElfPet& p0, const ElfPet& p1) {
    sbs_srand(20260916);
    return ScenarioBattle(scenario::make_party(p0), scenario::make_party(p1));
}

ElfPet miaoshi_pet() {
    SoulMark sm = SoulMark(kMiaoShi, "妙时天女", "百天浮世络");
    return scenario::make_pet(0, "妙时天女", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                              200, 200, 4000, std::move(sm));
}
// 对手队伍：6 只各带一个通用特性（槽位 0..5）
ElfPet enemy_pet(int slot, CommonTrait trait) {
    return scenario::make_pet(1, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                              100, 100, 4000, {}, std::move(trait));
}

// 统计 owner 的复制条目里某个 kind 的份数
int count_copies(BattleContext& ctx, int owner, TraitKind kind) {
    int n = 0;
    for (const TraitOverlay& e : ctx.trait_state_[owner].copies) {
        if (e.trait.kind == kind) {
            ++n;
        }
    }
    return n;
}
const EffectiveTrait* find_copy(BattleContext& ctx, int owner, TraitKind kind) {
    for (const TraitOverlay& e : ctx.trait_state_[owner].copies) {
        if (e.trait.kind == kind) {
            return &e.trait;
        }
    }
    return nullptr;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== T1~T5 登场授予 + 复制 ==========
    {
        // 对手队伍：0=瞬杀、1=坚硬、2=静电（毒）、3=精神、4=无特性、5=瞬杀（重复，验去重）
        std::array<ElfPet, 6> enemy{
            enemy_pet(0, scenario::make_trait("瞬杀", 3, {13, 0})),
            enemy_pet(1, scenario::make_trait("坚硬", 3, {7, 0})),
            enemy_pet(2, scenario::make_trait("静电", 5, {5, 0})),
            enemy_pet(3, scenario::make_trait("精神", 5, {2, 5})),
            enemy_pet(4, CommonTrait{}),                                   // 无特性
            enemy_pet(5, scenario::make_trait("瞬杀", 5, {30, 0})),        // 与槽 0 同 kind
        };
        ScenarioBattle b(scenario::make_party(miaoshi_pet()), enemy);
        b.start();
        BattleContext& ctx = b.ctx();
        const int me = ctx.on_stage[0] >= 0 ? 0 : 0;

        // ① 免疫并反弹异常 + 免弱
        bool anomaly_ticket = false, statdrop_ticket = false;
        for (const RuleTicket& t : ctx.rule_center_.entries()) {
            if (t.category != RuleCategory::IMMUNE || t.target != me) continue;
            if (t.subtype == static_cast<int>(ImmunityType::ANOMALY)) anomaly_ticket = true;
            if (t.subtype == static_cast<int>(ImmunityType::STAT_DROP)) statdrop_ticket = true;
        }
        CHECK(anomaly_ticket, "T1 应有 ANOMALY 免疫票（百天浮世络①）");
        CHECK(statdrop_ticket, "T1 应有 STAT_DROP 免疫票（百天浮世络②）");
        CHECK(ctx.rule_center_.has_reflect(me), "T1 应有 REFLECT 条目（免疫并反弹）");

        // ② 复制：瞬杀/坚硬/静电/精神 各 1 份（重复的瞬杀去重；无特性跳过）
        CHECK_EQ(count_copies(ctx, me, TraitKind::InstantKill), 1, "T2 瞬杀应只复制 1 份（去重）");
        CHECK_EQ(count_copies(ctx, me, TraitKind::Hardness), 1, "T2 坚硬应复制 1 份");
        CHECK_EQ(count_copies(ctx, me, TraitKind::ActivePoisonPhysical), 1, "T2 静电应复制 1 份");
        CHECK_EQ(count_copies(ctx, me, TraitKind::Spirit), 1, "T2 精神应复制 1 份");
        CHECK_EQ(count_copies(ctx, me, TraitKind::None), 0, "T5 无特性不应产生条目");

        // ③ 覆写：5 星 / 99%（990‰）/ 取消物特区分
        const EffectiveTrait* ik = find_copy(ctx, me, TraitKind::InstantKill);
        CHECK(ik != nullptr, "T2 前提：找到复制的瞬杀");
        CHECK_EQ(ik->star_level, 5, "T2 复制的特性应被抬到 5 星");
        CHECK_EQ(trait_proc_permille(*ik), 990, "T2 触发概率应提升至 99%（990‰）");
        CHECK(ik->any_attack_type, "T2 应取消攻击技能类型要求");
        CHECK(ik->copied, "T2 应标记为复制条目");

        // ④ 毒走现代通道
        const EffectiveTrait* poison = find_copy(ctx, me, TraitKind::ActivePoisonPhysical);
        CHECK(poison != nullptr && poison->modern_anomaly_channel,
              "T3 复制来的毒应标记 modern_anomaly_channel（走现代施加）");
        const EffectiveTrait* hardness = find_copy(ctx, me, TraitKind::Hardness);
        CHECK(hardness != nullptr && !hardness->modern_anomaly_channel,
              "T3 非毒类不应带该标记");
        std::cout << "T1~T5 PASS 登场授予 + 复制（5星/99%/取消类别/毒走现代通道/去重）\n";
    }

    //========== T6 免弱：STAT_DROP 票挡住对手弱化 ==========
    {
        ScenarioBattle b({scenario::make_party(miaoshi_pet())},
                         {scenario::make_party(enemy_pet(0, CommonTrait{}))});
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK_EQ(stat_drop(&ctx, 0, kAtk, 2), StatDropResult::IMMUNE,
                 "T6 百天浮世络②应免疫能力下降");
        CHECK_EQ(ctx.ability_levels[0][kAtk], 0, "T6 等级不应变化");
        std::cout << "T6 PASS 免疫能力下降\n";
    }

    //========== T7 异常免疫 + 反弹（层级口径）==========
    {
        ScenarioBattle b({scenario::make_party(miaoshi_pet())},
                         {scenario::make_party(enemy_pet(0, CommonTrait{}))});
        b.start();
        BattleContext& ctx = b.ctx();
        const int paralysis = abnormal_status_id(AbnormalStatusId::Paralysis);
        // 现代施加（施放方=1）→ 被弹回施放方
        CHECK_EQ(apply_anomaly(&ctx, 0, paralysis, 2, 1), ApplyAnomalyResult::REFLECTED,
                 "T7 现代施加应被弹控弹回");
        CHECK(ctx.has_active_abnormal_status(1, paralysis), "T7 异常应落在施放方身上");
        CHECK(!ctx.has_active_abnormal_status(0, paralysis), "T7 自己不应中招");
        // 主动毒（古早通道）→ 弹控对它不可见（层级口径不变）
        CHECK_EQ(apply_anomaly_ancient(&ctx, 0, paralysis, 2, 1), ApplyAnomalyResult::SUCCESS,
                 "T7 主动毒应穿透弹控落地");
        CHECK(ctx.has_active_abnormal_status(0, paralysis), "T7 主动毒应命中自己");
        std::cout << "T7 PASS 免疫并反弹（现代被弹 / 主动毒穿透）\n";
    }

    //========== T8 两态判定 ==========
    {
        // 读两态需先找到她的 pet 槽（魂印 4872）——两态写在 soulmark_storage。
        auto mode_of = [](BattleContext& ctx, int owner) {
            for (int slot = 0; slot < 6; ++slot) {
                const ElfPet& pet = ctx.seerRobot[owner].elfPets[slot];
                if (pet.soulMark.id != kMiaoShi) {
                    continue;
                }
                const int key = kMiaoShi * 100 + 7;
                auto it = pet.soulmark_storage.find(key);
                if (it == pet.soulmark_storage.end()) {
                    return -1;
                }
                return std::any_cast<int>(it->second);   // soulmark_storage 是 int→any
            }
            return -1;
        };

        // ① 低伤：每回合 100 点，3 回合共 300 < 4000×54% = 2160 → 怀生菩怜态(1)
        ScenarioBattle low({scenario::make_party(miaoshi_pet())},
                           {scenario::make_party(enemy_pet(0, CommonTrait{}))});
        low.start();
        for (int round = 0; round < 3; ++round) {
            (void)fixed_damage(&low.ctx(), 0, 100);
            low.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            low.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            low.drive();
        }
        CHECK_EQ(mode_of(low.ctx(), 0), 1,
                 "T8 受伤之和 < 54% 应进入怀生菩怜态（1）");

        // ② 高伤：一次性打 3000 点（≥ 2160）→ 拒赦怒威态(2)
        ScenarioBattle high({scenario::make_party(miaoshi_pet())},
                            {scenario::make_party(enemy_pet(0, CommonTrait{}))});
        high.start();
        for (int round = 0; round < 3; ++round) {
            if (round == 0) {
                (void)fixed_damage(&high.ctx(), 0, 3000);
            }
            high.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            high.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            high.drive();
        }
        CHECK_EQ(mode_of(high.ctx(), 0), 2,
                 "T8 受伤之和 ≥ 54% 应进入拒赦怒威态（2）");
        std::cout << "T8 PASS 两态判定（<54% → 怀生菩怜 / ≥54% → 拒赦怒威）\n";
    }

    //========== T9 ★ 拒赦怒威态：先制+1 / 必暴 / 伤害+100% / 混沌系结算 ==========
    {
        ScenarioBattle b({scenario::make_party(miaoshi_pet())},
                         {scenario::make_party(enemy_pet(0, CommonTrait{}))});
        b.start();
        BattleContext& ctx = b.ctx();
        // 快速进拒赦怒威态：直接写两态标记 + 模拟浮世络已结束
        {
            const int key = kMiaoShi * 100 + 7;
            for (int slot = 0; slot < 6; ++slot) {
                if (ctx.seerRobot[0].elfPets[slot].soulMark.id == kMiaoShi) {
                    ctx.seerRobot[0].elfPets[slot].soulmark_storage[key] = 2;
                }
            }
        }
        // 推进一回合，让 ROUND_START / MOVE_RIGHT 节点按"态"刷新 ws
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(ctx.ws.must_crit[0], "T9 拒赦怒威：必定致命一击");
        CHECK(ctx.ws.skill_element_grant_valid[0]
              && ctx.ws.skill_element_grant[0][0] == 222,
              "T9 拒赦怒威：以混沌系(222)结算");
        CHECK(ctx.ws.preemptive_level[0] >= 1, "T9 拒赦怒威：所有技能先制+1");
        // 伤害提升 100%：AMP 加法槽——打一回合，对比无条件对照 ×2
        const int hp_before = b.pet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const int dmg_mode2 = hp_before - b.pet(1).hp;
        ScenarioBattle ctrl = make_battle(miaoshi_pet(), enemy_pet(0, CommonTrait{}));
        ctrl.start();
        // 对照：无两态标记（mode=0）打同技能
        const int hp_ctrl = ctrl.pet(1).hp;
        ctrl.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        ctrl.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        ctrl.drive();
        const int dmg_ctrl = hp_ctrl - ctrl.pet(1).hp;
        CHECK(dmg_ctrl > 0, "T9 前提：对照组有伤害");
        CHECK_EQ(dmg_mode2, dmg_ctrl * 2, "T9 拒赦怒威：伤害 ×2（提升100%，AMP 加法）");
        std::cout << "T9 PASS 拒赦怒威态（先制+1/必暴/混沌系/伤害×2）\n";
    }

    //========== T10 ★ 怀生菩怜态：光系结算 + 用技能后随机弱化（miss 路径的场下回血）==========
    {
        ScenarioBattle b({scenario::make_party(miaoshi_pet())},
                         {scenario::make_party(enemy_pet(0, CommonTrait{}))});
        b.start();
        BattleContext& ctx = b.ctx();
        {
            const int key = kMiaoShi * 100 + 7;
            for (int slot = 0; slot < 6; ++slot) {
                if (ctx.seerRobot[0].elfPets[slot].soulMark.id == kMiaoShi) {
                    ctx.seerRobot[0].elfPets[slot].soulmark_storage[key] = 1;
                }
            }
        }
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(ctx.ws.skill_element_grant_valid[0]
              && ctx.ws.skill_element_grant[0][0] == 12,
              "T10 怀生菩怜：以光系(12)结算");
        // 用技能后随机弱化：对手应中一种弱化类异常（100% 施加、2 回合窗内随机 9 选 1）
        bool any_debuff = false;
        static const int kDebuffs[] = {1, 2, 3, 10, 11, 20, 28, 29, 30};
        for (int id : kDebuffs) {
            if (ctx.has_active_abnormal_status(1, id)) {
                any_debuff = true;
            }
        }
        CHECK(any_debuff, "T10 怀生菩怜：用技能后应随机附加一种弱化类异常");
        std::cout << "T10 PASS 怀生菩怜态（光系结算 + 随机弱化类异常）\n";
    }

    std::cout << "\n场景 065 全部通过：妙时天女（百天浮世络 + 复制 + 两态）✓" << std::endl;
    return 0;
}
