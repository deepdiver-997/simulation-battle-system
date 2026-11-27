// 场景 2150 —— J121 阈值门族二（473/503，moves_lib/j_threshold_gate2.cpp）
//（2026-10-03，worktree ../agents/j121；号段 2150-2159 → T2150 一块六例）
//
// 断言口径（伤害公式 ((2L/5+2)×P×A/D)/50，L=100 → 0.84×P×A/D，D=100）：
//   - 门槛两侧用攻击面板摆：atk=1 → 红伤 1~2（恒 < 全部阈值 100~300，门触发）；
//     atk=3000 → 红伤 ~4032（恒 ≥300，门不触发）；atk=250×P140 → [249,294]
//     跨过 200 且与"+150 误触发"带 [399,444] 不相交（负向判别）；
//   - 473 三码位钉官方码序→槽映射：code0→攻(14107)、code4→速(15329)、
//     code2→**特攻**(16825——16825 链上 689=高伤回血，atk=1 时 dormant，
//     即便日后激活也不碰能力等级，断言稳)；code2 例同时钉防槽不动（防/特攻
//     互换方向）；
//   - 503 命中侧：foe_loss = 红伤 1~2 + ROUND_END 固定 200 → [201,202] 精确带；
//     负侧 17529：foe_loss 留在红伤带内（无 +150）。
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

// ── 载体技（切片已逐条与 side_effect/arg 核对，全 prio=0）──
constexpr int kFumo   = 14107;  // 伏魔天威阵 [473]|[300,0,2]   物160（code0=攻+2）
constexpr int kZijin  = 15329;  // 紫金狂战击 [473]|[300,4,1]   物150（code4=速+1）
constexpr int kChilie = 16825;  // 炽烈炫光破 [473,689]|[300,2,1,300,4] 特160（code2=特攻+1）
constexpr int kJingTian = 14563; // 惊天狂雷击 [503]|[100,200]  特150（红伤<100 → 回合末-200）
constexpr int kZongXiang = 17529; // 棕香芳华 [503]|[200,150]   物140（红伤>200 → 不触发）
constexpr int kPaiDa = 10004;   // 拍打：填充位

constexpr int kMaxHp = 99999;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(int carrier, int atk) {
    ElfPet p = make_pet(1, "阈值者", {carrier, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, atk, kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, kMaxHp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T2150.a 473 code0→攻（14107 [300,0,2]，atk=1 红伤 1~2 <300）═════════
    {
        ScenarioBattle b(make_party(self_pet(kFumo, 1)), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        const int foe_loss = kMaxHp - ctx.getPet(1).hp;
        CHECK(foe_loss >= 1 && foe_loss <= 3,
              "T2150.a 前置：红伤小带已落地（实测 " + std::to_string(foe_loss) + "）");
        CHECK_EQ(ctx.ability_levels[0][0], 2,
                 "T2150.a 473：伤害不足 → 自身攻击+2（实测 " +
                     std::to_string(ctx.ability_levels[0][0]) + "）");
    }
    // ══ T2150.b 473 code4→速（15329 [300,4,1]）══════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet(kZijin, 1)), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(ctx.ability_levels[0][4], 1,
                 "T2150.b 473：code4 → 速度槽+1（实测 " +
                     std::to_string(ctx.ability_levels[0][4]) + "）");
        CHECK_EQ(ctx.ability_levels[0][0], 0,
                 "T2150.b 473：攻击槽不动（码位不串）");
    }
    // ══ T2150.c 473 门不过（14107，atk=3000 红伤 ~4032 ≥300）════════════════
    {
        ScenarioBattle b(make_party(self_pet(kFumo, 3000)), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        const int foe_loss = kMaxHp - ctx.getPet(1).hp;
        CHECK(foe_loss >= 3000,
              "T2150.c 前置：红伤已越阈值（实测 " + std::to_string(foe_loss) + "）");
        CHECK_EQ(ctx.ability_levels[0][0], 0,
                 "T2150.c 473：伤害足额不触发（实测 " +
                     std::to_string(ctx.ability_levels[0][0]) + "）");
    }
    // ══ T2150.d 473 code2→特攻（16825 [300,2,1]，特攻载体；689 dormant）══════
    {
        ScenarioBattle b(make_party(self_pet(kChilie, 1)), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(ctx.ability_levels[0][1], 1,
                 "T2150.d 473：code2 → 特攻槽+1（官方码序≠引擎槽序，实测 " +
                     std::to_string(ctx.ability_levels[0][1]) + "）");
        CHECK_EQ(ctx.ability_levels[0][2], 0,
                 "T2150.d 473：防御槽不动（防/特攻互换方向钉死）");
    }
    // ══ T2150.e 503 命中侧（14563 [100,200]：红伤 <100 → 回合末对手-200）══
    {
        ScenarioBattle b(make_party(self_pet(kJingTian, 1)), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        const int foe_loss = kMaxHp - ctx.getPet(1).hp;
        const int red = ctx.ws.raw_attack_damage[0];
        CHECK(red > 0 && red < 100, "T2150.e 前置：实际红伤小于阈值 100");
        CHECK_EQ(foe_loss, red + 200, "T2150.e 503：红伤之外精确附加 200");
    }
    // ══ T2150.f 503 门不过（17529 [200,150]：atk=250 → 红伤 [249,294] >200）══
    {
        ScenarioBattle b(make_party(self_pet(kZongXiang, 250)), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        const int foe_loss = kMaxHp - ctx.getPet(1).hp;
        CHECK(foe_loss >= 249 && foe_loss <= 294,
              "T2150.f 503：红伤带内落地（实测 " + std::to_string(foe_loss) + "）");
        CHECK(foe_loss <= 294,
              "T2150.f 503：伤害足额回合末不追加 150（实测 " + std::to_string(foe_loss) + "）");
    }

    std::cout << "scenario_2150 PASS (J121 阈值门族二 473/503)" << std::endl;
    return 0;
}
