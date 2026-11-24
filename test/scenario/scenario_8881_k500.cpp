// K500 / 8881：2181 零参解除自身全部有效异常，两个真实完整载体链。
// 工单 〔源: docs_local/docs/05-任务清单/已收官/E组批量效果工单.md:105〕
// 数据 https://github.com/SeerAPI/seer-unity-assets ，当前双表args_num/arg_count均0。
// 文案 http://news.4399.com/gonglue/seer/tujian/1005181.htm （37997）与
// http://news.4399.com/gonglue/seer/tujian/993813.htm （28731），均无类别限定。
// 正式原语遍历全部有效码0..43；不把解除写成自然到期，不解除对手、不授持续净化。
// 〔源: src/primitives/battle_primitives.cpp:883-896〕
// 37997链2137→2181→3先采样再清除：本场景不再放K493的后继解除桩，锁真实生产链。
// 〔源: resources/moves_lib/k493_anomaly_round_amp.cpp:9-16〕
// 全码控制态覆盖在真实FSM已通过行动门、将进入SKILL_EFFECT时置入Raw异常，随后
// 单步执行原链；另有施放前被控负测，不能把晚置入夹具误当“被控也能用解除技”。
// 其余验双方先后手、同轮镜像、无异常/只有对手异常、遮蔽本体、冻结、等级保留/兄弟3、
// miss/技能白板/被控、重新附加不自动清、离场回场、显示无伪驻留及零私有键。
// 〔源: docs/07-工具与测试/效果显示gate规范.md:34-49〕

#include <array>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <entities/pet_factory.h>
#include <plugin/core_api.h>
#include <primitives/battle_primitives.h>
#include "scenario_harness.h"

using namespace scenario;
namespace {
using Action = BattleFsm::ActionType;
constexpr int kAttack = 37997;
constexpr int kAttribute = 28731;
constexpr int kHp = 100000;

ElfPet carrier(int move,int speed) {
    auto p=make_pet(move==kAttack?4788:4646,"K500-2181",{move,10004,20001,10013,10001},speed,300,kHp);
    for (auto& s:p.skills) { s.must_hit=true; s.accuracy=100; s.critical_strike_rate=0; s.pp=100; }
    return p;
}
struct Fight {
    std::vector<BattleEvent> events;
    std::unique_ptr<ScenarioBattle> b;
    Fight(int side=0,bool first=true,int move=kAttack,bool mirror=false) {
        auto p=carrier(move,first?300:100),q=carrier(mirror?move:kAttack,first?100:300);
        b=std::make_unique<ScenarioBattle>(make_party(side==0?p:q),make_party(side==0?q:p));
        b->start(); ctx().event_center_.set_delivery_sink(&events);
    }
    BattleContext& ctx() { return b->ctx(); }
    ElfPet& pet(int s) { return b->pet(s); }
};
void turn(Fight& f,int s0,int s1) {
    f.events.clear(); const int r=f.ctx().roundCount;
    f.b->submit_operation(0,s0<0?Action::NONE:Action::SELECT_SKILL,std::max(0,s0));
    f.b->submit_operation(1,s1<0?Action::NONE:Action::SELECT_SKILL,std::max(0,s1));
    f.b->drive(); CHECK_EQ(f.ctx().roundCount,r+1,"有界单回合FSM");
}
void own(Fight& f,int side,int skill=0,int other=-1) { turn(f,side==0?skill:other,side==1?skill:other); }
void put(Fight& f,int side,int code,int duration=6) {
    REQUIRE(apply_anomaly_raw(&f.ctx(),side,code,duration,1-side)==ApplyAnomalyResult::SUCCESS);
}
bool active(Fight& f,int side,int code) { return f.ctx().has_active_abnormal_status(side,code); }
int count(Fight& f,int side) {
    int n=0; for (int code=0;code<=kOfficialAbnormalStatusMaxId;++code) if (active(f,side,code)) ++n;
    return n;
}
int expired_events(Fight& f,int side) {
    int n=0; for (const auto& e:f.events) if (e.type==EventType::EVENT_ANOMALY_EXPIRED && e.target==side) ++n;
    return n;
}
int red(Fight& f,int side) {
    int n=0; for (const auto& e:f.events) if (e.type==EventType::EVENT_TAKE_DAMAGE && e.actor==side
        && e.target==1-side && e.damage_kind==static_cast<int>(DamageKind::NORMAL)) n+=e.amount;
    return n;
}
void no_state(Fight& f) {
    for (int side:{0,1}) {
        int n=0;
        for (const auto& [st,per_side]:f.ctx().skills_effects.all()) {
            (void)st;
            for (const auto& [key,e]:per_side[side]) { (void)key;
                if (e->getEffectId()==2181 && !e->isActionOneShot() && e->isRoundEffect()
                    && !e->isExpired(f.ctx().roundCount) && e->valid_id_==f.ctx().round_effect_valid_id[side]) ++n;
            }
        }
        CHECK_EQ(n,0,"2181一次性无伪驻留窗");
    }
    bool used=false;
    for (int id:{1074,2181}) {
        const int base=1000000000+id*10000;
        for (const auto& [k,v]:f.ctx().plugin_storage) { (void)v; if (k>=base && k<base+10000) used=true; }
        for (int side:{0,1}) for (const auto& p:f.ctx().seerRobot[side].elfPets) {
            for (const auto& [k,v]:p.on_stage_storage) { (void)v; if (k>=base && k<base+10000) used=true; }
            for (const auto& [k,v]:p.soulmark_storage) { (void)v; if (k>=base && k<base+10000) used=true; }
        }
    }
    CHECK(!used,"K500不占任何私有键段");
}
void reach(Fight& f,State st) {
    for (int i=0;i<100 && f.b->state()!=st && f.b->state()!=State::FINISHED;++i) f.b->step();
    CHECK(f.b->state()==st,"有界单步到位");
}
void choose(Fight& f,int side) {
    f.events.clear(); f.ctx().debug_step_mode=true;
    f.b->submit_operation(0,side==0?Action::SELECT_SKILL:Action::NONE,0);
    f.b->submit_operation(1,side==1?Action::SELECT_SKILL:Action::NONE,0);
    f.ctx().debug_step_mode=false;
}
void swap(Fight& f,int side,int slot) {
    const int r=f.ctx().roundCount;
    f.b->submit_operation(side,Action::CHOOSE_PET,slot); f.b->submit_operation(1-side,Action::NONE,0);
    f.b->drive(); CHECK_EQ(f.ctx().roundCount,r+1,"真实换宠有界单回合");
    CHECK_EQ(f.ctx().on_stage[side],slot,"换宠到位");
}
}

int main() {
    if (!PetFactory::initialize_runtime_data()) return 1;
    sbs_srand(501);
    // A：解除零参不能吞后继参数；两载体完整链逐位。
    {
        const std::vector<std::pair<int,std::vector<std::pair<int,std::vector<int>>>>> want={
            {28731,{{191,{4}},{2181,{}},{1886,{3,2,100}},{998,{3,2}},{843,{2,2}}}},
            {37997,{{2137,{30,90}},{2181,{}},{3,{}}}}
        };
        auto& repo=official_data::OfficialDataStore::instance().repository();
        for (const auto& [move,chain]:want) {
            const auto got=repo.load_skill_effects(move);
            CHECK_EQ(got.size(),chain.size(),"A 全链长度");
            for (std::size_t i=0;i<chain.size();++i) {
                CHECK_EQ(got[i].effect_id,chain[i].first,"A 节点顺序");
                CHECK(got[i].args==chain[i].second,"A 双表切片，2181零参");
            }
        }
    }
    // B：两条完整生产链×双方×先后手；控制之外多类（弱化1/13+附属43）全清，对手不清。
    for (int move:{kAttack,kAttribute}) for (int side:{0,1}) for (bool first:{true,false}) {
        Fight f(side,first,move);
        for (int code:{1,13,43}) put(f,side,code);
        for (int code:{1,13}) put(f,1-side,code);
        CHECK_EQ(count(f,side),3,"B 自身三类夹具");
        own(f,side,0,first?-1:1);
        CHECK_EQ(count(f,side),0,"B 自身全部解除，不限类别");
        CHECK(active(f,1-side,1) && active(f,1-side,13),"B 不解除对手");
        CHECK_EQ(expired_events(f,side),0,"B 解除不是自然到期");
        if (move==kAttack) CHECK(red(f,side)>0,"B 攻击载体真实伤害");
        else CHECK_EQ(red(f,side),0,"B 属性载体不假造攻击伤害");
        no_state(f);
    }
    // C：逐个合法异常槽；晚置入绕过本轮行动禁用，仅用于测解除范围，不伪称可解控出手。
    for (int side:{0,1}) for (int code=0;code<=kOfficialAbnormalStatusMaxId;++code) {
        Fight f(side,true,kAttack);
        choose(f,side); reach(f,State::BATTLE_FIRST_SKILL_EFFECT);
        put(f,side,code,4); put(f,1-side,code,4);
        CHECK(active(f,side,code) && active(f,1-side,code),"C 两侧本体异常在位 "+std::to_string(code));
        f.events.clear(); f.b->step();
        CHECK(!active(f,side,code),"C 全码解除自身 "+std::to_string(code));
        CHECK(active(f,1-side,code),"C 全码不误清对手 "+std::to_string(code));
        CHECK_EQ(expired_events(f,side),0,"C 解除不伪发自然到期");
        no_state(f);
    }
    // D：37997先2137采样剩余回合，再2181清异常，3清等级下降；不再使用解除测试桩。
    for (int side:{0,1}) {
        Fight f(side,true,kAttack);
        put(f,side,1,2);
        REQUIRE(stat_change(&f.ctx(),side,4,-2)==StatChangeResult::SUCCESS);
        REQUIRE(stat_change(&f.ctx(),side,2,2)==StatChangeResult::SUCCESS);
        choose(f,side); reach(f,State::BATTLE_FIRST_ATTACK_DAMAGE);
        CHECK_EQ(f.ctx().ws.damage_add_pct[side],60,"D 先采样2回合×30%保留本刀");
        CHECK_EQ(count(f,side),0,"D 正式2181已在伤害前解除");
        CHECK_EQ(f.ctx().ability_levels[side][4],0,"D 后继3清自身下降");
        CHECK_EQ(f.ctx().ability_levels[side][2],2,"D 自身提升不被解除异常误清");
        f.b->drive();
        CHECK_EQ(red(f,side),f.ctx().ws.raw_attack_damage[side]*160/100,"D 实伤保留采样加成");
        own(f,side);
        CHECK_EQ(f.ctx().ws.damage_add_pct[side],0,"D 下一刀没有历史异常加成");
        no_state(f);
    }
    // E：无异常和只有对手异常均无副作用；28731兄弟链不清等级，本效果也不能顺手清。
    for (int side:{0,1}) for (bool target_only:{false,true}) {
        Fight f(side,true,kAttribute);
        if (target_only) put(f,1-side,13);
        REQUIRE(stat_change(&f.ctx(),side,0,2)==StatChangeResult::SUCCESS);
        REQUIRE(stat_change(&f.ctx(),side,4,-1)==StatChangeResult::SUCCESS);
        own(f,side);
        CHECK_EQ(count(f,side),0,"E 无异常自然空操作");
        CHECK_EQ(active(f,1-side,13),target_only,"E 不处理对手异常");
        CHECK_EQ(f.ctx().ability_levels[side][0],2,"E 不清正等级");
        CHECK_EQ(f.ctx().ability_levels[side][4],-1,"E 不清负等级");
        no_state(f);
    }
    // F：遮蔽“视为不处于”不阻止本体解除；冻结只拦回合消耗，不拦直接解除。
    for (int mode:{0,1}) {
        Fight f;
        put(f,0,1,4); put(f,0,13,4);
        if (mode==0) core_api().set_anomaly_view_layers(&f.ctx(),2181,0,1);
        else f.ctx().anomaly_rounds_frozen[0]=true;
        own(f,0);
        CHECK_EQ(count(f,0),0,"F 本体解除不受查询遮蔽/冻结妨碍");
        if (mode==0) core_api().set_anomaly_view_layers(&f.ctx(),2181,0,0);
        CHECK(!core_api().has_active_anomaly(&f.ctx(),0,-1),"F 解除遮蔽后仍无异常");
    }
    // G：真正的miss、命中效果白板、施放前被控都不能执行解除。
    for (int mode=0;mode<3;++mode) {
        Fight f; put(f,0,1,5);
        if (mode==0) { f.pet(0).skills[0].must_hit=false; f.pet(0).skills[0].accuracy=0; }
        if (mode==1) core_api().hit_effect_invalid(&f.ctx(),1,HitInvalidMode::kFullNull,1,false,2181);
        if (mode==2) put(f,0,6,2);
        own(f,0);
        CHECK(active(f,0,1),"G 未成功执行效果不能清异常");
        CHECK_EQ(red(f,0),0,"G 无攻击伤害");
        no_state(f);
    }
    // H：清一次不是持续净化；重新附加后普通攻击/NONE不自动解除，再用载体才解除。
    for (int side:{0,1}) {
        Fight f(side);
        put(f,side,1,8); own(f,side); CHECK_EQ(count(f,side),0,"H 首次解除");
        put(f,side,13,8); own(f,side,1); CHECK(active(f,side,13),"H 普攻不自动净化");
        own(f,side,-1); CHECK(active(f,side,13),"H NONE不自动净化");
        own(f,side); CHECK_EQ(count(f,side),0,"H 重新施放清新异常");
        no_state(f);
    }
    // I：同回合双侧完整链镜像，双方各清自己，采样的30/60不相串。
    for (bool first:{true,false}) {
        Fight f(0,first,kAttack,true);
        put(f,0,1,1); put(f,1,1,2);
        turn(f,0,0);
        CHECK_EQ(count(f,0),0,"I 镜像side0解除"); CHECK_EQ(count(f,1),0,"I 镜像side1解除");
        CHECK_EQ(f.ctx().ws.damage_add_pct[0],30,"I side0独立采样");
        CHECK_EQ(f.ctx().ws.damage_add_pct[1],60,"I side1独立采样");
        no_state(f);
    }
    // J：离场后无净化监听遗留；回场再中异常，普通技能保留、载体重新解除。
    for (int side:{0,1}) {
        Fight f(side); put(f,side,1); own(f,side);
        swap(f,side,1); put(f,side,13,8); own(f,side,1);
        CHECK(active(f,side,13),"J 替补不继承持续净化");
        swap(f,side,0); put(f,side,13,8); own(f,side,1);
        CHECK(active(f,side,13),"J 回场不自动净化");
        own(f,side); CHECK_EQ(count(f,side),0,"J 回场重新施放有效"); no_state(f);
    }
    std::cout << "scenario_8881_k500 PASS (2181 self cleanse, complete chains and all anomaly slots)\n";
    return 0;
}
