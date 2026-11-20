// 场景4450 —— 659持续态门、接线epoch、配平和伤害类型边界。
// 载体拍打10004：官方物理40、side_effect=[]，所有红伤都经真实技能FSM。
// 同回合重复伤害用有界 ON_SKILL_HIT -> ATTACK_DAMAGE 子流程重放探针：不是生产路径
// 的额外行动自动重用技能，也不宣称引擎已支持同方同回合多次额外行动/逐刀连击。
// 断言同时看真实HP差、AMP快照、原伤比例和收刀后其他来源保留量，不读“ws固定20”。
#include <utils/sbs_rand.h>

#include <cmath>
#include <iostream>
#include <vector>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kSlap = 10004;
constexpr int kMaxHp = 100000;
constexpr int kSpeedSlot = 4;

SoulMark make_mark() {
    SoulMark sm(659, "穆恩斯达", "自身处于能力提升状态,则自身伤害提升20%",
                EffectArgs(std::vector<int>{2, 20}));
    sm.monster_id = 2930;
    return sm;
}

ElfPet pet(bool marked = true, int speed = 300) {
    auto p = make_pet(1, "659测试载体", {kSlap, kSlap, kSlap, kSlap, kSlap},
                      speed, 300, kMaxHp, marked ? make_mark() : SoulMark{});
    for (auto& skill : p.skills) {
        skill.must_hit = 1;
        skill.critical_strike_rate = 0;
    }
    CHECK(p.skills[0].rawEffectRecords.empty(), "前提：拍打官方副作用为空，不混入自强/粉伤");
    return p;
}

void reach(ScenarioBattle& b, State target) {
    for (int steps = 0; steps < 100 && b.state() != target
         && b.state() != State::FINISHED; ++steps) {
        b.step();
    }
    CHECK(b.state() == target, "真实技能子流程必须在100步内到达目标");
}

void begin_round(ScenarioBattle& b, bool both_attack = false) {
    CHECK(b.state() == State::OPERATION_CHOOSE_SKILL_MEDICAMENT, "前提：正在等待本轮操作");
    auto& ctx = b.ctx();
    ctx.debug_step_mode = true;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, both_attack ? BattleFsm::ActionType::SELECT_SKILL
                                    : BattleFsm::ActionType::NONE,
                       both_attack ? 0 : -1);
    ctx.debug_step_mode = false;
    reach(b, State::BATTLE_FIRST_ATTACK_DAMAGE);
    CHECK(ctx.ws.skill_resolution_flags[0].allowAttackDamagePipeline,
          "前提：拍打成功执行，已过SKILL_EFFECT但本笔红伤尚未结算");
}

void replay_to_damage(ScenarioBattle& b) {
    auto& ctx = b.ctx();
    CHECK(ctx.roundChoice[0][1] == 0 && b.pet(0).skills[0].id == kSlap,
          "重放前提：仍是本轮实际选中的无副作用拍打");
    CHECK(ctx.ws.preemptive_right == PreemptiveRight::SEER_ROBOT_1,
          "重放前提：0方确为先手方");
    ctx.currentState = State::BATTLE_FIRST_ON_SKILL_HIT;
    reach(b, State::BATTLE_FIRST_ATTACK_DAMAGE);
    CHECK(ctx.ws.skill_resolution_flags[0].allowAttackDamagePipeline,
          "重放命中/效果物化仍须真实成功，不能裸deal_damage(NORMAL)");
}

struct Hit {
    int loss;
    DamageSnapshot damage;
};

Hit finish_hit(ScenarioBattle& b, int owner = 0, int retained_amp = 0) {
    const State after = owner == 0 ? State::BATTLE_FIRST_AFTER_ACTION
                                   : State::BATTLE_SECOND_AFTER_ACTION;
    const int hp_before = b.pet(1 - owner).hp;
    reach(b, after);
    Hit hit{hp_before - b.pet(1 - owner).hp, b.ctx().resolvedDamage};
    CHECK_EQ(hit.damage.attackerId, owner, "确实观测到指定持印方自己的伤害");
    CHECK_EQ(hit.loss, hit.damage.final, "实际HP差等于该笔技能最终红伤");
    CHECK_EQ(b.ctx().ws.damage_add_pct[owner], retained_amp,
             "DETECT撤回659本笔量，不覆盖/撤掉其他加算来源");
    return hit;
}

void expect_amp(const Hit& hit, int pct, const std::string& label) {
    CHECK(hit.loss > 0 && hit.damage.base > 0, label + "：前提是正原伤、正落血");
    CHECK_EQ(hit.damage.addPct, pct, label + "：本笔AMP量精确");
    const double ratio = static_cast<double>(hit.loss) / hit.damage.base;
    CHECK(std::abs(ratio - (1.0 + pct / 100.0)) < 0.015,
          label + "：实际红伤比例（实测 " + std::to_string(ratio) + "）");
}

void switch_to(ScenarioBattle& b, int slot) {
    CHECK(b.state() == State::OPERATION_CHOOSE_SKILL_MEDICAMENT, "换宠前提：选择期");
    b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, slot);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
    CHECK_EQ(b.ctx().on_stage[0], slot, "真实主动换宠已落地");
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data\n";
        return 1;
    }

    // T4450 回合内先无提升；用技效果已结束后才提升/清强，必须立即影响下一笔结算。
    {
        ScenarioBattle b(make_party(pet()), make_party(pet(false, 100)));
        b.start();
        auto& ctx = b.ctx();
        sbs_srand(4450);
        begin_round(b);
        const int round = ctx.roundCount;
        expect_amp(finish_hit(b), 0, "T4450a 无提升首笔");

        replay_to_damage(b);  // SKILL_EFFECT时门仍闭，随后才强化，旧用技快照也必须失败。
        CHECK(stat_change(&ctx, 0, kSpeedSlot, 1) == StatChangeResult::SUCCESS,
              "T4450b 前提：技能效果之后真实获得速度+1");
        CHECK_EQ(ctx.ability_levels[0][kSpeedSlot], 1, "T4450b 速度槽0->1");
        expect_amp(finish_hit(b), 20, "T4450b 回合内即时开门");

        replay_to_damage(b);  // SKILL_EFFECT时门开，伤害前消强，不能保留快照20。
        CHECK_EQ(clear_stat_boosts_as(&ctx, 0, 1), 1, "T4450c 真实消除唯一提升槽");
        CHECK_EQ(ctx.ability_levels[0][kSpeedSlot], 0, "T4450c 速度槽1->0");
        expect_amp(finish_hit(b), 0, "T4450c 清强化后即时关门");
        CHECK_EQ(ctx.roundCount, round, "T4450 三笔确实在同一回合内，未靠reset掩盖累积");

        // T4451 有界重复额外行动时点 + 真实技能子流程探针；生产额外行动不会自动重跑技能。
        REQUIRE(stat_change(&ctx, 0, kSpeedSlot, 1) == StatChangeResult::SUCCESS);
        for (int i = 0; i < 3; ++i) {
            ctx.currentState = State::BATTLE_FIRST_EXTRA_ACTION;
            ctx.declare_extra_action(0);
            ctx.declare_extra_action(0);  // 同次声明重复调用必须幂等
            const int hp_before_extra = b.pet(1).hp;
            b.step();
            CHECK_EQ(ctx.ws.extra_action_count[0], i + 1, "T4451 探针确实消费一份额外行动声明");
            CHECK_EQ(b.pet(1).hp, hp_before_extra, "T4451 额外行动空桶自身不凭空重跑技能红伤");
            replay_to_damage(b);
            expect_amp(finish_hit(b), 20, "T4451 连续真实结算仍仅20%第" + std::to_string(i));
            CHECK_EQ(ctx.roundCount, round, "T4451 重复结算间没有回合reset");
        }
        b.drive();
        begin_round(b);
        expect_amp(finish_hit(b), 20, "T4451 新回合reset后仍可用且无残账");
    }

    // T4452 BLOCK归零/原伤0均需配平；其他来源30不能被误回冲。
    {
        ScenarioBattle b(make_party(pet()), make_party(pet(false, 100)));
        b.start();
        auto& ctx = b.ctx();
        begin_round(b);
        REQUIRE(stat_change(&ctx, 0, kSpeedSlot, 1) == StatChangeResult::SUCCESS);
        ctx.ws.damage_add_pct[0] = 30;
        // DAMAGE免疫票可能在技能执行层就令原伤归零，不能拿它伪造“AMP后BLOCK”。
        // 本探针在真实技能的BLOCK相位仅拦一笔，专门验证已写+20后归零仍会撤回。
        bool blocked_once = false;
        ctx.register_damage_effect(
            DamagePhase::BLOCK, 1, DamageEffectCategory::BLOCK,
            [&blocked_once](BattleContext* c, int owner) {
                auto& d = c->resolvedDamage;
                if (owner == 1 && d.attackerId == 0 && d.isRed && !blocked_once) {
                    CHECK(d.base > 0 && d.final > 0 && d.addPct == 50,
                          "T4452 BLOCK探针前提：真实原伤已带20+30增幅");
                    d.final = 0;
                    blocked_once = true;
                }
            });
        const auto blocked = finish_hit(b, 0, 30);
        CHECK(blocked_once, "T4452 确实经过真实红伤的BLOCK相位");
        CHECK_EQ(blocked.loss, 0, "T4452 实际伤害被一次性BLOCK探针归零");
        CHECK(blocked.damage.base > 0, "T4452 前提：BLOCK前有正原伤，不是没攻击");
        replay_to_damage(b);
        expect_amp(finish_hit(b, 0, 30), 50, "T4452 挡伤票已消费，下一笔仍20+30");

        replay_to_damage(b);
        const double saved = ctx.ws.restraint_view[0];
        ctx.ws.restraint_view[0] = 0;  // 真实伤害公式的零克制视图，不是裸扣血假红伤
        const auto zero = finish_hit(b, 0, 30);
        CHECK_EQ(zero.damage.base, 0, "T4452 原伤确实为0");
        CHECK_EQ(zero.loss, 0, "T4452 零伤不多扣也不多撤20");
        ctx.ws.restraint_view[0] = saved;
        replay_to_damage(b);
        expect_amp(finish_hit(b, 0, 30), 50, "T4452 零伤之后再次正伤不留负账");
        b.drive();
        begin_round(b);
        expect_amp(finish_hit(b), 20, "T4452 跨回合30已reset，659无配平残账");
    }

    // T4453 early重复接线、下场清、换上无印不继承、再上场重新接线。
    {
        auto party = make_party(pet());
        party[1] = pet(false);
        ScenarioBattle b(party, make_party(pet(false, 100)));
        b.start();
        auto& ctx = b.ctx();
        const int old_epoch = ctx.pipeline_valid_id[0];
        for (int i = 0; i < 3; ++i) {
            b.pet(0).soulMark.retrigger_entrance(&ctx, 0, 0);
        }
        begin_round(b);
        REQUIRE(stat_change(&ctx, 0, kSpeedSlot, 1) == StatChangeResult::SUCCESS);
        expect_amp(finish_hit(b), 20, "T4453 重复early仍只接一对管线回调");
        b.drive();
        switch_to(b, 1);
        CHECK(ctx.pipeline_valid_id[0] != old_epoch, "T4453 换宠推进管线epoch");
        CHECK(ctx.seerRobot[0].elfPets[0].on_stage_storage.empty(), "T4453 离场宿主上场期私有槽已清");
        CHECK_EQ(ctx.ability_levels[0][kSpeedSlot], 0, "T4453 离场不继承提升状态");
        begin_round(b);
        REQUIRE(stat_change(&ctx, 0, kSpeedSlot, 1) == StatChangeResult::SUCCESS);
        expect_amp(finish_hit(b), 0, "T4453 无印替补有提升也不继承旧回调");
        b.drive();
        switch_to(b, 0);
        begin_round(b);
        expect_amp(finish_hit(b), 0, "T4453 原宿主再登场无提升先关门");
        replay_to_damage(b);
        REQUIRE(stat_change(&ctx, 0, kSpeedSlot, 1) == StatChangeResult::SUCCESS);
        expect_amp(finish_hit(b), 20, "T4453 再登场新epoch恢复可用且不双算");
    }

    // T4454 双方持相同印，仅看自己的强化；对手的20%不能串到自己。
    {
        ScenarioBattle b(make_party(pet()), make_party(pet(true, 100)));
        b.start();
        auto& ctx = b.ctx();
        REQUIRE(stat_change(&ctx, 0, kSpeedSlot, 1) == StatChangeResult::SUCCESS);
        begin_round(b, true);
        expect_amp(finish_hit(b), 20, "T4454a 0方有提升");
        reach(b, State::BATTLE_SECOND_ATTACK_DAMAGE);
        expect_amp(finish_hit(b, 1), 0, "T4454a 1方不借用0方提升");
        b.drive();
        CHECK_EQ(clear_stat_boosts_as(&ctx, 0, 1), 1, "T4454b 清掉0方唯一强化");
        REQUIRE(stat_change(&ctx, 1, kSpeedSlot, 1) == StatChangeResult::SUCCESS);
        begin_round(b, true);
        expect_amp(finish_hit(b), 0, "T4454b 0方已清强化");
        reach(b, State::BATTLE_SECOND_ATTACK_DAMAGE);
        expect_amp(finish_hit(b, 1), 20, "T4454b 1方独立生效");
    }

    // T4455 粉伤/属性伤害/真伤/无源伤害不消费既有通用红伤加算槽。
    {
        ScenarioBattle b(make_party(pet()), make_party(pet(false, 100)));
        b.start();
        auto& ctx = b.ctx();
        REQUIRE(stat_change(&ctx, 0, kSpeedSlot, 1) == StatChangeResult::SUCCESS);
        ctx.ws.damage_add_pct[0] = 30;
        for (const auto kind : {DamageKind::FIXED, DamageKind::PERCENT_VALUE}) {
            const int hp = b.pet(1).hp;
            REQUIRE(deal_pink_damage(&ctx, 1, 1000, kind, 0) == FixedDamageResult::SUCCESS);
            CHECK_EQ(hp - b.pet(1).hp, 1000, "T4455 固定/百分比粉伤不误加659或红伤槽30%");
            CHECK_EQ(ctx.ws.damage_add_pct[0], 30, "T4455 粉伤不误回冲红伤临时槽");
        }
        int hp = b.pet(1).hp;
        deal_damage(&ctx, 1, 1000, DamageKind::TRUE_DMG, 0);
        CHECK_EQ(hp - b.pet(1).hp, 1000, "T4455 真伤不误增幅");
        const int element[2] = {0, 0};
        hp = b.pet(1).hp;
        REQUIRE(deal_attribute_damage(&ctx, 1, 1000, element, 0) == FixedDamageResult::SUCCESS);
        CHECK_EQ(hp - b.pet(1).hp, 1000, "T4455 属性直伤只跑CAP/BLOCK，不消费AMP");
        hp = b.pet(1).hp;
        REQUIRE(deal_pink_damage(&ctx, 1, 1000, DamageKind::FIXED, -1) == FixedDamageResult::SUCCESS);
        CHECK_EQ(hp - b.pet(1).hp, 1000, "T4455 无源环境伤害不借用持印方");
        CHECK_EQ(ctx.ws.damage_add_pct[0], 30, "T4455 非红伤通路不改加算槽");

        // 异常自身扣血走独立真伤通路：双方NONE也必须只扣max/8，不被659放大。
        REQUIRE(apply_anomaly(&ctx, 1, static_cast<int>(AbnormalStatusId::Burn), 3,
                              0, 100) == ApplyAnomalyResult::SUCCESS);
        hp = b.pet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(hp - b.pet(1).hp, kMaxHp / 8, "T4455 烧伤原生扣血无增幅/配平污染");
        CHECK_EQ(ctx.ws.damage_add_pct[0], 0, "T4455 新回合reset后没有其他通路留下的账");
    }

    std::cout << "scenario_4450 PASS (659实时门/配平/生命周期/类型边界)\n";
    return 0;
}
