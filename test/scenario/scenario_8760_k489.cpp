// K489 / 8760：480 威力倍增窗；1935 因次数反弹内核缺口不在交付范围。
// 口径依据 〔源: docs_local/docs/05-任务清单/已收官/E组批量效果工单.md:94〕
// 数据来源 https://github.com/SeerAPI/seer-unity-assets ，2026-11-23 当前只读库逐链重核。
// 真实 FSM：五载体切片与施放、先后手/双侧镜像、InRounds 首尾、行参覆盖、刷新、
// 威力面（物/特、不同基值、179 加值/118 改值后的乘算、烧伤、0 威力）、红伤重算、
// 属性技/NONE/被控负测、主动离场和断回合、fullstate 真窗口与 args 显示。
// 倍乘写入应触发 rewritten，而 materialize/直接 value 赋值不算效果改写。
// 〔源: include/fsm/battleWorkspace.h:81〕〔源: include/fsm/battleWorkspace.h:87〕
// 观察器仅在场景的 passive 桶读取 ATTACK_DAMAGE 后、finish 前的视图，不改结算。
// 〔源: src/fsm/battleContext.cpp:1077〕〔源: src/fsm/battleContext.cpp:1081〕
// N>=2 后手顺延，先手含施放回合；观察攻击次数不能误当作回合窗长度。
// 〔源: include/fsm/battleContext.h:935〕〔源: include/fsm/battleContext.h:936〕

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
constexpr int kArm = 26449;  // 480[2] 纯链
constexpr int kHit = 10004;  // 拍打，物理40，纯链
constexpr int kSpecial = 10013;  // 水枪，特殊40，纯链
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
            Effect e(480,0,side,-1,EffectArgs(std::vector<int>{side}),&observe_power);
            auto ce = std::make_unique<ContinuousEffect>(e,state,side,-1,c.roundCount);
            ce->source_id_ = 480;
            c.passive_effects.register_effect(state,side,std::move(ce),c.round_effect_valid_id[side]);
        }
    }
}
ElfPet carrier(int speed = 300, int rounds = 2, int move = kArm) {
    ElfPet p = make_pet(3749,"K489",{move,kHit,kSpecial,12078,20001},speed,300,kHp);
    if (rounds != 2) {
        const auto m = official_data::OfficialDataStore::instance().repository().load_monster(3749);
        CHECK(m.has_value(), "fixture: 3749 真实学习者存在");
        p.skills[0] = Skills(kArm,*m,{{kArm,480,0,rounds}});
    }
    for (auto& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
        s.critical_strike_rate = 0;
    }
    return p;
}
std::unique_ptr<ScenarioBattle> solo(int owner, bool first = true, int rounds = 2,
                                    int move = kArm) {
    auto p = carrier(first ? 300 : 100,rounds,move);
    auto q = carrier(first ? 100 : 300);
    auto b = std::make_unique<ScenarioBattle>(make_party(owner==0 ? p : q),make_party(owner==0 ? q : p));
    b->start();
    watch_power(*b);
    return b;
}
void turn(ScenarioBattle& b, int skill0, int skill1) {
    const int before = b.ctx().roundCount;
    b.submit_operation(0,skill0 < 0 ? Action::NONE : Action::SELECT_SKILL,std::max(0,skill0));
    b.submit_operation(1,skill1 < 0 ? Action::NONE : Action::SELECT_SKILL,std::max(0,skill1));
    b.drive();
    CHECK_EQ(b.ctx().roundCount,before+1,"bounded one-round drive");
}
void own_turn(ScenarioBattle& b, int side, int ours, int theirs = -1) {
    turn(b,side==0 ? ours : theirs,side==1 ? ours : theirs);
}
void check_attack(ScenarioBattle& b, int side, int skill, int power, bool rewritten,
                  const std::string& label, int theirs = -1) {
    const int hp = b.pet(1-side).hp;
    own_turn(b,side,skill,theirs);
    CHECK_EQ(b.ctx().ws.skill_power_view[side].value,power,label+" 威力视图");
    CHECK_EQ(observed_power[side],power,label+" 收尾前视图");
    CHECK_EQ(observed_rewritten[side],rewritten,label+" 变威力标记");
    const double raw = 0.84 * 300.0 / 100.0 * power + 2.0;
    const int lo = static_cast<int>(std::floor(raw*0.85))-2;
    const int hi = static_cast<int>(std::floor(raw))+2;
    const int damage = hp-b.pet(1-side).hp;
    CHECK(damage>=lo && damage<=hi,label+" 红伤由威力面重算 damage="+std::to_string(damage));
}
std::vector<std::string> visible_windows(ScenarioBattle& b, int side) {
    const std::string dump = b.dump();
    std::vector<std::string> entries;
    std::size_t pos = 0;
    while ((pos=dump.find("\"effectId\":480,",pos))!=std::string::npos) {
        const std::size_t end = dump.find('}',pos);
        const std::string e = dump.substr(pos,end-pos);
        pos = end+1;
        if (e.find("\"owner\":"+std::to_string(side)+",")!=std::string::npos
            && e.find("\"isRoundEffect\":true")!=std::string::npos
            && e.find("\"actionOneShot\":false")!=std::string::npos
            && e.find("\"isExpired\":false")!=std::string::npos
            && e.find("\"invalidated\":false")!=std::string::npos) entries.push_back(e);
    }
    return entries;
}
void check_window(ScenarioBattle& b,int side,int rounds,int start,const std::string& label) {
    const auto entries = visible_windows(b,side);
    CHECK_EQ(entries.size(),2U,label+" FIRST/SECOND 双桶且不重复");
    for (const auto& e : entries) {
        CHECK(e.find("\"sourceId\":480,")!=std::string::npos,label+" 显示来源=480");
        CHECK(e.find("\"duration\":"+std::to_string(rounds)+",")!=std::string::npos,label+" 显示原回合数");
        CHECK(e.find("\"registeredRound\":"+std::to_string(start)+",")!=std::string::npos,label+" InRounds起点");
        const std::string args = "\"args\":["+std::to_string(side)+","+std::to_string(1-side)+","+std::to_string(rounds)+"]";
        CHECK(e.find(args)!=std::string::npos,label+" 模板参数保留（含owner/target前缀）");
    }
}
}

int main() {
    if (!PetFactory::initialize_runtime_data()) return 1;
    sbs_srand(489);
    // A：当前库五链双表切片，并逐载体真实施放。
    {
        const std::vector<std::pair<int,std::vector<std::pair<int,std::vector<int>>>>> want = {
            {24029,{{707,{4,3,3}},{480,{2}},{764,{3,3}}}},
            {24083,{{462,{3,200}},{480,{2}}}},
            {24162,{{585,{1,1,0,1,1,1}},{57,{3,4}},{480,{2}}}},
            {24315,{{48,{4}},{128,{1}},{480,{2}},{198,{2,2}}}},
            {26449,{{480,{2}}}}
        };
        auto& repo = official_data::OfficialDataStore::instance().repository();
        for (const auto& [move,chain] : want) {
            const auto recs = repo.load_skill_effects(move);
            CHECK_EQ(recs.size(),chain.size(),"A 载体链长");
            for (std::size_t i=0;i<chain.size();++i) {
                CHECK_EQ(recs[i].effect_id,chain[i].first,"A 效果序");
                CHECK(recs[i].args==chain[i].second,"A 全链参数无错切");
            }
            auto b=solo(0,true,2,move);
            own_turn(*b,0,0);
            check_window(*b,0,2,0,"A 真实载体 "+std::to_string(move));
            own_turn(*b,0,1);
            CHECK_EQ(b->ctx().ws.skill_power_view[0].value,80,"A 五载体均使后续攻击威力翻倍");
        }
    }
    // B：先手 N=2 是施放回合+下一回合；双侧、物/特、窗外基线。
    for (int side : {0,1}) {
        auto b=solo(side);
        check_attack(*b,side,1,40,false,"B 授予前");
        own_turn(*b,side,0);
        check_window(*b,side,2,1,"B 先手施放");
        check_attack(*b,side,side==0 ? 1 : 2,80,true,"B 窗内");
        check_attack(*b,side,1,40,false,"B 窗外");
        CHECK(visible_windows(*b,side).empty(),"B 过期显示窗消失");
    }
    // C：后手 N=2 顺延，接下来两轮均有效；对手不受倍增影响。
    for (int side : {0,1}) {
        auto b=solo(side,false);
        own_turn(*b,side,0,1);
        check_window(*b,side,2,1,"C 后手施放");
        check_attack(*b,side,1,80,true,"C 第1轮",1);
        CHECK_EQ(b->ctx().ws.skill_power_view[1-side].value,40,"C 不改对手威力");
        check_attack(*b,side,2,80,true,"C 第2轮",1);
        check_attack(*b,side,1,40,false,"C 过窗",1);
    }
    // D：装配期行参覆盖（不改 DB），不能写死2；N=1不延长到下回合。
    {
        auto b=solo(0,true,3);
        own_turn(*b,0,0);
        check_window(*b,0,3,0,"D 行参3");
        check_attack(*b,0,1,80,true,"D 第1轮");
        check_attack(*b,0,2,80,true,"D 第2轮");
        check_attack(*b,0,1,40,false,"D 回合3到期");
        auto one=solo(1,false,1);
        own_turn(*one,1,0,1);
        check_attack(*one,1,1,40,false,"D 后手N1不顺延",1);
    }
    // E：复用刷新而非叠加；两侧同时持有时窗界分别结算。
    {
        auto b=solo(0);
        own_turn(*b,0,0);
        own_turn(*b,0,0);
        check_window(*b,0,2,1,"E 重施刷新");
        check_attack(*b,0,1,80,true,"E 不是四倍");
        check_attack(*b,0,1,40,false,"E 刷新后到期");
        auto m=solo(0);
        turn(*m,0,0);
        check_window(*m,0,2,0,"E 镜像先手");
        check_window(*m,1,2,1,"E 镜像后手");
        turn(*m,1,1);
        CHECK_EQ(m->ctx().ws.skill_power_view[0].value,80,"E 镜像双方各翻倍0");
        CHECK_EQ(m->ctx().ws.skill_power_view[1].value,80,"E 镜像双方各翻倍1");
        turn(*m,1,1);
        CHECK_EQ(m->ctx().ws.skill_power_view[0].value,40,"E 先手窗先到期");
        CHECK_EQ(m->ctx().ws.skill_power_view[1].value,80,"E 后手窗仍有效");
        turn(*m,1,1);
        CHECK_EQ(m->ctx().ws.skill_power_view[1].value,40,"E 后手窗到期");
    }
    // F：乘“当前威力”而非定值/静态power/伤害AMP，包含先改值再倍乘。
    {
        auto b=solo(0);
        b->pet(0).skills[1].power=113;  // 场景面板fixture，不改库。
        own_turn(*b,0,0);
        check_attack(*b,0,1,226,true,"F 113乘二");
        auto add=solo(0);
        add->pet(0).skills[1]=Skills(12981);  // 179[20] 单效果载体。
        auto& skill=add->pet(0).skills[1];
        skill.must_hit=1;
        skill.critical_strike_rate=0;
        add->pet(0).elementalAttributes={skill.element[0],skill.element[1]};
        const int expect=(skill.power+20)*2;
        own_turn(*add,0,0);
        own_turn(*add,0,1);
        CHECK_EQ(add->ctx().ws.skill_power_view[0].value,expect,"F 179平直加值完成后倍乘");
        auto roll=solo(1,true,3);
        own_turn(*roll,1,0);
        for (int i=0;i<2;++i) {
            own_turn(*roll,1,3);  // 118[150,220] 先绝对改值。
            const int power=roll->ctx().ws.skill_power_view[1].value;
            CHECK(power>=300 && power<=440,"F 118改值后倍乘范围");
            CHECK_EQ(power%2,0,"F 118改值后倍乘应为偶数");
            CHECK(observed_rewritten[1],"F 变威力重算标记");
        }
        auto burn=solo(0);
        own_turn(*burn,0,0);
        REQUIRE(apply_anomaly_raw(&burn->ctx(),0,static_cast<int>(AbnormalStatusId::Burn),3,1)==ApplyAnomalyResult::SUCCESS);
        check_attack(*burn,0,1,40,true,"F 烧伤物化20再乘二");
    }
    // G：0威力仍是合法攻击改写；属性技不改写也不产生红伤。
    {
        auto b=solo(0,true,3);
        b->pet(0).skills[1].power=0;
        b->pet(0).skills[4].power=23;  // 属性技非零视图fixture，区分类型门与数值门。
        own_turn(*b,0,0);
        check_attack(*b,0,1,0,true,"G 零威力");
        const int hp=b->pet(1).hp;
        own_turn(*b,0,4);
        CHECK_EQ(b->ctx().ws.skill_power_view[0].value,23,"G 属性视图不乘二");
        CHECK(!observed_rewritten[0],"G 属性不触发变威力");
        CHECK_EQ(b->pet(1).hp,hp,"G 属性技无红伤");
    }
    // H：窗口不按次数冻结；NONE 消耗时间，被控未施放不会挂窗。
    {
        auto b=solo(0);
        own_turn(*b,0,0);
        turn(*b,-1,-1);
        check_attack(*b,0,1,40,false,"H NONE后已过窗");
        auto held=solo(1);
        REQUIRE(apply_anomaly_raw(&held->ctx(),1,static_cast<int>(AbnormalStatusId::Fear),1,0)==ApplyAnomalyResult::SUCCESS);
        own_turn(*held,1,0);
        CHECK(visible_windows(*held,1).empty(),"H 被控未施放不挂窗");
        check_attack(*held,1,1,40,false,"H 被控后的攻击无增益");
    }
    // I：主动换宠不继承；清回合原语终结窗口（长窗避免自然过期伪证）。
    for (int side : {0,1}) {
        auto b=solo(side,true,5);
        own_turn(*b,side,0);
        b->submit_operation(side,Action::CHOOSE_PET,1);
        b->submit_operation(1-side,Action::NONE,0);
        b->drive();
        CHECK_EQ(b->ctx().on_stage[side],1,"I 已换至替补");
        watch_power(*b);  // 观察器也是 ON_STAGE；新场上宠需重挂只读观察器。
        check_attack(*b,side,1,40,false,"I 替补不继承");
        CHECK(visible_windows(*b,side).empty(),"I 离场显示窗清除");
        auto cut=solo(side,true,5);
        own_turn(*cut,side,0);
        (void)core_api().break_round_effects(&cut->ctx(),side);
        watch_power(*cut);
        check_attack(*cut,side,1,40,false,"I 断回合不残留");
        CHECK(visible_windows(*cut,side).empty(),"I 断回合显示窗清除");
    }
    std::cout << "scenario_8760_k489 PASS (480 only; 1935 blocked separately)\n";
    return 0;
}
