#include <utils/sbs_rand.h>
// K6 攻击技命中率压制+条件结算族 —— effect 1314/1576/1843（3 宏 / 3 行）
//
// 行清单（tools/soul_effects_dump.py 1314 1576 1843，2026-10-04）：
//   1314 卡珀莉可 3738 args[35, 3, 3, 1, 1]（kind[5,12]）：
//     "对手使用攻击技能命中率降低35%；若对手攻击命中则自身战斗阶段结束时恢复自身
//     最大体力的1/3，若对手未命中则战斗阶段结束时令对手随机3项能力值-1（boss无效）"
//     → db[0]=命中降 35、db[1]=回血分母 3、db[2]=随机降项数 3；db[3]/db[4]=1 不消费。
//     命中降挂 ws.hit_rate_mod_attack[opp] 乘区（S1-P0，场景 962；消费 skills.cpp:882
//     !is_attribute 分段）。命中/未命中 = 对手 AFTER_ACTION 读 skill_exec_result
//     （36 判据），回合末（OLD_ROUND_END_1，J6 1839 锚）分支结算。
//   1576 幻影圣手·菲克曼 3977 args[50, 50, 1, 1, 1]（kind[10,12,15]）：
//     "回合开始时，若对手处于能力提升状态或能力下降状态则对手所有攻击技能命中率
//     降低50%；若对手当回合能力等级发生变化则战斗阶段结束时50%令对手害怕，未触发
//     则令对手随机1个技能PP值归零且对手下回合所有技能先制-1（boss无效）"
//     → ROUND_START：对手任一槽非零 → 乘区 0.5；FMR：消费上回合末 pending 先制降
//     （⚠️ pl 的 memset 在 MOVE_RIGHT 时点——先制降必须 FMR 写、ROUND_START 写会被清，
//     实测记档）；OLD_ROUND_END：等级变化 → 50% 害怕（SoulMark 源申报制），未落地
//     （含未变化）→ 兜底：随机 1 技 pp 归零 + pending 先制降。"未触发"读法记档：
//     害怕未落地（含未变化）都走兜底——两读法行为并集一致。
//   1843 幕 4257 args[1, 20, 10, 60, 1]（kind[9,12,13]）：
//     "回合开始时，若自身体力高于对手则当回合对手使用属性技能先制-1且下回合无法
//     使用该属性技能；|自身登场后对手所有攻击技能命中率降低20%，自身每次受到攻击
//     伤害则对手命中率降低10%，最高降低60%（切换下场后效果重置，boss无效）"
//     → B 段：受击计数（TAKE_DAMAGE watcher，NORMAL 红伤、onstage 门、on_exit 清），
//     pct=min(db1+db2×hits, db3) → ROUND_START 重写乘区；A 段：ROUND_START 高体门 →
//     preemptive_level[opp]-=1（⚠️ 引擎先制无类型维度，"属性技能先制-1"取全局 -1
//     近似记档）+ 门旗；对手 AFTER_ACTION 记属性技使用；OLD_ROUND_END → seal_skill
//     封属 duration=2（绝对窗自回合末起算覆盖下回合）。
//
// ⚠️ 实现形态（本单踩坑记档，K 系新单必读）：魂印的每回合节点必须**声明进
//    SOUL_PROGRAM**（SoulMarkManager 更新器每回合 ROUND_COMPLETION 重注册声明节点进
//    魂印桶）；arm 节点里经 register_skill_effect 手工挂的桶效果会在更新器重建后
//    失效（实测 FMR 常驻挂法 R2 起不触发）。watcher（事件面）不受重建影响，仍在
//    early arm 里 wired-once 挂。节点 fn 的 owner/opp/mark 参数从 bind_soulmark_args
//    的 args 解析（resolve_owner_from_args/host_row），无自定义 payload。
//
// ⚠️ 三查记录（2026-10-04）：①内核无 1314/1576/1843 魂印行为硬编码（hit_rate_
//    mod_attack 为 S1-P0 通用乘区）②effect_info 无此三 id 技能模板 ③全库并集无同
//    id 注册——真缺口。与 K5（在途）无分支撞车（工单行预核）。
// 先例：K2 k_anomaly_chance（SOUL_PROGRAM 形态/wired 旗/host_row）、场景 962（乘区）、
//   J23 36（skill_exec_result 读点）、J6 1839（OLD_ROUND_END_1 锚）、
//   artemis 1279（seal_skill 封属形）。
// 场景回归：test/scenario/scenario_2830_hit_suppress_cond.cpp（号段 2830-2839）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>
#include <entities/skills.h>

namespace {

// ── 键段（990168xxx，grep 全库无撞）─────────────────────────────────────
constexpr int kWiredBase   = 990168000;   // +宏*10+owner：watcher 已挂旗
constexpr int k1314Hit     = 990168100;   // +roundCount：对手攻击命中旗
constexpr int k1314Miss    = 990168200;   // +roundCount：对手攻击未命中旗
constexpr int k1314AtkUsed = 990168250;   // +roundCount：对手用了攻击技旗
constexpr int k1576Snap    = 990168300;   // +roundCount*6+i：对手六槽快照
constexpr int k1843Hits    = 990168400;   // +owner：1843 受击计数
constexpr int k1843Gate    = 990168500;   // +roundCount：高体门旗
constexpr int k1843Attr    = 990168600;   // +roundCount：对手用了属性技旗
constexpr int k1576Pending = 990168700;   // +owner：下回合先制降 pending（值=级数）

int db_at(const std::vector<int>* db, int idx, int fallback) {
    if (db && idx >= 0 && idx < static_cast<int>(db->size())) {
        return (*db)[idx];
    }
    return fallback;
}

struct HostRow {
    const std::vector<int>* db = nullptr;
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
    row.db = &ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    row.valid = true;
    return row;
}

bool flag_set(const BattleContext* ctx, int base, int key) {
    const auto it = ctx->plugin_storage.find(
        static_cast<unsigned long long>(base + key));
    return it != ctx->plugin_storage.end() && std::any_cast<int>(it->second) != 0;
}

bool watcher_wired(BattleContext* ctx, int macro, int owner) {
    const auto key = static_cast<unsigned long long>(kWiredBase + macro * 10 + owner);
    if (ctx->plugin_storage.count(key)) {
        return true;
    }
    ctx->plugin_storage.emplace(key, 1);
    return false;
}

// 判据统一 §四点五 私副本（soul 侧不链 moves_lib/common.h）。
bool executed_this_round(const BattleContext* ctx, int owner) {
    if (!ctx || owner < 0 || owner > 1) {
        return false;
    }
    return ctx->skill_use_seq[owner] > ctx->ws.skill_use_seq_round_start[owner];
}

bool executed_is_attack(const BattleContext* ctx, int owner) {
    if (!ctx || owner < 0 || owner > 1) {
        return false;
    }
    return ctx->ws.skill_type_view[owner] != static_cast<int>(SkillType::Attribute);
}

// ════════════════════ 1314 卡珀莉可 ════════════════════

// ROUND_START 节点：命中降乘区重写（复位后）。
EffectResult k6_1314_round_start(BattleContext* ctx, const EffectArgs& args) {
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 1314)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, 1314);
    const int pct = db_at(row.db, 0, 35);
    ctx->ws.hit_rate_mod_attack[1 - owner] = 1.0f - pct / 100.0f;
    return EffectResult::kOk;
}

// ⚠️ 对手行动反应不走声明节点（桶按 (state, 持印者) 配对——对手行动时执行的是
//    对手侧桶，持印者条目永不触发，K2 262 同场景用事件面）：命中=TAKE_DAMAGE
//    （NORMAL 红伤）、"用了攻击技"=EVENT_SKILL_USED（used_attribute=属性技排除）。
EffectResult k6_1314_arm_event(BattleContext* ctx, const EffectArgs& args) {
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || watcher_wired(ctx, 1314, owner)) {
        return EffectResult::kOk;
    }
    const int opp = 1 - owner;
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || ev.target != owner
                || ev.damage_kind != static_cast<int>(DamageKind::NORMAL)
                || !onstage_has_soul_mark(wctx, owner, 1314)) {
                return;
            }
            wctx->plugin_storage[static_cast<unsigned long long>(
                k1314Hit + wctx->roundCount)] = 1;
        },
        WatcherScope::TEAM);
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, opp](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || ev.actor != opp || ev.used_attribute
                || !onstage_has_soul_mark(wctx, owner, 1314)) {
                return;
            }
            wctx->plugin_storage[static_cast<unsigned long long>(
                k1314AtkUsed + wctx->roundCount)] = 1;
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// OLD_ROUND_END_1 节点：命中 → 回 max/3；未命中 → 随机 3 项 -1。
EffectResult k6_1314_round_end(BattleContext* ctx, const EffectArgs& args) {
    if (!g_core || !g_core->heal_amount || !g_core->stat_drop) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 1314)) {
        return EffectResult::kOk;
    }
    const int opp = 1 - owner;
    const HostRow row = host_row(ctx, args, 1314);
    const int heal_denom = db_at(row.db, 1, 3);
    const int drop_count = db_at(row.db, 2, 3);
    if (flag_set(ctx, k1314Hit, ctx->roundCount)) {
        const int max_hp =
            ctx->seerRobot[owner].elfPets[ctx->on_stage[owner]].numericalBase[
                NumericalPropertyIndex::HP];
        if (max_hp > 0) {
            (void)g_core->heal_amount(ctx, owner, max_hp / heal_denom);
        }
        return EffectResult::kOk;
    }
    if (flag_set(ctx, k1314AtkUsed, ctx->roundCount)) {   // 用了攻击技但没命中旗=miss/被盔

        int picked[6] = {};
        int done = 0;
        int guard = 0;
        while (done < drop_count && guard < 100) {
            ++guard;
            const int slot = sbs_rand() % 6;
            if (picked[slot]) {
                continue;
            }
            picked[slot] = 1;
            ++done;
            (void)g_core->stat_drop(ctx, opp, slot, 1, owner);
        }
    }
    return EffectResult::kOk;
}

// ════════════════════ 1576 菲克曼 ════════════════════

// ROUND_START 节点：对手任一槽非零 → 命中降乘区；快照六槽（回合末对账）。
EffectResult k6_1576_round_start(BattleContext* ctx, const EffectArgs& args) {
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 1576)) {
        return EffectResult::kOk;
    }
    const int opp = 1 - owner;
    const HostRow row = host_row(ctx, args, 1576);
    const int pct = db_at(row.db, 0, 50);
    bool any = false;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        const int lvl = ctx->ability_levels[opp][i];
        any = any || lvl != 0;
        ctx->plugin_storage[static_cast<unsigned long long>(
            k1576Snap + ctx->roundCount * 6 + i)] = lvl;
    }
    if (any) {
        ctx->ws.hit_rate_mod_attack[opp] = 1.0f - pct / 100.0f;
    }
    return EffectResult::kOk;
}

// FMR 节点：消费 pending 先制降（pl memset 在本时点——写入恰撑过序判定）。
EffectResult k6_1576_fmr_consume(BattleContext* ctx, const EffectArgs& args) {
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 1576)) {
        return EffectResult::kOk;
    }
    const int opp = 1 - owner;
    const auto pend = ctx->plugin_storage.find(
        static_cast<unsigned long long>(k1576Pending + opp));
    if (pend == ctx->plugin_storage.end()) {
        return EffectResult::kOk;
    }
    const int down = std::any_cast<int>(pend->second);
    ctx->plugin_storage.erase(pend);
    if (down > 0) {
        ctx->ws.preemptive_level[opp] -= down;
    }
    return EffectResult::kOk;
}

// OLD_ROUND_END_1 节点：等级变化 → 50% 害怕；未落地 → 兜底。
EffectResult k6_1576_round_end(BattleContext* ctx, const EffectArgs& args) {
    if (!g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 1576)) {
        return EffectResult::kOk;
    }
    const int opp = 1 - owner;
    const HostRow row = host_row(ctx, args, 1576);
    const int fear_pct = db_at(row.db, 1, 50);
    bool changed = false;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        const auto it = ctx->plugin_storage.find(static_cast<unsigned long long>(
            k1576Snap + ctx->roundCount * 6 + i));
        const int snap = it == ctx->plugin_storage.end() ? 0 : std::any_cast<int>(it->second);
        if (snap != ctx->ability_levels[opp][i]) {
            changed = true;
        }
    }
    bool landed = false;
    if (changed) {
        const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
            ctx, opp, static_cast<int>(AbnormalStatusId::Fear),
            random_anomaly_duration(), owner, fear_pct, ChanceSource::SoulMark);
        landed = r == ApplyAnomalyResult::SUCCESS
              || r == ApplyAnomalyResult::REPLACED_EXISTING
              || r == ApplyAnomalyResult::DURATION_EXTENDED;
    }
    if (landed) {
        return EffectResult::kOk;
    }
    // 兜底：随机 1 技 pp 归零（pp=-1 无限 PP 跳过）+ 下回合先制 -1（pending）。
    const int slot = sbs_rand() % 5;
    Skills& sk = ctx->seerRobot[opp].elfPets[ctx->on_stage[opp]].skills[slot];
    if (sk.pp >= 0) {
        sk.pp = 0;
    }
    ctx->plugin_storage[static_cast<unsigned long long>(k1576Pending + opp)] = 1;
    return EffectResult::kOk;
}

// ════════════════════ 1843 幕 ════════════════════

// ROUND_START 节点：B 段乘区重写 + A 段高体门（先制 -1 + 门旗）。
EffectResult k6_1843_round_start(BattleContext* ctx, const EffectArgs& args) {
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 1843)) {
        return EffectResult::kOk;
    }
    const int opp = 1 - owner;
    const HostRow row = host_row(ctx, args, 1843);
    const int base_pct = db_at(row.db, 1, 20);
    const int step = db_at(row.db, 2, 10);
    const int cap = db_at(row.db, 3, 60);
    const auto it = ctx->plugin_storage.find(static_cast<unsigned long long>(k1843Hits +
                                                                                    owner));
    const int hits = it == ctx->plugin_storage.end() ? 0 : std::any_cast<int>(it->second);
    const int pct = std::min(base_pct + step * hits, cap);
    ctx->ws.hit_rate_mod_attack[opp] = 1.0f - pct / 100.0f;
    if (ctx->getPet(owner).hp > ctx->getPet(opp).hp) {
        ctx->plugin_storage[static_cast<unsigned long long>(k1843Gate +
                                                            ctx->roundCount)] = 1;
    }
    return EffectResult::kOk;
}

// FMR 节点：高体门 → 对手先制 -1（pl memset 在本时点——写入恰撑过序判定；
// 属性技先制-1 的全局近似，文件头记档）。
EffectResult k6_1843_fmr_gate(BattleContext* ctx, const EffectArgs& args) {
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 1843)) {
        return EffectResult::kOk;
    }
    const int opp = 1 - owner;
    if (ctx->getPet(owner).hp > ctx->getPet(opp).hp) {
        ctx->ws.preemptive_level[opp] -= 1;
    }
    return EffectResult::kOk;
}

// ⚠️ 对手行动反应走事件面（同 1314 记档）：高体门回合用了属性技（SKILL_USED
//    used_attribute=true）→ 旗。
EffectResult k6_1843_arm_event(BattleContext* ctx, const EffectArgs& args) {
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || watcher_wired(ctx, 18430, owner)) {
        return EffectResult::kOk;
    }
    const int opp = 1 - owner;
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, opp](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || ev.actor != opp || !ev.used_attribute
                || !onstage_has_soul_mark(wctx, owner, 1843)
                || !flag_set(wctx, k1843Gate, wctx->roundCount)) {
                return;
            }
            wctx->plugin_storage[static_cast<unsigned long long>(
                k1843Attr + wctx->roundCount)] = 1;
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// OLD_ROUND_END_1 节点：对手用了属性技 → 封属 duration=2（覆盖下回合，绝对窗记档）。
EffectResult k6_1843_round_end(BattleContext* ctx, const EffectArgs& args) {
    if (!g_core || !g_core->seal_skill) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 1843)) {
        return EffectResult::kOk;
    }
    if (!flag_set(ctx, k1843Attr, ctx->roundCount)) {
        return EffectResult::kOk;
    }
    const int opp = 1 - owner;
    (void)g_core->seal_skill(ctx, owner, opp, /*effect_id=*/1843, /*attribute=*/true,
                             /*attack=*/false, /*count=*/0, /*duration_rounds=*/2,
                             /*penetrable=*/false, /*source_slot=*/-1,
                             EffectScope::ON_STAGE, /*hit_invalid=*/false,
                             /*chance_pct=*/100, /*consumed_when_pierced=*/false);
    return EffectResult::kOk;
}

// early arm：仅挂受击计数 watcher（wired-once；事件面不受节点重建影响）。
EffectResult k6_1843_arm(BattleContext* ctx, const EffectArgs& args) {
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || watcher_wired(ctx, 1843, owner)) {
        return EffectResult::kOk;
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* wctx, const BattleEvent& ev) {
            fprintf(stderr, "[DBG] w1843 ev t=%d kind=%d\n", ev.target, ev.damage_kind);
            if (!wctx || ev.target != owner
                || ev.damage_kind != static_cast<int>(DamageKind::NORMAL)
                || !onstage_has_soul_mark(wctx, owner, 1843)) {
                return;
            }
            const auto key = static_cast<unsigned long long>(k1843Hits + owner);
            const auto it = wctx->plugin_storage.find(key);
            const int cur =
                it == wctx->plugin_storage.end() ? 0 : std::any_cast<int>(it->second);
            wctx->plugin_storage[key] = cur + 1;
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// on_exit：切换重置受击计数（watcher onstage 门天然停计，重进重计）。
void k6_1843_exit(BattleContext* ctx, int owner) {
    if (!ctx) {
        return;
    }
    ctx->plugin_storage.erase(static_cast<unsigned long long>(k1843Hits + owner));
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +3（只增不改）────────────────
// 每回合节点全部**声明进 SOUL_PROGRAM**（更新器每回合重注册进魂印桶；arm 里手工
// 挂桶效果会被重建清掉——本单踩坑记档见文件头）。1843 另带 watcher early arm +
// on_exit 切换重置钩子。
SOUL_PROGRAM(1314, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE,
                    &k6_1314_arm_event, /*once=*/false, /*early=*/true),
                   SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE,
                    &k6_1314_round_start, /*once=*/false, /*early=*/true),
                   SoulMarkNodeRef(State::BATTLE_ROUND_START, &k6_1314_round_start),
                   SoulMarkNodeRef(State::BATTLE_OLD_ROUND_END_1, &k6_1314_round_end)});
SOUL_PROGRAM(1576, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &k6_1576_round_start,
                    /*once=*/false, /*early=*/true),
                   SoulMarkNodeRef(State::BATTLE_ROUND_START, &k6_1576_round_start),
                   SoulMarkNodeRef(State::BATTLE_FIRST_MOVE_RIGHT, &k6_1576_fmr_consume),
                   SoulMarkNodeRef(State::BATTLE_OLD_ROUND_END_1, &k6_1576_round_end)});
SOUL_PROGRAM(1843, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &k6_1843_arm,
                    /*once=*/false, /*early=*/true),
                   SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &k6_1843_arm_event,
                    /*once=*/false, /*early=*/true),
                   SoulMarkNodeRef(State::BATTLE_ROUND_START, &k6_1843_round_start),
                   SoulMarkNodeRef(State::BATTLE_FIRST_MOVE_RIGHT, &k6_1843_fmr_gate),
                   SoulMarkNodeRef(State::BATTLE_OLD_ROUND_END_1, &k6_1843_round_end)});
SOUL_MARK_HOOKS(1843, nullptr, &k6_1843_exit);
