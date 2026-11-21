// 场景 032：圣灵谱尼 1038 七刻印之「轮回」——致死残留1体力 + 回合结束后状态回档。
//
// 官方（effect_icon id=753）："轮回：对手出手回合若自身受到致命伤害则残留1点体力，
//   同时回合结束后自身体力、PP值、能力等级返回当回合选择操作之后的状态（每场战斗仅触发1次）"
//
// 实现（soul_lib 1038 程序，快照存 pet.soulmark_storage 的 std::any，不加 context 字段）：
//   ① OPERATION_ENTER_EXIT_STAGE  arm EVENT_TAKE_DAMAGE 监控（TEAM 永久，只 arm 一次）
//   ② OPERATION_PROTECTION_MECHANISM_1  快照（紧跟 CHOOSE 之后 = "选择操作之后"）
//   ③ BATTLE_ROUND_END  回档（体力/PP/能力等级）
//
// 验证点：
//   R1 致死一击当刻：体力残留 1（未死亡）+ 对手的降能力照常生效
//   R2 出手后（回档前）：PP 已实际消耗（为回档提供对照）
//   R3 回合结束后：体力/PP/能力等级**全部回到选技后**（降能力与 PP 消耗被回撤）
//   R4 每场仅 1 次：第 2 回合再受致命伤 → 正常阵亡，且回档**不复活**
#include <cassert>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/state.h>

#include "scenario_harness.h"

namespace {

constexpr int kZhuangJi = 10001;            // 撞击：普通 power 35，PP 35（有限，可验证 PP 回档）
constexpr int kFengXianZhiHuo = 31269;      // 奉献之火：120 特攻 + 100% 令对手攻击/特攻 -2
constexpr int kPuniMarkId = 1038;
constexpr int kAtkStat = static_cast<int>(NumericalPropertyIndex::PHYSICAL_ATTACK);

// 圣灵谱尼：50 体力（保证被一击打死，才能触发"受到致命伤害"）。
// 魂印 1038 的程序由 soul_lib 注册，构造 SoulMark 时从 SoulMarkManager 取到。
ElfPet make_puni() {
    return scenario::make_pet(5000, "圣灵谱尼",
                              {kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                              /*speed=*/100, /*atk=*/100, /*hp=*/50,
                              SoulMark(kPuniMarkId, "圣灵谱尼", "七刻印"));
}

// 高速高攻对手：先手，一发奉献之火既降能力又致命。
ElfPet make_attacker() {
    ElfPet pet = scenario::make_pet(1, "对手",
                                    {kFengXianZhiHuo, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                    /*speed=*/300, /*atk=*/200, /*hp=*/4000);
    pet.skills[0].must_hit = true;  // 保证命中，专注验证 轮回
    return pet;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    auto puni = make_puni();
    auto attacker = make_attacker();
    scenario::ScenarioBattle battle(scenario::make_party(puni), scenario::make_party(attacker));
    battle.start();
    BattleContext& ctx = battle.ctx();

    const int hp0 = ctx.getPet(0).hp;
    const int pp0 = ctx.getPet(0).skills[0].pp;
    CHECK_EQ(ctx.getPet(0).levels[kAtkStat], 0, "R0) 开局能力等级应为 0");

    // 提交双方操作；用 debug_step_mode 让 op1 只推进一个状态，从而能停在回合中途观察。
    battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    ctx.debug_step_mode = true;
    battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);

    //========== R1 致死当刻：残留 1 点体力（未死亡）；圣洁免弱挡住对手的降能力 ==========
    battle.step_past(State::BATTLE_FIRST_ATTACK_DAMAGE);
    CHECK_EQ(ctx.getPet(0).hp, 1, "R1) 受到致命伤害应残留 1 点体力（不死锁血）");
    // 七刻印「圣洁」免疫能力下降 → 奉献之火的攻击-2 落不下来（免弱接线后新行为）
    CHECK_EQ(ctx.getPet(0).levels[kAtkStat], 0, "R1) 圣洁免弱 → 对手的降能力不生效");
    // 绕过免弱直接改本体（模拟"自身下降"这类不受免弱约束的等级变化），
    // 用来验证回合结束的**等级回档**本身。
    ctx.getPet(0).levels[kAtkStat] = -2;
    ctx.ws.view_levels[0][kAtkStat] = -2;
    std::cout << "  [R1] 致死当刻: 体力=" << ctx.getPet(0).hp
              << " 攻击等级=" << ctx.getPet(0).levels[kAtkStat]
              << "（残留1点，未死亡；降能力被圣洁免弱挡下）\n";

    //========== R2 出手后（回档前）：PP 已被消耗 —— 为"回档确实发生"提供对照 ==========
    battle.step_past(State::BATTLE_SECOND_AFTER_ACTION);
    CHECK_EQ(ctx.getPet(0).skills[0].pp, pp0 - 1, "R2) 谱尼出手后 PP 应已消耗 1 点（回档前的对照）");
    std::cout << "  [R2] 出手后(回档前): 撞击PP=" << ctx.getPet(0).skills[0].pp << "（原 " << pp0
              << "，已消耗 1 点）\n";

    //========== R3 回合结束后：体力/PP/能力等级回档到"选择操作之后" ==========
    battle.drive();
    CHECK_EQ(ctx.getPet(0).hp, hp0, "R3) 回合结束后体力应回档到选技后");
    CHECK_EQ(ctx.getPet(0).levels[kAtkStat], 0, "R3) 回合结束后能力等级应回档到选技后（-2 → 0）");
    CHECK_EQ(ctx.getPet(0).skills[0].pp, pp0, "R3) 回合结束后 PP 应回档（消耗被回撤）");
    std::cout << "  [R3] 回合结束后: 体力=" << ctx.getPet(0).hp << "（原 " << hp0
              << "） 攻击等级=" << ctx.getPet(0).levels[kAtkStat]
              << " 撞击PP=" << ctx.getPet(0).skills[0].pp << "（原 " << pp0 << "）\n";

    //========== R4 虚无：第 2 回合的攻击被"免疫下1次技能伤害"挡下 ==========
    // （七刻印「虚无」第 1 回合受击后授予次数免伤 → 第 2 回合那次致命攻击被完全挡下）
    const int hp_r2_before = ctx.getPet(0).hp;
    battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    battle.drive();
    CHECK_EQ(ctx.seerRobot[0].elfPets[0].hp, hp_r2_before,
             "R4) 虚无：第2回合的攻击伤害被免伤挡下（体力不变）");
    std::cout << "  [R4] 虚无挡下第2回合攻击: 体力保持 " << hp_r2_before << "\n";

    //========== R5 免死每场仅 1 次：免死已消耗 → 再受致命伤正常阵亡，回档不复活 ==========
    // 用**真实伤害**（直通护盾/护罩、不经攻击管线）绕过虚无的次数免伤，制造本回合内的致命伤。
    battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    ctx.debug_step_mode = true;
    battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    battle.step_past(State::BATTLE_FIRST_ATTACK_DAMAGE);
    deal_damage(&ctx, /*target=*/0, /*amount=*/9999, DamageKind::TRUE, /*actor=*/1);
    CHECK_EQ(ctx.seerRobot[0].elfPets[0].hp, 0, "R5) 免死已消耗 → 致命伤正常击杀（不再残留1点）");
    battle.drive();
    CHECK_EQ(ctx.seerRobot[0].elfPets[0].hp, 0, "R5) 回合结束的回档**不复活**已阵亡的谱尼");
    std::cout << "  [R5] 免死仅 1 次: 第3回合真伤击杀 → 体力 " << ctx.seerRobot[0].elfPets[0].hp
              << "（回档不复活）\n";

    std::cout << "\n场景 032 通过：轮回 = 致死残留1体力 + 回合结束后 体力/PP/能力等级 "
                 "回档到\"当回合选择操作之后\"（每场仅1次）；虚无免伤挡下后续攻击。\n";
    return 0;
}
