// K503 / 8920：1935 四真实载体切片及真实 FSM 授票；只读观测组合票与异常结果。
// 数据来源 https://github.com/SeerAPI/seer-unity-assets ，2026-11-24 本区 SQLite 只读核验。
// 36545（4496 箫澈）为纯链主载体；28252/28316 属 4484/4510，28758 属 sp_hide_moves 4061。
// 其它载体只验全链加载和授票，避免 28758 自带的 191 回合弹控掩盖次弹缺失。
// 正负/同源刷新/镜像/末次与耗尽/保留原时长/穿透/回合弹控独立响应/离场均由本场景锁定。
// 1935 的所有票均由真实技能施放获得；纯免疫及附加穿透作为对照造料走公开原语。
// 8910 已覆盖多来源票、条件求值顺序、自施加、概率失败、古早施加，不复制其底层矩阵。
// 〔源: test/scenario/scenario_8910_immunity_handles_counted_reflect.cpp:123〕
// 〔源: test/scenario/scenario_8910_immunity_handles_counted_reflect.cpp:170〕
// 原生回程保留时长且不扣接收方票；组合票/回合弹控分别响应，不能 watcher 再施加。
// 〔源: src/primitives/battle_primitives.cpp:284〕〔源: src/primitives/battle_primitives.cpp:312〕
// fullstate 检查 category/sourceOwner/sourceEffectId/counts/rounds/scope/slot，耗尽后条目消失。
// 〔源: src/fsm/battleContext.cpp:1978〕〔源: src/fsm/battleContext.cpp:1985〕

#include <algorithm>
#include <iostream>
#include <memory>
#include <utility>
#include <vector>

#include <entities/pet_factory.h>
#include <nlohmann/json.hpp>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {
using namespace scenario;
using Action = BattleFsm::ActionType;
using Result = ApplyAnomalyResult;
constexpr int kMove = 36545;
constexpr int kConfusion = static_cast<int>(AbnormalStatusId::Confusion);
constexpr int kPoison = static_cast<int>(AbnormalStatusId::Poison);
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);
constexpr int kWeakness = static_cast<int>(AbnormalStatusId::Weakness);

ElfPet carrier(int move, int speed) {
    const int monster = move == 28252 ? 4484 : move == 28316 ? 4510 : move == 28758 ? 4061 : 4496;
    auto pet = make_pet(monster, "K503", {move, 20845, 29209, 10004, 10004}, speed, 1, 50000);
    for (auto& skill : pet.skills) {
        skill.must_hit = 1;
        skill.accuracy = 100;
    }
    return pet;
}

std::unique_ptr<ScenarioBattle> battle(int move = kMove) {
    auto b = std::make_unique<ScenarioBattle>(make_party(carrier(move, 300)),
                                             make_party(carrier(move, 100)));
    b->start();
    return b;
}

void turn(ScenarioBattle& b, int left, int right) {
    const int round = b.ctx().roundCount;
    b.submit_operation(0, left < 0 ? Action::NONE : Action::SELECT_SKILL, std::max(0, left));
    b.submit_operation(1, right < 0 ? Action::NONE : Action::SELECT_SKILL, std::max(0, right));
    b.drive();
    CHECK_EQ(b.ctx().roundCount, round + 1, "真实 FSM 一轮有界结束");
}

void cast(ScenarioBattle& b, int owner, int skill = 0) {
    turn(b, owner == 0 ? skill : -1, owner == 1 ? skill : -1);
}

// 从显示面检查唯一来源票及剩余次数；返回真实句柄用于刷新/镜像的身份差分。
int ticket(ScenarioBattle& b, int owner, int counts) {
    const auto full = nlohmann::json::parse(b.dump());
    int found = 0;
    int handle = 0;
    for (const auto& t : full.at("ruleTickets")) {
        if (t.at("sourceEffectId") != 1935 || t.at("sourceOwner") != owner) continue;
        ++found;
        CHECK_EQ(t.at("category").get<int>(), static_cast<int>(RuleCategory::ANOMALY_REFLECT_COUNTS),
                 "1935 是单一组合票，不是普通次免或回合反弹");
        CHECK_EQ(t.at("counts").get<int>(), counts, "fullstate 剩余次数正确");
        CHECK_EQ(t.at("rounds").get<int>(), 0, "次弹没有回合到期窗口");
        CHECK_EQ(t.at("target").get<int>(), owner, "票保护自身而非对手");
        CHECK_EQ(t.at("sourceSlot").get<int>(), b.ctx().on_stage[owner], "票锚当前施放宠");
        CHECK_EQ(t.at("scope").get<int>(), 0, "票为 ON_STAGE");
        CHECK(!t.at("expired").get<bool>(), "显示票仍有效");
        handle = t.at("grantId").get<int>();
    }
    CHECK_EQ(found, counts > 0 ? 1 : 0, "同侧同源仅一条；耗尽则显示中消失");
    return handle;
}

int remaining(const BattleContext& c, int handle) {
    for (const auto& t : c.rule_center_.entries()) {
        if (t.source_id == handle) return t.remaining_counts;
    }
    return 0;
}

int pure_guard(BattleContext& c, int side, int counts) {
    return c.grant_immunity(side, ImmunityType::ANOMALY, coverage_all(), 0, 0, 0,
                            false, EffectScope::ON_STAGE, counts, 1221);
}

void pierce(BattleContext& c, int actor) {
    c.rule_center_.grant_attach_pierce(actor, 2567, kPierceAnomaly,
        [](BattleContext*, int, int, bool) { return true; }, EffectScope::TEAM);
}

int immuned_events(const std::vector<BattleEvent>& events, int side, int code) {
    return static_cast<int>(std::count_if(events.begin(), events.end(), [&](const BattleEvent& e) {
        return e.type == EventType::EVENT_ANOMALY_IMMUNED && e.target == side && e.amount == code;
    }));
}
}  // namespace

int main() {
    REQUIRE(PetFactory::initialize_runtime_data());
    sbs_srand(8920);

    // A 四载体全链实切并施放；每一条 1935 都只授一张 1 次票。
    const std::vector<std::pair<int, std::vector<std::pair<int, std::vector<int>>>>> carriers{
        {28252, {{962, {1}}, {1935, {1}}, {1804, {1}}}},
        {28316, {{1935, {1}}, {1973, {}}, {945, {}}, {1974, {}}}},
        {28758, {{191, {4}}, {1935, {1}}, {570, {1}}, {1154, {3, 240, 0}}}},
        {36545, {{1935, {1}}}}};
    for (const auto& [move, expected] : carriers) {
        const auto actual = official_data::OfficialDataStore::instance().repository().load_skill_effects(move);
        REQUIRE(actual.size() == expected.size());
        for (std::size_t i = 0; i < expected.size(); ++i) {
            CHECK_EQ(actual[i].effect_id, expected[i].first, "真实载体效果序");
            CHECK(actual[i].args == expected[i].second, "真实载体全链参数无错位");
        }
        auto b = battle(move);
        cast(*b, 0);
        CHECK(ticket(*b, 0, 1) > 0, "真实施放建立组合票");
        ticket(*b, 1, 0);
    }

    // B 正反双方：无施放的负对照；刷新不叠加；空过不减；原生技能末次弹回，耗尽后照落。
    for (int owner : {0, 1}) {
        auto negative = battle();
        cast(*negative, 1 - owner, 1);
        CHECK(negative->ctx().has_active_abnormal_status(owner, kConfusion), "未授票时混乱正常落地");
        CHECK(!negative->ctx().has_active_abnormal_status(1 - owner, kConfusion), "无票不反弹");
        ticket(*negative, owner, 0);

        auto b = battle();
        cast(*b, owner);
        const int handle = ticket(*b, owner, 1);
        cast(*b, owner);
        CHECK_EQ(ticket(*b, owner, 1), handle, "同源再施放刷新原票，不叠次数");
        turn(*b, -1, -1);
        turn(*b, -1, -1);
        CHECK_EQ(ticket(*b, owner, 1), handle, "经过多回合仍有一次");
        const int round = b->ctx().roundCount;
        cast(*b, 1 - owner, 1);  // 混乱魔光 20845：99[100]，原生时长 2。
        CHECK(!b->ctx().has_active_abnormal_status(owner, kConfusion), "末次免疫半边有效");
        CHECK_EQ(b->ctx().get_abnormal_status_end_round(1 - owner, kConfusion), round + 2,
                 "末次弹回同异常，携带原时长");
        ticket(*b, owner, 0);
        CHECK_EQ(apply_anomaly(&b->ctx(), owner, kBurn, 5, 1 - owner), Result::SUCCESS,
                 "耗尽后新异常正常落地");
        CHECK(!b->ctx().has_active_abnormal_status(1 - owner, kBurn), "无回合反弹残留");
        turn(*b, -1, -1);
    }

    // C 镜像：真实双侧先授票；只扣原目标，回程不扣施加方，不乒乓。
    for (int target : {0, 1}) {
        auto b = battle();
        turn(*b, 0, 0);
        const int left = ticket(*b, 0, 1);
        const int right = ticket(*b, 1, 1);
        CHECK(left != right, "镜像双方句柄独立");
        cast(*b, 1 - target, 1);
        ticket(*b, target, 0);
        CHECK_EQ(ticket(*b, 1 - target, 1), target == 0 ? right : left, "回程不误扣对侧末票");
        CHECK(!b->ctx().has_active_abnormal_status(0, kConfusion)
                  && !b->ctx().has_active_abnormal_status(1, kConfusion), "镜像双方免疫且没有反弹循环");
    }

    // D K489 残留反例：单次组合票耗尽后，旧次免不能凭空继续反弹；长时长不是默认 2。
    {
        auto b = battle();
        cast(*b, 1);
        auto& c = b->ctx();
        const int pure = pure_guard(c, 1, 2);
        CHECK_EQ(apply_anomaly(&c, 1, kPoison, 5, 0), Result::REFLECTED, "末次原生回弹");
        CHECK_EQ(c.get_abnormal_status_end_round(0, kPoison), c.roundCount + 5, "完整携带 5 回合原时长");
        CHECK(!c.has_active_abnormal_status(1, kPoison), "目标未中毒");
        ticket(*b, 1, 0);
        CHECK_EQ(remaining(c, pure), 1, "匹配纯次免独立扣一次");
        CHECK_EQ(apply_anomaly(&c, 1, kBurn, 4, 0), Result::TARGET_IMMUNE, "剩余纯次免只挡不弹");
        CHECK(!c.has_active_abnormal_status(0, kBurn), "纯次免不继承耗尽的反弹");
        CHECK_EQ(apply_anomaly(&c, 1, kWeakness, 3, 0), Result::SUCCESS, "两类票耗尽后落地");
        turn(*b, -1, -1);
    }

    // E 穿透：仍弹回并耗最后一次，原目标也落地；纯次免不扣。双侧方向各跑一次。
    for (int target : {0, 1}) {
        auto b = battle();
        cast(*b, target);
        auto& c = b->ctx();
        const int pure = pure_guard(c, target, 2);
        pierce(c, 1 - target);
        CHECK_EQ(apply_anomaly(&c, target, kWeakness, 4, 1 - target), Result::SUCCESS,
                 "穿透原目标继续落地，不改返回结果");
        for (int side : {0, 1}) {
            CHECK_EQ(c.get_abnormal_status_end_round(side, kWeakness), c.roundCount + 4,
                     "原目标与反弹目标同码同时长");
        }
        ticket(*b, target, 0);
        CHECK_EQ(remaining(c, pure), 2, "穿透不耗纯次免");
        CHECK_EQ(apply_anomaly(&c, target, kBurn, 3, 1 - target), Result::SUCCESS, "次弹耗尽不影响穿透落地");
        CHECK(!c.has_active_abnormal_status(1 - target, kBurn), "穿透不能复活次弹");
        turn(*b, -1, -1);
    }

    // F 1935 与真实 2104 独立响应：末次有两趟回程，之后只余一趟回合弹控。
    for (bool pierced : {false, true}) {
        auto b = battle();
        cast(*b, 0);
        cast(*b, 0, 2);  // 毒株散播 29209：483[-1×6] + 2104[3]。
        auto& c = b->ctx();
        ticket(*b, 0, 1);
        CHECK(c.rule_center_.has_reflect(0), "真实 2104 回合弹控并存");
        const int receiver = pure_guard(c, 1, 2);
        const int pure = pure_guard(c, 0, 2);
        if (pierced) pierce(c, 1);
        std::vector<BattleEvent> events;
        c.event_center_.set_delivery_sink(&events);
        CHECK_EQ(apply_anomaly(&c, 0, kBurn, 4, 1), pierced ? Result::SUCCESS : Result::REFLECTED,
                 "穿透与正常施加分别保持原生结果");
        c.event_center_.drain(&c, c.roundCount);
        CHECK_EQ(immuned_events(events, 1, kBurn), 2, "次弹与回合弹控各反一次，不合并");
        CHECK_EQ(immuned_events(events, 0, kBurn), pierced ? 0 : 1, "原目标只结算一次免疫");
        ticket(*b, 0, 0);
        CHECK_EQ(remaining(c, pure), pierced ? 2 : 1, "纯次免按原生路径扣或保留");
        CHECK_EQ(remaining(c, receiver), 2, "两趟回程均不扣接收方次数");
        events.clear();
        CHECK_EQ(apply_anomaly(&c, 0, kPoison, 5, 1), pierced ? Result::SUCCESS : Result::REFLECTED,
                 "次弹耗尽后回合弹控仍独立生效");
        c.event_center_.drain(&c, c.roundCount);
        CHECK_EQ(immuned_events(events, 1, kPoison), 1, "仅余回合弹控一趟回程");
        CHECK_EQ(remaining(c, receiver), 2, "回程仍不扣次数");
        c.event_center_.set_delivery_sink(nullptr);
        turn(*b, -1, -1);
    }

    // G 真换宠清本侧票、不伤镜像对侧；新槽再次真实施放可得新票。
    for (int owner : {0, 1}) {
        auto b = battle();
        turn(*b, 0, 0);
        const int old_handle = ticket(*b, owner, 1);
        b->submit_operation(owner, Action::CHOOSE_PET, 1);
        b->submit_operation(1 - owner, Action::NONE, 0);
        b->drive();
        CHECK_EQ(b->ctx().on_stage[owner], 1, "真实主动换宠完成");
        ticket(*b, owner, 0);
        ticket(*b, 1 - owner, 1);
        CHECK_EQ(apply_anomaly(&b->ctx(), owner, kBurn, 4, 1 - owner), Result::SUCCESS,
                 "接替精灵不继承次弹");
        cast(*b, owner);
        CHECK(ticket(*b, owner, 1) != old_handle, "新宠新授票，不复活旧句柄");
    }

    // H miss / 被控不执行技能，不能授票；纯链避免其它效果掩盖负测。
    for (bool held : {false, true}) {
        auto b = battle();
        if (held) {
            REQUIRE(apply_anomaly_raw(&b->ctx(), 0, 6, 1, 1) == Result::SUCCESS);
        } else {
            b->pet(0).skills[0].must_hit = 0;
            b->pet(0).skills[0].accuracy = 0;
        }
        cast(*b, 0);
        ticket(*b, 0, 0);
    }

    std::cout << "scenario_8920_k503_counted_reflect PASS\n";
    return 0;
}
