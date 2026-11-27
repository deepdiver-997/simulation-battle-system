// J334 · 效果 410「{0}%恢复自身1/{1}体力值」——概率按比例回血
//（场景 test/scenario/scenario_8030..8031；号段 8030-8039）
//
// ══ 三查（2026-11-06，基线 main-local fa27a647）══
// ① 内核硬编码：`(^|[^0-9.])410([^0-9.]|$)` 对 src/ include/ 0 命中；
// ② effect_meta：effect_info 410 args_num=2 = side_effect.arg_count=2（切片一致，
//    默认参 [15,4]；逐载体：[15,4]×5、[30,3]×1、[50,2]×1）；
// ③ 全库先例：直接三件套——
//    · 概率掷点 roll_percent（common.h:102，100% 短路不耗抽取，「短路径不消耗
//      rng.next()」纪律，heal_drain.cpp:298 同注）；
//    · 比例回血 heal_max_hp_fraction（common.h:320，max_hp/denom 整除、返回实际量、
//      吃封回血/恢复修正；1239「恢复自身最大体力的1/{0}」与 43 同式）；
//    · 死亡封闭由 heal_amount 原语兜底（pet_death_notified no-op，2026-10-01 D2）。
//
// ══ 语义口径 ══
// 「{0}%恢复自身1/{1}体力值」= 受击无关的**使用时点**回复：本次技能命中效果趟
// （SKILL_EFFECT）掷 P% → 成功则恢复自身最大体力 1/denom（整除）。无窗、无跨回合
// 状态、无 pending——七模式 Q0-Q7 全「本回合使用时点一笔」，直接原语组合即收敛。
// 官方模板无异常码/无 boss 尾注（Q1 已核 analyze 原文）→ 不加任何门。
//
// ══ 审计槽（无随机精确断言口径）══
// 键段 = 1,004,100,000（10^9 + 410×10^4）+ owner×16 + slot：
//   slot 0  申报 P / slot 1 申报 denom / slot 2 掷点结果（1 过 0 拒 -1 未耗抽取）
//   slot 3  名义治疗量（max_hp/denom 整除）
//   slot 4  实际治疗量（heal_max_hp_fraction 返回值）
//   slot 5  过闸治疗次数累计
//   slot 6  原始掷点 r（-1 = 未耗抽取；0<=P<100 恰耗一抽）
// 新增前已跑 tools/check_storage_keys.py 全库无撞（J334 动工记录）。
#include "common.h"

#include <utils/sbs_rand.h>

namespace {

constexpr int k410AuditBase = 1004100000;

inline int key410(int owner, int slot) {
    return k410AuditBase + owner * 16 + slot;
}

int storage_int_local(BattleContext* ctx, int key, int def = 0) {
    const auto it = ctx->plugin_storage.find(key);
    if (it == ctx->plugin_storage.end()) {
        return def;
    }
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : def;
}

void set_storage_int_local(BattleContext* ctx, int key, int value) {
    ctx->plugin_storage[key] = value;
}

}  // namespace

// ═══════════════════════════════════════════════════════════════════════════
// 410 —— {0}%恢复自身1/{1}体力值
// 行参布局 [own, opp, P, denom]（DB 两参 {0}=P {1}=denom；切片 arg_count=2，
// 例：13292 星光奔腾击 [15,4] / 32389 雪花治愈 [30,3] / 17603 闪耀光束波 [50,2]）。
// P=0 永不触发；denom<=0 / P 越界 [0,100] 守卫 no-op（族守卫口径）。
// ═══════════════════════════════════════════════════════════════════════════
EffectResult effect_410_prob_heal_fraction(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int chance = args.int_args[2];
    const int denom = args.int_args[3];
    if (owner < 0 || owner > 1 || chance < 0 || chance > 100 || denom <= 0) {
        return EffectResult::kOk;
    }
    const int nominal = max_hp_of(ctx, owner) / denom;
    set_storage_int_local(ctx, key410(owner, 0), chance);
    set_storage_int_local(ctx, key410(owner, 1), denom);
    set_storage_int_local(ctx, key410(owner, 3), nominal);

    // 入口 P% 掷点（roll_percent 同式展开以申报 r；P<=0/P>=100 不耗抽取）
    bool pass = false;
    if (chance <= 0) {
        set_storage_int_local(ctx, key410(owner, 6), -1);
    } else if (chance >= 100) {
        pass = true;
        set_storage_int_local(ctx, key410(owner, 6), -1);
        set_storage_int_local(ctx, key410(owner, 2), 1);
    } else {
        const int r = choose_skill_range(*ctx, 410, 0, owner, owner, 0, 99);
        set_storage_int_local(ctx, key410(owner, 6), r);
        pass = r < chance;
        set_storage_int_local(ctx, key410(owner, 2), pass ? 1 : 0);
    }
    if (!pass) {
        set_storage_int_local(ctx, key410(owner, 4), 0);   // 拒绝置 0 不遗留
        return EffectResult::kOk;
    }
    set_storage_int_local(ctx, key410(owner, 5),
                          storage_int_local(ctx, key410(owner, 5), 0) + 1);
    // 生存门（一致性口径；死亡登记封闭由 heal_amount 原语兜底）
    if (ctx->getPet(owner).hp <= 0) {
        set_storage_int_local(ctx, key410(owner, 4), 0);
        return EffectResult::kOk;
    }
    const int actual = heal_max_hp_fraction(ctx, owner, denom);
    set_storage_int_local(ctx, key410(owner, 4), actual);
    return EffectResult::kOk;
}

SKILL_EFFECT(410, effect_410_prob_heal_fraction);
