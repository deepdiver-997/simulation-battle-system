// G 组 G3 · 条件增伤族：999×36 / 1360×27 / 609×28（2026-09-29，worktree ../agents/g3）
//
// 效果与载体（看板 G3 行；DB 切片 moves.side_effect/side_effect_arg，side_effect.arg_count
// 切分，2026-09-29 查库；4399 原文 reference/scrape_4399/out/pets_full.jsonl 交叉验证）：
//   999×36 "自身每处于{0}种能力提升状态，造成的伤害额外提升{1}%"（args_num=2，切片 [1,10]）
//     载体：异次元杀阵 19782（斯塔奥 3154，链 [521,999]——521 反转自身下降已实现，
//           本体无下降时 no-op）、圣诞夜光破 30394（圣诞雪姬 3294，链 [999]，必中）。
//     4399 双载体："自身每处于1种能力提升状态，造成的伤害提升10%"。
//   1360×27 "自己出手时若自身体力高于对手则造成的伤害提升{0}%"（args_num=1，切片 [50]）
//     载体：綪·笙夜凉歌 32280（天启星魂 3661，链 [928,1360,773]——928/773 已实现：
//           消回合→免异常（对手无回合类则 no-op）/体力低于才互换（高于分支不触发））、
//           白魔法之语 32464（妙法 3701，链 [1179,1360,1230]——1179 消成功才增伤
//           （对手无回合类则 no-op）、1230 未实现链上静默跳过）。
//     4399 双载体："自己出手时若自身体力高于对手则造成的伤害提升50%"。
//   609×28 "若对手{0}，技能威力翻倍"（args_num=1，切片 [1]）
//     载体：幻魔空牙冲 10373（萨格罗斯 125/萨罗德 2470 等，链 [180,609]——180 消对手
//           回合类已实现，对手无回合类则 no-op）、神奇韭菜 16231（五仁王中王 2494，
//           链 [609] 纯净）。
//     ⚠️ {0} = **官方异常码**（常识速查 §三：0麻痹 1中毒 2烧伤…）：两载体 4399 逐字
//     "若对手中毒（，）则技能威力翻倍" ↔ 切片 [1] → 判据 = 对手处于**指定**异常
//     （has_active_anomaly，671 同查询面；不是"任一异常"，671 才是任一）。
//
// 判据与先例：
//   - 999 计数 = 自身 ability_levels 六槽（0攻..5命中，含命中、负等级不算）**正等级槽数**
//     （F5 1472 count_boost_kinds_f5 / 3404 同式；{0}=1 即"每种"计数）。
//     增伤通道：effect_info 模板带"**额外**提升"二字 → 官方 L352 **非通用增伤 = 乘法**
//     （AMP_EXTRA damage_add_extra_mul，693 同判据——693 的 4399 转述同样省略"额外"，
//     仓库口径 **effect_info 模板文本即权威**）。孤立看数值与加算通道相同
//     （final×(100+N)/100），组合语义才分家：多个来源时乘法通道各自连乘。
//     档位 = (计数/{0})×{1}（"每处于{0}种…提升{1}%"；全库切片 {0}≡1，整数除法退化直乘）。
//   - 1360 "体力高于对手" = **严格 >**（F8 132 "低于"同判据的镜像；相等不算高于）。
//     模板无"额外" → 通用增伤加算通道 ws.damage_add_pct[owner]（1472 同款；
//     SKILL_EFFECT 直写、ws 每回合 reset，本效果只管本次攻击，无窗口）。
//   - 609 条件成立 → ws.skill_power_view[owner] = 现值×2（F8 132 同款物化：赋值自动置
//     rewritten → 标准变威力重算管线在伤害开跑前以双倍威力重算并覆盖第一次）。
//     视图在 SKILL_EFFECT 时点已由 resolve_skill_execution materialize 成 skill.power，
//     value>0 才翻倍（132 同守卫；威力 0 的属性技无红伤可翻，跳过）。
//   - ⚠️ 通道方向纪律（F6 817 教训）：三条都是**己方造成**侧——999/1360 写
//     damage_*_mul/pct[owner]、609 写 skill_power_view[owner]，不碰受击侧槽。
//
// 动工前三查（2026-09-29）：①内核 grep 无 999/1360/609 硬编码（仅 -999 哨兵等噪声）
// ②custom_effect_overrides 无这三个 effect_id 行（默认窗口族判定不牵连；三者均
// SKILL_EFFECT 即时型、无窗口声明需求）③moves_lib 无既有注册（SKILL_EFFECT 零命中）
// ——真缺口。
//
// 场景回归：test/scenario/scenario_730_cond_boost.cpp（730 号段，一文件覆盖三效果）。

#include "common.h"

namespace {

// ── "自身处于能力提升状态"的种数（F5 count_boost_kinds_f5 同判据：本体六槽正等级，
// 含命中、负不算；插件不跨 .so 复制，本地持私有副本）──
int count_boost_kinds_g3(const BattleContext* ctx, int owner) {
    int n = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[owner][i] > 0) {
            ++n;
        }
    }
    return n;
}

// ── 999 —— 自身每处于{0}种能力提升状态，造成的伤害额外提升{1}% ──
// int_args = [owner, target, {0}=每档种数, {1}=提升百分点]（build_effect_args_for_skill
// 固定前置 [owner,target]；切片 [1,10]）。
// 计数×{1}/{0} → damage_add_extra_mul[owner] 第一个空槽（通用 common.h helper 同款
// 抢槽法；"额外"字样 = 非通用乘法通道，见文件头）。无提升 → 不写槽（对照可断 loss==raw）。
EffectResult effect_999_per_boost_kind_damage_up(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 999, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 999, args,
                     "需要 [owner,target,每档种数,提升百分点]");
    const int owner = args.int_args[0];
    const int per_kinds = args.int_args[2];
    const int pct = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 999, args, "owner 越界");
    EFFECT_ARG_GUARD(per_kinds > 0 && pct > 0, 999, args, "需要 每档种数>0 且 百分点>0");
    const int kinds = count_boost_kinds_g3(ctx, owner);
    if (kinds <= 0) {
        return EffectResult::kOk;   // 无提升状态：不增伤
    }
    const int bonus = kinds / per_kinds * pct;
    if (bonus <= 0) {
        return EffectResult::kOk;   // 未满一档（{0}>1 且计数不足时）
    }
    int(&slots)[4] = ctx->ws.damage_add_extra_mul[owner];
    for (int i = 0; i < 4; ++i) {
        if (slots[i] == 0) {
            slots[i] = bonus;
            break;
        }
    }
    return EffectResult::kOk;
}

// ── 1360 —— 自己出手时若自身体力高于对手则造成的伤害提升{0}% ──
// int_args = [owner, target, {0}=提升百分点]（切片 [50]）。
// 严格 hp >（132 "低于"镜像判据）；无"额外" → 通用增伤加算 ws.damage_add_pct[owner]。
EffectResult effect_1360_higher_hp_damage_up(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1360, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1360, args,
                     "需要 [owner,target,提升百分点]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int pct = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner,
                     1360, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(pct > 0, 1360, args, "需要 百分点 > 0");
    if (ctx->getPet(owner).hp <= ctx->getPet(opp).hp) {
        return EffectResult::kOk;   // 未"高于对手"（严格）：不增伤
    }
    ctx->ws.damage_add_pct[owner] += static_cast<float>(pct);
    return EffectResult::kOk;
}

// ── 609 —— 若对手{0}，技能威力翻倍 ──
// int_args = [owner, target, {0}=官方异常码]（切片 [1]=中毒；4399 双载体逐字佐证）。
// 条件 = 对手处于**指定**异常（has_active_anomaly，671 同查询面、带 K6 虚拟异常视图）；
// 成立 → skill_power_view[owner] ×2（132 同款：赋值自动置 rewritten → 变威力重算）。
EffectResult effect_609_foe_status_power_double(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 609, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 609, args,
                     "需要 [owner,target,官方异常码]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int status_id = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner,
                     609, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(status_id >= 0 && status_id <= kOfficialAbnormalStatusMaxId, 609, args,
                     "官方异常码越界（0..43）");
    if (!g_core || !g_core->has_active_anomaly) {
        return EffectResult::kOk;
    }
    if (!g_core->has_active_anomaly(ctx, opp, status_id)) {
        return EffectResult::kOk;   // 对手不处于指定异常：威力不翻
    }
    auto& view = ctx->ws.skill_power_view[owner];
    if (view.value > 0) {
        view = view.value * 2;   // 置 rewritten → 标准变威力重算管线（132/1443 同路）
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(999, effect_999_per_boost_kind_damage_up);
SKILL_EFFECT(1360, effect_1360_higher_hp_damage_up);
SKILL_EFFECT(609, effect_609_foe_status_power_double);
