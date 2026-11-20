// 场景 045：增减伤时点链 —— DamagePhase 阶段序 + 通用/非通用两条增伤通道。
//
// 官方两条口径合并而成（用户 2026-09-14 逐条确认）：
//   · 时点链（L345）：「当前时点为**犀牛魂印—通用增伤—693增伤—保底伤害—护盾（从先到后）**」
//   · 减伤区固定顺序（L402）：「分出增伤区、减伤区，不再进行相互穿插。减伤顺序将固定为：
//     **点数减伤——百分比减伤——伤害锁定——伤害免疫**」
//   · 加/乘（L352）：「**所有的通用增伤加法计算，而非通用增伤全部乘法计算**」
//     —— 措辞判据是"**额外**提升X%"（693）vs"提升X%"（776）。
//   · L254：「坚硬是靠前的减伤，乘法计算，**时点早于保底伤害**」→ 保底在减伤之后。
//   · 用户口径：保底伤害**不触发**犀牛超350回满 → 它在犀牛**之后**才把伤害抬起来；
//     锁伤**最后再拦一次**（没穿锁伤凭证的话）。
//
// 引擎阶段序（`DamagePipeline::kOrder`，**顺序即机制**，改一行就是改机制）：
//   GUARD_DETECT → AMP → AMP_EXTRA → REDUCE_FLAT → REDUCE_PCT
//     → FLOOR → CAP → BLOCK → DETECT → FINAL_CORRECT
//
// ⚠️ 本轮只做**阶段骨架**：FLOOR / CAP 没有默认回调（保底/锁伤的具体效果待做），
//    本场景用探针把它们的效果演出来，锁的是**顺序**不是某个精灵。
//
// 验证点：
//   P1 十个阶段全被走到，且执行序 == kOrder（防"阶段空转但名字对"）
//   P2 通用增伤是**加法**：两条 +50 汇总成 +100 → ×2.0（不是 ×2.25）
//   P3 非通用增伤是**乘法**：两条 50 → ×1.5×1.5 = ×2.25
//   P4 ★ 693 排在通用增伤**之后**：两者各 ×2 → 合计 ×4
//   P5 ★ 点数减伤在百分比减伤**之前**：(base-30)×0.5 ≠ base×0.5-30
//   P6 ★ 保底在减伤**之后**、锁伤**之前**：减半 → 抬到 60 → 锁到 40
//   P7 ★ GUARD_DETECT（犀牛）看到的是**未增伤**的值 —— "693/保底穿犀牛"的结构性证据
//   P8 掩码语义没坏：BLOCK|DETECT 位只废挡伤/检测，不废增伤与减伤
//   P9 ★ 减伤**钳在 100%**：超出的部分不算、不溢出、**绝不打出负数伤害**
//      （官方：通用减伤叠加超过 100% 时"减伤效果失效"，本引擎取"至多减满"的钳法）
#include <cassert>
#include <iostream>

#include <effects/effect.h>
#include <entities/pet_factory.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

constexpr int kZhuangJi = 10001;   // 撞击：物理 power 35、无 side_effect
constexpr int kGaoSu = 20017;      // 高速移动：属性技能（不还手）

// 探针记录（只记"owner 是本次攻击方"的那一趟，否则后手方的 ATTACK_DAMAGE 会把 owner 0
// 当防守方再走一遍桶，记录翻倍）。
int g_order[DamagePipeline::kPhaseCount];
int g_at[DamagePipeline::kPhaseCount];
int g_seen = 0;

scenario::ScenarioBattle make_battle(int atk = 100) {
    auto p0 = scenario::make_pet(0, "攻方", {kZhuangJi, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                 /*speed=*/300, atk, /*hp=*/3000);
    auto p1 = scenario::make_pet(1, "守方", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                 /*speed=*/100, atk, /*hp=*/3000);
    p0.skills[0].accuracy = 100;
    p0.skills[0].must_hit = true;
    return scenario::ScenarioBattle(scenario::make_party(p0), scenario::make_party(p1));
}

// 打一回合：0 用技能 0 打 1，1 用属性技能不还手。
void strike(scenario::ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
    b.drive();
}

// 在**每个**阶段挂一个探针，记录"执行到第几个阶段 + 当时 final 多少"。
//
// ⚠️ 挂 **owner 1（防守方）**的桶，不是 owner 0——管线在一个阶段里按 {攻击方, 防守方}
//    顺序走两个桶，**防守方的减伤就跑在 owner 1 的桶里**。挂 owner 0 只能看到"减伤前"的值，
//    会把 REDUCE_FLAT/REDUCE_PCT 的观测点整体前移一格（踩过）。挂 owner 1 = 该阶段收尾的值。
//    （默认回调在 init_battle 时先注册 → 探针后注册 → 看到的是默认回调处理完的结果。）
// ⚠️ 类别用 MITIGATE（bit1）：不落在蚀砚之泪的 BLOCK|DETECT 掩码里。
// ⚠️ 过滤 `attackerId==0 && defenderId==1`：后手方（属性技能）也会跑一遍管线，
//    不滤的话记录翻倍、且那趟的数值是 0。
void register_trace(BattleContext& ctx) {
    g_seen = 0;
    for (DamagePhase phase : DamagePipeline::kOrder) {
        const int idx = static_cast<int>(phase);
        ctx.register_damage_effect(phase, /*owner=*/1, DamageEffectCategory::MITIGATE,
                                   [idx](BattleContext* c, int) {
                                       if (c->resolvedDamage.attackerId != 0
                                           || c->resolvedDamage.defenderId != 1) {
                                           return;  // 只记先手方打守方的那一趟
                                       }
                                       g_order[g_seen] = idx;
                                       g_at[idx] = c->resolvedDamage.final;
                                       ++g_seen;
                                   });
    }
}

int phase_idx(DamagePhase p) { return static_cast<int>(p); }

//========== 把"待测的 ws 字段值"写进 ws ==========
// ⚠️ 不能在 strike() 之前直接写 ws：strike 会走到 BATTLE_ROUND_START，那里 `ws.reset()`
//    是 memset，会把外面设的值全冲掉。必须走**真实路径**——在 SKILL_EFFECT 时点写
//    （晚于 ROUND_START、早于 ATTACK_DAMAGE），这也正是 693/776 的生产写法。
// ⚠️ EffectFn 是**裸函数指针**（不吃捕获 lambda），所以待写值放全局。
int g_amp_pct = 0;                              // 通用增伤（加法通道）
int g_extra[2] = {0, 0};                        // 非通用增伤（乘法通道，两槽）
int g_reduce_flat = 0;                          // 守方点数减伤
int g_reduce_add = 0;                           // 守方百分比减伤（加算）
int g_reduce_mul = 0;                           // 守方百分比减伤（乘算）

EffectResult apply_ws_setup(BattleContext* ctx, const EffectArgs&) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    ctx->ws.damage_add_pct[0] = static_cast<float>(g_amp_pct);
    ctx->ws.damage_add_extra_mul[0][0] = g_extra[0];
    ctx->ws.damage_add_extra_mul[0][1] = g_extra[1];
    ctx->ws.damage_reduce_flat[1][0] = g_reduce_flat;
    ctx->ws.damage_reduce_add[1][0] = g_reduce_add;
    ctx->ws.damage_reduce_mul[1][0] = g_reduce_mul;
    return EffectResult::kOk;
}

void register_ws_setup(BattleContext& ctx) {
    Effect e(999045, /*priority=*/0, /*owner=*/0, /*left_round=*/1,
             EffectArgs(std::vector<int>{0, 1}), &apply_ws_setup);
    auto ce = std::make_unique<ContinuousEffect>(e, State::BATTLE_FIRST_SKILL_EFFECT, 0,
                                                 /*duration=*/1, ctx.roundCount);
    ce->source_id_ = 999045;
    ctx.register_skill_effect(State::BATTLE_FIRST_SKILL_EFFECT, 0, std::move(ce));
}

void reset_setup() {
    g_amp_pct = 0;
    g_extra[0] = g_extra[1] = 0;
    g_reduce_flat = 0;
    g_reduce_add = 0;
    g_reduce_mul = 0;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== P1 十个阶段全被走到，且执行序 == kOrder ==========
    // ⚠️ 裸伤带上伤害浮动的随机（217~255），**每场战斗都不同** → 断言一律用
    //    本场 GUARD_DETECT 探针读到的值作基准，不能跨场比较。
    {
        scenario::ScenarioBattle b = make_battle();
        b.start();
        register_trace(b.ctx());
        strike(b);
        CHECK_EQ(g_seen, DamagePipeline::kPhaseCount,
                 "P1) 每个阶段都真的被走到（不是「名字在、阶段空转」）");
        for (int i = 0; i < DamagePipeline::kPhaseCount; ++i) {
            CHECK_EQ(g_order[i], phase_idx(DamagePipeline::kOrder[i]),
                     "P1) 执行序 == kOrder");
        }
        const int base = g_at[phase_idx(DamagePhase::GUARD_DETECT)];
        CHECK(base > 0, "P1) 裸伤 > 0（否则后面的算术断言没意义）");
        std::cout << "  [P1] 10 阶段执行序 == kOrder；本场裸伤 base=" << base << "\n";
    }

    //========== P2/P3/P4 增伤两通道 + 顺序 ==========
    // P2：通用增伤 = **加法**。两条 +50 由写入方汇总进同一个槽 → ×2.0，不是 ×2.25。
    {
        scenario::ScenarioBattle b = make_battle();
        b.start();
        register_trace(b.ctx());
        reset_setup();
        g_amp_pct = 100;   // 等价于两条 +50 相加（加法通道由写入方汇总）
        register_ws_setup(b.ctx());
        strike(b);
        const int base = g_at[phase_idx(DamagePhase::GUARD_DETECT)];
        const int at_amp = g_at[phase_idx(DamagePhase::AMP)];
        CHECK_EQ(at_amp, base * 200 / 100, "P2) 通用增伤加法：base ×2.0");
        std::cout << "  [P2] 通用增伤（加法）: " << base << " → " << at_amp
                  << "（×2.0，若是乘法会是 " << (base * 3 / 2 * 3 / 2) << "）\n";
    }
    // P3：非通用增伤 = **乘法**。两条 50 各占一槽 → ×1.5×1.5 = ×2.25。
    {
        scenario::ScenarioBattle b = make_battle();
        b.start();
        register_trace(b.ctx());
        reset_setup();
        g_extra[0] = 50;   // 两槽各 50 → 乘法连乘
        g_extra[1] = 50;
        register_ws_setup(b.ctx());
        strike(b);
        const int base = g_at[phase_idx(DamagePhase::GUARD_DETECT)];
        const int at_extra = g_at[phase_idx(DamagePhase::AMP_EXTRA)];
        CHECK_EQ(at_extra, base * 150 / 100 * 150 / 100, "P3) 非通用增伤乘法：base ×1.5 ×1.5 = ×2.25");
        CHECK(at_extra != base * 200 / 100, "P3) 确实不是加法（×2.0 ≠ ×2.25）");
        std::cout << "  [P3] 非通用增伤（乘法）: " << base << " → " << at_extra
                  << "（×2.25；加法只会到 " << (base * 200 / 100) << "）\n";
    }
    // P4：两者各 ×2 → 合计 ×4，且 AMP 探针看到的是**只施加了通用增伤**的值。
    {
        scenario::ScenarioBattle b = make_battle();
        b.start();
        register_trace(b.ctx());
        reset_setup();
        g_amp_pct = 100;
        g_extra[0] = 100;
        register_ws_setup(b.ctx());
        strike(b);
        const int base = g_at[phase_idx(DamagePhase::GUARD_DETECT)];
        const int at_amp = g_at[phase_idx(DamagePhase::AMP)];
        const int at_extra = g_at[phase_idx(DamagePhase::AMP_EXTRA)];
        CHECK_EQ(at_amp, base * 200 / 100, "P4) AMP 阶段只施加了通用增伤（×2）");
        CHECK_EQ(at_extra, at_amp * 200 / 100, "P4) AMP_EXTRA 阶段再 ×2 → 合计 ×4");
        std::cout << "  [P4] 693 排在通用增伤之后: base=" << base << " → AMP=" << at_amp
                  << " → AMP_EXTRA=" << at_extra << "（×4）\n";
    }

    //========== P5 ★ 点数减伤在百分比减伤之前 ==========
    {
        scenario::ScenarioBattle b = make_battle();
        b.start();
        register_trace(b.ctx());
        reset_setup();
        // 点数取 5：裸伤只有 ~26~31，取大了会被钳到 0（那样整段断言就退化了）。
        g_reduce_flat = 5;    // 守方：点数减伤 5
        g_reduce_add = 50;    // 守方：百分比减伤 50%
        register_ws_setup(b.ctx());
        strike(b);
        const int before = g_at[phase_idx(DamagePhase::AMP_EXTRA)];   // 减伤前
        const int flat_done = g_at[phase_idx(DamagePhase::REDUCE_FLAT)];
        const int after = g_at[phase_idx(DamagePhase::REDUCE_PCT)];
        const int reverse = before * 50 / 100 - 5;   // 若"百分比在前、点数在后"会得到这个
        CHECK(before > 5, "P5) 裸伤大于点数减伤值（否则被钳到 0，测不出顺序）");
        CHECK_EQ(flat_done, before - 5, "P5) REDUCE_FLAT 先把**点数**扣掉");
        CHECK_EQ(after, flat_done / 2, "P5) ★ REDUCE_PCT 再按**已扣完点数**的值得算百分比");
        CHECK(after != reverse, "P5) 确实不是反序（base×0.5-5）");
        std::cout << "  [P5] 点数减伤在前: " << before << " → 扣点数 " << flat_done
                  << " → ×50% = " << after << "（反序会得 " << reverse << "）\n";
    }

    //========== P6 ★ 保底在减伤之后、锁伤之前 ==========
    // 守方减伤 50% → FLOOR 抬到 60 → CAP 锁到 40。若保底在减伤**之前**，会得 30 而不是 40。
    {
        scenario::ScenarioBattle b = make_battle();
        b.start();
        BattleContext& ctx = b.ctx();
        register_trace(ctx);
        reset_setup();
        g_reduce_add = 50;
        register_ws_setup(ctx);
        // 保底探针（FLOOR）：把伤害抬到至少 60
        ctx.register_damage_effect(DamagePhase::FLOOR, 0, DamageEffectCategory::MITIGATE,
                                   [](BattleContext* c, int) {
                                       if (c->resolvedDamage.attackerId != 0) {
                                           return;
                                       }
                                       if (c->resolvedDamage.final < 60) {
                                           c->resolvedDamage.final = 60;
                                       }
                                   });
        // 锁伤探针（CAP）：把伤害锁到至多 40 —— 排在保底之后，所以它说了算
        ctx.register_damage_effect(DamagePhase::CAP, 0, DamageEffectCategory::MITIGATE,
                                   [](BattleContext* c, int) {
                                       if (c->resolvedDamage.attackerId != 0) {
                                           return;
                                       }
                                       if (c->resolvedDamage.final > 40) {
                                           c->resolvedDamage.final = 40;
                                       }
                                   });
        strike(b);
        const int reduced = g_at[phase_idx(DamagePhase::REDUCE_PCT)];
        const int floored = g_at[phase_idx(DamagePhase::FLOOR)];
        const int capped = g_at[phase_idx(DamagePhase::CAP)];
        CHECK(reduced < 60, "P6) 减伤后确实低于保底值（否则测不出保底生效）");
        CHECK_EQ(floored, 60, "P6) ★ 保底把伤害**抬**到 60 —— 说明它在减伤**之后**");
        CHECK_EQ(capped, 40, "P6) ★ 锁伤最后再拦一次 → 40（若保底在锁伤之后就还会是 60）");
        std::cout << "  [P6] 保底/锁伤位置: 减伤后=" << reduced << " → 保底=" << floored
                  << " → 锁伤=" << capped << "（锁伤最后说话）\n";
    }

    //========== P7 ★ GUARD_DETECT（犀牛）看到的是未增伤的值 ==========
    // 这是"693 增伤乱穿犀牛"（L348）与"保底不触发犀牛超350回满"的结构性证据：
    // 犀牛的检测在链首，之后才发生的任何"拉起伤害"它都看不到。
    {
        scenario::ScenarioBattle b = make_battle();
        b.start();
        register_trace(b.ctx());
        reset_setup();
        g_amp_pct = 100;
        g_extra[0] = 100;
        register_ws_setup(b.ctx());
        strike(b);
        const int base = g_at[phase_idx(DamagePhase::GUARD_DETECT)];
        const int guard = base;
        const int at_amp = g_at[phase_idx(DamagePhase::AMP)];
        CHECK_EQ(g_at[phase_idx(DamagePhase::GUARD_DETECT)], guard,
                 "P7) ★ 犀牛检测到的值 == 裸伤（未增伤）");
        CHECK(at_amp > guard, "P7) 通用增伤确实发生在犀牛检测之后");
        std::cout << "  [P7] GUARD_DETECT 在增伤之前: 犀牛看到 " << guard
                  << "，之后才被抬到 " << at_amp << "\n";
    }

    //========== P9 ★ 减伤钳在 100%（不溢出、不出负数）==========
    {
        // 加算减伤 150%（>100%）→ 只按 100% 算 → final = base × 0 = 0（不是负数/溢出）
        scenario::ScenarioBattle b = make_battle();
        b.start();
        register_trace(b.ctx());
        reset_setup();
        g_reduce_add = 150;
        register_ws_setup(b.ctx());
        strike(b);
        const int before = g_at[phase_idx(DamagePhase::REDUCE_FLAT)];
        const int after = g_at[phase_idx(DamagePhase::REDUCE_PCT)];
        CHECK(before > 0, "P9) 减伤前伤害 > 0（否则测不出钳制）");
        CHECK_EQ(after, 0, "P9) ★ 加算减伤 150% → 钳到 100% → 伤害 0（未溢出成负数）");
        std::cout << "  [P9] 减伤钳制: 150% 加算减伤 → " << before << " → " << after
                  << "（钳到 100%，不是负数）\n";
    }
    {
        // 乘算减伤超 100%（逐槽钳到 100）+ 加算也是 100% → 仍是 0
        scenario::ScenarioBattle b = make_battle();
        b.start();
        register_trace(b.ctx());
        reset_setup();
        g_reduce_add = 100;
        g_reduce_mul = 100;
        register_ws_setup(b.ctx());
        strike(b);
        const int after = g_at[phase_idx(DamagePhase::REDUCE_PCT)];
        CHECK_EQ(after, 0, "P9) 加算 100% × 乘算 100% → 0（不为负）");
        std::cout << "  [P9] 加算100% × 乘算100% → " << after << "\n";
    }

    //========== P8 掩码语义没坏：BLOCK|DETECT 位只废挡伤/检测 ==========
    {
        scenario::ScenarioBattle b = make_battle();
        b.start();
        BattleContext& ctx = b.ctx();
        int guard_hits = 0, block_hits = 0, detect_hits = 0, reduce_hits = 0, amp_hits = 0;
        // 只数"先手方打守方"的那一趟（后手方也会跑一遍管线，两个桶都再走一次）。
        auto mine = [](BattleContext* c) {
            return c->resolvedDamage.attackerId == 0 && c->resolvedDamage.defenderId == 1;
        };
        ctx.register_damage_effect(DamagePhase::GUARD_DETECT, 1, DamageEffectCategory::DETECT,
                                   [&](BattleContext* c, int) { if (mine(c)) ++guard_hits; });
        ctx.register_damage_effect(DamagePhase::BLOCK, 1, DamageEffectCategory::BLOCK,
                                   [&](BattleContext* c, int) { if (mine(c)) ++block_hits; });
        ctx.register_damage_effect(DamagePhase::DETECT, 1, DamageEffectCategory::DETECT,
                                   [&](BattleContext* c, int) { if (mine(c)) ++detect_hits; });
        ctx.register_damage_effect(DamagePhase::REDUCE_PCT, 1, DamageEffectCategory::MITIGATE,
                                   [&](BattleContext* c, int) { if (mine(c)) ++reduce_hits; });
        ctx.register_damage_effect(DamagePhase::AMP, 0, DamageEffectCategory::AMP,
                                   [&](BattleContext* c, int) { if (mine(c)) ++amp_hits; });
        // 蚀砚之泪 ≥4 滴 = 置对手 BLOCK|DETECT 掩码（挡伤失效，但**减伤不被无视**）
        ctx.getPet(1).damage_suppress_mask |= (1 << static_cast<int>(DamageEffectCategory::BLOCK))
                                            | (1 << static_cast<int>(DamageEffectCategory::DETECT));
        strike(b);
        CHECK_EQ(guard_hits, 0, "P8) GUARD_DETECT 被挡伤失效掩码压制");
        CHECK_EQ(block_hits, 0, "P8) BLOCK 被压制");
        CHECK_EQ(detect_hits, 0, "P8) DETECT 被压制");
        CHECK_EQ(reduce_hits, 1, "P8) 减伤（MITIGATE）**不受**挡伤失效影响");
        CHECK_EQ(amp_hits, 1, "P8) 增伤（AMP）**不受**挡伤失效影响");
        std::cout << "  [P8] 掩码: 挡伤/检测被压制(0/0/0)、减伤与增伤照常(" << reduce_hits << "/"
                  << amp_hits << ")\n";
    }

    std::cout << "\n场景 045 通过：DamagePhase 阶段序 = GUARD_DETECT(犀牛·看未增伤的值) → AMP(通用增伤·加法)"
                 " → AMP_EXTRA(非通用增伤·乘法/693) → REDUCE_FLAT(点数减伤) → REDUCE_PCT(百分比减伤)"
                 " → FLOOR(保底) → CAP(锁伤·最后再拦一次) → BLOCK(免疫) → DETECT → FINAL_CORRECT；"
                 "顺序即机制，kOrder 是可改常量。\n";
    return 0;
}
