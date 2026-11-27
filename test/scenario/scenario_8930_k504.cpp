// K504 / 8930：2061 随机附加弱化类异常；真实五载体、真实BattleFsm，不直调被测函数。
// 数据 https://github.com/SeerAPI/seer-unity-assets （2026-11-24 独立只读SQLite重核）。
// 〔源: docs_local/docs/05-任务清单/已收官/E组批量效果工单.md:98〕
// 旧K493阻塞已由基点44状态入表解除；不实现魇昧专属封锁/遮蔽，不注册2102/1935。
// 〔源: include/abnormal-system/abnormal-types.h:13-15〕
// 观察原语的真实基础池，精确比对16码（排除3，包含4/32/44），再由池改写票做
// 单成员/多成员/重复/空池对照；无改写腿用独立MINSTD+Fisher-Yates预测全部选码。
// 〔源: include/effects/rule_center.h:1249-1268〕
// 〔源: src/primitives/battle_primitives.cpp:528-539〕
// 锁定种数为不重复尝试数（非保证新落地数）：免疫/部分免疫/反弹/禁附加/闸门关闭/
// 已有同种不补抽；同种短时延长、长时不缩，默认固定2回合，不把种数当时长。
// 〔源: include/primitives/battle_primitives.h:67-69〕
// 〔源: src/primitives/battle_primitives.cpp:389-404〕
// fullstate锁异常身份/时长；一次性2061无驻留窗、无storage，换宠/下一技能不残留。
// 〔源: docs/07-工具与测试/效果显示gate规范.md:39〕

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <entities/pet_factory.h>
#include <nlohmann/json.hpp>
#include <plugin/core_api.h>
#include <primitives/battle_primitives.h>
#include "scenario_harness.h"

using namespace scenario;
namespace {
using Action = BattleFsm::ActionType;
using Json = nlohmann::json;
constexpr int kAttr = 28589;
constexpr int kAttack = 36923;
constexpr int kHit = 10004;
constexpr int kHp = 100000;
// 独立期待值来自battle_effects(group_type=0,efftype=1)，不是复制生产的分类循环。
const std::vector<int> kPool{1,2,4,5,10,11,13,16,20,28,29,30,31,32,36,44};
bool contains(const std::vector<int>& v, int id) {
    return std::find(v.begin(),v.end(),id)!=v.end();
}
int learner(int move) {
    switch (move) {
        case kAttr: return 4601;
        case 29125: return 4807;
        case 36968: return 4577;
        case 38658: return 4969;  // hidden_moves真实载体。
        default: return 4567;
    }
}
ElfPet carrier(int move, int speed, int count=-1) {
    auto p=make_pet(learner(move),"K504-2061",{move,kHit,kHit,kHit,kHit},speed,300,kHp);
    if (count>=0) {
        const auto m=official_data::OfficialDataStore::instance().repository().load_monster(learner(move));
        REQUIRE(m.has_value());
        p.skills[0]=Skills(move,*m,std::vector<EffectParamOverride>{{move,2061,0,count}});
    }
    for (auto& skill:p.skills) {
        skill.must_hit=1;
        skill.accuracy=100;
        skill.critical_strike_rate=0;
    }
    return p;
}
struct Fight {
    // 生命周期长于b，所有回调仅在本局存活。
    std::vector<BattleEvent> events;
    std::vector<std::vector<int>> observed_pools;
    std::vector<State> observed_states;
    std::unique_ptr<ScenarioBattle> b;
    Fight(int side=0, bool first=true, int move=kAttr, int count=-1, bool mirror=false) {
        auto p=carrier(move,first?300:100,count);
        auto q=carrier(mirror?move:kAttack,first?100:300,count);
        // 先制烧进selection_effects_，不能只改priority裸字段。真实无效果对照刀：
        // 29125(+3)配10630光闪击，36968(+1)配10008电光火石，其余配0先制拍打。
        q.skills[1]=Skills(move==29125?10630:(move==36968?10008:kHit));
        q.skills[1].must_hit=1;
        q.skills[1].critical_strike_rate=0;
        b=std::make_unique<ScenarioBattle>(make_party(side==0?p:q),make_party(side==0?q:p));
        b->start();
        b->ctx().event_center_.set_delivery_sink(&events);
        pool(false,{});
    }
    BattleContext& ctx() { return b->ctx(); }
    void pool(bool rewrite, std::vector<int> members) {
        ctx().rule_center_.grant_anomaly_pool_mod(0,2061,
            [this,rewrite,members](BattleContext* c,const std::vector<int>& base) {
                observed_pools.push_back(base);
                observed_states.push_back(c->currentState);
                CHECK(base==kPool,"生产送入原语的基础池精确等于官方16码（含4/32/44、不含3）");
                return rewrite?members:base;
            });
    }
};
void turn(Fight& f, int s0, int s1) {
    // 原语造料是入队晚投递：先正式drain，再开始本次FSM的事件采样，避免把前置施加
    // 算成被测2061的落地事件（清外部vector不能清EventCenter内部pending_）。
    f.ctx().event_center_.drain(&f.ctx(),f.ctx().roundCount,f.ctx().watcher_valid_id);
    f.events.clear();
    f.observed_pools.clear();
    f.observed_states.clear();
    const int r=f.ctx().roundCount;
    f.b->submit_operation(0,s0<0?Action::NONE:Action::SELECT_SKILL,std::max(0,s0));
    f.b->submit_operation(1,s1<0?Action::NONE:Action::SELECT_SKILL,std::max(0,s1));
    f.b->drive();
    CHECK_EQ(f.ctx().roundCount,r+1,"真实FSM有界完整一轮");
}
void own(Fight& f,int side,int skill=0,int other=-1) {
    turn(f,side==0?skill:other,side==1?skill:other);
}
std::vector<int> active(Fight& f,int side) {
    std::vector<int> out;
    for (int id=0;id<=kOfficialAbnormalStatusMaxId;++id)
        if (f.ctx().has_active_abnormal_status(side,id)) out.push_back(id);
    return out;
}
std::vector<int> applied(Fight& f,int side,int actor) {
    std::vector<int> out;
    for (const auto& e:f.events)
        if (e.type==EventType::EVENT_ANOMALY_APPLIED && e.target==side && e.actor==actor)
            out.push_back(e.amount);
    return out;
}
int events(Fight& f,EventType kind,int target,int actor) {
    return static_cast<int>(std::count_if(f.events.begin(),f.events.end(),[=](const BattleEvent& e) {
        return e.type==kind && e.target==target && e.actor==actor;
    }));
}
std::uint64_t next(std::uint64_t state) { return state*16807ULL%2147483647ULL; }
std::vector<int> picks(std::vector<int> bag,int count,unsigned seed) {
    std::uint64_t state=seed;
    const int n=std::min(count,static_cast<int>(bag.size()));
    for (int i=0;i<n;++i) {
        state=next(state);
        const int j=i+static_cast<int>(state%static_cast<unsigned>(bag.size()-i));
        std::swap(bag[i],bag[j]);
    }
    bag.resize(n);
    return bag;
}
void check_display(Fight& f,int side,const std::vector<int>& ids,int remaining=1) {
    const auto full=Json::parse(f.b->dump());
    const auto& player=full.at(side==0?"player0":"player1");
    const auto& states=player.at("party").at(player.at("onStage").get<int>()).at("abnormalStates");
    CHECK_EQ(states.size(),ids.size(),"fullstate异常总数吻合");
    for (int id:ids) {
        const auto it=std::find_if(states.begin(),states.end(),[=](const Json& entry) {
            return entry.at("id")==id;
        });
        CHECK(it!=states.end(),"fullstate含真实异常身份 "+std::to_string(id));
        if (it!=states.end()) {
            CHECK_EQ(it->at("remaining").get<int>(),remaining,"默认2回合、完整一轮后剩1");
            CHECK(it->at("name")==abnormal_status_name_cn(id),"中文异常显示可解析");
        }
    }
    const auto brief=Json::parse(f.ctx().getStateJson());
    CHECK(brief.at(side==0?"player0":"player1").at("abnormalStates")==states,
          "fullstate与战斗快照异常集合一致");
}
void no_resident(Fight& f) {
    const auto full=Json::parse(f.b->dump());
    const auto text=full.dump();
    std::size_t pos=0;
    int windows=0;
    while ((pos=text.find("\"effectId\":2061,",pos))!=std::string::npos) {
        const auto end=text.find('}',pos);
        const auto e=text.substr(pos,end-pos);
        pos=end+1;
        if (e.find("\"isRoundEffect\":true")!=std::string::npos
            && e.find("\"actionOneShot\":false")!=std::string::npos
            && e.find("\"isExpired\":false")!=std::string::npos) ++windows;
    }
    CHECK_EQ(windows,0,"2061一次性无驻留回合窗");
    for (const auto& [key,value]:f.ctx().plugin_storage) {
        (void)value;
        CHECK(key<1020610000 || key>=1020620000,"2061无storage键");
        CHECK(key<1021020000 || key>=1021030000,"2102未旁支实现");
    }
}
int guard(Fight& f,int side,int counts=0,std::uint64_t mask=0) {
    return f.ctx().grant_immunity(side,ImmunityType::ANOMALY,coverage_all(),mask,3,0,
                                  false,EffectScope::ON_STAGE,counts,1221,ImmunityTier::Modern);
}
int gate(Fight& f,int side,int result) {
    return f.ctx().grant_chance_gate(side,2061,ChanceTag::AnomalyAttach,0,-1,result,
                                     chance_source_bit(ChanceSource::Skill),0);
}
void swap(Fight& f,int side,int slot) {
    f.events.clear(); f.observed_pools.clear();
    const int before=f.ctx().roundCount;
    f.b->submit_operation(side,Action::CHOOSE_PET,slot);
    f.b->submit_operation(1-side,Action::NONE,0);
    f.b->drive();
    CHECK_EQ(f.ctx().on_stage[side],slot,"真实换宠到位");
    CHECK_EQ(f.ctx().roundCount,before+1,"换宠完成一轮");
}
}  // namespace

int main() {
    REQUIRE(PetFactory::initialize_runtime_data());
    // A 五真实载体全链及参数切片。friend_side_effect为空；38658为隐藏可学链。
    {
        const std::vector<std::pair<int,std::vector<std::pair<int,std::vector<int>>>>> want={
            {28589,{{48,{3}},{2061,{2}}}},
            {29125,{{191,{4}},{819,{0,50}},{2061,{3}}}},
            {36923,{{2061,{2}},{964,{}},{1960,{2}}}},
            {36968,{{2061,{2}},{914,{5,2,100}},{96,{}}}},
            {38658,{{789,{1}},{2061,{2}},{2355,{3,8,3}},{820,{2,2}},{185,{8}}}}
        };
        for (const auto& [move,chain]:want) {
            const auto got=official_data::OfficialDataStore::instance().repository().load_skill_effects(move);
            CHECK_EQ(got.size(),chain.size(),"A 真实载体链长度");
            for (std::size_t i=0;i<chain.size();++i) {
                CHECK_EQ(got[i].effect_id,chain[i].first,"A 真实链顺序");
                CHECK(got[i].args==chain[i].second,"A 参数切片不串位");
            }
            for (int side:{0,1}) for (bool first:{true,false}) {
                Fight f(side,first,move);
                f.ctx().seed_rng(23);
                const int r=f.ctx().roundCount;
                own(f,side,0,1);  // 对手真实普通攻击，覆盖先/后手而非只比较速度字段。
                const int count=move==29125?3:2;
                CHECK_EQ(f.observed_pools.size(),1,"A 每次命中仅解析一次池");
                CHECK_EQ(active(f,1-side).size(),count,"A 当前切片的真实种数2/3");
                CHECK(active(f,side).empty(),"A 仅施加对手，自身未误中");
                const auto landed=applied(f,1-side,side);
                CHECK_EQ(landed.size(),count,"A 归因对手的落地事件数");
                CHECK_EQ(f.observed_states.size(),1,"A 原语时点观察有且仅一次");
                CHECK_EQ(f.observed_states[0],first?State::BATTLE_FIRST_SKILL_EFFECT:
                    State::BATTLE_SECOND_SKILL_EFFECT,"A 实际先/后手命中趟路由");
                for (int id:landed) {
                    CHECK(contains(kPool,id),"A 每只均来自弱化池");
                    CHECK_EQ(f.ctx().get_abnormal_status_end_round(1-side,id),r+2,
                             "A 种数与默认时长独立，时长恒2");
                }
                check_display(f,1-side,active(f,1-side));
                no_resident(f);
            }
        }
    }
    // B 不改池、不借概率运气：24独立种子、MINSTD精确预测挑选顺序与最终RNG。
    for (unsigned seed=1;seed<=24;++seed) {
        Fight f;
        f.ctx().seed_rng(seed);
        own(f,0);
        const auto expected=picks(kPool,2,seed);
        CHECK(applied(f,1,0)==expected,"B 不缩池时的精确不重复选码序列");
        auto sorted=expected; std::sort(sorted.begin(),sorted.end());
        CHECK(active(f,1)==sorted,"B 终态集合与独立预测完全相同");
        auto expected_rng=next(next(seed));
        // 本轮先手施加混乱10时，对手即使NONE也会在ACTION_START多掷一次5%点伤。
        // 不把自然结算的额外耗随机数误算成2061重复挑选。
        // 〔源: src/fsm/battleFsm.cpp:1185-1193〕
        if (contains(expected,10)) expected_rng=next(expected_rng);
        CHECK_EQ(f.ctx().rng().state(),expected_rng,"B 恰两次挑选+可分离的混乱自然结算，无额外概率/时长掷点");
        CHECK_EQ(f.observed_pools.size(),1,"B 单子句一次池解析");
    }
    // C 全合法码逐项边界：16池中均可落，控制/附属/寄生对手等池外均不混入。
    for (int id=0;id<=44;++id) {
        Fight f;
        f.ctx().rule_center_.grant_anomaly_pool_mod(0,2061,
            [&f,id](BattleContext*,const std::vector<int>& base) {
                f.observed_pools.push_back(base);
                CHECK(base==kPool,"C 实际基础池精确16码");
                return contains(base,id)?std::vector<int>{id}:std::vector<int>{};
            });
        f.ctx().seed_rng(61);
        own(f,0);
        CHECK_EQ(f.observed_pools.size(),1,"C 单成员交集前确实走过生产池");
        CHECK_EQ(active(f,1).size(),contains(kPool,id)?1:0,"C 池中/池外成员 "+std::to_string(id));
        if (contains(kPool,id)) {
            CHECK_EQ(active(f,1)[0],id,"C 真实目标命中指定成员");
            check_display(f,1,{id});
        }
        CHECK(active(f,0).empty(),"C 不误施加自身");
    }
    // D 参数边界、超池数钳制、池改写去重、不重抽、不把0当默认2。
    for (int count:{0,1,2,3,16,17,std::numeric_limits<int>::max()}) {
        Fight f(0,true,kAttr,count);
        f.ctx().seed_rng(37);
        own(f,0);
        const int n=std::min(count,16);
        CHECK_EQ(active(f,1).size(),n,"D 请求数按池大小钳制");
        CHECK_EQ(applied(f,1,0).size(),n,"D 不重复挑选不产生重复落地事件");
        CHECK_EQ(f.observed_pools.size(),count==0?0:1,"D 零种数不执行原语，正种数单子句");
        for (int id:active(f,1))
            CHECK_EQ(f.ctx().get_abnormal_status_end_round(1,id),2,"D 任意种数不改时长2");
        no_resident(f);
    }
    {
        Fight f(0,true,kAttr,3);
        f.pool(true,{44,44,4,4,32,32});
        own(f,0);
        CHECK(active(f,1)==std::vector<int>({4,32,44}),"D 解析池去重后恰三种，含寄生/凝滞/魇昧");
        CHECK_EQ(applied(f,1,0).size(),3,"D 重复池条目不增加尝试数");
        check_display(f,1,{4,32,44});
    }
    // E 同种已有不是新种失败，不补抽；长异常不被默认2缩短，短异常延到2。
    for (int rounds:{1,8}) {
        Fight f;
        f.pool(true,{1,44});
        for (int id:{1,44}) REQUIRE(apply_anomaly_raw(&f.ctx(),1,id,rounds,0)==ApplyAnomalyResult::SUCCESS);
        f.ctx().seed_rng(7);
        own(f,0);
        CHECK(active(f,1)==std::vector<int>({1,44}),"E 只有已有两种，不另补新种");
        CHECK_EQ(applied(f,1,0).size(),rounds==1?2:0,"E 仅延长才发落地事件，已有长异常不伪发");
        for (int id:{1,44})
            CHECK_EQ(f.ctx().get_abnormal_status_end_round(1,id),std::max(rounds,2),"E 同种只延长不缩短");
        CHECK_EQ(f.ctx().rng().state(),next(next(7)),"E 已有异常仍恰尝试2次、不补抽");
    }
    // F 整体免疫和一张次数免疫：尝试数锁在请求数，不是落地数，不因失败补抽。
    {
        Fight f(0,true,kAttr,3);
        guard(f,1);
        f.ctx().seed_rng(13);
        own(f,0);
        CHECK(active(f,1).empty(),"F Modern整免挡所有异常");
        CHECK_EQ(events(f,EventType::EVENT_ANOMALY_IMMUNED,1,0),3,"F 只有三次免疫事件、不补抽");
        CHECK_EQ(f.ctx().rng().state(),next(next(next(13))),"F 免疫失败仅消耗三次挑选");
        CHECK_EQ(f.observed_pools.size(),1,"F 失败不重新解析池/重试");
    }
    {
        Fight f;
        guard(f,1,1);
        f.ctx().seed_rng(13);
        own(f,0);
        CHECK_EQ(events(f,EventType::EVENT_ANOMALY_IMMUNED,1,0),1,"F 次免只挡一只");
        CHECK_EQ(applied(f,1,0).size(),1,"F 第二只照常落地，不补齐到2落地");
        CHECK_EQ(active(f,1).size(),1,"F 实际只有1种");
        CHECK_EQ(f.ctx().rng().state(),next(next(13)),"F 次免不改变挑选次数");
    }
    {
        Fight f;
        f.pool(true,{1,2,44});
        guard(f,1,0,1ULL<<1);
        unsigned seed=1;
        while (!contains(picks({1,2,44},2,seed),1)) ++seed;
        f.ctx().seed_rng(seed);
        own(f,0);
        CHECK_EQ(events(f,EventType::EVENT_ANOMALY_IMMUNED,1,0),1,"F 指定中毒免疫只挡对应码");
        CHECK_EQ(active(f,1).size(),1,"F 部分失败不补抽池中第三种");
        CHECK_EQ(f.ctx().rng().state(),next(next(seed)),"F 部分失败仍2次挑选");
    }
    {
        Fight f;
        f.pool(true,{44});
        REQUIRE(f.b->pet(1).resistance.setUncontrolSlot(0,44,100));
        f.ctx().seed_rng(31);
        own(f,0);
        CHECK(active(f,1)==std::vector<int>({21}),"F 抗性抵抗44仅产生抗性标记21，不落原码");
        CHECK_EQ(events(f,EventType::EVENT_ANOMALY_RESISTED,1,0),1,"F 唯一成员仅尝试一次，不因抗性补抽");
        CHECK_EQ(f.ctx().rng().state(),next(next(31)),"F 一次挑选+一次抗性判定，无额外尝试");
    }
    // G 概率闸门针对100%申报且来源Skill，关闭整句时不挑选、不扣次免。
    {
        Fight f;
        guard(f,1,1);
        const int gate_handle=gate(f,1,0);
        f.ctx().seed_rng(19);
        own(f,0);
        CHECK(active(f,1).empty(),"G 100%经闸门改写为0，无落地");
        CHECK_EQ(events(f,EventType::EVENT_ANOMALY_ATTACH_LOW_CHANCE,1,0),1,"G 整子句只申报一次Skill概率");
        CHECK_EQ(events(f,EventType::EVENT_ANOMALY_IMMUNED,1,0),0,"G 未挑选所以不触免疫");
        CHECK_EQ(f.ctx().rng().state(),19,"G 0%短路零随机数");
        f.ctx().rule_center_.revoke(gate_handle);
        own(f,0);
        CHECK_EQ(events(f,EventType::EVENT_ANOMALY_IMMUNED,1,0),1,"G 原次数免疫仍在，下一次才消耗");
        CHECK_EQ(active(f,1).size(),1,"G 解除闸门后恢复尝试");
    }
    // H 空池和禁附加都无落地；禁附加不等于补偿，也不会改成自身施加。
    {
        Fight f;
        f.pool(true,{});
        f.ctx().seed_rng(29);
        own(f,0);
        CHECK(active(f,1).empty(),"H 空有效池不落地");
        CHECK_EQ(f.ctx().rng().state(),29,"H 空池不挑选不耗随机数");
        CHECK_EQ(f.observed_pools.size(),1,"H 空池确实由正式池改写通道产生");
    }
    {
        Fight f(0,true,kAttack);
        f.ctx().rule_center_.grant_attach_ban(1,2061,0,RuleCenter::kAttachBanAnomaly,
                                             coverage_all(),0,f.ctx().roundCount);
        own(f,0);
        CHECK(active(f,1).empty() && active(f,0).empty(),"H 禁附加挡住Modern施加");
        CHECK_EQ(applied(f,1,0).size(),0,"H 禁附加无假落地事件");
        CHECK_EQ(f.observed_pools.size(),1,"H 禁附加失败不补抽");
    }
    // I 反弹保留真实归因/时长；转化可令实际落地码出基础池，不误当池污染。
    {
        Fight f(0,true,kAttack);
        f.pool(true,{4,44});
        guard(f,1);  // 原生回合弹控是IMMUNE+REFLECT双授，REFLECT标记本身不提供免疫。
        f.ctx().rule_center_.grant_reflect(1,0,2104,1,3);
        own(f,0);
        CHECK(active(f,1).empty(),"I 弹控侧不收原异常");
        CHECK(active(f,0)==std::vector<int>({4,44}),"I 两个原码均弹回施加方");
        CHECK_EQ(applied(f,0,1).size(),2,"I 反弹后的actor为原目标");
        CHECK_EQ(events(f,EventType::EVENT_ANOMALY_IMMUNED,1,0),2,"I 原目标两次免疫、不额外重抽");
        check_display(f,0,{4,44});
    }
    {
        Fight f;
        f.pool(true,{44});
        f.ctx().anomaly_conversion[1][44]=12;
        own(f,0);
        CHECK(active(f,1)==std::vector<int>({12}),"I Modern转化44→附属12（落地码可出基础池）");
        CHECK_EQ(events(f,EventType::EVENT_ANOMALY_CONVERTED,1,0),1,"I 只尝试一次唯一成员");
        CHECK_EQ(applied(f,1,0).size(),1,"I 转化成功正常发APPLIED");
    }
    // J 无效/未出招负测：miss、控制、被盔，不进被测原语；不是直接调用函数造假。
    for (int kind:{0,1,2}) {
        Fight f(0,true,kAttack);
        if (kind==0) { f.b->pet(0).skills[0].must_hit=0; f.b->pet(0).skills[0].accuracy=0; }
        if (kind==1) REQUIRE(apply_anomaly_raw(&f.ctx(),0,8,3,1)==ApplyAnomalyResult::SUCCESS);
        if (kind==2) (void)core_api().seal_skill(&f.ctx(),1,0,2061,false,true,1,0,false,
                                               -1,EffectScope::ON_STAGE,false,100,false);
        own(f,0);
        CHECK_EQ(f.observed_pools.size(),0,"J miss/被控/被盔不进2061");
        CHECK(active(f,1).empty(),"J 未执行无异常落地");
    }
    // K 双侧镜像：不带48的攻击载体，非封锁成员4/44，真实先后手各落各的。
    for (bool first:{true,false}) {
        Fight f(0,first,kAttack,-1,true);
        f.pool(true,{4,44});
        turn(f,0,0);
        CHECK_EQ(f.observed_pools.size(),2,"K 双侧各一次解析");
        for (int side:{0,1}) {
            CHECK(active(f,side)==std::vector<int>({4,44}),"K 双侧异常不串目标");
            CHECK_EQ(applied(f,side,1-side).size(),2,"K 双侧actor归因正确");
            check_display(f,side,{4,44});
        }
    }
    // L 自然到期/换宠清理、下一技能不残留、回场不继承上次施加。
    {
        Fight f(0,true,kAttack);
        f.pool(true,{4,44});
        own(f,0);
        CHECK(active(f,1)==std::vector<int>({4,44}),"L 首刀异常到位");
        own(f,0,1);
        CHECK_EQ(f.observed_pools.size(),0,"L 下一普通攻击不触2061");
        CHECK(active(f,1).empty(),"L 两回合后自然到期");
        own(f,0);
        swap(f,1,1);
        CHECK(active(f,1).empty(),"L 目标下场异常按通用规则清理");
        swap(f,1,0);
        CHECK(active(f,1).empty(),"L 目标回场无旧异常");
        swap(f,0,1);
        own(f,0,1);
        CHECK_EQ(f.observed_pools.size(),0,"L 施加者下场后普通攻击不残留2061");
        no_resident(f);
    }
    std::cout<<"scenario_8930_k504 PASS (2061 random debuff anomalies)\n";
    return 0;
}
