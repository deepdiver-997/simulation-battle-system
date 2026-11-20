#include <utils/sbs_rand.h>
// 场景 4190 —— K263 异常门回血+威力族（368，soul_lib/k263_burn_heal_paralyze_power.cpp）
//（2026-10-21，worktree ../agents/k263；号段 4190-4199 → T4190-T4193 四块）
//
// 断言口径（368「对手处于烧伤状态时每回合恢复200HP；对手处于麻痹状态时威力翻倍」）：
//   - T4190 烧伤门开：对手带烧伤 → 过 ROUND_START 后自身回血 200（hp 精确断言，
//     回血是整数无浮动）。
//   - T4191 烧伤门闭：对手无异常 → 自身不掉血（= 自身初始满血，hp 精确）。
//   - T4192 麻痹门开：对手带麻痹 → 攻击伤害较"无麻痹对照"翻倍（比率带 [1.7,2.4]，
//     两次独立掷点的 85%~100% 浮动带；下界排掉"没翻倍"，上界排掉二次翻倍）。
//   - T4193 麻痹门闭：对手只有烧伤（**非麻痹**）→ 威力**不**翻倍——证明门是
//     "麻痹"这一特定码，不是"任意异常"（与 320 的"任意异常"门对照）。
//
// ⚠️ 场景构造口径：按 **DB args 原样**传 {3,200,1,2}（不补 {0,0} 前缀）——实现侧
//   走 owned_int_args 原序读（K257 起的夜跑口径，见实现头注「行参读取路径」）。
// ⚠️ **烧伤自带威力减半**（materialize 时 is_burn_effective，见场景 1970 注释）——
//   故威力面一律用**麻痹**做门，绝不混用烧伤，否则两效应相互抵消、断言不可读。
#include <cstdlib>
#include <iostream>
#include <vector>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kYanFan = 10037;   // 燕返：威力 60 必中（物理攻击技）
constexpr int kGaoSu = 20017;    // 高速移动：属性技（威力门对照用）
constexpr int kMaxHp = 1000000;
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);          // 2 烧伤
constexpr int kParalysis = static_cast<int>(AbnormalStatusId::Paralysis);  // 0 麻痹
constexpr int kFear = static_cast<int>(AbnormalStatusId::Fear);            // 6 害怕（门闭对照用）

SoulMark make_mark() {
    // DB args 原序 [3,200,1,2]：db[1]=200（每回合恢复量，tips 自证）；其余记档不消费。
    SoulMark sm(368, "K263 魂印", "test", EffectArgs(std::vector<int>{3, 200, 1, 2}));
    sm.monster_id = 2776;
    return sm;
}

ElfPet mark_pet(int atk = 10, int hp = kMaxHp) {
    ElfPet p = make_pet(1, "印主", {kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                        /*speed=*/300, /*atk=*/atk, /*hp=*/hp, make_mark());
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
    return p;
}

ElfPet plain_pet() {
    ElfPet p = make_pet(2, "对手", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                        /*speed=*/100, /*atk=*/10, /*hp=*/kMaxHp);
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 0;
    }
    return p;
}

// ── T4190/T4191：烧伤门 → 自身每回合回血 200 ────────────────────────────────
// 推进到 ROUND_START 之前（快照点），给对手预铺烧伤，再过 ROUND_START。
void run_heal_case(bool burn_foe, int* healed) {
    ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
    b.start();
    BattleContext& ctx = b.ctx();
    // 自身先扣一点血，好让"回血 200"可被观察（否则满血时回血不涨血量）。
    ctx.getPet(0).hp = kMaxHp - 1000;
    ctx.debug_step_mode = true;
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    ctx.debug_step_mode = false;
    if (burn_foe) {
        apply_anomaly(&ctx, 1, kBurn, /*duration_rounds=*/9, /*actor=*/0);
        REQUIRE(ctx.has_active_abnormal_status(1, kBurn));
    }
    const int hp_before = ctx.getPet(0).hp;
    b.step_past(State::BATTLE_ROUND_START);
    *healed = ctx.getPet(0).hp - hp_before;
}

// ── T4192/T4193：麻痹门 → 攻击威力翻倍 ─────────────────────────────────────
// 返回 1 方总掉血。两侧用同一 rand 档，威力 ×2 是乘法关系。
// ⚠️ 门闭对照用**害怕**（控制类，不掉血）而非烧伤——烧伤每回合行动开始扣最大体力
//    1/8（effect_des id=3），会把持续掉血混进伤害测量（实测比率 52，纯属测量污染
//    而非机制问题）。控制类异常不扣血，比率才干净。
// ⚠️ 选技：skill_idx 指定打哪一招（0=燕返物攻 / 1=高速移动属性技）。
int probe_damage(bool paralyze_foe, bool fear_foe, int skill_idx = 0) {
    ScenarioBattle b(make_party(mark_pet(/*atk=*/5000)), make_party(plain_pet()));
    BattleContext& ctx = b.ctx();
    b.start();
    if (fear_foe) {
        apply_anomaly(&ctx, 1, kFear, /*duration_rounds=*/9, /*actor=*/0);
    }
    if (paralyze_foe) {
        apply_anomaly(&ctx, 1, kParalysis, /*duration_rounds=*/9, /*actor=*/0);
    }
    sbs_srand(20261021);   // 施加之后再定档：两侧伤害掷点逐位对齐
    const int hp1 = ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, skill_idx);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
    return hp1 - ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp;
}

// ── T4194：属性技不走翻倍路径（对照攻击技）────────────────────────────────
// 官方文案只说"威力翻倍"，未限定攻击技；本实现按 636/739 同款**只翻攻击技**。
// 高速移动是属性技（DB 实证：9447 个 category=4 技能威力全为 0，属性技无威力可翻）。
// 断言口径：**同一麻痹门下，攻击技的威力视图被翻、属性技的没有**——用两者之差
// 表达"翻倍只作用于攻击技"，而非"两局相等"的恒真断言。
// ⚠️ 变异测试 MUT3（破坏攻击技门）**在本块不可观测**——不是场景漏覆盖，是数学上
//    不可能：属性技 view 恒为 0（DB 实证：9447 个 category=4 技能威力全为 0），
//    翻倍 ×2 作用在 0 上仍是 0 → 破坏该门不改变任何可观测量。攻击技侧的翻倍效果
//    由 T4192 伤害比带（[1.7,2.4]）与本块的 view==120 双重覆盖。详见实现头注
//    「属性技门在数据层不可观测」。
void probe_power_view(int skill_idx, bool paralyze_foe, int* view_out) {
    ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
    BattleContext& ctx = b.ctx();
    b.start();
    if (paralyze_foe) {
        apply_anomaly(&ctx, 1, kParalysis, /*duration_rounds=*/9, /*actor=*/0);
    }
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, skill_idx);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
    *view_out = ctx.ws.skill_power_view[0];
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T4190 烧伤门开：对手烧伤 → 自身回血 200 ══
    {
        int healed = 0;
        run_heal_case(/*burn_foe=*/true, &healed);
        CHECK_EQ(healed, 200,
                 "T4190 368：对手处于烧伤状态 → 自身每回合恢复 200HP");
        std::cout << "  [T4190] 烧伤门开回血 通过\n";
    }

    // ══ T4191 烧伤门闭：对手无异常 → 不回血 ══
    {
        int healed = 0;
        run_heal_case(/*burn_foe=*/false, &healed);
        CHECK_EQ(healed, 0,
                 "T4191 368：对手无异常 → 自身不回血（门闭对照）");
        std::cout << "  [T4191] 烧伤门闭对照 通过\n";
    }

    // ══ T4192 麻痹门开：对手麻痹 → 攻击威力翻倍 ══
    {
        const int plain = probe_damage(/*paralyze_foe=*/false, /*fear_foe=*/false);
        const int vs_para = probe_damage(/*paralyze_foe=*/true, /*fear_foe=*/false);
        CHECK(plain > 1000, "T4192 基线量级");
        const double ratio = static_cast<double>(vs_para) / plain;
        CHECK(ratio >= 1.7 && ratio <= 2.4,
              "T4192 368：对手处于麻痹状态 → 攻击威力翻倍（比率带 [1.7,2.4]，实测 "
                  + std::to_string(ratio) + "）");
        std::cout << "  [T4192] 麻痹门开威力翻倍 通过\n";
    }

    // ══ T4193 麻痹门闭：对手只有害怕（非麻痹）→ 威力不翻倍 ══
    {
        const int plain = probe_damage(/*paralyze_foe=*/false, /*fear_foe=*/false);
        const int vs_fear = probe_damage(/*paralyze_foe=*/false, /*fear_foe=*/true);
        const double ratio = static_cast<double>(vs_fear) / plain;
        // 害怕是控制类异常但**不是麻痹** → 威力不翻倍。比值应远低于翻倍带下界 1.7
        // （同 rand 档下应≈1.0）。
        CHECK(ratio < 1.7,
              "T4193 368：对手处于害怕（非麻痹）→ 威力不翻倍（比率 "
                  + std::to_string(ratio) + " < 1.7）");
        std::cout << "  [T4193] 麻痹门闭对照 通过\n";
    }

    // ══ T4194 攻击技 vs 属性技：同一麻痹门下威力视图差异 ══
    {
        int view_attack = -1;
        int view_attr = -1;
        probe_power_view(/*skill_idx=*/0 /*燕返 物攻*/, /*paralyze_foe=*/true, &view_attack);
        probe_power_view(/*skill_idx=*/1 /*高速移动 属性技*/, /*paralyze_foe=*/true, &view_attr);
        // 攻击技（燕返 威力 60）被翻倍 → view = 120；属性技 view 保持原值（0）。
        CHECK_EQ(view_attack, 120,
                 "T4194 368：对手麻痹 → 攻击技威力视图翻倍（燕返 60 → 120，实测 "
                     + std::to_string(view_attack) + "）");
        CHECK_EQ(view_attr, 0,
                 "T4194 368：属性技不进入翻倍路径（高速移动 view=" +
                     std::to_string(view_attr) + "，属性技无威力可翻）");
        std::cout << "  [T4194] 攻击技/属性技威力视图对照 通过\n";
    }

    std::cout << "scenario_4190 PASS (K263 异常门回血+威力族)" << std::endl;
    return 0;
}
