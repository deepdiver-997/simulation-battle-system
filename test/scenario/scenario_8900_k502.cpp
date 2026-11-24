// K502 / 8900：1583后手当前战斗轮次固伤 + 1554低体力自异常且次数免伤。
// 工单 〔源: docs_local/docs/05-任务清单/已收官/E组批量效果工单.md:107〕
// 数据来源 https://github.com/SeerAPI/seer-unity-assets （2026-11-23只读当前库）。
// 1554 {1}=异常码7/15/32，非能力槽： https://wiki.biligame.com/seer/技能:37852
// 1583日历第1轮=内核rc0+1；无官方首轮实测，按载体文义+既存绝对轮号先例锁定。
// https://wiki.biligame.com/seer/技能:29079
// 〔源: resources/moves_lib/j_gaia_stance.cpp:435,453〕
// 〔源: src/fsm/battleContext.cpp:90〕〔源: src/fsm/battleFsm.cpp:2503-2508〕
// 八载体完整链装配+行为（34033多5个未消费尾参，只锁实际切片，不改库）。
// 后手差分含速度/先制/必先/同速镜像；FIXED账独立于红伤，锁抗性/罩/盾/免粉/超杀。
// 1554门两侧+恰门+非整除门；异常落码在SKILL_EFFECT被动快照取证，不用回合末
// 已衍化/过期后的码反推施加。counts=0/1/3，逐击消费、空转/未命中/无效不消费，
// 自异常免疫不取消并列免伤，源句柄隔离、刷新、上下场与G2/G3显示同锁。
// 〔源: include/fsm/battleContext.h:1237-1255〕
// 〔源: include/effects/rule_center.h:507-540〕
// 〔源: src/fsm/battleContext.cpp:1915-1939〕
// 生产无storage审计槽；事件watcher显式TEAM+valid_id，回调只记录不改变战斗。

#include <algorithm>
#include <array>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <entities/pet_factory.h>
#include <plugin/core_api.h>
#include <primitives/battle_primitives.h>
#include "scenario_harness.h"

using namespace scenario;
namespace {
using Action = BattleFsm::ActionType;
using Json = nlohmann::json;
constexpr int kRoundMove = 29079;
constexpr int kLowMove = 37852;
constexpr int kHp = 12000;
struct Seen {
    int fixed = 0, normal = 0, other = 0, fixed_events = 0, immune = 0;
    int fixed_actor = -2, fixed_state = -2;
    std::vector<std::pair<int,int>> anomalies;
};
std::array<Seen,2> seen{};
std::array<std::array<bool,44>,2> snap{};

int learner(int move) {
    switch (move) {
        case 29079: return 4781;
        case 34206: return 4050;
        case 35837: return 4362;
        case 37878: return 4748;
        case 28337: return 4516;
        case 34033: return 4014;
        case 37726: return 4708;
        default: return 4742;
    }
}
ElfPet carrier(int move, int speed, int hp=kHp,
               const std::vector<EffectParamOverride>& overrides={}) {
    auto p=make_pet(learner(move),"K502",{move,10004,20001,10080,kRoundMove},speed,1,hp);
    if (!overrides.empty()) {
        const auto m=official_data::OfficialDataStore::instance().repository().load_monster(learner(move));
        REQUIRE(m.has_value());
        p.skills[0]=Skills(move,*m,overrides);
    }
    for (auto& s:p.skills) { s.must_hit=1; s.accuracy=100; s.critical_strike_rate=0; }
    return p;
}
EffectResult snapshot(BattleContext* c,const EffectArgs&) {
    for (int side:{0,1}) for (int code=0;code<=43;++code)
        snap[side][code]=snap[side][code] || c->has_active_abnormal_status(side,code);
    return EffectResult::kOk;
}
void watch(ScenarioBattle& b) {
    auto& c=b.ctx();
    for (EventType type:{EventType::EVENT_TAKE_DAMAGE,EventType::EVENT_DAMAGE_IMMUNED,
                         EventType::EVENT_ANOMALY_APPLIED}) {
        c.event_center_.register_watcher(type,0,c.roundCount,0,false,
            [](BattleContext*,const BattleEvent& e) {
                if (e.target<0 || e.target>1) return;
                auto& d=seen[e.target];
                if (e.type==EventType::EVENT_DAMAGE_IMMUNED) { ++d.immune; return; }
                if (e.type==EventType::EVENT_ANOMALY_APPLIED) {
                    d.anomalies.emplace_back(e.amount,e.actor); return;
                }
                if (e.damage_kind==static_cast<int>(DamageKind::FIXED)) {
                    d.fixed+=e.amount; ++d.fixed_events;
                    d.fixed_actor=e.actor; d.fixed_state=e.state;
                } else if (e.damage_kind==static_cast<int>(DamageKind::NORMAL)) {
                    d.normal+=e.amount;
                } else { d.other+=e.amount; }
            },WatcherScope::TEAM,c.watcher_valid_id[0]);
    }
    for (int side:{0,1}) for (State st:{State::BATTLE_FIRST_SKILL_EFFECT,State::BATTLE_SECOND_SKILL_EFFECT}) {
        Effect e(1554,0,side,-1,EffectArgs(std::vector<int>{side}),&snapshot);
        auto ce=std::make_unique<ContinuousEffect>(e,st,side,-1,c.roundCount);
        ce->source_id_=1554;
        c.passive_effects.register_effect(st,side,std::move(ce),c.round_effect_valid_id[side]);
    }
}
std::unique_ptr<ScenarioBattle> solo(int move,int side=0,bool first=true,int hp=kHp,
                                    const std::vector<EffectParamOverride>& overrides={}) {
    const auto p=carrier(move,first?300:100,hp,overrides);
    const auto q=carrier(kRoundMove,first?100:300,hp);
    auto b=std::make_unique<ScenarioBattle>(make_party(side==0?p:q),make_party(side==1?p:q));
    b->start(); watch(*b); return b;
}
void turn(ScenarioBattle& b,int slot0,int slot1,bool complete=true) {
    seen={}; snap={};
    const int round=b.ctx().roundCount;
    b.submit_operation(0,slot0<0?Action::NONE:Action::SELECT_SKILL,std::max(0,slot0));
    b.submit_operation(1,slot1<0?Action::NONE:Action::SELECT_SKILL,std::max(0,slot1));
    b.drive();
    if (complete) CHECK_EQ(b.ctx().roundCount,round+1,"单回合有界驱动");
}
void own_turn(ScenarioBattle& b,int side,int ours=0,int theirs=-1,bool complete=true) {
    turn(b,side==0?ours:theirs,side==1?ours:theirs,complete);
}
void fixed_is(int side,int amount,const std::string& label) {
    const auto& d=seen[1-side];
    CHECK_EQ(d.fixed,amount,label+" FIXED实扣");
    CHECK_EQ(d.fixed_events,amount>0?1:0,label+" 固伤恰一笔或无事件");
    CHECK_EQ(d.other,0,label+" 不是百分比/真伤");
    CHECK_EQ(seen[side].fixed,0,label+" 不打自身");
    if (amount>0) {
        CHECK_EQ(d.fixed_actor,side,label+" actor正确");
        CHECK_EQ(d.fixed_state,static_cast<int>(State::BATTLE_SECOND_SKILL_EFFECT),
                 label+" 后手SKILL_EFFECT即时结算");
    }
}
void cast_round(ScenarioBattle& b,int side,int amount,const std::string& label) {
    const int before=b.pet(1-side).hp;
    own_turn(b,side,0,2);
    fixed_is(side,amount,label);
    CHECK_EQ(before-b.pet(1-side).hp,amount+seen[1-side].normal,label+" HP和分型事件互锚");
}
std::vector<RuleTicket> tickets(const BattleContext& c,int side) {
    std::vector<RuleTicket> out;
    for (const auto& t:c.rule_center_.entries())
        if (t.source_effect_id==1554 && t.source_owner==side
            && !c.rule_center_.is_expired(t,c.roundCount)) out.push_back(t);
    return out;
}
void ticket_is(ScenarioBattle& b,int side,int counts,const std::string& label) {
    const auto ts=tickets(b.ctx(),side);
    CHECK_EQ(ts.size(),counts>0?1u:0u,label+" 1554来源票数量");
    if (ts.empty()) return;
    const auto& t=ts[0];
    CHECK_EQ(t.remaining_counts,counts,label+" 剩余次数");
    CHECK_EQ(t.remaining_rounds,0,label+" 不是回合窗票");
    CHECK_EQ(t.category,RuleCategory::IMMUNE,label+" 不是SEAL无效票");
    CHECK_EQ(t.subtype,static_cast<int>(ImmunityType::DAMAGE),label+" 只免红伤");
    CHECK_EQ(t.target,side,label+" 免伤目标是自身");
    // scope不在此仅查枚举；O腿用真实换宠后的票面/HP锁离场清理。
}
void display_is(ScenarioBattle& b,int side,int counts,const std::string& label) {
    const auto full=Json::parse(b.dump());
    const auto brief=Json::parse(b.ctx().getStateJson());
    int nf=0,ns=0;
    for (const auto& t:full.at("ruleTickets")) {
        if (t.at("sourceEffectId")==1554 && t.at("sourceOwner")==side && !t.at("expired").get<bool>()) {
            ++nf;
            CHECK_EQ(t.at("counts").get<int>(),counts,label+" fullstate counts");
            CHECK_EQ(t.at("target").get<int>(),side,label+" fullstate target");
        }
    }
    for (const auto& t:brief.at("player"+std::to_string(side)).at("effects").at("tickets")) {
        if (t.at("sourceEffectId")==1554) {
            ++ns; CHECK_EQ(t.at("counts").get<int>(),counts,label+" snapshot counts");
        }
    }
    CHECK_EQ(nf,counts>0?1:0,label+" G2有且仅有本来源票");
    CHECK_EQ(ns,counts>0?1:0,label+" G3快照剩余量同步");
}
void anomaly_is(int side,int code,bool present,const std::string& label) {
    CHECK_EQ(snap[side][code],present,label+" SKILL_EFFECT落码面");
    const int n=static_cast<int>(std::count(seen[side].anomalies.begin(),seen[side].anomalies.end(),
                                          std::make_pair(code,side)));
    CHECK_EQ(n,present?1:0,label+" 自施异常actor/码/次数");
    CHECK(!snap[1-side][code],label+" 没有误施给对手");
}
void no_storage(const BattleContext& c) {
    for (int id:{1554,1583}) {
        const int base=1000000000+id*10000;
        bool occupied=false;
        for (const auto& x:c.plugin_storage) if (x.first>=base && x.first<base+10000) occupied=true;
        for (int side:{0,1}) for (const auto& p:c.seerRobot[side].elfPets) {
            for (const auto& x:p.on_stage_storage) if (x.first>=base && x.first<base+10000) occupied=true;
            for (const auto& x:p.soulmark_storage) if (x.first>=base && x.first<base+10000) occupied=true;
        }
        CHECK(!occupied,"不占三个storage的候选键段 "+std::to_string(id));
    }
}
void anomaly_immune(ScenarioBattle& b,int side) {
    b.ctx().grant_immunity(side,ImmunityType::ANOMALY,coverage_all(),0,20,0,false,
                           EffectScope::ON_STAGE,0,48,ImmunityTier::Modern);
}
void swap(ScenarioBattle& b,int side,int slot) {
    seen={}; snap={};
    const int round=b.ctx().roundCount;
    b.submit_operation(side,Action::CHOOSE_PET,slot);
    b.submit_operation(1-side,Action::NONE,0);
    b.drive();
    CHECK_EQ(b.ctx().roundCount,round+1,"换宠单回合");
    CHECK_EQ(b.ctx().on_stage[side],slot,"换宠真实到位");
    CHECK_EQ(seen[0].fixed+seen[1].fixed,0,"换宠无延迟固伤");
}
}

int main() {
    if (!PetFactory::initialize_runtime_data()) return 1;
    sbs_srand(8900);
    // A：八条官方完整链，不剥离兄弟效果；34033尾参问题如头注，不重复读取第二组。
    {
        const std::vector<std::pair<int,std::vector<std::pair<int,std::vector<int>>>>> want={
            {29079,{{1583,{20}}}},
            {34206,{{1568,{}},{494,{}},{986,{100}},{1583,{15}}}},
            {35837,{{879,{}},{444,{}},{1583,{20}}}},
            {37878,{{455,{1,1}},{1583,{20}},{1974,{}}}},
            {28337,{{1932,{4,32}},{2019,{2,100}},{1677,{500,500,1}},{1554,{2,32,1}}}},
            {34033,{{828,{}},{1553,{2,100}},{1554,{2,15,1}}}},
            {37726,{{1012,{300}},{845,{3,30}},{1554,{3,15,1}}}},
            {37852,{{1554,{2,7,1}},{845,{2,50}}}}
        };
        auto& repo=official_data::OfficialDataStore::instance().repository();
        for (const auto& [move,chain]:want) {
            const auto got=repo.load_skill_effects(move);
            CHECK_EQ(got.size(),chain.size(),"A 真实载体链长");
            for (std::size_t i=0;i<chain.size();++i) {
                CHECK_EQ(got[i].effect_id,chain[i].first,"A 链序");
                CHECK(got[i].args==chain[i].second,"A 双表真实参数切片");
            }
        }
    }
    // B：1583四载体×双方×先后手；首轮固伤20/15，纯链属性无红伤。
    for (int move:{29079,34206,35837,37878}) for (int side:{0,1}) for (bool first:{false,true}) {
        auto b=solo(move,side,first);
        cast_round(*b,side,first?0:(move==34206?15:20),"B 首轮载体"+std::to_string(move));
        CHECK_EQ(seen[1-side].normal>0,move!=29079,"B 属性/攻击红伤分型");
        no_storage(b->ctx());
    }
    // C：轮次不是施放次数/在场回合；空转、其他技、换宠不重置也不留下延迟伤害。
    for (int side:{0,1}) {
        auto b=solo(kRoundMove,side,false);
        cast_round(*b,side,20,"C 第一轮");
        own_turn(*b,side,-1,-1); fixed_is(side,0,"C 空转第二轮");
        cast_round(*b,side,60,"C 第三轮第二次施放");
        own_turn(*b,side,2,2); fixed_is(side,0,"C 第四轮其他技能");
        swap(*b,side,1);
        cast_round(*b,side,120,"C 第六轮新宠首施放");
        swap(*b,side,0);
        cast_round(*b,side,160,"C 第八轮回场");
    }
    // D：真实序≠速度；慢方先制抢先门闭，快方被先制/必先压后门开。同速读裁定结果。
    for (int side:{0,1}) for (int mode=0;mode<4;++mode) {
        const bool fast=mode!=0;
        auto b=solo(kRoundMove,side,fast);
        // loadSkills把固有先制烧进selection_effects_末条，改裸priority不会重建它。
        // 此处只覆盖选择期造料；被测后手门仍由完整FSM真实序消费。
        const auto priority_fixture=[](Skills& skill,int value) {
            skill.priority=value;
            REQUIRE(!skill.selection_effects_.empty());
            skill.selection_effects_.back().effect.args=EffectArgs(std::vector<int>{0,1,value});
        };
        if (mode==0) priority_fixture(b->pet(side).skills[0],3);
        if (mode==1) priority_fixture(b->pet(1-side).skills[2],3);
        if (mode==2) {
            priority_fixture(b->pet(side).skills[0],5);
            core_api().grant_guaranteed_first(&b->ctx(),1-side,1);
        }
        if (mode==3) b->pet(1-side).numericalBase[NumericalPropertyIndex::SPEED]=300;
        own_turn(*b,side,0,2);
        const int first=b->ctx().preemptive_right==PreemptiveRight::SEER_ROBOT_2?1:0;
        if (mode!=3) CHECK_EQ(first,mode==0?side:1-side,"D 必先→先制→速度");
        fixed_is(side,first==side?0:20,"D 实际序后手门");
    }
    // E：倍率0/3/15与FIXED管线（首轮20；固定抗性50%→10、罩7→13、盾不挡粉）。
    for (int mul:{0,3,15}) {
        auto b=solo(kRoundMove,0,false,kHp,{{kRoundMove,1583,0,mul}});
        cast_round(*b,0,mul,"E 参数倍率"+std::to_string(mul));
    }
    for (int mode=0;mode<4;++mode) {
        auto b=solo(kRoundMove,0,false);
        if (mode==0) { b->pet(1).damage_resist.fixed_pct=50; b->pet(1).damage_resist.percent_pct=10; }
        if (mode==1) b->pet(1).hood_bank_.add(0,7,1583,0);
        if (mode==2) b->pet(1).shield_bank_.add(0,77,1583,0);
        if (mode==3) b->ctx().grant_immunity(1,ImmunityType::PINK_DAMAGE,coverage_all(),0,2,0,
                                             false,EffectScope::ON_STAGE,0,1583);
        cast_round(*b,0,mode==0?10:mode==1?13:mode==2?20:0,"E 固伤管线档"+std::to_string(mode));
        if (mode==1) CHECK_EQ(b->pet(1).hood_bank_.total(),0,"E 罩消耗7");
        if (mode==2) CHECK_EQ(b->pet(1).shield_bank_.total(),77,"E 红盾不消费");
    }
    // F：双方同回合用纯链，只后手有一笔；掉血/超杀与死亡归因独立锁。
    for (int first:{0,1}) {
        auto b=solo(kRoundMove,first,true);
        turn(*b,0,0);
        fixed_is(1-first,20,"F 双方同施仅后手发伤");
    }
    {
        auto b=solo(kRoundMove,0,false);
        b->pet(1).hp=7;
        own_turn(*b,0,0,2,false);
        fixed_is(0,7,"F 超杀实际扣7");
        CHECK_EQ(b->ctx().seerRobot[1].elfPets[0].hp,0,"F 目标倒地");
        CHECK_EQ(b->ctx().last_damage_actor[1][0],-1,"F 粉伤击杀无击败归因");
    }
    // G：1554八载体维度中的四条：低血门开/满血门闭，完整链，双方镜像。
    for (int move:{28337,34033,37726,37852}) for (int side:{0,1}) for (bool low:{true,false}) {
        auto b=solo(move,side,true);
        b->pet(side).hp=low?2000:kHp;
        const int code=move==28337?32:move==37852?7:15;
        own_turn(*b,side);
        anomaly_is(side,code,low,"G 官方载体"+std::to_string(move));
        ticket_is(*b,side,low?1:0,"G 低体力门");
        ticket_is(*b,1-side,0,"G 对侧无本来源票");
        display_is(*b,side,low?1:0,"G 原生票显示");
        no_storage(b->ctx());
    }
    // H：严格小于门，不用整数除法截断；每个独立对局都不借已存在异常制造假阳性。
    for (int side:{0,1}) for (int hp:{5999,6000,6001}) {
        auto b=solo(kLowMove,side,true);
        b->pet(side).hp=hp;
        own_turn(*b,side);
        anomaly_is(side,7,hp==5999,"H 半血恰门与两侧");
        ticket_is(*b,side,hp==5999?1:0,"H 半血恰门");
    }
    for (int hp:{3999,4000,4001}) {
        auto b=solo(37726);
        b->pet(0).hp=hp;
        own_turn(*b,0);
        anomaly_is(0,15,hp==3999,"H 三分之一原生参数");
        ticket_is(*b,0,hp==3999?1:0,"H 除数3不是2");
    }
    {
        auto b=solo(37726,0,true,12001);
        b->pet(0).hp=4000;
        own_turn(*b,0);
        anomaly_is(0,15,true,"H 4000×3<12001非整除门开");
        ticket_is(*b,0,1,"H 不能除法截断成4000<4000");
    }
    // I：后手受击后才读当前HP；先手恰门闭但随后受伤不能倒灌授票。
    for (bool first:{true,false}) {
        auto b=solo(kLowMove,0,first);
        b->pet(0).hp=6000;
        own_turn(*b,0,0,1);
        CHECK(b->pet(0).hp<6000,"I 对手正常红伤确实落地");
        anomaly_is(0,7,!first,"I 效果期现读HP");
        ticket_is(*b,0,first?0:1,"I 先后手现场门差分");
    }
    // J：原生1票当回合立即挡一次，下次攻击落地；攻击结果仍HIT而非技能无效。
    for (int side:{0,1}) {
        auto b=solo(kLowMove,side,true);
        b->pet(side).hp=2000;
        own_turn(*b,side,0,1);
        CHECK_EQ(b->pet(side).hp,2000,"J 当回合后手攻击已被挡");
        CHECK_EQ(seen[side].immune,1,"J EVENT_DAMAGE_IMMUNED一次");
        CHECK_EQ(b->ctx().ws.skill_exec_result[1-side],SkillExecResult::HIT,
                 "J 只挡伤害不令攻击技能无效");
        ticket_is(*b,side,0,"J 原生1票已用完");
        display_is(*b,side,0,"J 消耗后从显示中移除");
        own_turn(*b,side,-1,1);
        CHECK(b->pet(side).hp<2000,"J 第二次攻击落地");
        CHECK_EQ(seen[side].immune,0,"J 不永久免伤");
    }
    // K：三次票跨空转不衰减；MISS/无效攻击不进红伤BLOCK，不扣DAMAGE票。
    {
        auto b=solo(kLowMove,0,true,kHp,{{kLowMove,1554,2,3}});
        b->pet(0).hp=2000;
        own_turn(*b,0);
        ticket_is(*b,0,3,"K 授予三次");
        for (int i=0;i<4;++i) own_turn(*b,0,-1,-1);
        ticket_is(*b,0,3,"K 跨四个空转回合不消耗");
        b->pet(1).skills[1].must_hit=0; b->pet(1).skills[1].accuracy=0;
        own_turn(*b,0,-1,1);
        ticket_is(*b,0,3,"K MISS不消费"); CHECK_EQ(seen[0].immune,0,"K MISS无免伤事件");
        b->pet(1).skills[1].must_hit=1; b->pet(1).skills[1].accuracy=100;
        REQUIRE(core_api().seal_skill(&b->ctx(),0,1,1554,false,true,1,0,false,-1,
                                      EffectScope::ON_STAGE,false,100,false)>0);
        own_turn(*b,0,-1,1);
        ticket_is(*b,0,3,"K 无效攻击不消费"); CHECK_EQ(seen[0].immune,0,"K 无效不进BLOCK");
        for (int left:{2,1,0}) {
            own_turn(*b,0,-1,1);
            CHECK_EQ(b->pet(0).hp,2000,"K 三次红伤逐击挡下");
            CHECK_EQ(seen[0].immune,1,"K 每次免伤事件");
            ticket_is(*b,0,left,"K 逐次消费"); display_is(*b,0,left,"K 逐次显示");
        }
        own_turn(*b,0,-1,1);
        CHECK(b->pet(0).hp<2000,"K 第四次攻击落地");
    }
    // L：DAMAGE不等于粉伤/封技：龙之怒红挡粉40落；0次数不变永久票。
    {
        auto b=solo(kLowMove);
        b->pet(0).hp=2000;
        own_turn(*b,0,0,3);
        CHECK_EQ(seen[0].fixed,40,"L 龙之怒粉伤穿透");
        CHECK_EQ(seen[0].normal,0,"L 龙之怒红伤被挡");
        CHECK_EQ(seen[0].immune,1,"L 红伤免疫事件");
        CHECK_EQ(b->pet(0).hp,1960,"L 实际只失40HP");
        ticket_is(*b,0,0,"L 仅消费一次");
    }
    {
        auto b=solo(kLowMove,0,true,kHp,{{kLowMove,1554,2,0}});
        b->pet(0).hp=2000;
        own_turn(*b,0,0,1);
        anomaly_is(0,7,true,"L 零次数自异常仍独立落地");
        ticket_is(*b,0,0,"L 零次不授永久票");
        CHECK(b->pet(0).hp<2000,"L 零次不挡攻击");
    }
    // M：“且”不是“触发成功”：免控挡自异常仍授票；同源重施刷新，门闭不刷新旧票。
    for (int side:{0,1}) {
        auto b=solo(kLowMove,side,true,kHp,{{kLowMove,1554,2,3}});
        b->pet(side).hp=2000;
        anomaly_immune(*b,side);
        own_turn(*b,side);
        anomaly_is(side,7,false,"M 自异常免疫");
        ticket_is(*b,side,3,"M 并列免伤照授");
        const int handle=tickets(b->ctx(),side)[0].source_id;
        own_turn(*b,side,-1,1); ticket_is(*b,side,2,"M 已消费一票");
        own_turn(*b,side); ticket_is(*b,side,3,"M 重施刷新不叠到5");
        CHECK_EQ(tickets(b->ctx(),side)[0].source_id,handle,"M 同源句柄复用");
        own_turn(*b,side,-1,1);
        b->pet(side).hp=6000;
        own_turn(*b,side); ticket_is(*b,side,2,"M 门闭不刷新余票");
    }
    // N：双方同施，先手票剩2、后手票3；不能把1554当全局固定句柄互吞。
    for (int first:{0,1}) {
        const auto p=carrier(kLowMove,first==0?300:100,kHp,{{kLowMove,1554,2,3}});
        const auto q=carrier(kLowMove,first==1?300:100,kHp,{{kLowMove,1554,2,3}});
        ScenarioBattle b(make_party(p),make_party(q)); b.start(); watch(b);
        b.pet(0).hp=2000; b.pet(1).hp=2000;
        turn(b,0,0);
        ticket_is(b,first,2,"N 先手一票已挡对方攻击");
        ticket_is(b,1-first,3,"N 后手完整三票");
        CHECK(tickets(b.ctx(),0)[0].source_id!=tickets(b.ctx(),1)[0].source_id,"N 镜像句柄不串");
        display_is(b,first,2,"N 先手显示"); display_is(b,1-first,3,"N 后手显示");
        no_storage(b.ctx());
    }
    // O：主动离场原生票立即清，回场不复活；新宠/旧宠后续攻击照落。
    for (int side:{0,1}) {
        auto b=solo(kLowMove,side,true,kHp,{{kLowMove,1554,2,3}});
        b->pet(side).hp=2000;
        own_turn(*b,side); ticket_is(*b,side,3,"O 换前票在");
        swap(*b,side,1);
        ticket_is(*b,side,0,"O 离场清票"); display_is(*b,side,0,"O 显示同步清");
        own_turn(*b,side,-1,1); CHECK(b->pet(side).hp<kHp,"O 新宠挨击");
        swap(*b,side,0);
        ticket_is(*b,side,0,"O 回场不恢复旧票");
        own_turn(*b,side,-1,1); CHECK(b->pet(side).hp<2000,"O 旧宠回场照常受伤");
    }
    // P：1554及1583被控/MISS不执行（特别防1554旧parser无条件自施假实现）。
    for (int move:{kRoundMove,kLowMove}) for (int mode:{0,1}) {
        auto b=solo(move,0,false);
        b->pet(0).hp=2000;
        if (mode==0) { b->pet(0).skills[0].must_hit=0; b->pet(0).skills[0].accuracy=0; }
        if (mode==1) REQUIRE(apply_anomaly_raw(&b->ctx(),0,6,1,1)==ApplyAnomalyResult::SUCCESS);
        own_turn(*b,0,0,2);
        fixed_is(0,0,"P 不执行固伤");
        ticket_is(*b,0,0,"P 不执行免伤");
        CHECK(!snap[0][7],"P 未自施疲惫");
    }
    std::cout << "scenario_8900_k502 ALL PASS\n";
    return 0;
}
