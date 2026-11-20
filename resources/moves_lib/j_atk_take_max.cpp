// J147 · 攻特攻取高族 —— 2220×13（2026-10-04，worktree ../agents/j147）
//
// 家族语义（effect_info 官方模板即权威；零参）：
//
// 2220（args_num=0）"将自身攻击和特攻中最高的能力值作为自己的能力值进行攻击"：
//   - 视图层手术（2490「以XX系别结算」写视图同通道）：SKILL_EFFECT 时点读本侧
//     ws.battle_attrs 的物攻/特攻面板 → 双双抬至 max（攻击技无论物/特都吃
//     max(物攻,特攻)；模板只说"攻击能力值"，特攻技同享为双向取高落地——记档）；
//   - battle_attrs = ROUND_START 从本体重基的**本次攻击快照**，ATTACK_DAMAGE 读它
//     ——SKILL_EFFECT 写入恰落在两者之间，ws 每回合重基 → 无跨回合泄漏；
//   - 只抬面板值；能力等级修正仍在公式后段按槽位各自生效（不受本效果影响）。
//
// 动工前置三查（2026-10-04，立单时已核）：①内核 grep 2220 无行为硬编码
//   ②battle_effects/custom_effect_overrides 无行 ③moves_lib 全 worktree 并集无注册。
//   13 载体全空参；无纯链（链长 2-5）——场景载体 38015 冲顶强袭 [2220,1285]
//   （1285=未击败减 PP，已实现、不碰 hp 断言）与 37882 灭神协议·袭 [881,2220,733,1218]
//   （特150；881/733/1218 全已实现且无降槽/未击败时休眠）——**同链零 parser 风险**
//   （J129 教训：未注册效果会被文本 parser 激活，全已注册则惰性）。
//   ⚠️ 工具教训：shell 的 grep 别名（ugrep）-q/-l 行为与 BSD grep 不同，实现状态
//   核查必须用 command grep 或 subprocess（本会话 1685 误报 ❌ 复盘）。
// 先例：2490（视图写=结算面切换）、2030（同通道）。
// 场景回归：test/scenario/scenario_2420_atk_take_max.cpp（2420 号段，物/特双向双钉）。

#include "common.h"

// ── 2220 —— 将自身攻击和特攻中最高的能力值作为自己的能力值进行攻击（零参）──
// int_args = [owner, target]（args_num=0，bind_participants 仍带 [owner,target]）。
EffectResult effect_2220_atk_take_max(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2220, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 2220, args,
                     "args_num=0：bind_participants 仍应带 [owner,target]");
    const int self = args.int_args[0];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && args.int_args[1] >= 0 && args.int_args[1] <= 1,
                     2220, args, "owner/target 越界");
    auto& attrs = ctx->ws.battle_attrs[self];
    const int pa = attrs[NumericalPropertyIndex::PHYSICAL_ATTACK];
    const int sa = attrs[NumericalPropertyIndex::SPECIAL_ATTACK];
    const int mx = std::max(pa, sa);
    if (pa < mx) {
        attrs[NumericalPropertyIndex::PHYSICAL_ATTACK] = mx;
    }
    if (sa < mx) {
        attrs[NumericalPropertyIndex::SPECIAL_ATTACK] = mx;
    }
    return EffectResult::kOk;
}

SKILL_EFFECT(2220, effect_2220_atk_take_max)
