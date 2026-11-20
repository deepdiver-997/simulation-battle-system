// K390 对手提升门受击焚烬/必爆+伤害值固伤族 —— effect 1523（1 宏 / 2 行，极炎魔兽 3927）
//（接管实现 2026-10-24，worktree ../agents/k390；号段 7430-7439，a26-kprod）
//
// 行清单（tools/soul_effects_dump.py 1523，2026-10-24）：
//   row1192 本体 args[100,100,25,1]：「回合开始时，若对手不处于能力提升状态则当回合
//     自身受到攻击时100%令对手焚烬，若对手处于能力提升状态则当回合自身攻击100%打出
//     致命一击；自身攻击附加造成伤害值25%的固定伤害，若回合开始时对手处于异常状态则
//     恢复等量体力（boss无效）」
//   row1269 伙伴行 args[100,100,35,1]：同文 25%→35%（db2 附加固伤率）。
//
// ⚠️ 三查记录（a25-kprod 切片核查 2026-10-24 / a26-kprod 动工同日复核）：
//   ①内核 grep 无 1523 魂印行为硬编码；②effect_info 1523 异文（消强吸取模板
//   ——tips 权威）；③registry soul 无 1523、soul_lib 全库 grep 无同 id 注册——真缺口；
//   焚烬 Incinerate=22 已建模（abnormal-types.h）。
//
// 行参解码（两行同构互锚，db_at 原序）：
//   db[0]=①受击焚烬率（100）、db[1]=①必爆率（100）、db[2]=②附加固伤率（25/35）、
//   db[3]=1 旗（不消费——K1 纪律）。
//
// 实现（板行处方）：
//   ① BATTLE_ROUND_START 采样**两账本**（每回合重写，2492 形）：
//     · mode（990390100+own）：对手无提升（ability_levels[opp][i] 全 ≤0，K258 359
//       判据）→ 1=受击焚烬窗；有提升 → 2=自身必爆窗；
//     · opp_abn（990390200+own）：has_active_anomaly 全码扫（0..43，crawford 4644
//       同款循环）→ 对手处于任一异常 = 1。
//     必爆消费 = BATTLE_FIRST_MOVE_RIGHT 节点读 mode==2 → ws.must_crit[own]=true
//     （memset 后写，K386 1688 FMR 形；暴击判定只对攻击技跑）。
//   ①' 受击焚烬 = DETECT once-ever 票（TEAMB，818/K386 接线形）受击面：
//     d.defenderId==own && isRed && final>0 && mode==1 → apply_anomaly_checked
//     (Incinerate=22, attacker, random_anomaly_duration(), own, db0, SoulMark)
//     （100% 短路不耗 rand）。
//   ② 同票攻面：d.attackerId==own && isRed && final>0 → fixed = final×db2/100 →
//     deal_pink_damage(opp, fixed, FIXED)（伤害值%固伤，K383 ③ 同款通道）；
//     opp_abn==1 → heal_amount(own, fixed)（"恢复等量体力"对账量=②固伤量，处方口径；
//     heal 原语内吃封回血/上限封顶）。fixed==0（final<4 取整归零）不附加不回复。
//   enter 节点：wired 旗防重 + 清两账本跨局/换宠残留（中途上场当回合无 RS 采样
//   → 两分支都不启用，动工口径记档）。
// 注册：SOUL_PROGRAM(1523) 三节点（enter + ROUND_START + FMR）+ DETECT 票；
// 总数 +1（只增不改）；tips 尾注（boss无效）→ 全节点 boss_invalid=true
// （boss 局节点不注册 → 票不武装）。
// 场景回归：test/scenario/scenario_7430_boost_gate_incinerate_1523.cpp（7430-7439）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>
#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 1523;
constexpr int kIncinerate = static_cast<int>(AbnormalStatusId::Incinerate);  // 22
constexpr int kWiredKeyBase = 990390000;   // +owner：DETECT 票已接线旗标（一场一次）
constexpr int kModeKeyBase = 990390100;    // +owner：RS 采样回合分支（1=焚烬 2=必爆）
constexpr int kOppAbnKeyBase = 990390200;  // +owner：RS 采样对手异常（0/1）

// ── 宿主行参数（db_at 读 owned_int_args 原序——K384 方案 B 同款）──────────────
struct HostRow {
    const std::vector<int>* db = nullptr;
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
    row.db = &ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    row.valid = true;
    return row;
}

int db_at(const HostRow& row, int idx, int fallback) {
    if (row.db && idx >= 0 && idx < static_cast<int>(row.db->size())) {
        return (*row.db)[idx];
    }
    return fallback;
}

void write_int(BattleContext* ctx, int key, int v) {
    ctx->plugin_storage[key] = v;
}

int read_int(BattleContext* ctx, int key) {
    const auto it = ctx->plugin_storage.find(key);
    return it == ctx->plugin_storage.end() ? 0 : any_int_or(it->second, 0);
}

// 对手是否处于能力提升状态（引擎口径 = 任一能力槽正等级，K258 359 判据）。
bool opp_has_boost(BattleContext* ctx, int opp) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[opp][i] > 0) {
            return true;
        }
    }
    return false;
}

// 对手是否处于任一异常状态（全码扫 0..43，crawford 4644 同款通道）。
bool has_any_active_anomaly(BattleContext* ctx, int side) {
    if (!g_core || !g_core->has_active_anomaly) {
        return false;
    }
    for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
        if (g_core->has_active_anomaly(ctx, side, id)) {
            return true;
        }
    }
    return false;
}

// ── enter：wired 旗 + 清回合账本残留 + 接线 DETECT once-ever 票 ────────────────
EffectResult k390_1523_enter(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->apply_anomaly_checked ||
        !g_core->deal_pink_damage || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.erase(kModeKeyBase + owner);      // 跨局/换宠残留清账
    ctx->plugin_storage.erase(kOppAbnKeyBase + owner);    //（中途上场当回合无采样）
    const int wired_key = kWiredKeyBase + owner;
    if (ctx->plugin_storage.count(wired_key)) {
        return EffectResult::kOk;   // 已接线（一场一次，TEAM 票防堆叠——K386 记档）
    }
    ctx->plugin_storage.emplace(wired_key, 1);
    const HostRow row = host_row(ctx, a, kMarkId);
    const int burn_rate = db_at(row, 0, 100);
    const int fixed_rate = db_at(row, 2, 25);
    ctx->register_default_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner, burn_rate, fixed_rate](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner || !g_core ||
                !g_core->apply_anomaly_checked || !g_core->deal_pink_damage ||
                !g_core->heal_amount) {
                return;
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (!d.isRed || d.final <= 0) {
                return;
            }
            // ① 受击焚烬窗：自身被红伤命中 + RS 采样"对手无提升" → 100% 焚烬攻击者
            if (d.defenderId == owner && read_int(c, kModeKeyBase + owner) == 1) {
                (void)g_core->apply_anomaly_checked(
                    c, d.attackerId, kIncinerate, random_anomaly_duration(), owner,
                    burn_rate, ChanceSource::SoulMark);
            }
            // ② 攻面：自身红伤命中 → 附加伤害值×db2% 固伤；RS 采样对手异常 → 回复等量
            if (d.attackerId == owner) {
                int fixed = d.final * fixed_rate / 100;
                if (fixed <= 0) {
                    return;   // 取整归零无从附加
                }
                (void)g_core->deal_pink_damage(c, 1 - owner, fixed,
                                               DamageKind::FIXED, owner);
                if (read_int(c, kOppAbnKeyBase + owner) == 1) {
                    (void)g_core->heal_amount(c, owner, fixed);   // 等量=固伤量
                }
            }
        });
    return EffectResult::kOk;
}

// ── ROUND_START：采样两账本（每回合重写，2492 形）─────────────────────────────
EffectResult k390_1523_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 ||
        !onstage_has_soul_mark(ctx, a.owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const int opp = 1 - owner;
    // 对手有提升 → 自身必爆窗；无提升 → 受击焚烬窗
    write_int(ctx, kModeKeyBase + owner, opp_has_boost(ctx, opp) ? 2 : 1);
    write_int(ctx, kOppAbnKeyBase + owner, has_any_active_anomaly(ctx, opp) ? 1 : 0);
    return EffectResult::kOk;
}

// ── FMR：必爆窗消费（memset 后写，K386 1688 形）───────────────────────────────
EffectResult k390_1523_fmr(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 ||
        !onstage_has_soul_mark(ctx, a.owner, kMarkId)) {
        return EffectResult::kOk;
    }
    if (read_int(ctx, kModeKeyBase + a.owner) == 2) {
        ctx->ws.must_crit[a.owner] = true;   // 当回合自身攻击 100% 致命一击
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（1523 入账，只增不改）──────
// tips 尾注（boss无效）→ 全节点 boss_invalid=true。
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k390_1523_enter,
                        /*once=*/false, /*early=*/true, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_ROUND_START, &k390_1523_round_start,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_FIRST_MOVE_RIGHT, &k390_1523_fmr,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true)});
