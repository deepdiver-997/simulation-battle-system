// K494 / 8820：2024双方当前PP缺额×倍数，1564缺实例天赋输入仅核数据、不伪造门。
// 口径依据 〔源: docs_local/docs/05-任务清单/已收官/E组批量效果工单.md:99〕
// 数据来源 https://github.com/SeerAPI/seer-unity-assets （2026-11-23只读当前库重核）。
// 九载体全链装载；2024四载体完整链真实FSM；十槽独立缺额、双方/镜像/先后手、
// 常规PP在AFTER_ACTION扣除（不预扣本次；后手看见先手扣费），恢复/归零/新上限
// 现场读取，含第五技、空槽、无限PP、超上限、不计场下；0/3/10/20倍率。
// 〔源: include/entities/skills.h:286-288〕〔源: include/entities/elf-pet.h:198〕
// 〔源: src/fsm/battleFsm.cpp:2105-2109〕〔源: src/fsm/battleFsm.cpp:2247-2251〕
// 粉伤观测走TEAM watcher（显式valid_id）+真实HP差，锁FIXED/actor/时点/超杀实扣；
// 红伤另记，防止完整攻击链掩盖粉伤错误；固定抗性/护罩/护盾/免粉/臣服互斥辨别。
// PP历史与被测逻辑不共用求和助手，期望均为独立手算常量。无生产审计槽/驻留窗。
// 〔源: include/plugin/core_api.h:113-116〕
// 〔源: docs/07-工具与测试/效果显示gate规范.md:34-45〕
// 1564只核五条切片；不把1470满31常量先例冒充天赋读面/非满天赋负测。
// 〔源: include/entities/pet_factory.h:30-41〕
// 〔源: resources/moves_lib/j_1470_talent_power.cpp:3-5〕

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
constexpr int kMove = 28416;  // 属性载体2024[10]，无红伤，585升双防/919后手封属不改PP。
constexpr int kHit = 10004;
constexpr int kHp = 10000;
using Action = BattleFsm::ActionType;
using Five = std::array<int,5>;
struct DamageSeen {
    int fixed = 0;
    int normal = 0;
    int other = 0;
    int fixed_events = 0;
    int actor = -2;
    int state = -1;
};
std::array<DamageSeen,2> seen{};

int learner(int move) {
    if (move == 36668) return 4517;
    if (move == 36872) return 4557;
    if (move == 37109) return 4603;
    return 4545;
}
ElfPet carrier(int move, int speed, int multiplier = -1) {
    auto p = make_pet(learner(move),"K494",{move,kHit,10013,20001,10001},speed,300,kHp);
    if (multiplier >= 0) {
        const auto m = official_data::OfficialDataStore::instance().repository().load_monster(learner(move));
        REQUIRE(m.has_value());
        p.skills[0] = Skills(move,*m,{{move,2024,0,multiplier}});
    }
    for (auto& s:p.skills) {
        s.must_hit = true;
        s.accuracy = 100;
        s.critical_strike_rate = 0;
    }
    return p;
}
void watch(ScenarioBattle& b) {
    auto& c=b.ctx();
    c.event_center_.register_watcher(EventType::EVENT_TAKE_DAMAGE,0,c.roundCount,0,false,
        [](BattleContext*,const BattleEvent& e) {
            if (e.target<0 || e.target>1) return;
            auto& d=seen[e.target];
            if (e.damage_kind==static_cast<int>(DamageKind::FIXED)) {
                d.fixed+=e.amount;
                ++d.fixed_events;
                d.actor=e.actor;
                d.state=e.state;
            } else if (e.damage_kind==static_cast<int>(DamageKind::NORMAL)) {
                d.normal+=e.amount;
            } else {
                d.other+=e.amount;
            }
        },WatcherScope::TEAM,c.watcher_valid_id[0]);
}
std::unique_ptr<ScenarioBattle> solo(int side=0,bool first=true,int move=kMove,int multiplier=-1) {
    const auto p=carrier(move,first?300:100,multiplier);
    const auto q=carrier(kMove,first?100:300);
    auto b=std::make_unique<ScenarioBattle>(make_party(side==0?p:q),make_party(side==0?q:p));
    b->start();
    watch(*b);
    return b;
}
void losses(ElfPet& p,const Five& values) {
    for (int i=0;i<5;++i) {
        REQUIRE(values[i]>=0 && values[i]<=p.skills[i].maxPP);
        p.skills[i].pp=p.skills[i].maxPP-values[i];
    }
}
Five pp(const ElfPet& p) {
    Five out{};
    for (int i=0;i<5;++i) out[i]=p.skills[i].pp;
    return out;
}
void turn(ScenarioBattle& b,int slot0,int slot1,bool complete=true) {
    seen={};
    const int before=b.ctx().roundCount;
    b.submit_operation(0,slot0<0?Action::NONE:Action::SELECT_SKILL,std::max(0,slot0));
    b.submit_operation(1,slot1<0?Action::NONE:Action::SELECT_SKILL,std::max(0,slot1));
    b.drive();
    if (complete) CHECK_EQ(b.ctx().roundCount,before+1,"单回合有界驱动");
}
void own_turn(ScenarioBattle& b,int side,int ours=0,int theirs=-1,bool complete=true) {
    turn(b,side==0?ours:theirs,side==1?ours:theirs,complete);
}
void check_fixed(int side,int amount,const std::string& label) {
    const auto& d=seen[1-side];
    CHECK_EQ(d.fixed,amount,label+" FIXED实扣精确值");
    CHECK_EQ(d.fixed_events,amount>0?1:0,label+" 单笔或零事件");
    CHECK_EQ(d.other,0,label+" 未偷换百分比/真伤通道");
    CHECK_EQ(seen[side].fixed,0,label+" 不打自身");
    if (amount>0) {
        CHECK_EQ(d.actor,side,label+" actor=施放方");
        CHECK(d.state==static_cast<int>(State::BATTLE_FIRST_SKILL_EFFECT)
                  || d.state==static_cast<int>(State::BATTLE_SECOND_SKILL_EFFECT),
              label+" 在SKILL_EFFECT即时结算");
    }
}
void cast(ScenarioBattle& b,int side,int expected,const std::string& label,int theirs=-1) {
    const int before=b.pet(1-side).hp;
    own_turn(b,side,0,theirs);
    check_fixed(side,expected,label);
    CHECK_EQ(before-b.pet(1-side).hp,expected+seen[1-side].normal,label+" HP与事件互锚");
}
void no_resident(ScenarioBattle& b,const std::string& label) {
    const auto text=b.dump();
    for (int id:{1564,2024}) {
        std::size_t pos=0;
        int active=0;
        const auto needle="\"effectId\":"+std::to_string(id)+",";
        while ((pos=text.find(needle,pos))!=std::string::npos) {
            const auto end=text.find('}',pos);
            const auto entry=text.substr(pos,end-pos);
            pos=end+1;
            if (entry.find("\"isRoundEffect\":true")!=std::string::npos
                && entry.find("\"actionOneShot\":false")!=std::string::npos
                && entry.find("\"isExpired\":false")!=std::string::npos
                && entry.find("\"invalidated\":false")!=std::string::npos) ++active;
        }
        CHECK_EQ(active,0,label+" 无驻留显示态 "+std::to_string(id));
        const int base=1000000000+id*10000;
        bool occupied=false;
        for (const auto& item:b.ctx().plugin_storage)
            if (item.first>=base && item.first<base+10000) occupied=true;
        for (int side:{0,1}) for (const auto& p:b.ctx().seerRobot[side].elfPets) {
            for (const auto& item:p.on_stage_storage)
                if (item.first>=base && item.first<base+10000) occupied=true;
            for (const auto& item:p.soulmark_storage)
                if (item.first>=base && item.first<base+10000) occupied=true;
        }
        CHECK(!occupied,label+" 不占三类storage键段 "+std::to_string(id));
    }
}
void swap(ScenarioBattle& b,int side,int slot) {
    seen={};
    const int round=b.ctx().roundCount;
    b.submit_operation(side,Action::CHOOSE_PET,slot);
    b.submit_operation(1-side,Action::NONE,0);
    b.drive();
    CHECK_EQ(b.ctx().roundCount,round+1,"换宠单回合");
    CHECK_EQ(b.ctx().on_stage[side],slot,"换宠真实到位");
    CHECK_EQ(seen[0].fixed+seen[1].fixed,0,"换宠不重复附伤");
}
// 只作为时点夹具：选技时PP全满，ON_SKILL_HIT后经正式原语清对手第五槽。
EffectResult zero_before_effect(BattleContext* c,const EffectArgs& a) {
    REQUIRE(core_api().pp_zero_slot(c,1-a.int_args[0],4,a.int_args[0])==PpReduceResult::SUCCESS);
    return EffectResult::kOk;
}
}

int main() {
    if (!PetFactory::initialize_runtime_data()) return 1;
    sbs_srand(494);
    // A：2024四条与blocked1564五条全链双表切片；不以手工孤立函数替代装配。
    {
        const std::vector<std::pair<int,std::vector<std::pair<int,std::vector<int>>>>> want={
            {28416,{{585,{0,0,2,2,0,0}},{2024,{10}},{919,{1}}}},
            {36668,{{1767,{}},{2023,{1,3}},{2024,{10}},{1156,{}}}},
            {36872,{{1114,{2}},{2024,{20}},{2033,{2}},{2027,{3}}}},
            {37109,{{1877,{300,300}},{1857,{50,2}},{2024,{20}}}},
            {28664,{{48,{4}},{559,{4}},{1564,{500,100}}}},
            {34093,{{759,{2,5,-2}},{1564,{200,50}}}},
            {34141,{{1190,{0,0,0,0,-1,-2}},{1564,{200,50}}}},
            {34600,{{1034,{2}},{1564,{300,50}}}},
            {34869,{{1261,{300}},{1564,{300,100}}}}
        };
        auto& repo=official_data::OfficialDataStore::instance().repository();
        for (const auto& [move,chain]:want) {
            const auto got=repo.load_skill_effects(move);
            CHECK_EQ(got.size(),chain.size(),"A 载体链长");
            for (std::size_t i=0;i<chain.size();++i) {
                CHECK_EQ(got[i].effect_id,chain[i].first,"A 效果顺序");
                CHECK(got[i].args==chain[i].second,"A 双表全链参数切片");
            }
        }
    }
    // B：四个真实载体完整链。15+10=25，不同倍率10/20；攻击红伤单独记。
    for (int move:{kMove,36668,36872,37109}) {
        auto b=solo(0,true,move);
        losses(b->pet(0),{1,2,3,4,5});
        losses(b->pet(1),{0,1,2,3,4});
        cast(*b,0,move==kMove || move==36668?250:500,"B 载体"+std::to_string(move));
        CHECK_EQ(seen[1].normal>0,move!=kMove,"B 物/特有红伤，属性无红伤");
        no_resident(*b,"B");
    }
    // C：双方×先后手。后手多看见对手刚使用拍打的1点，自己当次费用尚未扣。
    for (int side:{0,1}) for (bool first:{true,false}) {
        auto b=solo(side,first);
        losses(b->pet(side),{1,2,3,4,5});
        losses(b->pet(1-side),{0,1,2,3,4});
        auto ours=pp(b->pet(side));
        auto theirs=pp(b->pet(1-side));
        cast(*b,side,first?250:260,"C 镜像时序",1);
        --ours[0]; --theirs[1];
        CHECK(pp(b->pet(side))==ours,"C 施放方只有正常1点扣费");
        CHECK(pp(b->pet(1-side))==theirs,"C 对手只有正常1点扣费");
        no_resident(*b,"C");
    }
    // D：满PP当次零伤，下一次才计先前的1点；NONE/其他技不继承附伤。
    {
        auto b=solo();
        cast(*b,0,0,"D 满PP");
        cast(*b,0,10,"D 上次已扣1点");
        own_turn(*b,0,-1);
        check_fixed(0,0,"D NONE不重复");
        own_turn(*b,0,1);
        check_fixed(0,0,"D 其他技能无残留");
    }
    // E：每侧每槽独立贡献；第五技不能丢，所选槽不能替代全栏。
    for (int side:{0,1}) for (int slot=0;slot<5;++slot) {
        auto b=solo();
        Five lost{}; lost[slot]=3;
        losses(b->pet(side),lost);
        cast(*b,0,30,"E 独立缺额side"+std::to_string(side)+"slot"+std::to_string(slot));
    }
    for (int mult:{0,3,20}) {
        auto b=solo(0,true,kMove,mult);
        losses(b->pet(0),{1,2,3,4,5});
        losses(b->pet(1),{0,1,2,3,4});
        cast(*b,0,25*mult,"E 覆盖倍率"+std::to_string(mult));
    }
    // F：恢复降低缺额（不是历史累加），正式清PP重新增大缺额。
    {
        auto b=solo();
        losses(b->pet(0),{1,2,3,4,5});
        losses(b->pet(1),{0,1,2,3,4});
        cast(*b,0,250,"F 初始缺额25");
        REQUIRE(core_api().pp_restore(&b->ctx(),0,100,false)==5);
        REQUIRE(core_api().pp_restore(&b->ctx(),1,100,false)==4);
        cast(*b,0,0,"F 回满不留历史");
        REQUIRE(core_api().pp_zero_slot(&b->ctx(),1,4,0)==PpReduceResult::SUCCESS);
        cast(*b,0,360,"F 自身上次1+对手第五槽35");
    }
    // G：无限/空槽/无上限不计，超上限不能负抵别槽；上限读当前对象而非静态DB。
    {
        auto b=solo();
        losses(b->pet(0),{1,0,0,0,0});
        b->pet(1).skills[1].pp=-1;
        b->pet(1).skills[2].maxPP=-1;
        b->pet(1).skills[3].maxPP=0;
        b->pet(1).skills[3].pp=0;
        b->pet(1).skills[4].pp=b->pet(1).skills[4].maxPP+7;
        cast(*b,0,10,"G 无限/非正上限/超上限");
        auto empty=solo();
        empty->pet(1).skills[4].id=-1;
        empty->pet(1).skills[4].pp=0;
        cast(*empty,0,0,"G 空槽不计");
        auto cap=solo();
        cap->pet(1).skills[2].maxPP=10;
        cap->pet(1).skills[2].pp=4;
        cast(*cap,0,60,"G 当前上限10减当前4");
    }
    // H：粉伤正式管线。缺额20→200；抗性35%→130，罩150→50，盾不挡粉。
    for (int mode=0;mode<5;++mode) {
        auto b=solo();
        losses(b->pet(0),{0,2,3,4,1});
        losses(b->pet(1),{0,1,2,3,4});
        if (mode==0) {
            b->pet(1).damage_resist.fixed_pct=35;
            b->pet(1).damage_resist.percent_pct=15;
        }
        if (mode==1) b->pet(1).hood_bank_.add(0,150,2024,0);
        if (mode==2) b->pet(1).shield_bank_.add(0,150,2024,0);
        if (mode==3) b->ctx().grant_immunity(1,ImmunityType::PINK_DAMAGE,coverage_all(),0,
                                             2,2024,false,EffectScope::ON_STAGE,0,2024);
        if (mode==4) REQUIRE(apply_anomaly_raw(&b->ctx(),0,31,2,1)==ApplyAnomalyResult::SUCCESS);
        const int expected=mode==0?130:mode==1?50:mode==2?200:0;
        cast(*b,0,expected,"H 管线档"+std::to_string(mode));
        if (mode==1) CHECK_EQ(b->pet(1).hood_bank_.total(),0,"H 护罩被消费");
        if (mode==2) CHECK_EQ(b->pet(1).shield_bank_.total(),150,"H 红盾未消费");
    }
    // I：真实MISS/被控/封属性门关闭，不能靠“没红伤”假证粉伤没发。
    for (int mode=0;mode<3;++mode) {
        auto b=solo();
        losses(b->pet(1),{0,1,2,3,4});
        if (mode==0) { b->pet(0).skills[0].must_hit=false; b->pet(0).skills[0].accuracy=0; }
        if (mode==1) REQUIRE(apply_anomaly_raw(&b->ctx(),0,6,1,1)==ApplyAnomalyResult::SUCCESS);
        if (mode==2) REQUIRE(core_api().seal_skill(&b->ctx(),1,0,2024,true,false,1,0,false,
                                                  -1,EffectScope::ON_STAGE,false,100,false)>0);
        cast(*b,0,0,"I 命中门档"+std::to_string(mode));
        no_resident(*b,"I");
    }
    // J：同回合双方施放；后手只多1点，不能串目标或缓存第一次总和。
    for (int first:{0,1}) {
        auto b=solo(first,true);
        losses(b->pet(0),{1,2,3,4,5});
        losses(b->pet(1),{0,1,2,3,4});
        turn(*b,0,0);
        CHECK_EQ(seen[1-first].fixed,250,"J 先手缺额25");
        CHECK_EQ(seen[first].fixed,260,"J 后手缺额26");
        CHECK_EQ(seen[1-first].actor,first,"J 先手归属");
        CHECK_EQ(seen[first].actor,1-first,"J 后手归属");
        CHECK_EQ(seen[0].fixed_events+seen[1].fixed_events,2,"J 恰两笔");
        no_resident(*b,"J");
    }
    // K：换宠只读双方当前在场PP；场下耗尽不计，换回仍读该宠真实缺额不清PP本体。
    for (int side:{0,1}) {
        auto b=solo();
        for (int team:{0,1}) for (int slot=2;slot<6;++slot)
            for (auto& s:b->ctx().seerRobot[team].elfPets[slot].skills) s.pp=0;
        cast(*b,0,0,"K 场下PP不计");
        REQUIRE(core_api().pp_restore(&b->ctx(),0,100,false)==1);
        losses(b->pet(side),{0,1,2,3,4});
        swap(*b,side,1);
        cast(*b,0,0,"K 新宠缺额0");
        REQUIRE(core_api().pp_restore(&b->ctx(),0,100,false)==1);
        swap(*b,side,0);
        cast(*b,0,100,"K 回场真实缺额10");
        no_resident(*b,"K");
    }
    // L：选技之后/效果之前经原语清PP，本次就读到35，不是选技快照或上回合值。
    {
        auto b=solo();
        const auto st=State::BATTLE_FIRST_ON_SKILL_HIT;
        Effect e(2024,0,0,-1,EffectArgs(std::vector<int>{0}),&zero_before_effect);
        auto ce=std::make_unique<ContinuousEffect>(e,st,0,1,b->ctx().roundCount);
        ce->source_id_=2024;
        b->ctx().passive_effects.register_effect(st,0,std::move(ce),b->ctx().round_effect_valid_id[0]);
        cast(*b,0,350,"L 现场读取刚归零第五槽");
    }
    // M：超杀只上报17实扣；粉伤击杀不伪造击败归因，死亡换宠无延迟债。
    {
        auto b=solo();
        losses(b->pet(1),{0,1,1,0,0});
        b->pet(1).hp=17;
        const int slot=b->ctx().on_stage[1];
        own_turn(*b,0,0,-1,false);
        check_fixed(0,17,"M 超杀实扣");
        CHECK_EQ(b->ctx().seerRobot[1].elfPets[slot].hp,0,"M 原宠倒地");
        CHECK_EQ(b->ctx().last_damage_actor[1][slot],-1,"M 粉伤无击败归因");
    }
    std::cout << "scenario_8820_k494 PASS (2024 only; 1564 blocked by instance talent input)\n";
    return 0;
}
