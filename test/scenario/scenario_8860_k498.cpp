// K498 / 8860：518伤害门自身能力变化；2084只核载体，不伪造定向PP吸取。
// 口径依据 〔源: docs_local/docs/05-任务清单/已收官/E组批量效果工单.md:103〕
// 数据来源 https://github.com/SeerAPI/seer-unity-assets （2026-11-23当前库只读重切）。
// 16543“自身特攻+1” https://wiki.biligame.com/seer/技能:16543 。
// 九载体全链切片；五条518完整链真实FSM；250/300严格边界以超杀钳制实扣构造，
// 不用抽样浮动边界。独立事件带与真实等级/HP互锚，六槽官方序、delta透传、双方先后手。
// 〔源: resources/moves_lib/k475_anomaly_cond_stat_change.cpp:173-175〕
// 〔源: resources/moves_lib/k470_attack_end_heal_and_hp_drain.cpp:327-349〕
// 夹具只经正式伤害原语在真实FSM桶发粉/真/属性直伤与反向/错时点红伤；不调用被测函数、
// 不手工emit伪造TAKE_DAMAGE。once必须过滤后消费；全盾/全挡桥清理在同回合单步观察，
// 防止回合末cleanup遮蔽泄漏。命中失效/真miss/不选技、重复施放、镜像、换宠回场均验。
// 〔源: include/effects/event_center.h:498-513〕
// 禁提升、到+6/越界拒写均走原语，不假设钳到上限；在途桥不冒充驻留buff显示。
// 〔源: src/primitives/battle_primitives.cpp:1462-1485〕
// 〔源: docs/07-工具与测试/效果显示gate规范.md:34-45〕
// 2084缺pp_reduce_slot：不以全槽减后补回/全清后补回替代；两者事件面不等价。
// 〔源: include/plugin/core_api.h:122-140〕

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
constexpr int kMove = 14732;
constexpr int kHp = 100000;
using Levels = std::array<int,6>;

int learner(int move) {
    switch (move) {
        case 15543: return 2273;
        case 16476: return 2556;
        case 16543: return 2568;
        case 31076: return 3429;
        default: return 1958;
    }
}
ElfPet carrier(int move, int speed, int atk, const std::vector<int>& params = {}) {
    auto p = make_pet(learner(move),"K498",{move,10004,20001,10013,10001},speed,atk,kHp);
    if (!params.empty()) {
        const auto m = official_data::OfficialDataStore::instance().repository().load_monster(learner(move));
        REQUIRE(m.has_value());
        p.skills[0] = Skills(move,*m,{{move,518,0,params[0]},
                                    {move,518,1,params[1]},
                                    {move,518,2,params[2]}});
    }
    for (auto& s : p.skills) {
        s.must_hit = true;
        s.accuracy = 100;
        s.critical_strike_rate = 0;
    }
    return p;
}
struct Fight {
    std::vector<BattleEvent> events; // 比battle先构造、后析构，sink指针始终有效。
    std::unique_ptr<ScenarioBattle> battle;
    Fight(int side=0, bool first=true, int move=kMove, int atk=1000,
          const std::vector<int>& params={}, bool mirror=false) {
        const auto p = carrier(move,first?300:100,atk,params);
        const auto q = carrier(kMove,first?100:300,mirror?atk:1);
        battle = std::make_unique<ScenarioBattle>(make_party(side==0?p:q),make_party(side==0?q:p));
        battle->start();
        ctx().event_center_.set_delivery_sink(&events);
    }
    BattleContext& ctx() { return battle->ctx(); }
    ElfPet& pet(int side) { return battle->pet(side); }
};
Levels levels(Fight& f,int side) {
    Levels out{};
    for (int i=0;i<6;++i) out[i]=f.ctx().ability_levels[side][i];
    return out;
}
void expect_levels(Fight& f,int side,const Levels& want,const std::string& label) {
    for (int i=0;i<6;++i) CHECK_EQ(f.ctx().ability_levels[side][i],want[i],label+" 槽"+std::to_string(i));
}
void turn(Fight& f,int slot0,int slot1,bool complete=true) {
    f.events.clear();
    const int round=f.ctx().roundCount;
    f.battle->submit_operation(0,slot0<0?Action::NONE:Action::SELECT_SKILL,std::max(0,slot0));
    f.battle->submit_operation(1,slot1<0?Action::NONE:Action::SELECT_SKILL,std::max(0,slot1));
    f.battle->drive();
    if (complete) CHECK_EQ(f.ctx().roundCount,round+1,"有界单回合驱动");
}
void cast(Fight& f,int side,int theirs=-1,bool complete=true) {
    turn(f,side==0?0:theirs,side==1?0:theirs,complete);
}
int damage(Fight& f,int actor,int target,DamageKind kind,bool attack_only=true) {
    int sum=0;
    for (const auto& e:f.events) {
        if (e.type!=EventType::EVENT_TAKE_DAMAGE || e.actor!=actor || e.target!=target
            || e.damage_kind!=static_cast<int>(kind)) continue;
        if (attack_only && e.state!=static_cast<int>(State::BATTLE_FIRST_ATTACK_DAMAGE)
            && e.state!=static_cast<int>(State::BATTLE_SECOND_ATTACK_DAMAGE)) continue;
        sum+=e.amount;
    }
    return sum;
}
int stat_events(Fight& f,int side,int slot,int delta) {
    int count=0;
    for (const auto& e:f.events) if (e.type==EventType::EVENT_STAT_CHANGED && e.target==side
        && e.stat_index==slot && e.level_delta==delta) ++count;
    return count;
}
void no_state(Fight& f,const std::string& label,bool pure=true) {
    if (pure) CHECK_EQ(f.ctx().event_center_.watch_count(EventType::EVENT_TAKE_DAMAGE),0,
                       label+" 无未消费伤害监听");
    const auto text=f.battle->dump();
    const std::string needle="\"effectId\":518,";
    std::size_t pos=0;
    int residents=0;
    while ((pos=text.find(needle,pos))!=std::string::npos) {
        const auto end=text.find('}',pos);
        const auto entry=text.substr(pos,end-pos);
        pos=end+1;
        if (entry.find("\"isRoundEffect\":true")!=std::string::npos
            && entry.find("\"actionOneShot\":false")!=std::string::npos
            && entry.find("\"isExpired\":false")!=std::string::npos
            && entry.find("\"invalidated\":false")!=std::string::npos) ++residents;
    }
    CHECK_EQ(residents,0,label+" 不伪造518驻留buff");
    for (int id:{518,2084}) {
        const int base=1000000000+id*10000;
        bool occupied=false;
        for (const auto& item:f.ctx().plugin_storage)
            if (item.first>=base && item.first<base+10000) occupied=true;
        for (int side:{0,1}) for (const auto& p:f.ctx().seerRobot[side].elfPets) {
            for (const auto& item:p.on_stage_storage)
                if (item.first>=base && item.first<base+10000) occupied=true;
            for (const auto& item:p.soulmark_storage)
                if (item.first>=base && item.first<base+10000) occupied=true;
        }
        CHECK(!occupied,label+" 无审计键或私有行为键 "+std::to_string(id));
    }
}
// 夹具：正式原语生成异类事件，处于真实FSM时点，不直接调用518或伪造事件。
// mode 0/1/2=粉/真/属性直伤，3=对方打自己，4=自己打自己，5=错时点红伤。
EffectResult extra_damage(BattleContext* c,const EffectArgs& a) {
    const int side=a.int_args[0], mode=a.int_args[1], amount=a.int_args[2];
    if (mode==0) (void)core_api().deal_pink_damage(c,1-side,amount,DamageKind::FIXED,side);
    else if (mode==1) (void)core_api().deal_true_damage(c,1-side,amount,side);
    else if (mode==2) deal_damage(c,1-side,amount,DamageKind::ATTRIBUTE,side);
    else if (mode==3) deal_damage(c,side,amount,DamageKind::NORMAL,1-side);
    else if (mode==4) deal_damage(c,side,amount,DamageKind::NORMAL,side);
    else deal_damage(c,1-side,amount,DamageKind::NORMAL,side);
    return EffectResult::kOk;
}
void inject(Fight& f,int side,State st,int mode,int amount) {
    Effect e(518,0,side,0,EffectArgs(std::vector<int>{side,mode,amount}),&extra_damage);
    auto ce=std::make_unique<ContinuousEffect>(e,st,side,1,f.ctx().roundCount);
    ce->source_id_=518;
    f.ctx().passive_effects.register_effect(st,side,std::move(ce),f.ctx().round_effect_valid_id[side]);
}
// 有界单步停在目标时点执行前，绝不用while(!FINISHED)。
void reach(Fight& f,State st) {
    for (int i=0;i<100 && f.battle->state()!=st && f.battle->state()!=State::FINISHED;++i)
        f.battle->step();
    CHECK(f.battle->state()==st,"有界单步到指定时点");
}
void choose_stepped(Fight& f,int side) {
    f.events.clear();
    f.ctx().debug_step_mode=true;
    f.battle->submit_operation(0,side==0?Action::SELECT_SKILL:Action::NONE,0);
    f.battle->submit_operation(1,side==1?Action::SELECT_SKILL:Action::NONE,0);
    f.ctx().debug_step_mode=false;
}
void swap(Fight& f,int side,int slot) {
    f.events.clear();
    const int r=f.ctx().roundCount;
    f.battle->submit_operation(side,Action::CHOOSE_PET,slot);
    f.battle->submit_operation(1-side,Action::NONE,0);
    f.battle->drive();
    CHECK_EQ(f.ctx().roundCount,r+1,"真实换宠有界单回合");
    CHECK_EQ(f.ctx().on_stage[side],slot,"真实换宠到位");
}
}

int main() {
    if (!PetFactory::initialize_runtime_data()) return 1;
    sbs_srand(498);
    // A：九条全链按双表切片，2084不做成功行为断言，不冒充已实现。
    {
        const std::vector<std::pair<int,std::vector<std::pair<int,std::vector<int>>>>> want={
            {14732,{{518,{300,0,1}}}}, {15543,{{518,{250,0,1}}}},
            {16476,{{665,{250,3,100}},{518,{250,0,1}}}},
            {16543,{{639,{0,300,4,50}},{518,{300,2,1}},{644,{4}}}},
            {31076,{{518,{300,0,2}}}},
            {28474,{{2083,{}},{2084,{5}},{1343,{1}},{951,{1}}}},
            {29053,{{191,{4}},{1118,{3,100,7,1}},{2084,{5}}}},
            {37678,{{1127,{1}},{2084,{5}},{1158,{1,50}},{523,{1,1,1,1,1,1}}}},
            {38091,{{948,{15}},{2084,{2}},{1125,{2,2}}}}
        };
        auto& repo=official_data::OfficialDataStore::instance().repository();
        for (const auto& [move,chain]:want) {
            const auto got=repo.load_skill_effects(move);
            CHECK_EQ(got.size(),chain.size(),"A 全链长度 "+std::to_string(move));
            for (std::size_t i=0;i<chain.size();++i) {
                CHECK_EQ(got[i].effect_id,chain[i].first,"A 效果顺序");
                CHECK(got[i].args==chain[i].second,"A 双表切片");
            }
        }
    }
    // B：五个真实载体完整链，双方镜像；16543官槽2是特攻，不是防御。
    for (int move:{14732,15543,16476,16543,31076}) for (int side:{0,1}) {
        Fight f(side,true,move);
        cast(f,side);
        CHECK(damage(f,side,1-side,DamageKind::NORMAL)>300,"B 真实载体红伤过门");
        Levels want{}; want[move==16543?1:0]=move==31076?2:1;
        expect_levels(f,side,want,"B 自身真实增益 "+std::to_string(move));
        expect_levels(f,1-side,{},"B 不强化对手");
        CHECK_EQ(stat_events(f,side,move==16543?1:0,move==31076?2:1),1,"B 增益只发一次变化事件");
        no_state(f,"B",move!=16476 && move!=16543);
    }
    // C：门-1/门/门+1，250/300真实载体，双方×先后手；raw远高于门，实扣以HP钳制。
    for (int side:{0,1}) for (bool first:{true,false}) for (int move:{14732,15543})
        for (int offset:{-1,0,1}) {
            Fight f(side,first,move);
            const int threshold=move==15543?250:300;
            const int hp=threshold+offset;
            f.pet(1-side).hp=hp;
            const int target_slot=f.ctx().on_stage[1-side];
            cast(f,side,first?-1:1,false);
            CHECK_EQ(damage(f,side,1-side,DamageKind::NORMAL),hp,"C 超杀实扣精确边界");
            CHECK_EQ(f.ctx().seerRobot[1-side].elfPets[target_slot].hp,0,"C 目标实际归零");
            Levels want{}; want[0]=offset>0?1:0;
            expect_levels(f,side,want,"C 严格大于阈值，不读raw/final");
            CHECK_EQ(stat_events(f,side,0,1),offset>0?1:0,"C 门档变化事件");
        }
    // D：低伤不强化；NONE、其他技能不继承；本次强化不反灌到本次伤害。
    {
        Fight low(0,true,kMove,1);
        cast(low,0);
        CHECK(damage(low,0,1,DamageKind::NORMAL)>0 && damage(low,0,1,DamageKind::NORMAL)<250,
              "D 正伤害但门闭");
        expect_levels(low,0,{},"D 低伤门闭");
        Fight f;
        turn(f,-1,-1);
        expect_levels(f,0,{},"D NONE无强化");
        turn(f,1,-1);
        expect_levels(f,0,{},"D 非载体无强化");
        cast(f,0);
        const int d1=damage(f,0,1,DamageKind::NORMAL);
        expect_levels(f,0,{1,0,0,0,0,0},"D 第一次强化");
        cast(f,0);
        const int d2=damage(f,0,1,DamageKind::NORMAL);
        CHECK(d2*10>d1*12 && d2*10<d1*18,"D 次击才吃前击攻+1，浮动比值带");
        expect_levels(f,0,{2,0,0,0,0,0},"D 每次重新武装一次");
        no_state(f,"D");
    }
    // E：六槽与变化量独立变档（0/2/3/-1），期望按官方槽标签显式枚举。
    const std::array<int,6> expected_engine={0,2,1,3,4,5};
    for (int slot=0;slot<6;++slot) for (int delta:{0,2,3,-1}) {
        Fight f(0,true,kMove,1000,{250,slot,delta});
        cast(f,0);
        Levels want{}; want[expected_engine[slot]]=delta;
        expect_levels(f,0,want,"E 六槽delta透传");
        CHECK_EQ(stat_events(f,0,expected_engine[slot],delta),delta==0?0:1,"E 单槽变化事件");
        no_state(f,"E");
    }
    // F：禁提升与免弱是两条通道；+5/+6再+2是拒写，不钳+6；+4再+2可到上限。
    for (int initial:{4,5,6}) {
        Fight f(0,true,31076);
        REQUIRE(stat_change(&f.ctx(),0,0,initial)==StatChangeResult::SUCCESS);
        cast(f,0);
        CHECK_EQ(f.ctx().ability_levels[0][0],initial==4?6:initial,"F 上限拒写非钳写");
        CHECK_EQ(stat_events(f,0,0,2),initial==4?1:0,"F 拒写不发等级事件");
    }
    for (auto immunity:{ImmunityType::STAT_BOOST,ImmunityType::STAT_DROP}) {
        Fight f;
        f.ctx().grant_immunity(0,immunity,coverage_all(),0,3,518,false,EffectScope::ON_STAGE,0,518);
        cast(f,0);
        const bool blocked=immunity==ImmunityType::STAT_BOOST;
        expect_levels(f,0,{blocked?0:1,0,0,0,0,0},"F 禁提升/免弱甄别");
        CHECK_EQ(stat_events(f,0,0,1),blocked?0:1,"F 正式stat_change事件通道");
    }
    // G：攻击时点高额粉/真/属性直伤不能替代低额红伤；固定伤害也不能吃掉监听。
    for (int mode:{0,1,2}) for (bool high:{false,true}) {
        Fight f(0,true,kMove,high?1000:1);
        inject(f,0,State::BATTLE_FIRST_ATTACK_DAMAGE,mode,600);
        cast(f,0);
        Levels want{};want[0]=high?1:0;
        expect_levels(f,0,want,"G 异类伤害不触发也不耗once");
        CHECK_EQ(stat_events(f,0,0,1),high?1:0,"G 恰一次合格攻击反馈");
        no_state(f,"G");
    }
    // H：反向/自伤先到不消费；错时点的高额NORMAL不是本次攻击伤害。
    for (int mode:{3,4}) {
        Fight f;
        inject(f,0,State::BATTLE_FIRST_ATTACK_DAMAGE,mode,10);
        cast(f,0);
        expect_levels(f,0,{1,0,0,0,0,0},"H actor/target双过滤");
        CHECK_EQ(stat_events(f,0,0,1),1,"H 只响应自己的攻击");
    }
    {
        Fight f(0,true,kMove,1);
        inject(f,0,State::BATTLE_FIRST_SKILL_EFFECT,5,600);
        cast(f,0);
        CHECK_EQ(damage(f,0,1,DamageKind::NORMAL,false)-damage(f,0,1,DamageKind::NORMAL),600,
                 "H 正式原语错时点红伤确实到达");
        expect_levels(f,0,{},"H 非攻击时点伤害不喂门");
    }
    // I：真miss、命中效果失效白板、伤害免疫、全盾四种负测；清理不影响下一次施放。
    for (int mode=0;mode<4;++mode) {
        Fight f;
        if (mode==0) {f.pet(0).skills[0].must_hit=false;f.pet(0).skills[0].accuracy=0;}
        if (mode==1) core_api().hit_effect_invalid(&f.ctx(),1,HitInvalidMode::kFullNull,1,false,518);
        if (mode==2) f.ctx().grant_immunity(1,ImmunityType::DAMAGE,coverage_all(),0,0,518,
                                          false,EffectScope::ON_STAGE,1,518);
        if (mode==3) f.pet(1).shield_bank_.add(0,999999,518,0);
        cast(f,0);
        CHECK_EQ(damage(f,0,1,DamageKind::NORMAL),0,"I 被挡/未命中无实扣红伤");
        expect_levels(f,0,{},"I 无伤害不强化");
        no_state(f,"I");
        f.pet(0).skills[0].must_hit=true;
        f.pet(1).shield_bank_=ShieldBank{};
        cast(f,0);
        expect_levels(f,0,{1,0,0,0,0,0},"I 下次重武装不多发");
    }
    // J：同回合桥清理 + 显示义务，避免只在round末检查让cleanup自动掩盖泄漏。
    for (bool shield:{false,true}) for (int side:{0,1}) {
        Fight f(side,true);
        if (shield) f.pet(1-side).shield_bank_.add(0,999999,518,0);
        choose_stepped(f,side);
        reach(f,State::BATTLE_FIRST_ATTACK_DAMAGE);
        CHECK_EQ(f.ctx().event_center_.watch_count(EventType::EVENT_TAKE_DAMAGE),1,"J 在途监听已武装");
        const auto full=f.battle->dump();
        CHECK(full.find("\"effectId\":518,")!=std::string::npos,"J fullstate有518在途桥身份");
        CHECK(full.find("\"actionOneShot\":true")!=std::string::npos,"J 桥是一次性节点");
        const auto snap=f.ctx().getStateJson();
        CHECK(snap.find("\"watchers\":[{")!=std::string::npos,"J 快照显示在途watcher");
        f.battle->step(); // ATTACK_DAMAGE -> AFTER_ACTION；全盾尚留监听，正常红伤已自拆。
        CHECK_EQ(f.ctx().event_center_.watch_count(EventType::EVENT_TAKE_DAMAGE),shield?1:0,
                 "J 匹配事件才自拆");
        f.battle->step(); // AFTER_ACTION桥，不借round末cleanup。
        CHECK_EQ(f.ctx().event_center_.watch_count(EventType::EVENT_TAKE_DAMAGE),0,
                 "J 同行动结束清理全挡监听");
        CHECK_EQ(f.ctx().roundCount,0,"J 清理发生在首回合结束之前");
        f.battle->drive();
        no_state(f,"J");
    }
    // K：镜像同轮两侧同技能，各自只升一次；先后手都能消费自己的在途监听。
    for (int first:{0,1}) {
        Fight f(0,first==0,kMove,1000,{},true);
        turn(f,0,0);
        expect_levels(f,0,{1,0,0,0,0,0},"K 双边镜像side0");
        expect_levels(f,1,{1,0,0,0,0,0},"K 双边镜像side1");
        CHECK_EQ(stat_events(f,0,0,1),1,"K side0仅一次事件");
        CHECK_EQ(stat_events(f,1,0,1),1,"K side1仅一次事件");
        no_state(f,"K");
    }
    // L：真实主动换宠与回场清等级，不把旧监听/增益带给新宠。
    for (int side:{0,1}) {
        Fight f(side);
        cast(f,side);
        expect_levels(f,side,{1,0,0,0,0,0},"L 离场前强化");
        swap(f,side,1);
        expect_levels(f,side,{},"L 新宠不继承强化");
        no_state(f,"L 出场");
        swap(f,side,0);
        expect_levels(f,side,{},"L 回场原宠强化已清");
        turn(f,side==0?1:-1,side==1?1:-1);
        expect_levels(f,side,{},"L 回场非载体不会补发");
        cast(f,side);
        expect_levels(f,side,{1,0,0,0,0,0},"L 回场重新施放一次");
        no_state(f,"L 回场");
    }
    std::cout << "scenario_8860_k498 PASS (518 delivered; 2084 blocked: targeted PP reduction API)\n";
    return 0;
}
