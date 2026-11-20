// 场景 7603 —— K403 effect 1680 环·圣芒（4088，row1330 本体 args[1,1,6,30,40,20,3,1]
// kind[4,11,14] / row2138 进化行 args[2,2,0,40,40,20,3,1] kind[11,14]）
// 「回合开始时，使自身防御+1（进化行+2）、特防+1（进化行+2）；自身受到攻击后，若双防
//  提升等级总和≥6（进化行≥0）则当回合战斗阶段结束时附加自身最大体力值30%（进化行40%）
//  的百分比伤害；自身被击败时…（板行范围外子句，实现侧记档跳过）（boss无效）」
//   —— soul_lib/k403_stack_streak_family.cpp
//
// 断言口径：
//   - 回合能力 = ROUND_START 后六槽等级精确断言（防槽2/特防槽3，K4 直系；行≠宏 +1 vs
//     +2 双行参数化直证）；
//   - 门 = 受击时点 ability_levels[2]+[3] raw 和 vs 门（6/0）：未达 → 战阶末无粉伤；
//     达 → ROUND_END 对手掉血 = 自身 maxHP×30%/40% **精确值**（PERCENT_VALUE 粉伤
//     无浮动——host maxHP 钉 1000 → 300/400，K400 ④ 同口径；4399 异文"固定伤害"不采，
//     dump 行文"百分比伤害"为权威）；
//   - 对照位 = 门未达/未受击两路战阶末均无粉伤（pink 计数不动）；
//   - boss 门 = is_boss_challenge 局不升防不受击结算；
//   - 全部 submit_operation + drive() 有限回合，绝不 while(!FINISHED)。
#include <any>
#include <cstdlib>
#include <iostream>
#include <vector>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kHit = 10004;      // 拍打（物理攻击，priority 0 载体）
constexpr int kHostMaxHp = 1000; // 粉伤基数（30%/40% → 300/400 精确）

// 审计/账本键（与 k403_stack_streak_family.cpp 一致；owner=0）。
constexpr int kPend = 991680010;
constexpr int kPink = 991680020;

const std::vector<int> kRowBody{1, 1, 6, 30, 40, 20, 3, 1};   // row1330 本体（+1/+1 门6 30%）
const std::vector<int> kRowEvo{2, 2, 0, 40, 40, 20, 3, 1};    // row2138 进化行（+2/+2 门0 40%）

SoulMark make_mark(const std::vector<int>& args) {
    SoulMark sm(1680, "K403 环·圣芒", "test", EffectArgs(args));
    sm.monster_id = 4088;
    sm.kind_tags = {4, 11, 14};   // 拟真本体行（进化行 [11,14]；无运行期判别需求）
    return sm;
}

ElfPet mark_pet(const std::vector<int>& args) {
    ElfPet p = make_pet(1, "环·圣芒", {kHit, kHit, kHit, kHit, kHit},
                        /*speed=*/100, /*atk=*/300, kHostMaxHp, make_mark(args));
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kHit, kHit, kHit, kHit, kHit}, /*speed=*/300,
                        /*atk=*/300, /*hp=*/99999);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

void foe_attack_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

void idle_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

int read_key(ScenarioBattle& b, int key) {
    const auto it = b.ctx().plugin_storage.find(key);
    if (it == b.ctx().plugin_storage.end()) {
        return -1;
    }
    const auto* p = std::any_cast<int>(&it->second);
    return p ? *p : -1;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7603a 回合能力档位（row1330）：R1/R2 防特防 +1/+1 逐回合叠 ═════════════
    {
        ScenarioBattle b(make_party(mark_pet(kRowBody)), make_party(foe_pet()));
        b.start();
        idle_round(b);
        CHECK_EQ(b.ctx().ability_levels[0][2], 1, "T7603a R1 防御+1");
        CHECK_EQ(b.ctx().ability_levels[0][3], 1, "T7603a R1 特防+1");
        idle_round(b);
        CHECK_EQ(b.ctx().ability_levels[0][2], 2, "T7603a R2 防御+2（逐回合叠）");
        CHECK_EQ(b.ctx().ability_levels[0][3], 2, "T7603a R2 特防+2");
        std::cout << "  [T7603a] 回合开始防+1特防+1 通过\n";
    }

    // ══ T7603b/c/d 门与战阶末粉伤（row1330）：R1/R2 门未达无粉、R3 达门 300 精确 ═
    {
        ScenarioBattle b(make_party(mark_pet(kRowBody)), make_party(foe_pet()));
        b.start();
        foe_attack_round(b);   // R1：sum=2 < 6
        CHECK_EQ(b.pet(1).hp, 99999, "T7603b R1 门未达无粉伤");
        CHECK_EQ(read_key(b, kPend), -1, "T7603b R1 门未达无结算戳");
        foe_attack_round(b);   // R2：sum=4 < 6
        CHECK_EQ(b.pet(1).hp, 99999, "T7603b R2 门未达无粉伤");
        idle_round(b);         // R3 前先看等级（RS 后 3/3、和=6 达门）
        CHECK_EQ(b.ctx().ability_levels[0][2], 3, "T7603c R3 RS 防御=3");
        CHECK_EQ(b.ctx().ability_levels[0][3], 3, "T7603c R3 RS 特防=3");
        foe_attack_round(b);   // R3：sum=6 ≥ 6 → 战阶末粉伤
        CHECK_EQ(b.pet(1).hp, 99999 - 300, "T7603c 达门战阶末粉伤=自身maxHP×30% 精确");
        CHECK_EQ(read_key(b, kPink), 1, "T7603c 粉伤落地计数=1");
        idle_round(b);         // R4：未受击对照
        CHECK_EQ(b.pet(1).hp, 99999 - 300, "T7603d 未受击不结算");
        CHECK_EQ(read_key(b, kPink), 1, "T7603d 粉伤计数不动");
        std::cout << "  [T7603b/c/d] 门6→300 精确粉伤+两路对照 通过\n";
    }

    // ══ T7603e 行≠宏（row2138）：+2/+2、门0 当回合即达、40% 精确 ═══════════════
    {
        ScenarioBattle b(make_party(mark_pet(kRowEvo)), make_party(foe_pet()));
        b.start();
        foe_attack_round(b);   // R1：RS +2/+2 → sum=4 ≥ 门0 → 即达
        CHECK_EQ(b.ctx().ability_levels[0][2], 2, "T7603e R1 防御+2");
        CHECK_EQ(b.ctx().ability_levels[0][3], 2, "T7603e R1 特防+2");
        CHECK_EQ(b.pet(1).hp, 99999 - 400, "T7603e 进化行粉伤=自身maxHP×40% 精确");
        CHECK_EQ(read_key(b, kPink), 1, "T7603e 粉伤落地计数=1");
        std::cout << "  [T7603e] 行≠宏 门0/40% 档位差 通过\n";
    }

    // ══ T7603f boss 门：不升防、不受击结算 ═════════════════════════════════════
    {
        ScenarioBattle b(make_party(mark_pet(kRowBody)), make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;
        b.start();
        foe_attack_round(b);
        CHECK_EQ(b.ctx().ability_levels[0][2], 0, "T7603f boss 局不升防");
        CHECK_EQ(b.ctx().ability_levels[0][3], 0, "T7603f boss 局不升特防");
        CHECK_EQ(b.pet(1).hp, 99999, "T7603f boss 局无粉伤");
        CHECK_EQ(read_key(b, kPink), -1, "T7603f boss 局无粉伤计数");
        std::cout << "  [T7603f] boss 门 通过\n";
    }

    std::cout << "scenario_7603 全部通过" << std::endl;
    return 0;
}
