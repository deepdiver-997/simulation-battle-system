// K359 攻击附当前体力固伤 + 命中回合末回最大体力族 —— effect 1045（1 宏 / 1 行）
//（2026-10-23，worktree kbatch；号段 7010-7019，场景 scenario_7010_curhp_attach_roundend_heal.cpp）
//
// 行清单（tools/soul_effects_dump.py 1045，2026-10-23）：
//   1045 赫里斯 3445 row760 args[3,3]（kind=[5,14]）：
//   「攻击附加自身当前体力1/3的固定伤害；当回合自身攻击命中对手则战斗阶段结束时恢复自身最大体力的1/3」
//   → db[0]=3 = 子句①「自身**当前**体力1/3」的分母；db[1]=3 = 子句②「自身**最大**体力1/3」
//     的分母（两参同值 → 顺序互换在数值上不可观测，参数布局无歧义）。
//   ⚠️ 子句①的基数是**当前体力**（不是最大体力！），子句②的基数是**最大体力**——
//     两者在实战里差很多（低血时附伤显著变小），口径必须分开读。
//
// ⚠️ 行≠宏检查：**1045 全库仅一行**（row760 / 赫里斯 3445）——无同 effect_id 第二行，
//    不存在"后注册静默覆盖先注册"的一族从未生效风险（撞号纪律 §七）。
//    （moves 侧有 SKILL_EFFECT(1045, ...)：属**技能效果空间**（effect_cache_），
//      与魂印程序空间（program_cache_）分开，不构成同族撞号——同 K347 663 口径。）
//
// ⚠️ 三查记录（a21-kprod 2026-10-23）：
//   ① 内核 grep \b1045\b（include/ src/）无魂印行为硬编码；
//   ② effect_info 1045 = 技能空间异文模板「未击败对手则附加自身最大体力1/{0}的百分比伤害」
//      —— 魂印以 effect_icon.tips 为权威（SOP §六）；
//   ③ 全 feat/integration 分支 soul_lib 并集无 1045 注册，registry soul 集合无 1045 —— 真缺口。
//
// ⚠️ 实现形态（T4 类型化行参 + db_at 原序读行参；禁读 a.row）：
//   子句①「攻击附加…固定伤害」= **DETECT 相位**（伤害管线内同步执行，K347 663 / K348 807
//     同款直系）：own 红伤命中（d.attackerId==owner && d.isRed && d.final>0）→
//     deal_pink_damage(对手, own当前HP/db[0], FIXED, owner)。
//     ⚠️ 基数 = **DETECT 时点的印主当前体力**（本次命中尚未落地扣血——印主是攻击方，
//       自己的体力不因这一刀变化；被打过则已含此前掉血）。
//   子句②「当回合自身攻击命中对手则战斗阶段结束时恢复自身最大体力的1/3」= **ROUND_END 节点**
//     （State::BATTLE_ROUND_END；同 K250 1536 直系口径）：
//     ① DETECT 命中时置「本回合命中旗」→ ② ROUND_END 读旗 → heal_amount(印主, maxHP/db[1])，
//     读完即清旗（天然实现"当回合"语义：命中旗每回合消费一次，不跨回合残留）。
//   ⚠️ 旗标用 plugin_storage（同 K250 1536 / K198 281 口径）；不手写窗账本（持续 N 回合的窗
//     才走注册面窗界——这里是"本回合"一次性标记，不是窗）。
//   ⚠️ boss 门：tips 无（boss无效/免疫）尾注 → 节点 boss_invalid **不置位**
//     （boss 对局照常生效；K264 356 / K347 663 同款口径）。
//
// ⚠️ 行参纪律：显式 0 = 关闸（db[0]=0 → 子句① 关闭，不回退 fallback；db[1]=0 → 子句② 关闭），
//   **不得**回退到 fallback 值（在位即消费，K314/K340 两犯）。两子句各自独立开关。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 1045;
constexpr int kAttachDenomFallback = 3;  // 子句①：当前体力 1/3 的分母
constexpr int kHealDenomFallback = 3;    // 子句②：最大体力 1/3 的分母
constexpr int kWiredKeyBase = 990359000;  // +owner：DETECT 票已接线旗标
constexpr int kHitKeyBase = 990359100;    // +owner：本回合命中旗

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

// db_at：读宿主魂印 owned_int_args 原序（禁读 a.row——相对 DB 偏移 2 位，真实对局常为空/错位）。
// 显式 0 会被原样读出（关闸档），只有该位缺失才用 fallback。
int db_at(const HostRow& row, int idx, int fallback) {
    if (row.db && idx >= 0 && idx < static_cast<int>(row.db->size())) {
        return (*row.db)[idx];
    }
    return fallback;
}

// arm 幂等守卫（plugin_storage 槽版；魂印程序在登场时 arm 一次，TEAM 作用域的 DETECT 票
// 随对局存在，换宠不重复接线）。
bool already_armed(BattleContext* ctx, int owner) {
    const int key = kWiredKeyBase + owner;
    if (ctx->plugin_storage.count(key)) {
        return true;
    }
    ctx->plugin_storage.emplace(key, 1);
    return false;
}

// ── 子句①：own 红伤命中 → 附加「自身当前体力/db[0]」点固伤，并置本回合命中旗 ──
EffectResult k359_1045_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || already_armed(ctx, owner)) {
        return EffectResult::kOk;
    }
    ctx->register_default_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner](BattleContext* c, int bucket_owner) {
            if (!c || !g_core || !g_core->deal_pink_damage || bucket_owner != owner) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            // 只认「印主自己造成的红伤」（属性直伤/固伤不进本管线；miss/被挡无 final>0）
            if (d.attackerId != owner || !d.isRed || d.final <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            // 命中旗：子句②的"当回合自身攻击命中对手"判据（每回合消费一次）
            c->plugin_storage[kHitKeyBase + owner] = 1;
            const HostRow row = host_row(c, owner);
            const int denom = db_at(row, 0, kAttachDenomFallback);
            if (denom <= 0) {
                return;   // 显式 0 = 子句① 关闸，不回退 fallback
            }
            const ElfPet& self = c->getPet(owner);
            if (self.hp <= 0) {
                return;
            }
            const int attach = self.hp / denom;   // 基数 = 印主**当前**体力
            if (attach > 0) {
                (void)g_core->deal_pink_damage(c, c->opponent(owner), attach,
                                               DamageKind::FIXED, owner);
            }
        });
    return EffectResult::kOk;
}

// ── 子句②：ROUND_END 读命中旗 → 恢复「自身最大体力/db[1]」点体力（读完清旗）────
EffectResult k359_1045_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    // 命中旗先读先清（读到即消费 → 天然"当回合"语义，不跨回合残留；
    // 魂印离场/换人时旗也不留残值——清在门检之前）。
    const auto it = ctx->plugin_storage.find(kHitKeyBase + owner);
    if (it == ctx->plugin_storage.end()) {
        return EffectResult::kOk;   // 本回合未命中 → 不回
    }
    ctx->plugin_storage.erase(it);
    if (!onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, owner);
    const int denom = db_at(row, 1, kHealDenomFallback);
    if (denom <= 0) {
        return EffectResult::kOk;   // 显式 0 = 子句② 关闸，不回退 fallback
    }
    const ElfPet& self = ctx->getPet(owner);
    const int max_hp = resolve_pet_max_hp(self);
    if (max_hp <= 0 || self.hp <= 0) {
        return EffectResult::kOk;
    }
    const int heal = max_hp / denom;   // 基数 = 印主**最大**体力
    if (heal > 0) {
        (void)g_core->heal_amount(ctx, owner, heal);
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（1045 入账，只增不改）────────
// ⚠️ boss_invalid 不置位（tips 无 boss 尾注 → boss 对局照常生效）。
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k359_1045_arm,
                        /*once=*/false, /*early=*/true, SoulScope::STAGE,
                        /*boss_invalid=*/false),
              SOUL_NODE(State::BATTLE_ROUND_END, &k359_1045_round_end,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
