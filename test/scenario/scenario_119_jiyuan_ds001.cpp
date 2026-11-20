// 场景 119：极渊DS-001 4797 —— 算力 / 算力乱流（真实概率提升）/ 算力共振 / 回合末五条 20%。
//
// 口径：数据库 effect_icon 1987 + 《机制解析—极渊DS-001》（详见开工文档 §一）。
// ★ 本场景的核心是"真实提高概率"：乱流只把特性掷点顶到必发（trait_proc_roll 的
//   proc_forced），主动毒**保持 Ancient 通道**（穿透 Modern 免疫/弹控）——不是妙时式
//   "改走现代施加"。以及虚无豁免（必修6 ③：必发不得把闪避推满）。
//
// 验证点：
//   T1 乱流必发 + 通道保持：0% 概率的静电（麻痹）在乱流期 100% 附上，且穿透对手的
//      Modern 免疫+反弹（Ancient 通道证据：没有被弹回自己）
//   T2 乱流窗口边界：R1/R2 必发 → R3（算力 100-50-50=0）不再必发，0% 特性回归不触发
//   T3 虚无豁免（unit）：proc_forced 对 VoidDodge 无效、对毒特性照常必发
//   T4 回合末五条（乱流期全发不掷点）：解除自身异常 / 自身 PP+1 / 对手 PP-1 /
//      全属性+1（含命中共 6 项）/ 回 200 + 对手 200 固定伤害
//   T5 算力共振：对方全队含同特性（瞬杀族）→ 限制解除；对方没有 / 非名单特性 → 保留
//   T6 换宠不泄漏：切走后必发标志不沾染后上场精灵；再上场重发 100 算力、乱流待下回合
#include <cstdlib>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <effects/trait_state.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

// 魂印注册键 = effect_icon.effect_id = 2414（2026-09-27 对齐装配层；4797 是 monster id）。
// 场景内的算力/限制/乱流存储键由本常量派生，与插件 kJiyuanMarkId*100+n 同源。
constexpr int kJiyuan = 2414;
constexpr int kHit = 10004;     // 拍打：物攻 40
constexpr int kBuff = 20265;    // 锋利：属性技能（自身攻击+1，无伤害）——给对手用，隔离伤害
constexpr int kSuanliKey = kJiyuan * 100 + 1;
constexpr int kRestrictedKey = kJiyuan * 100 + 2;
constexpr int kTurbulenceKey = kJiyuan * 100 + 3;

using scenario::ScenarioBattle;

ElfPet jiyuan_pet(CommonTrait trait) {
    return scenario::make_pet(0, "极渊DS-001", {kHit, kHit, kHit, kHit, kHit},
                              /*speed=*/200, /*atk=*/100, /*hp=*/3000,
                              SoulMark(kJiyuan, "极渊DS-001", "算力"), std::move(trait));
}

ElfPet plain_pet(int side, int speed = 100) {
    return scenario::make_pet(side, "对手", {kHit, kHit, kHit, kHit, kHit},
                              speed, /*atk=*/100, /*hp=*/2000);
}

ElfPet buffing_pet() {
    // 属性技能队（无伤害）：给"五条结算"做精确 HP 断言用
    return scenario::make_pet(1, "对手", {kBuff, kBuff, kBuff, kBuff, kBuff},
                              /*speed=*/100, /*atk=*/100, /*hp=*/2000);
}

ElfPet buffing_self_pet() {
    // DS-001 的属性技变体：T4 用——双方都无红伤，⑤的 HP 变化只含"回 200 / 固定 200"
    return scenario::make_pet(0, "极渊DS-001", {kBuff, kBuff, kBuff, kBuff, kBuff},
                              /*speed=*/200, /*atk=*/100, /*hp=*/3000,
                              SoulMark(kJiyuan, "极渊DS-001", "算力"));
}

int suanli_of(const ElfPet& pet) {
    auto it = pet.on_stage_storage.find(kSuanliKey);
    return it == pet.on_stage_storage.end() ? -1 : std::any_cast<int>(it->second);
}
bool restricted_of(const ElfPet& pet) {
    auto it = pet.on_stage_storage.find(kRestrictedKey);
    return it != pet.on_stage_storage.end() && std::any_cast<bool>(it->second);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== T3 虚无豁免（unit，无需开局）==========
    {
        EffectiveTrait void_dodge;
        void_dodge.kind = TraitKind::VoidDodge;
        void_dodge.proc_forced = true;
        void_dodge.args[0] = 0;   // 0‰：不吃必发 → 按自身概率（0）恒不触发
        CHECK(trait_proc_roll(void_dodge) == false,
              "T3 虚无不吃必发（必修6 ③）：proc_forced 不得把闪避推成 100%");

        EffectiveTrait poison;
        poison.kind = TraitKind::ActivePoisonPhysical;
        poison.proc_forced = true;
        poison.args[0] = 0;       // 0% 概率 + proc_forced → 必发
        poison.args[1] = static_cast<int>(AbnormalStatusId::Paralysis);
        CHECK(trait_proc_roll(poison) == true, "T3 毒特性吃必发（乱流的正当消费者）");

        EffectiveTrait poison_plain = poison;
        poison_plain.proc_forced = false;
        CHECK(trait_proc_roll(poison_plain) == false, "T3 对照：无必发时 0% 就是 0%");
        std::cout << "  [T3] 虚无豁免 / 毒必发 对照 通过\n";
    }

    //========== T1+T2 乱流必发 + 通道保持 + 窗口边界（同一场：0% 静电）==========
    {
        // 静电 args[0]=0（0% 概率，天然不触发）、args[1]=0（麻痹）——
        // 乱流期必发 = 麻痹照附；R3 乱流结束 = 回归 0% 不附。全程零概率断言。
        std::array<ElfPet, 6> foe{plain_pet(1), plain_pet(1), plain_pet(1),
                                  plain_pet(1), plain_pet(1), plain_pet(1)};
        ScenarioBattle b(scenario::make_party(jiyuan_pet(scenario::make_trait("静电", 5, {0, 0}))), foe);
        b.start();
        BattleContext& ctx = b.ctx();
        ElfPet& me = ctx.seerRobot[0].elfPets[ctx.on_stage[0]];

        // 战前：乱流未激活、算力 100、算力限制在场（对手无瞬杀族 → 不共振）
        CHECK_EQ(suanli_of(me), 100, "T2 登场送 100 算力");
        CHECK(restricted_of(me), "T2 登场获得算力限制（对手无同特性，不共振）");
        CHECK(ctx.trait_state_[0].own.proc_forced == false,
              "T2 战前不乱流：特性掷点未被顶满");

        // 对手挂 Modern 免疫 + 反弹：Ancient 通道的证据窗口
        ctx.grant_immunity(1, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                           /*duration_rounds=*/5, /*source_id=*/999, /*soul_immunity=*/false,
                           EffectScope::ON_STAGE, /*counts=*/0, /*source_effect_id=*/999,
                           /*tier=*/ImmunityTier::Modern);
        ctx.rule_center_.grant_reflect(1, /*source_slot=*/-1, /*source_effect_id=*/999,
                                       /*target=*/1, /*rounds=*/5);

        // ── R1：乱流期 ──
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(suanli_of(me), 50, "T2 R1 付出 50 算力");
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Paralysis)),
              "T1 乱流必发：0% 静电在 R1 照附麻痹");
        CHECK(!ctx.has_active_abnormal_status(0, static_cast<int>(AbnormalStatusId::Paralysis)),
              "T1 通道保持：主动毒穿透 Modern 弹控，没有被弹回自己（妙时式改道会中弹）");
        (void)dispel_active_anomalies(&ctx, 1);   // 清场，为 R2 断言腾地方

        // ── R2：乱流期最后一回合 ──
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(suanli_of(me), 0, "T2 R2 付完：算力归零");
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Paralysis)),
              "T1 乱流必发：R2 照附（重新施加/延长皆算 active）");
        (void)dispel_active_anomalies(&ctx, 1);

        // ── R3：算力不足，乱流结束 ──
        // ⚠️ step 夹在两次 submit 之间（场景 106 踩过的坑：两边 submit 后整回合已跑完，
        //     之后再 step_past 会在 CHOOSE 输入点上死循环）。
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        ctx.debug_step_mode = true;
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.step_past(State::BATTLE_ROUND_START);
        ctx.debug_step_mode = false;
        CHECK(ctx.trait_state_[0].own.proc_forced == false,
              "T2 R3 算力不足：必发标志已撤（0% 特性回归 0%）");
        b.drive();
        CHECK(!ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Paralysis)),
              "T2 R3 不再必发：0% 静电不触发");
        std::cout << "  [T1/T2] 乱流必发（0%→100%）/ Ancient 通道穿透 / R3 窗口关闭 通过\n";
    }

    //========== T4 回合末五条（乱流期全发，精确断言）==========
    {
        std::array<ElfPet, 6> foe{buffing_pet(), plain_pet(1), plain_pet(1),
                                  plain_pet(1), plain_pet(1), plain_pet(1)};
        // DS-001 无毒特性（None）：五条断言不被毒干扰；用属性技隔离红伤
        ScenarioBattle b(scenario::make_party(buffing_self_pet()), foe);
        b.start();
        BattleContext& ctx = b.ctx();
        ElfPet& me = ctx.seerRobot[0].elfPets[ctx.on_stage[0]];
        ElfPet& foe0 = ctx.seerRobot[1].elfPets[ctx.on_stage[1]];

        // 预置观测点：自身带失明（给①解；**不用中毒**——扣血档 1/8 会混进⑤的 HP 断言）、
        // PP 压低（②③）、体力拉开（⑤回 200 / 对手 -200）。
        // 双方都用属性技（锋利）：本回合零红伤，⑤的 HP 变化只含回血/固定伤。
        (void)apply_anomaly(&ctx, 0, static_cast<int>(AbnormalStatusId::Blind),
                            /*duration=*/3, /*actor=*/1);
        for (Skills& s : me.skills) {
            s.pp = 2;
        }
        for (Skills& s : foe0.skills) {
            s.pp = 3;
        }
        me.hp = 1000;
        foe0.hp = 2000;

        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();

        CHECK(!ctx.has_active_abnormal_status(0, static_cast<int>(AbnormalStatusId::Blind)),
              "T4-① 乱流期必解：自身失明被解除");
        // ⚠️ 断言用**未选用槽**（批2星赐同款坑）：选用槽在 AFTER_ACTION 先被
        //    consume_selected_skill_pp 扣 1，ROUND_END 再 ±1 → 净变化 0。
        CHECK_EQ(me.skills[1].pp, 3, "T4-② 自身未选用槽 PP+1（2→3）");
        CHECK_EQ(foe0.skills[1].pp, 2, "T4-③ 对手未选用槽 PP-1（3→2）");
        bool all_six_up = true;
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            all_six_up = all_six_up && ctx.ws.view_levels[0][i] == 1;
        }
        CHECK(all_six_up, "T4-④ 全属性+1：六项等级（含命中）全为 +1");
        CHECK_EQ(me.hp, 1200, "T4-⑤ 自身回 200（1000→1200；对手属性技无伤害）");
        CHECK_EQ(foe0.hp, 1800, "T4-⑤ 对手吃 200 固定伤害（2000→1800；属性技不掉血隔离红伤）");
        CHECK_EQ(suanli_of(me), 50, "T4 乱流期五条全发 → 不触发 +20 补偿；R1 后算力 50");
        std::cout << "  [T4] 五条 20% 乱流期全发（解异常/PP±1/全属性+1/回血+固定伤）通过\n";
    }

    //========== T5 算力共振 ==========
    {
        // A：对方 0 号槽带瞬杀（同特性 ∈ 名单）→ 共振解除算力限制
        {
            std::array<ElfPet, 6> foe{plain_pet(1), plain_pet(1), plain_pet(1),
                                      plain_pet(1), plain_pet(1), plain_pet(1)};
            foe[0] = scenario::make_pet(1, "瞬杀对手", {kHit, kHit, kHit, kHit, kHit},
                                        100, 100, 2000,
                                        {}, scenario::make_trait("瞬杀", 5, {30, 0}));
            ScenarioBattle b(scenario::make_party(jiyuan_pet(scenario::make_trait("瞬杀", 3, {13, 0}))), foe);
            b.start();
            BattleContext& ctx = b.ctx();
            const ElfPet& me = ctx.seerRobot[0].elfPets[ctx.on_stage[0]];
            CHECK(restricted_of(me) == false,
                  "T5-A 共振：对方全队含同特性（瞬杀）→ 算力限制解除");
        }
        // B：对方没有瞬杀 → 限制保留
        {
            std::array<ElfPet, 6> foe{plain_pet(1), plain_pet(1), plain_pet(1),
                                      plain_pet(1), plain_pet(1), plain_pet(1)};
            ScenarioBattle b(scenario::make_party(jiyuan_pet(scenario::make_trait("瞬杀", 3, {13, 0}))), foe);
            b.start();
            BattleContext& ctx = b.ctx();
            const ElfPet& me = ctx.seerRobot[0].elfPets[ctx.on_stage[0]];
            CHECK(restricted_of(me) == true, "T5-B 无同特性 → 算力限制保留");
        }
        // C：同为坚硬（不在瞬杀/顽强/回神名单）→ 不共振
        {
            std::array<ElfPet, 6> foe{plain_pet(1), plain_pet(1), plain_pet(1),
                                      plain_pet(1), plain_pet(1), plain_pet(1)};
            foe[0] = scenario::make_pet(1, "坚硬对手", {kHit, kHit, kHit, kHit, kHit},
                                        100, 100, 2000,
                                        {}, scenario::make_trait("坚硬", 3, {7, 0}));
            ScenarioBattle b(scenario::make_party(jiyuan_pet(scenario::make_trait("坚硬", 3, {7, 0}))), foe);
            b.start();
            BattleContext& ctx = b.ctx();
            const ElfPet& me = ctx.seerRobot[0].elfPets[ctx.on_stage[0]];
            CHECK(restricted_of(me) == true, "T5-C 同为坚硬（名单外）→ 不共振");
        }
        std::cout << "  [T5] 算力共振（名单内同特性解除限制；名单外/无同特性保留）通过\n";
    }

    //========== T6 换宠不泄漏 + 再上场重发 ==========
    {
        std::array<ElfPet, 6> mine{
            jiyuan_pet(scenario::make_trait("静电", 5, {0, 0})),   // 槽 0：DS-001
            plain_pet(0), plain_pet(0), plain_pet(1), plain_pet(1), plain_pet(1)};
        // 注：plain_pet 的 side 参数只影响名字；槽 3..5 不会上场，无碍
        std::array<ElfPet, 6> foe{plain_pet(1), plain_pet(1), plain_pet(1),
                                  plain_pet(1), plain_pet(1), plain_pet(1)};
        ScenarioBattle b(mine, foe);
        b.start();
        BattleContext& ctx = b.ctx();

        // R1：DS-001 乱流激活（必发置位）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(ctx.trait_state_[0].own.proc_forced == true, "T6 前置：R1 乱流必发已置位");

        // R2 选择期：DS-001 切走（槽 1 补位）
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.on_stage[0], 1, "T6 槽 1 已上场");
        CHECK(ctx.trait_state_[0].own.proc_forced == false,
              "T6 必发不泄漏：后上场精灵的特效性槽未被置位");
        CHECK(ctx.seerRobot[0].elfPets[0].on_stage_storage.empty(),
              "T6 DS-001 下场：on_stage_storage（算力/限制/乱流）即清");

        // R3 选择期：DS-001 再上场
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.on_stage[0], 0, "T6 DS-001 已再上场");
        const ElfPet& me = ctx.seerRobot[0].elfPets[0];
        // 读数时机：drive() 已跑完 R3 + R4 的 ROUND_START——重发的 100 在 R4 开始被扣 50。
        // 50 = 100 − 50 反证重发发生了（没重发会是 0）。五条 +20 补偿分支是概率路径，
        // 本序列未随机到（开工文档 §四.12：概率分支不赌断言）。
        CHECK_EQ(suanli_of(me), 50, "T6 再上场重发 100 → R4 开始付 50（拍板 §三.5：重置不累加）");
        CHECK(ctx.trait_state_[0].own.proc_forced == true,
              "T6 R4 乱流恢复：必发重新置位");
        std::cout << "  [T6] 换宠不泄漏 / 下场清存储 / 再上场重发 通过\n";
    }

    //========== T7 共振扫描含场下（解析帖："对方所有精灵中只要有一个"）==========
    {
        std::array<ElfPet, 6> foe{plain_pet(1), plain_pet(1), plain_pet(1),
                                  plain_pet(1), plain_pet(1), plain_pet(1)};
        // 瞬杀放在**场下** 3 号槽：共振按"对方全队"判定，不看在场
        foe[3] = scenario::make_pet(1, "板凳瞬杀", {kHit, kHit, kHit, kHit, kHit},
                                    100, 100, 2000,
                                    {}, scenario::make_trait("瞬杀", 5, {30, 0}));
        ScenarioBattle b(scenario::make_party(jiyuan_pet(scenario::make_trait("瞬杀", 3, {13, 0}))), foe);
        b.start();
        BattleContext& ctx = b.ctx();
        const ElfPet& me = ctx.seerRobot[0].elfPets[ctx.on_stage[0]];
        CHECK(restricted_of(me) == false, "T7 对手场下（3号槽）有同特性 → 共振照常触发");
        std::cout << "  [T7] 共振扫描含场下精灵 通过\n";
    }

    //========== T8 瞬杀秒杀 × 扎克斯抑制（与乱流必发的组合）==========
    {
        // 瞬杀 args[0]=1000‰：trait_proc_roll 100% 档必发不耗 rand——确定性触发秒杀钩。
        // 对照组：秒杀落地（对手归零）；抑制组：suppress_common_trait 后**照常掷点、
        // 照常调原语**，抑制表现为原语短路（不归零）——乱流的必发不绕过抑制。
        auto instant_pet = []() {
            return jiyuan_pet(scenario::make_trait("瞬杀", 5, {1000, 0}));
        };
        // ── 对照：无抑制 → 归零 ──
        {
            std::array<ElfPet, 6> foe{plain_pet(1), plain_pet(1), plain_pet(1),
                                      plain_pet(1), plain_pet(1), plain_pet(1)};
            ScenarioBattle b(scenario::make_party(instant_pet()), foe);
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK_EQ(b.pet(1).hp, 0, "T8-对照 1000‰ 瞬杀命中 → 对手体力归零");
        }
        // ── 抑制组：扎克斯式抑制 001 的瞬杀 → 原语短路，不归零 ──
        // ⚠️ 宿主（湮灭之主·扎克斯）未落地，直接调内核 suppress_common_trait 预演联动；
        //    被抑制 ≠ 不触发：掷点照常（乱流必发也照常），短路发生在 force_hp_to_zero 内。
        //    对手队伍里放一只瞬杀 → 同时验证"共振按『拥有』判定"（抑制不影响匹配）。
        {
            std::array<ElfPet, 6> foe{plain_pet(1), plain_pet(1), plain_pet(1),
                                      plain_pet(1), plain_pet(1), plain_pet(1)};
            foe[3] = scenario::make_pet(1, "板凳瞬杀", {kHit, kHit, kHit, kHit, kHit},
                                        100, 100, 2000,
                                        {}, scenario::make_trait("瞬杀", 5, {30, 0}));
            ScenarioBattle b(scenario::make_party(instant_pet()), foe);
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.suppress_common_trait(/*target_side=*/0, TraitKind::InstantKill,
                                      /*source_owner=*/1);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(b.pet(1).hp > 0, "T8-抑制 瞬杀被扎克斯挡下：原语短路不归零（必发不绕过抑制）");
            // 口径待拍板（§三.17，当前按"拥有"实现并锁现状）：自身瞬杀被抑制仍参与共振匹配
            const ElfPet& me = ctx.seerRobot[0].elfPets[ctx.on_stage[0]];
            CHECK(restricted_of(me) == false,
                  "T8 共振按『拥有』判定：自身瞬杀被抑制仍匹配（解析帖『均拥有』字面，§三.17）");
        }
        std::cout << "  [T8] 瞬杀秒杀 / 扎克斯抑制短路 / 共振『拥有』口径 通过\n";
    }

    std::cout << "scenario_119_jiyuan_ds001: all passed" << std::endl;
    return 0;
}
