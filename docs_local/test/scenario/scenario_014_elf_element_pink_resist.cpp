// 场景 N：精灵系别半持久化视图 + 粉伤抗性系统。
//
// N1 半持久化跨回合：魂印 ROUND_START 把 elf_element_view[0] 改为水；sync 每回合从视图写
//    workspace（ws.reset 不清），故第 2 回合 ws 仍是水。对手草系技能打 pet0：
//    改系别后克制 0.5(草对草)→2.0(草对水)，第 2 回合伤害显著更高。
// N2 下场刷新：elf_element_view 绑定的精灵槽变化（换宠）→ invalidate 置 bound=-1 →
//    sync 从新精灵重基（覆盖掉效果改的系别）。
// N3 免疫粉伤：pink_immune → FIXED 伤害 0（HP 不变）。
// N4 三种伤害抗性：固定抗性减 FIXED、百分比抗性减 PERCENT、减粉(通用)再减。
// N5 粉转真：免疫粉伤 + pink_to_true → FIXED 改以真实伤害全额结算。
// N6 免粉补偿一类：效果侧比较 HP，免疫挡下（HP 不变）→ 补偿触发。
// N7 暴击抗性：100% 暴击技能 → 2 倍；防御方暴击抗性 50% → 1.5 倍（削减加成部分）。
#include <cassert>
#include <iostream>

#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

constexpr int kElementGrass = 1;    // 草
constexpr int kElementWater = 2;    // 水
constexpr int kElementElectric = 5; // 电
constexpr int kSkillGrass = 10003;     // 疾风刃：草(1)，无 side_effect
constexpr int kSkillZhuangJi = 10001;  // 撞击：普通(8)

// N1 魂印：ROUND_START 把精灵系别视图改为水（属性反转演示；半持久化跨回合保留）。
inline EffectResult change_elf_element_effect(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ctx->elf_element_view[owner][0] = kElementWater;
    ctx->elf_element_view[owner][1] = 0;
    return EffectResult::kOk;
}

inline SoulMark make_change_element_soul_mark() {
    SoulMark sm;
    sm.id = 9993;
    sm.name = "属性反转(演示)";
    sm.description = "ROUND_START 把精灵系别视图改为水系";
    sm.args = EffectArgs(std::vector<int>{0, 1});
    sm.program_ = {SoulMarkNodeRef(State::BATTLE_ROUND_START, &change_elf_element_effect)};
    sm.has_program_ = true;
    return sm;
}

// 造精灵：技能 + 数值 + 可选魂印，元素手动覆盖。
ElfPet make_pet(int id, int element, SoulMark sm = {}) {
    auto pet = scenario::make_pet(id, "精灵", {kSkillGrass, kSkillGrass, kSkillGrass,
                                               kSkillGrass, kSkillGrass},
                                  /*speed=*/100, /*atk=*/150, /*hp=*/800, std::move(sm));
    pet.elementalAttributes[0] = element;
    pet.elementalAttributes[1] = 0;
    return pet;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== N1 半持久化跨回合：改系别效果存活 ws.reset，且影响克制伤害 ==========
    {
        // pet0（防御方，后手）系别视图被改为水；对手（先手）草系技能打它。
        auto pet0 = make_pet(0, kElementGrass, make_change_element_soul_mark());
        auto opp = make_pet(1, kElementWater);
        opp.numericalProperties[NumericalPropertyIndex::SPEED] = 200;  // 对手先手
        scenario::ScenarioBattle battle(scenario::make_party(pet0), scenario::make_party(opp));
        battle.start();
        battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.drive();  // 回合 1
        const bool r1_grass =
            (battle.ctx().ws.view_elementalAttributes[0][0] == kElementGrass);
        const int hp_after_r1 = battle.ctx().getPet(0).hp;
        battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.drive();  // 回合 2
        const bool r2_water =
            (battle.ctx().ws.view_elementalAttributes[0][0] == kElementWater);
        const int hp_after_r2 = battle.ctx().getPet(0).hp;
        const int dmg_r1 = 800 - hp_after_r1;
        const int dmg_r2 = hp_after_r1 - hp_after_r2;
        // 回合1：sync 在 ROUND_START 效果前 → 基线草（克制 0.5）；回合2：视图水（克制 2.0）
        assert(r1_grass && r2_water);
        assert(dmg_r2 >= dmg_r1 * 3);  // 0.5x → 2.0x ≈ 4 倍
        std::cout << "  [N1] 半持久化: r1 元素草->r2 元素水 (ws.reset 不清); 受击 "
                  << dmg_r1 << "->" << dmg_r2 << " (克制 0.5->2.0)" << std::endl;
    }

    //========== N2 下场刷新：换宠 → 从新精灵重基系别（覆盖效果改的值） ==========
    {
        auto petA = make_pet(0, kElementGrass);   // 槽0 草
        auto petE = make_pet(0, kElementElectric); // 槽2 电
        std::array<ElfPet, 6> party0{petA, petA, petE, petA, petA, petA};
        auto opp = make_pet(1, kElementWater);
        opp.numericalProperties[NumericalPropertyIndex::SPEED] = 100;
        scenario::ScenarioBattle battle(party0, scenario::make_party(opp));
        battle.start();
        // 模拟某改系别效果：把 pet0 视图改为水并绑定槽0
        battle.ctx().elf_element_view[0][0] = kElementWater;
        battle.ctx().elf_element_view[0][1] = 0;
        battle.ctx().elf_element_view_bound_slot[0] = 0;
        battle.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 2);  // 换到槽2（电系）
        battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.drive();
        assert(battle.ctx().on_stage[0] == 2);
        // 换宠：invalidate 置 bound=-1 → sync 从新精灵(电)重基，覆盖掉效果改的水
        assert(battle.ctx().elf_element_view[0][0] == kElementElectric);
        assert(battle.ctx().ws.view_elementalAttributes[0][0] == kElementElectric);
        std::cout << "  [N2] 下场刷新: 换宠后 elf_element_view[0]={电,"
                  << battle.ctx().elf_element_view[0][1] << "} (从新精灵重基)" << std::endl;
    }

    //========== N3-N6 粉伤抗性（直接调 deal_damage 原语） ==========
    {
        auto pet1 = scenario::make_pet(1, "对手", {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                                   kSkillZhuangJi, kSkillZhuangJi},
                                       /*speed=*/100, /*atk=*/100, /*hp=*/5000);
        scenario::ScenarioBattle battle(scenario::make_party(pet1), scenario::make_party(pet1));
        BattleContext& ctx = battle.ctx();

        // N3 免疫粉伤
        ctx.pink_immune[1] = true;
        const int hp0 = ctx.getPet(1).hp;
        deal_damage(&ctx, 1, 100, DamageKind::FIXED, 0);
        assert(ctx.getPet(1).hp == hp0);
        std::cout << "  [N3] 免疫粉伤: FIXED 100 -> HP 不变" << std::endl;
        ctx.pink_immune[1] = false;

        // N4 三种伤害抗性（固定/百分比）+ 减粉
        ctx.getPet(1).damage_resist.fixed_pct = 50;
        ctx.sync_damage_resist_view(1);
        const int hp1 = ctx.getPet(1).hp;
        deal_damage(&ctx, 1, 100, DamageKind::FIXED, 0);
        assert(hp1 - ctx.getPet(1).hp == 50);
        std::cout << "  [N4] 固定抗性50%: FIXED 100 -> " << (hp1 - ctx.getPet(1).hp) << std::endl;
        ctx.getPet(1).damage_resist.fixed_pct = 0;
        ctx.sync_damage_resist_view(1);
        ctx.getPet(1).damage_resist.percent_pct = 50;
        ctx.sync_damage_resist_view(1);
        const int hp1b = ctx.getPet(1).hp;
        deal_damage(&ctx, 1, 50, DamageKind::PERCENT, 0);  // 50% 最大体力 = 2500
        assert(hp1b - ctx.getPet(1).hp == 1250);           // 50% 百分比抗性 → 1250
        std::cout << "  [N4b] 百分比抗性50%: PERCENT 50% -> " << (hp1b - ctx.getPet(1).hp)
                  << " (2500→1250)" << std::endl;
        ctx.getPet(1).damage_resist.percent_pct = 0;
        ctx.sync_damage_resist_view(1);
        ctx.pink_reduce_pct[1] = 25;
        const int hp2 = ctx.getPet(1).hp;
        deal_damage(&ctx, 1, 100, DamageKind::FIXED, 0);
        assert(hp2 - ctx.getPet(1).hp == 75);
        std::cout << "  [N4c] 减粉25%: FIXED 100 -> " << (hp2 - ctx.getPet(1).hp) << std::endl;
        ctx.pink_reduce_pct[1] = 0;

        // N5 粉转真
        ctx.pink_immune[1] = true;
        ctx.pink_to_true[1] = true;
        const int hp3 = ctx.getPet(1).hp;
        deal_damage(&ctx, 1, 100, DamageKind::FIXED, 0);
        assert(hp3 - ctx.getPet(1).hp == 100);  // 免疫但粉转真 → 真实伤害全额
        std::cout << "  [N5] 粉转真: FIXED 100 -> 真伤 " << (hp3 - ctx.getPet(1).hp) << std::endl;
        ctx.pink_immune[1] = false;
        ctx.pink_to_true[1] = false;

        // N6 免粉补偿一类：效果侧比较 HP
        ctx.pink_immune[1] = true;
        const int hp4 = ctx.getPet(1).hp;
        deal_damage(&ctx, 1, 100, DamageKind::FIXED, 0);
        if (ctx.getPet(1).hp == hp4) {
            deal_damage(&ctx, 1, 60, DamageKind::TRUE, 0);  // 被挡 → 补偿真伤
        }
        assert(hp4 - ctx.getPet(1).hp == 60);
        std::cout << "  [N6] 免粉补偿一类: 免疫挡粉(HP不变) -> 补偿真伤 "
                  << (hp4 - ctx.getPet(1).hp) << std::endl;
    }

    //========== N7 暴击抗性（三种伤害抗性之一：削减暴击加成部分） ==========
    {
        auto atk = scenario::make_pet(0, "攻", {kSkillGrass, kSkillGrass, kSkillGrass,
                                                kSkillGrass, kSkillGrass},
                                      /*speed=*/200, /*atk=*/150, /*hp=*/800);
        atk.elementalAttributes[0] = kElementGrass;
        atk.elementalAttributes[1] = 0;
        atk.skills[0].critical_strike_rate = 100.0f;  // 100% 暴击

        auto def = scenario::make_pet(1, "防", {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                                kSkillZhuangJi, kSkillZhuangJi},
                                      /*speed=*/100, /*atk=*/100, /*hp=*/5000);
        def.elementalAttributes[0] = kElementWater;  // 草打水 2x，放大差异
        def.elementalAttributes[1] = 0;

        scenario::ScenarioBattle b0(scenario::make_party(atk), scenario::make_party(def));
        b0.start();
        b0.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b0.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b0.drive();
        const int dmg_no_resist = 5000 - b0.ctx().getPet(1).hp;

        scenario::ScenarioBattle b1(scenario::make_party(atk), scenario::make_party(def));
        b1.start();
        b1.ctx().getPet(1).damage_resist.crit_pct = 50;  // 防御方本体暴击抗性 50%
        b1.ctx().sync_damage_resist_view(1);
        // 注：b1.drive() 会跑完整回合，sync 在回合开始重基一次，此处抗性来自 pet 本体
        b1.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b1.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b1.drive();
        const int dmg_with_resist = 5000 - b1.ctx().getPet(1).hp;

        // 2 倍暴击 + 50% 暴击抗性 → 1.5 倍：dmg_with ≈ dmg_no * 0.75（±8% 随机容差）
        assert(dmg_no_resist > 0);
        assert(dmg_with_resist * 100 >= dmg_no_resist * 68);
        assert(dmg_with_resist * 100 <= dmg_no_resist * 82);
        std::cout << "  [N7] 暴击抗性50%: 暴击2x=" << dmg_no_resist
                  << " 抗性后=" << dmg_with_resist << " (≈1.5x)" << std::endl;
    }

    //========== N8 本体抗性跨切换保留（本次修复点） ==========
    // 原先抗性存 BattleContext（per-owner）且换宠时被 invalidate_on_stage_effects 清零 →
    // 精灵**自身**的抗性切走再切回来就丢了。现抗性存在 ElfPet::damage_resist（本体），
    // 换宠只是让 ws 视图从新精灵重基，本体值不受影响。
    {
        auto resister = scenario::make_pet(0, "带抗性", {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                                         kSkillZhuangJi, kSkillZhuangJi},
                                           /*speed=*/200, /*atk=*/100, /*hp=*/5000);
        resister.damage_resist.fixed_pct = 50;  // 本体固定伤害抗性 50%
        auto plain = scenario::make_pet(0, "无抗性", {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                                      kSkillZhuangJi, kSkillZhuangJi},
                                        /*speed=*/200, /*atk=*/100, /*hp=*/5000);
        std::array<ElfPet, 6> party0{resister, plain, plain, plain, plain, plain};
        auto enemy = scenario::make_pet(1, "对手", {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                                    kSkillZhuangJi, kSkillZhuangJi},
                                        /*speed=*/100, /*atk=*/100, /*hp=*/5000);
        scenario::ScenarioBattle battle(party0, scenario::make_party(enemy));
        BattleContext& ctx = battle.ctx();
        battle.start();

        // 带抗性的精灵先站场（槽 0）→ 切到槽 1（切走）→ 再切回槽 0
        battle.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.drive();
        battle.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 0);
        battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.drive();

        assert(ctx.on_stage[0] == 0);
        // 本体抗性未被切换清掉
        assert(ctx.seerRobot[0].elfPets[0].damage_resist.fixed_pct == 50);
        // ws 视图已从新上场精灵重基
        assert(ctx.ws.eff_fixed_resist_pct[0] == 50);
        // 实际结算：FIXED 100 → 50（抗性仍生效）
        const int hp_before = ctx.getPet(0).hp;
        deal_damage(&ctx, 0, 100, DamageKind::FIXED, 1);
        const int taken = hp_before - ctx.getPet(0).hp;
        assert(taken == 50);
        std::cout << "  [N8] 本体抗性跨切换保留: 切走再切回后 FIXED 100 -> " << taken
                  << " (50% 抗性仍生效)" << std::endl;
    }

    //========== N9 临时 buff 改视图不改本体（混元天尊死亡 buff 型） ==========
    // 官方：该 buff 让己方精灵抗性"被视为 100%"，3 回合后恢复；且只把**非 0** 的抗性
    // 提升到 100%（本体没开抗性 = 0 则不受影响）。故 buff 写 ws 视图、不写 pet 本体。
    {
        auto pet1 = scenario::make_pet(1, "对手", {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                                   kSkillZhuangJi, kSkillZhuangJi},
                                       /*speed=*/100, /*atk=*/100, /*hp=*/5000);
        scenario::ScenarioBattle battle(scenario::make_party(pet1), scenario::make_party(pet1));
        BattleContext& ctx = battle.ctx();

        // N9a：本体 30% → buff 把视图提到 100% → 全额免伤；本体不变
        ctx.getPet(1).damage_resist.fixed_pct = 30;
        ctx.sync_damage_resist_view(1);
        assert(ctx.ws.eff_fixed_resist_pct[1] == 30);
        ctx.ws.eff_fixed_resist_pct[1] = 100;  // buff 生效（本体非 0，才提升）
        const int hp_a = ctx.getPet(1).hp;
        deal_damage(&ctx, 1, 100, DamageKind::FIXED, 0);
        assert(ctx.getPet(1).hp == hp_a);                          // 100% 抗性 → 全免
        assert(ctx.getPet(1).damage_resist.fixed_pct == 30);       // 本体未被 buff 改
        std::cout << "  [N9a] buff 视图→100%: FIXED 100 -> 0；本体仍 30%" << std::endl;

        // N9b：本体 0（未开抗性）→ buff 不提升 → 伤害全额
        ctx.getPet(1).damage_resist.fixed_pct = 0;
        ctx.sync_damage_resist_view(1);
        const int hp_b = ctx.getPet(1).hp;
        deal_damage(&ctx, 1, 100, DamageKind::FIXED, 0);
        assert(hp_b - ctx.getPet(1).hp == 100);
        std::cout << "  [N9b] 本体为0 时 buff 不生效: FIXED 100 -> 100" << std::endl;
    }

    std::cout << "scenario_n PASS (精灵系别半持久化 + 粉伤抗性: 免疫/暴击/固定/百分比抗性/减粉/粉转真/补偿"
                 " + 本体跨切换保留/view 分离)"
              << std::endl;
    return 0;
}
