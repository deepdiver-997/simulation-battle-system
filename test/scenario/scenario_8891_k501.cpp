// K501 / 8891：1710 掷点伤害翻倍+对手异常概率门；真实FSM，不直接调用被测函数。
// 口径依据 〔源: docs_local/docs/05-任务清单/已收官/E组批量效果工单.md:106〕
// 数据 https://github.com/SeerAPI/seer-unity-assets ，四载体完整链/当前双表实切。
// 1710 = SKILL_EFFECT 命中趟一次性：异常门（对手全合法码 has_active_anomaly 任一
// 命中→概率保持 {0}；全不命中→概率×2，吃 K6 虚拟视图遮蔽）→ 本地 roll_percent
// 掷点（0/100 短路不耗 rand；无 ChanceTag 申报面——闸门仅 AnomalyAttach，K456
// 头注同款口径）→ 命中沿 SkillView 效果侧赋值通道把当前视图×2 并置 rewritten
// （K491 2204 一次性直系，非 K489 跨回合窗、非伤害 AMP、非静态改值）。
// 〔源: resources/moves_lib/k501_pp_gate_and_chance_double.cpp:52-72〕
// 〔源: resources/moves_lib/k491_conditional_power.cpp:55-71〕
// 掷点档差分 = 审计槽精确断言（slot2=有效概率）+ MINSTD 独立递推预测掷歪/掷中；
// 翻倍面 = 观察器在 ATTACK_DAMAGE 被动桶收尾重算前采样（8790 同款）+ 红伤带。
// 覆盖四载体、两侧先后手、门正负、遮蔽、附属/高码异常、0/50/100/200 档与
// 短路不耗 rand、当前视图而非静态值（烧伤物化/0威力）、miss/被控/被盔/属性技、
// 同链新施加即时生效、镜像、换宠离场、无驻留窗、键段审计。

#include <array>
#include <cmath>
#include <cstdint>
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
constexpr int kCarrierPure1 = 36764;  // 引光灵息 [1710][50]，cat2 特攻130（learner 4536）
constexpr int kCarrierPure2 = 36976;  // 急火飞奔 [1710][50]，cat1 物理130（learner 4579）
constexpr int kCarrierChain1 = 35264;  // 乌天魂风祭 [1124,1710,1206][2,100,50,20,10,40]（hide 4254）
constexpr int kCarrierChain2 = 36918;  // 澹拂花漫径 [1710,1106,989][50,320,100,8,320,300]（learner 4566）
constexpr int kMove = kCarrierPure1;
constexpr int kHit1 = 10004;
constexpr int kHit2 = 10013;
constexpr int kHp = 50000;
constexpr int k1710Base = 1017100000;

int learner(int move) {
    switch (move) {
        case kCarrierPure2: return 4579;
        case kCarrierChain1: return 4254;
        case kCarrierChain2: return 4566;
        default: return 4536;
    }
}
std::array<int, 2> observed_power{};
std::array<bool, 2> observed_rewritten{};
EffectResult observe_power(BattleContext* c, const EffectArgs& a) {
    const int side = a.int_args[0];
    observed_power[side] = c->ws.skill_power_view[side].value;
    observed_rewritten[side] = c->ws.skill_power_view[side].rewritten;
    return EffectResult::kOk;
}
void watch_power(ScenarioBattle& b) {
    auto& c = b.ctx();
    observed_power = {-1, -1};
    observed_rewritten = {false, false};
    for (int side : {0, 1}) {
        for (State state : {State::BATTLE_FIRST_ATTACK_DAMAGE,
                            State::BATTLE_SECOND_ATTACK_DAMAGE}) {
            Effect e(1710, 0, side, -1, EffectArgs(std::vector<int>{side}), &observe_power);
            auto ce = std::make_unique<ContinuousEffect>(e, state, side, -1, c.roundCount);
            ce->source_id_ = 1710;
            c.passive_effects.register_effect(state, side, std::move(ce),
                                              c.round_effect_valid_id[side]);
        }
    }
}
int stor(BattleContext& c, int key) {
    const auto it = c.plugin_storage.find(key);
    return it == c.plugin_storage.end() ? 0 : std::any_cast<int>(it->second);
}
int slot(BattleContext& c, int side, int n) { return stor(c, k1710Base + side * 16 + n); }
ElfPet carrier(int move, int speed, int atk, const std::vector<int>& params = {},
               bool hit_forced = true) {
    auto p = make_pet(learner(move), "K501-1710", {move, kHit1, kHit2, kHit1, kHit2},
                      speed, atk, kHp);
    if (!params.empty()) {
        REQUIRE(params.size() == 1);
        const auto m = official_data::OfficialDataStore::instance().repository()
                           .load_monster(learner(move));
        REQUIRE(m.has_value());
        std::vector<EffectParamOverride> overrides;
        overrides.push_back({move, 1710, 0, params[0]});
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
        watch_power(*b);
        ctx().event_center_.set_delivery_sink(&events);
    }
    BattleContext& ctx() { return b->ctx(); }
    ElfPet& pet(int side) { return b->pet(side); }
};
void turn(Fight& f, int s0, int s1, bool complete = true) {
    f.events.clear();
    observed_power = {-1, -1};
    observed_rewritten = {false, false};
    const int r = f.ctx().roundCount;
    f.b->submit_operation(0, s0 < 0 ? Action::NONE : Action::SELECT_SKILL, std::max(0, s0));
    f.b->submit_operation(1, s1 < 0 ? Action::NONE : Action::SELECT_SKILL, std::max(0, s1));
    f.b->drive();
    if (complete) CHECK_EQ(f.ctx().roundCount, r + 1, "有界单回合FSM");
}
void own(Fight& f, int side, int skill = 0, int other = -1, bool complete = true) {
    turn(f, side == 0 ? skill : other, side == 1 ? skill : other, complete);
}
int red(Fight& f, int side) {
    int n = 0;
    for (const auto& e : f.events) if (e.type == EventType::EVENT_TAKE_DAMAGE
        && e.actor == side && e.target == 1 - side
        && e.damage_kind == static_cast<int>(DamageKind::NORMAL)
        && (e.state == static_cast<int>(State::BATTLE_FIRST_ATTACK_DAMAGE)
            || e.state == static_cast<int>(State::BATTLE_SECOND_ATTACK_DAMAGE)))
        n += e.amount;
    return n;
}
void put_anomaly(Fight& f, int side, int code, int rounds = 3) {
    REQUIRE(apply_anomaly_raw(&f.ctx(), side, code, rounds, 1 - side)
                == ApplyAnomalyResult::SUCCESS);
}
void check_band(Fight& f, int side, int power, const std::string& label) {
    // 面板300/100、无本系/克制、暴击清零；红伤按改后威力重算（8790 同式）。
    const double raw = 0.84 * 300.0 / 100.0 * power + 2.0;
    const int damage = f.ctx().ws.raw_attack_damage[side];
    CHECK(damage >= static_cast<int>(std::floor(raw * 0.85)) - 2
              && damage <= static_cast<int>(std::floor(raw)) + 2,
          label + " 按改后威力重算红伤=" + std::to_string(damage));
}
void swap(Fight& f, int side, int slot_idx) {
    f.events.clear();
    const int r = f.ctx().roundCount;
    f.b->submit_operation(side, Action::CHOOSE_PET, slot_idx);
    f.b->submit_operation(1 - side, Action::NONE, 0);
    f.b->drive();
    CHECK_EQ(f.ctx().on_stage[side], slot_idx, "真实换宠到位");
    CHECK_EQ(f.ctx().roundCount, r + 1, "换宠有界单回合");
    watch_power(*f.b);
}
void no_resident(Fight& f, const std::string& label) {
    const std::string dump = f.b->dump();
    std::size_t pos = 0;
    int active = 0;
    while ((pos = dump.find("\"effectId\":1710,", pos)) != std::string::npos) {
        const auto end = dump.find('}', pos);
        const auto e = dump.substr(pos, end - pos);
        pos = end + 1;
        if (e.find("\"isRoundEffect\":true") != std::string::npos
            && e.find("\"actionOneShot\":false") != std::string::npos
            && e.find("\"isExpired\":false") != std::string::npos) ++active;
    }
    CHECK_EQ(active, 0, label + " 1710 无驻留回合窗（一次性）");
}
void key_audit(Fight& f, const std::string& label) {
    int mine = 0;
    for (const auto& [k, v] : f.ctx().plugin_storage) {
        (void)v;
        if (k >= k1710Base && k < k1710Base + 32) { ++mine; continue; }
        CHECK(k < 1017100000 || k >= 1017100000 + 10000, label + " 无 1710 越界键");
        CHECK(!(k >= 1015550000 && k < 1015550000 + 10000), label + " 1555 段零占用");
    }
    CHECK(mine > 0, label + " 1710 审计键在位");
    for (int s : {0, 1}) for (const auto& p : f.ctx().seerRobot[s].elfPets) {
        CHECK(p.on_stage_storage.empty() && p.soulmark_storage.empty(),
              label + " 无 on_stage/soulmark storage 键");
    }
}
std::uint64_t next_rng(std::uint64_t state) { return state * 16807ULL % 2147483647ULL; }
}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) return 1;
    sbs_srand(501);
    // A：四载体全链切片（双表实切零剩余）。
    {
        const std::vector<std::pair<int, std::vector<std::pair<int, std::vector<int>>>>> want = {
            {kCarrierChain1, {{1124, {2, 100}}, {1710, {50}}, {1206, {20, 10, 40}}}},
            {kCarrierChain2, {{1710, {50}}, {1106, {320, 100, 8}}, {989, {320, 300}}}},
            {kCarrierPure1, {{1710, {50}}}},
            {kCarrierPure2, {{1710, {50}}}},
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
    // B：门开档（对手无异常 → 50×2=100 → 必翻倍，掷点零耗 rand）。
    // 掷点档差分主锚：审计槽 slot2=100 + 观察器 260 + rewritten + 红伤带。
    {
        Fight f(0, true);
        sbs_srand(11);
        own(f, 0);
        CHECK_EQ(observed_power[0], 260, "B 当前视图×2=260");
        CHECK(observed_rewritten[0], "B rewritten 置位");
        CHECK_EQ(observed_power[1], -1, "B 对手视图未物化（未 touch）");
        CHECK(!observed_rewritten[1], "B 对手无改写");
        check_band(f, 0, 260, "B 翻倍红伤带");
        CHECK_EQ(slot(f.ctx(), 0, 0), 50, "B 行参 50 落账");
        CHECK_EQ(slot(f.ctx(), 0, 1), 0, "B 异常门开（对手无异常）");
        CHECK_EQ(slot(f.ctx(), 0, 2), 100, "B 有效概率=100（翻倍档）");
        CHECK_EQ(slot(f.ctx(), 0, 3), 1, "B 掷中");
        CHECK_EQ(slot(f.ctx(), 0, 8), 1, "B 进入计数");
        CHECK_EQ(slot(f.ctx(), 0, 9), 0, "B 无跳过");
        CHECK_EQ(slot(f.ctx(), 1, 0), 0, "B 对手侧零串写");
        no_resident(f, "B");
        // 掷点零耗锚：终态=种子推进 2 次（初始伤害浮动 + rewritten 重算浮动），
        // 掷点本身 0 消费（100 短路）；0 档单次计算=推进 1 次，同证掷点 0 消费。
        CHECK_EQ(sbs_rand_state(), next_rng(next_rng(11)), "B 100 档短路零耗掷点");
        Fight z(0, true, kMove, 300, {0});
        sbs_srand(11);
        own(z, 0);
        CHECK_EQ(observed_power[0], 130, "B0 档不翻倍");
        CHECK(!observed_rewritten[0], "B0 无改写");
        check_band(z, 0, 130, "B0 基线红伤带");
        CHECK_EQ(slot(z.ctx(), 0, 2), 0, "B0 有效概率=0");
        CHECK_EQ(slot(z.ctx(), 0, 3), 0, "B0 掷歪（短路）");
        CHECK_EQ(sbs_rand_state(), next_rng(11), "B0 0 档短路零耗掷点（单计算）");
        key_audit(f, "B");
    }
    // C：门闭档（对手有异常 → 概率保持 50 → MINSTD 独立递推预测掷点）。
    int doubled_seeds = 0;
    for (unsigned seed = 1; seed <= 24; ++seed) {
        Fight f(0, true);
        put_anomaly(f, 1, 1);
        sbs_srand(seed);
        own(f, 0);
        const bool hit = next_rng(seed) % 100 < 50;  // SKILL_EFFECT 首消费=掷点
        CHECK_EQ(slot(f.ctx(), 0, 1), 1, "C 异常门闭（对手有异常）");
        CHECK_EQ(slot(f.ctx(), 0, 2), 50, "C 有效概率=50（不翻倍档）");
        CHECK_EQ(slot(f.ctx(), 0, 3), hit ? 1 : 0, "C 掷点真值 " + std::to_string(seed));
        if (hit) {
            CHECK_EQ(observed_power[0], 260, "C 掷中翻倍 " + std::to_string(seed));
            CHECK(observed_rewritten[0], "C 掷中改写 " + std::to_string(seed));
            check_band(f, 0, 260, "C 掷中红伤带");
            ++doubled_seeds;
        } else {
            CHECK_EQ(observed_power[0], 130, "C 掷歪不翻倍 " + std::to_string(seed));
            CHECK(!observed_rewritten[0], "C 掷歪无改写 " + std::to_string(seed));
            check_band(f, 0, 130, "C 掷歪红伤带");
        }
        // 同种子重跑确定性。
        const int first_roll = slot(f.ctx(), 0, 3);
        const int first_power = observed_power[0];
        Fight g(0, true);
        put_anomaly(g, 1, 1);
        sbs_srand(seed);
        own(g, 0);
        CHECK_EQ(slot(g.ctx(), 0, 3), first_roll, "C 同种子掷点一致 " + std::to_string(seed));
        CHECK_EQ(observed_power[0], first_power, "C 同种子局面一致 " + std::to_string(seed));
    }
    CHECK(doubled_seeds > 0 && doubled_seeds < 24, "C 24 种子双结局均出现");
    // D：行参档差分（100 原始档异常门闭仍必中；200 原始档开门越 100 短路）。
    {
        Fight f(0, true, kMove, 300, {100});
        put_anomaly(f, 1, 6);
        sbs_srand(3);
        own(f, 0);
        CHECK_EQ(slot(f.ctx(), 0, 0), 100, "D 行参 100 落账");
        CHECK_EQ(slot(f.ctx(), 0, 1), 1, "D 门闭（有异常）");
        CHECK_EQ(slot(f.ctx(), 0, 2), 100, "D 有效概率=100（不翻倍也必中）");
        CHECK_EQ(slot(f.ctx(), 0, 3), 1, "D 必中");
        CHECK_EQ(observed_power[0], 260, "D 翻倍照常");
        const auto st = sbs_rand_state();
        Fight g(0, true, kMove, 300, {200});
        sbs_srand(3);
        own(g, 0);
        CHECK_EQ(slot(g.ctx(), 0, 2), 400, "D 开门有效概率=200×2（≥100 短路）");
        CHECK_EQ(slot(g.ctx(), 0, 3), 1, "D 越档必中");
        CHECK_EQ(observed_power[0], 260, "D 越档照常×2");
        CHECK_EQ(sbs_rand_state(), st, "D ≥100 短路不耗掷点");
    }
    // E：遮蔽差分（K6 虚拟视图——本体异常被遮蔽 → 门按"视为无异常"翻倍）。
    {
        Fight f(0, true);
        put_anomaly(f, 1, 1);
        core_api().set_anomaly_view_layers(&f.ctx(), 1710, 1, 1);
        sbs_srand(7);
        own(f, 0);
        CHECK_EQ(slot(f.ctx(), 0, 1), 0, "E 遮蔽后门开（虚拟视图判读）");
        CHECK_EQ(slot(f.ctx(), 0, 2), 100, "E 遮蔽后有效概率翻倍");
        CHECK_EQ(slot(f.ctx(), 0, 3), 1, "E 必中");
        CHECK_EQ(observed_power[0], 260, "E 遮蔽照常翻倍");
        core_api().set_anomaly_view_layers(&f.ctx(), 1710, 1, 0);
        Fight g(0, true);
        put_anomaly(g, 1, 1);
        sbs_srand(7);
        own(g, 0);
        CHECK_EQ(slot(g.ctx(), 0, 1), 1, "E 解除遮蔽恢复门闭");
        CHECK_EQ(slot(g.ctx(), 0, 2), 50, "E 恢复不翻倍档");
    }
    // F：异常不限控制/低码——附属类与高码同样闭门（全合法码扫描）。
    for (int code : {1, 14, 28, 43}) {
        Fight f(0, true);
        put_anomaly(f, 1, code);
        sbs_srand(9);
        own(f, 0);
        CHECK_EQ(slot(f.ctx(), 0, 1), 1, "F 任意异常码闭门 " + std::to_string(code));
        CHECK_EQ(slot(f.ctx(), 0, 2), 50, "F 不翻倍档 " + std::to_string(code));
    }
    // G：同链激活载体 36918（1106 红伤>320 后 100% 附加睡眠）。睡眠带受击苏醒
    // （anomaly_on_attack_hit_hook 于 ON_SKILL_HIT、先于 SKILL_EFFECT 桶，
    // battleFsm.cpp:239-244）→ 次轮攻击命中即唤醒，门按唤醒后判读——与 K491 2204
    // 的 SKILL_EFFECT 时点判读先例一致，非实现缺陷。门闭差分由 C/F 腿承担。
    {
        Fight f(0, true, kCarrierChain2);
        sbs_srand(13);
        own(f, 0);
        CHECK_EQ(slot(f.ctx(), 0, 1), 0, "G 首轮门开");
        CHECK_EQ(slot(f.ctx(), 0, 2), 100, "G 首轮翻倍档");
        CHECK_EQ(observed_power[0], 300, "G 150×2=300");
        check_band(f, 0, 300, "G 翻倍红伤带");
        CHECK(red(f, 0) > 320, "G 红伤过 1106 阈值（附加支激活前提）");
        CHECK_EQ(f.ctx().has_active_abnormal_status(1, 8), true, "G 1106 附加睡眠落地");
        no_resident(f, "G");
        sbs_srand(13);
        own(f, 0);
        // 次轮：受击苏醒（ON_SKILL_HIT 先于 SKILL_EFFECT）→ 门读干净 → 照常翻倍；
        // 伤害后 1106 红伤>320 再次 100% 附加 → 轮末睡眠又在（复合账两断言齐验）。
        CHECK_EQ(slot(f.ctx(), 0, 1), 0, "G 唤醒后门开（SKILL_EFFECT 时点判读）");
        CHECK_EQ(slot(f.ctx(), 0, 2), 100, "G 唤醒后翻倍档");
        CHECK_EQ(observed_power[0], 300, "G 唤醒后照常翻倍");
        CHECK_EQ(f.ctx().has_active_abnormal_status(1, 8), true, "G 1106 次轮再附加");
    }
    // H：纯链物攻载体 36976 + 链载体 35264（1206 粉伤并发，红伤判定不受扰）。
    {
        Fight f(0, true, kCarrierPure2);
        sbs_srand(17);
        own(f, 0);
        CHECK_EQ(observed_power[0], 260, "H 物攻 130×2");
        check_band(f, 0, 260, "H 物攻翻倍带");
        no_resident(f, "H");
    }
    {
        Fight f(0, true, kCarrierChain1);
        sbs_srand(17);
        own(f, 0);
        CHECK_EQ(observed_power[0], 320, "H 链载体 160×2");
        check_band(f, 0, 320, "H 链载体翻倍带");
        int pink = 0;
        for (const auto& e : f.events) if (e.type == EventType::EVENT_TAKE_DAMAGE
            && e.actor == 0 && e.target == 1
            && e.damage_kind != static_cast<int>(DamageKind::NORMAL)) pink += e.amount;
        CHECK(pink > 0, "H 1206 粉伤并发（不混红伤账）");
        CHECK_EQ(slot(f.ctx(), 0, 2), 100, "H 链载体翻倍档");
    }
    // I：miss/被控/被盔不通过命中趟；属性技语境跳掷点。
    {
        Fight miss(0, true);
        miss.pet(0).skills[0].must_hit = false;
        miss.pet(0).skills[0].accuracy = 0;
        own(miss, 0);
        CHECK_EQ(slot(miss.ctx(), 0, 8), 0, "I miss 未进 SKILL_EFFECT");
        CHECK(!observed_rewritten[0], "I miss 无改写");
        CHECK_EQ(miss.ctx().ws.raw_attack_damage[0], 0, "I miss 无红伤");
        Fight held(0, true);
        put_anomaly(held, 0, 6, 1);
        own(held, 0);
        CHECK_EQ(slot(held.ctx(), 0, 8), 0, "I 被控未使用");
        CHECK(!observed_rewritten[0], "I 被控无改写");
        Fight seal(0, true);
        (void)core_api().seal_skill(&seal.ctx(), 1, 0, 1710, false, true, 1, 0, false,
                                    -1, EffectScope::ON_STAGE, false, 100, false);
        own(seal, 0);
        CHECK_EQ(seal.ctx().ws.raw_attack_damage[0], 0, "I 被盔无红伤");
        CHECK_EQ(slot(seal.ctx(), 0, 8), 0, "I 被盔未进命中趟");
        CHECK(!observed_rewritten[0], "I 被盔无改写");
        Fight attr(0, true);
        attr.pet(0).skills[0].type = SkillType::Attribute;
        const int hp = attr.pet(1).hp;
        own(attr, 0);
        CHECK_EQ(slot(attr.ctx(), 0, 9), 1, "I 属性技跳过（非攻击语境）");
        CHECK_EQ(slot(attr.ctx(), 0, 8), 0, "I 属性技不进计数");
        CHECK(!observed_rewritten[0], "I 属性技无改写");
        CHECK_EQ(attr.pet(1).hp, hp, "I 属性技无红伤");
    }
    // J：镜像（双侧纯链载体各翻各的刀，先/后手两序）。
    for (bool first : {true, false}) {
        Fight f(0, first, kMove, 300, {}, true);
        sbs_srand(19);
        turn(f, 0, 0);
        CHECK_EQ(observed_power[0], 260, "J 侧0 翻倍");
        CHECK_EQ(observed_power[1], 260, "J 侧1 翻倍");
        CHECK(observed_rewritten[0] && observed_rewritten[1], "J 双侧改写");
        CHECK_EQ(slot(f.ctx(), 0, 2), 100, "J 侧0 翻倍档");
        CHECK_EQ(slot(f.ctx(), 1, 2), 100, "J 侧1 翻倍档");
        CHECK_EQ(slot(f.ctx(), 0, 3), 1, "J 侧0 必中");
        CHECK_EQ(slot(f.ctx(), 1, 3), 1, "J 侧1 必中");
        CHECK(red(f, 0) > red(f, 1) / 2 && red(f, 1) > red(f, 0) / 2, "J 双侧红伤同量级");
        no_resident(f, "J");
    }
    // K：换宠离场——门读当前对手（新宠无异常 → 重新开门）；一次性无残留。
    {
        Fight f(0, true);
        put_anomaly(f, 1, 1);
        sbs_srand(23);
        own(f, 0);
        CHECK_EQ(slot(f.ctx(), 0, 1), 1, "K 首轮门闭");
        swap(f, 1, 1);
        CHECK_EQ(f.ctx().has_active_abnormal_status(1, 1), false, "K 新宠无异常");
        sbs_srand(23);
        own(f, 0);
        CHECK_EQ(slot(f.ctx(), 0, 1), 0, "K 换宠后门开（判读现读）");
        CHECK_EQ(slot(f.ctx(), 0, 2), 100, "K 换宠后翻倍档");
        CHECK_EQ(observed_power[0], 260, "K 新对手照常翻倍");
        swap(f, 0, 1);
        sbs_srand(23);
        own(f, 0, 1);
        CHECK_EQ(slot(f.ctx(), 0, 8), 2, "K 替补（非载体水枪）不进计数");
        CHECK(!observed_rewritten[0], "K 替补无残留改写");
        no_resident(f, "K");
        key_audit(f, "K");
    }
    std::cout << "scenario_8891_k501 PASS (1710 chance power double)\n";
    return 0;
}
