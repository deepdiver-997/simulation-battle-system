// core_api() — 填充 Core→插件 的函数指针集合（真实原语实现地址）。
//
// ⚠️ 填充纪律（2026-09-19 治理，替代旧的位置初始化列表）：
//   - 全部**具名赋值**：槽声明顺序与赋值顺序解耦，往头文件中间插槽不再错位；
//   - 首次初始化跑 first_empty_slot 完整性检查：漏填当场 abort 报槽名，
//     不再是"插件侧判空 → 效果静默 no-op"的无声失败（旧位置列表时代曾连踩两次）。
//   新增槽三步：① core_api.h 声明（NSDMI 自动 = nullptr，可按家族插中间）
//              ② 下面具名赋值 ③ first_empty_slot 补一行检查。
#include <plugin/core_api.h>

#include <cstdio>
#include <cstdlib>

#include <primitives/battle_primitives.h>
#include <fsm/battleContext.h>
#include <effects/effect_meta.h>
#include <numerical-calculation/calculation.h>

namespace {
// 插件侧窗口家族查询：取不到 meta（未收录）→ 默认 InRounds。
EffectWindowKind effect_window_kind_impl(int effect_id) {
    const EffectMeta* meta = EffectMetaCatalog::instance().find(effect_id);
    return meta ? meta->window : EffectWindowKind::InRounds;
}

// 完整性检查：返回第一个未填充的槽名（全满返回 nullptr）。
// ⚠️ 新增槽必须在这里补一行——检查列表与赋值列表共同构成"所有槽已显式接线"的证据。
const char* first_empty_slot(const CoreApi& api) {
    if (!api.version) return "version";
    if (!api.apply_anomaly) return "apply_anomaly";
    if (!api.apply_anomaly_checked) return "apply_anomaly_checked";
    if (!api.apply_anomaly_ancient_checked) return "apply_anomaly_ancient_checked";
    if (!api.apply_anomaly_raw_checked) return "apply_anomaly_raw_checked";
    if (!api.break_round_effects) return "break_round_effects";
    if (!api.clear_stat_boosts) return "clear_stat_boosts";
    if (!api.transfer_stat_boosts) return "transfer_stat_boosts";
    if (!api.heal_amount) return "heal_amount";
    if (!api.grant_guaranteed_first) return "grant_guaranteed_first";
    if (!api.stat_change) return "stat_change";
    if (!api.stat_drop) return "stat_drop";
    if (!api.seal_skill) return "seal_skill";
    if (!api.stat_reversal) return "stat_reversal";
    if (!api.stat_boost_reversal) return "stat_boost_reversal";
    if (!api.fixed_damage) return "fixed_damage";
    if (!api.effect_window_kind) return "effect_window_kind";
    if (!api.restraint_multiplier) return "restraint_multiplier";
    if (!api.deal_pink_damage) return "deal_pink_damage";
    if (!api.pp_reduce) return "pp_reduce";
    if (!api.pp_zero_slot) return "pp_zero_slot";
    if (!api.pp_restore) return "pp_restore";
    if (!api.pp_restore_slot) return "pp_restore_slot";
    if (!api.pp_restore_points) return "pp_restore_points";
    if (!api.clear_stat_drops) return "clear_stat_drops";
    if (!api.deal_true_damage) return "deal_true_damage";
    if (!api.force_hp_to_zero) return "force_hp_to_zero";
    if (!api.apply_anomaly_ancient) return "apply_anomaly_ancient";
    if (!api.apply_anomaly_raw) return "apply_anomaly_raw";
    if (!api.drain_hp) return "drain_hp";
    if (!api.count_dead) return "count_dead";
    if (!api.reduce_max_hp_pct) return "reduce_max_hp_pct";
    if (!api.raise_max_hp_pct) return "raise_max_hp_pct";
    if (!api.defeat_pet) return "defeat_pet";
    if (!api.revive_pet) return "revive_pet";
    if (!api.vanish_spirit) return "vanish_spirit";
    if (!api.vanish_dead_spirits) return "vanish_dead_spirits";
    if (!api.register_death_interceptor) return "register_death_interceptor";
    if (!api.remove_death_interceptors) return "remove_death_interceptors";
    if (!api.reduce_max_hp_flat) return "reduce_max_hp_flat";
    if (!api.raise_max_hp_flat) return "raise_max_hp_flat";
    if (!api.deal_attribute_damage) return "deal_attribute_damage";
    if (!api.attach_random_anomalies) return "attach_random_anomalies";
    if (!api.reduce_active_anomaly_rounds) return "reduce_active_anomaly_rounds";
    if (!api.has_active_anomaly) return "has_active_anomaly";
    if (!api.get_abnormal_status_end_round) return "get_abnormal_status_end_round";
    if (!api.set_abnormal_status_end_round) return "set_abnormal_status_end_round";
    if (!api.dispel_active_anomalies) return "dispel_active_anomalies";
    if (!api.cure_anomalies) return "cure_anomalies";
    if (!api.deal_off_field_true_damage) return "deal_off_field_true_damage";
    if (!api.retrigger_entrance) return "retrigger_entrance";
    if (!api.set_anomaly_rounds) return "set_anomaly_rounds";
    if (!api.consume_shield) return "consume_shield";
    if (!api.clear_stat_boosts_as) return "clear_stat_boosts_as";
    if (!api.transfer_stat_boosts_as) return "transfer_stat_boosts_as";
    if (!api.is_spirit_king) return "is_spirit_king";
    if (!api.hit_effect_invalid) return "hit_effect_invalid";
    if (!api.reset_hp) return "reset_hp";
    return nullptr;
}
}  // namespace

const CoreApi& core_api() {
    static const CoreApi api = [] {
        CoreApi a;  // 全槽 NSDMI = nullptr（见 core_api.h 填充纪律）
        a.version = "1.0";
        a.abi_version = kPluginAbiVersion;  // ABI 身份戳：插件装载即核对（verify_plugin_abi）
        a.api_size = sizeof(CoreApi);
        // 旧槽 = **未申报概率**的适配器（chance_pct=-1 → 闸门不介入，行为与改造前一致）。
        // 插件迁到 *_checked 即受闸门管辖；不迁也不会错，只是漏管。
        a.apply_anomaly = [](BattleContext* ctx, int target, int anomaly_id, int duration_rounds,
                             int actor) {
            return apply_anomaly(ctx, target, anomaly_id, duration_rounds, actor,
                                 /*chance_pct=*/-1, ChanceSource::Skill);
        };
        a.apply_anomaly_checked = &apply_anomaly;
        a.apply_anomaly_ancient_checked = &apply_anomaly_ancient;
        a.apply_anomaly_raw_checked = &apply_anomaly_raw;
        a.break_round_effects = &break_round_effects;
        a.clear_stat_boosts = &clear_stat_boosts;
        a.transfer_stat_boosts = &transfer_stat_boosts;
        a.heal_amount = &heal_amount;
        a.grant_guaranteed_first = &grant_guaranteed_first;
        a.stat_change = &stat_change;
        a.stat_drop = &stat_drop;
        a.seal_skill = &seal_skill;
        a.hit_effect_invalid = &hit_effect_invalid;
        a.reset_hp = &reset_hp;
        a.stat_reversal = &stat_reversal;
        a.stat_boost_reversal = &stat_boost_reversal;
        a.fixed_damage = &fixed_damage;
        a.effect_window_kind = &effect_window_kind_impl;
        a.restraint_multiplier = &Calculation::calculateRestraintMultiples;
        a.deal_pink_damage = &deal_pink_damage;
        a.pp_reduce = &pp_reduce;
        a.pp_zero_slot = &pp_zero_slot;
        a.pp_restore = &pp_restore;
        a.clear_stat_drops = &clear_stat_drops;
        a.deal_true_damage = &deal_true_damage;
        a.force_hp_to_zero = &force_hp_to_zero;
        a.apply_anomaly_ancient = [](BattleContext* ctx, int target, int anomaly_id,
                                     int duration_rounds, int actor) {
            return apply_anomaly_ancient(ctx, target, anomaly_id, duration_rounds, actor,
                                         /*chance_pct=*/-1, ChanceSource::Skill);
        };
        a.apply_anomaly_raw = [](BattleContext* ctx, int target, int anomaly_id,
                                 int duration_rounds, int actor) {
            return apply_anomaly_raw(ctx, target, anomaly_id, duration_rounds, actor,
                                     /*chance_pct=*/-1, ChanceSource::Skill);
        };
        a.drain_hp = &drain_hp;
        // ── 精灵生命周期（存活/死亡/消逝）──
        a.count_dead = &count_dead;
        a.reduce_max_hp_pct = &reduce_max_hp_pct;
        a.raise_max_hp_pct = &raise_max_hp_pct;
        a.defeat_pet = &defeat_pet;
        a.revive_pet = &revive_pet;
        a.vanish_spirit = &vanish_spirit;
        a.vanish_dead_spirits = &vanish_dead_spirits;
        a.register_death_interceptor = &register_death_interceptor;
        a.attach_hp_consume_mark = [](BattleContext* c, int ms, int mslot, int ts, int tslot) {
            if (c) c->attach_hp_consume_mark(ms, mslot, ts, tslot);
        };
        a.suppress_extra_action = [](BattleContext* c, int owner) {
            if (c) c->suppress_extra_action(owner);
        };
        a.remove_death_interceptors = &remove_death_interceptors;
        // ── 点数版体力上限削减/提升（2026-09-17）──
        a.reduce_max_hp_flat = &reduce_max_hp_flat;
        a.raise_max_hp_flat = &raise_max_hp_flat;
        // ── 属性伤害（2026-09-18）──
        a.deal_attribute_damage = &deal_attribute_damage;
        // ── 随机异常附加（2026-09-19）──
        a.attach_random_anomalies = &attach_random_anomalies;
        a.reduce_active_anomaly_rounds = &reduce_active_anomaly_rounds;
        a.has_active_anomaly = [](BattleContext* c, int target, int status_id) {
            if (!c) {
                return false;
            }
            // K6 虚拟异常视图（查询时拉取）：扫描约定段，任一来源层数 >0 → 视为不处于异常。
            // 目标按方判定（荣光之裁等印记是 side 级）。内核结算面不走这里。
            for (auto it = c->plugin_storage.lower_bound(kAnomalyViewMaskPubBase);
                 it != c->plugin_storage.end() && it->first < kAnomalyViewMaskPubBase + 800000;
                 ++it) {
                const int side = it->first % 8;
                if (side == target) {
                    const int* v = std::any_cast<int>(&it->second);
                    if (v && *v > 0) {
                        return false;
                    }
                }
            }
            return c->has_active_abnormal_status(target, status_id);
        };
        a.get_abnormal_status_end_round = [](const BattleContext* c, int target, int status_id) {
            return c ? c->get_abnormal_status_end_round(target, status_id) : 0;
        };
        a.set_abnormal_status_end_round = [](BattleContext* c, int target, int status_id,
                                             int end_round) {
            if (c) {
                c->set_abnormal_status_end_round(target, status_id, end_round);
            }
        };
        a.dispel_active_anomalies = &dispel_active_anomalies;
        a.cure_anomalies = &cure_anomalies;
        a.deal_off_field_true_damage = &deal_off_field_true_damage;

        // ── 重触发登场行为（2026-09-20，莫塔里安 4541 击杀节点首用）──
        a.retrigger_entrance = [](BattleContext* c, int side, int slot) {
            if (!c || side < 0 || side > 1 || slot < 0 || slot > 5) {
                return;
            }
            c->seerRobot[side].elfPets[slot].soulMark.retrigger_entrance(c, side, slot);
        };
        a.set_anomaly_rounds = &set_anomaly_rounds;
        a.consume_shield = &consume_shield;
        a.pp_restore_slot = &pp_restore_slot;
        a.pp_restore_points = &pp_restore_points;
        a.clear_stat_boosts_as = &clear_stat_boosts_as;
        a.transfer_stat_boosts_as = &transfer_stat_boosts_as;
        a.is_spirit_king = [](const BattleContext* c, int side, int slot) -> bool {
            if (!c || side < 0 || side > 1 || slot < 0 || slot > 5) {
                return false;
            }
            return c->seerRobot[side].elfPets[slot].is_spirit_king;
        };

        if (const char* missing = first_empty_slot(a)) {
            std::fprintf(stderr,
                         "[core_api] 槽 %s 未填充——新增槽必须补：core_api.h 声明(自动 NSDMI) + "
                         "core_api.cpp 具名赋值 + first_empty_slot 检查\n",
                         missing);
            std::abort();
        }
        return a;
    }();
    return api;
}
