#include <utils/sbs_rand.h>
// K356/2374，5510-5519已alloc：双侧槽与必暴标记、真实红伤、回合窗重算。
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
using Levels=std::array<int,6>;
ElfPet pet(bool marked) {
    SoulMark sm(2374,"雷弧","",EffectArgs(std::vector<int>{1,0}));
    sm.monster_id=4754;
    ElfPet p=make_pet(4754,"必暴增伤探针",{10004,10004,10004,10004,10004},200,300,5000,
                     marked ? sm : SoulMark());
    set_crit_sixteenths(p,0);
    for (auto& skill:p.skills) { skill.must_hit=1; skill.accuracy=100; }
    return p;
}
void fields(const std::vector<Levels>& rounds,const std::vector<bool>& expected,
            int owner=0,bool boss=false,bool own_only=false,bool marked=true) {
    ScenarioBattle b(make_party(pet(marked && owner==0)),make_party(pet(marked && owner==1)));
    b.ctx().is_boss_challenge=boss;
    b.start();
    auto& ctx=b.ctx();
    for (size_t r=0;r<rounds.size();++r) {
        std::array<Levels,2> before{};
        before[own_only ? owner : 1-owner]=rounds[r];
        for (int side=0;side<2;++side)
            for (int slot=0;slot<6;++slot) ctx.ability_levels[side][slot]=before[side][slot];
        ctx.debug_step_mode=true;
        b.submit_operation(0,BattleFsm::ActionType::NONE,-1);
        b.submit_operation(1,BattleFsm::ActionType::NONE,-1);
        ctx.debug_step_mode=false;
        b.step_past(State::BATTLE_ROUND_START);
        CHECK_EQ(ctx.ws.must_crit[owner],expected[r],"自身必暴门精确值");
        CHECK_EQ(ctx.ws.damage_add_pct[owner],expected[r] ? 50.0f : 0.0f,"自身增伤门精确值");
        CHECK_EQ(ctx.ws.must_crit[1-owner],false,"对手必暴不改");
        CHECK_EQ(ctx.ws.damage_add_pct[1-owner],0.0f,"对手增伤不改");
        for (int side=0;side<2;++side)
            for (int slot=0;slot<6;++slot)
                CHECK_EQ(ctx.ability_levels[side][slot],before[side][slot],"两侧六槽前后快照含cap");
        // 当回合已快照：之后清除强化，不应撤回已经生效的当回合效果。
        for (int slot=0;slot<6;++slot) ctx.ability_levels[1-owner][slot]=0;
        b.drive();
        CHECK_EQ(ctx.ws.must_crit[owner],expected[r],"回合内清强化不撤回必暴");
        CHECK_EQ(ctx.ws.damage_add_pct[owner],expected[r] ? 50.0f : 0.0f,"回合内清强化不撤回增伤");
        std::cout<<"owner="<<owner<<" boss="<<boss<<" R"<<r+1<<" pct="<<ctx.ws.damage_add_pct[owner]<<'\n';
    }
}
int attack(bool marked,bool forced_crit,bool boosted_foe,bool boss=false) {
    sbs_srand(5510);
    auto p=pet(marked);
    if (forced_crit) set_crit_sixteenths(p,16);
    ScenarioBattle b(make_party(p),make_party(pet(false)));
    b.ctx().is_boss_challenge=boss;
    b.start();
    auto& ctx=b.ctx();
    if (boosted_foe) ctx.ability_levels[1][4]=1; // 仅速度，不污染本刀防御数值
    const int before=ctx.getPet(1).hp;
    b.submit_operation(0,BattleFsm::ActionType::SELECT_SKILL,0);
    b.submit_operation(1,BattleFsm::ActionType::NONE,-1);
    b.drive();
    CHECK_EQ(ctx.crit_happened[0],forced_crit || (marked && boosted_foe && !boss),"真实攻击必暴而非只看申报槽");
    return before-ctx.getPet(1).hp;
}
}  // namespace
int main() {
    if (!PetFactory::initialize_runtime_data()) return 1;
    fields({{0,0,6,0,6,0},{0,0,0,0,0,0},{0,0,0,0,0,1}},{true,false,true});
    fields({{6,6,6,6,6,6}},{true});
    fields({{-1,-1,-1,-1,-1,-1}},{false});
    fields({{0,-2,0,1,0,0}},{true},1);
    fields({{6,6,6,6,6,6}},{false},0,true);
    fields({{6,6,6,6,6,6}},{false},1,true);
    fields({{6,6,6,6,6,6}},{false},0,false,true);
    fields({{6,6,6,6,6,6}},{false},0,false,false,false);
    const int baseline=attack(false,true,true), boosted=attack(true,false,true);
    CHECK(baseline>0,"真实攻击非零");
    CHECK(boosted*100>=baseline*145 && boosted*100<=baseline*155,"同为暴击时额外50%加算增伤");
    const int inactive=attack(true,false,false), boss=attack(true,false,true,true);
    CHECK(inactive>0 && boss>0,"门闭仍正常攻击");
    std::cout<<"real-damage crit-baseline="<<baseline<<" boosted="<<boosted
             <<" gate-off="<<inactive<<" boss="<<boss<<'\n';
    std::cout<<"scenario_5510 PASS (K356 2374 对手强化门必暴增伤)\n";
    return 0;
}
