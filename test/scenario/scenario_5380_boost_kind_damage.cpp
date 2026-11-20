#include <utils/sbs_rand.h>
// K353 / 1493：5380-5389已alloc。ROUND_START字段与逐槽快照＋真实技能伤害。
#include <array>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include "scenario_harness.h"
using namespace scenario;
namespace {
using Levels = std::array<int,6>;
ElfPet pet(bool marked, std::vector<int> args, int speed) {
    SoulMark sm(1493, "巡逻者T1", "", EffectArgs(std::move(args)));
    sm.monster_id = 3904;
    ElfPet p = make_pet(3904, "种类增伤探针", {10004,10004,10004,10004,10004},
                        speed, 300, 5000, marked ? sm : SoulMark());
    for (auto& skill : p.skills) { skill.must_hit = 1; skill.accuracy = 100; }
    return p;
}
void fields(const char* label, const std::vector<Levels>& rounds,
            std::vector<int> args, const std::vector<int>& expected,
            int owner = 0, bool boss = false, bool marked = true) {
    ScenarioBattle b(make_party(pet(marked && owner==0, args, 300)),
                     make_party(pet(marked && owner==1, args, 100)));
    b.ctx().is_boss_challenge = boss;
    b.start();
    auto& ctx = b.ctx();
    for (size_t r = 0; r < rounds.size(); ++r) {
        for (int slot=0; slot<6; ++slot) ctx.ability_levels[owner][slot]=rounds[r][slot];
        const Levels before = rounds[r];
        ctx.debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        ctx.debug_step_mode = false;
        b.step_past(State::BATTLE_ROUND_START);
        CHECK_EQ(ctx.ws.damage_add_pct[owner], static_cast<float>(expected[r]), "按正等级种类申报百分数");
        CHECK_EQ(ctx.ws.damage_add_pct[1-owner], 0.0f, "不改对手增伤槽");
        for (int slot=0; slot<6; ++slot)
            CHECK_EQ(ctx.ability_levels[owner][slot], before[slot], "六槽前后快照，满cap也保持原值");
        std::cout << label << " R" << r+1 << " pct=" << ctx.ws.damage_add_pct[owner] << '\n';
        b.drive();
    }
}
int attack(bool marked) {
    sbs_srand(5380);
    ScenarioBattle b(make_party(pet(marked,{10,0},300)), make_party(pet(false,{},100)));
    b.start();
    auto& ctx=b.ctx();
    ctx.ability_levels[0][2]=1;  // 防御和速度不抬高本次攻击本体面板
    ctx.ability_levels[0][4]=1;
    const int before=ctx.getPet(1).hp;
    b.submit_operation(0,BattleFsm::ActionType::SELECT_SKILL,0);
    b.submit_operation(1,BattleFsm::ActionType::NONE,-1);
    b.drive();
    return before-ctx.getPet(1).hp;
}
}  // namespace
int main() {
    if (!PetFactory::initialize_runtime_data()) return 1;
    fields("kinds-not-level-sum/reset", {{0,0,6,0,6,0},{0,0,0,0,1,0},{0,0,0,0,0,0}}, {10,0}, {20,10,0});
    fields("all-six-at-cap", {{6,6,6,6,6,6}}, {10,0}, {60});
    fields("mixed-positive-negative", {{1,-2,0,6,-6,1}}, {10,0}, {30});
    fields("only-negative", {{-1,-1,-1,-1,-1,-1}}, {10,0}, {0});
    fields("accuracy-is-a-kind", {{0,0,0,0,0,1}}, {10,0}, {10});
    fields("raw-db17", {{0,0,1,0,1,0}}, {17,0}, {34});
    fields("explicit-zero", {{1,1,1,1,1,1}}, {0,0}, {0});
    fields("missing-args", {{0,0,1,0,1,0}}, {}, {20});
    fields("owner1-boss", {{0,0,1,0,1,0}}, {10,0}, {20}, 1, true);
    fields("no-mark", {{0,0,1,0,1,0}}, {10,0}, {0}, 0, false, false);
    const int raw=attack(false), boosted=attack(true);
    CHECK(raw>0, "真实技能造成红伤");
    CHECK(boosted*100>=raw*115 && boosted*100<=raw*125, "两种提升的真实红伤比约1.2");
    std::cout << "real-damage raw=" << raw << " boosted=" << boosted << '\n';
    std::cout << "scenario_5380 PASS (K353 1493 正等级种类增伤)\n";
    return 0;
}
