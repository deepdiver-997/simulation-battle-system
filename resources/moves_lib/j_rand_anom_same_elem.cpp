// J 组 J88 · 随机异常/同系威力族 —— 1291×15 / 179×18（2026-10-03，worktree ../agents/j88）
//
// 家族语义（effect_info 官方模板即权威）：
//   1291（arg_count=3）"造成的伤害高于{0}则{1}%随机附加{2}种异常状态"
//   179（arg_count=1）"若属性相同则技能威力提升{0}"
//     ⚠️ 179 不在本文件：核心 effect_unit_parser.cpp:145 已内建（SameElement+PowerBoost
//     单元）——clone_effect 对 admitted 单元返回空 → 走解析器注册通用执行器。插件侧
//     再注册 SKILL_EFFECT(179) 无效且可被解析路径遮蔽（本次实证）。registry 不计
//     unit 解析效果 → "179 未实现"系盘点假阳性（J51 记档实为冗余）。
//
// 实现口径：
//   - 1291 = DETECT 攻方桶恒久回调（1088 形，模板无窗口）：红伤 final 严格 > {0} →
//     attach_random_anomalies 原语（anomaly_pool_all() 全池、count={2}、
//     probability_pct={1}——概率申报给原语，亮节闸门管辖，batch_a 1367 同款）。
//   - 179 = J51 记档转正（该行有施工图）：同系判据 = 技能静态系别
//     （roundChoice → skills[idx].element，785 防自污染口径）与自身属性视图
//     （ws.view_elementalAttributes，属性反转类改写后读视图才对）相交 →
//     skill_power_view += view×{0}/100（845 变威力 102 通道，J27 直系）。
//     view>0 才写（845 同守卫；威力 0 的技能乘算无意义）。
//
// 载体切片（unity 库 json_each 精确匹配 15/18=33；顺序切分 side_effect.arg_count
// 全载体自洽、零剩余，脚本核算 2026-10-03）：
//   1291 [阈值,概率,种数]：{300,100,1}×10/{300,100,2}×2/{300,70,1}×2/{300,80,1}
//     （纯链载体 禁灵之咒 34079 / 荒古石像 34998 均 [300,100,1]）
//   179 [pct]：{20}/{30}×2/{50}×2/…/{470} 全分散（纯链 7 条：机械能量炮 12981 [20]、
//     仙气环绕 32220 [30] 等）
//
// 动工前置三查（2026-10-03 本 worktree 复核）：①内核 grep 1291/179 无硬编码
// ②effect_meta 无行 ③moves_lib/custom overrides 无注册——真缺口。
// 先例：1088/729（DETECT 攻方桶恒久回调，J15）、attach_random_anomalies +
//   anomaly_pool_all（batch_a 1367）、1616（技能静态系别读法，J17）、845（变威力
//   写入，J27）、785（属性视图读法）。
// 场景回归：test/scenario/scenario_1780_rand_anom_same_elem.cpp（1780 号段，一文件两宏）。

#include "common.h"

namespace {

// ── 1291 —— 造成的伤害高于{0}则{1}%随机附加{2}种异常状态 ──
// int_args = [owner, target, threshold, chance, count]（args_num=3）。
EffectResult effect_1291_threshold_rand_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1291, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1291, args,
                     "1291 布局=[owner,target,阈值,概率,种数]");
    const int owner = args.int_args[0];
    const int threshold = args.int_args[2];
    const int chance = args.int_args[3];
    const int count = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && args.int_args[1] >= 0 && args.int_args[1] <= 1,
                     1291, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(threshold > 0 && chance > 0 && count > 0, 1291, args,
                     "需要 阈值/概率/种数 > 0");
    if (!g_core || !g_core->attach_random_anomalies) {
        return EffectResult::kOk;
    }
    const int opp = args.int_args[1];
    ctx->register_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner, opp, threshold, chance, count](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner || !g_core
                || !g_core->attach_random_anomalies) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || d.defenderId != opp) {
                return;
            }
            if (!d.isRed || d.final <= threshold) {
                return;   // 红伤且严格大于阈值（1088/729 同判据）
            }
            (void)g_core->attach_random_anomalies(c, opp, anomaly_pool_all(), count,
                                                  chance, owner, ChanceSource::Skill);
        });
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1291, effect_1291_threshold_rand_anomaly)
