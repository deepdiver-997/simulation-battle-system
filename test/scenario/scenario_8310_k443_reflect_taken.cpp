// 场景 8310：K443 · 929 克制族「先制-N；反弹{0}倍的伤害给对手并使自身恢复等量体力」
//（resources/moves_lib/k443_reflect_taken.cpp + 内核查询面 ws.round_taken_attack_damage
//  （battleFsm apply_resolved_damage 唯一写入点），2026-11-09；号段 8310-8319）
//
// 断言口径（详见插件头注）：**全自洽精确等式**——弹回量 == 查询面读数 ×{0}、回复 == 弹回量、
//   查询面读数 == 我方实际净掉血；红伤 85%~100% 浮动只影响等式两边共同的具体值。
//   先制序 = 结构断言（1390/1629 同法）：我方速度 1000 > 对手 100，若无先制-N 则我方
//   先出手、出招时点查询面读数 = 0 → 弹回 0；「taken > 0 且弹回落地」即证明对手先手。
// 腿布局（派发单三腿 + 盾料腿 + mult=1 变体腿）：
//   T1 预置受击造料腿（31355 神谕蓄势：native 必中、先制-6、{0}=2）：对手拍打先手造料
//      → 弹回 2×实扣精确、回复等量精确（半血观测防 clamp，坑集 §9）。
//   T2 零受击腿（19563 狂魂崩断）：对手不动 → 弹回 0、回复 0（如实口径，模板无兜底
//      子句——审计槽 2/3 = 0、对手/自身血量纹丝不动）。
//   T3 多段腿（19563；对手贯火龙 10818 固定 2 段 [31]|[2,2]，一次公式 ×N 单笔落地）：
//      累计含连击全段总扣 → 弹回 = 2×全段总扣精确（变异「按单段份额入账」在此转红）。
//   T4 盾料腿（33431 未来·克制；1 点盾恰吸 1）：弹回基线 = 实扣（吸收点排除）——
//      变异「基线换链首单笔 guard_detect_damage（增伤前/护盾前口径）」在此转红。
//   T5 mult=1 变体腿（37973 琅珀托风雨：先制-1、{0}=1；共链 1468 已由 K445 落地
//      （k445_purge_both_fixed_pp.cpp）——本腿零状态零盾 → 其消除门闭 → ③④不发，
//      断言面不受影响，2026-11-09 复核仍全绿）。
//   载体覆盖说明：37150/37385 不入场景腿——共链 1327/1202 **已实现**（d5_D15/
//      j_low_damage_flat_reduce），同局驱动会引入其语义噪声污染精确等式；六载体共用
//      同一 SKILL_EFFECT(929) 注册（注册面按效果 id 命中），37150/37385 的 929 路径
//      与其余四载体逐字节一致。
// 断言种子无关（等式断言不吃浮动；make_pet 清 crit_rate；必中短路命中掷骰不耗 rand）。
#include <any>
#include <array>
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/skills.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <utils/sbs_rand.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kPaiDa  = 10004;   // 拍打：物40 无效果填充刀（crit_rate=0）
constexpr int kCombo2 = 10818;   // 贯火龙：物60 acc100 [31]|[2,2] → 固定 2 段

constexpr int k31355 = 31355;    // 神谕蓄势   [929]|[2]  先制-6 必中 native
constexpr int k19563 = 19563;    // 狂魂崩断   [929]|[2]  先制-6
constexpr int k33431 = 33431;    // 未来·克制  [929]|[2]  先制-6
constexpr int k37973 = 37973;    // 琅珀托风雨 [929]|[1]  先制-1（共链 1468 已由 K445 落地）

constexpr int kAudit929 = 1092900000;   // 10^9 + 929×10^4 + owner×16 + slot

// 我方（side 0）：929 载体，速度 1000（远超对手；先手权只可能被先制-N 翻转）。
// ⚠️ 载体技元素全部改写为电(5)：对手宠物元素=地面(7) → 克制矩阵 [5][7]=0 →
//    载体自身 pow=0 攻击**精确归零**（否则公式保底 +2×浮动 ∈ {1,2} 会污染
//    「对手掉血 == 弹回量」精确等式；弹回走粉伤不受元素影响）。
ElfPet carrier(int skill_id) {
    ElfPet p = scenario::make_pet(0, "929载体", {skill_id, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                  /*speed=*/1000, /*atk=*/1, /*hp=*/2000000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;     // 载体出刀必中（pow=0 攻击技无伤害噪声；K7 短路命中掷骰不耗 rand）
        s.accuracy = 100;
        s.element[0] = 5;   // 电 → 打地面(7)克制=0（载体自伤归零，见上注）
    }
    return p;
}
// 对手（side 1）：造料刀，速度 100（先手权必输给先制-N 载体——除非先制丢失）。
// 元素地面(7)：拍打(普通8)/贯火龙(火3) → 地面倍率均 >0（造料刀照常落地）。
ElfPet foe(int skill_id, int atk) {
    ElfPet p = scenario::make_pet(1, "对手", {skill_id, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                  /*speed=*/100, atk, /*hp=*/600000);
    p.elementalAttributes = {7, 7};
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

struct LegRead {
    int low = 0;        // 我方压血后的起点
    int foe_hp0 = 0;    // 对手起点
    int mult = -99;     // 行参 {0}（审计槽 0）
    int taken = 0;      // 查询面读数（审计槽 1）
    int reflect = 0;    // 弹回计算值（审计槽 2）
    int healed = 0;     // 回复实际落地（审计槽 3）
    int uses = 0;       // 使用计数（审计槽 8）
    int skip = -99;     // 跳过原因（审计槽 9；0 = 正常结算）
    int my_final = 0;   // 我方回合末体力
    int foe_final = 0;  // 对手回合末体力
};

// 通用骨架：开局 → 半血（防回复 clamp）→（可选 1 点盾）→ 一回合 → 读审计槽与血量。
LegRead run_leg(int carrier_skill, int foe_skill, int foe_atk, unsigned seed,
                BattleFsm::ActionType foe_op = BattleFsm::ActionType::SELECT_SKILL,
                bool half_hp = true, int shield_qty = 0) {
    ScenarioBattle b(scenario::make_party(carrier(carrier_skill)),
                     scenario::make_party(foe(foe_skill, foe_atk)));
    b.start();
    BattleContext& ctx = b.ctx();
    LegRead r;
    r.low = half_hp ? ctx.getPet(0).hp / 2 : ctx.getPet(0).hp;
    ctx.getPet(0).hp = r.low;
    if (shield_qty > 0) {
        ctx.getPet(0).shield_bank_.add(/*priority=*/0, shield_qty,
                                       /*source_id=*/91003, /*duration=*/0);
    }
    r.foe_hp0 = ctx.getPet(1).hp;
    sbs_srand(seed);
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, foe_op, foe_op == BattleFsm::ActionType::NONE ? -1 : 0);
    b.drive();
    auto read = [&](int slot) {
        const auto it = ctx.plugin_storage.find(kAudit929 + 0 * 16 + slot);
        if (it == ctx.plugin_storage.end()) return -99;
        const int* v = std::any_cast<int>(&it->second);
        return v ? *v : -99;
    };
    r.mult = read(0);
    r.taken = read(1);
    r.reflect = read(2);
    r.healed = read(3);
    r.uses = read(8);
    r.skip = read(9);
    r.my_final = ctx.getPet(0).hp;
    r.foe_final = ctx.getPet(1).hp;
    return r;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══════════ T1 预置受击造料腿（31355：先制-6 native 必中，{0}=2）══════════
    {
        const LegRead r = run_leg(k31355, kPaiDa, 200, 20261109);
        CHECK(r.taken > 0,
              "T1 核心·先制序：出招时点查询面读数>0 = 对手先手造料成功"
              "（我方速度 1000 > 对手 100，唯一能翻转先手权的只有先制-6）");
        CHECK_EQ(r.skip, 0, "T1 正常结算（无跳过）");
        CHECK_EQ(r.mult, 2, "T1 行参 {0}=2");
        CHECK_EQ(r.taken, r.my_final - r.low,
                 "T1 查询面读数 == 我方净掉血（自洽：攻击先落、回复后行）");
        CHECK_EQ(r.reflect, 2 * r.taken, "T1 核心：弹回量 == 实扣 ×2（精确）");
        CHECK_EQ(r.foe_final, r.foe_hp0 - 2 * r.taken,
                 "T1 核心：对手掉血 == 弹回量（粉伤全额落地，无护罩/抗性）");
        CHECK_EQ(r.healed, 2 * r.taken, "T1 核心：回复 == 弹回量（等量，heal_amount 实落地）");
        CHECK_EQ(r.my_final, r.low - r.taken + 2 * r.taken,
                 "T1 我方回合末 == low - 掉血 + 回复（半血区间无 clamp）");
        CHECK_EQ(r.uses, 1, "T1 使用计数=1");
        std::cout << "  [T1] 造料腿通过（taken=" << r.taken << " reflect=" << r.reflect
                  << " healed=" << r.healed << "）\n";
    }

    // ══════════ T2 零受击腿（19563：对手不动 → 弹回 0、回复 0）══════════
    {
        const LegRead r = run_leg(k19563, kPaiDa, 200, 20261109,
                                  BattleFsm::ActionType::NONE, /*half_hp=*/false);
        CHECK_EQ(r.taken, 0, "T2 前提：本回合零受击（对手 NONE）");
        CHECK_EQ(r.skip, 0, "T2 正常结算（零受击不是跳过——如实走 0 档）");
        CHECK_EQ(r.reflect, 0, "T2 核心：弹回 0（模板无兜底子句，不造固定弹）");
        CHECK_EQ(r.healed, 0, "T2 核心：回复 0");
        CHECK_EQ(r.foe_final, r.foe_hp0, "T2 对手体力纹丝不动");
        CHECK_EQ(r.my_final, r.low, "T2 自身体力纹丝不动（载体 pow=0 无伤害噪声）");
        CHECK_EQ(r.uses, 1, "T2 使用计数=1（效果确实执行过）");
        std::cout << "  [T2] 零受击腿通过（弹回 0 / 回复 0 如实档）\n";
    }

    // ══════════ T3 多段腿（19563；对手贯火龙固定 2 段）══════════
    {
        const LegRead r = run_leg(k19563, kCombo2, 200, 20261109);
        CHECK(r.taken > 0, "T3 前提：连击造料落地");
        CHECK_EQ(r.taken, r.my_final - r.low, "T3 查询面读数 == 全段总扣（自洽）");
        CHECK_EQ(r.reflect, 2 * r.taken,
                 "T3 核心：弹回 == 全段总扣 ×2（变异「按单段份额入账」在此转红："
                 "单段份额口径下 reflect == taken 而非 2×taken）");
        CHECK_EQ(r.foe_final, r.foe_hp0 - 2 * r.taken, "T3 对手掉血 == 弹回量（精确）");
        CHECK_EQ(r.healed, 2 * r.taken, "T3 回复 == 弹回量（等量）");
        CHECK_EQ(r.my_final, r.low + r.taken, "T3 我方回合末（半血区间无 clamp）");
        std::cout << "  [T3] 多段腿通过（全段总扣=" << r.taken << " 弹回=" << r.reflect << "）\n";
    }

    // ══════════ T4 盾料腿（33431；1 点盾恰吸 1 → 基线 = 实扣）══════════
    {
        const LegRead r = run_leg(k33431, kPaiDa, 200, 20261109,
                                  BattleFsm::ActionType::SELECT_SKILL,
                                  /*half_hp=*/true, /*shield_qty=*/1);
        CHECK(r.taken > 0, "T4 前提：穿盾落地");
        CHECK_EQ(r.taken, r.my_final - r.low,
                 "T4 查询面读数 == 净掉血（吸收的 1 点盾不在内——实扣裁决）");
        CHECK_EQ(r.reflect, 2 * r.taken,
                 "T4 核心：弹回基线 = 实扣（变异「换链首单笔 guard_detect_damage"
                 "（护盾前口径，比实扣大 1）」在此转红）");
        CHECK_EQ(r.foe_final, r.foe_hp0 - 2 * r.taken, "T4 对手掉血 == 弹回量（精确）");
        CHECK_EQ(r.healed, 2 * r.taken, "T4 回复 == 弹回量（等量）");
        std::cout << "  [T4] 盾料腿通过（实扣=" << r.taken << " 弹回=" << r.reflect << "）\n";
    }

    // ══════════ T5 mult=1 变体腿（37973：先制-1、{0}=1）══════════
    {
        const LegRead r = run_leg(k37973, kPaiDa, 200, 20261109);
        CHECK(r.taken > 0, "T5 核心·先制序：先制-1 同样让对手先手（速度 1000>100 仍后手）");
        CHECK_EQ(r.mult, 1, "T5 行参 {0}=1");
        CHECK_EQ(r.reflect, 1 * r.taken, "T5 核心：弹回 == 实扣 ×1（checked_pct_mul 零偏带）");
        CHECK_EQ(r.foe_final, r.foe_hp0 - r.taken, "T5 对手掉血 == 弹回量（精确）");
        CHECK_EQ(r.healed, r.taken, "T5 回复 == 弹回量（等量）");
        CHECK_EQ(r.my_final, r.low - r.taken + r.taken, "T5 我方回到压血线（掉 1 补 1）");
        std::cout << "  [T5] mult=1 变体腿通过（taken=" << r.taken << "）\n";
    }

    std::cout << "scenario_8310 全部通过\n";
    return 0;
}
