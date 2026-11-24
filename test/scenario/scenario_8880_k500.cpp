// K500 / 8880：1074 高伤门双层申报制异常；真实FSM，不直接调用被测函数。
// 工单 〔源: docs_local/docs/05-任务清单/已收官/E组批量效果工单.md:105〕
// 数据 https://github.com/SeerAPI/seer-unity-assets ，五载体完整链/当前双表实切。
// 34282上下文 http://news.4399.com/gonglue/seer/tujian/916274.htm ：100%主支仍有
// 100%回退，按未落地宽口径建模（免疫/抗性/反弹/禁附加亦回退，非官方实测裁决）。
// 第一层TAKE_DAMAGE实扣与严格>；门闭不武装；第二层NextRounds+armed_round同回合闸。
// 〔源: resources/moves_lib/k498_damage_gate_stat.cpp:84-105〕
// 〔源: resources/moves_lib/k474_lowdmg_fallback_and_purge_delta.cpp:234-244〕
// 〔源: resources/moves_lib/k474_lowdmg_fallback_and_purge_delta.cpp:309-335〕
// 观测为异常/事件/窗口/显示面，不依赖生产审计槽。精确阈值用真实红伤同种子复跑及
// 超杀HP钳制双锚；概率档用独立MINSTD递推+0/100短路+两层闸门，不靠宽统计带冒充。
// 覆盖五载体、两侧先后手、层间参数差分、0/50/80/100、重掷/过期、转换/延长成功、
// 免疫/抗性/反弹/禁令、miss/盔/盾、NONE/属性/被控、换宠/断回合、重施放及同轮镜像。
// fullstate与快照锁1074真实窗，args前六位即模板参数；一次性桥不冒充驻留窗。
// 〔源: web/frontend/src/effectText.js:32-35〕
// 〔源: docs/07-工具与测试/效果显示gate规范.md:34-49〕

#include <array>
#include <cstdint>
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
constexpr int kMove = 30192;
constexpr int kHp = 100000;

int learner(int move) {
    switch (move) {
        case 32405: return 3686;
        case 34014: return 4009;
        case 34282: return 4065;
        case 38370: return 3791;
        default: return 3249;
    }
}
ElfPet carrier(int move, int speed, int atk, const std::vector<int>& params = {}) {
    auto p = make_pet(learner(move), "K500-1074", {move,10004,20001,10013,10001}, speed, atk, kHp);
    if (!params.empty()) {
        REQUIRE(params.size() == 6);
        const auto m = official_data::OfficialDataStore::instance().repository().load_monster(learner(move));
        REQUIRE(m.has_value());
        std::vector<EffectParamOverride> overrides;
        for (int i=0; i<6; ++i) overrides.push_back({move,1074,i,params[i]});
        p.skills[0] = Skills(move,*m,overrides);
    }
    for (auto& s:p.skills) { s.must_hit=true; s.accuracy=100; s.critical_strike_rate=0; s.pp=100; }
    return p;
}
struct Fight {
    std::vector<BattleEvent> events;
    std::unique_ptr<ScenarioBattle> b;
    Fight(int side=0, bool first=true, int move=kMove, int atk=600,
          const std::vector<int>& params={}, bool mirror=false) {
        auto p=carrier(move,first?300:100,atk,params);
        auto q=carrier(kMove,first?100:300,mirror?atk:1,mirror?params:std::vector<int>{});
        b=std::make_unique<ScenarioBattle>(make_party(side==0?p:q),make_party(side==0?q:p));
        b->start();
        ctx().event_center_.set_delivery_sink(&events);
    }
    BattleContext& ctx() { return b->ctx(); }
    ElfPet& pet(int side) { return b->pet(side); }
};
void turn(Fight& f, int s0, int s1, bool complete=true) {
    f.events.clear();
    const int r=f.ctx().roundCount;
    f.b->submit_operation(0,s0<0?Action::NONE:Action::SELECT_SKILL,std::max(0,s0));
    f.b->submit_operation(1,s1<0?Action::NONE:Action::SELECT_SKILL,std::max(0,s1));
    f.b->drive();
    if (complete) CHECK_EQ(f.ctx().roundCount,r+1,"有界单回合FSM");
}
void own(Fight& f, int side, int skill=0, int other=-1, bool complete=true) {
    turn(f,side==0?skill:other,side==1?skill:other,complete);
}
int events(Fight& f, EventType type, int actor, int target, int amount=-1) {
    int n=0;
    for (const auto& e:f.events) if (e.type==type && e.actor==actor && e.target==target
        && (amount<0 || e.amount==amount)) ++n;
    return n;
}
int applied(Fight& f, int side, int code) {
    return events(f,EventType::EVENT_ANOMALY_APPLIED,1-side,side,code);
}
int red(Fight& f, int side) {
    int n=0;
    for (const auto& e:f.events) if (e.type==EventType::EVENT_TAKE_DAMAGE && e.actor==side
        && e.target==1-side && e.damage_kind==static_cast<int>(DamageKind::NORMAL)
        && (e.state==static_cast<int>(State::BATTLE_FIRST_ATTACK_DAMAGE)
            || e.state==static_cast<int>(State::BATTLE_SECOND_ATTACK_DAMAGE))) n+=e.amount;
    return n;
}
bool active(Fight& f, int side, int code) { return core_api().has_active_anomaly(&f.ctx(),side,code); }
void clear(Fight& f, int side) { (void)core_api().dispel_active_anomalies(&f.ctx(),side); }
std::vector<const ContinuousEffect*> windows(Fight& f, int owner) {
    std::vector<const ContinuousEffect*> out;
    for (const auto& [st,sides]:f.ctx().skills_effects.all()) {
        (void)st;
        for (const auto& [key,e]:sides[owner]) {
            (void)key;
            if (e->getEffectId()==1074 && e->isRoundEffect() && !e->isActionOneShot()
                && !e->isExpired(f.ctx().roundCount)
                && (e->scope_==EffectScope::TEAM || e->valid_id_==f.ctx().round_effect_valid_id[owner]))
                out.push_back(e.get());
        }
    }
    return out;
}
void window(Fight& f,int side,int count,const std::string& label) {
    CHECK_EQ(windows(f,side).size(),count,label+" 真驻留窗条目数");
}
void no_keys(Fight& f) {
    for (int id:{1074,2181}) {
        const int base=1000000000+id*10000;
        bool used=false;
        for (const auto& [k,v]:f.ctx().plugin_storage) { (void)v; if (k>=base && k<base+10000) used=true; }
        for (int s:{0,1}) for (const auto& p:f.ctx().seerRobot[s].elfPets) {
            for (const auto& [k,v]:p.on_stage_storage) { (void)v; if (k>=base && k<base+10000) used=true; }
            for (const auto& [k,v]:p.soulmark_storage) { (void)v; if (k>=base && k<base+10000) used=true; }
        }
        CHECK(!used,"K500零storage键 "+std::to_string(id));
    }
}
void immune(Fight& f,int target,int code,int rounds=1) {
    f.ctx().grant_immunity(target,ImmunityType::ANOMALY,coverage_all(),
                          code<0?0:(uint64_t{1}<<code),rounds,0,false,EffectScope::ON_STAGE,0,1074);
}
void gate(Fight& f,int target,int below,int above,int rounds=1) {
    f.ctx().rule_center_.grant_chance_gate(target,1074,ChanceTag::AnomalyAttach,50,
        below,above,chance_source_bit(ChanceSource::Skill),rounds,f.ctx().roundCount);
}
void reach(Fight& f,State st) {
    for (int i=0; i<100 && f.b->state()!=st && f.b->state()!=State::FINISHED; ++i) f.b->step();
    CHECK(f.b->state()==st,"有界单步到位");
}
void stepped(Fight& f,int side) {
    f.events.clear();
    f.ctx().debug_step_mode=true;
    f.b->submit_operation(0,side==0?Action::SELECT_SKILL:Action::NONE,0);
    f.b->submit_operation(1,side==1?Action::SELECT_SKILL:Action::NONE,0);
    f.ctx().debug_step_mode=false;
}
void swap(Fight& f,int side,int slot) {
    f.events.clear();
    const int r=f.ctx().roundCount;
    f.b->submit_operation(side,Action::CHOOSE_PET,slot);
    f.b->submit_operation(1-side,Action::NONE,0);
    f.b->drive();
    CHECK_EQ(f.ctx().on_stage[side],slot,"真实换宠到位");
    CHECK_EQ(f.ctx().roundCount,r+1,"换宠有界单回合");
}
// 只在真实FSM被动桶调用正式伤害原语，不伪造事件、不直调1074。
EffectResult inject_damage(BattleContext* c,const EffectArgs& a) {
    const int side=a.int_args[0],mode=a.int_args[1],n=a.int_args[2];
    if (mode==0) (void)core_api().deal_pink_damage(c,1-side,n,DamageKind::FIXED,side);
    else if (mode==1) (void)core_api().deal_true_damage(c,1-side,n,side);
    else if (mode==2) deal_damage(c,1-side,n,DamageKind::ATTRIBUTE,side);
    else if (mode==3) deal_damage(c,side,n,DamageKind::NORMAL,1-side);
    else if (mode==4) deal_damage(c,side,n,DamageKind::NORMAL,side);
    else deal_damage(c,1-side,n,DamageKind::NORMAL,side);
    return EffectResult::kOk;
}
void inject(Fight& f,int side,State st,int mode,int n) {
    Effect e(1074,0,side,0,EffectArgs(std::vector<int>{side,mode,n}),&inject_damage);
    auto ce=std::make_unique<ContinuousEffect>(e,st,side,1,f.ctx().roundCount);
    ce->source_id_=1074;
    f.ctx().passive_effects.register_effect(st,side,std::move(ce),f.ctx().round_effect_valid_id[side]);
}
std::uint64_t next_rng(std::uint64_t state) { return state*16807ULL%2147483647ULL; }
}

int main() {
    if (!PetFactory::initialize_runtime_data()) return 1;
    sbs_srand(500);
    // A：五条全链切片，非只测选中的一个效果切片。
    {
        const std::vector<std::pair<int,std::vector<std::pair<int,std::vector<int>>>>> want={
            {30192,{{1074,{280,80,6,2,50,6}}}},
            {32405,{{1208,{1}},{1074,{280,100,6,2,80,7}}}},
            {34014,{{747,{1}},{1392,{}},{1074,{300,100,6,2,50,6}}}},
            {34282,{{1224,{2}},{1074,{300,100,5,1,100,20}},{984,{2}}}},
            {38370,{{2463,{}},{1044,{2}},{907,{2}},{2307,{280,280}},{1074,{280,100,24,2,100,26}}}}
        };
        auto& repo=official_data::OfficialDataStore::instance().repository();
        for (const auto& [move,chain]:want) {
            const auto got=repo.load_skill_effects(move);
            CHECK_EQ(got.size(),chain.size(),"A 全链长度");
            for (std::size_t i=0;i<chain.size();++i) {
                CHECK_EQ(got[i].effect_id,chain[i].first,"A 顺序");
                CHECK(got[i].args==chain[i].second,"A 双表完整切片");
            }
        }
    }
    // B：真实五载体完整链×两侧；纯链80%以seed1第二掷49确保主支成功。
    for (int move:{30192,32405,34014,34282,38370}) for (int side:{0,1}) {
        Fight f(side,true,move);
        sbs_srand(1);
        own(f,side);
        const int code=move==34282?5:(move==38370?24:6);
        CHECK(red(f,side)>300,"B 五载体实扣过门");
        CHECK(active(f,1-side,code),"B 行参主支异常落对手 "+std::to_string(move));
        CHECK_EQ(applied(f,1-side,code),1,"B 恰一次主支异常事件");
        window(f,side,0,"B 成功不回退");
        no_keys(f);
    }
    // C：相同种子、相同完整链先测实扣，再以threshold=damage±1锁非致死严格边界。
    for (int side:{0,1}) for (bool first:{true,false}) {
        int actual=0;
        {
            Fight f(side,first,kMove,600,{999999,100,6,2,100,8});
            sbs_srand(71); own(f,side,0,first?-1:1); actual=red(f,side);
            CHECK(actual>300,"C 真实采样远离浮动门");
            window(f,side,0,"C 门闭无回退");
        }
        for (int offset:{-1,0,1}) {
            Fight f(side,first,kMove,600,{actual+offset,100,6,2,100,8});
            sbs_srand(71); own(f,side,0,first?-1:1);
            CHECK_EQ(red(f,side),actual,"C 同种子真实红伤不受门影响");
            CHECK_EQ(active(f,1-side,6),offset<0,"C 严格>，等号不开门");
            window(f,side,0,"C 门闭和成功均不回退");
        }
    }
    // D：超杀actual=目标HP，不能拿更大的管线final开门；死目标不落地时仅过门保留回退。
    for (int threshold:{280,300}) for (int offset:{-1,0,1}) {
        Fight f(0,true,kMove,600,{threshold,100,6,2,100,8});
        f.pet(1).hp=threshold+offset;
        stepped(f,0); reach(f,State::BATTLE_FIRST_ATTACK_DAMAGE); f.b->step();
        CHECK_EQ(red(f,0),threshold+offset,"D 超杀钳制实扣");
        CHECK_EQ(f.pet(1).hp,0,"D 真实归零");
        window(f,0,offset>0?2:0,"D 实扣门，非raw/final");
        CHECK_EQ(applied(f,1,6),0,"D 不给亡者写异常");
        f.b->drive();
    }
    // E：四个概率档的独立MINSTD判据；0/100短路，50/80只多消费一个随机数。
    for (unsigned seed=1;seed<=24;++seed) {
        std::array<std::uint64_t,4> states{};
        std::array<int,4> damage{};
        const std::array<int,4> chances={0,50,80,100};
        for (int i=0;i<4;++i) {
            const int chance=chances[i];
            Fight f(0,true,kMove,600,{280,chance,6,2,50,8});
            sbs_srand(seed); own(f,0);
            const bool hit=chance==100 || (chance>0 && next_rng(next_rng(seed))%100<static_cast<unsigned>(chance));
            CHECK_EQ(active(f,1,6),hit,"E 主支概率真实掷点 "+std::to_string(chance));
            window(f,0,hit?0:2,"E 仅未落地武装");
            CHECK_EQ(events(f,EventType::EVENT_ANOMALY_ATTACH_LOW_CHANCE,0,1,chance),chance<=50?1:0,
                     "E 申报制低概率事件");
            CHECK_EQ(applied(f,1,8),0,"E 施放当回合绝不掷回退");
            states[i]=sbs_rand_state(); damage[i]=red(f,0);
        }
        CHECK_EQ(states[0],states[3],"E 0/100均不耗掷点");
        CHECK_EQ(states[1],states[2],"E 50/80同样恰一掷");
        CHECK_EQ(states[1],next_rng(states[0]),"E 没有本地+原语双重掷点");
        for (int i=1;i<4;++i) CHECK_EQ(damage[0],damage[i],"E 掷点在伤害之后");
    }
    // F：下N回合起点、逐回合独立重掷、最终过期；两侧×先后手，物攻/特攻均算攻击。
    for (int side:{0,1}) for (bool first:{true,false}) for (int rounds:{1,2,3}) {
        Fight f(side,first,kMove,600,{280,0,6,rounds,50,8});
        own(f,side,0,first?-1:1);
        window(f,side,2,"F 初次失败武装");
        CHECK_EQ(applied(f,1-side,8),0,"F armed_round挡同回合");
        const auto full=f.b->dump();
        const auto snap=f.ctx().getStateJson();
        CHECK(full.find("\"effectId\":1074,")!=std::string::npos,"F fullstate显示1074");
        CHECK(full.find("\"actionOneShot\":false")!=std::string::npos,"F 真窗非一次性桥");
        CHECK(snap.find("\"effectId\":1074,")!=std::string::npos,"F 快照有驻留窗");
        const std::string payload="\"args\":[280,0,6,"+std::to_string(rounds)+",50,8,"+std::to_string(side)+",0]";
        CHECK(full.find(payload)!=std::string::npos,"F 描述六参原位，内部状态放尾部");
        for (const auto* w:windows(f,side)) {
            CHECK_EQ(w->getRegisteredRound(),1,"F NextRounds起点R+1");
            CHECK_EQ(w->getDurationRounds(),rounds,"F 原样行参时长");
            CHECK_EQ(w->source_id_,1074,"F 窗有真实来源");
        }
        for (int r=1;r<=rounds+1;++r) {
            clear(f,1-side);
            sbs_srand(r%2?1:2);
            own(f,side,r%2?1:3);
            const bool hit=r<=rounds && r%2==1;
            CHECK_EQ(active(f,1-side,8),hit,"F 每回合重掷且仅在窗内");
            CHECK_EQ(events(f,EventType::EVENT_ANOMALY_ATTACH_LOW_CHANCE,side,1-side,50),r<=rounds?1:0,
                     "F 每回合一次申报，不重复双侧桶");
        }
        window(f,side,0,"F 窗已过期");
        no_keys(f);
    }
    // G：第二层0/50/80/100分别申报，独立于第一层0%；原始异常码也独立。
    for (int chance:{0,50,80,100}) for (unsigned seed:{1u,2u}) {
        Fight f(0,true,kMove,600,{280,0,6,1,chance,13});
        own(f,0);
        sbs_srand(seed); own(f,0,1);
        const bool hit=chance==100 || (chance>0 && next_rng(next_rng(seed))%100<static_cast<unsigned>(chance));
        CHECK_EQ(active(f,1,13),hit,"G 第二层概率与异常码透传");
        CHECK(!active(f,1,6),"G 第二层不拿主支异常码");
        CHECK_EQ(events(f,EventType::EVENT_ANOMALY_ATTACH_LOW_CHANCE,0,1,chance),chance<=50?1:0,
                 "G 第二层走申报通道");
    }
    // H：低伤整段门闭；NONE/非载体不被parser提前套回退；高额异色伤不能喂红伤门。
    for (int mode:{0,1,2}) {
        Fight f(0,true,kMove,1,{280,100,6,2,100,8});
        inject(f,0,State::BATTLE_FIRST_ATTACK_DAMAGE,mode,600);
        own(f,0);
        CHECK(red(f,0)>0 && red(f,0)<280,"H 低红伤夹具");
        CHECK(!active(f,1,6) && !active(f,1,8),"H 非红伤不替代，parser已接管");
        window(f,0,0,"H 门关不回退");
    }
    for (int mode:{3,4}) {
        Fight f(0,true,kMove,600,{280,100,6,2,100,8});
        inject(f,0,State::BATTLE_FIRST_ATTACK_DAMAGE,mode,600);
        own(f,0);
        CHECK(active(f,1,6),"H 异向/自伤不吃掉once");
        CHECK(!active(f,0,6),"H 不把主支落自己");
    }
    {
        Fight f(0,true,kMove,1,{280,100,6,2,100,8});
        inject(f,0,State::BATTLE_FIRST_SKILL_EFFECT,5,600);
        own(f,0);
        CHECK(!active(f,1,6),"H 错时点NORMAL不喂门");
        window(f,0,0,"H 错时点不回退");
        own(f,0,-1); own(f,0,1);
        window(f,0,0,"H NONE/非载体不继承监听");
    }
    // I：免疫/抗性/反弹/禁附加均未落地，故武装；不是仅ROLL_FAILED。
    for (int mode=0;mode<4;++mode) for (int side:{0,1}) {
        Fight f(side,true,kMove,600,{280,100,6,2,100,13});
        if (mode==0 || mode==2) immune(f,1-side,6);
        if (mode==1) REQUIRE(f.pet(1-side).resistance.setControlSlot(0,6,100));
        if (mode==2) f.ctx().rule_center_.grant_reflect(1-side,-1,1074,1-side,1);
        if (mode==3) f.ctx().rule_center_.grant_attach_ban(1-side,1074,side,
            RuleCenter::kAttachBanAnomaly,coverage_all(),1,0);
        own(f,side);
        CHECK(!active(f,1-side,6),"I 主支未落地");
        window(f,side,2,"I 宽口径未触发");
        if (mode==0 || mode==2) CHECK_EQ(events(f,EventType::EVENT_ANOMALY_IMMUNED,side,1-side,6),1,
                                       "I 正式免疫事件");
        if (mode==1) CHECK_EQ(events(f,EventType::EVENT_ANOMALY_RESISTED,side,1-side,6),1,"I 正式抗性事件");
        if (mode==2) CHECK(active(f,side,6),"I 真实反弹落施放者");
        clear(f,side); clear(f,1-side);
        REQUIRE(f.pet(1-side).resistance.setControlSlot(0,6,0));
        own(f,side,1);
        CHECK(active(f,1-side,13),"I 下轮独立回退落地");
    }
    // J：转换/同种延长/已有更长均算成功，不能误开回退。
    for (int mode=0;mode<3;++mode) {
        Fight f(0,true,kMove,600,{280,100,6,2,100,8});
        if (mode==0) f.ctx().anomaly_conversion[1][6]=13;
        else REQUIRE(apply_anomaly_raw(&f.ctx(),1,6,mode==1?1:6,0)==ApplyAnomalyResult::SUCCESS);
        own(f,0);
        CHECK(active(f,1,mode==0?13:6),"J 成功/转换/延长仍在");
        window(f,0,0,"J 四档落地不回退");
    }
    // K：第一层和第二层均受Skill概率闸门控制，不使用本地roll绕过。
    {
        Fight f(0,true,kMove,600,{280,80,6,2,100,8});
        gate(f,1,0,0); own(f,0);
        CHECK(!active(f,1,6),"K 主支80被闸门改零");
        window(f,0,2,"K 闸门未触发也回退");
        gate(f,1,0,0); own(f,0,1);
        CHECK(!active(f,1,8),"K 第二层100也经过申报闸门");
        CHECK_EQ(events(f,EventType::EVENT_ANOMALY_ATTACH_LOW_CHANCE,0,1,0),1,"K 第二层真实概率0事件");
    }
    // L：窗期只有成功使用攻击才掷；NONE/属性/被控不算，miss/被盔仍算使用。
    for (int mode=0;mode<5;++mode) {
        Fight f(0,true,kMove,600,{280,0,6,1,100,13});
        own(f,0);
        if (mode==2) REQUIRE(apply_anomaly_raw(&f.ctx(),0,6,1,1)==ApplyAnomalyResult::SUCCESS);
        if (mode==3) { f.pet(0).skills[1].must_hit=false; f.pet(0).skills[1].accuracy=0; }
        if (mode==4) core_api().hit_effect_invalid(&f.ctx(),1,HitInvalidMode::kFullNull,1,false,1074);
        own(f,0,mode==0?-1:(mode==1?2:1));
        CHECK_EQ(active(f,1,13),mode>=3,"L 执行期攻击门");
        window(f,0,0,"L 窗按回合流逝，不延期等用技");
    }
    // M：第一层miss/白板/全免伤/全盾无伤不武装；全盾同回合桥撤监听而非等回合末掩盖。
    for (int mode=0;mode<4;++mode) {
        Fight f(0,true,kMove,600,{280,0,6,2,100,8});
        if (mode==0) { f.pet(0).skills[0].must_hit=false; f.pet(0).skills[0].accuracy=0; }
        if (mode==1) core_api().hit_effect_invalid(&f.ctx(),1,HitInvalidMode::kFullNull,1,false,1074);
        if (mode==2) f.ctx().grant_immunity(1,ImmunityType::DAMAGE,coverage_all(),0,1,0,
                                          false,EffectScope::ON_STAGE,0,1074);
        if (mode==3) f.pet(1).shield_bank_.add(0,999999,1074,0);
        own(f,0);
        CHECK_EQ(red(f,0),0,"M 无实扣红伤");
        window(f,0,0,"M 无事件不武装");
        CHECK_EQ(f.ctx().event_center_.watch_count(EventType::EVENT_TAKE_DAMAGE),0,"M 监听清理");
    }
    for (int side:{0,1}) {
        Fight f(side,true,kMove,600,{280,0,6,2,100,8});
        f.pet(1-side).shield_bank_.add(0,999999,1074,0);
        stepped(f,side); reach(f,State::BATTLE_FIRST_ATTACK_DAMAGE);
        CHECK_EQ(f.ctx().event_center_.watch_count(EventType::EVENT_TAKE_DAMAGE),1,"M 在途武装");
        f.b->step();
        CHECK_EQ(f.ctx().event_center_.watch_count(EventType::EVENT_TAKE_DAMAGE),1,"M 全盾未消费");
        f.b->step();
        CHECK_EQ(f.ctx().event_center_.watch_count(EventType::EVENT_TAKE_DAMAGE),0,"M AFTER_ACTION桥清理");
        CHECK_EQ(f.ctx().roundCount,0,"M 非回合末自然过期遮掩");
        f.b->drive();
    }
    // N：双侧同轮各自窗口；回退用非控制13以避免先手控住后手污染镜像观察。
    for (bool first:{true,false}) {
        Fight f(0,first,kMove,600,{280,0,6,2,100,13},true);
        turn(f,0,0); window(f,0,2,"N 镜像0"); window(f,1,2,"N 镜像1");
        turn(f,1,1);
        CHECK(active(f,0,13) && active(f,1,13),"N 双边各落自身目标");
        CHECK_EQ(applied(f,0,13),1,"N side1恰一次");
        CHECK_EQ(applied(f,1,13),1,"N side0恰一次");
    }
    // O：重施放失败刷新不叠加；成功不撤旧窗；清理桥不得覆盖旧1074窗口。
    {
        Fight f(0,true,kMove,600,{280,0,6,2,100,13});
        own(f,0); own(f,0);
        window(f,0,2,"O 同源只两桶");
        CHECK_EQ(applied(f,1,13),0,"O 新armed_round不当回合掷");
        own(f,0,1); CHECK(active(f,1,13),"O 刷新后第一轮"); clear(f,1);
        own(f,0,1); CHECK(active(f,1,13),"O 刷新后第二轮"); clear(f,1);
        own(f,0,1); CHECK(!active(f,1,13),"O 刷新后过期");
    }
    {
        Fight f(0,true,kMove,600,{280,0,6,2,100,13});
        own(f,0);
        f.pet(0).skills[0]=carrier(kMove,300,600,{280,100,6,2,100,13}).skills[0];
        own(f,0);
        CHECK(active(f,1,6) && active(f,1,13),"O 本次成功不取消上次回退");
        CHECK_EQ(applied(f,1,13),1,"O 清理桥不覆盖回退窗");
    }
    // P：双方离场/回场与断回合均不残留；不写私有行为/审计键。
    for (int side:{0,1}) {
        Fight f(side,true,kMove,600,{280,0,6,3,100,13});
        own(f,side); swap(f,side,1); window(f,side,0,"P 新宠不继承");
        own(f,side,1); CHECK(!active(f,1-side,13),"P 替补攻击无旧回退");
        swap(f,side,0); own(f,side,1);
        CHECK(!active(f,1-side,13),"P 回场不复活旧窗");
        no_keys(f);
    }
    {
        Fight f(0,true,kMove,600,{280,0,6,2,100,13}); own(f,0);
        (void)core_api().break_round_effects(&f.ctx(),0);
        own(f,0,1); CHECK(!active(f,1,13),"P 正式断回合终止窗"); window(f,0,0,"P 断窗");
    }
    std::cout << "scenario_8880_k500 PASS (1074 damage gate, declared chances and fallback windows)\n";
    return 0;
}
