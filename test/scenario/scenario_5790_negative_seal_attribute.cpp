// 场景 5790 —— J248 降态封属窗族（1438，moves_lib/j_negative_seal_attribute.cpp）
//（2026-10-22，worktree feat/jbatch；号段 5790-5799）
//
// 断言口径（精确槽位 + 窗界）：
//   - 载体 35891 龙舞·逆 [1009,1438]|[1,2]：1009 清对手**正**等级（对负态无操作），
//     1438 取 [2] → 2 回合属性封。
//   - 对手属性技 = 20017 高速移动（自身速度 +2），观测对方速度槽（4）。
//   - T5790.a：对手预置防槽 -1 → R1 封属 → R2 属性技无效（速度仍 0）→ R3 窗过期
//     （duration={0}=2 → R1-R2 有效）属性技生效（速度 +2）。
//   - T5790.b：对手无等级 → 门闭不封 → R2 属性技生效（速度 +2）。
//   - T5790.c：对手只有正等级（攻 +2）→ 仍不满足"能力下降" → R2 属性技生效。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/seer-robot.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <fsm/state.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa   = 10004;   // 拍打：物 40
constexpr int kSeal    = 35891;   // [1009,1438]|[1,2] 龙舞·逆：物 80
constexpr int kSealAlt = 38113;   // [1684,1478,1438]|[2,1,2] 乍暖雪：特 150
                                  // （1684 锁降态/1478 锁切，均不制造负等级）
constexpr int kAttrUp  = 20017;   // 高速移动：自身速度+2（属性技）
constexpr int kMaxHp   = 99999;

ElfPet mk_self(int carrier = kSeal) {
    ElfPet p = make_pet(1, "己方", {carrier, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/300, /*hp=*/kMaxHp);
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
    return p;
}

ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kAttrUp, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/kMaxHp);
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
    return p;
}

void round_with(ScenarioBattle& b, int self_slot, int foe_slot) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, self_slot);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_slot);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5790.a 对手防槽 -1 → 封属窗 R1-R2 ══
    {
        ScenarioBattle b(make_party(mk_self()), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][2] = -1;   // 2=防御：负态门
        round_with(b, 0, 1);             // R1：施放 1438（对手用拍打）
        CHECK_EQ(ctx.ability_levels[1][4], 0, "T5790.a R1 对手速度仍 0");
        round_with(b, 1, 0);             // R2：对手属性技 → 被封
        std::cout << "  [T5790.a] R2 foe speed=" << ctx.ability_levels[1][4] << std::endl;
        CHECK_EQ(ctx.ability_levels[1][4], 0, "T5790.a R2 属性技无效（速度仍 0）");
        round_with(b, 1, 0);             // R3：窗过期 → 属性技生效
        std::cout << "  [T5790.a] R3 foe speed=" << ctx.ability_levels[1][4] << std::endl;
        CHECK_EQ(ctx.ability_levels[1][4], 2, "T5790.a R3 窗过期 → 速度 +2");
    }

    // ══ T5790.b 对手无等级 → 门闭不封 ══
    {
        ScenarioBattle b(make_party(mk_self()), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        round_with(b, 0, 1);   // R1：1438 门闭（对手无负态）
        round_with(b, 1, 0);   // R2：属性技正常生效
        std::cout << "  [T5790.b] R2 foe speed=" << ctx.ability_levels[1][4] << std::endl;
        CHECK_EQ(ctx.ability_levels[1][4], 2, "T5790.b 无负态 → 不封，属性技生效 +2");
    }

    // ══ T5790.c 对手只有正等级 → 门闭不封 ══
    // 用 38113 [1684,1478,1438]：1684/1478 不制造负等级，1438 门闭。
    // （35891 的 1009 会在"消除成功"后全属性-1，自己制造负态——不能拿来做正等级对照；
    //   38400 的 1726 在吸取后同样会走"不处于提升"分支全属性-1。）
    {
        ScenarioBattle b(make_party(mk_self(kSealAlt)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][0] = 2;   // 0=攻击：正等级
        round_with(b, 0, 1);
        round_with(b, 1, 0);
        std::cout << "  [T5790.c] R2 foe speed=" << ctx.ability_levels[1][4] << std::endl;
        CHECK_EQ(ctx.ability_levels[1][4], 2, "T5790.c 只有正等级 → 不封，属性技生效 +2");
    }

    std::cout << "scenario_5790 PASS (1438 降态封属窗)" << std::endl;
    return 0;
}
