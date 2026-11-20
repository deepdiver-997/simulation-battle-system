// 场景 048：选择期"微弱→先制"一族的**两条判据系别**（785 判精灵 / 2030 判技能）。
//
// 官方模板（全 args_num=0 → 先制数是文案常量）：
//   785  「若**自身**攻击对手时克制关系为微弱则先制+2」
//   2030 「若**本次技能**微弱对手则先制+1」
//   2490 「此技能微弱对手时技能先制+3、获得本系属性加成并以神灵系进行伤害结算」（场景 047 已覆盖）
//
// ★ 用户 2026-09-14 口径：**785 的"自身"指精灵系别**；2030/2490 的"本次技能/此技能"指技能系别。
//   落地：
//     · 判**技能**系别 → 读**技能槽位的静态系别**（core 追加进 args[2]/args[3] 喂给插件；
//       不读 `ws.skill_element_view`，否则会被"以XX系别进行伤害结算"自污染）
//     · 判**精灵**系别 → 读 `ws.view_elementalAttributes[self]`（精灵系别**视图**，
//       这样"属性反转"这类改系别效果会被计入）
//   本场景的关键设计：**故意让两条判据给出相反结论**，从而把"读错了哪个系别"直接暴露出来——
//     · 785 用例：技能系别**不微弱**、精灵系别**微弱** → 只有读精灵系别才会触发
//     · 2030 用例：技能系别**微弱**、精灵系别**不微弱** → 只有读技能系别才会触发
//
// 验证点：
//   S1 785 触发（精灵系别微弱）：先制 = 基础 0 + **2**，且先手翻盘
//   S2 785 不触发（精灵系别换成不微弱的）：先制 = 0
//   S3 2030 触发（技能系别微弱）：先制 = 0 + **1**，且先手翻盘
//   S4 2030 不触发（守方换成技能系别不微弱的）：先制 = 0
#include <cassert>
#include <iostream>

#include <effects/effect.h>
#include <entities/pet_factory.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

constexpr int kGaoSu = 20017;     // 高速移动：属性技能（不还手）

// 785 用例：逆命龙魂（普通系 8，side_effect=[43,55,785]，priority 0）
//   普通 → 草(1) = 1.0（**不微弱**）；水(2) → 草(1) = 0.5（**微弱**）
// ⚠️ 实测：同系相打**不一定是 0.5**（grass|grass=0.5、steel|steel=0.5、但 dark|dark=**2.0**、
//    normal|normal=1.0）—— 挑对照系别时必须查表，别想当然。
constexpr int kNilMing = 24163;
constexpr int kGrass = 1;
constexpr int kWater = 2;
constexpr int kNormal = 8;    // 普通→草 = 1.0（刚好"不微弱"的对照）

// 2030 用例：影月散华（暗影系 13，side_effect=[2030,1685,1832]，priority 0）
//   暗影 → 机械(6) = 0.5（**微弱**）；水(2) → 机械(6) = 1.0（**不微弱**，用来验"读的不是精灵系别"）
constexpr int kYingYue = 37768;
constexpr int kSteel = 6;
constexpr int kShadow = 13;

// 探针：读攻方的先制等级（MOVE_RIGHT 之后、回合重置之前）。
// 攻方可能是先手也可能是后手 → FIRST/SECOND 两个时点都挂（同一个 owner 只会命中一次）。
int g_preemptive = -999;
EffectResult probe_preemptive(BattleContext* ctx, const EffectArgs&) {
    if (ctx) {
        g_preemptive = ctx->ws.preemptive_level[0];
    }
    return EffectResult::kOk;
}

void register_effect(BattleContext& ctx, int owner, State state, EffectFn fn, int effect_id) {
    Effect e(effect_id, /*priority=*/0, owner, /*left_round=*/1,
             EffectArgs(std::vector<int>{owner, 1 - owner}), fn);
    auto ce = std::make_unique<ContinuousEffect>(e, state, owner, /*duration=*/1, ctx.roundCount);
    ce->source_id_ = effect_id;
    ctx.register_skill_effect(state, owner, std::move(ce));
}

void arm_probe(BattleContext& ctx) {
    register_effect(ctx, 0, State::BATTLE_FIRST_SKILL_EFFECT, &probe_preemptive, 999049);
    register_effect(ctx, 0, State::BATTLE_SECOND_SKILL_EFFECT, &probe_preemptive, 999049);
}

// 攻方速度 100、守方 101 → 没有先制加成时守方先手；拿到加成后攻方翻盘。
scenario::ScenarioBattle make_battle(int attacker_skill, int attacker_elem, int defender_elem) {
    auto p0 = scenario::make_pet(0, "攻方", {attacker_skill, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                 /*speed=*/100, /*atk=*/200, /*hp=*/3000);
    auto p1 = scenario::make_pet(1, "守方", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                 /*speed=*/101, /*atk=*/100, /*hp=*/3000);
    p0.skills[0].accuracy = 100;
    p0.skills[0].must_hit = true;
    p0.elementalAttributes[0] = attacker_elem;   // 精灵系别（785 的判据）
    p1.elementalAttributes[0] = defender_elem;
    return scenario::ScenarioBattle(scenario::make_party(p0), scenario::make_party(p1));
}

// 跑一回合，返回 (攻方先制等级, 攻方是否先手)。
struct RunResult { int preemptive; bool attacker_first; };
RunResult run(scenario::ScenarioBattle& b) {
    b.start();
    arm_probe(b.ctx());
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
    b.drive();
    RunResult r;
    r.preemptive = g_preemptive;
    r.attacker_first = (b.ctx().preemptive_right == PreemptiveRight::SEER_ROBOT_1);
    return r;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== S1/S2 785：判**精灵**系别 ==========
    {
        // S1：精灵系别 = 水 → 水打草微弱 → 触发
        g_preemptive = -999;
        scenario::ScenarioBattle b1 = make_battle(kNilMing, /*atk_elem=*/kWater, kGrass);
        const RunResult r1 = run(b1);

        // S2 对照：精灵系别 = 普通 → 普通打草 1.0（**不微弱**）→ 不触发。
        // 注意技能系别（普通→草 = 1.0）在两种情况下都**不微弱**，所以若 785 错读技能系别，
        // S1 也不会触发 —— S1/S2 这对就是"读的是精灵系别还是技能系别"的判别器。
        g_preemptive = -999;
        scenario::ScenarioBattle b2 = make_battle(kNilMing, /*atk_elem=*/kNormal, kGrass);
        const RunResult r2 = run(b2);

        CHECK_EQ(r1.preemptive, 2, "S1) ★ 精灵系别微弱 → 先制 +2");
        CHECK(r1.attacker_first, "S1) +2 让速度更慢的攻方抢到先手");
        CHECK_EQ(r2.preemptive, 0, "S2) 精灵系别不微弱 → 不触发（先制保持基础值 0）");
        CHECK(!r2.attacker_first, "S2) 不触发 → 先手归速度更快的守方");
        std::cout << "  [S1] 785 触发(精灵系别水打草 0.5): 先制=" << r1.preemptive
                  << "、先手=攻方\n";
        std::cout << "  [S2] 785 不触发(精灵系别普通打草 1.0): 先制=" << r2.preemptive
                  << "、先手=守方（技能系别普通打草也是 1.0 → 错读技能系别则 S1 同样不触发）\n";
    }

    //========== S3/S4 2030：判**技能**系别 ==========
    {
        // S3：守方机械 → 技能系别 暗影打机械 = 0.5 微弱 → 触发
        g_preemptive = -999;
        scenario::ScenarioBattle b1 = make_battle(kYingYue, /*atk_elem=*/kWater, kSteel);
        const RunResult r1 = run(b1);

        // S4 对照：守方换成暗影 → 技能系别 暗影打暗影 = **2.0**（不是 0.5！）→ 不触发。
        // S3 那组里精灵系别是水（水打机械 1.0），所以若 2030 错读精灵系别，S3 也不会触发
        // —— S3/S4 这对就是"读的是技能系别还是精灵系别"的判别器。
        g_preemptive = -999;
        scenario::ScenarioBattle b2 = make_battle(kYingYue, /*atk_elem=*/kWater, /*def=*/kShadow);
        const RunResult r2 = run(b2);

        CHECK_EQ(r1.preemptive, 1, "S3) ★ 技能系别微弱 → 先制 +1");
        CHECK(r1.attacker_first, "S3) +1 让速度更慢的攻方抢到先手");
        CHECK_EQ(r2.preemptive, 0, "S4) 技能系别不微弱（暗影打暗影 2.0）→ 不触发");
        CHECK(!r2.attacker_first, "S4) 不触发 → 先手归守方");
        std::cout << "  [S3] 2030 触发(技能系别暗影打机械 0.5): 先制=" << r1.preemptive
                  << "、先手=攻方（精灵系别是水、水打机械 1.0 → 若错读精灵系别则不触发）\n";
        std::cout << "  [S4] 2030 不触发(守方换暗影，暗影打暗影 2.0): 先制=" << r2.preemptive
                  << "、先手=守方\n";
    }

    std::cout << "\n场景 048 通过：785 判**精灵**系别（读精灵系别视图，改系别效果计入）、"
                 "2030/2490 判**技能**系别（读技能槽位静态系别，防\"以XX系别结算\"自污染）——"
                 "两条判据在本场景里被故意做成相反结论，读错哪一条都会露馅。\n";
    return 0;
}
