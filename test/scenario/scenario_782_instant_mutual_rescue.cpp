// 场景 782 —— 死亡模型（2026-09-30 用户定稿）：瞬杀×机盖 + 内核同归保护
//
// 机制口径（docs_local/docs/06-交接/魂印与技能机制-交接.md 死亡模型两节）：
//   · 三通道：伤害（红/粉/真，走 deal_damage/run_pink_damage，落血≤0 发**预死亡信号**
//     EVENT_HP_EXHAUSTED 并计数同归标志）/ 秒杀（force_hp_to_zero，自带 EVENT_HP_TO_ZERO、
//     查免疫秒杀票）/ 体力修正（自爆 HP_CONSUME、斩杀印记——无信号、免死不可见）。
//   · 机盖（圣甲·盖亚 3056 魂印 736）：死亡拦截器里弹"对手造成的攻击伤害"等量真伤 +
//     heal(1) 自救（**受封回血影响**——与内核同归保护完全不同）。
//     瞬杀击杀走漏斗 DAMAGE 通道 → 机盖照常弹（弹量=本回合落榜的红伤量）。
//   · 内核同归保护（funnel_on_stage_deaths 入口）：进入死亡时点双方在场都 ≤0 →
//     **先**把对手打到 ≤0 的一方直写保留 1 点体力（流程保护，不走 heal——封回血不适用）。
//     机盖拦截器内部同步子判定的同归（希拓式）由盖亚 path B 处理，不经此分支。
//
// 断言组：
//   A 瞬杀×机盖（用户场景一）：R1 红伤 D 落榜 → R2 瞬杀清 0 → 机盖弹 D 真伤弹死我方 →
//     机盖 heal(1) 保留成功 → 死亡时点我死机活（反同归未介入——自救已够）。
//   B 内核同归保护（非机盖干净同归）：反击风暴（21 族）受致命红伤反 d/4 粉伤 →
//     双方 ≤0 进死亡时点 → 同归标志=先手方 → 直写保留 1 血活下来，对方正常死亡。
//   C 希拓式（机盖自救被封回血 → path B）：红伤致死 + 反弹真伤同归 + 机盖 heal 被封 →
//     先造成致命伤的一方由 path B 拉起 1 血；内核规则不重复介入。
//
// 确定性纪律：crit=0（make_pet 默认）、技能必中强制；伤害有 ±1% 浮动 → 数值断言留带宽带。
#include <array>
#include <iostream>
#include <memory>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kGaiaId = 3056;
constexpr int kGaiaMark = 736;
constexpr int kGaoSu = 20017;     // 高速移动：属性技（无伤害噪声）
constexpr int kYanFan = 10037;    // 燕返：威力 60 必中（伤害标定基准：atk≈0.437·atk）
constexpr int kFanJi = 20070;     // 反击风暴：effect 21，args=[4,5,4] → 反 d/4 粉伤

void force_must_hit(ElfPet& pet) {
    for (Skills& s : pet.skills) {
        s.must_hit = 1;
    }
}

ElfPet gaia(int hp, int speed = 50) {
    ElfPet p = scenario::make_pet(kGaiaId, "圣甲·盖亚",
                                  {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                  speed, /*atk=*/100, hp, SoulMark(kGaiaMark, "狂", "机盖"));
    force_must_hit(p);
    return p;
}

ElfPet striker(const std::array<int, 5>& skills, int atk, int hp, int speed = 300) {
    ElfPet p = scenario::make_pet(1, "进攻桩", skills, speed, atk, hp);
    force_must_hit(p);
    return p;
}

// 注入的瞬杀（ON_SKILL_HIT、once）：force_hp_to_zero 打对方在场（测试桩，场景 023 注入同款）。
EffectResult injected_instant_kill(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    (void)force_hp_to_zero(ctx, 1 - owner, owner);
    return EffectResult::kOk;
}

void submit_skills(ScenarioBattle& b, int slot0, int slot1) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, slot0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, slot1);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ A 瞬杀×机盖：弹真伤 + heal(1) 自救（受封回血影响的那个保留语义）══════════
    {
        auto me = striker({kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                          /*atk=*/500, /*hp=*/100);   // 反弹 218 > 100 → 必被弹死
        scenario::ScenarioBattle battle(scenario::make_party(me),
                                        scenario::make_party(gaia(/*hp=*/1000)));
        BattleContext& ctx = battle.ctx();
        battle.start();

        // R1：先手红伤落榜（机盖扣血但存活，last_landed_hit 记下攻击伤害量）。
        submit_skills(battle, 0, 0);
        battle.drive();
        const int d = 1000 - ctx.getPet(1).hp;
        CHECK(d > 150 && d < 400, "A R1 红伤落榜且非致命");
        CHECK_EQ(ctx.getPet(1).hp, 1000 - d, "A R1 机盖扣血存活（未触发拦截）");

        // R2：注入瞬杀清 0 → 死亡漏斗（DAMAGE 通道）→ 机盖弹 D 真伤弹死我方 →
        //     heal(1) 保留成功。走到死亡时点：我死机活——自救已够，反同归不介入。
        {
            Effect e(99031, 0, 0, 0, EffectArgs(std::vector<int>{0}),
                     &injected_instant_kill);
            auto ce = std::make_unique<ContinuousEffect>(
                e, State::BATTLE_FIRST_ON_SKILL_HIT, /*owner=*/0, /*duration=*/-1,
                ctx.roundCount);
            ce->source_id_ = 99031;
            ce->once_ = true;
            ctx.registerEffect(State::BATTLE_FIRST_ON_SKILL_HIT, 0, std::move(ce),
                               EffectContainer::Skill);
        }
        submit_skills(battle, 1, 0);   // 我方高速移动 + 注入瞬杀
        battle.drive();
        CHECK_EQ(ctx.getPet(0).hp, 0, "A 我方被 200 级真伤弹死");
        CHECK(ctx.pet_death_notified[0][ctx.on_stage[0]], "A 我方死亡成立（反同归未救）");
        CHECK_EQ(ctx.getPet(1).hp, 1, "A 机盖 heal(1) 保留成功（受封回血影响的自救语义）");
        std::cout << "  [A] 瞬杀×机盖：弹等量真伤弹死先手方、机盖保留 1 血存活 ✓" << std::endl;
    }

    // ══ B 内核同归保护：干净同归（非机盖）→ 先造成致命伤的一方直写保留 1 血 ══════
    {
        // 反弹量 = 钳后的实际落血（300/4=75）→ 我方 50 血必被弹死成同归。
        auto me = striker({kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                          /*atk=*/1200, /*hp=*/50);
        auto mirror = scenario::make_pet(2, "反射桩", {kFanJi, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                         /*speed=*/50, /*atk=*/100, /*hp=*/300);
        force_must_hit(mirror);
        scenario::ScenarioBattle battle(scenario::make_party(me),
                                        scenario::make_party(mirror));
        BattleContext& ctx = battle.ctx();
        battle.start();

        // R1：反射桩用反击风暴武装受击 watcher（属性技无伤害，窗口 4-5 回合）。
        submit_skills(battle, 1, 0);
        battle.drive();

        // R2：先手致命红伤（同归标志=我方）→ 反 d/4 粉伤同归 → 死亡时点双方 ≤0 →
        //     内核规则直写我方 1 血（封回血无关），对方正常死亡。
        submit_skills(battle, 0, 0);
        battle.drive();
        CHECK_EQ(ctx.ws.first_to_exhaust_foe, 0, "B 同归标志=先造成致命伤的一方");
        CHECK_EQ(ctx.getPet(0).hp, 1, "B 同归保护：先手方直写保留 1 血");
        CHECK(!ctx.pet_death_notified[0][ctx.on_stage[0]], "B 我方未登记死亡（活）");
        CHECK_EQ(ctx.getPet(1).hp, 0, "B 对方正常死亡");
        std::cout << "  [B] 内核同归保护：干净同归先手方保留 1 血 ✓" << std::endl;
    }

    // ══ C 希拓式：机盖自救被封回血 → path B 拉起先造成致命伤的一方 ══════════════
    {
        auto me = striker({kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                          /*atk=*/2400, /*hp=*/100);   // 红伤 ≈1048 ≥ 1000 致命
        scenario::ScenarioBattle battle(scenario::make_party(me),
                                        scenario::make_party(gaia(/*hp=*/1000)));
        BattleContext& ctx = battle.ctx();
        battle.start();
        // 希拓魂印效果桩：给机盖挂封回血（HEAL_BLOCK 常驻票）——heal(1) 拉不起来。
        ctx.grant_immunity(/*owner=*/1, ImmunityType::HEAL_BLOCK, coverage_all(),
                           /*anomaly_mask=*/0, /*duration_rounds=*/0, /*handle=*/99001,
                           /*soul_immunity=*/false, EffectScope::TEAM, /*counts=*/0,
                           /*source_effect_id=*/99000);

        // R1：先手致命红伤打死机盖 → 机盖弹等量真伤同归 → 机盖 heal 被封拉不起来 →
        //     path B 反同归给我方拉 1（先造成致命伤者活）；内核同归保护不重复介入
        //     （死亡时点进入时只有机盖一侧 ≤0，我方还在）。
        submit_skills(battle, 0, 0);
        battle.drive();
        CHECK_EQ(ctx.getPet(0).hp, 1, "C path B：先造成致命伤的一方保留 1 血");
        CHECK(!ctx.pet_death_notified[0][ctx.on_stage[0]], "C 我方未登记死亡（活）");
        CHECK_EQ(ctx.getPet(1).hp, 0, "C 机盖死亡成立（封回血自救失败）");
        std::cout << "  [C] 希拓式：机盖自救被封 → path B 救先手方，内核规则不重复介入 ✓"
                  << std::endl;
    }

    std::cout << "scenario_782 PASS (死亡模型：瞬杀×机盖 / 内核同归保护 / 希拓式 path B)"
              << std::endl;
    return 0;
}
