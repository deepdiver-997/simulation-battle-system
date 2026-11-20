// 场景 935 —— J10 解弱/反转成功族（13 宏，moves_lib/j_dispel_invert_success.cpp）
//（2026-10-01，worktree ../agents/j10；号段 935-939 → T935-T939 五块）
//
// 断言口径：
//   - 门槛强制手段 = setup 直授 stat_drop/stat_change（burn_trio 733/1115 门槛同款）：
//     反转门要"有下降可翻"、解除门要"有下降可清"；门槛负例一律"不 setup 下降"；
//   - 点数/固定值断言（吸取 300、减伤 300、PP 消耗次数）走精确/半带；
//     伤害比例断言带浮动（85%~100%），带差以 R1 实测值标定；
//   - 先手探针 = 对手 PP 精确计数（T966 同款斩杀线，PP 无浮动）；
//   - 载体链污染已排查：805/1055/1897/985/868 等消强门槛链在"对手无强化"下自然失败。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

// ── 载体技（切片已逐条与 side_effect/arg 核对）──
constexpr int kPaiDa   = 10004;   // 拍打：物40（攻击探针）
constexpr int kXiuFu   = 20309;   // 神经修复[43]：属（回 1/3）
constexpr int kHunMG   = 20845;   // 混乱魔光[[99]][100]：属（对手用，100% 混乱）
constexpr int kYingCai = 19651;   // 英才奇佐释[955[300]]          特150
constexpr int kTianHuo = 30159;   // 天火煌煌[33,1043[1]]           特85
constexpr int kYouQin  = 30147;   // 幽侵洪荒决[881,1069[3]]        物150
constexpr int kYiXing  = 32125;   // 异星共振[985,1350[1],447]      物85
constexpr int kWuHui   = 37893;   // 无悔之落日[699,697,2297[1],2121] 物160
constexpr int kHanPo   = 38522;   // 极度·寒魄[1055,2568[1],2533]   物90
constexpr int kXuanMing = 30634;  // 玄冥御风斩[1153[1,300],626]    物150
constexpr int kZiYou   = 25517;   // 自由之翼[585,1183[3,150],843]  属
constexpr int kTaLang  = 31660;   // 踏浪听风吟[868,1312[1,2],773]  物160
constexpr int kWangZhi = 28209;   // 王之冥想[1352[1]]              属
constexpr int kLingHun = 32570;   // 灵魂控制[1392,1131]            物150
constexpr int kLongQi  = 28934;   // 龙契·临[1441[2],805,43,87]     属
constexpr int kReChen  = 36291;   // 热忱凝铸[1700[1],1897]         物90

constexpr int kConfusion = static_cast<int>(AbnormalStatusId::Confusion);   // 10

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300, int atk = 1) {
    ElfPet p = make_pet(1, "己方", skills, speed, atk, /*hp=*/99999);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100, int atk = 1, int hp = 99999) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, hp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

int level_sum(BattleContext& ctx, int side) {
    int sum = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        sum += ctx.ability_levels[side][i];
    }
    return sum;
}

int zero_pp_count(BattleContext& ctx, int side) {
    int n = 0;
    for (const Skills& s : ctx.getPet(side).skills) {
        if (s.pp <= 0) {
            ++n;
        }
    }
    return n;
}

// 探针：一方 ROUND_START 计数节点（886 同款构造；break 可清）。
int g_probe_ticks = 0;
EffectResult probe_tick(BattleContext*, const EffectArgs&) {
    ++g_probe_ticks;
    return EffectResult::kOk;
}

void register_foe_round_probe(BattleContext* ctx) {
    Effect e(93501, 0, /*owner=*/1, /*left_round=*/-1, EffectArgs(), &probe_tick);
    auto ce = std::make_unique<ContinuousEffect>(e, State::BATTLE_ROUND_START, /*owner=*/1,
                                                 /*duration=*/5, ctx->roundCount);
    ce->source_id_ = 93501;
    ctx->register_skill_effect(State::BATTLE_ROUND_START, 1, std::move(ce));
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T935 反转门主干：2297 全属性 / 955 吸取 ══
    {
        // 2297+：有下降可翻 → 反转（-2→+2）+ 全属性+1
        {
            ScenarioBattle b(make_party(self_pet({kWuHui, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)stat_drop(&ctx, 0, 0, 2, 1);   // setup：自身攻击 -2
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.ability_levels[0][0], 3,
                     "T935.a 2297：反转（-2→+2）+ 全属性+1 → 攻击槽 +3");
            CHECK_EQ(level_sum(ctx, 0), 8, "T935.a 2297：六槽和 = 3+1×5 = 8");
        }
        // 2297-：无下降 → 反转失败 → 不加成
        {
            ScenarioBattle b(make_party(self_pet({kWuHui, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(level_sum(ctx, 0), 0, "T935.a 2297 门槛：无下降不反转不加成");
        }
        std::cout << "  [T935.a] 2297 反转全属性 通过\n";
    }
    {
        // 955+：反转成功 → 吸取 300 点（固伤通道精确）+ 载体攻击蹭 1~2
        int loss_pos = 0, loss_neg = 0;
        {
            ScenarioBattle b(make_party(self_pet({kYingCai, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)stat_drop(&ctx, 0, 0, 2, 1);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            loss_pos = 99999 - ctx.getPet(1).hp;
        }
        CHECK(loss_pos >= 300 && loss_pos <= 306,
              "T935.b 955：反转成功吸取 300 点（实测掉 " + std::to_string(loss_pos) + "）");
        {
            ScenarioBattle b(make_party(self_pet({kYingCai, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            loss_neg = 99999 - ctx.getPet(1).hp;
        }
        CHECK(loss_neg <= 3, "T935.b 955 门槛：无下降不吸取（仅载体蹭伤）");
        std::cout << "  [T935.b] 955 反转吸取 通过 (+" << loss_pos << "/门槛 " << loss_neg
                  << ")\n";
    }

    // ══ T936 反转窗口族：1043 转化 / 1069 躲避 / 1350 白板 / 2568 封属 ══
    {
        // 1043（天火煌煌 [1]）：R1 受击全额转化（净掉血 0），R2 窗过期掉血
        {
            ScenarioBattle b(make_party(self_pet({kTianHuo, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*speed=*/100, /*atk=*/3000)));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)stat_drop(&ctx, 0, 0, 2, 1);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, 99999,
                     "T936.a 1043：R1 受击 1:1 转化为体力（净 0）");
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(ctx.getPet(0).hp < 99999, "T936.a 1043：R2 窗口过期正常掉血");
        }
        // 1069（幽侵洪荒决 [3]）：R1-R3 全躲避，R4 落地
        {
            ScenarioBattle b(make_party(self_pet({kYouQin, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*speed=*/100, /*atk=*/3000)));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)stat_drop(&ctx, 0, 0, 2, 1);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            for (int r = 0; r < 2; ++r) {   // R2/R3 仍在躲避窗
                b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
                b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
                b.drive();
            }
            CHECK_EQ(ctx.getPet(0).hp, 99999, "T936.b 1069：R1-R3 躲避所有攻击");
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(ctx.getPet(0).hp < 99999, "T936.b 1069：R4 窗口过期落地");
        }
        // 1350（异星共振 [1]）：对手 R2 攻击技无效，R3 恢复
        {
            ScenarioBattle b(make_party(self_pet({kYiXing, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*speed=*/100, /*atk=*/3000)));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)stat_drop(&ctx, 0, 0, 2, 1);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            // ⚠️ 2390 计账（rounds+2/register+1）可能连授予回合一并覆盖（890 T16 未断言
            //    R1 时点）——这里锚定"R2 白板、R3 恢复"，不设 R1 时点断言。
            const int hp_r1 = ctx.getPet(0).hp;
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, hp_r1, "T936.c 1350：R2 对手攻击技无效");
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(ctx.getPet(0).hp < hp_r1, "T936.c 1350：R3 窗过期恢复");
        }
        // 2568（极度·寒魄 [1]）：对手 R2 属性技无效（神经修复被封），R3 恢复
        {
            ScenarioBattle b(make_party(self_pet({kHanPo, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kXiuFu, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)stat_drop(&ctx, 0, 0, 2, 1);
            // ⚠️ 链上 2533[100,5,2] = 100% 令对手冻伤（每回合扣 1/8 最大体力）——
            //    挂 1 次异常免票挡掉它（冻伤非本单断言面），2568 封属照常触发。
            ctx.grant_immunity(1, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                               /*duration_rounds=*/0, /*source_id=*/93502,
                               /*soul_immunity=*/false, EffectScope::ON_STAGE,
                               /*counts=*/1, /*source_effect_id=*/93502);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int foe_hp_r1 = ctx.getPet(1).hp;
            CHECK(foe_hp_r1 < 99999, "T936.d 前置：对手被蹭伤（回血可观测）");
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R2 神经修复
            b.drive();
            CHECK_EQ(ctx.getPet(1).hp, foe_hp_r1, "T936.d 2568：R2 对手属性技无效（未回血）");
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R3 再修
            b.drive();
            CHECK(ctx.getPet(1).hp > foe_hp_r1, "T936.d 2568：R3 次数耗尽回血落地");
        }
        std::cout << "  [T936] 1043/1069/1350/2568 反转窗口族 通过\n";
    }

    // ══ T937 解除门主干：1352 全属性 / 1392 消回 / 1441 随机 PP ══
    {
        // 1352（王之冥想 [1]，链干净）：清除 -2 → 0，再全属性+1
        {
            ScenarioBattle b(make_party(self_pet({kWangZhi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)stat_drop(&ctx, 0, 0, 2, 1);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.ability_levels[0][0], 1,
                     "T937.a 1352：解除（-2 清零）+ 全属性+1 → 攻击槽 +1");
            CHECK_EQ(level_sum(ctx, 0), 6, "T937.a 1352：六槽和 = 6");
        }
        // 1392（灵魂控制）：解除成功 → 对手回合类探针被消除；门槛负例探针存活
        {
            g_probe_ticks = 0;
            ScenarioBattle b(make_party(self_pet({kLingHun, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)stat_drop(&ctx, 0, 0, 2, 1);
            register_foe_round_probe(&ctx);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(g_probe_ticks, 1, "T937.b 前置：探针 R1 已 tick");
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(g_probe_ticks, 1, "T937.b 1392：解除成功 → 探针被消除（R2 无 tick）");
        }
        {
            g_probe_ticks = 0;
            ScenarioBattle b(make_party(self_pet({kLingHun, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            register_foe_round_probe(&ctx);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(g_probe_ticks, 2,
                     "T937.b 1392 门槛：无下降解除失败 → 探针存活（R2 仍 tick）");
        }
        // 1441（龙契·临 [2]）：恰好 2 槽 PP 归零，其余不动
        {
            ElfPet me = self_pet({kLongQi, kPaiDa, kPaiDa, kPaiDa, kPaiDa});
            ElfPet foe = foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa});
            const int preset[5] = {7, 9, 11, 13, 15};
            for (int i = 0; i < 5; ++i) {
                foe.skills[i].pp = preset[i];
            }
            ScenarioBattle b(make_party(std::move(me)), make_party(std::move(foe)));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)stat_drop(&ctx, 0, 0, 2, 1);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(zero_pp_count(ctx, 1), 2, "T937.c 1441：恰好随机 2 槽 PP 归零");
            int untouched = 0;
            for (int i = 0; i < 5; ++i) {
                if (ctx.getPet(1).skills[i].pp == preset[i]) {
                    ++untouched;
                }
            }
            CHECK_EQ(untouched, 3, "T937.c 1441：其余 3 槽 PP 不动");
        }
        std::cout << "  [T937] 1352/1392/1441 解除门主干 通过\n";
    }

    // ══ T938 解除窗口族：1153 点数减伤 / 1183 增伤窗 / 1312 先制窗 / 1700 次免 ══
    {
        // 1153（玄冥御风斩 [1,300]）：R2 受伤 -300（带差标定），R3 恢复
        {
            ScenarioBattle b(make_party(self_pet({kXuanMing, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*speed=*/100, /*atk=*/3000)));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)stat_drop(&ctx, 0, 0, 2, 1);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 用技（窗=R2）
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            const int d_r1 = 99999 - ctx.getPet(0).hp;
            CHECK(d_r1 > 400, "T938.a 前置：R1 全价掉血（拿到尺度）");
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            const int hp_after_r2 = ctx.getPet(0).hp;
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            const int hp_after_r3 = ctx.getPet(0).hp;
            const int taken_r2 = (99999 - d_r1) - hp_after_r2;   // R2 掉血
            const int taken_r3 = hp_after_r2 - hp_after_r3;      // R3 掉血（窗已过）
            // 85%~100% 浮动下 R1/R2 两刀差 ∈ 300 ± 0.15X；带 ±0.2X（X 以 R1 实测标定）
            const int drift = d_r1 - taken_r2 - 300;
            CHECK(drift <= d_r1 / 5 && drift >= -d_r1 / 5,
                  "T938.a 1153：R2 掉血 ≈ R1 − 300（差 " + std::to_string(d_r1 - taken_r2) +
                      "，实测刀 " + std::to_string(d_r1) + "）");
            CHECK(taken_r3 > taken_r2, "T938.a 1153：R3 窗过期恢复全价");
        }
        // 1183（自由之翼 [3,150]）：R2/R3（窗内）拍打伤害 ≈ R4（窗过期）的 2.5 倍
        {
            ScenarioBattle b(make_party(self_pet({kZiYou, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)stat_drop(&ctx, 0, 0, 2, 1);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 用技
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            int boosted = 0;
            for (int r = 0; r < 2; ++r) {   // R2/R3 窗内拍打
                const int hp0 = ctx.getPet(1).hp;
                b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
                b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
                b.drive();
                boosted += hp0 - ctx.getPet(1).hp;
            }
            const int hp0 = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R4 窗已过
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int baseline = hp0 - ctx.getPet(1).hp;
            CHECK(baseline > 0, "T938.b 前置：拍打出正伤");
            const int ratio_x100 = boosted * 100 / (baseline * 2);   // 两刀对一刀
            CHECK(ratio_x100 > 200 && ratio_x100 < 320,
                  "T938.b 1183：窗内伤害 +150%（实测比值 " + std::to_string(ratio_x100) +
                      "%，带 200~320）");
        }
        // 1312（踏浪听风吟 [1,2]）：R2 先制+2 抢先手（速度劣势方；斩杀线 PP 探针）
        {
            ElfPet me = self_pet({kTaLang, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                 /*speed=*/100, /*atk=*/1);
            ElfPet foe = foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                 /*speed=*/400, /*atk=*/1);
            ScenarioBattle b(make_party(std::move(me)), make_party(std::move(foe)));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)stat_drop(&ctx, 0, 0, 2, 1);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 用技（窗=R2）
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(1).skills[0].pp, 35, "T938.c 前置：R1 对手未行动（NONE）");
            ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp = 1;   // 斩杀线
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 我拍打
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(ctx.getPet(1).hp <= 0, "T938.c 前置：对手被击倒");
            CHECK_EQ(ctx.getPet(1).skills[0].pp, 35,
                     "T938.c 1312：下回合先制+2 抢先手（对手死在行动前，PP 未消耗）");
        }
        // 1700（热忱凝铸 [1]）：R1 混乱被次免挡下，R2 落地
        {
            ScenarioBattle b(make_party(self_pet({kReChen, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kHunMG, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)stat_drop(&ctx, 0, 0, 2, 1);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(!ctx.has_active_abnormal_status(0, kConfusion),
                  "T938.d 1700：第 1 次异常被免疫（次数耗尽）");
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(ctx.has_active_abnormal_status(0, kConfusion),
                  "T938.d 1700：R2 次数已尽，混乱落地");
        }
        std::cout << "  [T938] 1153/1183/1312/1700 解除窗口族 通过\n";
    }

    // ══ T939 门槛负例打包：1350 / 1069 / 1441 ══
    {
        {
            ScenarioBattle b(make_party(self_pet({kYiXing, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*speed=*/100, /*atk=*/3000)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            const int hp_r1 = ctx.getPet(0).hp;
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(ctx.getPet(0).hp < hp_r1,
                  "T939 1350 门槛：无下降 → R2 攻击不白板");
        }
        {
            ScenarioBattle b(make_party(self_pet({kYouQin, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*speed=*/100, /*atk=*/3000)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(ctx.getPet(0).hp < 99999, "T939 1069 门槛：无下降 → R1 攻击落地");
        }
        {
            ElfPet me = self_pet({kLongQi, kPaiDa, kPaiDa, kPaiDa, kPaiDa});
            ElfPet foe = foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa});
            foe.skills[0].pp = 7;
            ScenarioBattle b(make_party(std::move(me)), make_party(std::move(foe)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(1).skills[0].pp, 7, "T939 1441 门槛：无下降 → PP 不动");
        }
        std::cout << "  [T939] 门槛负例打包 通过\n";
    }

    std::cout << "scenario_935 PASS (J10 解弱/反转成功族)" << std::endl;
    return 0;
}
