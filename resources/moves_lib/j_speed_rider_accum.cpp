// J 组 J131 · 速度附加累积族 —— 1 宏/12 载体（2026-10-04，worktree ../agents/j131）
//
// 家族语义（effect_info 模板 + 4399 逐字双源）：
//   869 [基值,增量,上限] "附加自身速度{0}%的固定伤害，每次触发增加{1}%，最高{2}%"
//     —— 4399 逐字钉死为**固定**伤害（异空反冲"附加自身速度30%的固定伤害，每次
//     增加10%，最高50%"；effect_info 写"百分比伤害"系模板陈旧——1024 同款记档，
//     取 DamageKind::FIXED）。 rider 原语 = fixed_damage(防御方, 速度×pct/100)。
//     ⚠️"每次触发增加{1}%"系**量累积**（4399 无几率字样；同族 1366"每次使用几率
//     增加"才是率累积——两族勿混，工单口径）。
//
// 实现口径：
//   - 触发位 = DETECT **攻方桶**恒久回调（模板无窗口；J15 1088 同位同恒久形）：
//     d.attackerId==owner、d.isRed、d.final>0（有红伤落地才算"触发"）。
//   - 速度取值面 = ws.getTempAbilityValue(owner, SPEED) **视图**（吃能力等级升降，
//     battleWorkspace.h:561——"自身速度"指当下有效速度，记档）。
//   - 量累积走 plugin_storage 审计槽 kAudit869Base+owner（F9 782 口径）：武装时无槽
//     则初始化为 {0}（**重复武装不重置**——累积跨用技持续，记档：官方对重施无实证，
//     取"堆叠属于精灵"口径）；每次触发后 pct=min(pct+{1}, {2})。
//   - rider 在 DETECT 时点当场 fixed_damage（红伤未扣血即附加——1088 drain 同位先例）；
//     fixed 走粉伤路径不回流 DETECT，无重入。
//
// 载体切片（unity 库 side_effect.arg_count 切分 12/12 自洽、零剩余，脚本核算
// 2026-10-04）：[30,10,50]、[40,10,60]×3、[40,20,80]×3、[40,20,100]、[40,10,100]、
//   [50,10,100]、[50,25,100]、[35,20,75]、[20,20,60]
//   纯链：异空反冲 33537 [30,10,50]（物130——施放即红伤，首跳载体）
//   短链：甜蜜发酵 34703（1295 伴生）、逆魂之羽 34813（1471 伴生）等
//
// 动工前置三查（2026-10-04）：①内核 grep 869 无硬编码 ②custom_effect_overrides
//   零行 ③moves_lib 全 worktree 并集无注册——真缺口。
// 先例：J15 1088（DETECT 攻方桶恒久 rider）、F9 782（审计槽）、1024/J14（模板
//   "百分比"vs 4399"固定"的取 FIXED 判例）。
// 场景回归：test/scenario/scenario_2260_speed_rider_accum.cpp（号段 2260-2269，
//   纯链载体 + 审计槽做无随机精确断言）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

// 869 审计槽基址（F9 782 口径；+owner）。
constexpr int kAudit869Base = 990086910;

// ── 869 —— 附加自身速度{0}%的固定伤害，每次触发增加{1}%，最高{2}% ──
// int_args = [owner, target, {0}基值, {1}增量, {2}上限]（args_num=3）。
EffectResult effect_869_speed_rider_accum(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 869, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 869, args,
                     "869 布局=[owner,target,基值,增量,上限]");
    const int owner = args.int_args[0];
    const int base = args.int_args[2];
    const int inc = args.int_args[3];
    const int cap = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 869, args, "owner 越界");
    EFFECT_ARG_GUARD(base > 0 && inc > 0 && cap >= base, 869, args,
                     "需要 基值>0 且 增量>0 且 上限>=基值");
    if (!g_core || !g_core->fixed_damage) {
        return EffectResult::kOk;
    }
    // 武装：无槽则初始化为基值（重复武装不重置——累积跨用技持续，文件头记档）。
    const auto key = static_cast<unsigned long long>(kAudit869Base + owner);
    if (ctx->plugin_storage.find(key) == ctx->plugin_storage.end()) {
        ctx->plugin_storage[key] = base;
    }
    ctx->register_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner, inc, cap](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner || !g_core || !g_core->fixed_damage) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || d.defenderId < 0 || d.defenderId > 1) {
                return;
            }
            if (!d.isRed || d.final <= 0) {
                return;   // 有红伤落地才算"触发"
            }
            const auto it = c->plugin_storage.find(kAudit869Base + owner);
            const int pct = it == c->plugin_storage.end() ? 0 : std::any_cast<int>(it->second);
            if (pct <= 0) {
                return;
            }
            const int speed =
                c->ws.getTempAbilityValue(owner, NumericalPropertyIndex::SPEED);
            if (speed > 0) {
                (void)g_core->fixed_damage(c, d.defenderId, speed * pct / 100);
            }
            // 每次触发后累积（封顶 {2}）。
            it->second = std::min(pct + inc, cap);
        });
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(869, effect_869_speed_rider_accum);
