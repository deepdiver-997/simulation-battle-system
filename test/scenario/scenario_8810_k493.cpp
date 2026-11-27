// K493 / 8810：2137 自身当前异常剩余回合合计×步长，增加百分比封顶。
// 口径依据 〔源: docs_local/docs/05-任务清单/已收官/E组批量效果工单.md:98〕
// 数据来源 https://github.com/SeerAPI/seer-unity-assets （2026-11-23 当前只读库重核）。
// 官方无实测：不是历史累计/当前布尔/种数/最大单项；37997数据链先2137再2181清自身异常。
// 2181本基点未注册：G以SKILL_EFFECT后继被动桩调用正式解除原语，只锁2137采样边界，
// 不宣称2181已实现、不在生产注册它。四载体完整链及不同步长/封顶均用真实FSM验证。
// 2061仅核五载体切片，不伪测截短随机池；弱化池含44而内核只到43，沿K491 Patch A挂账。
// 〔源: include/abnormal-system/abnormal-types.h:13〕
// 〔源: resources/moves_lib/k491_conditional_power.cpp:29-35〕
// 断言面：线性/恰顶/超顶、多异常求和、非零回合基点、遮蔽、清除/到期、同链清异常、
// 两侧×先后手、同回合双侧、离场回场、不串下一技能、合法0参/非整档、64位乘积、
// AMP加算不改威力/不变AMP_EXTRA、既有增伤不被封顶吞掉、miss/被控/被盔负测。
// 观测：passive桶读取威力与增伤槽，TEAM watcher仅记录NORMAL实际伤害（显式valid_id）。
// 〔源: src/fsm/battleContext.cpp:1077〕〔源: src/fsm/battleContext.cpp:1081〕
// 本效果无驻留显示态：验无活跃回合窗、不分配storage键，不造空描述或伪窗口。
// 〔源: docs/07-工具与测试/效果显示gate规范.md:39〕

#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <entities/pet_factory.h>
#include <plugin/core_api.h>
#include "scenario_harness.h"

using namespace scenario;
namespace {
constexpr int kMove = 38195;  // 2137[25,150]；无能力下降时881/623惰性。
constexpr int kCure = 37997;  // 2137[30,90]→2181清自身异常→3清自身下降。
constexpr int kLowCap = 37764;  // 2137[10,70]；无PP归零、无对手窗口时兄弟链惰性。
constexpr int kHit = 10004;
constexpr int kHp = 100000;
using Action = BattleFsm::ActionType;
std::array<int,2> observed_amp{};
std::array<int,2> observed_power{};
std::array<bool,2> observed_rewritten{};
std::array<int,2> red_damage{};

EffectResult observe(BattleContext* c, const EffectArgs& a) {
    const int side = a.int_args[0];
    observed_amp[side] = static_cast<int>(c->ws.damage_add_pct[side]);
    observed_power[side] = c->ws.skill_power_view[side].value;
    observed_rewritten[side] = c->ws.skill_power_view[side].rewritten;
    return EffectResult::kOk;
}
void watch_views(ScenarioBattle& b) {
    auto& c = b.ctx();
    for (int side : {0,1}) {
        for (State st : {State::BATTLE_FIRST_ATTACK_DAMAGE,State::BATTLE_SECOND_ATTACK_DAMAGE}) {
            Effect e(2137,0,side,-1,EffectArgs(std::vector<int>{side}),&observe);
            auto ce = std::make_unique<ContinuousEffect>(e,st,side,-1,c.roundCount);
            ce->source_id_ = 2137;
            c.passive_effects.register_effect(st,side,std::move(ce),c.round_effect_valid_id[side]);
        }
    }
}
void watch_damage(ScenarioBattle& b) {
    auto& c = b.ctx();
    c.event_center_.register_watcher(EventType::EVENT_TAKE_DAMAGE,0,c.roundCount,0,false,
        [](BattleContext*,const BattleEvent& e) {
            if (e.damage_kind==static_cast<int>(DamageKind::NORMAL)
                && e.actor>=0 && e.actor<2 && e.target==1-e.actor) red_damage[e.actor]+=e.amount;
        },WatcherScope::TEAM,c.watcher_valid_id[0]);
}
int learner(int move) {
    if (move==37226) return 4623;
    if (move==kLowCap) return 4716;
    if (move==kCure) return 4788;
    return 4850;
}
ElfPet carrier(int move, int speed, int step=-1, int cap=-1) {
    ElfPet p=make_pet(learner(move),"K493",{move,kHit,10013,20001,kCure},speed,300,kHp);
    if (step>=0 || cap>=0) {
        const auto m=official_data::OfficialDataStore::instance().repository().load_monster(learner(move));
        REQUIRE(m.has_value());
        std::vector<EffectParamOverride> overrides;
        if (step>=0) overrides.push_back({move,2137,0,step});
        if (cap>=0) overrides.push_back({move,2137,1,cap});
        p.skills[0]=Skills(move,*m,overrides);
    }
    for (auto& s:p.skills) {
        s.must_hit=1;
        s.accuracy=100;
        s.critical_strike_rate=0;
        s.pp=100;  // 避免37764兄弟2174的PP归零门；不改效果链。
    }
    return p;
}
std::unique_ptr<ScenarioBattle> solo(int owner=0, bool first=true, int move=kMove,
                                    int step=-1, int cap=-1) {
    auto p=carrier(move,first?300:100,step,cap);
    auto q=carrier(kMove,first?100:300);
    auto b=std::make_unique<ScenarioBattle>(make_party(owner==0?p:q),make_party(owner==0?q:p));
    b->start();
    watch_views(*b);
    watch_damage(*b);
    return b;
}
void turn(ScenarioBattle& b, int skill0, int skill1) {
    observed_amp={-1,-1};
    observed_power={-1,-1};
    observed_rewritten={false,false};
    red_damage={0,0};
    const int before=b.ctx().roundCount;
    b.submit_operation(0,skill0<0?Action::NONE:Action::SELECT_SKILL,std::max(0,skill0));
    b.submit_operation(1,skill1<0?Action::NONE:Action::SELECT_SKILL,std::max(0,skill1));
    b.drive();
    CHECK_EQ(b.ctx().roundCount,before+1,"有界单回合驱动");
}
void own_turn(ScenarioBattle& b,int side,int skill,int theirs=-1) {
    turn(b,side==0?skill:theirs,side==1?skill:theirs);
}
void put(ScenarioBattle& b,int side,int code,int rounds) {
    REQUIRE(apply_anomaly_raw(&b.ctx(),side,code,rounds,1-side)==ApplyAnomalyResult::SUCCESS);
}
void check_result(ScenarioBattle& b,int side,int amp,int power,const std::string& label,
                  bool exact_damage=true) {
    CHECK_EQ(observed_amp[side],amp,label+" AMP百分比");
    CHECK_EQ(observed_power[side],power,label+" 未挪到威力通道");
    CHECK(!observed_rewritten[side],label+" 未伪造威力改写");
    for (int v:b.ctx().ws.damage_add_extra_mul[side]) CHECK_EQ(v,0,label+" 无AMP_EXTRA");
    const int raw=b.ctx().ws.raw_attack_damage[side];
    const double base=0.84*300.0/100.0*power+2.0;
    CHECK(raw>=static_cast<int>(std::floor(base*0.85))-2
              && raw<=static_cast<int>(std::floor(base))+2,label+" 原始红伤浮动带");
    if (exact_damage) {
        CHECK_EQ(red_damage[side],static_cast<int>(static_cast<long long>(raw)*(100+amp)/100),
                 label+" 真实NORMAL落地伤害=raw×(100+AMP)%");
    }
}
void cast(ScenarioBattle& b,int side,int amp,const std::string& label,int theirs=-1,
          int power=150,bool exact_damage=true) {
    own_turn(b,side,0,theirs);
    check_result(b,side,amp,power,label,exact_damage);
}
void no_resident(ScenarioBattle& b,const std::string& label) {
    const auto text=b.dump();
    std::size_t pos=0;
    int active=0;
    while ((pos=text.find("\"effectId\":2137,",pos))!=std::string::npos) {
        const auto end=text.find('}',pos);
        const auto entry=text.substr(pos,end-pos);
        pos=end+1;
        if (entry.find("\"isRoundEffect\":true")!=std::string::npos
            && entry.find("\"actionOneShot\":false")!=std::string::npos
            && entry.find("\"isExpired\":false")!=std::string::npos
            && entry.find("\"invalidated\":false")!=std::string::npos) ++active;
    }
    CHECK_EQ(active,0,label+" 2137无驻留回合窗");
    for (const auto& item:b.ctx().plugin_storage) {
        CHECK(!(item.first>=1021370000 && item.first<1021380000),label+" 2137不占storage键");
        CHECK(!(item.first>=1020610000 && item.first<1020620000),label+" blocked2061不占键");
    }
}
EffectResult existing_amp(BattleContext* c,const EffectArgs& a) {
    c->ws.damage_add_pct[a.int_args[0]]+=40;
    return EffectResult::kOk;
}
void add_existing_amp(ScenarioBattle& b,int owner) {
    const auto st=State::BATTLE_ROUND_START;
    Effect e(2137,0,owner,-1,EffectArgs(std::vector<int>{owner}),&existing_amp);
    auto ce=std::make_unique<ContinuousEffect>(e,st,owner,-1,b.ctx().roundCount);
    ce->source_id_=2137;
    b.ctx().passive_effects.register_effect(st,owner,std::move(ce),b.ctx().round_effect_valid_id[owner]);
}
// 仅测试脚手架：技能效果桶之后、伤害相位之前解除异常。不注册/补做2181。
EffectResult cleanse_after_skill(BattleContext* c,const EffectArgs& a) {
    (void)core_api().dispel_active_anomalies(c,a.int_args[0]);
    return EffectResult::kOk;
}
void add_cleanse_fixture(ScenarioBattle& b,int owner) {
    for (State st:{State::BATTLE_FIRST_SKILL_EFFECT,State::BATTLE_SECOND_SKILL_EFFECT}) {
        Effect e(2137,0,owner,-1,EffectArgs(std::vector<int>{owner}),&cleanse_after_skill);
        auto ce=std::make_unique<ContinuousEffect>(e,st,owner,1,b.ctx().roundCount);
        ce->source_id_=2137;
        b.ctx().passive_effects.register_effect(st,owner,std::move(ce),b.ctx().round_effect_valid_id[owner]);
    }
}
}

int main() {
    if (!PetFactory::initialize_runtime_data()) return 1;
    // A：当前库9载体全链切片。2061五条只核加载数据，不伪装已有随机实现。
    {
        const std::vector<std::pair<int,std::vector<std::pair<int,std::vector<int>>>>> want={
            {37226,{{195,{}},{1187,{100,28}},{2137,{30,150}}}},
            {37764,{{1114,{2}},{2174,{70}},{2137,{10,70}},{2232,{1}}}},
            {37997,{{2137,{30,90}},{2181,{}},{3,{}}}},
            {38195,{{881,{}},{623,{1}},{2137,{25,150}}}},
            {28589,{{48,{3}},{2061,{2}}}},
            {29125,{{191,{4}},{819,{0,50}},{2061,{3}}}},
            {36923,{{2061,{2}},{964,{}},{1960,{2}}}},
            {36968,{{2061,{2}},{914,{5,2,100}},{96,{}}}},
            {38658,{{789,{1}},{2061,{2}},{2355,{3,8,3}},{820,{2,2}},{185,{8}}}}
        };
        auto& repo=official_data::OfficialDataStore::instance().repository();
        for (const auto& [move,chain]:want) {
            const auto got=repo.load_skill_effects(move);
            CHECK_EQ(got.size(),chain.size(),"A 载体链长");
            for (std::size_t i=0;i<chain.size();++i) {
                CHECK_EQ(got[i].effect_id,chain[i].first,"A 效果顺序");
                CHECK(got[i].args==chain[i].second,"A 双表全链切片");
            }
        }
    }
    // B：两侧×先后手；只有对手异常不增伤；自身剩余3→2→1→0逐档下降而非历史递增。
    for (int side:{0,1}) for (bool first:{true,false}) {
        auto b=solo(side,first);
        put(*b,1-side,1,10);
        cast(*b,side,0,"B 仅对手异常",1);
        put(*b,side,1,3);
        for (int n:{3,2,1,0}) {
            cast(*b,side,25*n,"B 当前剩余"+std::to_string(n),1);
            CHECK_EQ(observed_amp[1-side],0,"B 不改对手的普通攻击");
        }
        no_resident(*b,"B");
    }
    // C：四个真实参数档位，分别锁线性段、恰封顶、超封顶；不只测默认150档。
    for (const auto& c:std::vector<std::array<int,3>>{{37226,30,150},{kLowCap,10,70},
                                                    {kCure,30,90},{kMove,25,150}}) {
        const int top=c[2]/c[1];
        for (int n:{top-1,top,top+1}) {
            auto b=solo(0,true,c[0]);
            put(*b,0,1,n);
            cast(*b,0,std::min(n*c[1],c[2]),"C 载体"+std::to_string(c[0])+" 回合"+std::to_string(n));
            no_resident(*b,"C");
        }
    }
    // D：两种2+3=5回合（非max3/种数2/布尔1）；附属类43也计入，双侧互不串账。
    {
        auto b=solo();
        put(*b,0,1,2);
        put(*b,0,13,3);
        cast(*b,0,125,"D 多异常剩余相加");
    }
    {
        auto b=solo();
        put(*b,0,43,2);
        cast(*b,0,50,"D 高码附属类也计数");
    }
    {
        auto b=solo();
        put(*b,0,1,2);
        put(*b,1,1,4);
        turn(*b,0,0);
        check_result(*b,0,50,150,"D 双侧0");
        check_result(*b,1,100,150,"D 双侧1");
    }
    // E：非零回合基点；解除历史不保留；重新附加从新剩余量开始；普通攻击不继承。
    {
        auto b=solo();
        for (int i=0;i<3;++i) turn(*b,-1,-1);
        put(*b,0,1,3);
        cast(*b,0,75,"E round3看剩余非绝对end");
        REQUIRE(core_api().dispel_active_anomalies(&b->ctx(),0)>0);
        cast(*b,0,0,"E 已解除无历史累计");
        put(*b,0,13,2);
        cast(*b,0,50,"E 重新附加");
        own_turn(*b,0,1);
        check_result(*b,0,0,40,"E 非载体不继承");
    }
    // F：虚拟异常视图遮蔽下本体仍有槽，但“自身处于”判据必须读查询面。
    {
        auto b=solo();
        put(*b,0,1,4);
        put(*b,0,13,3);
        core_api().set_anomaly_view_layers(&b->ctx(),2137,0,1);
        CHECK(b->ctx().has_active_abnormal_status(0,1),"F 遮蔽不清本体");
        cast(*b,0,0,"F 视为无异常");
        core_api().set_anomaly_view_layers(&b->ctx(),2137,0,0);
        cast(*b,0,125,"F 遮蔽解除剩余3+2");
    }
    // G：37997数据链要求先增伤后解除；2181缺位，用正式原语后继桩锁采样边界。
    for (int side:{0,1}) {
        auto b=solo(side,true,kCure);
        put(*b,side,1,2);
        add_cleanse_fixture(*b,side);
        cast(*b,side,60,"G 后继解除不抹掉本刀快照");
        CHECK(!core_api().has_active_anomaly(&b->ctx(),side,1),"G 后继桩确已解除异常");
        cast(*b,side,0,"G 下刀不保留已解除异常");
    }
    // H：封顶仅管本效果贡献；与已有+40%加算，非覆盖、非AMP_EXTRA乘算。
    {
        auto b=solo();
        put(*b,0,1,7);
        add_existing_amp(*b,0);
        cast(*b,0,190,"H 自身150封顶不吞既有40");
    }
    // I：参数覆盖仍走正式Skills装配；0合法、非整档、先乘64位后封顶。
    for (const auto& p:std::vector<std::array<int,4>>{{0,150,3,0},{25,0,3,0},{17,61,3,51},
                                                   {17,61,4,61},{std::numeric_limits<int>::max(),90,3,90}}) {
        auto b=solo(0,true,kMove,p[0],p[1]);
        put(*b,0,1,p[2]);
        cast(*b,0,p[3],"I 行参边界");
    }
    // J：烧伤物化半威力与本效果不同通道；按实际剩余算，但不再次改威力。
    {
        auto b=solo();
        put(*b,0,2,2);
        cast(*b,0,50,"J 烧伤半威力+伤害增幅",-1,75);
    }
    // K：自身离场后异常自然清理，替补/回场不继承旧档；两侧镜像。
    for (int side:{0,1}) {
        auto b=solo(side);
        put(*b,side,1,5);
        cast(*b,side,125,"K 离场前");
        for (int slot:{1,0}) {
            b->submit_operation(side,Action::CHOOSE_PET,slot);
            b->submit_operation(1-side,Action::NONE,0);
            b->drive();
            CHECK_EQ(b->ctx().on_stage[side],slot,"K 已切换到指定槽");
            watch_views(*b);
            cast(*b,side,0,"K 替补/回场无旧异常增幅");
            no_resident(*b,"K");
        }
    }
    // L：miss、被控、被盔不走命中效果；下回合普通攻击无污染。
    {
        auto b=solo();
        put(*b,0,1,3);
        b->pet(0).skills[0].must_hit=0;
        b->pet(0).skills[0].accuracy=0;
        own_turn(*b,0,0);
        CHECK_EQ(b->ctx().ws.damage_add_pct[0],0,"L miss不增伤");
        CHECK_EQ(red_damage[0],0,"L miss零NORMAL");
    }
    {
        auto b=solo();
        put(*b,0,6,1);
        own_turn(*b,0,0);
        CHECK_EQ(b->ctx().ws.damage_add_pct[0],0,"L 被控不执行");
        CHECK_EQ(red_damage[0],0,"L 被控无攻击伤害");
        own_turn(*b,0,1);
        check_result(*b,0,0,40,"L 被控后普通攻击");
    }
    {
        auto b=solo();
        put(*b,0,1,3);
        (void)core_api().seal_skill(&b->ctx(),1,0,2137,false,true,1,0,false,
                                   -1,EffectScope::ON_STAGE,false,100,false);
        own_turn(*b,0,0);
        CHECK_EQ(b->ctx().ws.damage_add_pct[0],0,"L 被盔不增伤");
        CHECK_EQ(red_damage[0],0,"L 被盔零NORMAL");
        own_turn(*b,0,1);
        check_result(*b,0,0,40,"L 盔后无遗留");
    }
    // M：同MINSTD种子下0%/有效增伤不移动随机序列；这不是2061随机池的验收替代。
    std::array<std::uint64_t,2> rng{};
    std::array<int,2> raw{};
    for (int i:{0,1}) {
        auto b=solo(0,true,kMove,i?25:0,150);
        put(*b,0,1,3);
        b->ctx().seed_rng(8810);
        cast(*b,0,i?75:0,"M 2137不额外消费随机数");
        rng[i]=b->ctx().rng().state();
        raw[i]=b->ctx().ws.raw_attack_damage[0];
    }
    CHECK_EQ(rng[0],rng[1],"M 同种子随机状态一致");
    CHECK_EQ(raw[0],raw[1],"M 同种子原始伤害一致");
    std::cout<<"scenario_8810_k493 PASS (2137 only; 2061 blocked by anomaly44)\n";
    return 0;
}
