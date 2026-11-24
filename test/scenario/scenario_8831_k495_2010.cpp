// K495 / 8831：2010 护盾读面倍乘（对手每存在{0}点护盾则造成的攻击伤害提升{1}倍）。
// 口径依据 〔源: docs_local/docs/05-任务清单/已收官/E组批量效果工单.md:100〕
// 数据来源 https://github.com/SeerAPI/seer-unity-assets ，2026-11-23 当前只读库全链重核。
// 真实FSM：四载体切片/施放、护盾档位线性（199/200/400/600 → 0/1/2/3 档 ×1/×2/×3/×4，
// 整除地板不足一档不计）、SkillView rewritten 面（观察器收尾前采样+精确视图值）、
// 链序快照（36628 全活链：1828 清盾在 2010 之后 → 倍乘按清盾前盘面=×7 档）、
// {1}=0 ×1 改写档、镜像、离场/跨回合无残留、miss/属性技/非载体负测。
// 「每存在{0}点」按每{0}点一档线性（整除地板）定案：官方无实测，载体 [200,1]/
// [100,1]/[250,1] 档位语感 + 2171"每有1级"整数段族同构（插件文件头记档）。
// 〔源: resources/moves_lib/k495_dual_gate_control_and_shield_amp.cpp 文件头〕
// 2010 无驻留显示态（一次性改写），不挂伪窗；审计槽 1020100000+owner×16+slot 头注登记。
// 〔源: docs/07-工具与测试/效果显示gate规范.md:39〕

#include <any>
#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <entities/pet_factory.h>
#include <plugin/core_api.h>
#include "scenario_harness.h"

using namespace scenario;
namespace {

constexpr int kMove200 = 37776;  // 天外叹拘囿 [1487,2010]=[200,1]（cat1 p150）
constexpr int kMove100 = 37855;  // 风尘沙爆 [2010,1678,1232]=[100,1,50]（cat1 p80）
constexpr int kMoveChain = 36628;  // 霜·凌天寂帝枪 [533,2010,1720,1828,1933]=[60,6,...]（cat1 p6）
constexpr int kHit = 10004;
constexpr int kHp = 50000;
constexpr int kAuditBase = 1020100000;   // 10^9 + 2010×10^4
using Action = BattleFsm::ActionType;

std::array<int, 2> observed_power{};
std::array<bool, 2> observed_rewritten{};

int audit(const BattleContext& c, int owner, int slot) {
    const auto it = c.plugin_storage.find(kAuditBase + owner * 16 + slot);
    if (it == c.plugin_storage.end()) {
        return -1;
    }
    if (const int* p = std::any_cast<int>(&it->second)) {
        return *p;
    }
    return -1;
}

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
        for (State state :
             {State::BATTLE_FIRST_ATTACK_DAMAGE, State::BATTLE_SECOND_ATTACK_DAMAGE}) {
            Effect e(2010, 0, side, -1, EffectArgs(std::vector<int>{side}), &observe_power);
            auto ce = std::make_unique<ContinuousEffect>(e, state, side, -1, c.roundCount);
            ce->source_id_ = 2010;
            c.passive_effects.register_effect(state, side, std::move(ce),
                                              c.round_effect_valid_id[side]);
        }
    }
}

// 载体真实归属（monster_learnable_moves/hide_moves 现查 2026-11-23）。
int owner_monster_of(int move) {
    switch (move) {
        case 37776: return 4720;   // 天外叹拘囿
        case 37855: return 4743;   // 风尘沙爆
        case 36628: return 4511;   // 霜·凌天寂帝枪（hide_moves 第五技）
        case 38190: return 4848;   // 寄予无名之枪（hide_moves 第五技）
        default: return 4720;
    }
}

ElfPet carrier(int move, int speed, int threshold = -1, int mult = -1) {
    const int mon = owner_monster_of(move);
    ElfPet p = make_pet(mon, "K495", {move, kHit, 10013, 26449, 20001}, speed, 300, kHp);
    if (threshold >= 0 || mult >= 0) {
        const auto m = official_data::OfficialDataStore::instance().repository().load_monster(mon);
        CHECK(m.has_value(), "fixture: 真实学习者存在");
        std::vector<EffectParamOverride> overrides;
        if (threshold >= 0) overrides.push_back({move, 2010, 0, threshold});
        if (mult >= 0) overrides.push_back({move, 2010, 1, mult});
        p.skills[0] = Skills(move, *m, overrides);
    }
    for (auto& skill : p.skills) {
        skill.must_hit = 1;
        skill.accuracy = 100;
        skill.critical_strike_rate = 0;
    }
    return p;
}

std::unique_ptr<ScenarioBattle> solo(int move = kMove200, int threshold = -1, int mult = -1,
                                     int shield0 = 0, int shield1 = 0) {
    auto p = carrier(move, 300, threshold, mult);
    auto q = carrier(kMove200, 100);
    auto b = std::make_unique<ScenarioBattle>(make_party(p), make_party(q));
    b->start();
    if (shield0 > 0) {
        b->pet(0).shield_bank_.add(/*priority=*/0, shield0, /*source_id=*/9999, /*duration=*/0);
    }
    if (shield1 > 0) {
        b->pet(1).shield_bank_.add(/*priority=*/0, shield1, /*source_id=*/9999, /*duration=*/0);
    }
    watch_power(*b);
    return b;
}

void turn(ScenarioBattle& b, int skill0, int skill1) {
    const int round = b.ctx().roundCount;
    observed_power = {-1, -1};
    observed_rewritten = {false, false};
    b.submit_operation(0, skill0 < 0 ? Action::NONE : Action::SELECT_SKILL, std::max(0, skill0));
    b.submit_operation(1, skill1 < 0 ? Action::NONE : Action::SELECT_SKILL, std::max(0, skill1));
    b.drive();
    CHECK_EQ(b.ctx().roundCount, round + 1, "bounded one-round drive");
}

void check_band(ScenarioBattle& b, int side, int power, int factor, const std::string& label) {
    // 面板 300/100、无本系/克制、无暴击；raw = 0.84×(atk/def)×power×F + 2（K491 同式，
    // ±2 余量盖 floor/取整），伤害带 [floor(raw×0.85)-2, floor(raw)+2]。
    const double raw = 0.84 * 300.0 / 100.0 * power * factor + 2.0;
    const int damage = b.ctx().ws.raw_attack_damage[side];
    CHECK(damage >= static_cast<int>(std::floor(raw * 0.85)) - 2
              && damage <= static_cast<int>(std::floor(raw)) + 2,
          label + " 红伤带（F=" + std::to_string(factor) + " raw=" + std::to_string(raw) +
              " 实测=" + std::to_string(damage) + "）");
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) return 1;
    sbs_srand(8833);

    // A：当前库四载体全链切片（加载器实切）。
    {
        const std::vector<std::pair<int, std::vector<std::pair<int, std::vector<int>>>>> want = {
            {36628, {{533, {}}, {2010, {60, 6}}, {1720, {60}}, {1828, {60}}, {1933, {100, 32}}}},
            {37776, {{1487, {}}, {2010, {200, 1}}}},
            {37855, {{2010, {100, 1}}, {1678, {50}}, {1232, {}}}},
            {38190, {{781, {2}}, {2010, {250, 1}}, {1578, {250, 1}}, {1579, {250, 1}},
                     {422, {75}}}},
        };
        auto& repo = official_data::OfficialDataStore::instance().repository();
        for (const auto& [move, chain] : want) {
            const auto got = repo.load_skill_effects(move);
            CHECK_EQ(got.size(), chain.size(), "A 载体链长 " + std::to_string(move));
            for (std::size_t i = 0; i < chain.size(); ++i) {
                CHECK_EQ(got[i].effect_id, chain[i].first, "A 效果序");
                CHECK(got[i].args == chain[i].second, "A 全链参数无错切");
            }
        }
    }

    // B：零护盾基线——count=0 整句不适用（不改写不重算），红伤 ×1 带。
    {
        auto b = solo();
        turn(*b, 0, -1);
        CHECK_EQ(observed_power[0], 150, "B 视图原值");
        CHECK(!observed_rewritten[0], "B 无改写");
        check_band(*b, 0, 150, 1, "B ×1");
        CHECK_EQ(audit(b->ctx(), 0, 2), 0, "B 护盾快照=0");
        CHECK_EQ(audit(b->ctx(), 0, 3), 0, "B count=0");
        CHECK_EQ(audit(b->ctx(), 0, 9), 3, "B skip=count0");
        CHECK_EQ(audit(b->ctx(), 0, 0), 200, "B {0} 行参回显");
        CHECK_EQ(audit(b->ctx(), 0, 1), 1, "B {1} 行参回显");
    }

    // C：档位线性阶梯（37776 真实行参 [200,1]）——199 不计档、200/400/600 = ×2/×3/×4；
    //    视图值精确（150×(100+count×100)/100），rewritten 只在 count>0 置位。
    {
        auto b = solo(kMove200, -1, -1, 0, 199);
        turn(*b, 0, -1);
        CHECK_EQ(observed_power[0], 150, "C 199 不足一档");
        CHECK(!observed_rewritten[0], "C 199 无改写");
        check_band(*b, 0, 150, 1, "C 199 ×1");
        CHECK_EQ(audit(b->ctx(), 0, 3), 0, "C 199 count=0");
        auto b1 = solo(kMove200, -1, -1, 0, 200);
        turn(*b1, 0, -1);
        CHECK_EQ(observed_power[0], 300, "C 200 档视图=300");
        CHECK(observed_rewritten[0], "C 200 rewritten");
        check_band(*b1, 0, 150, 2, "C 200 ×2");
        CHECK_EQ(audit(b1->ctx(), 0, 2), 200, "C 200 快照");
        CHECK_EQ(audit(b1->ctx(), 0, 3), 1, "C 200 count=1");
        CHECK_EQ(audit(b1->ctx(), 0, 4), 100, "C 200 pct=100");
        auto b2 = solo(kMove200, -1, -1, 0, 400);
        turn(*b2, 0, -1);
        CHECK_EQ(observed_power[0], 450, "C 400 档视图=450");
        check_band(*b2, 0, 150, 3, "C 400 ×3");
        CHECK_EQ(audit(b2->ctx(), 0, 3), 2, "C 400 count=2");
        auto b3 = solo(kMove200, -1, -1, 0, 600);
        turn(*b3, 0, -1);
        CHECK_EQ(observed_power[0], 600, "C 600 档视图=600");
        check_band(*b3, 0, 150, 4, "C 600 ×4");
        CHECK_EQ(audit(b3->ctx(), 0, 3), 3, "C 600 count=3");
    }

    // D：第二真实载体风尘沙爆 [100,1]（2010 居链首，1678/1232 后继不写威力视图）。
    {
        auto b = solo(kMove100, -1, -1, 0, 350);
        turn(*b, 0, -1);
        CHECK_EQ(observed_power[0], 320, "D 350/100=3档 视图=80×4");
        CHECK(observed_rewritten[0], "D rewritten");
        check_band(*b, 0, 80, 4, "D ×4");
    }

    // E：链序快照（36628 全活链 [533,2010,1720,1828,1933]）——倍乘按清盾前盘面：
    //    120 护盾/60 档 → count=2 → ×7 视图=78；1828 随后清盾（链内后继）不反噬本档。
    {
        auto b = solo(kMoveChain, -1, -1, 0, 120);
        turn(*b, 0, -1);
        CHECK_EQ(observed_power[0], 78, "E 清盾前快照 ×7 视图=6×13");
        CHECK(observed_rewritten[0], "E rewritten");
        check_band(*b, 0, 6, 13, "E ×7 红伤带");
        CHECK_EQ(b->pet(1).shield_bank_.total(), 0, "E 1828 链内清盾生效");
        CHECK_EQ(audit(b->ctx(), 0, 3), 2, "E count=2（快照面）");
    }

    // F：{1}=0 ×1 改写档——rewritten 置位（改过就重算纪律）数值不变。
    {
        auto b = solo(kMove200, -1, 0, 0, 200);
        turn(*b, 0, -1);
        CHECK_EQ(observed_power[0], 150, "F ×1 视图不变");
        CHECK(observed_rewritten[0], "F rewritten 仍置位");
        check_band(*b, 0, 150, 1, "F ×1 带");
        CHECK_EQ(audit(b->ctx(), 0, 3), 1, "F count=1");
        CHECK_EQ(audit(b->ctx(), 0, 4), 0, "F pct=0");
    }

    // G：镜像——双侧互有护盾、同回合互攻，各自按对手盘面独立取档。
    {
        auto b = solo(kMove200, -1, -1, 400, 200);
        turn(*b, 0, 0);
        CHECK_EQ(observed_power[0], 300, "G side0 读 side1 护盾 200 → 1档 ×2");
        CHECK_EQ(observed_power[1], 450, "G side1 读 side0 护盾 400 → 2档 ×3");
        check_band(*b, 0, 150, 2, "G side0 ×2");
        check_band(*b, 1, 150, 3, "G side1 ×3");
    }

    // H：跨回合无残留——倍乘次回合换普通技能，视图/红伤回常（ws 每回合 reset）。
    {
        auto b = solo(kMove200, -1, -1, 0, 800);
        turn(*b, 0, -1);
        check_band(*b, 0, 150, 5, "H R1 800/200=4档 ×5");
        turn(*b, 1, -1);
        CHECK_EQ(observed_power[0], 40, "H R2 拍打视图原值");
        CHECK(!observed_rewritten[0], "H R2 无残留改写");
        check_band(*b, 0, 40, 1, "H R2 ×1");
    }

    // I：离场——对手换宠后护盾盘随宠走，倍乘按新宠盘面重估（无旧宠残留）。
    {
        auto b = solo(kMove200, -1, -1, 0, 200);
        turn(*b, 0, -1);
        CHECK_EQ(audit(b->ctx(), 0, 3), 1, "I R1 1档");
        b->submit_operation(1, Action::CHOOSE_PET, 1);
        b->submit_operation(0, Action::NONE, 0);
        b->drive();
        CHECK_EQ(b->ctx().on_stage[1], 1, "I 对手已换宠");
        watch_power(*b);
        turn(*b, 0, -1);
        CHECK_EQ(observed_power[0], 150, "I 新宠无盾 ×1");
        CHECK(!observed_rewritten[0], "I 新宠无改写");
    }

    // J：miss / 属性技 / 非攻击技——不适用不改写。
    {
        auto miss = solo(kMove200, -1, -1, 0, 200);
        miss->pet(0).skills[0].must_hit = 0;
        miss->pet(0).skills[0].accuracy = 0;
        turn(*miss, 0, -1);
        CHECK_EQ(miss->ctx().ws.raw_attack_damage[0], 0, "J miss 无红伤");
        CHECK(!observed_rewritten[0], "J miss 不改写");
        CHECK_EQ(audit(miss->ctx(), 0, 9), -1, "J miss 未执行（审计无痕）");
        auto attr = solo(kMove200, -1, -1, 0, 200);
        attr->pet(0).skills[0].type = SkillType::Attribute;
        const int hp = attr->pet(1).hp;
        turn(*attr, 0, -1);
        CHECK_EQ(attr->ctx().ws.skill_power_view[0].value, 150, "J 属性视图不改");
        CHECK(!attr->ctx().ws.skill_power_view[0].rewritten, "J 属性无改写");
        CHECK_EQ(attr->pet(1).hp, hp, "J 属性无红伤");
        CHECK_EQ(audit(attr->ctx(), 0, 9), 1, "J 属性技 skip=非攻击");
    }

    std::cout << "scenario_8831_k495_2010 PASS\n";
    return 0;
}
