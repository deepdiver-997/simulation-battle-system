#include <utils/sbs_rand.h>
// 场景 270：混沌魔尊 3683 双第五技缺失效果 1038、2561、2352、2496
//（moves_lib/d5_D11.cpp，夜跑 night-2，2026-09-28）
//
// 断言组：
//   TA 2561 剥离混沌系：开场后 elementalAttributes 不再含 222（混沌）。
//   TB 2561 登场封属：3683 中切下场再回归 → 对手下 1 次属性技无效（第 2 次恢复）；
//      队友（非携带者）登场不触发。对手带控制类异常时回归不触发（TC）。
//   TD 1038 双倍反转+盾（38526 切片 400）：-2/-3 → +4/+6，盾 400。
//   TE 1038（32396 切片 300）：-1 → +2，盾 300。
//   TF 2352 增伤翻倍：对手无异常 = +100%（掉血 2×裸伤+吸体 3333）；有异常 = +200%
//      （3×裸伤、无吸体）。
//   TG 2496 零伤换血：对手满盾吸满攻击（final=0）且体力低于对手 → 绝对值互换。
//
// 确定性：32396/38526/拍打/高速移动 crit_rate=0、必中；链内 1257 的吸体量固定
// 9999/3=3333，按对手异常与否分别计入等式。
#include <array>
#include <cstdlib>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kHunShi = 32396;  // 魔使碎灭令：effect [1038, 1048, 1257]
constexpr int kJunLin = 38526;  // 混沌君临诏：effect [2561, 1038, 2352, 1257, 2496]
constexpr int kPaDa = 10004;    // 拍打：物攻 40、无效果
constexpr int kGaoSu = 20017;   // 高速移动：属性技、必中
constexpr int kChaos = 222;     // 混沌组件
constexpr int kDark = 13;       // 暗影组件
constexpr int kDrain = 3333;    // 1257：对手最大体力 1/3

ElfPet mozun(int speed, int fifth) {
    ElfPet p = scenario::make_pet(3683, "混沌魔尊", {fifth, kPaDa, kPaDa, kPaDa, kPaDa},
                                  speed, /*atk=*/150, /*hp=*/9999);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    p.elementalAttributes[0] = kChaos;
    p.elementalAttributes[1] = kDark;
    return p;
}

ElfPet buddy(int speed) {   // 非携带者队友（无 38526）
    ElfPet p = scenario::make_pet(999901, "队友", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                  speed, /*atk=*/10, /*hp=*/9999);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe(int speed, int atk = 10) {
    ElfPet p = scenario::make_pet(1, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                  speed, atk, /*hp=*/9999);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet attr_foe(int speed) {   // 带属性技（高速移动）的对手：封属断言用
    ElfPet p = scenario::make_pet(1, "对手", {kGaoSu, kPaDa, kPaDa, kPaDa, kPaDa},
                                  speed, /*atk=*/10, /*hp=*/9999);
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

void drive_switch0(ScenarioBattle& b, int to_slot) {
    b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, to_slot);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);   // 拍打：不消耗可能的封属
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== TA 2561：战斗开始剥离混沌系 ==========
    {
        sbs_srand(20260928);
        ScenarioBattle b(scenario::make_party(mozun(/*speed=*/200, kJunLin)),
                         scenario::make_party(foe(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK(ctx.getPet(0).elementalAttributes[0] != kChaos
                  && ctx.getPet(0).elementalAttributes[1] != kChaos,
              "TA 核心：开场后自身不再含混沌分量（剥离）");
        std::cout << "  [TA] 剥离混沌系 通过（现 "
                  << ctx.getPet(0).elementalAttributes[0] << "/"
                  << ctx.getPet(0).elementalAttributes[1] << "）\n";
    }

    //========== TB 2561 登场封属：回归触发、队友不触发、消费后恢复 ==========
    {
        sbs_srand(20260928);
        const ElfPet m = mozun(/*speed=*/200, kJunLin);
        const ElfPet bd = buddy(/*speed=*/100);
        std::array<ElfPet, 6> team{m, bd, bd, bd, bd, bd};
        ScenarioBattle b(team, scenario::make_party(attr_foe(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b, 1, 0);            // R1：双方拍打（对手无异常可被消除，链内其余无碍）
        drive_switch0(b, 1);             // R2：换队友上场（非携带者 → 不封）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        drive_switch0(b, 0);             // R3：混沌魔尊回归 → 封对手 1 次属性技
        CHECK(ctx.rule_center_.has_seal_kind(1, SealKind::SEAL_ATTRIBUTE, ctx.roundCount),
              "TB 前提：回归后对手身上有封属票");
        drive_round(b, 1, 0);            // R4：对手用属性技 → 无效
        CHECK_EQ(ctx.ws.skill_exec_result[1], SkillExecResult::SKILL_INVALID,
                 "TB 核心：混沌魔尊登场 → 对手下 1 次属性技能无效");
        drive_round(b, 1, 0);            // R5：次数已消费 → 恢复
        CHECK(ctx.ws.skill_exec_result[1] != SkillExecResult::SKILL_INVALID,
              "TB：封技次数已消费，属性技恢复生效");
        std::cout << "  [TB] 登场封属 + 消费恢复 通过\n";
    }

    //========== TC 2561 对照：对手带控制类异常时登场不封 ==========
    {
        sbs_srand(20260928);
        const ElfPet m = mozun(/*speed=*/200, kJunLin);
        const ElfPet bd = buddy(/*speed=*/100);
        std::array<ElfPet, 6> team{m, bd, bd, bd, bd, bd};
        ScenarioBattle b(team, scenario::make_party(attr_foe(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.abnormal_status_end_round[1][static_cast<std::size_t>(AbnormalStatusId::Paralysis)] =
            ctx.roundCount + 9;   // 对手带控制类异常（直写槽，覆盖回归回合的判定点）
        drive_round(b, 1, 0);
        drive_switch0(b, 1);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        drive_switch0(b, 0);         // 混沌魔尊回归：对手处于控制类异常 → 不封
        CHECK(!ctx.rule_center_.has_seal_kind(1, SealKind::SEAL_ATTRIBUTE, ctx.roundCount),
              "TC 核心：对手处于控制类异常 → 登场不封属性技（无封属票）");
        std::cout << "  [TC] 控制类异常压制登场封属 通过\n";
    }

    //========== TD 1038：双倍反转 + 盾（38526 切片 400）==========
    {
        sbs_srand(20260928);
        ScenarioBattle b(scenario::make_party(mozun(/*speed=*/200, kJunLin)),
                         scenario::make_party(foe(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[0][0] = -2;
        ctx.ability_levels[0][1] = -3;
        drive_round(b, 0, 0);
        CHECK_EQ(ctx.ability_levels[0][0], 4, "TD 核心：-2 双倍反转 → +4");
        CHECK_EQ(ctx.ability_levels[0][1], 6, "TD 核心：-3 双倍反转 → +6");
        const int foe_hit = ctx.ws.raw_attack_damage[1];   // 对手回合内攻击消耗了盾
        CHECK_EQ(ctx.getPet(0).shield_bank_.total(), 400 - foe_hit,
                 "TD 核心：反转成功 → 获得护盾 400（38526 切片；末尾被对手拍打消耗）");
        std::cout << "  [TD] 双倍反转 +4/+6、盾 400 通过\n";
    }

    //========== TE 1038（32396 切片 300）：-1 → +2 ==========
    {
        sbs_srand(20260928);
        ScenarioBattle b(scenario::make_party(mozun(/*speed=*/200, kHunShi)),
                         scenario::make_party(foe(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[0][2] = -1;
        drive_round(b, 0, 0);
        CHECK_EQ(ctx.ability_levels[0][2], 2, "TE 核心：-1 双倍反转 → +2");
        const int foe_hit2 = ctx.ws.raw_attack_damage[1];
        CHECK_EQ(ctx.getPet(0).shield_bank_.total(), 300 - foe_hit2,
                 "TE 核心：护盾 300（32396 切片；被对手拍打消耗）");
        std::cout << "  [TE] 双倍反转 +2、盾 300 通过\n";
    }

    //========== TF 2352：无异常 +100%、异常翻倍 +200%（含 1257 吸体记账）==========
    {
        int loss_clean = 0;
        int loss_anom = 0;
        int raw = 0;
        {
            sbs_srand(20260928);
            ScenarioBattle b(scenario::make_party(mozun(/*speed=*/200, kJunLin)),
                             scenario::make_party(foe(/*speed=*/100)));
            b.start();
            BattleContext& ctx = b.ctx();
            drive_round(b, 0, 0);
            raw = ctx.ws.raw_attack_damage[0];
            loss_clean = 9999 - ctx.getPet(1).hp;   // = 2×裸伤 + 3333 吸体
            CHECK(loss_clean >= 2 * raw + kDrain - 1 && loss_clean <= 2 * raw + kDrain + 1,
                  "TF 核心：无异常 → +100%（2×裸伤）+ 吸体 3333");
        }
        {
            sbs_srand(20260928);
            ScenarioBattle b(scenario::make_party(mozun(/*speed=*/200, kJunLin)),
                             scenario::make_party(foe(/*speed=*/100)));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.abnormal_status_end_round[1][static_cast<std::size_t>(AbnormalStatusId::Paralysis)] =
                ctx.roundCount + 9;   // 对手异常（麻痹无回合扣血）：2352 翻倍、1257 不触发
            drive_round(b, 0, 0);
            loss_anom = 9999 - ctx.getPet(1).hp;   // = 3×裸伤（吸体不触发）
            CHECK(loss_anom >= 3 * raw - 1 && loss_anom <= 3 * raw + 1,
                  "TF 核心：对手异常 → 翻倍 +200%（3×裸伤）、1257 不触发");
        }
        std::cout << "  [TF] 增伤 100%/异常翻倍 通过（clean " << loss_clean << " / anom "
                  << loss_anom << "，裸伤 " << raw << "）\n";
    }

    //========== TG 2496：攻击被盾吸满（final=0）且体力低于对手 → 换血 ==========
    {
        sbs_srand(20260928);
        ScenarioBattle b(scenario::make_party(mozun(/*speed=*/200, kJunLin)),
                         scenario::make_party(foe(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 3000;   // 自身体力低于对手
        ctx.getPet(1).hp = 5000;
        // 对手挂"免伤"票（管线 BLOCK 段 → 本击攻击伤害 final=0；SKILL_EFFECT 照常注册）
        ctx.rule_center_.grant_immune(/*owner=*/1, static_cast<int>(ImmunityType::DAMAGE),
                                      ~0ULL, /*anomaly_mask=*/0, /*duration_rounds=*/0,
                                      ctx.roundCount, /*source_id=*/70002, /*soul=*/false,
                                      EffectScope::ON_STAGE, /*source_slot=*/-1,
                                      /*counts=*/1);
        ctx.abnormal_status_end_round[1][static_cast<std::size_t>(AbnormalStatusId::Paralysis)] =
            ctx.roundCount + 9;   // 对手麻痹：1257 不吸（隔离吸体对换血前提的干扰）
        drive_round(b, 0, 0);
        CHECK_EQ(ctx.ws.resolvedDamage.final, 0, "TG 前提：攻击伤害被盾吸满（final=0）");
        CHECK_EQ(ctx.getPet(0).hp, 5000, "TG 核心：换血后自身=对手体力 5000");
        CHECK_EQ(ctx.getPet(1).hp, 3000, "TG 核心：换血后对手=自身体力 3000");
        std::cout << "  [TG] 零伤换血 通过\n";
    }

    std::cout << "scenario_270 all passed" << std::endl;
    return 0;
}
