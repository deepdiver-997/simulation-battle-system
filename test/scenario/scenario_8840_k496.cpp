// K496 / 8840：1934 己方场下死亡线性增伤；1981因内核缺口不注册、不伪测。
// 工单 〔源: docs_local/docs/05-任务清单/已收官/E组批量效果工单.md:101〕
// 数据来源 https://github.com/SeerAPI/seer-unity-assets （2026-11-23 当前只读库）。
// 八载体全链加载核验（含blocked1981，仅核切片）；1934四真实载体、0..5梯度、
// 双侧先后手、无归因死亡/复活/再死/消逝/额外精灵、残留与死亡拦截、下场回场、
// AMP/AMP_EXTRA/威力差分、行参0/7/100/200、混合伤害与AMP加算、miss/属性负测。
// 不以击败事件计数；场下定义来自正式 count_dead(OFF_FIELD) 契约。
// 〔源: include/primitives/battle_primitives.h:829-836〕
// 〔源: include/effects/spirit_lifecycle.h:64-77〕
// 被动桶只读观察位于技能桶之后、伤害重算之前；红伤事件取实际落地量，生命值留足。
// 〔源: src/fsm/battleContext.cpp:1077-1081〕
// 1934无驻留显示态，检查无伪窗；1981的属性误封/双时钟探针留在build/k496-evidence。
// 〔源: docs/07-工具与测试/效果显示gate规范.md:39〕

#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <entities/pet_factory.h>
#include <plugin/core_api.h>
#include "scenario_harness.h"

using namespace scenario;
namespace {
constexpr int kMove=36553;
constexpr int kHit=10004;
constexpr int kHp=100000;
using Action=BattleFsm::ActionType;
std::array<int,2> amp{},power{},red{},pink{},death{},defeat{};
std::array<bool,2> rewritten{};

EffectResult observe(BattleContext* c,const EffectArgs& a) {
    const int side=a.int_args[0];
    amp[side]=static_cast<int>(c->ws.damage_add_pct[side]);
    power[side]=c->ws.skill_power_view[side].value;
    rewritten[side]=c->ws.skill_power_view[side].rewritten;
    return EffectResult::kOk;
}
void watch(ScenarioBattle& b) {
    auto& c=b.ctx();
    for (int side:{0,1}) for (State st:{State::BATTLE_FIRST_ATTACK_DAMAGE,State::BATTLE_SECOND_ATTACK_DAMAGE}) {
        Effect e(1934,0,side,-1,EffectArgs(std::vector<int>{side}),&observe);
        auto ce=std::make_unique<ContinuousEffect>(e,st,side,-1,c.roundCount);
        ce->source_id_=1934;
        c.passive_effects.register_effect(st,side,std::move(ce),c.round_effect_valid_id[side]);
    }
}
void watch_events(ScenarioBattle& b) {
    auto& c=b.ctx();
    death={0,0}; defeat={0,0};
    for (EventType type:{EventType::EVENT_TAKE_DAMAGE,EventType::EVENT_DEATH,EventType::EVENT_OPPONENT_DEFEATED}) {
        c.event_center_.register_watcher(type,0,c.roundCount,0,false,
            [](BattleContext*,const BattleEvent& e) {
                if(e.type==EventType::EVENT_DEATH && e.target>=0 && e.target<2) ++death[e.target];
                if(e.type==EventType::EVENT_OPPONENT_DEFEATED && e.actor>=0 && e.actor<2) ++defeat[e.actor];
                if(e.type!=EventType::EVENT_TAKE_DAMAGE || e.actor<0 || e.actor>1 || e.target!=1-e.actor) return;
                if(e.damage_kind==static_cast<int>(DamageKind::NORMAL)) red[e.actor]+=e.amount;
                if(e.damage_kind==static_cast<int>(DamageKind::FIXED)) pink[e.actor]+=e.amount;
            },WatcherScope::TEAM,c.watcher_valid_id[0]);
    }
}
int learner(int move) {
    if(move==36470) return 4482;
    if(move==36558) return 4498;
    if(move==36944) return 3705;
    return 4499;
}
ElfPet carrier(int move,int speed,int pct=-1) {
    auto p=make_pet(learner(move),"K496",{move,kHit,10013,20017,kHit},speed,300,kHp);
    if(pct>=0) {
        const auto m=official_data::OfficialDataStore::instance().repository().load_monster(learner(move));
        REQUIRE(m.has_value());
        p.skills[0]=Skills(move,*m,{{move,1934,0,pct}});
    }
    for(auto& s:p.skills) { s.must_hit=1; s.accuracy=100; s.critical_strike_rate=0; s.pp=100; }
    return p;
}
std::unique_ptr<ScenarioBattle> solo(int side=0,bool first=true,int move=kMove,int pct=-1) {
    auto p=carrier(move,first?300:100,pct);
    auto q=carrier(kMove,first?100:300);
    auto b=std::make_unique<ScenarioBattle>(make_party(side==0?p:q),make_party(side==0?q:p));
    b->start(); watch(*b); watch_events(*b);
    return b;
}
void turn(ScenarioBattle& b,int skill0,int skill1) {
    amp={-1,-1}; power={-1,-1}; rewritten={false,false}; red={0,0}; pink={0,0};
    const int r=b.ctx().roundCount;
    b.submit_operation(0,skill0<0?Action::NONE:Action::SELECT_SKILL,std::max(0,skill0));
    b.submit_operation(1,skill1<0?Action::NONE:Action::SELECT_SKILL,std::max(0,skill1));
    b.drive();
    CHECK_EQ(b.ctx().roundCount,r+1,"有界真实FSM单回合");
}
void own_turn(ScenarioBattle& b,int side,int skill,int theirs=-1) {
    turn(b,side==0?skill:theirs,side==1?skill:theirs);
}
void kill_reserve(ScenarioBattle& b,int side,int slot,int actor=-1) {
    REQUIRE(deal_off_field_true_damage(&b.ctx(),side,slot,kHp,actor)==OffFieldTrueDamageResult::DEFEATED);
    CHECK_EQ(b.ctx().seerRobot[side].elfPets[slot].hp,0,"死亡漏斗真实归零");
}
void check_result(ScenarioBattle& b,int side,int expected,int expected_power,const std::string& label,
                  bool exact=true) {
    CHECK_EQ(amp[side],expected,label+" AMP百分比");
    CHECK_EQ(power[side],expected_power,label+" 威力不改写");
    CHECK(!rewritten[side],label+" rewritten不置位");
    for(int value:b.ctx().ws.damage_add_extra_mul[side]) CHECK_EQ(value,0,label+" 不误用AMP_EXTRA");
    const int raw=b.ctx().ws.raw_attack_damage[side];
    if(exact) {
        const double base=0.84*300.0/100.0*expected_power+2.0;
        CHECK(raw>=static_cast<int>(std::floor(base*0.85))-2
                  && raw<=static_cast<int>(std::floor(base))+2,label+" 原始红伤浮动带");
        CHECK_EQ(red[side],static_cast<int>(static_cast<long long>(raw)*(100+expected)/100),
                 label+" NORMAL落地=raw×(100+AMP)%");
    }
}
void cast(ScenarioBattle& b,int side,int expected,const std::string& label,int theirs=-1) {
    own_turn(b,side,0,theirs);
    check_result(b,side,expected,160,label);
}
void no_resident(ScenarioBattle& b) {
    const auto text=b.dump();
    std::size_t pos=0;
    int active=0;
    while((pos=text.find("\"effectId\":1934,",pos))!=std::string::npos) {
        const auto end=text.find('}',pos);
        const auto e=text.substr(pos,end-pos); pos=end+1;
        if(e.find("\"isRoundEffect\":true")!=std::string::npos
            && e.find("\"actionOneShot\":false")!=std::string::npos
            && e.find("\"isExpired\":false")!=std::string::npos
            && e.find("\"invalidated\":false")!=std::string::npos) ++active;
    }
    CHECK_EQ(active,0,"1934 一次性无伪驻留窗/显示债");
    for(const auto& t:b.ctx().rule_center_.entries()) CHECK(t.source_effect_id!=1934,"1934 无伪规则票");
}
EffectResult existing_amp(BattleContext* c,const EffectArgs& a) {
    c->ws.damage_add_pct[a.int_args[0]]+=25; // 场景前置：已有通用AMP，不替被测效果做事。
    return EffectResult::kOk;
}
}

int main() {
    if(!PetFactory::initialize_runtime_data()) return 1;
    sbs_srand(496);
    // A：双表切片，全八载体；1981只核数据，不把缺口宣称已实现。
    {
        const std::vector<std::pair<int,std::vector<std::pair<int,std::vector<int>>>>> want={
            {36470,{{1318,{10,1}},{1013,{1,50}},{1773,{50}},{1934,{50}}}},
            {36553,{{699,{}},{1568,{}},{1934,{20}},{1972,{100}}}},
            {36558,{{1964,{}},{1305,{100,100}},{1934,{30}}}},
            {36944,{{978,{}},{1305,{80,80}},{1934,{20}},{2017,{}}}},
            {29247,{{1414,{30}},{47,{4}},{1932,{4,30}},{1981,{30,1}}}},
            {36579,{{1297,{6,3}},{1981,{16,2}},{444,{}}}},
            {36663,{{191,{3}},{2022,{2,2}},{1981,{16,2}}}},
            {38354,{{1491,{}},{2450,{2}},{2452,{37,1}},{1981,{37,2}},{449,{100,31}}}}
        };
        auto& repo=official_data::OfficialDataStore::instance().repository();
        for(const auto& [move,chain]:want) {
            const auto got=repo.load_skill_effects(move);
            CHECK_EQ(got.size(),chain.size(),"A 载体链长");
            for(std::size_t i=0;i<chain.size();++i) {
                CHECK_EQ(got[i].effect_id,chain[i].first,"A 效果序");
                CHECK(got[i].args==chain[i].second,"A 全链参数零错切");
            }
        }
    }
    // B：双方×先后手×0..5阵亡；不是布尔增伤，也不是累计击败/对方阵亡数。
    for(int side:{0,1}) for(bool first:{true,false}) for(int n=0;n<=5;++n) {
        auto b=solo(side,first);
        for(int slot=1;slot<=n;++slot) kill_reserve(*b,side,slot);
        cast(*b,side,n*20,"B 阵亡梯度"+std::to_string(n),1);
        CHECK_EQ(amp[1-side],0,"B 对手无串账");
        CHECK_EQ(defeat[side],0,"B 己方未击败任何对手也能增伤");
        CHECK_EQ(death[side],n,"B 无归因死亡事件数");
        no_resident(*b);
    }
    // C：真实四载体：20/30/50按行参；36470有秒杀/降防链，只锁AMP与类型，不硬钉raw带。
    for(const auto& row:std::vector<std::array<int,3>>{{36470,50,160},{36553,20,160},{36558,30,150},{36944,20,160}}) {
        auto b=solo(0,true,row[0]);
        b->ctx().grant_immunity(1,ImmunityType::INSTANT_KILL,coverage_all(),0,8,1934,false);
        kill_reserve(*b,0,1); kill_reserve(*b,0,2);
        own_turn(*b,0,0);
        check_result(*b,0,2*row[1],row[2],"C 真实载体"+std::to_string(row[0]),row[0]!=36470);
    }
    // D：对方三只死亡不混进己方；1972粉伤仍是300，1934只放大红伤。
    {
        auto b=solo();
        for(int slot=1;slot<=3;++slot) kill_reserve(*b,1,slot);
        cast(*b,0,0,"D 只死对手");
        CHECK_EQ(pink[0],300,"D 同链1972粉伤不放大");
        kill_reserve(*b,0,1); kill_reserve(*b,0,2);
        cast(*b,0,40,"D 己2对3");
        CHECK_EQ(pink[0],300,"D 红伤提升后粉伤仍不放大");
    }
    // E：实时阵容而非历史计数：无归因死亡→复活→再次死亡→消逝；额外精灵不算场下。
    for(int side:{0,1}) {
        auto b=solo(side);
        kill_reserve(*b,side,1);
        cast(*b,side,20,"E 死亡");
        REQUIRE(revive_pet(&b->ctx(),side,1,100)==100);
        cast(*b,side,0,"E 复活减计数");
        kill_reserve(*b,side,1,1-side);
        cast(*b,side,20,"E 再死仍只算一只");
        REQUIRE(vanish_spirit(&b->ctx(),side,1,1-side)==VanishResult::VANISHED);
        cast(*b,side,0,"E 消逝不计死亡收益");
        b->ctx().extra_spirits[side].push_back({0,ExtraSpiritState::DEAD,1934,side,-1});
        b->ctx().extra_spirits[side].push_back({0,ExtraSpiritState::ALIVE,1934,side,-1});
        cast(*b,side,0,"E 额外精灵非场下");
        REQUIRE(deal_off_field_true_damage(&b->ctx(),side,2,kHp,-1,1)==OffFieldTrueDamageResult::RESIDUE);
        cast(*b,side,0,"E 残留1血非死亡");
        DeathInterceptor interceptor;
        interceptor.source_effect_id=1934; interceptor.owner=side;
        interceptor.fn=[side](BattleContext* c,int s,int slot,int,DefeatCause) {
            if(s!=side || slot!=2) return false;
            c->seerRobot[s].elfPets[slot].hp=1;
            return true;
        };
        register_death_interceptor(&b->ctx(),std::move(interceptor));
        REQUIRE(deal_off_field_true_damage(&b->ctx(),side,2,kHp,-1)==OffFieldTrueDamageResult::DEALT);
        cast(*b,side,0,"E 被死亡拦截的精灵不计");
    }
    // F：行参独立消费与合法零档，不硬写当前库20/30/50，也不是复利。
    for(int pct:{0,7,100,200}) {
        auto b=solo(0,true,kMove,pct);
        for(int slot=1;slot<=3;++slot) kill_reserve(*b,0,slot);
        cast(*b,0,3*pct,"F override步长"+std::to_string(pct));
    }
    // G：双侧同回合各自增伤；下一技能/NONE不继承。已有AMP与1934加算、不覆盖。
    {
        auto b=solo();
        kill_reserve(*b,0,1); kill_reserve(*b,1,1); kill_reserve(*b,1,2);
        turn(*b,0,0);
        check_result(*b,0,20,160,"G 镜像0"); check_result(*b,1,40,160,"G 镜像1");
        turn(*b,1,2);
        check_result(*b,0,0,40,"G 后续物理无残留"); check_result(*b,1,0,40,"G 后续特殊无残留");
        turn(*b,-1,-1);
        CHECK_EQ(static_cast<int>(b->ctx().ws.damage_add_pct[0]),0,"G NONE清空0");
        CHECK_EQ(static_cast<int>(b->ctx().ws.damage_add_pct[1]),0,"G NONE清空1");
    }
    {
        auto b=solo(); kill_reserve(*b,0,1); kill_reserve(*b,0,2); kill_reserve(*b,0,3);
        auto& c=b->ctx();
        Effect e(1934,0,0,-1,EffectArgs(std::vector<int>{0}),&existing_amp);
        auto ce=std::make_unique<ContinuousEffect>(e,State::BATTLE_ROUND_START,0,-1,c.roundCount);
        ce->source_id_=1934;
        c.passive_effects.register_effect(State::BATTLE_ROUND_START,0,std::move(ce),c.round_effect_valid_id[0]);
        cast(*b,0,85,"G 已有25+本次60");
    }
    // H：离场不携带旧AMP；回场/换槽按当前死亡阵容重采样，场下不是固定1..5槽。
    for(int side:{0,1}) {
        auto b=solo(side); kill_reserve(*b,side,5);
        cast(*b,side,20,"H 离场前");
        b->submit_operation(side,Action::CHOOSE_PET,3);
        b->submit_operation(1-side,Action::NONE,0); b->drive();
        CHECK_EQ(b->ctx().on_stage[side],3,"H 真实换到槽3"); watch(*b);
        own_turn(*b,side,1); check_result(*b,side,0,40,"H 换宠后普通攻击无残留");
        cast(*b,side,20,"H 新槽同技能重采样");
        kill_reserve(*b,side,0);
        cast(*b,side,40,"H 原场上槽0死亡现在也算场下");
        no_resident(*b);
    }
    // I：命中节点与攻击类型门。属性不参与红伤阶段，直接检查回合工作区零AMP。
    {
        auto b=solo(); kill_reserve(*b,0,1);
        b->pet(0).skills[0].must_hit=0; b->pet(0).skills[0].accuracy=0;
        own_turn(*b,0,0);
        CHECK_EQ(static_cast<int>(b->ctx().ws.damage_add_pct[0]),0,"I miss不施加AMP");
        CHECK_EQ(red[0],0,"I miss无红伤");
        auto attr=solo(); kill_reserve(*attr,0,1);
        attr->pet(0).skills[0].type=SkillType::Attribute;
        own_turn(*attr,0,0);
        CHECK_EQ(static_cast<int>(attr->ctx().ws.damage_add_pct[0]),0,"I 物化属性类型不增攻击伤害");
        CHECK_EQ(red[0],0,"I 属性无红伤");
        no_resident(*attr);
    }
    std::cout<<"scenario_8840 K496 PASS: 1934 roster/current/AMP; 1981 blocked, not registered\n";
    return 0;
}
