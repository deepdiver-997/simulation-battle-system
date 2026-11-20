#include <utils/sbs_rand.h>
// 场景 340：百罗鬼帝 4444 五技 蔑天行·戾杀 38384（726/820/1843/2469/2472）
// （2026-09-28，night-3 / D18；实现 resources/moves_lib/d5_D18.cpp）
//
// T1 726：击杀隔离 820 → R2/R3 对手先手攻击被无效、R4 恢复。
// T2 820：未击败 → R2/R3 先制 +1。
// T3 1843：伤害 >250 → 100% 害怕。
// T4 2469 满四道：消耗 4 层 → 回血满格 + PP 全恢复 + 四杀鬼囹标记。
// T5 2469 两道：+50000 回血、无标记。
// T6 2472：首炸背包 4 点；死宠未受到 → 二炸 64 点（倍率 ×16）。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/state.h>

#include "../../resources/bailuo_runtime.h"

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kHit = 10004;      // 拍打
constexpr int kLiSha = 38384;    // 蔑天行·戾杀：[726,2469,2472,1843,820]
constexpr int kGuiZhua = 19215;  // 鬼爪斩：先制+3 攻击（让守方在 T1 当先手方）
constexpr int kSishMarker = 44442000;

ElfPet guidi_pet(int speed, int atk) {
    ElfPet p = scenario::make_pet(4444, "百罗鬼帝", {kLiSha, kHit, kHit, kHit, kHit},
                                  speed, atk, 100000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet(int speed, int skill0 = kHit, int atk = 20) {
    ElfPet p = scenario::make_pet(1, "守方", {skill0, kHit, kHit, kHit, kHit},
                                  speed, /*atk=*/atk, 100000);
    for (Skills& s : p.skills) {
        s.accuracy = 100;
    }
    return p;
}

void one_round(ScenarioBattle& b, int my_skill, int foe_skill = 0) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, my_skill);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_skill);
    b.drive();
}

}  // namespace

int main() {
    sbs_srand(20260928);
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== T1 726：下2回合对手先手攻击被无效（击杀隔离 820） ==========
    {
        // 注：38384 自带静态先制+1（跨回合累积）→ 我方几乎恒先手；
        // 让守方用先制+3 的鬼爪斩当先手攻击方，726 条件才成立。
        ScenarioBattle b(scenario::make_party(guidi_pet(100, 20)),
                         scenario::make_party(foe_pet(100, kGuiZhua)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp = 1;
        one_round(b, 0, 1);   // R1：击杀（守方鬼爪斩落空），726 武装（R2/R3）
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        const int h1 = ctx.getPet(0).hp;
        one_round(b, 1, 0);   // R2：替补鬼爪斩（先手）攻击 → 无效
        CHECK_EQ(ctx.getPet(0).hp, h1, "T1 R2 对手先手攻击被无效");
        one_round(b, 1, 0);   // R3：窗口第二回合仍无效
        CHECK_EQ(ctx.getPet(0).hp, h1, "T1 R3 仍无效");
        one_round(b, 1, 1);   // R4：窗口过期（守方用拍打避免+3 先制干扰断言）
        std::cout << "  [T1] 726 下2回合先手攻击无效 通过\n";
    }

    //========== T2 820 未击败：下2回合先制+1 ==========
    {
        ScenarioBattle b(scenario::make_party(guidi_pet(100, 20)),
                         scenario::make_party(foe_pet(300)));
        b.start();
        BattleContext& ctx = b.ctx();
        one_round(b, 0, 0);   // R1：未击败
        one_round(b, 1, 0);   // R2：先制窗口（38384 静态先制+1 跨回合累积 → 基线 1）
        CHECK_EQ(ctx.ws.preemptive_level[0], 2, "T2 R2 静态+1 与 820 窗口+1 叠加");
        one_round(b, 1, 0);   // R3：窗口第二回合
        CHECK_EQ(ctx.ws.preemptive_level[0], 2, "T2 R3 仍 2");
        std::cout << "  [T2] 820 未击败 → 下2回合先制+1 通过\n";
    }

    //========== T3 1843：伤害 >250 → 100% 害怕 ==========
    {
        ScenarioBattle b(scenario::make_party(guidi_pet(200, 300)),
                         scenario::make_party(foe_pet(100)));
        b.start();
        BattleContext& ctx = b.ctx();
        one_round(b, 0, 0);
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Fear)),
              "T3 伤害高于 250 → 害怕落地");
        std::cout << "  [T3] 1843 阈值害怕 通过\n";
    }

    //========== T4 2469 满四道：回满血 + PP 恢复 + 四杀鬼囹标记 ==========
    {
        ScenarioBattle b(scenario::make_party(guidi_pet(200, 20)),
                         scenario::make_party(foe_pet(100, kHit, 0)));
        b.start();
        BattleContext& ctx = b.ctx();
        for (int k = 0; k < 4; ++k) {
            bailuo::hold_kind(&ctx, 0, k);
        }
        ctx.seerRobot[0].elfPets[0].hp = 50000;
        one_round(b, 0, 0);
        CHECK_EQ(ctx.getPet(0).hp, 100000, "T4 消耗 4 道：4×max/4 回满（回合末）");
        CHECK_EQ(bailuo::kind_count(&ctx, 0), 0, "T4 鬼祰全消耗");
        CHECK_EQ(ctx.getPet(0).skills[0].pp, 4, "T4 五技 PP 回满（4/4，回合末退还）");
        CHECK_EQ(ctx.plugin_storage.contains(kSishMarker) ? 1 : 0, 1,
                 "T4 四杀鬼囹标记已进入");
        std::cout << "  [T4] 2469 满四道消耗 通过\n";
    }

    //========== T5 2469 两道：部分回血、无标记 ==========
    {
        ScenarioBattle b(scenario::make_party(guidi_pet(200, 20)),
                         scenario::make_party(foe_pet(100, kHit, 0)));
        b.start();
        BattleContext& ctx = b.ctx();
        bailuo::hold_kind(&ctx, 0, 0);
        bailuo::hold_kind(&ctx, 0, 1);
        ctx.seerRobot[0].elfPets[0].hp = 40000;
        one_round(b, 0, 0);
        CHECK_EQ(ctx.getPet(0).hp, 89999, "T5 消耗 2 道：2×max/4（39999+50000）");
        CHECK(ctx.plugin_storage.contains(kSishMarker) == false, "T5 未满四道无标记");
        std::cout << "  [T5] 2469 两道消耗 通过\n";
    }

    //========== T6 2472 炸背包：首炸 4 点；死宠未受到 → 二炸 64 点 ==========
    {
        ScenarioBattle b(scenario::make_party(guidi_pet(200, 20)),
                         scenario::make_party(foe_pet(100)));
        b.start();
        BattleContext& ctx = b.ctx();
        one_round(b, 0, 0);   // 首炸：背包 5 只各 -4，无未受到 → 倍率不变
        for (int slot = 1; slot <= 5; ++slot) {
            CHECK_EQ(ctx.seerRobot[1].elfPets[slot].hp, 99996, "T6 首炸背包 -4");
        }
        ctx.seerRobot[1].elfPets[4].hp = 0;
        ctx.seerRobot[1].elfPets[5].hp = 0;   // 两只阵亡 → 二炸后未受到 ×2（倍率→16）
        one_round(b, 0, 0);   // 二炸：本击仍 4 点（倍率影响的是"本场战斗此伤害"）
        for (int slot = 1; slot <= 3; ++slot) {
            CHECK_EQ(ctx.seerRobot[1].elfPets[slot].hp, 99992, "T6 二炸仍 4 点");
        }
        one_round(b, 0, 0);   // 三炸：4×16=64
        CHECK_EQ(ctx.seerRobot[1].elfPets[1].hp, 99928, "T6 三炸 64（99992-64）");
        CHECK_EQ(ctx.seerRobot[1].elfPets[4].hp, 0, "T6 阵亡宠不再受击");
        std::cout << "  [T6] 2472 炸背包倍率 通过\n";
    }

    std::cout << "scenario_340 all passed" << std::endl;
    return 0;
}
