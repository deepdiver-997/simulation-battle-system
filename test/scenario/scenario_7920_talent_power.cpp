// J330 / 7920：moves1470 满31特化，11载体真实装配+执行、覆盖、烧伤顺序、重算消费。
// 载体链逐条核对全部参数（尤其37964中间零参），不把其他未实现子句宣称为已覆盖。
// 〔源: docs_local/docs/05-任务清单/1470天赋威力调研-2026-11-05.md:71-87〕
// 字段断言停泊SKILL_EFFECT；不以相同seed的伤害比例代替威力值/rewritten。
// 〔源: test/scenario/scenario_7550_cond_power_rewrite.cpp:81-96〕
// 〔源: docs_local/docs/06-交接/场景断言口径坑集-2026-10-21.md:169-194〕
// 烧伤先物化面板/2，1470再覆盖180；暴击重算在ATTACK_DAMAGE收尾消费标记。
// 〔源: src/fsm/battleFsm.cpp:498-513〕〔源: src/fsm/battleFsm.cpp:826-832〕
// 计数由scenario_harness自动输出SBS_SCENARIO_CHECKS，非手写通过数量。

#include <iostream>
#include <utility>
#include <vector>

#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPlain = 10004;  // 拍打，物理40、无效果
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);
using Chain = std::vector<std::pair<int, std::vector<int>>>;

struct Carrier {
    int id;
    SkillType type;
    int power;
    Chain chain;
};

ElfPet attacker(int id) {
    auto p = make_pet(1, "天赋威力", {id, kPlain, kPlain, kPlain, kPlain},
                      /*speed=*/300, /*atk=*/300, /*hp=*/1000000);
    for (auto& s : p.skills) {
        s.must_hit = true;
        s.accuracy = 100;
        // 官方部分五技PP为0：只补测试可用性，不修改效果链/零参数/基础威力。
        if (s.maxPP == 0) s.maxPP = s.pp = 5;
    }
    return p;
}

ElfPet dummy() {
    return make_pet(2, "木桩", {kPlain, kPlain, kPlain, kPlain, kPlain},
                    /*speed=*/10, /*atk=*/10, /*hp=*/1000000);
}

void submit_first(ScenarioBattle& b, int slot = 0) {
    auto& ctx = b.ctx();
    ctx.debug_step_mode = true;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, slot);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    ctx.debug_step_mode = false;
    b.step_until(State::BATTLE_FIRST_SKILL_EFFECT);
}

bool assembled(const Skills& skill, const Carrier& expected) {
    if (skill.power != expected.power || skill.type != expected.type
        || skill.rawEffectRecords.size() != expected.chain.size()) return false;
    for (std::size_t i = 0; i < expected.chain.size(); ++i) {
        const auto& record = skill.rawEffectRecords[i];
        if (record.effect_id != expected.chain[i].first
            || record.args != expected.chain[i].second) return false;
    }
    int nodes = 0;
    for (const auto& [branch, entries] : skill.effectBranches) {
        for (const auto& node : entries) {
            if (node.effect.id != 1470) continue;
            ++nodes;
            if (branch != SkillExecResult::HIT
                || node.registerState != State::BATTLE_FIRST_SKILL_EFFECT
                || node.effect.args.int_count != 2
                || !node.effect.logic
                || node.effect.logic != EffectFactory::getInstance().getEffect(1470).logic)
                return false;
        }
    }
    return nodes == 1;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data\n";
        return 1;
    }
    // 两个纯链先跑：180->160在第一条红；+=180在基础140的第二条红。
    const std::vector<Carrier> carriers{
        {35431, SkillType::Physical, 0, {{1470, {}}}},
        {34256, SkillType::Physical, 140, {{1470, {}}}},
        {13745, SkillType::Physical, 0, {{1470, {}}, {976, {1}}, {935, {28}}, {773, {}}}},
        {13746, SkillType::Special, 0, {{1470, {}}, {976, {1}}, {935, {28}}, {773, {}}}},
        {33310, SkillType::Special, 0, {{1470, {}}, {1199, {2}}, {1106, {300, 100, 8}}, {669, {}}}},
        {33463, SkillType::Physical, 0, {{1470, {}}, {781, {2}}, {1106, {300, 100, 19}}, {475, {300, 2}}}},
        {33653, SkillType::Special, 0, {{1470, {}}, {868, {2}}, {1258, {300, 1}}, {1082, {300, 30}}}},
        {33903, SkillType::Special, 0, {{1470, {}}, {1538, {}}, {868, {2}}, {696, {40, 20, 100}}}},
        {34376, SkillType::Physical, 0, {{1470, {}}, {178, {4, 2}}}},
        {34942, SkillType::Physical, 0, {{1470, {}}, {1060, {1}}, {731, {1}}, {1107, {50, 1, 50, 1}}}},
        {37964, SkillType::Special, 0, {{1897, {2, 200, 2, 100}}, {1470, {}}, {1538, {}}, {980, {}}, {1718, {100, 50}}}},
    };
    for (const auto& c : carriers) {
        sbs_srand(7920);
        ScenarioBattle b(make_party(attacker(c.id)), make_party(dummy()));
        auto& ctx = b.ctx();
        const auto& skill = ctx.getPet(0).skills[0];
        const bool chain_ok = assembled(skill, c);
        b.start();
        submit_first(b);
        if (c.id == 35431 || c.id == 34256) {
            CHECK(ctx.ws.skill_power_view[0].value == c.power
                      && !ctx.ws.skill_power_view[0].rewritten,
                  "纯链改写前仅物化：id=" + std::to_string(c.id)
                      + " value=" + std::to_string(ctx.ws.skill_power_view[0].value));
        }
        b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);
        CHECK(chain_ok && ctx.ws.skill_exec_result[0] == SkillExecResult::HIT
                  && ctx.ws.skill_power_view[0].value == 180
                  && ctx.ws.skill_power_view[0].rewritten
                  && skill.power == c.power
                  && ctx.ws.skill_type_view[0] == static_cast<int>(c.type)
                  && ctx.ws.skill_element_view[0][0] == skill.element[0]
                  && ctx.ws.skill_element_view[0][1] == skill.element[1],
              "载体完整切片/HIT节点/覆盖180/rewritten/物特系别不变：id="
                  + std::to_string(c.id) + " chain=" + std::to_string(chain_ok)
                  + " value=" + std::to_string(ctx.ws.skill_power_view[0].value)
                  + " rewritten=" + std::to_string(ctx.ws.skill_power_view[0].rewritten));
    }

    // 实际烧伤物化140->70，随后覆盖180；不是70+180或180/2。
    {
        ScenarioBattle b(make_party(attacker(34256)), make_party(dummy()));
        auto& ctx = b.ctx();
        b.start();
        apply_anomaly(&ctx, 0, kBurn, 3, 1);
        submit_first(b);
        CHECK(ctx.has_active_abnormal_status(0, kBurn)
                  && ctx.ws.skill_power_view[0].value == 70
                  && !ctx.ws.skill_power_view[0].rewritten,
              "烧伤前提成立：面板140先物化成70，不声明变威力");
        b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);
        CHECK(ctx.ws.skill_power_view[0].value == 180
                  && ctx.ws.skill_power_view[0].rewritten
                  && ctx.getPet(0).skills[0].power == 140,
              "烧伤后HIT覆盖180，未修改面板140");
    }

    // 真FSM必暴：正防御等级清零、重算消费rewritten；下一技能重新物化自己的40。
    {
        auto p = attacker(34256);
        set_crit_sixteenths(p, 16);
        ScenarioBattle b(make_party(p), make_party(dummy()));
        auto& ctx = b.ctx();
        b.start();
        ctx.ability_levels[1][2] = 6;
        submit_first(b);
        b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);
        const bool requested = ctx.ws.skill_power_view[0].rewritten;
        b.step_past(State::BATTLE_FIRST_ATTACK_DAMAGE);
        CHECK(requested && ctx.crit_happened[0] && ctx.ability_levels[1][2] == 0
                  && ctx.ws.skill_power_view[0].value == 180
                  && !ctx.ws.skill_power_view[0].rewritten
                  && ctx.ws.raw_attack_damage[0] > 0,
              "必暴破防后走变威力重算并消费标记（不比较随机伤害比例）");
        b.drive();
        submit_first(b, 1);
        b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);
        CHECK(ctx.ws.skill_power_view[0].value == 40
                  && !ctx.ws.skill_power_view[0].rewritten
                  && ctx.getPet(0).skills[0].power == 140,
              "下一次普通技能恢复40，不泄漏180或rewritten，不改1470载体基值");
    }

    std::cout << "scenario_7920 PASS (J330 满31天赋威力)\n";
    return 0;
}
