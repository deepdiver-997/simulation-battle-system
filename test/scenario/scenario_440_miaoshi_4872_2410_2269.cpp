#include <utils/sbs_rand.h>
// 场景 440：妙时天女 4872 第五技「万卍觉心法界镇 38275」缺失效果 2410、2269
//（moves_lib/d5_D28.cpp，2026-09-29）
//
// 断言组：
//   TA 2410 怀生菩怜态：对手中毒+盾 1000 → 命中 → 对手转疲惫、自身得盾 990（99%）。
//   TB 2410 拒赦怒威态：对手中毒 → 命中 → 对手转害怕、当回合回血 -99%（1000→10）、
//      下回合复原（1000→1000）。
//   TC 2410 无态 no-op：mode=0（百天浮世络中/未结算）→ 异常保持、无盾、恢复修正不动。
//   TD 2269 打盔触发：38275 命中一次（武装）→ 再用打在对手攻击盔上 → 对手回合类效果
//      被消 + 4872 得免疫盔（对手下次攻击被免、hp 不动）。
//   TE 2269 收窄反例：武装后用拍打打盔 → 对手回合票保留、无 2269 盔（只认万卍）。
//   TF 1962/1963 复用：38275 命中未击败 → 对手回合票+强化被清（D19 d5_D19.cpp 的
//      全局注册在 38275 链上生效，本组守"勿重复注册"的复用口径）。
//
// 确定性：两态 mode 用场景直写（scenario_065 先例）；R1~R3 先烧百天浮世络 window
//（魂印 settle 在 window 归零回合会重选态覆盖直写，故直写必须落在 R4 之前且 window
// 已尽）；38275 必中、crit_rate=0；TA/TB 给 4872 挂一次攻击免疫盔挡对手后手拍打，
// 保盾值等式断言不被对手攻击污染。
#include <cstdlib>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kWanwan = 38275;   // 万卍觉心法界镇：特攻 160、必中、effect [2410,2269,1962,1963]
constexpr int kPaiDa = 10004;    // 拍打：物攻 40、无效果
constexpr int kSuoTou = 20001;   // 缩头：属性技、无效果、必中（TD 用于"对手不攻击"回合）
constexpr int kMiaoShiMark = 2478;
constexpr int kMiaoShiModeKey = kMiaoShiMark * 100 + 7;   // 247807（soul_lib 落点）

ElfPet miaoshi_pet(int atk = 200) {
    SoulMark sm = SoulMark(kMiaoShiMark, "妙时天女", "百天浮世络");
    ElfPet p = scenario::make_pet(4872, "妙时天女", {kWanwan, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                  /*speed=*/200, atk, /*hp=*/9999, std::move(sm));
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet(int speed = 100, int atk = 10) {
    ElfPet p = scenario::make_pet(1, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                  speed, atk, /*hp=*/9999);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// TD 专用对手：slot0=缩头（属性技，"不攻击"回合用）、slot1=拍打（高攻，免疫验证用）。
ElfPet foe_pet_high_atk() {
    ElfPet p = scenario::make_pet(1, "对手", {kSuoTou, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                  /*speed=*/100, /*atk=*/3000, /*hp=*/9999);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

void drive_round(ScenarioBattle& b, int slot0, int slot1) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, slot0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, slot1);
    b.drive();
}

// 直写两态（scenario_065 同款落点：soulmark_storage[247807]）。
void force_mode(BattleContext& ctx, int mode) {
    for (int slot = 0; slot < 6; ++slot) {
        if (ctx.seerRobot[0].elfPets[slot].soulMark.id == kMiaoShiMark) {
            ctx.seerRobot[0].elfPets[slot].soulmark_storage[kMiaoShiModeKey] = mode;
        }
    }
}

// 给 4872 一层一次性攻击免疫盔（挡对手后手攻击，保等式断言不被污染）。
void guard_self(BattleContext& ctx) {
    seal_skill(&ctx, /*source=*/0, /*target=*/1, /*effect_id=*/74401,
               /*attribute=*/false, /*attack=*/true, /*count=*/1,
               /*duration_rounds=*/0, /*penetrable=*/true, /*source_slot=*/-1,
               EffectScope::ON_STAGE, /*hit_invalid=*/false, /*chance_pct=*/100,
               /*consumed_when_pierced=*/false);
}

// 对手名下持回合类票（350 TC 同款：source=1 封 0 方属 3 回合 → has_round_type(1)）。
void give_foe_round_ticket(BattleContext& ctx) {
    seal_skill(&ctx, /*source=*/1, /*target=*/0, /*effect_id=*/74405,
               /*attribute=*/true, /*attack=*/false, /*count=*/0,
               /*duration_rounds=*/3, /*penetrable=*/true, /*source_slot=*/-1,
               EffectScope::ON_STAGE, /*hit_invalid=*/false, /*chance_pct=*/100,
               /*consumed_when_pierced=*/false);
}

// 对手给 4872 上一层可穿攻击盔（1 次）——38275/拍打打上去 → SKILL_INVALID。
void give_self_attack_armor(BattleContext& ctx) {
    seal_skill(&ctx, /*source=*/1, /*target=*/0, /*effect_id=*/74402,
               /*attribute=*/false, /*attack=*/true, /*count=*/1,
               /*duration_rounds=*/0, /*penetrable=*/true, /*source_slot=*/-1,
               EffectScope::ON_STAGE, /*hit_invalid=*/false, /*chance_pct=*/100,
               /*consumed_when_pierced=*/false);
}

bool has_2269_armor(BattleContext& ctx) {
    for (const RuleTicket& t : ctx.rule_center_.entries()) {
        if (t.source_effect_id == 2269) {
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

    //========== TA 2410 怀生菩怜态：异常→疲惫 + 护盾 99% 转移 ==========
    {
        sbs_srand(20260929);
        ScenarioBattle b(scenario::make_party(miaoshi_pet()),
                         scenario::make_party(foe_pet(/*speed=*/100, /*atk=*/10)));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b, 1, 0);   // R1~R3 拍打互殴：烧百天浮世络 window（settle 选态）
        drive_round(b, 1, 0);
        drive_round(b, 1, 0);
        force_mode(ctx, 1);     // 怀生菩怜态（window 已尽，settle 不再覆盖）
        apply_anomaly(&ctx, 1, abnormal_status_id(AbnormalStatusId::Poison),
                      /*duration_rounds=*/5, /*actor=*/0);
        ctx.getPet(1).shield_bank_.add(0, 1000, /*source_id=*/74403, /*duration=*/0);
        guard_self(ctx);        // 挡对手后手拍打，保盾等式
        drive_round(b, 0, 0);   // R4：38275 命中
        CHECK(ctx.has_active_abnormal_status(1, abnormal_status_id(AbnormalStatusId::Fatigue)),
              "TA 核心：对手异常转化为疲惫");
        CHECK(!ctx.has_active_abnormal_status(1, abnormal_status_id(AbnormalStatusId::Poison)),
              "TA 核心：原异常（中毒）已解除");
        CHECK_EQ(ctx.getPet(0).shield_bank_.total(), 990,
                 "TA 核心：获得对手护盾值 99% 的护盾（1000×99%=990）");
        std::cout << "  [TA] 菩怜态转化+护盾 通过\n";
    }

    //========== TB 2410 拒赦怒威态：异常→害怕 + 当回合恢复 -99% ==========
    {
        sbs_srand(20260929);
        ScenarioBattle b(scenario::make_party(miaoshi_pet()),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b, 1, 0);
        drive_round(b, 1, 0);
        drive_round(b, 1, 0);
        force_mode(ctx, 2);     // 拒赦怒威态
        apply_anomaly(&ctx, 1, abnormal_status_id(AbnormalStatusId::Poison), 5, 0);
        guard_self(ctx);
        drive_round(b, 0, 0);   // R4：38275 命中
        CHECK(ctx.has_active_abnormal_status(1, abnormal_status_id(AbnormalStatusId::Fear)),
              "TB 核心：对手异常转化为害怕");
        CHECK_EQ(ctx.heal_mod_pct[1], -99, "TB 核心：当回合恢复效果 -99%");
        const int hp_before = ctx.getPet(1).hp;
        heal_amount(&ctx, 1, 1000);
        CHECK_EQ(ctx.getPet(1).hp - hp_before, 10, "TB 行为：回血 1000 实回 10（-99%）");
        drive_round(b, 1, 0);   // R5：ROUND_START tick 复原
        CHECK_EQ(ctx.heal_mod_pct[1], 0, "TB 核心：下回合恢复修正复原");
        const int hp2 = ctx.getPet(1).hp;
        heal_amount(&ctx, 1, 500);   // 对手血线接近上限，取小额验证避免满血钳制
        CHECK_EQ(ctx.getPet(1).hp - hp2, 500, "TB 行为：复原后回血 500 实回 500");
        std::cout << "  [TB] 怒威态转化+恢复减少 通过\n";
    }

    //========== TC 2410 无态 no-op ==========
    {
        sbs_srand(20260929);
        ScenarioBattle b(scenario::make_party(miaoshi_pet()),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b, 1, 0);
        drive_round(b, 1, 0);
        drive_round(b, 1, 0);
        force_mode(ctx, 0);     // 两态未进入
        apply_anomaly(&ctx, 1, abnormal_status_id(AbnormalStatusId::Poison), 5, 0);
        ctx.getPet(1).shield_bank_.add(0, 1000, 74403, 0);
        guard_self(ctx);
        drive_round(b, 0, 0);
        CHECK(ctx.has_active_abnormal_status(1, abnormal_status_id(AbnormalStatusId::Poison)),
              "TC 核心：无态 → 对手异常保持");
        CHECK_EQ(ctx.getPet(0).shield_bank_.total(), 0, "TC 核心：无态 → 自身无盾");
        CHECK_EQ(ctx.heal_mod_pct[1], 0, "TC 核心：无态 → 恢复修正不动");
        std::cout << "  [TC] 无态 no-op 通过\n";
    }

    //========== TD 2269 打盔触发：消对手回合 + 免疫下次攻击 ==========
    {
        sbs_srand(20260929);
        ScenarioBattle b(scenario::make_party(miaoshi_pet()),
                         scenario::make_party(foe_pet_high_atk()));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b, 0, 0);   // R1：38275 命中（无盔）→ 2269 watcher 武装（对手缩头不攻击）
        CHECK(has_2269_armor(ctx) == false, "TD 前提：命中不触发补偿（无 2269 盔）");
        give_self_attack_armor(ctx);
        give_foe_round_ticket(ctx);
        CHECK(ctx.rule_center_.has_round_type(1), "TD 前提：对手持有回合类效果");
        drive_round(b, 0, 0);   // R2：38275 打盔 → SKILL_INVALID → 补偿（对手缩头，不消耗新盔）
        CHECK(!ctx.rule_center_.has_round_type(1), "TD 核心：技能无效 → 消除对手回合类效果");
        CHECK(has_2269_armor(ctx), "TD 核心：获得免疫下次攻击的盔（effect_id=2269）");
        const int hp_before = ctx.getPet(0).hp;
        drive_round(b, 1, 1);   // R3：对手拍打（atk 3000）→ 被 2269 盔拦
        CHECK_EQ(ctx.getPet(0).hp, hp_before,
                 "TD 核心：对手下次攻击被免疫（高攻拍打零掉血）");
        CHECK(has_2269_armor(ctx) == false, "TD 附加：拦截后次数盔消耗");
        std::cout << "  [TD] 打盔补偿+免疫盔 通过\n";
    }

    //========== TE 2269 收窄反例：其他技能打盔不触发 ==========
    {
        sbs_srand(20260929);
        ScenarioBattle b(scenario::make_party(miaoshi_pet()),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b, 0, 0);   // R1：38275 命中 → 武装
        give_self_attack_armor(ctx);
        give_foe_round_ticket(ctx);
        drive_round(b, 1, 0);   // R2：拍打打盔 → SKILL_INVALID，但非万卍
        CHECK(ctx.rule_center_.has_round_type(1), "TE 核心：非万卍无效 → 对手回合票保留");
        CHECK(!has_2269_armor(ctx), "TE 核心：非万卍无效 → 不授予免疫盔");
        std::cout << "  [TE] 收窄反例 通过\n";
    }

    //========== TF 1962/1963 复用：38275 链上消回合+消提升 ==========
    {
        sbs_srand(20260929);
        ScenarioBattle b(scenario::make_party(miaoshi_pet()),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        give_foe_round_ticket(ctx);
        ctx.ability_levels[1][0] = 2;
        drive_round(b, 0, 0);   // R1：38275 命中未击败（对手 9999 血）→ D19 桥生效
        CHECK(ctx.getPet(1).hp > 0, "TF 前提：未击败（对手存活）");
        CHECK(!ctx.rule_center_.has_round_type(1), "TF 核心：未击败 → 消除对手回合类效果（1962 复用）");
        CHECK_EQ(ctx.ability_levels[1][0], 0, "TF 核心：未击败 → 消除对手能力提升（1963 复用）");
        std::cout << "  [TF] 1962/1963 复用 通过\n";
    }

    std::cout << "scenario_440 all passed" << std::endl;
    return 0;
}
