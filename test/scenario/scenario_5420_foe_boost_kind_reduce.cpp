#include <utils/sbs_rand.h>
// K354 / 1496：5420-5429已alloc。减伤槽精确断言＋双侧六槽快照＋真实红伤。
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
    SoulMark sm(1496,"护卫者P2","",EffectArgs(std::move(args)));
    sm.monster_id=3906;
    ElfPet p=make_pet(3906,"减伤探针",{10004,10004,10004,10004,10004},speed,300,5000,
                     marked ? sm : SoulMark());
    for (auto& skill:p.skills) { skill.must_hit=1; skill.accuracy=100; }
    return p;
}
void fields(const char* label,const std::vector<Levels>& rounds,std::vector<int> args,
            const std::vector<int>& expected,int owner=0,bool boss=false,
            bool own_only=false,bool marked=true) {
    ScenarioBattle b(make_party(pet(marked && owner==0,args,100)),
                     make_party(pet(marked && owner==1,args,300)));
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
        for (int slot=0;slot<4;++slot) {
            CHECK_EQ(ctx.ws.damage_reduce_add[owner][slot],slot==0 ? expected[r] : 0,
                     "自身减伤槽精确值");
            CHECK_EQ(ctx.ws.damage_reduce_add[1-owner][slot],0,"不改对手减伤槽");
        }
        for (int side=0;side<2;++side)
            for (int slot=0;slot<6;++slot)
                CHECK_EQ(ctx.ability_levels[side][slot],before[side][slot],"两侧六槽前后快照");
        std::cout<<label<<" R"<<r+1<<" reduce="<<ctx.ws.damage_reduce_add[owner][0]<<'\n';
        b.drive();
    }
}
int attack(bool marked) {
    sbs_srand(5420);
    ScenarioBattle b(make_party(pet(marked,{10,0},100)),make_party(pet(false,{},300)));
    b.start();
    auto& ctx=b.ctx();
    ctx.ability_levels[1][2]=1;  // 攻方防御/速度两种提升，不污染攻击面板
    ctx.ability_levels[1][4]=1;
    const int before=ctx.getPet(0).hp;
    b.submit_operation(0,BattleFsm::ActionType::NONE,-1);
    b.submit_operation(1,BattleFsm::ActionType::SELECT_SKILL,0);
    b.drive();
    return before-ctx.getPet(0).hp;
}
}  // namespace
int main() {
    if (!PetFactory::initialize_runtime_data()) return 1;
    fields("kinds-not-sum/reset",{{0,0,6,0,6,0},{0,0,0,0,1,0},{0,0,0,0,0,0}},{10,0},{20,10,0});
    fields("all-six-at-cap",{{6,6,6,6,6,6}},{10,0},{60});
    fields("mixed",{{1,-2,0,6,-6,1}},{10,0},{30});
    fields("negative",{{-1,-1,-1,-1,-1,-1}},{10,0},{0});
    fields("accuracy-kind",{{0,0,0,0,0,1}},{10,0},{10});
    fields("raw-db17",{{0,0,1,0,1,0}},{17,0},{34});
    fields("zero",{{1,1,1,1,1,1}},{0,0},{0});
    fields("missing",{{0,0,1,0,1,0}},{},{20});
    fields("owner1-boss",{{0,0,1,0,1,0}},{10,0},{20},1,true);
    fields("own-boost-not-foe",{{6,6,6,6,6,6}},{10,0},{0},0,false,true);
    fields("no-mark",{{0,0,1,0,1,0}},{10,0},{0},0,false,false,false);
    const int raw=attack(false), reduced=attack(true);
    CHECK(raw>0,"真实技能驱动红伤管线");
    CHECK(reduced*100>=raw*75 && reduced*100<=raw*85,"两种提升承伤比约0.8");
    std::cout<<"real-damage raw="<<raw<<" reduced="<<reduced<<'\n';
    std::cout<<"scenario_5420 PASS (K354 1496 对手提升种类减伤)\n";
    return 0;
}
