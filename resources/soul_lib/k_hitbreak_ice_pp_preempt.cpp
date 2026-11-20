#include <utils/sbs_rand.h>
// K378 消回合类+冰封/PP归零/先制解降族 —— effect 1355（1 宏 2 行）
//
// 行清单（tools/soul_effects_dump.py 1355，2026-10-24）：
//   1355 row1057（3778 冰幽仙子·尤米娜）args[50,1,1,1]：
//     「自身攻击技能命中后消除对手回合类效果，消除成功则50%令对手冰封；|回合开始时，
//      对手处于能力下降状态则攻击技能使对手随机1个技能的PP归零，自身处于能力下降
//      状态则攻击技能先制+1且解除自身能力下降状态（boss无效）」
//   1355 row1650（四灵归位伙伴效果行）args[80,1,2,1]：80% 档 / 先制+2，其余同构互锚。
//
// 行参解码（两行互锚自证；⚠️ 工单处方 db2=PP归零数/db3=先制级数 与行文矛盾——
//   row1650 tips「先制+2」↔ db[2]=2、两行 PP 均「随机1个技能」↔ db[3]=1，以行文
//   权威修正为 db[2]=先制级数、db[3]=PP归零数，头注释记档；行≠模板：effect_info
//   1355 技能空间异文，tips 权威）：
//   db[0]=冰封率%（50/80）   db[1]=旗恒 1（不消费，记档）
//   db[2]=先制级数（1/2）    db[3]=PP归零数（1/1）
//
// 落法（板行处方逐子句）：
//   ①「攻击技命中后消回合类，成功则冰封」：DETECT 攻方票（K203 344 同窗；
//      "命中后"≠399 的"使用后"——红伤 final>0 才触发，属性技天然排除）→
//      break_round_effects(对手)（三态原语）→ SUCCESS 分支才 apply_anomaly_checked
//      (Icebound=15, db[0], SoulMark)（申报单掷，K191 勘误）+ 审计槽。
//   ②「回合开始对手能力下降 → 攻击技随机 1 技能 PP 归零」：ROUND_START 采样
//      对手任一负等级（K253 判据）→ 门旗；DETECT 票见旗 → pp_zero_slot 随机槽
//      ×db[3]（K230 循环直系：rand%5、SUCCESS 才计数、无限 PP 槽原语拒）。
//   ③「回合开始自身能力下降 → 攻击技先制+db[2] 且解除自身能力下降」：ROUND_START
//      采样自身任一负等级 → 门旗；FMR 节点（ws.preemptive_level 在 MOVE_RIGHT
//      memset 之后写，K218/yanshou 形）→ preemptive_level[owner] += db[2]
//      （侧级先制，277"所有攻击先制+1"同款通道，tips"攻击技"限定词记档）+
//      逐槽 stat_change 复位负等级（K361 通道，负 lv → -lv 复位到 0）。
//   boss 标注：tips 尾注（boss无效）挂在第二分句（| 分句）→ ROUND_START/FMR/
//   PP 票节点置位；第一分句（消回合类+冰封）无尾注不置位——arm 分双节点分别带旗。
//   行参读取：db_at 读宿主 owned_int_args 原序（方案 B）。审计槽 990378800 段。
//
// 三查记录（2026-10-24，worktree ../agents/k378）：
//   ①内核 grep 1355 无魂印行为硬编码；②effect_icon kind=[8,10,15]、tips 为权威文
//   （dump 原文已核，见上）；③registry soul 集合无 1355、soul_lib 全库 grep 无
//   SOUL_PROGRAM(1355)——真缺口。
// 先例：k203 344（DETECT 攻方票）、k399（break_round_effects 消回合类族）、
//   k230 1562（pp_zero_slot 循环）、k253 336（能力下降判据）、k218 277（FMR 写
//   preemptive_level）、k361 819（stat_change 复位通道）、k366 1210（审计槽）。
// 场景回归：test/scenario/scenario_7103_k378_hitbreak_ice_pp.cpp（号段 7103）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>
#include <entities/skills.h>
#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 1355;
constexpr int kWiredBase = 990378000;        // 接线去重：+宏*10+owner（grep 无撞）
constexpr int kPpFlagBase = 990378500;       // 对手降属 PP 门旗：+owner
constexpr int kPreemptFlagBase = 990378600;  // 自身降属先制门旗：+owner
constexpr int kAuditPctBase = 990378800;     // 审计槽：+owner*2=冰封申报概率；+1=结果

// ── 宿主行参数（方案 B：db_at 读 owned_int_args 原序，与真实对局一致）────────
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

bool watcher_wired(BattleContext* ctx, int owner, int slot_no) {
    const int key = kWiredBase + kMarkId * 10 + owner + slot_no;
    if (ctx->plugin_storage.count(key)) {
        return true;
    }
    ctx->plugin_storage.emplace(key, 1);
    return false;
}

// 任一能力槽为负（K253 判据）
bool has_stat_down(const BattleContext* ctx, int side) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[side][i] < 0) {
            return true;
        }
    }
    return false;
}

// ── arm ①（无 boss 尾注子句）：DETECT 攻方票——命中消回合类，成功则冰封 ────
EffectResult k1355_arm_break_ice(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || watcher_wired(ctx, owner, 0)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    const int ice_pct = db_at(row, 0, 0);
    if (ice_pct <= 0) {
        return EffectResult::kOk;
    }
    ctx->register_default_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner, ice_pct](BattleContext* c, int bucket_owner) {
            DamageSnapshot& d = c->resolvedDamage;
            if (bucket_owner != owner || d.attackerId != owner || !d.isRed
                || d.final <= 0) {
                return;   // 自身攻击技命中（红伤落地）
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId) || !g_core
                || !g_core->break_round_effects
                || !g_core->apply_anomaly_checked) {
                return;
            }
            if (g_core->break_round_effects(c, 1 - owner)
                != BreakResult::SUCCESS) {
                return;   // 消除成功才冰封（NO_EFFECTS/IMMUNE 不触发）
            }
            const ApplyAnomalyResult res = g_core->apply_anomaly_checked(
                c, 1 - owner, static_cast<int>(AbnormalStatusId::Icebound),
                random_anomaly_duration(), owner, ice_pct, ChanceSource::SoulMark);
            // 审计槽（F9 782 技巧）
            c->plugin_storage[kAuditPctBase + owner * 2] = ice_pct;
            c->plugin_storage[kAuditPctBase + owner * 2 + 1] =
                static_cast<int>(res);
        });
    return EffectResult::kOk;
}

// ── arm ②（boss 尾注子句）：DETECT 攻方票——对手降属门 → 随机 PP 归零 ───────
EffectResult k1355_arm_pp_zero(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || watcher_wired(ctx, owner, 1)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    const int pp_count = db_at(row, 3, 0);
    if (pp_count <= 0 || !g_core || !g_core->pp_zero_slot) {
        return EffectResult::kOk;
    }
    ctx->register_default_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner, pp_count](BattleContext* c, int bucket_owner) {
            DamageSnapshot& d = c->resolvedDamage;
            if (bucket_owner != owner || d.attackerId != owner || !d.isRed
                || d.final <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            auto it = c->plugin_storage.find(kPpFlagBase + owner);
            if (it == c->plugin_storage.end() || any_int_or(it->second, 0) != 1) {
                return;   // 本回合回合开始对手无能力下降 → 门闭
            }
            int done = 0;
            int guard = 0;
            while (done < pp_count && guard < 100) {
                ++guard;
                const int slot = sbs_rand() % 5;   // 5 技能槽（K230 直系：
                                                    // 无限 PP 槽由原语拒并重试）
                if (g_core->pp_zero_slot(c, 1 - owner, slot, owner)
                    == PpReduceResult::SUCCESS) {
                    ++done;
                }
            }
        });
    return EffectResult::kOk;
}

// ── ROUND_START（boss 尾注子句）：双门采样（对手降属 / 自身降属）────────────
EffectResult k1355_round_start_gates(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage[kPpFlagBase + owner] =
        has_stat_down(ctx, 1 - owner) ? 1 : 0;       // 每回合无脑重写（K361 形）
    ctx->plugin_storage[kPreemptFlagBase + owner] =
        has_stat_down(ctx, owner) ? 1 : 0;
    return EffectResult::kOk;
}

// ── FMR（boss 尾注子句）：先制+db[2] + 解除自身能力下降 ─────────────────────
EffectResult k1355_fmr_preempt_clear(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const auto it = ctx->plugin_storage.find(kPreemptFlagBase + owner);
    if (it == ctx->plugin_storage.end() || any_int_or(it->second, 0) != 1) {
        return EffectResult::kOk;   // 回合开始自身无能力下降
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    const int preempt = db_at(row, 2, 0);
    if (preempt > 0) {
        ctx->ws.preemptive_level[owner] += preempt;   // memset 后写（K218 形）
    }
    if (g_core->stat_change) {
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            const int lv = ctx->ability_levels[owner][i];
            if (lv < 0) {
                (void)g_core->stat_change(ctx, owner, i, -lv);   // 复位到 0（K361 通道）
            }
        }
    }
    ctx->plugin_storage.erase(it);   // 当回合一次性
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（1355 入账，只增不改）──────
SOUL_PROGRAM(kMarkId, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k1355_arm_break_ice,
                             /*once=*/false, /*early=*/true, SoulScope::STAGE,
                             /*boss_invalid=*/false),
                       SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k1355_arm_pp_zero,
                             /*once=*/false, /*early=*/true, SoulScope::STAGE,
                             /*boss_invalid=*/true),
                       SOUL_NODE(State::BATTLE_ROUND_START, &k1355_round_start_gates,
                             /*once=*/false, /*early=*/false, SoulScope::STAGE,
                             /*boss_invalid=*/true),
                       SOUL_NODE(State::BATTLE_FIRST_MOVE_RIGHT,
                             &k1355_fmr_preempt_clear,
                             /*once=*/false, /*early=*/false, SoulScope::STAGE,
                             /*boss_invalid=*/true)});
