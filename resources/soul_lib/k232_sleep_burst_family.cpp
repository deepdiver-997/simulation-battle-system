// K232 睡眠/自强/消回合/吸血复合族 —— effect 834（1 宏 / 2 行）
//（王·缪斯 3205；author k-prod-5 2026-10-08 立单，号段 3760-3769）
//
// 行清单（tools/soul_effects_dump.py 834，2026-10-08）：
//   row535 args[35,50,3,1]「攻击技能35%使对手睡眠，未触发则自身特攻+1、速度+1且有50%的
//          概率消除对手回合类效果；攻击技能命中睡眠中的对手时，附加自身最大体力1/3的
//          固定伤害（boss无效）」
//   row548 args[40,60,3,1] 同上 40%/60%（王·缪斯超能传承魂印进化）
//   行差 = db[0]（睡眠概率）与 db[1]（消回合概率）随行变；db[2]=吸血分母（恒 3）；
//   db[3] 两行恒 1、无对应子句——不消费，记档。
//
// 实现：
//   ①+② EVENT_SKILL_USED 窗（K399 同窗：own + used_attribute 属性技排除；"使用即触发，
//      miss/被盔也算使用"——K399/K179 口径记档）→
//      apply_anomaly_checked(睡眠8, random_anomaly_duration, own, db[0], SoulMark)
//      （申报通道，闸门管辖；K172 808 同形）→ ROLL_FAILED（仅此档，免疫/抗性不补偿）
//      = "未触发" → stat_change(特攻槽1,+1)+stat_change(速槽4,+1) + roll_percent(db[1])
//      → break_round_effects(对手)（K399 三态原语，返回值不分支）。
//   ③ BATTLE_FIRST/SECOND_ON_SKILL_HIT 桶节点：mover（FIRST=resolve first mover，
//      SECOND=其对位；preemptive_right 映射同 battleFsm resolve_first_mover_id）==own
//      && ws.skill_type_view[mover]!=Attribute（本时点已由 resolve_skill_execution 物化）
//      && ws.skill_exec_result[mover]!=SKILL_INVALID（miss/被盔/被封不算命中——
//      anomaly_on_attack_hit_hook 同判据）&& 对手仍在睡（has_active_anomaly 睡眠8）
//      && self.hp>0 → deal_pink_damage(victim, 1/db[2] maxHP, FIXED, own)。
//      ⚠️ 时点记档：内核醒睡钩子 anomaly_on_attack_hit_hook 在 ON_SKILL_HIT 桶执行
//      **之后**跑（battleFsm handle_*OnSkillHit：桶 → 特性钩子 → 醒睡）——"命中即醒"
//      （SOP §三.2）在本时点尚未发生，桶内读"受击方仍在睡"是官方"命中睡眠中的对手"
//      的正确观察面；命中判据同钩子（SKILL_INVALID 不算）。
// boss 无效标注：tips 尾注 → 全节点 boss_invalid。
// ⚠️ 三查记录（author k-prod-5 2026-10-08）：①内核无 834 魂印硬编码；②effect_info 834
//    同号模板不同文（args_num=2 错位行——SOP §六.1，按行 tips 落地）；③soul_lib/registry
//    无 834 注册（registry soul=172 复核缺席）——真缺口。
// 场景回归：test/scenario/scenario_3760_sleep_burst_family.cpp（号段 3760-3769）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 834;
constexpr int kWiredKeyBase = 990760300;   // +owner：watcher 已挂旗标

struct K232Runtime {
    bool watcher_armed = false;
};

K232Runtime& k232_runtime(BattleContext* ctx, int owner) {
    auto& storage = ctx->getPet(owner).soulmark_storage;
    if (!storage.count(kMarkId)) {
        storage[kMarkId] = K232Runtime{};
    }
    return std::any_cast<K232Runtime&>(storage[kMarkId]);
}

// 行参解码（db[0..1] 随行变，原序读 owned_int_args；越界/非法回退行1缺省 [35,50,3]）。
struct K232Row {
    int sleep_pct = 35;
    int break_pct = 50;
    int drain_denom = 3;
};

K232Row k232_read_row(BattleContext* ctx, int owner) {
    K232Row row;
    const int slot = ctx->find_pet_with_soulmark(owner, kMarkId);
    if (slot < 0 || slot > 5) {
        return row;
    }
    const auto& db = ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    if (db.size() > 0 && db[0] > 0) {
        row.sleep_pct = db[0];
    }
    if (db.size() > 1 && db[1] > 0) {
        row.break_pct = db[1];
    }
    if (db.size() > 2 && db[2] > 0) {
        row.drain_denom = db[2];
    }
    return row;
}

// ── ①+② 使用攻击技：概率睡眠 → 未触发自强+概率消回合 ─────────────────────────

EffectResult k232_834_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    K232Runtime& rt = k232_runtime(ctx, owner);
    if (rt.watcher_armed) {
        return EffectResult::kOk;
    }
    if (!g_core->apply_anomaly_checked || !g_core->stat_change
        || !g_core->break_round_effects) {
        return EffectResult::kOk;
    }
    rt.watcher_armed = true;
    const K232Row row = k232_read_row(ctx, owner);
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, row](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || !g_core || ev.actor != owner || ev.used_attribute) {
                return;   // 仅自己的攻击技（声明类型，K399 同窗）
            }
            if (!onstage_has_soul_mark(wctx, owner, kMarkId)) {
                return;
            }
            const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
                wctx, wctx->opponent(owner),
                static_cast<int>(AbnormalStatusId::Sleep),
                random_anomaly_duration(), owner, row.sleep_pct,
                ChanceSource::SoulMark);
            if (r != ApplyAnomalyResult::ROLL_FAILED) {
                return;   // 睡眠已施加/免疫/抗性——"未触发"仅 ROLL_FAILED（K172 口径）
            }
            // 未触发 → 自身特攻+1、速度+1（槽序：1=特攻、4=速）。
            (void)g_core->stat_change(wctx, owner, 1, 1);
            (void)g_core->stat_change(wctx, owner, 4, 1);
            if (roll_percent(row.break_pct)) {
                (void)g_core->break_round_effects(wctx, wctx->opponent(owner));
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── ③ 命中睡眠中的对手 → 附加 1/N 最大体力固定伤 ─────────────────────────────

EffectResult k232_834_on_hit(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !g_core || !g_core->deal_pink_damage
        || !g_core->has_active_anomaly) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    // mover：FIRST 桶=先手方、SECOND 桶=后手方（preemptive_right 映射同内核）。
    const bool second = ctx->currentState == State::BATTLE_SECOND_ON_SKILL_HIT;
    int mover = ctx->preemptive_right == PreemptiveRight::SEER_ROBOT_2 ? 1 : 0;
    if (second) {
        mover = 1 - mover;
    }
    if (mover != owner) {
        return EffectResult::kOk;   // 只认自己的攻击命中
    }
    if (ctx->ws.skill_exec_result[mover] == SkillExecResult::SKILL_INVALID) {
        return EffectResult::kOk;   // miss/被盔/被威/被封不算命中（内核钩子同判据）
    }
    if (ctx->ws.skill_type_view[mover] == static_cast<int>(SkillType::Attribute)) {
        return EffectResult::kOk;   // 官方限定攻击技能
    }
    const int victim = 1 - owner;
    if (!g_core->has_active_anomaly(ctx, victim,
                                    static_cast<int>(AbnormalStatusId::Sleep))) {
        return EffectResult::kOk;   // 命中时对手不在睡（醒睡钩子在桶后，本时点仍未醒）
    }
    ElfPet& self = ctx->getPet(owner);
    if (self.hp <= 0) {
        return EffectResult::kOk;
    }
    const K232Row row = k232_read_row(ctx, owner);
    const int max_hp = resolve_pet_max_hp(self);
    const int amount = row.drain_denom > 0 ? max_hp / row.drain_denom : 0;
    if (amount > 0) {
        (void)g_core->deal_pink_damage(ctx, victim, amount, DamageKind::FIXED, owner);
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（834 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k232_834_arm,
                        /*once=*/false, /*early=*/true, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_FIRST_ON_SKILL_HIT, &k232_834_on_hit,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_SECOND_ON_SKILL_HIT, &k232_834_on_hit,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true)});
