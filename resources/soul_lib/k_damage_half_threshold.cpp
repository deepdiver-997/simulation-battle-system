// K347 伤害对半阈值分支族 —— effect 663（1 宏 / 2 行）
//
// 行清单（tools/soul_effects_dump.py 663，2026-10-22）：
//   663 光之子·米瑞斯 2939 两行（本体行 → 伙伴效果行，同宠同 id 的版本链）：
//     row338（icon 324，本体行）      args[2, 250, 50]
//     row554（icon 536，伙伴效果行）  args[2, 350, 60]
//   tips（两行同文，4399 原文同款）：
//     「若造成伤害小于对手体力的1/2,则追加250点固定伤害;
//       若伤害大于对手体力1/2,则造成伤害的50%恢复自身体力」
//   → db[0]=2 = tips 字面「1/2」的**分母**（两处 1/2 同一分母，一个槽位覆盖）；
//     db[1]=250/350 = 小伤害分支**追加的固定伤害**点数；
//     db[2]=50/60 = 大伤害分支**恢复比例**（造成伤害的百分之几恢复自身体力）。
//
// ⚠️ 行≠宏：两行同 id（本体 250/50 与伙伴效果 350/60），引擎魂印程序按 id 注册**一次**，
//    参数从宿主魂印的 owned_int_args 原序读（db_at）——哪一行被装载就按哪一行的数值走，
//    两个数值档由同一份实现覆盖（K185 1148 / K188 983 / K181 617 同款两行同构口径）。
//
// ⚠️ 实现形态（T4 类型化行参 + db_at 原序读行参）：
//   触发点 = **DETECT 相位**（伤害管线内同步执行）——条件查询类一律走管线同步查询，
//   不用 AFTER_ACTION/TAKE_DAMAGE watcher（K1 SOP：watcher 交付被推迟到 AFTER_ACTION，
//   条件已过期；K189 1016 阈值分叉同款 DETECT 直系）。
//   门 = d.attackerId == owner（**印主自己造成的伤害**）+ d.isRed（红伤；属性直伤不进
//     本管线）+ d.final > 0 + 印主魂印在场（onstage_has_soul_mark）。
//   分支 = 拿「本次造成的伤害」d.final 与「对手当前体力」opp_hp 比：
//     d.final × db[0] <  opp_hp → 小伤害分支：deal_pink_damage(对手, db[1], FIXED, owner)
//                                        （追加固定伤害；粉伤 FIXED 档吃护罩，293/259 同原语）
//     d.final × db[0] >  opp_hp → 大伤害分支：heal_amount(owner, d.final × db[2] / 100)
//                                        （造成伤害的 db[2]% 恢复自身体力，363/312 吸血同款）
//     相等（恰好一半）→ 两分支皆不触发（tips 写「小于」「大于」，等号无归属）。
//   ⚠️ 对手体力口径 = **DETECT 时点现值**（本次命中尚未落地扣血：apply_resolved_damage
//     在 damage_pipeline_.run 之后）——即「打出这一下之前对手还剩多少体力」。
//     乘 db[0] 比大小而非先除后比，避免整数截断把「恰好一半」判错档。
//   ⚠️ boss 门：tips 无（boss无效/免疫）尾注 → 节点 boss_invalid **不置位**
//     （boss 对局照常生效；K264 356 同款口径）。
//
// ⚠️ 三查记录（a17-kprod 2026-10-22）：
//   ① 内核 grep 663 无魂印行为硬编码（include/ src/ 全量 grep 663 只命中无关处）；
//   ② effect_info 663 = 技能空间异文模板「{0}回合内若对手使用攻击技能则{1}%使对手{2}」
//      —— 与魂印行不同空间，魂印以 effect_icon.tips 为权威（SOP §六）；
//   ③ 全库 soul_lib/moves_lib 并集无 663 的**魂印**注册（moves 侧 SKILL_EFFECT(663)
//      属技能效果空间，与魂印程序空间分开，不构成同族撞号；
//      registry soul 集合无 663 —— 真缺口。
//
// 先例：K189 1016（DETECT 相位 d.final 阈值分叉）、K293/259（DETECT 附粉 FIXED 固伤）、
//   K363 312（造成伤害的 % 恢复自身体力，DETECT 相位）、K264 356（DETECT 相位查对手当前体力）。
// 场景回归：test/scenario/scenario_5090_damage_half_threshold.cpp（号段 5090-5099）。
//
// 行参一律 db_at 读 owned_int_args **原序**（禁读 a.row——相对 DB 偏移 2 位，真实对局
// 常为空/错位；见《魂印行参读取路径缺陷-2026-10-21》）。显式 0 = 关闸，不 fallback。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 663;
constexpr int kDenomFallback = 2;    // tips 字面「1/2」的分母
constexpr int kFlatFallback = 250;   // 小伤害分支追加的固定伤害点数
constexpr int kHealPctFallback = 50; // 大伤害分支恢复比例（造成伤害的 %）
constexpr int kWiredKeyBase = 990347000;  // +owner：DETECT 票已接线旗标

// ── 宿主行参数视图（各文件自治同款）────────────────────────────────────────
struct HostRow {
    const std::vector<int>* db = nullptr;
    bool valid = false;
};

HostRow host_row(BattleContext* ctx, int owner) {
    HostRow row;
    if (!ctx || owner < 0 || owner > 1) {
        return row;
    }
    const int slot = ctx->find_pet_with_soulmark(owner, kMarkId);
    if (slot < 0 || slot > 5) {
        return row;
    }
    row.db = &ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    row.valid = true;
    return row;
}

// db_at：读宿主魂印 owned_int_args 原序（官方行第 idx+1 位）。
// 显式 0 会被原样读出（关闸档），只有该位缺失才用 fallback。
int db_at(const HostRow& row, int idx, int fallback) {
    if (row.db && idx >= 0 && idx < static_cast<int>(row.db->size())) {
        return (*row.db)[idx];
    }
    return fallback;
}

// arm 幂等守卫（K1 EpochGuard 同款语义，plugin_storage 槽版）。
// 魂印程序在登场时 arm 一次；TEAM 作用域的 DETECT 票随对局存在，换宠不重复接线。
bool already_armed(BattleContext* ctx, int owner) {
    const int key = kWiredKeyBase + owner;
    if (ctx->plugin_storage.count(key)) {
        return true;
    }
    ctx->plugin_storage.emplace(key, 1);
    return false;
}

// ── 663 效果本体：自身攻击的红伤落地时按「伤害 vs 对手当前体力的一半」分叉 ────
EffectResult k347_663_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || already_armed(ctx, owner)) {
        return EffectResult::kOk;
    }
    ctx->register_default_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner](BattleContext* c, int bucket_owner) {
            if (!c || !g_core || bucket_owner != owner) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            // 只认「印主自己造成的红伤」（属性直伤不进本管线；miss/被挡无 final>0）
            if (d.attackerId != owner || !d.isRed || d.final <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            const HostRow row = host_row(c, owner);
            const int denom = db_at(row, 0, kDenomFallback);
            const int flat = db_at(row, 1, kFlatFallback);
            const int pct = db_at(row, 2, kHealPctFallback);
            if (denom <= 0) {
                return;   // 分母非法（显式 0）→ 整条效果关闸，不猜语义
            }
            const int opp = c->opponent(owner);
            const int opp_hp = c->getPet(opp).hp;
            if (opp_hp <= 0) {
                return;
            }
            const long long scaled = static_cast<long long>(d.final) * denom;
            if (scaled < static_cast<long long>(opp_hp)) {
                // 小伤害分支：追加固定伤害（粉伤 FIXED 档，吃护罩）
                if (flat > 0 && g_core->deal_pink_damage) {
                    (void)g_core->deal_pink_damage(c, opp, flat, DamageKind::FIXED, owner);
                }
            } else if (scaled > static_cast<long long>(opp_hp)) {
                // 大伤害分支：造成伤害的 db[2]% 恢复自身体力
                if (pct > 0 && g_core->heal_amount) {
                    (void)g_core->heal_amount(
                        c, owner,
                        static_cast<int>(static_cast<long long>(d.final) * pct / 100));
                }
            }
            // 相等（恰好一半）→ tips「小于」「大于」皆不满足 → 不触发
        });
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（663 入账，只增不改）──────
// ⚠️ boss_invalid 不置位（tips 无 boss 尾注 → boss 对局照常生效）。
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k347_663_arm,
                        /*once=*/false, /*early=*/true, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
