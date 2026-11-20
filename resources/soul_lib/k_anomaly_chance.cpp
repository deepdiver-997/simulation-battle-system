// K2 概率异常族 —— effect 114/262/265/306/358/662（6 宏 / 18 宠 17 行）
//
// 行清单（tools/soul_effects_dump.py 114 262 265 306 358 662，2026-10-01）：
//   114 古早主动毒：2625 暗黑鲁斯王 args[10,5]（10%冻伤）/ 2631 暗黑蒙娜丽莎 [10,1]（中毒）/
//       2637 暗黑炽焰金刚 [10,2]（烧伤）——"使用所有技能都有X%"，任何技能（含属性技）
//       → EVENT_SKILL_USED 无系别过滤 + **Ancient 通道**（core_api 注释点名的古早模板族
//       10/11/12/14/15/114 之一：跳过抗性/转化、不触发弹控，只查 Ancient 层免疫）
//       ｜ 2877 萨夫特 [50,16] 流血 → **记档跳过**（见下）
//   262 受击施加：2439 贝露 / 2440 贝露丝 "受到攻击20%的概率使对手中毒"（无参）
//       → EVENT_TAKE_DAMAGE（只认两攻击伤害时点）→ apply checked（Modern 全链）
//   265 攻击附带：2458 艾妮 / 2459 艾尼维亚 "开场2回合内所有直接攻击额外附带30%冻伤"
//       （roundCount<=2 窗）；2591 耀世之魂 args[1000,6,2] "每次攻击6%烧伤"（db[1]/db[2]，
//       db[0]=1000 语义不明不消费）｜ 2562 格雷姆 流血 → 记档跳过
//   306 每回合施加：2612 邪影之尊 args[15,6] "每回合15%令对手害怕" → ROUND_START 节点
//   358 攻击技施加：2747 影忍·黑段 args[1,1,10,7]（10%疲惫）/ 2753 火忍·浓信 [1,1,30,2]
//       （30%烧伤）/ 2759 木忍·加楠 [1,1,30,1]（30%中毒）——"自身攻击类技能"，时长
//       **2回合**（tips 明示；db[0]/db[1]=1,1 语义不明不消费，备注见下）
//       ｜ 3369 砂忍·越马 [1,1,30,20] 失明 → 记档跳过
//   662 概率递增：2940 达赫迪 / 3250 达赫迪拉 args[1,20,19,50,5,50] "攻击时20%瘫痪，
//       每降低50点体力+5%，最高50%"；4610 小卷 args[1,50,7,100,10,100]（50%疲惫，
//       每失100体力+10%，最高100%）—— 自身已损体力 = max-cur，pct = min(base+lost/档×步, 上限)
//       → **F9 审计槽**（990670000+owner 记本次申报概率，场景做无随机梯度断言）
//
// ⚠️ 记档：流血(16)/失明(20) 行按 K2 工单口径跳过（"非 0-9 段异常先跳，记档进内核
//    微任务"）——枚举有号但行为链未建模；瘫痪(19) 属限制类、有专门建模，照做。
//    db[0]/db[1] 两位参数（265/358/662 行首）语义不明，一律不消费——与 K1 同款纪律：
//    解析不了的字段宁可不用，不猜。
// ⚠️ 三查记录（2026-10-01）：①内核无这 6 个魂印 id 硬编码；②effect_info 均为技能空间
//    模板与魂印无关；③全库无同 id 注册。概率 roll 走 apply_anomaly_checked（内核
//    [2.2]：0/100 短路不耗 rand → 场景用 db 100/0 档做确定性断言；662 配审计槽）。
#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kAuditChanceKeyBase = 990670000;   // +owner：662 本次申报概率（审计槽，只读）
constexpr int kWiredKeyBase = 990671000;         // +宏*10+owner：watcher 已挂旗标（防 early 双触发）

// ── 宿主行参数（"行≠宏"：全量 DB args + monster_id，K1 同款私副本）──────────

struct HostRow {
    const std::vector<int>* db = nullptr;
    int monster_id = 0;
    bool valid = false;
};

HostRow host_row(BattleContext* ctx, const EffectArgs& args, int mark_id) {
    HostRow row;
    if (!ctx) {
        return row;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return row;
    }
    int slot = resolve_subject_slot_from_args(args);
    if (slot < 0) {
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

// 行级数值：args 优先、monster 表兜底（无参行）；返回负值 = 该行不做。
int row_value(const HostRow& row, int db_idx,
              std::initializer_list<std::pair<int, int>> monster_table) {
    const int from_db = db_at(row, db_idx, -1);
    if (from_db >= 0) {
        return from_db;
    }
    for (const auto& [monster, value] : monster_table) {
        if (row.monster_id == monster) {
            return value;
        }
    }
    return -1;
}

// 记档行（流血/失明）：不注册、不施加。
bool anomaly_unmodeled(int anomaly_id) {
    return anomaly_id == static_cast<int>(AbnormalStatusId::Bleed)
        || anomaly_id == static_cast<int>(AbnormalStatusId::Blind);
}

// 概率申报施加：ancient=true 走古早通道（114），否则 Modern 全链（亮节管辖）。
void apply_chance_anomaly(BattleContext* ctx, int actor, int target, int anomaly_id,
                          int duration_rounds, int pct, bool ancient) {
    if (!ctx || !g_core || pct <= 0 || anomaly_unmodeled(anomaly_id)
        || !is_valid_abnormal_status_id(anomaly_id)) {
        return;
    }
    if (ancient && g_core->apply_anomaly_ancient_checked) {
        g_core->apply_anomaly_ancient_checked(ctx, target, anomaly_id, duration_rounds, actor,
                                              pct, ChanceSource::SoulMark);
    } else if (!ancient && g_core->apply_anomaly_checked) {
        g_core->apply_anomaly_checked(ctx, target, anomaly_id, duration_rounds, actor, pct,
                                      ChanceSource::SoulMark);
    }
}

bool watcher_wired(BattleContext* ctx, int macro, int owner) {
    const int key = kWiredKeyBase + macro * 10 + owner;
    if (ctx->plugin_storage.count(key)) {
        return true;
    }
    ctx->plugin_storage.emplace(key, 1);
    return false;
}

// "直接攻击"事件判据：自家魂印在场 + 本方使用了攻击类技能（used_attribute=属性技排除）。
bool self_attack_skill_event(BattleContext* ctx, int owner, const BattleEvent& ev, int mark_id) {
    if (ev.actor != owner || ev.used_attribute) {
        return false;
    }
    return onstage_has_soul_mark(ctx, owner, mark_id);
}

// ── effect 114：古早主动毒（任何技能）───────────────────────────────────────

EffectResult soulmark_114_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, 114);
    const int pct = db_at(row, 0, -1);
    const int anomaly = db_at(row, 1, -1);
    if (pct <= 0 || anomaly < 0 || anomaly_unmodeled(anomaly)) {
        return EffectResult::kOk;   // 2877 流血行 → 记档
    }
    if (watcher_wired(ctx, 114, owner)) {
        return EffectResult::kOk;
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, pct, anomaly](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || ev.actor != owner || !onstage_has_soul_mark(wctx, owner, 114)) {
                return;
            }
            // "使用所有技能"：属性技/攻击技都算，不做系别过滤。
            apply_chance_anomaly(wctx, owner, 1 - owner, anomaly, random_anomaly_duration(),
                                 pct, /*ancient=*/true);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── effect 262：受到攻击 → 概率施加（贝露双生，20% 中毒）────────────────────

EffectResult soulmark_262_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, 262);
    const int pct = row_value(row, 0, {{2439, 20}, {2440, 20}});
    if (pct <= 0) {
        return EffectResult::kOk;
    }
    if (watcher_wired(ctx, 262, owner)) {
        return EffectResult::kOk;
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, pct](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || ev.target != owner || ev.actor == owner) {
                return;   // 只认"被对手打"
            }
            if (!onstage_has_soul_mark(wctx, owner, 262)) {
                return;
            }
            // "受到攻击"：只认攻击伤害时点（固定/真伤/粉伤不算）。
            if (ev.state != static_cast<int>(State::BATTLE_FIRST_ATTACK_DAMAGE)
                && ev.state != static_cast<int>(State::BATTLE_SECOND_ATTACK_DAMAGE)) {
                return;
            }
            apply_chance_anomaly(wctx, owner, ev.actor, static_cast<int>(AbnormalStatusId::Poison),
                                 random_anomaly_duration(), pct, /*ancient=*/false);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── effect 265：直接攻击附带施加（艾妮双生带 2 回合窗）──────────────────────

EffectResult soulmark_265_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, 265);
    // 艾妮双生无参（30% 冻伤、开场 2 回合窗）；耀世之魂 db[1]=pct db[2]=烧伤、无窗。
    int pct = db_at(row, 1, -1);
    int anomaly = db_at(row, 2, -1);
    int window_rounds = 0;   // 0 = 无回合窗
    if (pct < 0) {
        pct = row_value(row, 0, {{2458, 30}, {2459, 30}});
        anomaly = static_cast<int>(AbnormalStatusId::Frostbite);
        window_rounds = 2;
    }
    if (pct <= 0 || anomaly < 0 || anomaly_unmodeled(anomaly)) {
        return EffectResult::kOk;   // 2562 格雷姆 流血行 → 记档
    }
    if (watcher_wired(ctx, 265, owner)) {
        return EffectResult::kOk;
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, pct, anomaly, window_rounds](BattleContext* wctx, const BattleEvent& ev) {
            if (!self_attack_skill_event(wctx, owner, ev, 265)) {
                return;
            }
            if (window_rounds > 0 && wctx->roundCount + 1 > window_rounds) {
                return;   // "开场2回合内"：round N 时 roundCount==N-1（advanceRound 在
                          // ROUND_COMPLETION，battleFsm handle_BattleRoundStart 注释锚点）
            }
            apply_chance_anomaly(wctx, owner, 1 - owner, anomaly, random_anomaly_duration(),
                                 pct, /*ancient=*/false);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── effect 306：每回合概率施加（邪影之尊 15% 害怕）──────────────────────────

EffectResult soulmark_306_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (!onstage_has_soul_mark(ctx, owner, 306)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, 306);
    const int pct = db_at(row, 0, 15);
    const int anomaly = db_at(row, 1, static_cast<int>(AbnormalStatusId::Fear));
    apply_chance_anomaly(ctx, owner, 1 - owner, anomaly, random_anomaly_duration(), pct,
                         /*ancient=*/false);
    return EffectResult::kOk;
}

// ── effect 358：攻击类技能概率施加 2 回合（忍者族）──────────────────────────

EffectResult soulmark_358_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, 358);
    const int pct = db_at(row, 2, -1);
    const int anomaly = db_at(row, 3, -1);
    if (pct <= 0 || anomaly < 0 || anomaly_unmodeled(anomaly)) {
        return EffectResult::kOk;   // 3369 越马 失明行 → 记档
    }
    if (watcher_wired(ctx, 358, owner)) {
        return EffectResult::kOk;
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, pct, anomaly](BattleContext* wctx, const BattleEvent& ev) {
            if (!self_attack_skill_event(wctx, owner, ev, 358)) {
                return;
            }
            // 时长 2 回合 = tips 明示（"使对手疲惫2回合"）；db[0]/db[1]=1,1 语义不明不消费。
            apply_chance_anomaly(wctx, owner, 1 - owner, anomaly, /*duration_rounds=*/2, pct,
                                 /*ancient=*/false);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── effect 662：概率递增（自身已损体力爬坡 + 审计槽）────────────────────────

EffectResult soulmark_662_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, 662);
    const int base_pct = db_at(row, 1, -1);
    const int anomaly = db_at(row, 2, -1);
    const int step_threshold = db_at(row, 3, 50);
    const int step_pct = db_at(row, 4, 5);
    const int cap_pct = db_at(row, 5, 50);
    if (base_pct < 0 || anomaly < 0 || anomaly_unmodeled(anomaly) || step_threshold <= 0
        || step_pct <= 0 || cap_pct <= 0) {
        return EffectResult::kOk;
    }
    if (watcher_wired(ctx, 662, owner)) {
        return EffectResult::kOk;
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, base_pct, anomaly, step_threshold, step_pct, cap_pct](BattleContext* wctx,
                                                                      const BattleEvent& ev) {
            if (!self_attack_skill_event(wctx, owner, ev, 662)) {
                return;
            }
            const ElfPet& self = wctx->getPet(owner);
            const int max_hp = resolve_pet_max_hp(self);
            const int lost = std::max(0, max_hp - self.hp);
            // "每降低{档}点体力概率提升{步}%，最高{上限}%"——达赫迪/小卷同族，爬自身已损体力。
            const int pct = std::min(base_pct + lost / step_threshold * step_pct, cap_pct);
            wctx->plugin_storage[kAuditChanceKeyBase + owner] = pct;   // F9 审计槽
            apply_chance_anomaly(wctx, owner, 1 - owner, anomaly, random_anomaly_duration(),
                                 pct, /*ancient=*/false);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +6（只增不改）────────────────
// 114/262/265/358/662 = early arm（ENTER_EXIT_STAGE，挂 TEAM watcher，plugin_storage
// 旗标防 early 双触发）；306 = ROUND_START 节点（无注册副作用，不需要防重）。
SOUL_PROGRAM(114, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_114_arm,
                    /*once=*/false, /*early=*/true)});
SOUL_PROGRAM(262, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_262_arm,
                    /*once=*/false, /*early=*/true)});
SOUL_PROGRAM(265, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_265_arm,
                    /*once=*/false, /*early=*/true)});
SOUL_PROGRAM(306, {SoulMarkNodeRef(State::BATTLE_ROUND_START, &soulmark_306_round_start)});
SOUL_PROGRAM(358, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_358_arm,
                    /*once=*/false, /*early=*/true)});
SOUL_PROGRAM(662, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_662_arm,
                    /*once=*/false, /*early=*/true)});
