// K355/2381：5460-5469已alloc。五技能逐槽PP、六等级逐槽与异常施加行为。
#include <algorithm>
#include <array>
#include <iostream>
#include <vector>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include "scenario_harness.h"
using namespace scenario;
namespace {
ElfPet pet(bool marked) {
    SoulMark sm(2381,"嗒啦啦","",EffectArgs(std::vector<int>{1,0}));
    sm.monster_id=4760;
    return make_pet(4760,"双免PP探针",{10004,10004,10004,10004,10004},200,100,5000,
                    marked ? sm : SoulMark());
}
void immunity(BattleContext& ctx,int owner,bool active) {
    const std::array<int,6> before={0,1,2,-1,6,-6};
    for (int slot=0;slot<6;++slot) {
        ctx.ability_levels[owner][slot]=before[slot];
        (void)stat_drop(&ctx,owner,slot,2,1-owner);
        CHECK_EQ(ctx.ability_levels[owner][slot],active ? before[slot] : std::max(-6,before[slot]-2),
                 "六槽下降前后精确值，含上下cap");
    }
    const int level_before=ctx.ability_levels[owner][0];
    (void)stat_change(&ctx,owner,0,1);
    CHECK_EQ(ctx.ability_levels[owner][0],level_before+1,"免降不挡自身强化");
    for (int code: {0,1,2,5,6,19}) {
        (void)dispel_active_anomalies(&ctx,owner);
        CHECK(!ctx.has_active_abnormal_status(owner,code),"异常施加前快照");
        const bool applied=try_apply_anomaly(&ctx,owner,code,3,1-owner);
        CHECK_EQ(applied,!active,"现代异常施加成功与否");
        CHECK_EQ(ctx.has_active_abnormal_status(owner,code),!active,"异常槽施加后快照");
    }
    (void)dispel_active_anomalies(&ctx,owner);
}
void run(int owner,bool boss,bool marked,int rounds) {
    ScenarioBattle b(make_party(pet(marked && owner==0)),make_party(pet(marked && owner==1)));
    b.ctx().is_boss_challenge=boss;
    b.start();
    auto& ctx=b.ctx();
    const bool active=marked && !boss;
    immunity(ctx,owner,active);  // 登场即免疫，未等ROUND_START
    immunity(ctx,1-owner,false);
    for (int r=0;r<rounds;++r) {
        const std::array<int,5> before={0,1,34,35,33};
        for (int side=0;side<2;++side)
            for (int slot=0;slot<5;++slot) ctx.getPet(side).skills[slot].pp=before[slot];
        const int hp0=ctx.getPet(0).hp,hp1=ctx.getPet(1).hp;
        ctx.debug_step_mode=true;
        b.submit_operation(0,BattleFsm::ActionType::NONE,-1);
        b.submit_operation(1,BattleFsm::ActionType::NONE,-1);
        ctx.debug_step_mode=false;
        b.step_past(State::BATTLE_ROUND_START);
        for (int slot=0;slot<5;++slot) {
            CHECK_EQ(ctx.getPet(owner).skills[slot].pp,active ? std::min(35,before[slot]+2) : before[slot],
                     "五技能逐槽PP回合首快照含满cap");
            CHECK_EQ(ctx.getPet(1-owner).skills[slot].pp,before[slot],"对手五槽PP不改");
        }
        CHECK_EQ(ctx.getPet(0).hp,hp0,"PP与免疫不改owner0血量");
        CHECK_EQ(ctx.getPet(1).hp,hp1,"PP与免疫不改owner1血量");
        immunity(ctx,owner,active);
        std::cout<<"owner="<<owner<<" boss="<<boss<<" marked="<<marked<<" R"<<r+1
                 <<" pp0="<<ctx.getPet(owner).skills[0].pp<<'\n';
        b.drive();
    }
}
void exit_cleanup() {
    auto party=make_party(pet(true));
    party[1]=pet(false);
    ScenarioBattle b(party,make_party(pet(false)));
    b.start();
    immunity(b.ctx(),0,true);
    b.ctx().on_stage[0]=1;
    b.ctx().invalidate_on_stage_effects(0);
    immunity(b.ctx(),0,false);
}
}  // namespace
int main() {
    if (!PetFactory::initialize_runtime_data()) return 1;
    run(0,false,true,3);
    run(1,false,true,2);
    run(0,true,true,2);
    run(1,true,true,1);
    run(0,false,false,1);
    exit_cleanup();
    std::cout<<"scenario_5460 PASS (K355 2381 双免疫全PP恢复)\n";
    return 0;
}
