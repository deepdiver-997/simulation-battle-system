// 场景 960 —— J11 武道衍化姿态机族（7 宏 + 779，moves_lib/j_gaia_stance.cpp）
//（2026-10-01，worktree ../agents/j11；号段 960-969 → T963-T967 五块）
//
// 断言口径：
//   - 姿态不可直接读（plugin_storage），全部经 **行为探针** 断言：逆转↔正转由 2291
//     的分支效果（减伤窗/封回血窗）反向标识；
//   - 伤害断言全走比例带/不等式（85%~100% 浮动；atk=1 单刀 1~2 点，禁精确等式）；
//   - 2298 先后手探针 = **对手 PP 精确计数**（斩杀线：对手 hp=3、两刀死；先手方
//     R2 先打死它 → 死方不再行动 → PP 只消耗一次。PP 无浮动，严格等式）；
//   - 779 与 2298 共用节点/tick，仅条件分支不同（review 覆盖不单测：foe 带强化时
//     985 吸强封属会干扰探针，记档）；2298 首用回合节点未注册吃不到 +2（实现记档）；
//   - 攻击噪声消音：非关键路径的对手一律 NONE。
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
constexpr int kPaiDa   = 10004;   // 拍打：物40（填充/攻击）
constexpr int kXiuFu   = 20309;   // 神经修复[43]：属（回 1/3 体力）
constexpr int kPoYuan  = 37909;   // 战·破元归衍[2299,2294,693]     物85
constexpr int kRiYue   = 37910;   // 战·日月逆行[881,699,733,2292,567] 物150
constexpr int kPoTian  = 37911;   // 战神·破天闪[779,2298,1568,2300,985,2291] 物160
constexpr int kGuiZhen = 29046;   // 战·归真复元[2293,885,1065,843] 属
constexpr int kShenGang = 29045;  // 战·神罡不灭[191,47,2301,1693]  属

constexpr int kFear = static_cast<int>(AbnormalStatusId::Fear);   // 6

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300, int atk = 1, int hp = 99999) {
    ElfPet p = make_pet(1, "盖亚", skills, speed, atk, hp);
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

// 探针：一方 ROUND_START 计数节点（886 同款构造；break 可清）。
int g_probe_ticks = 0;
EffectResult probe_tick(BattleContext*, const EffectArgs&) {
    ++g_probe_ticks;
    return EffectResult::kOk;
}

void register_foe_round_probe(BattleContext* ctx) {
    Effect e(96001, 0, /*owner=*/1, /*left_round=*/-1, EffectArgs(), &probe_tick);
    auto ce = std::make_unique<ContinuousEffect>(e, State::BATTLE_ROUND_START, /*owner=*/1,
                                                 /*duration=*/5, ctx->roundCount);
    ce->source_id_ = 96001;
    ctx->register_skill_effect(State::BATTLE_ROUND_START, 1, std::move(ce));
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T963 2291 按姿态分支（37911 切片 [2,2,50]）══
    // (a) 逆转：R1 37910(2292) 改逆转（2026-11-10 des 449 校正：2294=沿轨迹推进、
    //     不再翻向——方向切换归 2292）→ R2/R3 37911 触发减伤窗 → 对手两刀 ≈ 基线一半。
    {
        int base2 = 0;
        {
            ScenarioBattle b(make_party(self_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*speed=*/100, /*atk=*/3000)));
            b.start();
            BattleContext& ctx = b.ctx();
            for (int r = 0; r < 2; ++r) {
                const int hp0 = ctx.getPet(0).hp;
                b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
                b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
                b.drive();
                base2 += hp0 - ctx.getPet(0).hp;
            }
            CHECK(base2 > 0, "T963.a 前置：基线伤害为正");
        }
        int loss_r23 = 0;
        {
            ScenarioBattle b(make_party(self_pet({kRiYue, kPoTian, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*speed=*/100, /*atk=*/3000)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 2292 改逆转
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            for (int r = 0; r < 2; ++r) {   // R2 直写 + R3 tick 重写，两刀都应减半
                const int hp0 = ctx.getPet(0).hp;
                b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // 37911
                b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
                b.drive();
                loss_r23 += hp0 - ctx.getPet(0).hp;
            }
        }
        const int ratio_x100 = loss_r23 * 150 / base2;   // 两刀对两刀（基线取两刀）
        CHECK(ratio_x100 > 35 && ratio_x100 < 65,
              "T963.a 2291 逆转：对手攻击伤害约减半（实测 " + std::to_string(ratio_x100) +
                  "%，浮动带 35~65）");
        std::cout << "  [T963.a] 2291 逆转减伤窗 通过 (比值 " << ratio_x100 << "%)\n";
    }
    // (b) 正转：直接 37911（初始正转）→ 封对手技能回血 2 回合
    {
        ScenarioBattle b(make_party(self_pet({kPoTian, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kXiuFu, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 37911（正转）
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const int foe_hp_r1 = ctx.getPet(1).hp;
        CHECK(foe_hp_r1 < 99999, "T963.b 前置：对手被我蹭掉一丝（atk=1）");
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R2 对手回血
        b.drive();
        CHECK(ctx.getPet(1).hp == foe_hp_r1,
              "T963.b 2291 正转：对手技能回血被封（2 回合窗内）");
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R3 窗已过
        b.drive();
        CHECK(ctx.getPet(1).hp == 99999, "T963.b 2291 正转：R3 窗口过期回血落地");
        std::cout << "  [T963.b] 2291 正转封回血窗 通过\n";
    }

    // ══ T964 2292 改逆转 + 挡伤记账反爆（37910 切片 [150]）══
    // R1 29045（2301 两回合免攻窗 + 记账武装）→ 对手攻击被挡入账（正转）；
    // R2 37910 → 2292：改逆转 + 读账反爆；对手攻击仍被挡（逆转不入账）；
    // R3 攻击落地（拿到同攻方单刀尺度）+ 无二次反馈（账已清）。
    {
        ScenarioBattle b(make_party(self_pet({kShenGang, kRiYue, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            /*speed=*/100, /*atk=*/3000)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 29045
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(ctx.getPet(0).hp == 99999, "T964 前置：2301 免攻窗 R1 挡下攻击");
        CHECK_EQ(ctx.getPet(1).hp, 99999, "T964 前置：R1 我方属性技不掉对手血");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 37910 反爆
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(ctx.getPet(0).hp == 99999, "T964 前置：免攻窗 R2 仍挡（InRounds 两回合）");
        const int reflected = 99999 - ctx.getPet(1).hp;
        const int foe_hp_r2 = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);   // R3 账已清、窗已过
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const int landed = 99999 - ctx.getPet(0).hp;
        CHECK(landed > 0, "T964 前置：R3 免攻窗过期攻击落地（拿到单刀尺度）");
        CHECK(reflected * 100 > landed * 80 && reflected * 100 < landed * 125,
              "T964 2292 反爆：金额=被挡那刀（实测反爆 " + std::to_string(reflected) +
                  " vs 落地 " + std::to_string(landed) + "，带 80%~125%）");
        CHECK_EQ(ctx.getPet(1).hp, foe_hp_r2, "T964 2292 读账即清：R3 无二次反馈");
        std::cout << "  [T964] 2292 记账反爆 通过 (反爆 " << reflected << ")\n";
    }

    // ══ T965 2293 复原 + 下 N 次衍化全属性+1（29046 切片 [1]；37909 衍化）══
    // 对手全程 NONE：R2 链上 885 因我方满血高于对手（R1 蹭了一下）翻倍 → +2；
    // R3 衍化消费次数 → +3；R4 再衍化无次数 → 仍 +3（六槽和：12/18/18）。
    {
        ScenarioBattle b(make_party(self_pet({kPoYuan, kGuiZhen, kPoYuan, kPoYuan, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 衍化（推进道位）
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(level_sum(ctx, 0), 0, "T965 前置：首次衍化无强化次数，六槽不动");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 归真复元
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(level_sum(ctx, 0), 12, "T965 R2：复原+链上 885 翻倍 +2（六槽和 12）");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R3 衍化消费次数
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(level_sum(ctx, 0), 18, "T965 R3：2293 次数兑现，衍化再 +1（六槽和 18）");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R4 次数已尽
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(level_sum(ctx, 0), 18, "T965 R4：次数耗尽不再加成");
        std::cout << "  [T965] 2293 复原 + 衍化强化次数 通过\n";
    }

    // ══ T966 2298 条件先制（37911，选择期路由）══
    // 点击即判定（on_selected → MOVE_RIGHT 桶，先手权比较前）：我方速度劣势（100 vs
    // 400）、双方 atk=1、对手 hp=1（斩杀线）——R1 无强化 → +2 抢先手，对手死在行动前
    // （PP 满值 35 未消耗）；对照：我方带强化 → 节点哑火 → 对手先手出一刀（PP 34）。
    // PP 无浮动，严格等式。
    {
        int foe_pp_pos = 0, foe_pp_ctrl = 0;
        {
            ScenarioBattle b(make_party(self_pet({kPoTian, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 /*speed=*/100, /*atk=*/1)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*speed=*/400, /*atk=*/1, /*hp=*/1)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 37911
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(ctx.getPet(1).hp <= 0, "T966 前置：对手被击倒");
            foe_pp_pos = ctx.getPet(1).skills[0].pp;
        }
        CHECK_EQ(foe_pp_pos, 35,
                 "T966 2298：无强化点击期+2 抢先手（对手死在行动前，PP 未消耗）");
        {
            ScenarioBattle b(make_party(self_pet({kPoTian, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 /*speed=*/100, /*atk=*/1)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*speed=*/400, /*atk=*/1, /*hp=*/1)));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)stat_change(&ctx, 0, 0, 1);   // setup：自身攻击 +1（处于提升状态）
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(ctx.getPet(1).hp <= 0, "T966 对照前置：对手同样被击倒");
            foe_pp_ctrl = ctx.getPet(1).skills[0].pp;
        }
        CHECK_EQ(foe_pp_ctrl, 34,
                 "T966 2298 对照：处于提升状态不加先制（对手先手出一刀后死，PP 34）");
        std::cout << "  [T966] 2298 选择期条件先制 + 对照 通过 (PP " << foe_pp_pos << "/"
                  << foe_pp_ctrl << ")\n";
    }

    // ══ T967 2299（J1 宏，37909 载体端到端）/ 2300 / 2301 ══
    {
        // 2299：对手带回合类探针 → 消除成功 → 攻击技 PP 归 0（属性技不动）
        {
            g_probe_ticks = 0;
            ElfPet me = self_pet({kPoYuan, kPaiDa, kPaiDa, kPaiDa, kPaiDa});
            ElfPet foe = foe_pet({kPaiDa, kXiuFu, kPaiDa, kPaiDa, kPaiDa});
            foe.skills[0].pp = 7;
            foe.skills[1].pp = 10;
            ScenarioBattle b(make_party(std::move(me)), make_party(std::move(foe)));
            b.start();
            BattleContext& ctx = b.ctx();
            register_foe_round_probe(&ctx);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 37909
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(g_probe_ticks, 1, "T967.a 前置：探针 R1 已 tick");
            CHECK_EQ(ctx.getPet(1).skills[0].pp, 0, "T967.a 2299：攻击技 PP 归 0");
            CHECK_EQ(ctx.getPet(1).skills[1].pp, 10, "T967.a 2299：属性技 PP 不动");
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(g_probe_ticks, 1, "T967.a 2299：探针被消除（R2 无 tick）");
        }
        // 2299 门槛：对手无回合类 → 不归零
        {
            ElfPet me = self_pet({kPoYuan, kPaiDa, kPaiDa, kPaiDa, kPaiDa});
            ElfPet foe = foe_pet({kPaiDa, kXiuFu, kPaiDa, kPaiDa, kPaiDa});
            foe.skills[0].pp = 7;
            ScenarioBattle b(make_party(std::move(me)), make_party(std::move(foe)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(1).skills[0].pp, 7, "T967.a 2299 门槛：消除失败不归零");
        }
        // 2300：自己的攻击被挡（对手 DAMAGE 次免票）→ 100% 害怕
        {
            ScenarioBattle b(make_party(self_pet({kPoTian, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.grant_immunity(1, ImmunityType::DAMAGE, coverage_all(), /*anomaly_mask=*/0,
                               /*duration_rounds=*/0, /*source_id=*/9602,
                               /*soul_immunity=*/false, EffectScope::ON_STAGE,
                               /*counts=*/1, /*source_effect_id=*/9602);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(ctx.has_active_abnormal_status(1, kFear),
                  "T967.b 2300：攻击被限制时 100% 令对手害怕");
        }
        // 2301（29045 载体）：两回合免疫攻击
        {
            ScenarioBattle b(make_party(self_pet({kShenGang, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*speed=*/100, /*atk=*/3000)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(ctx.getPet(0).hp == 99999, "T967.c 2301：R1 攻击被免疫");
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(ctx.getPet(0).hp == 99999, "T967.c 2301：R2 仍在窗内");
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(ctx.getPet(0).hp < 99999, "T967.c 2301：R3 窗口过期攻击落地");
        }
        std::cout << "  [T967] 2299 端到端 + 2300 + 2301 通过\n";
    }

    std::cout << "scenario_963 PASS (J11 武道衍化姿态机族)" << std::endl;
    return 0;
}
