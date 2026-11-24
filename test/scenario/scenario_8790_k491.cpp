// K491 / 8790：2204 条件百分比威力；2102 因异常44内核缺口不注册、不伪测。
// 口径依据 〔源: docs_local/docs/05-任务清单/已收官/E组批量效果工单.md:96〕
// 数据来源 https://github.com/SeerAPI/seer-unity-assets ，2026-11-23 当前只读库全链重核。
// 真实FSM：四载体切片/施放、减25/50与增50互斥、0/100与非整乘积、当前视图而非静态值、
// 0威力也 rewritten、物/特/属性、对手而非自身、控制/弱化/附属类、遮蔽/过期/换宠清除、
// 双侧先后手镜像、同链1711/547施加后的条件、无跨技/跨场残留、miss/被控/被盔负测。
// 同时核对2102四载体切片，但不把缺44的随机池视为有效实现。
// 观察器在passive桶只读：技能桶先跑、被测改写后且伤害重算consume之前采样。
// 〔源: src/fsm/battleContext.cpp:1077〕〔源: src/fsm/battleContext.cpp:1081〕
// 写入就须打标，0%与0威力均不能省略写入；不是伤害AMP。
// 〔源: include/fsm/battleWorkspace.h:81〕〔源: include/fsm/battleWorkspace.h:87〕
// 2204 无驻留显示态，不挂伪窗；断言无活跃非actionOneShot条目，无G4新增债。
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
constexpr int kMove = 38257;  // 2204[50,50]，无护盾/目标无烧伤时兄弟链惰性。
constexpr int kQuarter = 37758;  // 2204[25,50]，无能力下降时1711惰性。
constexpr int kHit = 10004;
constexpr int kHp = 50000;
using Action = BattleFsm::ActionType;
std::array<int,2> observed_power{};
std::array<bool,2> observed_rewritten{};

EffectResult observe_power(BattleContext* c, const EffectArgs& a) {
    const int side = a.int_args[0];
    observed_power[side] = c->ws.skill_power_view[side].value;
    observed_rewritten[side] = c->ws.skill_power_view[side].rewritten;
    return EffectResult::kOk;
}
void watch_power(ScenarioBattle& b) {
    auto& c = b.ctx();
    observed_power = {-1,-1};
    observed_rewritten = {false,false};
    for (int side : {0,1}) {
        for (State state : {State::BATTLE_FIRST_ATTACK_DAMAGE, State::BATTLE_SECOND_ATTACK_DAMAGE}) {
            Effect e(2204,0,side,-1,EffectArgs(std::vector<int>{side}),&observe_power);
            auto ce = std::make_unique<ContinuousEffect>(e,state,side,-1,c.roundCount);
            ce->source_id_ = 2204;
            c.passive_effects.register_effect(state,side,std::move(ce),c.round_effect_valid_id[side]);
        }
    }
}
ElfPet carrier(int move, int speed, int reduction = -1, int increase = -1) {
    ElfPet p = make_pet(4868,"K491",{move,kHit,10013,26449,20001},speed,300,kHp);
    if (reduction >= 0 || increase >= 0) {
        const int monster = move == kQuarter ? 4715 : 4868;
        const auto m = official_data::OfficialDataStore::instance().repository().load_monster(monster);
        CHECK(m.has_value(),"fixture: 真实学习者存在");
        std::vector<EffectParamOverride> overrides;
        if (reduction >= 0) overrides.push_back({move,2204,0,reduction});
        if (increase >= 0) overrides.push_back({move,2204,1,increase});
        p.skills[0] = Skills(move,*m,overrides);
    }
    for (auto& skill : p.skills) {
        skill.must_hit = 1;
        skill.accuracy = 100;
        skill.critical_strike_rate = 0;
    }
    return p;
}
std::unique_ptr<ScenarioBattle> solo(int owner = 0, bool first = true, int move = kMove,
                                    int reduction = -1, int increase = -1) {
    auto p = carrier(move,first ? 300 : 100,reduction,increase);
    auto q = carrier(kMove,first ? 100 : 300);
    auto b = std::make_unique<ScenarioBattle>(make_party(owner==0 ? p : q),make_party(owner==0 ? q : p));
    b->start();
    watch_power(*b);
    return b;
}
void turn(ScenarioBattle& b, int skill0, int skill1) {
    const int round = b.ctx().roundCount;
    observed_power = {-1,-1};
    observed_rewritten = {false,false};
    b.submit_operation(0,skill0 < 0 ? Action::NONE : Action::SELECT_SKILL,std::max(0,skill0));
    b.submit_operation(1,skill1 < 0 ? Action::NONE : Action::SELECT_SKILL,std::max(0,skill1));
    b.drive();
    CHECK_EQ(b.ctx().roundCount,round+1,"bounded one-round drive");
}
void own_turn(ScenarioBattle& b, int side, int ours, int theirs = -1) {
    turn(b,side==0 ? ours : theirs,side==1 ? ours : theirs);
}
void check_attack(ScenarioBattle& b, int side, int skill, int power, bool rewritten,
                  const std::string& label, int theirs = -1, bool damage_band = true) {
    own_turn(b,side,skill,theirs);
    CHECK_EQ(b.ctx().ws.skill_power_view[side].value,power,label+" 威力视图");
    CHECK_EQ(observed_power[side],power,label+" 收尾前视图");
    CHECK_EQ(observed_rewritten[side],rewritten,label+" rewritten");
    if (damage_band) {
        // 读红伤台账，排除中毒等另行结算的扣血；面板300/100、无本系/克制、无暴击。
        const double raw = 0.84 * 300.0 / 100.0 * power + 2.0;
        const int damage = b.ctx().ws.raw_attack_damage[side];
        CHECK(damage >= static_cast<int>(std::floor(raw*0.85))-2
                  && damage <= static_cast<int>(std::floor(raw))+2,
              label+" 按改后威力重算红伤="+std::to_string(damage));
    }
}
void put_anomaly(ScenarioBattle& b, int side, int code, int rounds = 3) {
    REQUIRE(apply_anomaly_raw(&b.ctx(),side,code,rounds,1-side)==ApplyAnomalyResult::SUCCESS);
}
void no_resident(ScenarioBattle& b, const std::string& label) {
    const auto dump = b.dump();
    std::size_t pos=0;
    int active=0;
    while ((pos=dump.find("\"effectId\":2204,",pos))!=std::string::npos) {
        const auto end=dump.find('}',pos);
        const auto e=dump.substr(pos,end-pos);
        pos=end+1;
        if (e.find("\"isRoundEffect\":true")!=std::string::npos
            && e.find("\"actionOneShot\":false")!=std::string::npos
            && e.find("\"isExpired\":false")!=std::string::npos
            && e.find("\"invalidated\":false")!=std::string::npos) ++active;
    }
    CHECK_EQ(active,0,label+" 无驻留回合窗，不造显示义务");
}
}

int main() {
    if (!PetFactory::initialize_runtime_data()) return 1;
    sbs_srand(491);
    // A：当前库八载体全链，不把effect_info与side_effect两种计数混淆。
    {
        const std::vector<std::pair<int,std::vector<std::pair<int,std::vector<int>>>>> want = {
            {37758,{{2143,{}},{1711,{100,5}},{2204,{25,50}}}},
            {38084,{{1395,{}},{881,{}},{1054,{19}},{2204,{50,50}},{2341,{}}}},
            {38257,{{2204,{50,50}},{1281,{}},{2117,{2,22}}}},
            {38396,{{547,{10,3}},{2204,{50,50}},{672,{}}}},
            {28552,{{191,{4}},{426,{4,100}},{1937,{300}},{2102,{4,100,1}}}},
            {37353,{{2167,{3,2}},{2102,{2,100,1}},{1654,{1}}}},
            {38587,{{598,{100,1}},{438,{100,3}},{2102,{3,100,1}}}},
            {38599,{{2102,{2,100,1}},{54,{3,2}},{1217,{1}},{43,{2}}}}
        };
        auto& repo=official_data::OfficialDataStore::instance().repository();
        for (const auto& [move,chain] : want) {
            const auto got=repo.load_skill_effects(move);
            CHECK_EQ(got.size(),chain.size(),"A 载体链长");
            for (std::size_t i=0;i<chain.size();++i) {
                CHECK_EQ(got[i].effect_id,chain[i].first,"A 效果序");
                CHECK(got[i].args==chain[i].second,"A 全链参数无错切");
            }
        }
    }
    // B：真实四载体施放；38396 的547先上混乱，当刀即走增支。
    for (const auto [move,power] : std::vector<std::pair<int,int>>{{37758,112},{38084,150},{38257,60},{38396,240}}) {
        auto b=solo(0,true,move);
        check_attack(*b,0,0,power,true,"B 真实载体 "+std::to_string(move),-1,move!=38084);
        no_resident(*b,"B");
    }
    // C：两侧×先后手，攻击对方不变；条件必须读对手，先减再增/读自身均杀。
    for (int side : {0,1}) for (bool first : {true,false}) {
        auto b=solo(side,first);
        check_attack(*b,side,0,60,true,"C 无异常减半",1);
        CHECK_EQ(observed_power[1-side],40,"C 不改对手技能");
        put_anomaly(*b,side,1);
        check_attack(*b,side,0,60,true,"C 只有自身中毒仍减半",1);
        put_anomaly(*b,1-side,6,1);
        check_attack(*b,side,0,180,true,"C 对手害怕改为加50%");
        check_attack(*b,side,0,60,true,"C 对手异常到期恢复减支");
    }
    // D：异常不限控制类/低码，附属类也算；不从底层槽绕过虚拟异常视图。
    for (int code : {1,14,28,43}) {
        auto b=solo();
        put_anomaly(*b,1,code);
        check_attack(*b,0,0,180,true,"D 任意异常码 "+std::to_string(code));
    }
    {
        auto b=solo();
        put_anomaly(*b,1,1,5);
        core_api().set_anomaly_view_layers(&b->ctx(),2204,1,1);  // 只用场景原语造遮蔽，不改本体。
        CHECK(b->ctx().has_active_abnormal_status(1,1),"D 本体异常仍在");
        check_attack(*b,0,0,60,true,"D 视为无异常走减支");
        core_api().set_anomaly_view_layers(&b->ctx(),2204,1,0);
        check_attack(*b,0,0,180,true,"D 遮蔽解除恢复增支");
    }
    // E：两参独立消费，整积后截断；0%不是“不写”、100%减至0、提升可超过100%。
    for (const auto& p : std::vector<std::array<int,4>>{{35,80,73,203},{0,0,113,113},{100,150,0,282}}) {
        auto b=solo(0,true,kMove,p[0],p[1]);
        b->pet(0).skills[0].power=113;
        check_attack(*b,0,0,p[2],true,"E 无异常行参");
        put_anomaly(*b,1,6);
        check_attack(*b,0,0,p[3],true,"E 有异常行参");
    }
    // F：读当前视图而非静态120，含烧伤物化、已有倍增窗、0威力、实际类型。
    {
        auto b=solo();
        put_anomaly(*b,0,2,5);
        check_attack(*b,0,0,30,true,"F 烧伤60再减半");
        put_anomaly(*b,1,6);
        check_attack(*b,0,0,90,true,"F 烧伤60改增50%");
        auto window=solo();
        own_turn(*window,0,3);  // 真实480[2]，下一轮仍在窗。
        check_attack(*window,0,0,120,true,"F 2204与480复合");
        auto zero=solo();
        zero->pet(0).skills[0].power=0;
        check_attack(*zero,0,0,0,true,"F 零威力仍改写");
        auto attr=solo();
        attr->pet(0).skills[0].type=SkillType::Attribute;
        const int hp=attr->pet(1).hp;
        own_turn(*attr,0,0);
        CHECK_EQ(attr->ctx().ws.skill_power_view[0].value,120,"F 属性视图不改");
        CHECK(!attr->ctx().ws.skill_power_view[0].rewritten,"F 属性无改写");
        CHECK_EQ(attr->pet(1).hp,hp,"F 属性无红伤");
    }
    // G：同链1711先清下降并附加冻伤，条件在SKILL_EFFECT现场读，不在选择期锁死。
    {
        auto b=solo(0,true,kQuarter);
        REQUIRE(stat_drop(&b->ctx(),0,0,1,1)==StatDropResult::SUCCESS);
        check_attack(*b,0,0,225,true,"G 同链新上异常即时生效");
        CHECK(b->ctx().has_active_abnormal_status(1,5),"G 真实冻伤落地");
    }
    // H：两侧同回合分别减25/50；下一技能、NONE、离场回场不继承。
    {
        auto b=solo(0,true,kQuarter);
        turn(*b,0,0);
        CHECK_EQ(observed_power[0],112,"H 双侧独立25%");
        CHECK_EQ(observed_power[1],60,"H 双侧独立50%");
        turn(*b,1,2);
        CHECK_EQ(observed_power[0],40,"H 无跨技遗留0");
        CHECK_EQ(observed_power[1],40,"H 无跨技遗留1");
        CHECK(!observed_rewritten[0] && !observed_rewritten[1],"H 非载体不改写");
        turn(*b,-1,-1);
        no_resident(*b,"H NONE");
    }
    for (int side : {0,1}) {
        auto b=solo(side);
        put_anomaly(*b,1-side,6,5);
        check_attack(*b,side,0,180,true,"H 离场前增支");
        b->submit_operation(1-side,Action::CHOOSE_PET,1);
        b->submit_operation(side,Action::NONE,0);
        b->drive();
        CHECK_EQ(b->ctx().on_stage[1-side],1,"H 对手已换宠");
        watch_power(*b);
        check_attack(*b,side,0,60,true,"H 新对手无异常");
        b->submit_operation(side,Action::CHOOSE_PET,1);
        b->submit_operation(1-side,Action::NONE,0);
        b->drive();
        CHECK_EQ(b->ctx().on_stage[side],1,"H 自身已换宠");
        watch_power(*b);
        check_attack(*b,side,1,40,false,"H 替补无残留");
        no_resident(*b,"H 离场");
    }
    // I：miss、被控、被盔不会通过命中效果；后续无载体仍无改写。
    {
        auto miss=solo();
        miss->pet(0).skills[0].must_hit=0;
        miss->pet(0).skills[0].accuracy=0;
        own_turn(*miss,0,0);
        CHECK_EQ(miss->ctx().ws.raw_attack_damage[0],0,"I miss无红伤");
        CHECK(!observed_rewritten[0],"I miss不改威力");
        auto held=solo();
        put_anomaly(*held,0,6,1);
        own_turn(*held,0,0);
        CHECK_EQ(held->ctx().ws.raw_attack_damage[0],0,"I 被控未使用");
        check_attack(*held,0,1,40,false,"I 被控后无残留");
        auto seal=solo();
        (void)core_api().seal_skill(&seal->ctx(),1,0,2204,false,true,1,0,false,
                                   -1,EffectScope::ON_STAGE,false,100,false);
        own_turn(*seal,0,0);
        CHECK_EQ(seal->ctx().ws.raw_attack_damage[0],0,"I 被盔无红伤");
        CHECK(!observed_rewritten[0],"I 被盔不改威力");
        check_attack(*seal,0,1,40,false,"I 盔后无残留");
    }
    std::cout << "scenario_8790_k491 PASS (2204 only; 2102 blocked by anomaly44)\n";
    return 0;
}
