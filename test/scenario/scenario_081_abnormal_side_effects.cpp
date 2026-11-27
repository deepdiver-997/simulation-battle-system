#include <utils/sbs_rand.h>
// 场景 081：异常状态的**副作用**（官方 effect_des kind=2 逐条文本）
//
// 079 测的是异常系统"骨架"（分档扣血 / 限制切换 / 限制附加 / 自我衍化 / 技能无效）；
// 本场景测**挂在同一批异常上的行为修正**——"处于该状态则它的伤害/命中/先制/回复变成什么样"。
//
// 口径来源：数据库 `effect_des` 的 `kind=2`（`json_extract(raw_json,'$.icon')` = 引擎槽位）。
// 原文点要：
//   2  烧伤   「攻击技能的威力会减少50%」
//   8  睡眠   「对手使用攻击技能且未miss时睡眠解除」
//   10 混乱   「攻击技能的命中率减少80%」
//   11 衰弱   「若衰弱的回合数为1/2/3/4/5及以上，则受到的攻击伤害额外提升25%/50%/100%/250%/500%」
//   12 山神守护「每回合对手受到的攻击伤害减少80%」
//   13 易燃   「攻击技能命中率降低30%，被火系攻击技能命中时转化为烧伤」
//   14 狂暴   「造成的攻击伤害翻倍」
//   25 致命诅咒「受到的攻击伤害额外提升50%」
//   26 虚弱诅咒「造成的攻击伤害额外降低50%」
//   28 束缚   「先制效果失效」
//   31 臣服   「无法对对手造成攻击伤害、固定伤害、百分比伤害」
//   33 星赐   「造成的攻击伤害提升30%，每回合结束时恢复2点PP值」
//   34 星哲   「造成的固定伤害、百分比伤害提升30%，每回合结束时恢复最大体力的1/4」
//   35 超频   「技能先制+1且行动开始时恢复所选择技能的全部PP值」
//
// ⚠️ **星赎(38) / 砥砺(37) 不在本场景**：它们的 `effect_des.icon` 是 37/38，落在
//    `kOfficialAbnormalStatusMaxId = 35` 之外（36 槽表里没有）→ 要接先扩表。代码里已标注。
//
// 验证点：
//   G 伤害修正：G1 狂暴翻倍 / G2 虚弱诅咒-50% / G3 致命诅咒+50% / G4 山神守护对手-80% /
//              G5 星赐+30% / G6 星哲增粉（固定伤害档）/ G7 臣服归零
//   H 威力与命中：H1 烧伤威力减半 / H2 混乱命中率-80% / H3 易燃命中率-30%
//   I 先制：I1 束缚先制失效 / I2 超频先制+1 / I3 束缚下超频仍能给自己+1（"靠后绕过"）
//   J 命中时点：J1 睡眠被攻击技能命中解除 / J2 属性技能不解除 /
//              J3 易燃被火系攻击命中转烧伤 / J4 非火系不转化
//   K 回复与PP：K1 星赐回合结束+2PP / K2 星哲回合结束回1/4体力 / K3 超频行动开始回满所选技能PP
#include <array>
#include <cassert>
#include <cstdlib>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <effects/rule_center.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kGaoSu = 20017;   // 高速移动：属性技能、必中、威力 0（不干扰 hp 归因）
constexpr int kPaiDa = 10004;   // 拍打：物理、必中、威力 40
constexpr int kHuoYanWei = 12072;   // 火焰尾：**火系**物理、必中、威力 110
constexpr int kXianZhi = 20302;     // 蹦跳：属性、必中、**先制 +1**（测束缚/超频的先制门）
constexpr int kHp = 5000;

constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);
constexpr int kSleep = static_cast<int>(AbnormalStatusId::Sleep);
constexpr int kConfusion = static_cast<int>(AbnormalStatusId::Confusion);
constexpr int kWeakness = static_cast<int>(AbnormalStatusId::Weakness);
constexpr int kMountainGuardian = static_cast<int>(AbnormalStatusId::MountainGuardian);
constexpr int kFlammable = static_cast<int>(AbnormalStatusId::Flammable);
constexpr int kBerserk = static_cast<int>(AbnormalStatusId::Berserk);
constexpr int kDeathCurse = static_cast<int>(AbnormalStatusId::DeathCurse);
constexpr int kWeaknessCurse = static_cast<int>(AbnormalStatusId::WeaknessCurse);
constexpr int kBind = static_cast<int>(AbnormalStatusId::Bind);
constexpr int kSubmission = static_cast<int>(AbnormalStatusId::Submission);
constexpr int kStarBlessing = static_cast<int>(AbnormalStatusId::StarBlessing);
constexpr int kStarWisdom = static_cast<int>(AbnormalStatusId::StarWisdom);
constexpr int kOverclock = static_cast<int>(AbnormalStatusId::Overclock);
// ── 2026-09-18 扩表（icon 37~43）后新接入的 ──
constexpr int kStarRedemption = static_cast<int>(AbnormalStatusId::StarRedemption);   // 38
constexpr int kWandering = static_cast<int>(AbnormalStatusId::Wandering);             // 39
constexpr int kVoidFixation = static_cast<int>(AbnormalStatusId::VoidFixation);       // 40
constexpr int kThunderRelease = static_cast<int>(AbnormalStatusId::ThunderRelease);   // 42
constexpr int kSlowFreeze = static_cast<int>(AbnormalStatusId::SlowFreeze);           // 43
constexpr int kDistraction = static_cast<int>(AbnormalStatusId::Distraction);         // 29
constexpr int kSilence = static_cast<int>(AbnormalStatusId::Silence);                 // 30
constexpr int kStasis = static_cast<int>(AbnormalStatusId::Stasis);                   // 32

using SkillArr = std::array<int, 5>;
const SkillArr all_gaosu() { return {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu}; }
const SkillArr has_paida() { return {kPaiDa, kGaoSu, kGaoSu, kGaoSu, kGaoSu}; }
const SkillArr has_fire() { return {kHuoYanWei, kGaoSu, kGaoSu, kGaoSu, kGaoSu}; }
const SkillArr has_prio() { return {kXianZhi, kGaoSu, kGaoSu, kGaoSu, kGaoSu}; }

ElfPet attacker(const SkillArr& s) {
    return scenario::make_pet(0, "攻方", s, /*speed=*/200, /*atk=*/100, kHp);
}
ElfPet defender(const SkillArr& s) {
    return scenario::make_pet(1, "守方", s, /*speed=*/100, /*atk=*/100, kHp);
}
ScenarioBattle make_battle(const SkillArr& s0 = all_gaosu(), const SkillArr& s1 = all_gaosu()) {
    return ScenarioBattle({scenario::make_party(attacker(s0))}, {scenario::make_party(defender(s1))});
}
// ⚠️ 伤害公式里有 `(217 + sbs_rand()%39)/255` 的**浮动**（`calculation.h`）→ 不同测量点
//    之间 rand 序列位置不同，伤害不可直接比。这里每回合先播同一个种子，让所有测量
//    （基线 / 带异常）拿到**同一条 rand 序列** → 比值才有意义。
void run_round(ScenarioBattle& b, int s0 = 0, int s1 = 0) {
    b.ctx().seed_rng(20260918u);
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, s0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, s1);
    b.drive();
}

// 量一次"攻方(0) 用拍打打守方(1) 一回合"的伤害（守方用属性技能，不干扰）。
int one_round_attack_damage(int attacker_status = -1) {
    auto b = make_battle(has_paida(), all_gaosu());
    b.start();
    BattleContext& ctx = b.ctx();
    if (attacker_status >= 0) {
        apply_anomaly(&ctx, 0, attacker_status, /*duration_rounds=*/3, /*actor=*/1);
    }
    const int hp0 = ctx.getPet(1).hp;
    run_round(b);
    return hp0 - ctx.getPet(1).hp;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //==================== G 伤害修正 ====================
    const int base_dmg = one_round_attack_damage();
    CHECK(base_dmg > 0, "G 前置：基线攻击伤害应为正");

    // G1 狂暴(14)「造成的攻击伤害翻倍」
    {
        const int dmg = one_round_attack_damage(kBerserk);
        CHECK_EQ(dmg, base_dmg * 2, "G1 狂暴应令造成的攻击伤害翻倍");
        std::cout << "G1 PASS 狂暴翻倍（" << base_dmg << " → " << dmg << "）\n";
    }

    // G2 虚弱诅咒(26)「造成的攻击伤害额外降低50%」
    {
        const int dmg = one_round_attack_damage(kWeaknessCurse);
        CHECK_EQ(dmg, base_dmg * 50 / 100, "G2 虚弱诅咒应令造成的攻击伤害额外 -50%");
        std::cout << "G2 PASS 虚弱诅咒 -50%（" << base_dmg << " → " << dmg << "）\n";
    }

    // G3 致命诅咒(25)「受到的攻击伤害额外提升50%」——挂在**守方**
    {
        auto b = make_battle(has_paida(), all_gaosu());
        b.start();
        BattleContext& ctx = b.ctx();
        apply_anomaly(&ctx, 1, kDeathCurse, 3, 0);
        const int hp0 = ctx.getPet(1).hp;
        run_round(b);
        CHECK_EQ(hp0 - ctx.getPet(1).hp, base_dmg * 150 / 100,
                 "G3 致命诅咒应令受到的攻击伤害额外 +50%");
        std::cout << "G3 PASS 致命诅咒 +50%\n";
    }

    // G4 山神守护(12)「每回合**对手**受到的攻击伤害减少80%」——挂在攻方(0)，减的是守方(1)的伤
    {
        auto b = make_battle(has_paida(), all_gaosu());
        b.start();
        BattleContext& ctx = b.ctx();
        apply_anomaly(&ctx, 0, kMountainGuardian, 3, 1);
        const int hp0 = ctx.getPet(1).hp;
        run_round(b);
        CHECK_EQ(hp0 - ctx.getPet(1).hp, base_dmg * 20 / 100,
                 "G4 山神守护应令**对手**受到的攻击伤害 -80%（判定方是持有者的对面）");
        std::cout << "G4 PASS 山神守护令对手受伤 -80%\n";
    }

    // G5 星赐(33)「造成的攻击伤害提升30%」
    {
        const int dmg = one_round_attack_damage(kStarBlessing);
        CHECK_EQ(dmg, base_dmg * 130 / 100, "G5 星赐应令造成的攻击伤害 +30%");
        std::cout << "G5 PASS 星赐 +30%\n";
    }

    // G6 星哲(34)「造成的**固定伤害**提升30%」——用固定伤害档量（deal_pink_damage）
    {
        auto b = make_battle();
        b.start();
        BattleContext& ctx = b.ctx();
        auto pink = [&](int status) {
            auto b2 = make_battle();
            b2.start();
            BattleContext& c2 = b2.ctx();
            if (status >= 0) {
                apply_anomaly(&c2, 0, status, 3, 1);
            }
            const int hp0 = c2.getPet(1).hp;
            deal_pink_damage(&c2, 1, 1000, DamageKind::FIXED, /*actor=*/0);
            return hp0 - c2.getPet(1).hp;
        };
        const int plain = pink(-1);
        const int amped = pink(kStarWisdom);
        CHECK_EQ(plain, 1000, "G6 前置：无星哲时固定伤害应为原值");
        CHECK_EQ(amped, 1300, "G6 星哲应令造成的固定伤害 +30%");
        std::cout << "G6 PASS 星哲固定伤害 +30%（" << plain << " → " << amped << "）\n";
    }

    // G7 臣服(31)「无法对对手造成攻击伤害」——攻方有臣服 → 打不出红伤
    {
        const int dmg = one_round_attack_damage(kSubmission);
        CHECK_EQ(dmg, 0, "G7 臣服应令持有者无法对对手造成攻击伤害");
        std::cout << "G7 PASS 臣服令攻击伤害归零\n";
    }

    //==================== H 威力与命中 ====================
    // H1 烧伤(2)「攻击技能的**威力**会减少50%」
    // ⚠️ 减的是**威力**不是伤害，而伤害公式带一个**加法常数项**（`+2`）
    //    → 威力减半后伤害**不等于**伤害减半：base=30（威力 40）时半威力 ≈ (30-2)/2+2 = 16。
    //    所以这里只断言"落在半值附近"，不断言精确相等。
    {
        const int dmg = one_round_attack_damage(kBurn);
        CHECK(dmg > 0 && dmg < base_dmg, "H1 烧伤应显著降低攻击伤害");
        CHECK(dmg * 100 >= base_dmg * 45 && dmg * 100 <= base_dmg * 55,
              "H1 烧伤应令攻击伤害落在基线的一半附近（威力减半，非伤害减半）");
        std::cout << "H1 PASS 烧伤威力减半（伤害 " << base_dmg << " → " << dmg
                  << "，非严格一半：公式有加法常数项）\n";
    }

    // H2/H3 混乱(10) -80% / 易燃(13) -30% —— 命中率用统计（未播种 rand → 序列固定）
    {
        auto measure_hit_rate = [](int status) {
            auto b = make_battle(has_paida(), all_gaosu());
            b.start();
            BattleContext& ctx = b.ctx();
            if (status >= 0) {
                apply_anomaly(&ctx, 0, status, 3, 1);
            }
            Skills& sk = ctx.getPet(0).skills[0];   // 拍打：命中率 100
            ctx.roundChoice[0][0] = static_cast<int>(BattleFsm::ActionType::SELECT_SKILL);
            ctx.roundChoice[0][1] = 0;
            constexpr int kTrials = 2000;
            sbs_srand(20260918u);   // 同种子 → 各档的掷点序列可比
            int hits = 0;
            for (int i = 0; i < kTrials; ++i) {
                if (sk.query_usage(&ctx, 0) != SkillUsageResult::MISS) {
                    ++hits;
                }
            }
            return hits * 100 / kTrials;   // 命中率（百分数）
        };
        const int plain = measure_hit_rate(-1);
        const int confusion = measure_hit_rate(kConfusion);
        const int flammable = measure_hit_rate(kFlammable);
        CHECK_EQ(plain, 100, "H 前置：无异常时命中率应为 100%");
        CHECK(confusion <= 30, "H2 混乱应把攻击技能命中率压到 ~20%");
        CHECK(flammable >= 60 && flammable <= 80, "H3 易燃应把攻击技能命中率压到 ~70%");
        std::cout << "H2/H3 PASS 混乱命中率 " << confusion << "% / 易燃 " << flammable
                  << "%（基线 " << plain << "%）\n";
    }

    //==================== I 先制 ====================
    // I1 束缚(28)「先制效果失效」
    //     守方(速度 100)带**先制+1**技能，攻方(速度 200)不带：
    //       无束缚 → 守方先制 1 > 0 → 守方先手；有束缚 → 先制被清零 → 回到速度 → 攻方先手
    {
        auto no_bind = make_battle(all_gaosu(), has_prio());
        no_bind.start();
        apply_anomaly(&no_bind.ctx(), 1, kBurn, 3, 0);   // 无关异常做对照（不碰先制）
        run_round(no_bind, /*s0=*/0, /*s1=*/0);
        CHECK(no_bind.ctx().preemptive_right == PreemptiveRight::SEER_ROBOT_2,
              "I1 对照组：守方靠先制+1 应先手（确认先制技能真的注册了）");

        auto b = make_battle(all_gaosu(), has_prio());
        b.start();
        BattleContext& ctx = b.ctx();
        apply_anomaly(&ctx, 1, kBind, 3, 0);
        run_round(b, /*s0=*/0, /*s1=*/0);
        CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_1,
              "I1 束缚应清零先制 → 先手权回到速度判定 → 速度更快的攻方先手");
        std::cout << "I1 PASS 束缚令先制失效（对照组守方先手 → 束缚后攻方先手）\n";
    }

    // I2 超频(35)「技能先制+1」：速度慢的一方靠 +1 抢到先手
    {
        // 让守方速度更低（100）但靠超频 +1 反超攻方——攻方无先制、速度 200，
        // 单靠 +1 不足以反超（先制等级相同才比速度）→ 这里改为验证 ws 层：
        auto b = make_battle(all_gaosu(), all_gaosu());
        b.start();
        BattleContext& ctx = b.ctx();
        apply_anomaly(&ctx, 1, kOverclock, 3, 0);
        run_round(b);
        // 超频 +1 → 守方先制 1 > 攻方 0 → 守方先手
        CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_2,
              "I2 超频 +1 应让守方拿到先手权");
        std::cout << "I2 PASS 超频先制+1 抢到先手权\n";
    }

    // I3 束缚 + 超频：超频的 +1 在清零**之后**施加 → 仍能给自己加先制（用户口径：
    //    "有的靠后改变先制的效果依旧可以绕过束缚给自己加先制"）
    {
        auto b = make_battle(all_gaosu(), all_gaosu());
        b.start();
        BattleContext& ctx = b.ctx();
        apply_anomaly(&ctx, 1, kBind, 3, 0);
        apply_anomaly(&ctx, 1, kOverclock, 3, 0);
        run_round(b);
        CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_2,
              "I3 束缚不该废掉靠后的超频+1（超频在清零之后生效）");
        std::cout << "I3 PASS 束缚下超频仍能加先制（靠后者绕过）\n";
    }

    //==================== J 命中时点 ====================
    // J1 睡眠(8)「对手使用攻击技能且未miss时睡眠解除」
    {
        auto b = make_battle(has_paida(), all_gaosu());
        b.start();
        BattleContext& ctx = b.ctx();
        apply_anomaly(&ctx, 1, kSleep, 3, 0);
        CHECK(ctx.has_active_abnormal_status(1, kSleep), "J1 前置：守方应处于睡眠");
        run_round(b);
        CHECK(!ctx.has_active_abnormal_status(1, kSleep),
              "J1 被攻击技能命中后睡眠应解除");
        std::cout << "J1 PASS 攻击命中解除睡眠\n";
    }

    // J2 属性技能不解除睡眠
    {
        auto b = make_battle(all_gaosu(), all_gaosu());
        b.start();
        BattleContext& ctx = b.ctx();
        apply_anomaly(&ctx, 1, kSleep, 3, 0);
        run_round(b);
        CHECK(ctx.has_active_abnormal_status(1, kSleep),
              "J2 属性技能（非攻击）不应解除睡眠");
        std::cout << "J2 PASS 属性技能不解除睡眠\n";
    }

    // J3 易燃(13)「被火系攻击技能命中时转化为烧伤」
    {
        auto b = make_battle(has_fire(), all_gaosu());
        b.start();
        BattleContext& ctx = b.ctx();
        apply_anomaly(&ctx, 1, kFlammable, 3, 0);
        run_round(b);
        CHECK(!ctx.has_active_abnormal_status(1, kFlammable), "J3 易燃源形态应消失");
        CHECK(ctx.has_active_abnormal_status(1, kBurn), "J3 被火系攻击命中应转化为烧伤");
        std::cout << "J3 PASS 易燃被火系命中→烧伤\n";
    }

    // J4 非火系攻击不转化
    {
        auto b = make_battle(has_paida(), all_gaosu());
        b.start();
        BattleContext& ctx = b.ctx();
        apply_anomaly(&ctx, 1, kFlammable, 3, 0);
        run_round(b);
        CHECK(ctx.has_active_abnormal_status(1, kFlammable), "J4 非火系攻击不应转化易燃");
        CHECK(!ctx.has_active_abnormal_status(1, kBurn), "J4 不应凭空出现烧伤");
        std::cout << "J4 PASS 非火系攻击不转化\n";
    }

    //==================== K 回复与 PP ====================
    // K1 星赐(33)「每回合结束时恢复2点PP值」
    {
        auto b = make_battle(all_gaosu(), all_gaosu());
        b.start();
        BattleContext& ctx = b.ctx();
        for (Skills& sk : ctx.getPet(0).skills) {
            sk.pp = std::min(sk.pp, 5);   // 压低 PP 便于观察（避免"本来已满"看不出效果）
        }
        // ⚠️ 观察**未选用**的槽（2 号）：本回合选的是 1 号槽，它在 AFTER_ACTION 会被
        //    `consume_selected_skill_pp` 扣 1（早于 ROUND_REDUCTION 的星赐回 PP）→ 1 号槽
        //    的净变化是 -1+2=+1，拿它做断言会误判。2 号槽没有任何消耗 → 干净 +2。
        const int pp_before = ctx.getPet(0).skills[2].pp;
        apply_anomaly(&ctx, 0, kStarBlessing, 3, 1);
        run_round(b, /*s0=*/1, /*s1=*/0);
        const int pp_after = ctx.getPet(0).skills[2].pp;
        CHECK_EQ(pp_after, pp_before + 2, "K1 星赐应令每回合结束 +2 PP（未选用的技能）");
        std::cout << "K1 PASS 星赐回合结束 +2PP（" << pp_before << " → " << pp_after << "）\n";
    }

    // K2 星哲(34)「每回合结束时恢复最大体力的1/4」
    {
        auto b = make_battle(all_gaosu(), all_gaosu());
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 1000;   // 先掉血，便于观察回复
        apply_anomaly(&ctx, 0, kStarWisdom, 3, 1);
        const int hp_before = ctx.getPet(0).hp;
        run_round(b, 1, 0);
        CHECK_EQ(ctx.getPet(0).hp - hp_before, kHp / 4, "K2 星哲应令每回合结束回最大体力 1/4");
        std::cout << "K2 PASS 星哲回合结束回 1/4 体力\n";
    }

    // K3 超频(35)「行动开始时恢复所选择技能的全部PP值」
    //    先回满、后扣 1（消耗在 AFTER_ACTION）→ 净 = maxPP - 1
    {
        auto b = make_battle(all_gaosu(), all_gaosu());
        b.start();
        BattleContext& ctx = b.ctx();
        Skills& sk = ctx.getPet(0).skills[0];
        sk.pp = 1;
        const int max_pp = sk.maxPP;
        apply_anomaly(&ctx, 0, kOverclock, 3, 1);
        run_round(b);
        CHECK_EQ(ctx.getPet(0).skills[0].pp, max_pp - 1,
                 "K3 超频应在行动开始时回满所选技能 PP（再扣掉本次消耗 1）");
        std::cout << "K3 PASS 超频行动开始回满所选技能 PP（" << max_pp << " → "
                  << ctx.getPet(0).skills[0].pp << "）\n";
    }

    //==================== L 臣服的拦截范围（用户 2026-09-18 口径）====================
    // 「臣服只拦红伤与粉伤；**真伤拦不住，属性直伤也拦不住**；身上有臣服也可以给对面附加
    //   烧伤然后造成真伤」——G7 已测红伤归零，这里补其余四种。
    {
        // L1 粉伤被拦（固定伤害档）
        auto b = make_battle();
        b.start();
        BattleContext& ctx = b.ctx();
        apply_anomaly(&ctx, 0, kSubmission, 3, 1);
        const int hp0 = ctx.getPet(1).hp;
        deal_pink_damage(&ctx, 1, 1000, DamageKind::FIXED, /*actor=*/0);
        CHECK_EQ(hp0 - ctx.getPet(1).hp, 0, "L1 臣服应拦下粉伤（固定伤害档）");

        // L2 真伤放行：臣服持有者造成的真伤照打
        const int hp1 = ctx.getPet(1).hp;
        deal_true_damage(&ctx, 1, 300, /*actor=*/0);
        CHECK_EQ(hp1 - ctx.getPet(1).hp, 300, "L2 臣服**拦不住真伤**（用户口径）");
        std::cout << "L1/L2 PASS 臣服拦粉伤、放行真伤\n";
    }
    {
        // L3 属性直伤放行（之诗 2088 族走的 deal_attribute_damage）
        auto b = make_battle();
        b.start();
        BattleContext& ctx = b.ctx();
        apply_anomaly(&ctx, 0, kSubmission, 3, 1);
        const int hp0 = ctx.getPet(1).hp;
        const int kElement[2] = {3, 3};   // 属性直伤要带系别（火系做例子）
        deal_attribute_damage(&ctx, 1, 400, kElement, /*actor=*/0);
        CHECK(hp0 - ctx.getPet(1).hp > 0, "L3 臣服**拦不住属性直伤**（用户口径）");
        std::cout << "L3 PASS 臣服放行属性直伤\n";
    }
    {
        // L4 交给对面的异常照常造成真伤：臣服持有者给对面挂烧伤，烧伤的真伤照扣
        auto b = make_battle(all_gaosu(), all_gaosu());
        b.start();
        BattleContext& ctx = b.ctx();
        apply_anomaly(&ctx, 0, kSubmission, 3, 1);
        apply_anomaly(&ctx, 1, kBurn, /*duration_rounds=*/2, /*actor=*/0);
        const int hp0 = ctx.getPet(1).hp;
        run_round(b);
        CHECK_EQ(hp0 - ctx.getPet(1).hp, kHp / 8,
                 "L4 臣服持有者给对面挂的烧伤，其真伤照常结算（不吃臣服）");
        std::cout << "L4 PASS 臣服不拦对面身上由它挂的烧伤真伤\n";
    }

    //==================== M 星赎(38)：回复+30% / 非限制类异常回合数+1 ====================
    {
        // M1 体力恢复效果 +30%
        auto b = make_battle();
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 1000;
        apply_anomaly(&ctx, 0, kStarRedemption, 3, 1);
        const int hp0 = ctx.getPet(0).hp;
        heal(&ctx, 0, 4);   // 恢复最大体力 1/4 = 1250 → 星赎后 1625
        CHECK_EQ(ctx.getPet(0).hp - hp0, kHp / 4 * 130 / 100,
                 "M1 星赎应令体力恢复效果 +30%");
        std::cout << "M1 PASS 星赎回复 +30%\n";
    }
    {
        // M2 非限制类异常回合数 +1（= 给 end_round 加一，不是加永久字段）
        auto b = make_battle(all_gaosu(), has_paida());
        b.start();
        BattleContext& ctx = b.ctx();
        apply_anomaly(&ctx, 1, kBurn, /*duration_rounds=*/2, 0);       // 非限制类
        apply_anomaly(&ctx, 1, kStasis, /*duration_rounds=*/2, 0);     // 限制类
        apply_anomaly(&ctx, 1, kStarRedemption, /*duration_rounds=*/9, 0);
        const int burn_end0 = ctx.get_abnormal_status_end_round(1, kBurn);
        const int stasis_end0 = ctx.get_abnormal_status_end_round(1, kStasis);
        run_round(b, 0, 0);
        // 自然扣减：roundCount +1；星赎 +1 → 非限制类的 end 也 +1 → **剩余回合数不变**
        const int burn_end1 = ctx.get_abnormal_status_end_round(1, kBurn);
        const int stasis_end1 = ctx.get_abnormal_status_end_round(1, kStasis);
        CHECK_EQ(burn_end1, burn_end0 + 1, "M2 星赎应给非限制类异常的 end_round +1");
        CHECK_EQ(stasis_end1, stasis_end0, "M2 限制类（凝滞）不应被星赎 +1");
        // 用户口径的核心：**剩余回合数保持不变**（end+1 抵消自然扣减）。
        // 施加时 roundCount=0、duration=2 → 初始剩余 = 2；跑完一回合（roundCount=1）仍应为 2。
        const int kInitialRemaining = 2;
        CHECK_EQ(burn_end1 - ctx.roundCount, kInitialRemaining,
                 "M2 星赎应令非限制类异常的**剩余回合数保持不变**");
        // 对照组：没有星赎时同一条异常跑一回合后剩余会掉 1（确认上面不是恒等式）
        {
            auto b2 = make_battle(all_gaosu(), has_paida());
            b2.start();
            BattleContext& c2 = b2.ctx();
            apply_anomaly(&c2, 1, kBurn, /*duration_rounds=*/2, 0);
            run_round(b2, 0, 0);
            CHECK_EQ(c2.get_abnormal_status_end_round(1, kBurn) - c2.roundCount,
                     kInitialRemaining - 1, "M2 对照组：无星赎时剩余回合数应掉 1");
        }
        std::cout << "M2 PASS 星赎令非限制类异常 end_round+1 → 剩余回合数保持不变（对照组掉 1）\n";
    }

    //==================== N 扩表后的衍化族（icon 39/40/43）====================
    {
        struct Case { int source; int derived; int rounds; const char* name; };
        const Case cases[] = {
            {kWandering,   kDistraction, 0, "神游→失神"},
            {kVoidFixation, kSilence,    2, "空定→2回合沉默"},
            {kSlowFreeze,  static_cast<int>(AbnormalStatusId::Icebound), 3, "渐冻→3回合冰封"},
        };
        for (const Case& c : cases) {
            auto b = make_battle(all_gaosu(), has_paida());
            b.start();
            BattleContext& ctx = b.ctx();
            apply_anomaly(&ctx, 1, c.source, /*duration_rounds=*/2, 0);
            run_round(b, 0, 0);
            run_round(b, 0, 0);
            run_round(b, 0, 0);   // 源异常在第 3 回合扣减点衍化
            CHECK(!ctx.has_active_abnormal_status(1, c.source), "N 源异常应已结束");
            CHECK(ctx.has_active_abnormal_status(1, c.derived), c.name);
            if (c.rounds > 0) {
                CHECK_EQ(ctx.get_abnormal_status_end_round(1, c.derived) - ctx.roundCount,
                         c.rounds, "N 转出异常的回合数应与官方文本一致");
            }
            std::cout << "N PASS " << c.name << "\n";
        }
    }

    //==================== O 雷解(42)：造成攻击伤害后附 60% 真伤 ====================
    {
        auto b = make_battle(has_paida(), all_gaosu());
        b.start();
        BattleContext& ctx = b.ctx();
        apply_anomaly(&ctx, 0, kThunderRelease, 3, 1);
        const int hp0 = ctx.getPet(1).hp;
        run_round(b);
        const int dealt = hp0 - ctx.getPet(1).hp;
        CHECK(dealt > base_dmg, "O 雷解应在攻击伤害之外补一段真伤");
        CHECK_EQ(dealt, base_dmg + base_dmg * 60 / 100, "O 雷解应附裸伤 60% 的真伤");
        std::cout << "O PASS 雷解附 60% 真伤（" << base_dmg << " → " << dealt << "）\n";
    }

    std::cout << "\n场景 081 全部通过（异常状态副作用：伤害修正/威力命中/先制/命中时点/回复与PP/"
                 "臣服范围/星赎/扩表衍化/雷解）\n";
    return 0;
}
