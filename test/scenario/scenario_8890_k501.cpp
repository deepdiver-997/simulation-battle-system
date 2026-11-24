// K501 / 8890：1555 无提升全槽PP减；真实FSM，不直接调用被测函数。
// 口径依据 〔源: docs_local/docs/05-任务清单/已收官/E组批量效果工单.md:106〕
// 数据 https://github.com/SeerAPI/seer-unity-assets ，四载体完整链/当前双表实切。
// 1555 = SKILL_EFFECT 命中趟（HIT 分支；miss/被盔走 SKILL_INVALID 结构性不触发）：
// 提升门 = 对手 ability_levels 六槽存储面任一正槽（K490 2171 直系；视图手术不擦
// 存储的差分 = D 腿）；落点 = pp_reduce 正式原语（全槽减、钳 0、-1 无限槽跳过、
// 逐实变槽 EVENT_PP_REDUCED）。使用自身 PP 扣费在 AFTER_ACTION 直写不发事件
// （battleFsm.cpp:2105），与本效果的被削事件面互不混淆。
// 〔源: resources/moves_lib/k501_pp_gate_and_chance_double.cpp:32-50〕
// 〔源: include/plugin/core_api.h:122-130〕〔源: src/entities/skills.cpp:1240〕
// 观测为 PP 本体/事件/审计槽，确定性效果无随机档；白盒审计键
// 1015550000（10^9+1555×10^4+owner×16+slot）与插件头注手工同步。
// 覆盖四载体、两侧先后手、门正负、存储面/视图差分、逐槽门、钳制/无限槽、
// 0/3 行参差分、miss/被控/被盔、属性载体、同链激活、镜像、换宠离场、无驻留窗。

#include <any>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <entities/pet_factory.h>
#include <plugin/core_api.h>
#include "scenario_harness.h"

using namespace scenario;
namespace {
using Action = BattleFsm::ActionType;
constexpr int kCarrierPure1 = 34121;  // 圣浴之耀 [1555][1]，cat2 特攻140（learner 4032）
constexpr int kCarrierPure2 = 34691;  // 虚弱魔咒 [1555][1]，cat2 特攻140（learner 4146）
constexpr int kCarrierAttr = 27880;   // 柱神祈愿 [48,1555,780][4,1,3,100,1]，cat4（learner 4336）
constexpr int kCarrierChain = 34056;  // 娇影笑春风 [913,514,1555][100,25,1]，cat2（hide learner 4018）
constexpr int kMove = kCarrierPure1;
constexpr int kHit1 = 10004;  // 拍打 cat1 40 maxPP35
constexpr int kHit2 = 10013;  // 水枪 cat2 40 maxPP25
constexpr int kHp = 50000;
constexpr int k1555Base = 1015550000;

int learner(int move) {
    switch (move) {
        case kCarrierPure2: return 4146;
        case kCarrierAttr: return 4336;
        case kCarrierChain: return 4018;
        default: return 4032;
    }
}
int stor(BattleContext& c, int key) {
    const auto it = c.plugin_storage.find(key);
    return it == c.plugin_storage.end() ? 0 : std::any_cast<int>(it->second);
}
int slot(BattleContext& c, int side, int n) { return stor(c, k1555Base + side * 16 + n); }
int total_pp(const ElfPet& pet) {
    int sum = 0;
    for (const auto& s : pet.skills) if (s.id > 0 && s.maxPP > 0 && s.pp >= 0) sum += s.pp;
    return sum;
}
// 铺能力提升（raw + view 同写 = write_ability_level 语义，8780 同款）。
void put_boost(BattleContext& c, int side, int slot_idx, int level) {
    c.ability_levels[side][slot_idx] = level;
    c.ws.view_levels[side][slot_idx] = level;
}
// 桶内视图手术（M 判别面：视图手术不擦存储）。回合起点 sync_workspace_from_on_stage
// 会把 pre-round 视图摆动重基（battleFsm.cpp:559），故必须挂在 SKILL_EFFECT 桶内、
// 先于载体链注册执行（K490 T8792 同款判别面，1874 视图翻正同位）。
EffectResult flip_view(BattleContext* c, const EffectArgs& a) {
    if (c && a.int_args && a.int_count >= 1) {
        c->ws.view_levels[a.int_args[0]][a.int_args[1]] = a.int_args[2];
    }
    return EffectResult::kOk;
}
ElfPet carrier(int move, int speed, int atk, const std::vector<int>& params = {},
               bool hit_forced = true) {
    auto p = make_pet(learner(move), "K501-1555", {move, kHit1, kHit2, kHit1, kHit2},
                      speed, atk, kHp);
    if (!params.empty()) {
        REQUIRE(params.size() == 1);
        const auto m = official_data::OfficialDataStore::instance().repository()
                           .load_monster(learner(move));
        REQUIRE(m.has_value());
        std::vector<EffectParamOverride> overrides;
        overrides.push_back({move, 1555, 0, params[0]});
        p.skills[0] = Skills(move, *m, overrides);
    }
    for (auto& s : p.skills) {
        s.critical_strike_rate = 0;
        if (hit_forced) { s.must_hit = true; s.accuracy = 100; }
    }
    return p;
}
struct Fight {
    std::vector<BattleEvent> events;
    std::unique_ptr<ScenarioBattle> b;
    Fight(int side = 0, bool first = true, int move = kMove, int atk = 300,
          const std::vector<int>& params = {}, bool mirror = false) {
        auto p = carrier(move, first ? 300 : 100, atk, params);
        auto q = carrier(mirror ? (side == 0 ? kCarrierPure2 : kCarrierPure1)
                                : kCarrierPure1,
                         first ? 100 : 300, mirror ? atk : 1);
        b = std::make_unique<ScenarioBattle>(make_party(side == 0 ? p : q),
                                             make_party(side == 0 ? q : p));
        b->start();
        ctx().event_center_.set_delivery_sink(&events);
    }
    BattleContext& ctx() { return b->ctx(); }
    ElfPet& pet(int side) { return b->pet(side); }
};
int pp_events(Fight& f, int target, int amount = -1) {
    int n = 0;
    for (const auto& e : f.events) if (e.type == EventType::EVENT_PP_REDUCED
        && e.actor == -1 && e.target == target && (amount < 0 || e.amount == amount)) ++n;
    return n;
}
void turn(Fight& f, int s0, int s1, bool complete = true) {
    f.events.clear();
    const int r = f.ctx().roundCount;
    f.b->submit_operation(0, s0 < 0 ? Action::NONE : Action::SELECT_SKILL, std::max(0, s0));
    f.b->submit_operation(1, s1 < 0 ? Action::NONE : Action::SELECT_SKILL, std::max(0, s1));
    f.b->drive();
    if (complete) CHECK_EQ(f.ctx().roundCount, r + 1, "有界单回合FSM");
}
void own(Fight& f, int side, int skill = 0, int other = -1, bool complete = true) {
    turn(f, side == 0 ? skill : other, side == 1 ? skill : other, complete);
}
void swap(Fight& f, int side, int slot_idx) {
    f.events.clear();
    const int r = f.ctx().roundCount;
    f.b->submit_operation(side, Action::CHOOSE_PET, slot_idx);
    f.b->submit_operation(1 - side, Action::NONE, 0);
    f.b->drive();
    CHECK_EQ(f.ctx().on_stage[side], slot_idx, "真实换宠到位");
    CHECK_EQ(f.ctx().roundCount, r + 1, "换宠有界单回合");
}
void no_resident(Fight& f, const std::string& label) {
    const std::string dump = f.b->dump();
    std::size_t pos = 0;
    int active = 0;
    while ((pos = dump.find("\"effectId\":1555,", pos)) != std::string::npos) {
        const auto end = dump.find('}', pos);
        const auto e = dump.substr(pos, end - pos);
        pos = end + 1;
        if (e.find("\"isRoundEffect\":true") != std::string::npos
            && e.find("\"actionOneShot\":false") != std::string::npos
            && e.find("\"isExpired\":false") != std::string::npos) ++active;
    }
    CHECK_EQ(active, 0, label + " 1555 无驻留回合窗");
}
// 键段审计：1555 段只有本人审计键（owner×16+slot，跨 32 边界即越界）；1710 段
// 零占用；on_stage/soulmark 存储零键（soul 同号无运行时实现免子块）。
void key_audit(Fight& f, const std::string& label) {
    int mine = 0;
    for (const auto& [k, v] : f.ctx().plugin_storage) {
        (void)v;
        if (k >= k1555Base && k < k1555Base + 32) { ++mine; continue; }
        CHECK(k < 1015550000 || k >= 1015550000 + 10000, label + " 无 1555 越界键");
        CHECK(!(k >= 1017100000 && k < 1017100000 + 10000), label + " 1710 段零占用");
    }
    CHECK(mine > 0, label + " 1555 审计键在位");
    for (int s : {0, 1}) for (const auto& p : f.ctx().seerRobot[s].elfPets) {
        CHECK(p.on_stage_storage.empty() && p.soulmark_storage.empty(),
              label + " 无 on_stage/soulmark storage 键");
    }
}
}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) return 1;
    sbs_srand(501);
    // A：四载体全链切片（side_effect.arg_count 优先，双表实切零剩余）。
    {
        const std::vector<std::pair<int, std::vector<std::pair<int, std::vector<int>>>>> want = {
            {kCarrierAttr, {{48, {4}}, {1555, {1}}, {780, {3, 100, 1}}}},
            {kCarrierChain, {{913, {100}}, {514, {25}}, {1555, {1}}}},
            {kCarrierPure1, {{1555, {1}}}},
            {kCarrierPure2, {{1555, {1}}}},
        };
        auto& repo = official_data::OfficialDataStore::instance().repository();
        for (const auto& [move, chain] : want) {
            const auto got = repo.load_skill_effects(move);
            CHECK_EQ(got.size(), chain.size(), "A 载体链长 " + std::to_string(move));
            for (std::size_t i = 0; i < chain.size(); ++i) {
                CHECK_EQ(got[i].effect_id, chain[i].first, "A 顺序");
                CHECK(got[i].args == chain[i].second, "A 全链参数无错切");
            }
        }
    }
    // B：纯链载体门开（对手无提升）——五槽各 -1、审计/事件对账。
    {
        Fight f(0, true);
        const int before = total_pp(f.pet(1));
        own(f, 0);
        CHECK_EQ(f.pet(1).skills[0].pp, f.pet(1).skills[0].maxPP - 1, "B 槽0 -1");
        CHECK_EQ(f.pet(1).skills[1].pp, f.pet(1).skills[1].maxPP - 1, "B 槽1 -1");
        CHECK_EQ(f.pet(1).skills[4].pp, f.pet(1).skills[4].maxPP - 1, "B 槽4 -1");
        CHECK_EQ(before - total_pp(f.pet(1)), 5, "B 合计恰 -5");
        CHECK_EQ(slot(f.ctx(), 0, 0), 1, "B 行参落账");
        CHECK_EQ(slot(f.ctx(), 0, 1), 0, "B 门开（无提升）");
        CHECK_EQ(slot(f.ctx(), 0, 2), 1, "B fire 恰一次");
        CHECK_EQ(slot(f.ctx(), 0, 3), 5, "B 实扣合计=五槽各1");
        CHECK_EQ(slot(f.ctx(), 0, 8), 1, "B 进入计数");
        CHECK_EQ(slot(f.ctx(), 0, 9), 0, "B 无跳过");
        CHECK_EQ(slot(f.ctx(), 1, 0), 0, "B 对手侧零串写");
        CHECK_EQ(pp_events(f, 1), 5, "B 五槽各一条 EVENT_PP_REDUCED");
        no_resident(f, "B");
        key_audit(f, "B");
    }
    // C：门正（对手任一正槽提升 → 整句门闭，不扣 PP）。
    {
        Fight f(0, true);
        const int before = total_pp(f.pet(1));
        put_boost(f.ctx(), 1, 0, 2);
        own(f, 0);
        CHECK_EQ(total_pp(f.pet(1)), before, "C PP 分文未动");
        CHECK_EQ(slot(f.ctx(), 0, 1), 1, "C 门闭落账（对手有提升）");
        CHECK_EQ(slot(f.ctx(), 0, 2), 0, "C 零 fire");
        CHECK_EQ(slot(f.ctx(), 0, 8), 1, "C 进入计数仍=1（门闭非未进入）");
        CHECK_EQ(slot(f.ctx(), 0, 9), 1, "C 跳过原因=对手提升");
        CHECK_EQ(pp_events(f, 1), 0, "C 零事件");
        // 命中槽正等级也闭门（六槽全扫描，非只攻/防）。
        Fight g(0, true);
        put_boost(g.ctx(), 1, 5, 1);
        own(g, 0);
        CHECK_EQ(slot(g.ctx(), 0, 1), 1, "C 命中槽正等级同样闭门");
        CHECK_EQ(pp_events(g, 1), 0, "C 命中槽零事件");
    }
    // D：存储面判别（SKILL_EFFECT 桶内视图手术先行——视图摆满正槽但存储为 0，
    // 1555 仍开门扣 PP；读视图的变异实现会把手术后的虚提升当门，D 腿即红）。
    {
        Fight f(0, true);
        Effect e(1555, 0, 0, 0, EffectArgs(std::vector<int>{1, 0, 3}), &flip_view);
        auto ce = std::make_unique<ContinuousEffect>(
            e, State::BATTLE_FIRST_SKILL_EFFECT, 0, 1, f.ctx().roundCount);
        f.ctx().register_skill_effect(State::BATTLE_FIRST_SKILL_EFFECT, 0, std::move(ce));
        own(f, 0);
        CHECK_EQ(f.ctx().ws.view_levels[1][0], 3, "D 桶内手术到位（视图=+3）");
        CHECK_EQ(f.ctx().ability_levels[1][0], 0, "D 存储仍为 0");
        CHECK_EQ(slot(f.ctx(), 0, 1), 0, "D 虚提升不开门（读存储面）");
        CHECK_EQ(slot(f.ctx(), 0, 2), 1, "D fire 恰一次");
        CHECK_EQ(f.pet(1).skills[0].pp, f.pet(1).skills[0].maxPP - 1, "D 照常 -1");
    }
    // E：钳制/零 PP/无限槽（pp=1→0、pp=0 不动、pp=-1 跳过、事件逐实变槽）。
    {
        Fight f(0, true);
        f.pet(1).skills[0].pp = 1;
        f.pet(1).skills[1].pp = 0;
        f.pet(1).skills[3].pp = -1;
        own(f, 0);
        CHECK_EQ(f.pet(1).skills[0].pp, 0, "E 钳 0 不为负");
        CHECK_EQ(f.pet(1).skills[1].pp, 0, "E 已零不动");
        CHECK_EQ(f.pet(1).skills[3].pp, -1, "E 无限槽跳过");
        CHECK_EQ(f.pet(1).skills[2].pp, f.pet(1).skills[2].maxPP - 1, "E 正常槽 -1");
        CHECK_EQ(slot(f.ctx(), 0, 3), 3, "E 实扣合计=1+1+1");
        CHECK_EQ(pp_events(f, 1), 3, "E 三个实变槽各一条事件");
        CHECK_EQ(slot(f.ctx(), 0, 2), 1, "E fire 恰一次（原语一次调用）");
    }
    // F：行参透传（0=自然零点不跳过登记；3=逐槽 -3）。
    {
        Fight f(0, true, kMove, 300, {0});
        own(f, 0);
        CHECK_EQ(total_pp(f.pet(1)), 5 + 35 + 25 + 35 + 25, "F 0 档零变动");
        CHECK_EQ(slot(f.ctx(), 0, 0), 0, "F 行参 0 落账");
        CHECK_EQ(slot(f.ctx(), 0, 1), 0, "F 门开");
        CHECK_EQ(slot(f.ctx(), 0, 2), 0, "F 0 档零 fire（INVALID_PARAM）");
        CHECK_EQ(slot(f.ctx(), 0, 9), 0, "F 0 档非跳过（自然零点）");
        CHECK_EQ(pp_events(f, 1), 0, "F 0 档零事件");
    }
    {
        Fight f(0, true, kMove, 300, {3});
        own(f, 0);
        CHECK_EQ(f.pet(1).skills[0].pp, f.pet(1).skills[0].maxPP - 3, "F 3 档槽0 -3");
        CHECK_EQ(slot(f.ctx(), 0, 0), 3, "F 行参 3 落账");
        CHECK_EQ(slot(f.ctx(), 0, 3), 15, "F 实扣合计=5×3");
    }
    // G：镜像（双侧各用各自纯链载体互扣；先/后手两序）。PP 断言含使用费
    // （AFTER_ACTION 直写 -1）+ 对扣 -1；事件面只有被削五槽。
    for (bool first : {true, false}) {
        Fight f(0, first, kMove, 300, {}, true);
        turn(f, 0, 0);
        CHECK_EQ(f.pet(0).skills[0].pp, f.pet(0).skills[0].maxPP - 2, "G 侧0 对扣1+自耗1");
        CHECK_EQ(f.pet(1).skills[0].pp, f.pet(1).skills[0].maxPP - 2, "G 侧1 对扣1+自耗1");
        CHECK_EQ(slot(f.ctx(), 0, 1), 0, "G 侧0 门开");
        CHECK_EQ(slot(f.ctx(), 1, 1), 0, "G 侧1 门开");
        CHECK_EQ(slot(f.ctx(), 0, 2), 1, "G 侧0 fire 恰一次");
        CHECK_EQ(slot(f.ctx(), 1, 2), 1, "G 侧1 fire 恰一次");
        CHECK_EQ(slot(f.ctx(), 0, 8), 1, "G 侧0 进入计数");
        CHECK_EQ(slot(f.ctx(), 1, 8), 1, "G 侧1 进入计数");
        CHECK_EQ(pp_events(f, 0), 5, "G 侧0 被削五槽事件");
        CHECK_EQ(pp_events(f, 1), 5, "G 侧1 被削五槽事件");
        no_resident(f, "G");
    }
    // H：miss/被控/被盔不通过命中趟（结构性不触发，进入计数=0）。
    {
        Fight miss(0, true);
        miss.pet(0).skills[0].must_hit = false;
        miss.pet(0).skills[0].accuracy = 0;
        const int before = total_pp(miss.pet(1));
        own(miss, 0);
        CHECK_EQ(slot(miss.ctx(), 0, 8), 0, "H miss 未进 SKILL_EFFECT");
        CHECK_EQ(total_pp(miss.pet(1)), before, "H miss 零扣");
        CHECK_EQ(pp_events(miss, 1), 0, "H miss 零事件");
        Fight held(0, true);
        REQUIRE(apply_anomaly_raw(&held.ctx(), 0, 6, 1, 1) == ApplyAnomalyResult::SUCCESS);
        own(held, 0);
        CHECK_EQ(slot(held.ctx(), 0, 8), 0, "H 被控未使用");
        CHECK_EQ(pp_events(held, 1), 0, "H 被控零事件");
        Fight seal(0, true);
        (void)core_api().seal_skill(&seal.ctx(), 1, 0, 1555, false, true, 1, 0, false,
                                    -1, EffectScope::ON_STAGE, false, 100, false);
        own(seal, 0);
        CHECK_EQ(seal.ctx().ws.raw_attack_damage[0], 0, "H 被盔无红伤");
        CHECK_EQ(slot(seal.ctx(), 0, 8), 0, "H 被盔未进命中趟");
        CHECK_EQ(pp_events(seal, 1), 0, "H 被盔零事件");
    }
    // I：属性载体 27880（cat4 也算命中）——1555 照扣；同链 780 窗不误归零。
    {
        Fight f(0, true, kCarrierAttr);
        own(f, 0);
        CHECK_EQ(f.pet(1).skills[0].pp, f.pet(1).skills[0].maxPP - 1, "I 属性技 -1");
        CHECK_EQ(slot(f.ctx(), 0, 8), 1, "I 属性技进入计数");
        CHECK_EQ(slot(f.ctx(), 0, 3), 5, "I 全槽 -1（780 未误归零）");
        CHECK_EQ(pp_events(f, 1), 5, "I 五槽事件（非归零事件面）");
        no_resident(f, "I");
    }
    // J：同链激活载体 34056（913 消强失败/514 满血零伤均惰性，1555 照扣）。
    {
        Fight f(0, true, kCarrierChain);
        own(f, 0);
        CHECK_EQ(f.pet(1).skills[0].pp, f.pet(1).skills[0].maxPP - 1, "J 链载体 -1");
        CHECK_EQ(slot(f.ctx(), 0, 2), 1, "J 链载体 fire");
        CHECK_EQ(slot(f.ctx(), 0, 0), 1, "J 链载体行参 1");
    }
    // K：换宠离场——扣减只在被扣宠本体，替补零沾；审计键无行为残留。
    {
        Fight f(0, true);
        own(f, 0);
        CHECK_EQ(f.pet(1).skills[0].pp, f.pet(1).skills[0].maxPP - 1, "K 首扣到位");
        swap(f, 1, 1);
        CHECK_EQ(f.pet(1).skills[0].pp, f.pet(1).skills[0].maxPP, "K 替补零沾");
        own(f, 0);
        CHECK_EQ(f.pet(1).skills[0].pp, f.pet(1).skills[0].maxPP - 1, "K 新宠独立扣减");
        CHECK_EQ(slot(f.ctx(), 0, 8), 2, "K 第二次使用进入计数=2");
        swap(f, 0, 1);
        CHECK_EQ(f.pet(0).skills[0].pp, f.pet(0).skills[0].maxPP, "K 施放侧替补零沾");
        no_resident(f, "K");
        key_audit(f, "K");
    }
    std::cout << "scenario_8890_k501 PASS (1555 pp gate reduce)\n";
    return 0;
}
