// J 组 J15 · 伤害阈值门族 —— 3 宏/71 载体（2026-10-02，worktree ../agents/j15）
//
// 家族语义（effect_info 模板即权威）：全部"伤害阈值门"——伤害量跨过门槛才触发后缀。
//   729  [4] {0}回合内若造成伤害大于{1}则{2}%概率使对手{3}
//   775  [2] {0}回合内若受到的伤害大于{1}，则恢复自身全部体力
//   1088 [2] 造成的攻击伤害高于{0}则吸取对手最大体力的1/{1}
//
// 实现口径（J15 工单行 + 1236/21/597 先例）：
//   - 729 = DETECT 相位回调（管线 BLOCK→DETECT，**攻击方桶**读 final——1236 圣光吟诵的
//     对称位：同一阶段、方向相反）。窗口"{0}回合内"= InRounds（round_effect_start_round
//     折算，后手不少算）；严格大于 {1}；异常对象 = **当次攻击的防御方**；{2}% 申报制
//     （apply_anomaly_checked，亮节闸门管辖）。红伤触发（d.isRed，属性技无伤害相位自然
//     不触发——23909 天纵鬼才等属性技载体=纯窗，靠后续攻击技结算）。
//   - 775 = EVENT_TAKE_DAMAGE watcher（462/580/21 同通道，WatcherScope::TEAM 跨回合）。
//     ⚠️ 不用 DETECT 防守桶：DETECT 时红伤已算出但**尚未扣血**（1236 注），"受到的伤害…
//     则回满"要**落地之后**再回满——TAKE_DAMAGE 在扣血后发，ev.amount 即实扣量（E12 字段）。
//     量判 = ev.amount > {1}（严格大于）；伤害类别不设限（模板无"攻击"字样——21 的
//     NORMAL 窄口径系其"受攻击"字面，此处按宽口径记档）；门槛 200~400 段下小粉伤 tick
//     自然够不着。回满 = heal_max_hp_fraction(ctx, owner, 1)（1361 同款，吃封回血/修正）。
//   - 1088 = DETECT 攻方桶恒久回调（模板无窗口）：红伤 final > {0} →
//     drain_hp(actor=owner, target=防御方, denom={1})——597 吸取口径：对手掉
//     最大体力 1/{1} 固定伤、自身回等量。
//
// 载体切片（unity 库 json_each 精确匹配 22+24+25=71；side_effect.arg_count 切分全载体
// 自洽、零剩余，脚本核算 2026-10-02）：
//   729  [rounds,阈值,概率,异常码]：30001 千星死魂灭 [1,300,100,6]（chance=100 测试载体）、
//        23909 天纵鬼才 [5,300,100,6]（属性技=纯窗）
//   775  [rounds,阈值]：25119 鬼哭神嚎 [3,200]、17697 创世雷神决 [2,300]
//   1088 [阈值,分母]：36047 贯焱击 [300,5]、30351 凶年风水煞 [300,3]
//
// 动工前置三查（2026-10-02）：①内核 grep 3 id 无硬编码（GUARD_DETECT 是犀牛魂印链
// 专属相位，与本族无涉）②custom_effect_programs/overrides 无声明 ③moves_lib 无既有
// 注册——真缺口。
// 场景回归：test/scenario/scenario_1000_damage_threshold.cpp（号段 1000-1009）。

#include "common.h"

namespace {

// ── 729 —— {0}回合内若造成伤害大于{1}则{2}%概率使对手{3} ──
// int_args = [owner, target, rounds, threshold, chance, anomaly]（args_num=4）。
EffectResult effect_729_dealt_threshold_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 729, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 729, args,
                     "729 布局=[owner,target,回合,阈值,概率,异常码]");
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    const int threshold = args.int_args[3];
    const int chance = args.int_args[4];
    const int anomaly = args.int_args[5];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && args.int_args[1] >= 0 && args.int_args[1] <= 1,
                     729, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && threshold > 0 && chance > 0, 729, args,
                     "需要 回合/阈值/概率 > 0");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 729, args, "需要 异常码 ∈ [0,43]");
    const int start = ctx->round_effect_start_round(owner, rounds, EffectWindowKind::InRounds);
    const int expire = start + rounds;
    ctx->register_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner, start, expire, threshold, chance, anomaly](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner) {
                return;
            }
            if (c->roundCount < start || c->roundCount >= expire) {
                return;   // "{0}回合内"（InRounds，1236 同款窗自检）
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || d.defenderId < 0 || d.defenderId > 1) {
                return;
            }
            if (!d.isRed || d.final <= threshold) {
                return;   // 红伤且严格大于阈值（属性技无红伤自然不触发）
            }
            if (g_core && g_core->apply_anomaly_checked) {
                (void)g_core->apply_anomaly_checked(c, d.defenderId, anomaly,
                                                    random_anomaly_duration(), owner, chance,
                                                    ChanceSource::Skill);
            }
        });
    return EffectResult::kOk;
}

// ── 775 —— {0}回合内若受到的伤害大于{1}，则恢复自身全部体力 ──
// int_args = [owner, target, rounds, threshold]（args_num=2）。
EffectResult effect_775_taken_threshold_heal_full(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 775, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 775, args,
                     "775 布局=[owner,target,回合,阈值]");
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    const int threshold = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && args.int_args[1] >= 0 && args.int_args[1] <= 1,
                     775, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && threshold > 0, 775, args, "需要 回合/阈值 > 0");
    // 窗口 InRounds 含当回合；跨回合 watcher 必须 TEAM scope（常识速查 §五）。
    const int start = ctx->round_effect_start_round(owner, rounds, EffectWindowKind::InRounds);
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, start, rounds, /*once=*/false,
        [owner, threshold](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != owner || ev.amount <= threshold) {
                return;   // 受到的伤害（实扣量）严格大于阈值
            }
            (void)heal_max_hp_fraction(c, owner, 1);   // 恢复自身全部体力
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 1088 —— 造成的攻击伤害高于{0}则吸取对手最大体力的1/{1} ──
// int_args = [owner, target, threshold, denom]（args_num=2）；恒久（模板无窗口）。
EffectResult effect_1088_dealt_threshold_drain(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1088, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1088, args,
                     "1088 布局=[owner,target,阈值,分母]");
    const int owner = args.int_args[0];
    const int threshold = args.int_args[2];
    const int denom = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && args.int_args[1] >= 0 && args.int_args[1] <= 1,
                     1088, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(threshold > 0 && denom > 0, 1088, args, "需要 阈值/分母 > 0");
    ctx->register_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner, threshold, denom](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || d.defenderId < 0 || d.defenderId > 1) {
                return;
            }
            if (!d.isRed || d.final <= threshold) {
                return;   // "造成的攻击伤害高于{0}"（严格大于）
            }
            if (g_core && g_core->drain_hp) {
                g_core->drain_hp(c, /*actor=*/owner, /*target=*/d.defenderId, denom);
            }
        });
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(729, effect_729_dealt_threshold_anomaly)
SKILL_EFFECT(775, effect_775_taken_threshold_heal_full)
SKILL_EFFECT(1088, effect_1088_dealt_threshold_drain)
