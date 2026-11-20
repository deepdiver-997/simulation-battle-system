// K358 自身半血门攻击附伤族 —— effect 1421（1 宏 / 1 行）
//（2026-10-23，worktree kbatch；号段 7000-7009，场景 scenario_7000_half_gate_attach.cpp）
//
// 行清单（tools/soul_effects_dump.py 1421，2026-10-23）：
//   1421 时分公主 3832 row1107 args[2,150]（kind=[14]）：
//   「自身体力高于最大体力的1/2时攻击技能命中后附加150点固定伤害，自身体力低于最大体力的
//     1/2时附加的固定伤害效果翻倍」
//   → db[0] = 「1/2」的分母（字面自证）；**db[1] = 附加固伤点数**（150 自证）；
//     低体力档 = db[1] × 2（"效果翻倍"字面自证）。
//
// ⚠️ 行≠宏检查：**1421 全库仅一行**（row1107，pet 3832）——无同 effect_id 第二行，
//    不存在"后注册静默覆盖先注册"的一族从未生效风险（撞号纪律 §七）。
//
// ⚠️ 三查记录（a21-kprod 2026-10-23，a20-kprod 2026-10-23 复核）：
//   ① 内核 grep \b1421\b（include/ src/）无魂印行为硬编码；
//   ② effect_info 1421 = 技能空间异文模板「当回合打出的攻击伤害在{0}—{1}之间则{2}%令对手{3}」
//      —— 魂印以 effect_icon.tips 为权威（SOP §六）；
//   ③ 全 feat/integration 分支 soul_lib 并集无 1421 注册，registry soul 集合无 1421 —— 真缺口。
//      （moves 侧无 SKILL_EFFECT(1421)；即便有也属技能效果空间，与魂印程序空间分开。）
//
// ⚠️ 官方文义读法（**两子句互斥的严格档**，与 K245/K246 同款口径）：
//   tips 写「自身体力**高于**最大体力的1/2时…附加150点固定伤害」+「自身体力**低于**最大体力的
//   1/2时附加的固定伤害效果翻倍」——官方把同一效果写成两个带各自条件的子句（对比 1492/1536
//   那种"基准无条件 + 翻倍修饰"的写法），故按 **两分支严格互斥** 处理：
//     hp × db[0] >  max_hp → 高体力档：附加 db[1] 点固伤
//     hp × db[0] <  max_hp → 低体力档：附加 db[1] × 2 点固伤（翻倍）
//     hp × db[0] == max_hp → **两分支皆不触发**（等号无归属；记档口径，同 K347 663 / K245 / K246）
//   ⚠️ 备选读法（"基准 150 + 低体力翻倍修饰"，则等号档取 150）——官方文本两种读法都讲得通；
//     本实现按互斥严格档落地并把该歧义写进报告勘误记档，等官方实盘对账时复核。
//   ⚠️ 比大小用「乘 db[0] 对比 max_hp」而非「先除后比」——避免整数截断把「恰好一半」判错档
//     （同 K347 663 口径）。
//
// ⚠️ 实现形态（T4 类型化行参 + db_at 原序读行参；禁读 a.row）：
//   触发点 = **DETECT 相位**（伤害管线内同步执行）——"攻击技能命中后附加"是同步条件查询，
//     不用 AFTER_ACTION/TAKE_DAMAGE watcher（K1 SOP：watcher 交付被推迟到 AFTER_ACTION，
//     条件已过期；K347 663 / K348 807 同款 DETECT 直系）。
//   门 = d.attackerId == owner（只认印主自己打出的红伤）+ d.isRed（红伤；属性直伤与
//     固伤不进本管线）+ d.final > 0（未命中/被挡无 final）+ 印主魂印在场
//     （onstage_has_soul_mark）。
//   效果 = deal_pink_damage(对手, amount, FIXED, owner)（固伤档吃护罩；K347 663 / K348 807
//     同款通道）。
//   ⚠️ boss 门：tips 无（boss无效/免疫）尾注 → 节点 boss_invalid **不置位**
//     （boss 对局照常生效；K264 356 同款口径）。
//
// ⚠️ 行参纪律：显式 0 = 关闸（db[0]=0 → 分母非法，整条不触发；db[1]=0 → 附加 0 点即不附加），
//   **不得**回退到 fallback 值（在位即消费，K314/K340 两犯）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 1421;
constexpr int kDenomFallback = 2;    // tips 字面「1/2」的分母
constexpr int kFlatFallback = 150;   // 附加固伤点数
constexpr int kWiredKeyBase = 990358000;  // +owner：DETECT 票已接线旗标

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

// ── 1421 效果本体：印主攻击命中红伤时，按「自身当前体力 vs 自身最大体力的一半」分档附加固伤 ──
EffectResult k358_1421_arm(BattleContext* ctx, const SoulNodeArgs& a) {
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
            const HostRow row = host_row(c, owner);
            const int denom = db_at(row, 0, kDenomFallback);
            const int flat = db_at(row, 1, kFlatFallback);
            if (denom <= 0 || flat <= 0) {
                return;   // 显式 0 = 关闸，不回退 fallback
            }
            const ElfPet& self = c->getPet(owner);
            const int max_hp = resolve_pet_max_hp(self);
            if (max_hp <= 0 || self.hp <= 0) {
                return;
            }
            const long long scaled = static_cast<long long>(self.hp) * denom;
            int amount = 0;
            if (scaled > static_cast<long long>(max_hp)) {
                amount = flat;             // 高体力档：原值
            } else if (scaled < static_cast<long long>(max_hp)) {
                amount = flat * 2;         // 低体力档：翻倍
            } else {
                return;                    // 恰好一半：两子句皆不触发（严格档口径）
            }
            (void)g_core->deal_pink_damage(c, c->opponent(owner), amount,
                                           DamageKind::FIXED, owner);
        });
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（1421 入账，只增不改）────────
// ⚠️ boss_invalid 不置位（tips 无 boss 尾注 → boss 对局照常生效）。
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k358_1421_arm,
                        /*once=*/false, /*early=*/true, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
