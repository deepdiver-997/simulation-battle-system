// 异常施加双通道族：古早"命中后{0}%令对方XX"(10-16，主动毒通道)
// / 古代纯免(48) / 现代弹控(191) / 现代参数化施加(2189)。
//
// （2026-09-17 从 lib_1.cpp 按家族拆分；移动零改动，行为由全量场景回归守护。）
#include "common.h"

namespace {

EffectResult effect_10_ancient_paralysis(BattleContext* ctx, const EffectArgs& args) {
    apply_ancient_hit_status(ctx, args, AbnormalStatusId::Paralysis);
    return EffectResult::kOk;
}

EffectResult effect_11_ancient_poison(BattleContext* ctx, const EffectArgs& args) {
    apply_ancient_hit_status(ctx, args, AbnormalStatusId::Poison);
    return EffectResult::kOk;
}

EffectResult effect_12_ancient_burn(BattleContext* ctx, const EffectArgs& args) {
    apply_ancient_hit_status(ctx, args, AbnormalStatusId::Burn);
    return EffectResult::kOk;
}

EffectResult effect_14_ancient_frostbite(BattleContext* ctx, const EffectArgs& args) {
    apply_ancient_hit_status(ctx, args, AbnormalStatusId::Frostbite);
    return EffectResult::kOk;
}

EffectResult effect_15_ancient_fear(BattleContext* ctx, const EffectArgs& args) {
    apply_ancient_hit_status(ctx, args, AbnormalStatusId::Fear);
    return EffectResult::kOk;
}

EffectResult effect_114_ancient_flammable(BattleContext* ctx, const EffectArgs& args) {
    apply_ancient_hit_status(ctx, args, AbnormalStatusId::Flammable);
    return EffectResult::kOk;
}

// effect 48 - {N}回合内免疫所有受到的异常状态（古代层纯免；意志模组·怒 29025 等）。
// Args=[owner,target,rounds]。 Ancient 层：现代/古早施加都挡；纯免疫不反弹。
EffectResult effect_48_ancient_pure_immunity(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2] > 0 ? args.int_args[2] : 1;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(),
                        /*anomaly_mask=*/0, /*duration_rounds=*/rounds,
                        /*source_id=*/48, /*soul_immunity=*/false, EffectScope::ON_STAGE,
                        /*counts=*/0, /*source_effect_id=*/48,
                        /*tier=*/ImmunityTier::Ancient);
    return EffectResult::kOk;
}

// effect 191 - {N}回合内免疫并反弹所有受到的异常状态（现代弹控主流件）。
// Args=[owner,target,rounds]。 Modern 层 ANOMALY 票 + REFLECT 条目：
// 现代施加 → 免疫命中 → 反弹给施放方；古早施加（主动毒）→ 整体不可见（不免疫不反弹）。
EffectResult effect_191_modern_reflect(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2] > 0 ? args.int_args[2] : 1;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(),
                        /*anomaly_mask=*/0, /*duration_rounds=*/rounds,
                        /*source_id=*/191, /*soul_immunity=*/false, EffectScope::ON_STAGE,
                        /*counts=*/0, /*source_effect_id=*/191,
                        /*tier=*/ImmunityTier::Modern);
    ctx->rule_center_.grant_reflect(owner, /*source_slot=*/-1, /*source_effect_id=*/191,
                                    /*target=*/owner, /*rounds=*/rounds);
    return EffectResult::kOk;
}

// Helper：一方五技能 PP 总和（pp==-1 无限 PP 视为大数——"PP 值总和"语义下无限即极多）。
static int total_pp(const ElfPet& pet) {
    static constexpr int kUnlimitedPp = 99;
    int sum = 0;
    for (const Skills& s : pet.skills) {
        sum += (s.pp < 0) ? kUnlimitedPp : s.pp;
    }
    return sum;
}

// effect 2189 - 自身的PP值总和高于对手时{0}%令对手{1}（现代参数化施加；月下伏 37526）。
// Args=[owner,target,chance,anomaly]。现代通道：全检查链（会被弹控弹/被抗性抗/被转化）。
// 概率申报给原语（2026-09-22 概率闸门）：掷骰在亮节族闸门改写之后（见 common.h 同款注释）。
EffectResult effect_2189_pp_higher_anomaly(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core
        || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int chance = args.int_args[2];
    const int anomaly = args.int_args[3];
    if (owner < 0 || owner > 1 || target < 0 || target > 1
        || !is_valid_abnormal_status_id(anomaly)) {
        return EffectResult::kOk;
    }
    if (total_pp(ctx->getPet(owner)) <= total_pp(ctx->getPet(target))) {
        return EffectResult::kOk;  // 条件不满足：PP 总和不高于对手
    }
    g_core->apply_anomaly_checked(ctx, target, anomaly, random_anomaly_duration(), owner,
                                  chance, ChanceSource::Skill);
    return EffectResult::kOk;
}

// effect 16 —— 命中后{0}%令对手睡眠（119 个技能在用；古早施加族补员，爱心 10285、
// 幻化之火 20486 等）。走 apply_anomaly_ancient（主动毒通道：只被古代层免疫挡）。
EffectResult effect_16_ancient_sleep(BattleContext* ctx, const EffectArgs& args) {
    apply_ancient_hit_status(ctx, args, AbnormalStatusId::Sleep);
    return EffectResult::kOk;
}


// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表）──

    // ── 异常双通道验收件（2026-09-16，古早施加/古代纯免/现代弹控/现代参数化施加）──
    SKILL_EFFECT(10, effect_10_ancient_paralysis);    // 命中后{0}%令对方麻痹(古早;瞬雷天闪)
    SKILL_EFFECT(11, effect_11_ancient_poison);       // 命中后{0}%令对方中毒(古早族)
    SKILL_EFFECT(12, effect_12_ancient_burn);         // 命中后{0}%令对方烧伤(古早族)
    SKILL_EFFECT(14, effect_14_ancient_frostbite);    // 命中后{0}%令对方冻伤(古早族)
    SKILL_EFFECT(15, effect_15_ancient_fear);         // 命中后{0}%令对方害怕(古早族)
    SKILL_EFFECT(114, effect_114_ancient_flammable);  // 命中后{0}%概率令对方易燃(古早族)
    SKILL_EFFECT(48, effect_48_ancient_pure_immunity); // {N}回合内免疫所有异常(古代纯免)
    SKILL_EFFECT(191, effect_191_modern_reflect);     // {N}回合内免疫并反弹所有异常(弹控)
    SKILL_EFFECT(2189, effect_2189_pp_higher_anomaly); // PP总和高于对手时{0}%令对手{1}(月下伏)
    SKILL_EFFECT(16, effect_16_ancient_sleep);         // 命中后{0}%令对手睡眠(古早族)

}  // namespace
