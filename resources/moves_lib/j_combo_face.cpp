#include <utils/sbs_rand.h>
// J 组 · 连击面批产 —— 动态次数 / 逐段子效果（2026-10-03 白天专项，worktree=主仓）
//
// 家族语义（effect_info 模板即权威）：
//   484  [3] 连击{0}次，每次命中后连击数+{1}，最高连击{2}次
//   612  [2] {0}%概率攻击{1}次
//   1454 [4] 1回合做{0}-{1}次攻击，每次攻击有{2}%的概率附加{3}点固定伤害
//   1172 [10] {0}回合做{1}~{2}次攻击，每次攻击都有{3}%的概率令自身{4}
//   1685 [3] 1回合做{0}-{1}次攻击，对手处于异常状态时连击上限为{2}
//
// 内核面（本专项新增，2026-10-03）：
//   - ws.combo_count_resolver：动态次数——效果在 SKILL_EFFECT 挂 std::function，
//     finish_attack_damage 在变威力重算前咨询一次、operator= 写回 combo_view
//     （打变威力标记 → 两段重算共用同一 N），用后即焚；固定次数族不挂、走静态 roll。
//   - ws.combo_hit_hook：逐段子效果——红伤落地后按 resolvedDamage.hitCount 循环
//     调用（效果函数自带概率），挡伤作用于整段（final<=0）不触发。
//   ×N 本身 = "一次公式 ×N"（官方 L101 口径，连击/暴击整段共用判定）——不变。
//
// 实现口径：
//   - 484 递增读法（记档）：「每次命中后连击数+{1}」按**跨使用**递增——本次使用
//     count = min({0} + 已命中次数×{1}, {2})，本次攻击落地（红伤 final>0）后
//     AFTER_ACTION 计一次（单次结算模型的实用读法；官方若为"攻击内增长"需逐段
//     重演，内核单次结算语义下不可表达，记档）。owner×技能 分账（1328 同款）。
//   - 612 = 解析器内 roll_percent({0}) → {1} : 1（每次使用掷一次，与静态 roll 同语义）。
//   - 1454 = 静态次数（combo_arg_indices (0,1) 已物化）+ 逐段钩子：每段
//     roll_percent({2}) → fixed_damage({3})（伤害类概率走本地 roll，非申报闸）。
//   - 1172 = 静态次数（(1,2)）+ 逐段钩子：每段 apply_anomaly_checked({3}%，{4}，自身)
//     （异常类概率走申报闸门，J3/J52 口径）。
//   - 1685 = 解析器：对手有任一异常 → roll [{0},{2}]（上限提至{2}），否则 [{0},{1}]。
//
// plugin_storage 键段（990181x，grep 全库无撞）：
//   990181100 + owner*40000 + skill_id%40000   484 已命中次数（跨使用递增账）
//
// 载体切片（unity 库 json_each 精确匹配；场景全走纯链载体）：
//   484 [基数,步长,上限]：14226 月牙之刃 [5,1,10]（物25）
//   612 [概率,次数]：18134 快速斩击 [60,5]（物50）
//   1454 [下限,上限,概率,固伤]：34009 汤圆连打 [5,15,10,50]（物20）
//   1172 [回合,下限,上限,概率,异常码,...]：30762 炽焰狙击 [1,2,6,30,1,...]（特30）
//   1685 [下限,上限,上限提至]：35058 光能连击 [2,4,6]（物30）
//
// 动工前置三查（2026-10-03）：①内核无 5 id 行为硬编码（combo_arg_indices 的 1172/1454/1685
// 仅次数解析=既定次数通道，本批在其上叠加钩子/解析器）②custom_effect_overrides 无声明
// ③moves_lib 无既有注册——真缺口。
// 场景回归：test/scenario/scenario_2070_combo_face.cpp（号段 2070-2079）。

#include "common.h"

#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include <cstdlib>

namespace {

constexpr int k484RampKeyBase = 990181100;   // +owner*40000 + skill_id%40000：已命中次数

int jcf_stor(const BattleContext* ctx, int key) {
    const auto it = ctx->plugin_storage.find(key);
    return it == ctx->plugin_storage.end() ? 0 : any_int_or(it->second);
}

// ── 484 —— 连击{0}次，每次命中后连击数+{1}，最高连击{2}次 ──
// int_args = [owner, target, base, step, cap]（args_num=3）。
EffectResult effect_484_ramp_combo(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 484, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 484, args,
                     "484 布局=[owner,target,基数,步长,上限]");
    const int owner = args.int_args[0];
    const int base = args.int_args[2];
    const int step = args.int_args[3];
    const int cap = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && base > 0 && step > 0 && cap >= base, 484,
                     args, "需要 base>0、step>0、cap>=base");
    const int slot = ctx->executing_skill_slot(owner);
    const int skill_id = (slot >= 0 && slot < 5)
                             ? ctx->getPet(owner).skills[static_cast<std::size_t>(slot)].id
                             : 0;
    const int ramp = jcf_stor(ctx, k484RampKeyBase + owner * 40000 + (skill_id % 40000));
    const int count = std::min(base + ramp * step, cap);
    ctx->ws.combo_count_resolver[owner] = [count](BattleContext*) { return count; };
    // 命中账：本次攻击落地（红伤 final>0）→ AFTER_ACTION 计一次（J50 单发同款双侧注册）
    const int ramp_key = k484RampKeyBase + owner * 40000 + (skill_id % 40000);
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_round_effect(
            ctx, st, owner, 484, /*rounds=*/1, {owner, ramp_key},
            [](BattleContext* c, const EffectArgs& tick_args) -> EffectResult {
                const int side = tick_args.int_args[0];
                if (c->resolvedDamage.attackerId != side || c->resolvedDamage.final <= 0) {
                    return EffectResult::kOk;   // 门：本侧本次攻击命中（红伤落地）
                }
                const int key = tick_args.int_args[1];
                c->plugin_storage[key] = jcf_stor(c, key) + 1;
                return EffectResult::kOk;
            });
    }
    return EffectResult::kOk;
}

// ── 612 —— {0}%概率攻击{1}次 ──
// int_args = [owner, target, chance, count]（args_num=2）。
EffectResult effect_612_chance_combo(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 612, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 612, args,
                     "612 布局=[owner,target,概率,次数]");
    const int owner = args.int_args[0];
    const int chance = args.int_args[2];
    const int count = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && chance > 0 && count > 1, 612, args,
                     "需要 概率>0 且 次数>1");
    ctx->ws.combo_count_resolver[owner] = [chance, count](BattleContext* c) {
        return roll_percent(c->rng(), chance) ? count : 1;
    };
    return EffectResult::kOk;
}

// ── 1454 —— 做{0}-{1}次攻击，每次攻击有{2}%概率附加{3}点固定伤害 ──
// int_args = [owner, target, lo, hi, chance, fixed]（args_num=4）。
EffectResult effect_1454_perhit_fixed(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1454, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 1454, args,
                     "1454 布局=[owner,target,下限,上限,概率,固伤]");
    const int owner = args.int_args[0];
    const int chance = args.int_args[4];
    const int fixed = args.int_args[5];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && chance > 0 && fixed > 0, 1454, args,
                     "需要 概率/固伤 > 0");
    // 次数走静态 roll（combo_arg_indices (0,1)）；钩子只在攻击方挂
    ctx->ws.combo_hit_hook[owner] = [chance, fixed](BattleContext* c, int) {
        if (!g_core || !g_core->fixed_damage) {
            return;
        }
        const int defender = c->resolvedDamage.defenderId;
        if (defender < 0 || defender > 1) {
            return;
        }
        if (roll_percent(c->rng(), chance)) {
            (void)g_core->fixed_damage(c, defender, fixed);
        }
    };
    return EffectResult::kOk;
}

// ── 1172 —— {0}回合做{1}~{2}次攻击，每次攻击都有{3}%的概率令自身{4} ──
// int_args = [owner, target, rounds, lo, hi, chance, anomaly]（args_num=10）。
EffectResult effect_1172_perhit_self_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1172, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 7, 1172, args,
                     "1172 布局=[owner,target,回合,下限,上限,概率,异常码]");
    const int owner = args.int_args[0];
    const int chance = args.int_args[5];
    const int anomaly = args.int_args[6];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && chance > 0
                         && is_valid_abnormal_status_id(anomaly),
                     1172, args, "需要 概率>0 且合法异常码");
    if (!g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    ctx->ws.combo_hit_hook[owner] = [owner, chance, anomaly](BattleContext* c, int) {
        // 异常类概率走申报闸门（J3/J52 口径：掷骰在闸门内、"以实际概率为准"）
        (void)g_core->apply_anomaly_checked(c, owner, anomaly, random_anomaly_duration(),
                                            owner, chance, ChanceSource::Skill);
    };
    return EffectResult::kOk;
}

// ── 1685 —— 做{0}-{1}次攻击，对手处于异常状态时连击上限为{2} ──
// int_args = [owner, target, lo, hi, cap_raise]（args_num=3）。
EffectResult effect_1685_status_cap_combo(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1685, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1685, args,
                     "1685 布局=[owner,target,下限,上限,上限提至]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int lo = args.int_args[2];
    const int hi = args.int_args[3];
    const int cap_raise = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && lo > 0 && hi >= lo && cap_raise >= hi, 1685,
                     args, "需要 lo>0、hi>=lo、cap>=hi");
    ctx->ws.combo_count_resolver[owner] = [owner, target, lo, hi, cap_raise](BattleContext* c) {
        const int up = c->has_any_active_abnormal_status(target) ? cap_raise : hi;
        if (up <= lo) {
            return lo;
        }
        return lo + (c->rng().next() % (up - lo + 1));
    };
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(484, effect_484_ramp_combo)
SKILL_EFFECT(612, effect_612_chance_combo)
SKILL_EFFECT(1454, effect_1454_perhit_fixed)
SKILL_EFFECT(1172, effect_1172_perhit_self_anomaly)
SKILL_EFFECT(1685, effect_1685_status_cap_combo)
