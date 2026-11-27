// 古早挡伤 / 反弹族（索伦森 1001、兰斯洛特 1002、犀牛 1217）
//
// （2026-09-17 从 lib_1.cpp 按家族拆分；代码零改动，行为由全量场景回归守护。）
//
// ⚠️ 2026-10-21 方案 B 迁移**豁免**（本文件不迁 a.row_or）：
//    这里的 1001/1002/1003 是**手写魂印 id**，不对应 effect_icon.effect_id：
//    DB 里 effect_id=1001 是「珂莱蒂尔 3401」的护盾效果（args `200 0`），与
//    「索伦森 30% 概率挡红伤」毫无关系；全库 tips 也搜不到"30% 概率挡红伤"这一子句
//    （最接近的是 effect 1376「受到攻击时5%的概率抵挡当回合受到的攻击伤害」args=`20 5`）。
//    即索伦森这个魂印在 effect_icon 里**没有对应行** → 无处可读 db_at。
//    保留 a.row_or(0, 30)（= 绑定位 2 起读，真实对局为空 → 落 fallback 30）。
//    （S19 复核 2026-11-05：全库 row_or 现存仅本文件与 canglan_tears.cpp 两处，
//    均属手写魂印 id 豁免范围，维持不迁。）
#include "common.h"

namespace {

EffectResult soulmark_damage_guard_sorensen(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const int chance = a.row_or(0, 30);

    if (!roll_soul_percent(*ctx, 9901001, 0, owner, owner, chance)) {
        return EffectResult::kOk;
    }

    try_block_current_red_damage(ctx, owner);
    return EffectResult::kOk;
}

EffectResult soulmark_damage_guard_lancelot_crit_reflect(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }

    const int owner = resolve_owner_from_args(ctx, args);
    const int attacker = resolve_target_from_args(ctx, args, owner);
    DamageSnapshot& damage = ctx->resolvedDamage;

    if (damage.defenderId != owner || !damage.isRed || !damage.isCrit || damage.final <= 0) {
        return EffectResult::kOk;
    }

    const int reflected = damage.final;
    if (try_block_current_red_damage(ctx, owner)) {
        reflect_damage_to_attacker(ctx, attacker, reflected);
    }
    return EffectResult::kOk;
}

// ── 不灭地威·萨瑞卡 1217「犀牛」──
// 原文（effect_icon.pet_id=[3637], effect_id=1217；dump 2026-10-26 两行：
//   row 928 args[1,1,350,350,50,1] 主行 / row 1474 args[1,1,340,340,50,1]
//   「X之战意伙伴效果」变体——1474 是 **effect_icon 行号**而非独立 effect_id，
//   同宏 1217 参数驱动，回合开始子句两行文本一致）：
//   「回合开始时令自身防御+1、特防+1、速度+1，令对手防御-1、特防-1、速度-1；
//     自身受到的攻击伤害大于350则恢复自身所有体力，自身受到的攻击伤害小于350则反弹给对手
//     造成伤害50%的固定伤害（boss无效）」
// 受击两分支（K1 落地）+「回合开始六项能力变化」（K393 补齐，见 round_start 节点）。
//
// ★ 判定值取 **链首快照** `ws.guard_detect_damage`（core 在 GUARD_DETECT 阶段写入）——
//   含暴击与暴击抗性减免、**未含任何增伤/减伤**。这正是官方时点链
//   「**犀牛魂印—通用增伤—693增伤—保底伤害—护盾**」的第一位：
//   后半程的 693 增伤（`AMP_EXTRA`）把实际伤害顶过 350 时犀牛**看不到**，于是既不回满血、
//   伤害也照穿 = 经典现象「**693 增伤乱穿犀牛**」（检测早于增伤）。
// ★ 回血/反弹挂在 **EVENT_TAKE_DAMAGE**（伤害**落地之后**）而不是 `_AFTER_ACTION` 时点：
//   那些时点的桶**只跑当回合 mover 一侧**，犀牛作为非 mover 时根本不执行（谱尼虚无同款坑）；
//   而且"恢复自身所有体力"是**受了伤之后**才回，必须晚于扣血。
// ★ 回满血走 `heal_amount` **原语**（吃封回血 `HEAL_BLOCK` + `heal_mod_pct` 恢复修正 +
//   记 `last_heal_amount`），不再直写 hp。
constexpr int kSarikaMarkId = 1217;
constexpr int kSarikaThreshold = 350;   // 原文字面值（另有一条 340 的 boss 变体 effect_icon row 1474，见头注）

bool onstage_has_sarika_mark(BattleContext* ctx, int owner) {
    if (!ctx || owner < 0 || owner > 1) {
        return false;
    }
    const ElfPet& pet = ctx->seerRobot[owner].elfPets[ctx->on_stage[owner]];
    return pet.soulMark.id == kSarikaMarkId;
}

struct SarikaRuntime {
    bool watcher_armed = false;
};

SarikaRuntime& sarika_runtime(BattleContext* ctx, int owner) {
    auto& storage = ctx->getPet(owner).soulmark_storage;
    constexpr int kSlotKey = kSarikaMarkId * 100 + 91;   // 与其它魂印的 any 槽错开
    if (!storage.count(kSlotKey)) {
        storage[kSlotKey] = SarikaRuntime{};
    }
    return std::any_cast<SarikaRuntime&>(storage[kSlotKey]);
}

EffectResult soulmark_1217_arm_sarika(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    SarikaRuntime& rt = sarika_runtime(ctx, owner);
    if (rt.watcher_armed) {
        return EffectResult::kOk;   // arm 一次（TEAM 永久）
    }
    rt.watcher_armed = true;
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || ev.target != owner || ev.actor == owner) {
                return;   // 只认"对手造成的"
            }
            if (!onstage_has_sarika_mark(wctx, owner)) {
                return;   // 换宠/失去魂印后失效
            }
            // 只认**攻击伤害**时点（粉伤/真伤不走伤害管线，时点不是这两个）。
            if (ev.state != static_cast<int>(State::BATTLE_FIRST_ATTACK_DAMAGE)
                && ev.state != static_cast<int>(State::BATTLE_SECOND_ATTACK_DAMAGE)) {
                return;
            }
            const int chain_head = wctx->ws.guard_detect_damage[owner];
            if (chain_head > kSarikaThreshold) {
                // 受高伤 → 恢复自身所有体力（走 heal 原语）
                if (g_core && g_core->heal_amount) {
                    g_core->heal_amount(wctx, owner, resolve_pet_max_hp(wctx->getPet(owner)));
                }
            } else if (chain_head > 0) {
                // 受低伤 → 反弹"造成伤害 50%"的固定伤害给攻击方
                if (g_core && g_core->deal_pink_damage) {
                    g_core->deal_pink_damage(wctx, ev.actor, chain_head / 2,
                                             DamageKind::FIXED, owner);
                }
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// 回合开始六项能力变化（K393，2026-10-26）：
//   「回合开始时令自身防御+1、特防+1、速度+1，令对手防御-1、特防-1、速度-1」。
//   · 自身三项走 `stat_change`（自身增益不查免弱；K4 612/636 同款 ROUND_START 形态）；
//   · 对手三项走 `stat_drop` 弱化原语（actor=施术者=萨瑞卡；原语内查附加免弱/
//     强化保护、钳 ±6，amount=下降低度须为正——k1210 头注同款坑）；
//   · 槽号 = 引擎等级槽序（0攻 1特攻 2防 3特防 4速 5命中，速查 §三）：
//     原文「防御/特防/速度」按语义序映射恰为 2/3/4，无双序坑。
//   · boss_invalid 不置位：dump 行文「（boss无效）」尾注在**受击子句**（tips 按 | 分句
//     的第二句）句尾，标注单位=带尾注的子句（plugin_interface.h 口径）→ 回合开始
//     子句不带尾注，本节点照常注册（记档：既有受击 arm 节点未置位属受击分支范围，
//     本单不动）。
EffectResult soulmark_1217_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_sarika_mark(ctx, owner)) {
        return EffectResult::kOk;
    }
    const int opp = 1 - owner;
    if (g_core && g_core->stat_change) {
        g_core->stat_change(ctx, owner, /*防御槽=*/2, +1);
        g_core->stat_change(ctx, owner, /*特防槽=*/3, +1);
        g_core->stat_change(ctx, owner, /*速度槽=*/4, +1);
    }
    if (g_core && g_core->stat_drop) {
        (void)g_core->stat_drop(ctx, opp, /*防御槽=*/2, /*amount=*/1, owner);
        (void)g_core->stat_drop(ctx, opp, /*特防槽=*/3, /*amount=*/1, owner);
        (void)g_core->stat_drop(ctx, opp, /*速度槽=*/4, /*amount=*/1, owner);
    }
    return EffectResult::kOk;
}

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表）──
    // ⚠️ 私有号段（2026-10-21 撞号治理）：1001/1002 原是手写魂印 id，与官方
    //    effect_icon 行（1001=珂莱蒂尔3401 护盾、1002=3405 兰斯洛）撞号——pet_factory
    //    按官方行造宠会误触发本文件程序。已迁 9901xxx 私有号段；官方 1001/1002 语义
    //    未实现，现按"未注册=开机审计警告"呈现（effect.cpp 启动审计）。
    // 9901001: 索伦森概率挡红伤（原 1001，无 DB 行，fallback 30 为生产值）
    // 9901002: 兰斯洛特免疫并反弹暴击伤害（原 1002，同上）
    // 1003: 萨瑞卡受到超过 350 红伤后回满（未单独注册，并入 1217 程序语义）
    SOUL_MARK_TYPED(9901001, soulmark_damage_guard_sorensen);
    SOUL_MARK(9901002, soulmark_damage_guard_lancelot_crit_reflect);
    // 不灭地威·萨瑞卡 1217（"犀牛"）：受高伤回满血 / 受低伤反弹 50% 固定伤害 +
    //   回合开始六项能力变化（K393 补齐，两行变体同覆盖——回合子句无行参差异）。
    //   单节点 arm：OPERATION_ENTER_EXIT_STAGE 注册 EVENT_TAKE_DAMAGE watcher（TEAM 永久），
    //   判定值取链首快照 `ws.guard_detect_damage`（增伤前）。
    //   ROUND_START 节点：自身 防/特防/速 +1 + 对手三槽 stat_drop（STAGE scope，
    //   持有者不在场不跑；boss_invalid 不置位——尾注属受击子句，见 handler 注释）。
    SOUL_PROGRAM(1217, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_1217_arm_sarika,
                         /*once=*/false, /*early=*/false),
                        SoulMarkNodeRef(State::BATTLE_ROUND_START, &soulmark_1217_round_start)});

}  // namespace
