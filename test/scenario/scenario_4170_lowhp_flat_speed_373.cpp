#include <utils/sbs_rand.h>
// 场景 4170 —— K261 低血门减伤+后手加速族（373，soul_lib/k261_lowhp_flat_speed.cpp）
//（2026-10-21，worktree ../agents/k261；号段 4170-4179 → T4170-T4172 三块）
//
// 断言口径（373「回合开始时若血量低于对手则受到的伤害减少150点；后出手时速度+1」）：
//   - T4170 子句①：印主血量低于对手 → 回合开始时挂旗 → 受到的红伤**少 150 点**
//     （双局对照：同 seed 一局带印、一局不带，减伤回调零掷点 → rand 流对齐，
//     差值精确 = 150；场景 3200 同款手法）。
//   - T4171 子句①门闭：印主血量**不低于**对手 → 掉血与对照局相等（不减免）。
//   - T4172 子句②：后出手 → 自身速度等级 +1；先出手 → 不加速。
//
// ⚠️ 场景构造口径：按 **DB args 原样**传 {1,150,5,1}（不补 {0,0} 前缀）——实现侧走
//   owned_int_args 原序读（K257 实现头注「行参读取路径」记档）。
#include <cstdlib>
#include <iostream>
#include <vector>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaDa = 10004;    // 拍打：攻击载体（K229/K178 同款载体）
constexpr int kSpeedSlot = 4;  // 能力等级槽 4 = 速度
constexpr int kReduce = 150;    // db[1] = 减伤点数

// 印主：hp=1000（低于对手 99999 → 触发"血量低于对手"门；且 > 减伤点数 150，
// 保证减伤后仍有可见掉血）。
ElfPet mark_pet(int hp, int speed = 300) {
    std::vector<int> args{1, 150, 5, 1};
    ElfPet p = make_pet(1, "印主", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/speed, /*atk=*/1, /*hp=*/hp,
                        /*soul_mark=*/SoulMark(373, "K261 魂印", "",
                                               EffectArgs(std::move(args))));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// 对照：无魂印的印主（同样的 hp）。
ElfPet plain_mark_pet(int hp) {
    ElfPet p = make_pet(1, "印主", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/300, /*atk=*/1, /*hp=*/hp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// 攻方：atk=600 → 拍打红伤 raw ∈ [174,204]（常识速查 §五 红伤浮动锚：
// atk600 → [174,204]）。**必须让 raw > 减伤点数 150**，否则减伤后归零、
// 观测不到 150 的差值（raw<150 时 final 截 0，实测差 = raw-1）。
ElfPet foe_pet(int speed) {
    ElfPet p = make_pet(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/speed, /*atk=*/600, /*hp=*/99999);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// 攻方出手一次（印主 NONE）。
int foe_strike(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    return b.ctx().getPet(0).hp;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T4170 子句①：血量低于对手 → 受伤少 150（双局对照）══════════════════
    {
        // 对照局：无印，印主 hp=1000，被对手拍打一次 → 掉 raw。
        int ctrl_hp = 0;
        {
            ScenarioBattle b(make_party(plain_mark_pet(1000)),
                             make_party(foe_pet(100)));   // 对手速度 100 → 印主先手、对手后手攻击
            b.start();
            sbs_srand(7);   // 钉种子 → 红伤浮动序列可复现（与实验局对齐）
            ctrl_hp = foe_strike(b);
        }
        // 实验局：带印，同样条件 → 掉 raw-150。
        int mark_hp = 0;
        {
            ScenarioBattle b(make_party(mark_pet(1000)),
                             make_party(foe_pet(100)));
            b.start();
            sbs_srand(7);   // 同种子 → 两局红伤 raw 相同，差值 = 减伤点数
            mark_hp = foe_strike(b);
        }
        const int ctrl_loss = 1000 - ctrl_hp;
        const int mark_loss = 1000 - mark_hp;
        CHECK(ctrl_loss > 0, "T4170 对照局前提：对手拍打有伤害");
        CHECK_EQ(ctrl_loss - mark_loss, kReduce,
                 "T4170 373：血量低于对手 → 受到的红伤减少 150 点（双局对照）");
        std::cout << "  [T4170] 低血门减伤 通过（ctrl_loss=" << ctrl_loss
                  << " mark_loss=" << mark_loss << "）\n";
    }

    // ══ T4171 子句①门闭：血量不低于对手 → 不减免 ════════════════════════════
    {
        int ctrl_hp = 0;
        {
            ScenarioBattle b(make_party(plain_mark_pet(99999)),
                             make_party(foe_pet(100)));
            b.start();
            sbs_srand(7);   // 钉种子（与实验局对齐）
            ctrl_hp = foe_strike(b);
        }
        int mark_hp = 0;
        {
            ScenarioBattle b(make_party(mark_pet(99999)),
                             make_party(foe_pet(100)));
            b.start();
            sbs_srand(7);   // 同种子 → 两局 raw 相同
            mark_hp = foe_strike(b);
        }
        CHECK_EQ(ctrl_hp, mark_hp,
                 "T4171 373：血量不低于对手 → 掉血与对照局相等（门闭）");
        std::cout << "  [T4171] 门闭对照 通过\n";
    }

    // ══ T4172 子句②：后出手 → 速度+1；先出手 → 不加速 ═════════════════════
    {
        // 印主速度 300 > 对手 100 → 印主先手 → 不加速。
        ScenarioBattle b(make_party(mark_pet(1000, 300)), make_party(foe_pet(100)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(ctx.ability_levels[0][kSpeedSlot], 0,
                 "T4172 373：先出手 → 自身速度等级不变");
        std::cout << "  [T4172a] 先手对照 通过\n";
    }
    {
        // 印主速度 100 < 对手 300 → 印主后手 → 速度+1。
        ScenarioBattle b(make_party(mark_pet(1000, 100)), make_party(foe_pet(300)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(ctx.ability_levels[0][kSpeedSlot], 1,
                 "T4172 373：后出手 → 自身速度等级 +1");
        std::cout << "  [T4172b] 后手加速 通过\n";
    }

    std::cout << "scenario_4170 PASS (K261 低血门减伤+后手加速族)" << std::endl;
    return 0;
}
