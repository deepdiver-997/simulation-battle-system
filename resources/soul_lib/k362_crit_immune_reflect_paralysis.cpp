// K362 暴击免疫反弹+瘫痪分支族 —— effect 786（1 宏 / 2 行）
//
// 行清单（tools/soul_effects_dump.py 786，2026-10-24）：
//   786 英灵剑圣·兰斯洛特 3134 两行（kind=[5,6,10]，row582 come=魂印进化活动）：
//     row476 args[500,2,50,19,1]：「免疫并反弹致命一击伤害，触发后自身恢复500点体力
//       且下回合自身先制+2（boss无效）；|能力提升状态被消除则对手瘫痪，未触发瘫痪则
//       消除对手回合类效果（boss无效）」
//     row582 args[600,3,50,19,1]：同文，恢复 600/先制+3（进化行）
//   行参解码（db_at 原序，与 tips 数值子句一一自证）：
//     db[0]=触发回血 N（500/600） db[1]=下回合先制 M（2/3） db[2]=瘫痪概率（50）
//     db[3]=瘫痪异常码（19=Crippled 瘫痪；官方码，非 Paralysis=0 麻痹）
//     db[4]=boss 无效旗（1）——节点级 boss_invalid=true 承载（SoulMarkNodeRef 标注线），
//     db 旗不另消费（K180 同款口径）。
//
// 落法：
//   ①免疫+反弹 = **BLOCK 票 isCrit 门**（K15 133 直系——官方文"免疫并反弹对手造成的
//     致命一击"同款子句绑定）：defender==bucket_owner、isRed、isCrit、final>0 →
//     按免疫前快照全额 FIXED 反弹攻方 + 全字段清零（免疫）。⚠️ 工单行提示"DETECT 相位
//     isCrit 门"指的是 isCrit 门的读法（k279 601 是攻方回血走 DETECT）；守方"免疫归零"
//     的通道按 133/k_lowhit 定式走 **BLOCK**（DamageEffectCategory::BLOCK，吃"挡伤失效"）。
//   ①触发后回血 N：heal_amount（吃封回血钳满；601 同通道）。
//   ①下回合先制 +M：置 pending 旗（宿主 soulmark_storage）→ SOUL_PROGRAM 声明
//     BATTLE_FIRST_MOVE_RIGHT 节点消费（ws.preemptive_level 的 memset 在 MOVE_RIGHT
//     时点，SOP §二.2；1263 同款一次性消费）。无条件先制（文无"攻击"门）。
//   ②被消除→瘫痪分支 = EVENT_STAT_BOOSTS_LOST watcher（TEAM 作用域，k399 窗式）：
//     ev.target==owner 且 stat_loss_kind==0（被消除；被吸取=1 不算"被消除"，口径记档）
//     → apply_anomaly_checked(对手, db[3], 2回合, owner, db[2], SoulMark)（概率闸门
//     管辖；概率申报版红线条）→ landed（SUCCESS/REPLACED_EXISTING/DURATION_EXTENDED/
//     CONVERTED）不兜底；ROLL_FAILED/免疫/抗性/反弹 = "未触发瘫痪" → break_round_effects
//     （对手）（battle_primitives.h ROLL_FAILED 档注释的官方"未触发补偿"口径；k177 399
//     兜底同款）。瘫痪时长官方未给 → 按 133"死亡瘫痪 2 回合"家族档。
//
// ⚠️ 已知近似（家族口径记档）：pending 先制旗绑宿主 soulmark_storage——触发后该宠被
//     换下再上场时旗才被消费（官方语义"下回合"，换下回合先制本就无观测面）。
//
// 三查记录（2026-10-24 本 worktree 复核）：①内核 grep 786 无魂印行为硬编码；
//   ②effect_info 786 = 技能空间同号模板（"随机附加{0}种异常状态"），魂印以 effect_icon
//   tips 为权威（SOP §六.1，dump 原文已核两行 tips 同构）；③registry soul 集合无 786、
//   soul_lib 全库无 SOUL_MARK/SOUL_PROGRAM(786)——真缺口。4399✓3134（id≤5000 真精灵）。
// 先例：k_reflect_tail_family 133（免疫+反弹致命一击，BLOCK isCrit 门旗舰直系）、
//   k_lowhit_immune_reflect 608（BLOCK 票 epoch 重挂 + db_at 原序）、
//   k279_crit_heal 601（isCrit 判据三重门 + heal_amount 通道）、
//   k399_attack_break 399（EVENT watcher TEAM 窗 + break_round_effects 兜底）、
//   chaos_mozun_1263（storage 旗 + MOVE_RIGHT 消费先制）。
// 场景回归：test/scenario/scenario_7070_crit_immune_reflect_786.cpp（号段 7070-7079）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 786;
constexpr int kEpochKey = 786 * 100 + 9;        // BLOCK 票已挂 epoch 旗（soulmark_storage）
constexpr int kPrioPendingKey = 786 * 100 + 1;  // 下回合先制 pending 旗（soulmark_storage）
constexpr int kWatcherWiredBase = 990786000;    // +owner：watcher 已挂旗（plugin_storage）
constexpr int kParalysisFallback = 19;          // db[3] 兜底：瘫痪（官方码 19）
constexpr int kParalysisRounds = 2;             // 官方未给时长 → 133 家族档（记档）

// ── 宿主行参数（db_at 原序读 owned_int_args——真实对局与场景语义一致，K279 口径）──
struct HostRow {
    const std::vector<int>* db = nullptr;
    int monster_id = 0;
    bool valid = false;
};

HostRow host_row(BattleContext* ctx, const SoulNodeArgs& a, int mark_id) {
    HostRow row;
    if (!ctx) {
        return row;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return row;
    }
    int slot = a.host_slot;
    if (slot < 0 || slot > 5) {
        slot = ctx->find_pet_with_soulmark(owner, mark_id);
    }
    if (slot < 0 || slot > 5) {
        return row;
    }
    const ElfPet& host = ctx->seerRobot[owner].elfPets[slot];
    row.db = &host.soulMark.args.owned_int_args;
    row.monster_id = host.soulMark.monster_id;
    row.valid = true;
    return row;
}

int db_at(const HostRow& row, int idx, int fallback) {
    if (row.db && idx >= 0 && idx < static_cast<int>(row.db->size())) {
        return (*row.db)[idx];
    }
    return fallback;
}

// ── ①arm：BLOCK 票（每 epoch 重挂一次，k_lowhit 同款 epoch 守卫）──────────────
EffectResult k362_786_arm_block(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    // epoch 幂等守卫（K1/K9 同款 int 槽；先 find 后 insert）
    ElfPet& pet = ctx->getPet(owner);
    auto it = pet.soulmark_storage.find(kEpochKey);
    if (it == pet.soulmark_storage.end()) {
        it = pet.soulmark_storage.emplace(kEpochKey, int{-1}).first;
    }
    if (int* epoch = std::any_cast<int>(&it->second)) {
        if (*epoch == ctx->pipeline_valid_id[owner]) {
            return EffectResult::kOk;
        }
        *epoch = ctx->pipeline_valid_id[owner];
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    const int heal = db_at(row, 0, 500);
    const int prio = db_at(row, 1, 2);
    if (heal <= 0) {
        return EffectResult::kOk;   // 无参不挂（K1 纪律）
    }
    ctx->register_damage_effect(
        DamagePhase::BLOCK, owner, DamageEffectCategory::BLOCK,
        [owner, heal, prio](BattleContext* c, int bucket_owner) {
            DamageSnapshot& d = c->resolvedDamage;
            // ①被击方是持印者 ②红伤 ③暴击（"致命一击"=isCrit，133 口径）④伤害未归零
            if (d.defenderId != bucket_owner || !d.isRed || !d.isCrit || d.final <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(c, bucket_owner, kMarkId)) {
                return;
            }
            const int reflected = d.final;   // 免疫前快照（"免疫并反弹"子句绑定，133 同位）
            d.base = 0;
            d.afterAdd = 0;
            d.afterMul = 0;
            d.final = 0;
            d.addPct = 0;
            d.mulCoef = 0.0;
            // 反弹攻方（FIXED 档粉伤；133 同位）
            if (d.attackerId >= 0 && d.attackerId <= 1 && g_core
                && g_core->deal_pink_damage) {
                if (reflected > 0) {
                    g_core->deal_pink_damage(c, d.attackerId, reflected, DamageKind::FIXED,
                                             bucket_owner);
                }
            }
            // 触发后恢复 N 点体力（吃封回血钳满；601 同通道）
            if (g_core && g_core->heal_amount) {
                (void)g_core->heal_amount(c, bucket_owner, heal);
            }
            // 下回合先制 +M：置 pending 旗（绑宿主 pet，FMR 节点消费；1263 同款）
            if (prio > 0) {
                ElfPet& host = c->getPet(bucket_owner);
                auto pit = host.soulmark_storage.find(kPrioPendingKey);
                if (pit == host.soulmark_storage.end()) {
                    host.soulmark_storage.emplace(kPrioPendingKey, int{1});
                } else {
                    if (int* v = std::any_cast<int>(&pit->second)) {
                        *v = 1;
                    }
                }
            }
        });
    return EffectResult::kOk;
}

// ── ②arm：EVENT_STAT_BOOSTS_LOST watcher（k399 窗式，一次性挂线）──────────────
EffectResult k362_786_arm_watcher(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int wired_key = kWatcherWiredBase + owner;
    if (ctx->plugin_storage.count(wired_key)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.emplace(wired_key, 1);
    const HostRow row = host_row(ctx, a, kMarkId);
    const int pct = db_at(row, 2, 50);
    const int anomaly_id = db_at(row, 3, kParalysisFallback);
    ctx->event_center_.register_watcher(
        EventType::EVENT_STAT_BOOSTS_LOST, owner, ctx->roundCount,
        /*duration_rounds=*/0, /*once=*/false,
        [owner, pct, anomaly_id](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || ev.target != owner) {
                return;   // 只认"自身的提升被消除"
            }
            if (ev.stat_loss_kind != 0) {
                return;   // 0=被消除；1=被吸取不算（口径记档）
            }
            if (!onstage_has_soul_mark(wctx, owner, kMarkId)) {
                return;
            }
            const int foe = wctx->opponent(owner);
            bool landed = false;
            if (g_core && g_core->apply_anomaly_checked && pct > 0 && anomaly_id > 0) {
                // 概率走申报版（闸门管辖；红线条）。ROLL_FAILED = "未触发瘫痪"档。
                const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
                    wctx, foe, anomaly_id, kParalysisRounds, owner, pct,
                    ChanceSource::SoulMark);
                landed = r == ApplyAnomalyResult::SUCCESS
                         || r == ApplyAnomalyResult::REPLACED_EXISTING
                         || r == ApplyAnomalyResult::DURATION_EXTENDED
                         || r == ApplyAnomalyResult::CONVERTED;
            }
            if (!landed && g_core && g_core->break_round_effects) {
                // 未触发瘫痪 → 消除对手回合类效果（399 兜底同款，三态原语不分支）
                (void)g_core->break_round_effects(wctx, foe);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── ①FMR 节点：消费 pending 旗 → ws.preemptive_level += M（1263 同款一次性）────
EffectResult k362_786_move_right(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int host_slot = ctx->find_pet_with_soulmark(owner, kMarkId);
    if (host_slot < 0 || host_slot > 5) {
        return EffectResult::kOk;
    }
    auto& st = ctx->seerRobot[owner].elfPets[host_slot].soulmark_storage;
    auto it = st.find(kPrioPendingKey);
    if (it == st.end()) {
        return EffectResult::kOk;
    }
    int* pending = std::any_cast<int>(&it->second);
    if (!pending || *pending != 1) {
        return EffectResult::kOk;
    }
    *pending = 0;   // 下回合一次性
    const HostRow row = host_row(ctx, a, kMarkId);
    const int prio = db_at(row, 1, 2);
    if (prio > 0) {
        ctx->ws.preemptive_level[owner] += prio;   // 无条件先制（文无"攻击"门）
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（786 入账，只增不改）────────
// 两句 tips 均带"（boss无效）"尾注 → 全节点 boss_invalid=true（标注单位=子句，
// 同一子句所有节点统一置位——SoulMarkNodeRef 标注线）。
SOUL_PROGRAM(kMarkId, {
    SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k362_786_arm_block,
              /*once=*/false, /*early=*/true, SoulScope::STAGE, /*boss_invalid=*/true),
    SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k362_786_arm_watcher,
              /*once=*/false, /*early=*/true, SoulScope::STAGE, /*boss_invalid=*/true),
    SOUL_NODE(State::BATTLE_FIRST_MOVE_RIGHT, &k362_786_move_right,
              /*once=*/false, /*early=*/false, SoulScope::STAGE, /*boss_invalid=*/true),
});
