// 场景 079：异常状态系统 —— 扣血分档 / 限制切换 / 限制附加 / 自我衍化 / 技能无效
//
// 口径来源（两条主线，冲突时以数据库为准）：
//   ① 数据库 `effect_des` 的 `kind=2`（43 条"异常状态定义"）—— **官方文本**，逐条给出
//      名字 + 效果 + 时点 + 档位。查询见 docs_local/docs/05-任务清单/开工-异常状态系统.md §一。
//   ② 语料 `reference/scrape/out/all_posts.jsonl`：
//      idx=472 把异常扣血归纳为三档（真实百分比 / 百分比 / 固定）；
//      idx=266「真实伤害：无视对手护盾、护罩与抗性的伤害」；
//      idx=355「限制类异常状态……限制对方切换，嗑药等等，目前只有凝滞和瘫痪」；
//      idx=473「失神：50 概率属性无效」「沉默：第五效果必定无效」；
//      idx=37/466「异常转化为弱化会无视免弱」。
//
// 验证点：
//   A 扣血分档
//     A1 中毒(1) = 真实百分比：扣 max_hp/8，**护盾不响应真伤**
//     A2 流血(16) = 固定 80
//     A3 沉默(30) = 百分比档，在**回合扣减点**结算（不在行动开始）
//     A4 沉默的百分比伤害**吃护罩**（与 A1 的真伤形成对照 → 证明分档真的生效）
//   B 自我衍化（"a 结束后转化为 b"，官方 effect_des 15/22/23/27）
//     B1 焚烬(22) → 烧伤(2) + **命中等级 -1**
//     B2 焚烬的附带弱化**无视免弱**（目标挂满 STAT_DROP 免疫票照样降）
//     B3 冰封(15) → 冻伤(5) + 速度等级 -1
//     B4 感染(27) → 中毒(1) + 攻击&特攻等级 -1
//     B5 诅咒(23) → 随机转出 烈焰诅咒/致命诅咒/虚弱诅咒 之一
//     B6 过期回写：转出后源异常的 `end_round` 归 0（修掉 `end != 0` 假阳性）
//   C 限制切换
//     C1 凝滞(32) 拦下主动切换（operation 里 roundChoice 不被改写）
//     C2 瘫痪(19) 同样拦下
//     C3 死后补位（is_forced=true）**不受**限制类异常影响（否则对局卡死）
//   D 限制附加
//     D1 凝滞下 Modern 施加控制类异常 → TARGET_IMMUNE
//     D2 凝滞不拦弱化类异常（烧伤照常受）
//     D3/D4 主动毒(Ancient)、被动毒(Raw) **照穿**凝滞
//     D5 凝滞的免疫**不消耗**目标的次数免
//   E 技能无效
//     E1 沉默(30) → 第五技能**无效**（SEALED = SKILL_INVALID，不是 HIT_INVALID）
//     E2 沉默不影响非第五技能
//     E3 失神(29) → 属性技能约 50% 无效（统计档）
//     E4 失神不影响攻击技能
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

// 高速移动：属性技能、必中、威力 0 —— 用来让"hp 变化"只归因于异常（攻方打不出伤害）。
constexpr int kGaoSu = 20017;
// 拍打：物理、命中率 100（必然命中）、威力 40。
constexpr int kPaiDa = 10004;

constexpr int kHp = 5000;

constexpr int kPoison = static_cast<int>(AbnormalStatusId::Poison);
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);
constexpr int kBleed = static_cast<int>(AbnormalStatusId::Bleed);
constexpr int kIcebound = static_cast<int>(AbnormalStatusId::Icebound);
constexpr int kIncinerate = static_cast<int>(AbnormalStatusId::Incinerate);
constexpr int kCurse = static_cast<int>(AbnormalStatusId::Curse);
constexpr int kFlameCurse = static_cast<int>(AbnormalStatusId::FlameCurse);
constexpr int kDeathCurse = static_cast<int>(AbnormalStatusId::DeathCurse);
constexpr int kWeaknessCurse = static_cast<int>(AbnormalStatusId::WeaknessCurse);
constexpr int kInfection = static_cast<int>(AbnormalStatusId::Infection);
constexpr int kSilence = static_cast<int>(AbnormalStatusId::Silence);
constexpr int kDistraction = static_cast<int>(AbnormalStatusId::Distraction);
constexpr int kStasis = static_cast<int>(AbnormalStatusId::Stasis);
constexpr int kCrippled = static_cast<int>(AbnormalStatusId::Crippled);

using SkillArr = std::array<int, 5>;

const SkillArr all_gaosu() { return {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu}; }
const SkillArr has_paida() { return {kPaiDa, kGaoSu, kGaoSu, kGaoSu, kGaoSu}; }

ElfPet attacker(const SkillArr& s) {
    return scenario::make_pet(0, "攻方", s, /*speed=*/200, /*atk=*/100, kHp);
}
ElfPet defender(const SkillArr& s) {
    return scenario::make_pet(1, "守方", s, /*speed=*/100, /*atk=*/100, kHp);
}
ScenarioBattle make_battle(const SkillArr& s0 = all_gaosu(), const SkillArr& s1 = all_gaosu()) {
    return ScenarioBattle({scenario::make_party(attacker(s0))}, {scenario::make_party(defender(s1))});
}

void run_round(ScenarioBattle& b, int s0 = 0, int s1 = 0) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, s0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, s1);
    b.drive();
}

// 可**单步**的驱动器（自建，不改冻结点 scenario_harness.h）。
// 为什么需要：`ScenarioBattle::step_until` 走不通 —— 它在 CHOOSE 等输入点会卡死
// （`need_input()` 为真 → `wait_for_input` 是 no-op → `step()` 原地空转）。
// 本驱动器把 `is_empty=false` 与 `debug_step_mode` 一起用，于是"提交操作"也只推进一个状态，
// 之后可以状态级步进，用来锁"扣血到底发生在哪个时点"。
struct Driver {
    scenario::StubControlBlock cb;
    std::array<SeerRobot, 2> robots;
    BattleContext ctx;
    BattleFsm fsm;

    Driver(const std::array<ElfPet, 6>& p0, const std::array<ElfPet, 6>& p1)
        : robots{SeerRobot(p0), SeerRobot(p1)}
        , ctx(&cb, robots.data())
        , fsm(/*enable_debug=*/false) {}

    void to_choose() { fsm.run(&ctx); }

    // 提交一次操作；debug_step_mode → 只推进一个状态（不跑完整回合）
    void submit_step(int player, int skill) {
        const int buf[4] = {player, static_cast<int>(BattleFsm::ActionType::SELECT_SKILL), skill, 0};
        ctx.m_buffer.assign(reinterpret_cast<const char*>(buf),
                            reinterpret_cast<const char*>(buf) + 4 * sizeof(int));
        ctx.is_empty = false;
        const bool saved = ctx.debug_step_mode;
        ctx.debug_step_mode = true;
        fsm.run(&ctx);
        ctx.debug_step_mode = saved;
    }

    // 单步一个状态
    bool step_once() {
        if (ctx.currentState == State::FINISHED || ctx.need_input()) {
            return false;   // 到等输入点就不能再步进了（harness 的 no-op wait 会原地打转）
        }
        ctx.debug_step_mode = true;
        fsm.run(&ctx);
        ctx.debug_step_mode = false;
        return true;
    }

    // 步进到 target 状态**被到达**（其处理器尚未执行）；返回 false = 走不到
    bool step_to(State target, int max_steps = 200) {
        for (int i = 0; i < max_steps; ++i) {
            if (ctx.currentState == target) {
                return true;
            }
            if (!step_once()) {
                return false;
            }
        }
        return false;
    }
};

// 让守方挂满"免弱"（STAT_DROP）——验证衍化的附带弱化无视它。
void grant_weakness_immunity(BattleContext& ctx, int owner) {
    ctx.grant_immunity(owner, ImmunityType::STAT_DROP, coverage_all(), /*anomaly_mask=*/0,
                       /*duration_rounds=*/0, /*source_id=*/7001, /*soul_immunity=*/false);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //==================== A 扣血分档 ====================
    // A1 中毒 = 真实百分比：扣 max_hp/8，护盾不响应真伤
    {
        auto b = make_battle();
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(1).shield_bank_.add(/*priority=*/1, /*quantity=*/999999, /*source_id=*/1,
                                       /*duration=*/9);
        const int hp0 = ctx.getPet(1).hp;
        apply_anomaly(&ctx, 1, kPoison, /*duration_rounds=*/2, /*actor=*/0);
        run_round(b);
        CHECK_EQ(hp0 - ctx.getPet(1).hp, kHp / 8, "A1 中毒应扣 max_hp/8");
        CHECK_EQ(ctx.getPet(1).shield_bank_.total(), 999999,
                 "A1 真实伤害不应消耗护盾（idx=266 真伤无视护盾）");
        std::cout << "A1 PASS 中毒真伤扣 max_hp/8=" << kHp / 8 << "，护盾未被消耗\n";
    }

    // A2 流血 = 固定 80 点
    {
        auto b = make_battle();
        b.start();
        BattleContext& ctx = b.ctx();
        const int hp0 = ctx.getPet(1).hp;
        apply_anomaly(&ctx, 1, kBleed, 2, 0);
        run_round(b);
        CHECK_EQ(hp0 - ctx.getPet(1).hp, 80, "A2 流血应扣固定 80");
        std::cout << "A2 PASS 流血固定 80 点\n";
    }

    // A3 沉默 = 百分比档，在**回合扣减点**结算（行动开始/回合结束时**都不**扣）
    //    用单步驱动器把"扣血发生在哪个时点"钉死：走到"回合结束后的死亡时点"仍未扣，
    //    跑完 ROUND_REDUCTION 处理器才扣。
    {
        Driver d(scenario::make_party(attacker(all_gaosu())),
                 scenario::make_party(defender(all_gaosu())));
        d.to_choose();
        const int hp0 = d.ctx.getPet(1).hp;
        apply_anomaly(&d.ctx, 1, kSilence, 2, 0);
        d.submit_step(0, 0);
        d.submit_step(1, 0);
        CHECK(d.step_to(State::BATTLE_ROUND_END), "A3 应能步进到回合结束");
        // 到 ROUND_END 时两个行动开始（含 ACTION_START 桶）都已跑完 → 沉默一点没扣
        CHECK_EQ(hp0 - d.ctx.getPet(1).hp, 0, "A3 行动开始时沉默不应扣血");
        // 回合结束之后、扣减点之前的每个状态都不该扣（钉死"不是回合结束时"）
        CHECK(d.step_to(State::BATTLE_OLD_ROUND_END_1), "A3 应能步进到回合结束 1");
        CHECK_EQ(hp0 - d.ctx.getPet(1).hp, 0, "A3 回合结束时沉默不应扣血");
        CHECK(d.step_to(State::BATTLE_ROUND_REDUCTION_ALL_ROUND_MINUS),
              "A3 应能步进到回合扣减点");
        CHECK_EQ(hp0 - d.ctx.getPet(1).hp, 0, "A3 到达回合扣减点时（处理器未跑）仍未扣");
        CHECK(d.step_once(), "A3 应能执行回合扣减点处理器");
        CHECK_EQ(hp0 - d.ctx.getPet(1).hp, kHp / 8,
                 "A3 沉默应在**回合扣减点**扣 max_hp/8（语料 idx=429）");
        std::cout << "A3 PASS 沉默百分比伤害只在回合扣减点结算\n";
    }

    // A4 沉默的百分比伤害吃护罩（与 A1 的真伤形成对照）
    {
        auto b = make_battle();
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(1).hood_bank_.add(/*priority=*/1, /*quantity=*/999999, /*source_id=*/1,
                                     /*duration=*/9);
        const int hp0 = ctx.getPet(1).hp;
        const int hood0 = ctx.getPet(1).hood_bank_.total();
        apply_anomaly(&ctx, 1, kSilence, 2, 0);
        run_round(b);
        CHECK_EQ(hp0 - ctx.getPet(1).hp, 0, "A4 百分比伤害应被护罩完全吸收");
        CHECK_EQ(hood0 - ctx.getPet(1).hood_bank_.total(), kHp / 8,
                 "A4 护罩应吸掉 max_hp/8（与 A1 真伤形成分档对照）");
        std::cout << "A4 PASS 百分比伤害吃护罩（真伤/百分比分档生效）\n";
    }

    //==================== B 自我衍化 ====================
    // B1 焚烬(22) → 烧伤(2) + 命中等级 -1
    {
        auto b = make_battle(all_gaosu(), has_paida());
        b.start();
        BattleContext& ctx = b.ctx();
        apply_anomaly(&ctx, 1, kIncinerate, /*duration_rounds=*/2, 0);
        run_round(b, 0, 0);   // 第 1 回合：焚烬生效（回合末 roundCount 已 +1，仍在窗口内）
        CHECK(ctx.has_active_abnormal_status(1, kIncinerate), "B1 第 1 回合焚烬应仍生效");
        run_round(b, 0, 0);   // 第 2 回合：焚烬最后一个生效回合
        CHECK(!ctx.has_active_abnormal_status(1, kIncinerate),
              "B1 第 2 回合后焚烬已越过失效回合（窗口 = 生效 2 回合）");
        run_round(b, 0, 0);   // 第 3 回合扣减点：焚烬自然结束 → 衍化为烧伤
        CHECK(!ctx.has_active_abnormal_status(1, kIncinerate), "B1 焚烬应已结束");
        CHECK(ctx.has_active_abnormal_status(1, kBurn), "B1 焚烬结束后应转化为烧伤");
        CHECK_EQ(ctx.ability_level(1, kAbilityLevelIndexHit), -1,
                 "B1 焚烬应附带命中等级 -1");
        CHECK_EQ(ctx.ws.view_levels[1][kAbilityLevelIndexHit], -1,
                 "B1 命中等级必须同时写进视图（精度公式读视图）");
        std::cout << "B1 PASS 焚烬→烧伤 + 命中等级-1\n";
    }

    // B2 焚烬的附带弱化**无视免弱**
    {
        auto b = make_battle(all_gaosu(), has_paida());
        b.start();
        BattleContext& ctx = b.ctx();
        grant_weakness_immunity(ctx, 1);
        // 先确认免弱真的在挡普通弱化（否则本组等于没测）
        CHECK(stat_drop(&ctx, 1, kAbilityLevelIndexHit, 1) == StatDropResult::IMMUNE,
              "B2 前置：免弱应挡下普通弱化");
        apply_anomaly(&ctx, 1, kIncinerate, 2, 0);
        run_round(b, 0, 0);
        run_round(b, 0, 0);
        run_round(b, 0, 0);
        CHECK(ctx.has_active_abnormal_status(1, kBurn), "B2 焚烬仍应衍化为烧伤");
        CHECK_EQ(ctx.ability_level(1, kAbilityLevelIndexHit), -1,
                 "B2 衍化带来的弱化应无视免弱（idx=37/466）");
        std::cout << "B2 PASS 焚烬附带弱化无视免弱\n";
    }

    // B3 冰封(15) → 冻伤(5) + 速度等级 -1
    {
        auto b = make_battle(all_gaosu(), has_paida());
        b.start();
        BattleContext& ctx = b.ctx();
        apply_anomaly(&ctx, 1, kIcebound, 2, 0);
        run_round(b, 0, 0);
        run_round(b, 0, 0);
        run_round(b, 0, 0);
        CHECK(!ctx.has_active_abnormal_status(1, kIcebound), "B3 冰封应已结束");
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Frostbite)),
              "B3 冰封结束后应转化为冻伤");
        CHECK_EQ(ctx.ability_level(1, kAbilityLevelIndexSpeed), -1, "B3 应附带速度等级 -1");
        std::cout << "B3 PASS 冰封→冻伤 + 速度等级-1\n";
    }

    // B4 感染(27) → 中毒(1) + 攻击&特攻等级 -1
    {
        auto b = make_battle(all_gaosu(), has_paida());
        b.start();
        BattleContext& ctx = b.ctx();
        apply_anomaly(&ctx, 1, kInfection, 2, 0);
        run_round(b, 0, 0);
        run_round(b, 0, 0);
        run_round(b, 0, 0);
        CHECK(!ctx.has_active_abnormal_status(1, kInfection), "B4 感染应已结束");
        CHECK(ctx.has_active_abnormal_status(1, kPoison), "B4 感染结束后应转化为中毒");
        CHECK_EQ(ctx.ability_level(1, kAbilityLevelIndexAttack), -1, "B4 应附带攻击等级 -1");
        CHECK_EQ(ctx.ability_level(1, kAbilityLevelIndexSpecialAttack), -1,
                 "B4 应附带特攻等级 -1");
        std::cout << "B4 PASS 感染→中毒 + 攻击/特攻等级-1\n";
    }

    // B5 诅咒(23) → 随机转出 烈焰诅咒/致命诅咒/虚弱诅咒 之一；B6 过期回写
    {
        auto b = make_battle(all_gaosu(), has_paida());
        b.start();
        BattleContext& ctx = b.ctx();
        apply_anomaly(&ctx, 1, kCurse, 2, 0);
        run_round(b, 0, 0);
        run_round(b, 0, 0);
        run_round(b, 0, 0);
        const int roll = ctx.has_active_abnormal_status(1, kFlameCurse) ? kFlameCurse
                       : ctx.has_active_abnormal_status(1, kDeathCurse)  ? kDeathCurse
                       : ctx.has_active_abnormal_status(1, kWeaknessCurse) ? kWeaknessCurse
                                                                          : -1;
        CHECK(roll >= 0, "B5 诅咒结束后应随机转出三种诅咒之一");
        CHECK(!ctx.has_active_abnormal_status(1, kCurse), "B5 源诅咒应已结束");
        // B6 过期回写：源异常槽必须被清 0（不再是"end != 0 判有异常"的假阳性）
        CHECK_EQ(ctx.get_abnormal_status_end_round(1, kCurse), 0,
                 "B6 过期异常应回写清 0");
        std::cout << "B5/B6 PASS 诅咒→随机诅咒（" << abnormal_status_name_cn(roll)
                  << "）+ 过期回写清 0\n";
    }

    //==================== C 限制切换 ====================
    // C1/C2 凝滞、瘫痪拦下主动切换；C3 死后补位不受影响
    {
        for (const int restriction : {kStasis, kCrippled}) {
            auto b = make_battle();
            b.start();
            BattleContext& ctx = b.ctx();
            BattleFsm fsm(/*enable_debug=*/false);
            apply_anomaly(&ctx, 0, restriction, 3, 1);
            constexpr int kSentinel = 99;   // 非法 ActionType → 未被改写即"被拦下"
            ctx.roundChoice[0][0] = kSentinel;
            fsm.operation(&ctx, 0, BattleFsm::ActionType::CHOOSE_PET, 1);
            CHECK_EQ(ctx.roundChoice[0][0], kSentinel,
                     "C 限制类异常（凝滞/瘫痪）应拦下主动切换");
            // C3 死后补位：is_forced=true 必须放行（否则宠物阵亡后无法补位 → 对局卡死）
            fsm.operation(&ctx, 0, BattleFsm::ActionType::CHOOSE_PET, 1, /*is_forced=*/true);
            CHECK_EQ(ctx.roundChoice[0][0], static_cast<int>(BattleFsm::ActionType::CHOOSE_PET),
                     "C3 死后补位不受限制类异常影响");
            // 对照：无限制类异常时主动切换放行
            auto b2 = make_battle();
            b2.start();
            BattleContext& ctx2 = b2.ctx();
            BattleFsm fsm2(/*enable_debug=*/false);
            ctx2.roundChoice[0][0] = kSentinel;
            fsm2.operation(&ctx2, 0, BattleFsm::ActionType::CHOOSE_PET, 1);
            CHECK_EQ(ctx2.roundChoice[0][0], static_cast<int>(BattleFsm::ActionType::CHOOSE_PET),
                     "C 对照：无限制类异常时主动切换应放行");
            std::cout << "C PASS " << abnormal_status_name_cn(restriction)
                      << " 禁主动切换 / 死后补位放行 / 对照放行\n";
        }
    }

    //==================== D 限制附加 ====================
    // D1 凝滞下 Modern 控制类异常被免疫
    {
        auto b = make_battle(all_gaosu(), has_paida());
        b.start();
        BattleContext& ctx = b.ctx();
        apply_anomaly(&ctx, 1, kStasis, 3, 0);
        CHECK_EQ(static_cast<int>(apply_anomaly(&ctx, 1, kIncinerate, 2, 0)),
                 static_cast<int>(ApplyAnomalyResult::TARGET_IMMUNE),
                 "D1 凝滞应免疫控制类异常（焚烬）");
        CHECK(!ctx.has_active_abnormal_status(1, kIncinerate), "D1 焚烬不应落地");
        std::cout << "D1 PASS 凝滞免疫控制类异常（Modern）\n";
    }

    // D2 凝滞不拦弱化类异常（烧伤照常受）
    {
        auto b = make_battle(all_gaosu(), has_paida());
        b.start();
        BattleContext& ctx = b.ctx();
        apply_anomaly(&ctx, 1, kStasis, 3, 0);
        CHECK(apply_anomaly(&ctx, 1, kBurn, 2, 0) == ApplyAnomalyResult::SUCCESS,
              "D2 凝滞只免控制类 → 烧伤（弱化类）应照常受");
        CHECK(ctx.has_active_abnormal_status(1, kBurn), "D2 烧伤应落地");
        std::cout << "D2 PASS 凝滞不拦弱化类异常（烧伤照常受）\n";
    }

    // D3/D4 主动毒(Ancient) / 被动毒(Raw) 照穿凝滞
    {
        auto b = make_battle(all_gaosu(), has_paida());
        b.start();
        BattleContext& ctx = b.ctx();
        apply_anomaly(&ctx, 1, kStasis, 3, 0);
        CHECK(apply_anomaly_ancient(&ctx, 1, kIncinerate, 2, 0) == ApplyAnomalyResult::SUCCESS,
              "D3 主动毒（Ancient 通道）应照穿凝滞");
        CHECK(ctx.has_active_abnormal_status(1, kIncinerate), "D3 主动毒应落地");

        auto b2 = make_battle();
        b2.start();
        BattleContext& ctx2 = b2.ctx();
        apply_anomaly(&ctx2, 1, kStasis, 3, 0);
        CHECK(apply_anomaly_raw(&ctx2, 1, kIncinerate, 2, 0) == ApplyAnomalyResult::SUCCESS,
              "D4 被动毒（Raw 通道）应照穿凝滞");
        CHECK(ctx2.has_active_abnormal_status(1, kIncinerate), "D4 被动毒应落地");
        std::cout << "D3/D4 PASS 主动毒/被动毒照穿凝滞\n";
    }

    // D5 凝滞的免疫**不消耗**目标的次数免（它是凝滞自身的免疫，不是目标的免疫票）
    {
        auto b = make_battle(all_gaosu(), has_paida());
        b.start();
        BattleContext& ctx = b.ctx();
        apply_anomaly(&ctx, 1, kStasis, 3, 0);
        ctx.grant_immunity(1, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                           /*duration_rounds=*/0, /*source_id=*/7101,
                           /*soul_immunity=*/false, EffectScope::ON_STAGE,
                           /*counts=*/1, /*source_effect_id=*/7101);
        CHECK_EQ(static_cast<int>(apply_anomaly(&ctx, 1, kIncinerate, 2, 0)),
                 static_cast<int>(ApplyAnomalyResult::TARGET_IMMUNE), "D5 先被凝滞挡下");
        // 撤掉凝滞后重施：若次免还在，应仍被挡（TARGET_IMMUNE）→ 证明它没被凝滞"白扣"
        ctx.set_abnormal_status_end_round(1, kStasis, 0);
        CHECK_EQ(static_cast<int>(apply_anomaly(&ctx, 1, kIncinerate, 2, 0)),
                 static_cast<int>(ApplyAnomalyResult::TARGET_IMMUNE),
                 "D5 凝滞的免疫不应消耗目标的次数免");
        // 再撤掉次免 → 这才该成功（证明上面那次确实是被次免挡的）
        ctx.revoke_immunity(1, 7101);
        CHECK(apply_anomaly(&ctx, 1, kIncinerate, 2, 0) == ApplyAnomalyResult::SUCCESS,
              "D5 去掉次免后应能正常施加");
        std::cout << "D5 PASS 凝滞免疫不消耗次数免\n";
    }

    //==================== E 技能无效 ====================
    // E1/E2/E3 沉默 → 第五技能无效（SEALED = SKILL_INVALID）；非第五技能不受影响
    {
        auto b = make_battle(has_paida());
        b.start();
        BattleContext& ctx = b.ctx();
        Skills& slot0 = ctx.getPet(0).skills[0];
        Skills& slot4 = ctx.getPet(0).skills[kFifthSkillSlot];
        ctx.roundChoice[0][0] = static_cast<int>(BattleFsm::ActionType::SELECT_SKILL);

        ctx.roundChoice[0][1] = kFifthSkillSlot;
        CHECK(slot4.query_usage(&ctx, 0) != SkillUsageResult::SEALED,
              "E1 前置：无沉默时第五技能应可正常使用");
        ctx.roundChoice[0][1] = 0;
        CHECK(slot0.query_usage(&ctx, 0) != SkillUsageResult::SEALED,
              "E2 前置：无沉默时非第五技能可正常使用");

        apply_anomaly(&ctx, 0, kSilence, 3, 1);
        ctx.roundChoice[0][1] = kFifthSkillSlot;
        CHECK_EQ(static_cast<int>(slot4.query_usage(&ctx, 0)),
                 static_cast<int>(SkillUsageResult::SEALED),
                 "E1 沉默应使第五技能**无效**（走 SKILL_INVALID，语料 idx=473）");
        ctx.roundChoice[0][1] = 0;
        CHECK(slot0.query_usage(&ctx, 0) != SkillUsageResult::SEALED,
              "E2 沉默不应影响非第五技能");
        std::cout << "E1/E2 PASS 沉默只无效第五技能\n";
    }

    // E3/E4 失神 → 属性技能约 50% 无效；攻击技能不受影响
    {
        auto b = make_battle(all_gaosu(), has_paida());
        b.start();
        BattleContext& ctx = b.ctx();
        apply_anomaly(&ctx, 0, kDistraction, 3, 1);
        Skills& attr_skill = ctx.getPet(0).skills[0];   // 高速移动 = 属性技能
        ctx.roundChoice[0][0] = static_cast<int>(BattleFsm::ActionType::SELECT_SKILL);
        ctx.roundChoice[0][1] = 0;
        constexpr int kTrials = 400;
        int sealed_attr = 0;
        for (int i = 0; i < kTrials; ++i) {
            if (attr_skill.query_usage(&ctx, 0) == SkillUsageResult::SEALED) {
                ++sealed_attr;
            }
        }
        CHECK(sealed_attr > 0 && sealed_attr < kTrials,
              "E3 失神应使属性技能**约 50%** 无效（既非全无效也非全有效）");
        std::cout << "E3 PASS 失神使属性技能 50% 无效（" << sealed_attr << "/" << kTrials << "）\n";
    }
    {
        auto b = make_battle(has_paida());
        b.start();
        BattleContext& ctx = b.ctx();
        apply_anomaly(&ctx, 0, kDistraction, 3, 1);
        Skills& atk_skill = ctx.getPet(0).skills[0];   // 拍打 = 物理攻击技能
        ctx.roundChoice[0][0] = static_cast<int>(BattleFsm::ActionType::SELECT_SKILL);
        ctx.roundChoice[0][1] = 0;
        int sealed_atk = 0;
        for (int i = 0; i < 400; ++i) {
            if (atk_skill.query_usage(&ctx, 0) == SkillUsageResult::SEALED) {
                ++sealed_atk;
            }
        }
        CHECK_EQ(sealed_atk, 0, "E4 失神不封攻击技能（官方 29 只写属性技能）");
        std::cout << "E4 PASS 失神不影响攻击技能\n";
    }

    //==================== F 能力等级索引口径（槽 5 = 命中，不是体力）====================
    // 这一组的由来：本线一开始把"命中"当成**新增的第 7 槽**（因为 battleContext.h 的注释把
    // 槽 5 写成"体力"、又看不到命中），结果与 `state_tape.h` 的 kSampleLevelSlots=6
    // 的 static_assert 撞车。用户 2026-09-18 更正：**能力提升状态官方就是 6 种**
    // （双攻+双防+速度+**命中**），"基础数值"的第 6 项才是**体力**——两套索引只在 0..4 重合。
    // 引擎内早有证据：`moves_lib/stat_dispel.cpp` 的 `kStatMap[6]={0,2,1,3,4,5}`（5命中→5），
    // 以及 `getTempAbilityValue` 里那句被误置的"命中等级为负时的特殊处理"（85/70/55/45/35/25）。
    // 本组把该口径钉住，免得再有人扩槽。
    {
        auto b = make_battle(all_gaosu(), has_paida());
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK_EQ(kAbilityLevelIndexHit, 5, "F 命中等级必须落在槽 5（能力提升六项，无体力）");
        CHECK_EQ(BattleContext::kAbilityLevelSlotCount, 6, "F 能力等级就是 6 槽（别扩表）");

        // F1 命中等级走的就是槽 5：stat_change/stat_drop(stat=5) 能写进去（既有插件同款调用）
        const int hp_view_before =
            ctx.ws.getTempAbilityValue(1, NumericalPropertyIndex::HP);
        const int speed_view_before =
            ctx.ws.getTempAbilityValue(1, NumericalPropertyIndex::SPEED);
        CHECK(stat_drop(&ctx, 1, kAbilityLevelIndexHit, 1) == StatDropResult::SUCCESS,
              "F1 应有'令对手命中等级-1'这条路径（stat=5）");
        CHECK_EQ(ctx.ability_level(1, kAbilityLevelIndexHit), -1, "F1 命中等级本体应 -1");
        CHECK_EQ(ctx.ws.view_levels[1][kAbilityLevelIndexHit], -1, "F1 命中等级视图应同步");

        // F2 **体力不吃等级**（这是原实现的口径错误：getTempAbilityValue(HP) 会去读槽 5）
        CHECK_EQ(ctx.ws.getTempAbilityValue(1, NumericalPropertyIndex::HP), hp_view_before,
                 "F2 命中等级不应改变体力属性值（体力没有等级）");
        CHECK_EQ(ctx.ws.getTempAbilityValue(1, NumericalPropertyIndex::SPEED), speed_view_before,
                 "F2 命中等级不应串到速度属性值上");

        // F3 负档沿用引擎既有的官方档位表（原误置在 getTempAbilityValue 里）
        CHECK_EQ(hit_level_accuracy_pct(-1), 85, "F3 命中等级 -1 → 85%");
        CHECK_EQ(hit_level_accuracy_pct(-6), 25, "F3 命中等级 -6 → 25%");
        CHECK_EQ(hit_level_accuracy_pct(0), 100, "F3 命中等级 0 → 100%");
        CHECK_EQ(hit_level_accuracy_pct(1), 150, "F3 命中等级 +1 → 150%（通用等级换算）");

        // F4 换宠清等级时命中槽一起清（clear_ability_levels 跟着 kAbilityLevelSlotCount 走）
        ctx.ability_level(1, kAbilityLevelIndexHit) = -2;
        ctx.clear_ability_levels(1);
        CHECK_EQ(ctx.ability_level(1, kAbilityLevelIndexHit), 0, "F4 换宠应清掉命中等级");

        std::cout << "F PASS 能力等级 6 槽且槽 5=命中；体力不吃等级；负档档位表接线\n";
    }

    std::cout << "\n场景 079 全部通过（异常状态系统：扣血分档/限制切换/限制附加/自我衍化/技能无效）\n";
    return 0;
}
