#include <utils/sbs_rand.h>
// 场景 950 —— K3 比例反弹（effect 11）："每次受到攻击反馈给对手 1/N 的伤害"
//
// 行口径：2718 究影魔天 db[3,0] → 1/3；无参行 monster 表（2580 斗神瑞尔斯 1/6、
// [1203,1204] 蒙塔迈尔斯 1/8）。反弹量 = 落血 ev.amount / N（整数除），粉伤 FIXED。
// 断言：同一场内读双方损失——攻方总损 = 守方落血 + 守方落血/N。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kYanFan = 10037;   // 燕返：威力 60 必中
constexpr int kGaoSu = 20017;    // 高速移动：属性技

SoulMark make_mark(int effect_id, std::vector<int> db_args, int monster_id) {
    SoulMark sm(effect_id, "K" + std::to_string(effect_id), "test",
                EffectArgs(std::move(db_args)));
    sm.monster_id = monster_id;
    return sm;
}

// 守方（0 方）带魂印挨一击，返回 {守方损失, 攻方损失}。
struct LossPair {
    int defender = 0;
    int attacker = 0;
};

LossPair probe(const SoulMark& mark) {
    ScenarioBattle b(scenario::make_party(scenario::make_pet(
                         1, "受击方", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                         /*speed=*/100, /*atk=*/10, /*hp=*/1000000, mark)),
                     scenario::make_party(scenario::make_pet(
                         2, "进攻方", {kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                         /*speed=*/300, /*atk=*/5000, /*hp=*/1000000)));
    BattleContext& ctx = b.ctx();
    sbs_srand(20260916);
    b.start();
    const int hp0 = ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp;
    const int hp1 = ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp;
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    LossPair r;
    r.defender = hp0 - ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp;
    r.attacker = hp1 - ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp;
    return r;
}

void check_ratio(const SoulMark& mark, int denom, const char* label) {
    const LossPair r = probe(mark);
    CHECK(r.defender > 1000, label);
    // 攻方总损失 = 反弹量（守方落血/N）；攻方不打自己，红伤部分落在守方身上。
    CHECK_EQ(r.attacker, r.defender / denom, label);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ A 有参行 db[3,0]（究影魔天）→ 1/3 ═══════════════════════════════════
    check_ratio(make_mark(11, {3, 0}, 2718), 3, "A 1/3 反弹（db[0]=3）");
    // ══ B 无参行 monster=2580 → 1/6 ════════════════════════════════════════
    check_ratio(make_mark(11, {}, 2580), 6, "B 1/6 反弹（斗神瑞尔斯表查）");
    // ══ C 无参行 monster=1204 → 1/8 ════════════════════════════════════════
    check_ratio(make_mark(11, {}, 1204), 8, "C 1/8 反弹（迈尔斯表查）");

    std::cout << "scenario_950 PASS (K3 11 比例反弹：db[0]/monster 表双源)" << std::endl;
    return 0;
}
