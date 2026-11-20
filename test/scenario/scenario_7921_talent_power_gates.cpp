// J330 / 7921：moves1470 的MISS、普通盔、命中失效、禁用、未注册、强制执行、
// 双方先后手、boss与soul同号隔离。负例用绝对0/140和rewritten=false，不比两局伤害。
// 〔源: docs_local/docs/06-交接/场景断言口径坑集-2026-10-21.md:169-194〕
// 只测is_boss_challenge单独开启，不模拟/修复R7的boss禁用预填。
// soul1470使用K399真实程序的满体100%封属档，只作命名空间隔离，不改其实现。
// 〔源: resources/soul_lib/k399_hp_gate_output_family.cpp:18-24〕
// 〔源: resources/soul_lib/k399_hp_gate_output_family.cpp:758-765〕

#include <iostream>

#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPlain = 10004;
constexpr int kBlind = static_cast<int>(AbnormalStatusId::Blind);

ElfPet pet(int skill_id, int speed = 300, bool disabled = false, SoulMark mark = {}) {
    auto p = make_pet(1, "1470门", {skill_id, kPlain, kPlain, kPlain, kPlain},
                      speed, /*atk=*/300, /*hp=*/1000000, std::move(mark));
    if (disabled) {
        DisabledEffects de;
        de.skill_effects = {1470};
        // 使用真实Skills禁用构造路径与真实学习关联，而非测试手动擦节点。
        const auto monster = official_data::OfficialDataStore::instance().repository()
                                 .load_monster(skill_id == 35431 ? 4286 : 4060);
        if (!monster) throw std::runtime_error("1470纯链学习者数据缺失");
        p.skills[0] = Skills(skill_id, *monster, {}, de);
        p.skills[0].critical_strike_rate = 0.0f;
    }
    for (auto& s : p.skills) {
        s.must_hit = true;
        s.accuracy = 100;
    }
    return p;
}

int nodes1470(const Skills& skill) {
    int count = 0;
    for (const auto& [branch, entries] : skill.effectBranches) {
        (void)branch;
        for (const auto& node : entries) count += node.effect.id == 1470;
    }
    return count;
}

void submit(ScenarioBattle& b, bool both = false) {
    b.ctx().debug_step_mode = true;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, both ? BattleFsm::ActionType::SELECT_SKILL
                             : BattleFsm::ActionType::NONE, both ? 0 : -1);
    b.ctx().debug_step_mode = false;
}

bool untouched(const BattleContext& ctx, int base) {
    return ctx.ws.skill_power_view[0].value == base
        && !ctx.ws.skill_power_view[0].rewritten;
}

void armor(BattleContext& ctx) {
    seal_skill(&ctx, /*source=*/1, /*target=*/0, /*effect_id=*/570,
               /*attribute=*/false, /*attack=*/true, /*count=*/1);
}

// 只在本测试进程短暂令moves工厂返回空logic；恢复后再运行已装配的技能。
// 不删除dylib、不动全局配置/DB、也不把无操作handler冒充“未注册”。
struct MissingMoves1470 {
    EffectFn saved = EffectFactory::getInstance().getEffect(1470).logic;
    MissingMoves1470() { EffectFactory::getInstance().registerEffect(1470, nullptr); }
    ~MissingMoves1470() { EffectFactory::getInstance().registerEffect(1470, saved); }
};

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data\n";
        return 1;
    }
    sbs_srand(7921);

    for (const int id : {35431, 34256}) {
        const int base = id == 35431 ? 0 : 140;
        ScenarioBattle b(make_party(pet(id, 300, true)), make_party(pet(kPlain, 10)));
        auto& ctx = b.ctx();
        b.start();
        submit(b);
        b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);
        const auto& skill = ctx.getPet(0).skills[0];
        CHECK(skill.rawEffectRecords.empty() && nodes1470(skill) == 0
                  && ctx.ws.skill_exec_result[0] == SkillExecResult::HIT
                  && untouched(ctx, base) && skill.power == base,
              "禁用1470摘掉纯链节点，HIT仍保留绝对基值且未改写：" + std::to_string(id));
    }

    REQUIRE(EffectFactory::getInstance().getEffect(1470).logic != nullptr);
    for (const int id : {35431, 34256}) {
        const int base = id == 35431 ? 0 : 140;
        auto absent = [&] {
            MissingMoves1470 missing;
            return pet(id);
        }();
        ScenarioBattle b(make_party(absent), make_party(pet(kPlain, 10)));
        auto& ctx = b.ctx();
        b.start();
        submit(b);
        b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);
        const auto& skill = ctx.getPet(0).skills[0];
        CHECK(skill.rawEffectRecords.size() == 1
                  && skill.rawEffectRecords[0].effect_id == 1470
                  && nodes1470(skill) == 0 && untouched(ctx, base)
                  && ctx.ws.skill_exec_result[0] == SkillExecResult::HIT
                  && EffectFactory::getInstance().getEffect(1470).logic != nullptr,
              "未注册装配仍跳过1470，工厂已恢复也不偷偷改基值：" + std::to_string(id));
    }

    // 非必中+真实失明=确定MISS；不使用概率抽样或只把精度设0的近似。
    {
        auto p = pet(34256);
        p.skills[0].must_hit = false;
        ScenarioBattle b(make_party(p), make_party(pet(kPlain, 10)));
        auto& ctx = b.ctx();
        b.start();
        apply_anomaly(&ctx, 0, kBlind, 3, 1);
        submit(b);
        b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);
        CHECK(ctx.has_active_abnormal_status(0, kBlind)
                  && ctx.ws.skill_exec_result[0] == SkillExecResult::SKILL_INVALID
                  && untouched(ctx, 140),
              "确定MISS不进入HIT改写，仍为140/未改写");
    }
    {
        ScenarioBattle b(make_party(pet(34256)), make_party(pet(kPlain, 10)));
        auto& ctx = b.ctx();
        b.start();
        armor(ctx);
        submit(b);
        b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);
        CHECK(ctx.ws.skill_exec_result[0] == SkillExecResult::SKILL_INVALID
                  && untouched(ctx, 140),
              "普通盔阻断HIT，1470不能隔盔自行改写");
    }
    {
        ScenarioBattle b(make_party(pet(34256)), make_party(pet(kPlain, 10)));
        auto& ctx = b.ctx();
        b.start();
        // ③层票挂防御方1，由攻击方0出手时消费（不是挂在被封攻击者0）。
        hit_effect_invalid(&ctx, 1, HitInvalidMode::kEffectsOnly, 1,
                           /*is_attribute_skill=*/false, /*source_id=*/1);
        submit(b);
        b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);
        CHECK(ctx.ws.hit_invalid_detected[0]
                  && ctx.ws.skill_exec_result[0] == SkillExecResult::HIT
                  && untouched(ctx, 140),
              "命中但效果失效，逐节点门过滤1470，仍为140/未改写");
    }

    // 强制执行与普通盔单独对照：引擎置0不是效果改写，后续1470才声明变威力。
    {
        auto p = pet(34256);
        p.skills[0].penetration_flags.force_execute = true;
        ScenarioBattle b(make_party(p), make_party(pet(kPlain, 10)));
        auto& ctx = b.ctx();
        b.start();
        armor(ctx);
        submit(b);
        b.step_until(State::BATTLE_FIRST_SKILL_EFFECT);
        CHECK(ctx.ws.skill_exec_result[0] == SkillExecResult::HIT && untouched(ctx, 0),
              "强制执行打盔先物化为0，HIT且尚未声明变威力");
        b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);
        CHECK(ctx.ws.skill_power_view[0].value == 180 && ctx.ws.skill_power_view[0].rewritten,
              "强制执行的HIT趟照常将0覆盖180并申报重算");
    }

    for (const int id : {35431, 34256}) {
        ScenarioBattle b(make_party(pet(id)), make_party(pet(kPlain, 10)));
        auto& ctx = b.ctx();
        ctx.is_boss_challenge = true;
        b.start();
        submit(b);
        b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);
        CHECK(nodes1470(ctx.getPet(0).skills[0]) == 1
                  && ctx.ws.skill_exec_result[0] == SkillExecResult::HIT
                  && ctx.ws.skill_power_view[0].value == 180 && ctx.ws.skill_power_view[0].rewritten,
              "只开boss仍覆盖180，不摘moves1470：" + std::to_string(id));
    }

    // 两方各做先/后手；对手只用无效果的40威力拍打，写错方会立即暴露。
    for (const int owner : {0, 1}) {
        for (const bool second : {false, true}) {
            const auto caster = pet(34256, second ? 50 : 600);
            const auto other = pet(kPlain, 300);
            ScenarioBattle b(make_party(owner == 0 ? caster : other),
                             make_party(owner == 1 ? caster : other));
            auto& ctx = b.ctx();
            b.start();
            submit(b, true);
            b.step_past(second ? State::BATTLE_SECOND_SKILL_EFFECT
                               : State::BATTLE_FIRST_SKILL_EFFECT);
            CHECK(ctx.is_second_mover(owner) == second
                      && ctx.ws.skill_power_view[owner].value == 180
                      && ctx.ws.skill_power_view[owner].rewritten
                      && ctx.ws.skill_power_view[1 - owner].value == (second ? 40 : -1)
                      && !ctx.ws.skill_power_view[1 - owner].rewritten,
                  "使用者独立改写，另一方不串值：owner=" + std::to_string(owner)
                      + " second=" + std::to_string(second));
        }
    }

    for (const bool boss : {false, true}) {
        SoulMark mark(1470, "暗夜·诺伊尔顿", "K399 row1448",
                      EffectArgs(std::vector<int>{100, 1, 50, 50, 1, 50, 1}));
        mark.monster_id = 3883;
        // 禁用moves1470不能摘同号soul；只开boss则仍应关闭soul1470的原有节点。
        ScenarioBattle b(make_party(pet(34256, 300, true, mark)),
                         make_party(pet(20017, 10)));
        auto& ctx = b.ctx();
        ctx.is_boss_challenge = boss;
        b.start();
        submit(b, true);
        b.drive();
        CHECK(untouched(ctx, 140)
                  && ctx.ws.skill_exec_result[1] == (boss ? SkillExecResult::HIT
                                                        : SkillExecResult::SKILL_INVALID)
                  && ctx.ability_levels[1][4] == (boss ? 2 : 0),
              "禁moves不禁同号soul，soul满体封属/boss无效原行为保持：boss="
                  + std::to_string(boss) + " speed_level="
                  + std::to_string(ctx.ability_levels[1][4]));
    }

    std::cout << "scenario_7921 PASS (J330 门与空间隔离)\n";
    return 0;
}
