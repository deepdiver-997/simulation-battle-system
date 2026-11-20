// K357/1505，5550-5559已alloc。双防槽前后精确值、双正门、真实暴击管线。
#include <array>
#include <iostream>
#include <vector>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include "scenario_harness.h"
using namespace scenario;
namespace {
using Levels=std::array<int,6>;
ElfPet pet(bool marked,std::vector<int> args) {
    SoulMark sm(1505,"变形者C4","",EffectArgs(std::move(args)));
    sm.monster_id=3913;
    ElfPet p=make_pet(3913,"双防暴率探针",{10004,10004,10004,10004,10004},200,300,5000,
                     marked ? sm : SoulMark());
    set_crit_sixteenths(p,0);
    for (auto& skill:p.skills) { skill.must_hit=1; skill.accuracy=100; }
    return p;
}
void fields(Levels initial,std::vector<int> args,const std::vector<Levels>& expected,
            const std::vector<int>& rates,int owner=0,bool boss=false,bool marked=true) {
    ScenarioBattle b(make_party(pet(marked && owner==0,args)),make_party(pet(marked && owner==1,args)));
    b.ctx().is_boss_challenge=boss;
    b.start();
    auto& ctx=b.ctx();
    const Levels foe={1,-1,2,-2,3,-3};
    for (int slot=0;slot<6;++slot) {
        ctx.ability_levels[owner][slot]=initial[slot];
        ctx.ability_levels[1-owner][slot]=foe[slot];
    }
    for (size_t r=0;r<expected.size();++r) {
        const Levels before=r==0 ? initial : expected[r-1];
        for (int slot=0;slot<6;++slot) CHECK_EQ(ctx.ability_levels[owner][slot],before[slot],"触发前六槽快照");
        ctx.debug_step_mode=true;
        b.submit_operation(0,BattleFsm::ActionType::NONE,-1);
        b.submit_operation(1,BattleFsm::ActionType::NONE,-1);
        ctx.debug_step_mode=false;
        b.step_past(State::BATTLE_ROUND_START);
        for (int slot=0;slot<6;++slot) {
            CHECK_EQ(ctx.ability_levels[owner][slot],expected[r][slot],"触发后六槽含cap精确值");
            CHECK_EQ(ctx.ability_levels[1-owner][slot],foe[slot],"对手六槽不改");
        }
        CHECK_EQ(ctx.ws.crit_rate_add[owner],static_cast<float>(rates[r]),"双防均正后的暴击百分点");
        CHECK_EQ(ctx.ws.crit_rate_add[1-owner],0.0f,"对手暴击份额不改");
        CHECK_EQ(ctx.ws.crit_rate_mod[owner],1.0f,"不误写乘算暴率");
        CHECK_EQ(ctx.ws.must_crit[owner],false,"不误写必暴开关");
        std::cout<<"owner="<<owner<<" boss="<<boss<<" R"<<r+1<<" def="<<ctx.ability_levels[owner][2]
                 <<" spdef="<<ctx.ability_levels[owner][3]<<" crit-add="<<ctx.ws.crit_rate_add[owner]<<'\n';
        b.drive();
    }
}
void attack(int rate,bool expected_crit) {
    ScenarioBattle b(make_party(pet(true,{1,1,rate,1})),make_party(pet(false,{})));
    b.start();
    const int before=b.ctx().getPet(1).hp;
    b.submit_operation(0,BattleFsm::ActionType::SELECT_SKILL,0);
    b.submit_operation(1,BattleFsm::ActionType::NONE,-1);
    b.drive();
    CHECK_EQ(b.ctx().crit_happened[0],expected_crit,"base=0下100百分点必暴、显式0不暴");
    CHECK(b.ctx().getPet(1).hp<before,"真实技能红伤结算");
    std::cout<<"real-crit rate="<<rate<<" happened="<<b.ctx().crit_happened[0]<<'\n';
}
}  // namespace
int main() {
    if (!PetFactory::initialize_runtime_data()) return 1;
    fields({0,0,0,0,0,0},{1,1,50,1},{{0,0,1,1,0,0},{0,0,2,2,0,0},{0,0,3,3,0,0}},{50,50,50});
    fields({0,0,-1,0,0,0},{1,1,50,1},{{0,0,0,1,0,0},{0,0,1,2,0,0}},{0,50});
    fields({0,0,0,-1,0,0},{1,1,50,1},{{0,0,1,0,0,0}},{0});
    fields({6,6,6,6,6,6},{1,1,50,1},{{6,6,6,6,6,6}},{50});
    fields({1,-1,0,0,2,-2},{2,3,75,1},{{1,-1,2,3,2,-2}},{75});
    fields({0,0,0,0,0,0},{0,0,50,1},{{0,0,0,0,0,0}},{0});
    fields({0,0,0,0,0,0},{1,1,0,1},{{0,0,1,1,0,0}},{0});
    fields({0,0,0,0,0,0},{},{{0,0,1,1,0,0}},{50});
    fields({0,0,0,0,0,0},{1,1,50,1},{{0,0,1,1,0,0}},{50},1);
    fields({0,0,1,1,0,0},{1,1,50,1},{{0,0,1,1,0,0}},{0},0,true);
    fields({0,0,1,1,0,0},{1,1,50,1},{{0,0,1,1,0,0}},{0},1,true);
    fields({0,0,1,1,0,0},{1,1,50,1},{{0,0,1,1,0,0}},{0},0,false,false);
    attack(100,true);
    attack(0,false);
    std::cout<<"scenario_5550 PASS (K357 1505 双防提升暴率)\n";
    return 0;
}
