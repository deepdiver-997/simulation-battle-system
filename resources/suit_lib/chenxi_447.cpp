// 晨曦之星战甲（套装 id 447，部件 1301035-1301038 晨曦战盔/机甲/束腰/战靴）
//
// 官方全文（equip.bytes desc；suit.bytes id=447）：
//   背包内所有精灵速度、体力+60，回合开始时若己方在场精灵不处于异常状态则当回合
//   忽略对手10点速度；己方在场精灵造成的攻击伤害提升10%且自身攻击后附加100点固定
//   伤害，体力高于1/2时效果翻倍；己方在场精灵受到的攻击伤害减少10%且对手攻击后
//   自身恢复100点体力，体力低于1/2时效果翻倍；（仅限赛尔与赛尔间对战）
//
// 拆解（数值部分走 custom_equip_stats 数据层，见 suit.cpp resolve_equipment）：
//   A 速度、体力+60          → 数据层 per_suit 行（不动效果）
//   B 忽略对手10点速度        → ROUND_START 节点：己方在场无异常 → ws.speed_ignore_points[对手] += 10
//   C 攻击伤害提升10%         → ATTACK_DAMAGE 节点：ws.damage_add_pct[己] += 10（加算通用增伤）；
//       体力>1/2 翻倍 → +20（结算时按当前体力判定）
//   D 自身攻击后附加100固伤    → EVENT_HIT 监听（actor==己方、攻击技能）→ deal_pink_damage；
//       攻击方体力>1/2 → 200
//   E 受到的攻击伤害减少10%    → 伤害管线 REDUCE_PCT 常驻条目（TEAM，激活时装一次）；
//       受击方体力<1/2 → 20%
//   F 对手攻击后自身恢复100体力 → EVENT_HIT 监听（actor==对手、攻击技能）→ heal_amount；
//       受击方体力<1/2 → 200
//
// 为什么 D/F 走 EVENT_HIT 而不是 AFTER_ACTION 桶：时点桶只执行 mover 一侧
// （execute_registered_actions(first_mover_id, ...)），防守方的套装效果在对手行动后
// 没有执行点；EVENT_HIT 每次技能命中恰好发一次（skills.cpp，属性技能也算命中 →
// 回调里用 skill_type_view 过滤掉非攻击技能），是"攻击后"的正确粒度。
// 翻倍判定取**结算时**当前体力；边界（恰为 1/2）不翻倍（高于/低于的严格口径）。
// 生命周期：全部 TEAM 常驻（套装纪律，见 suit_reg.h）。

#include "common.h"

namespace {

constexpr int kSuitChenxi = 447;
constexpr int kNodeInstall = 0;   // early：装 E 管线条目 + D/F 监听器（幂等）
constexpr int kNodeRoundStart = 1;
constexpr int kNodeAtkDmg = 2;    // FIRST/SECOND_ATTACK_DAMAGE 共用函数，节点 id 不同

inline bool hp_above_half(const ElfPet& pet) {
    const int max_hp = resolve_pet_max_hp(pet);
    return max_hp > 0 && pet.hp * 2 > max_hp;
}
inline bool hp_below_half(const ElfPet& pet) {
    const int max_hp = resolve_pet_max_hp(pet);
    return max_hp <= 0 || pet.hp * 2 < max_hp;
}

// 本次命中是否是**攻击技能**（EVENT_HIT 对属性技能也发，官方"攻击后"不含属性技能）。
inline bool hit_was_attack(BattleContext* ctx, int actor) {
    return ctx->ws.skill_type_view[actor] != static_cast<int>(SkillType::Attribute);
}

// ── E + D/F 的安装节点（early，幂等）────────────────────────────────────
EffectResult chenxi_install(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int key = kSuitChenxi * 10 + owner;
    if (ctx->plugin_storage.count(key)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage[key] = true;

    // E：受到的攻击伤害减少 10%/20%（红伤；受击方体力<1/2 翻倍）。
    // MITIGATE 类别当前无抑制源（蚀砚之泪只设 BLOCK|DETECT 位）；若未来出现
    // MITIGATE 抑制，为守"套装不可屏蔽"口径需换非抑制类别（届时收口）。
    ctx->register_damage_effect(
        DamagePhase::REDUCE_PCT,
        owner,
        DamageEffectCategory::MITIGATE,
        [](BattleContext* pctx, int bucket_owner) {
            if (!pctx) {
                return;
            }
            const DamageSnapshot& damage = pctx->resolvedDamage;
            if (damage.defenderId != bucket_owner || !damage.isRed || damage.final <= 0) {
                return;   // 只减"受到的攻击伤害"（红伤）
            }
            ElfPet& defender = on_stage_pet(pctx, bucket_owner);
            const int reduce = hp_below_half(defender) ? 20 : 10;
            // ★ 并入**通用减伤加算钳**（通用减伤叠加超 100% 修正为 100%——红伤归零；2026-09-20 用户
            //   语境：晨曦 -10% 要与克雷守备 -40 / 构造II -50 等同桶叠加 → 恰好 100% 归零）。
            //   实现管线条目在默认 REDUCE_PCT 之后跑：从当前 final 反解"未含加算减伤的基数"，
            //   按桶值+套装的总额（钳 100）重算 final。桶已 ≥100 时 final 本就为 0，上方守卫早退。
            int bucket_sum = 0;
            const int(&buckets)[4] = pctx->damage_reduce_add[bucket_owner];
            for (int i = 0; i < 4; ++i) {
                bucket_sum += buckets[i];
            }
            if (bucket_sum >= 100 || bucket_sum <= 0) {
                return;   // 桶已打满（归零）/ 桶没参与（本条独立 -10/20% 语义保留）
            }
            const int total = bucket_sum + reduce > 100 ? 100 : bucket_sum + reduce;
            const int base = pctx->resolvedDamage.final * 100 / (100 - bucket_sum);
            pctx->resolvedDamage.final = base * (100 - total) / 100;
        },
        EffectScope::TEAM
    );

    // D/F：EVENT_HIT 监听（TEAM、永久）。一次命中只跑其一：actor==己方 → D；
    // actor==对手 → F。D 的固伤走粉伤管线（EVENT_TAKE_PINK_DAMAGE），不会再进本监听。
    ctx->event_center_.register_watcher(
        EventType::EVENT_HIT,
        owner,
        ctx->roundCount,
        /*duration_rounds=*/0,   // 永久（战斗结束 clear_all 清）
        /*once=*/false,
        [owner](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || !g_core) {
                return;
            }
            if (ev.actor < 0 || ev.actor > 1 || !hit_was_attack(wctx, ev.actor)) {
                return;
            }
            if (ev.actor == owner) {
                // D：自身攻击后附加固伤（打命中目标；目标若已阵亡按**当前在场**结算，
                // 官方无明说，追打 replacement 是保守口径）。
                ElfPet& attacker = on_stage_pet(wctx, owner);
                const int amount = hp_above_half(attacker) ? 200 : 100;
                g_core->deal_pink_damage(wctx, 1 - owner, amount, DamageKind::FIXED, owner);
            } else {
                // F：对手攻击后回血（回复吃封回血/恢复修正，走 heal_amount 原语）。
                ElfPet& defender = on_stage_pet(wctx, owner);
                const int amount = hp_below_half(defender) ? 200 : 100;
                g_core->heal_amount(wctx, owner, amount);
            }
        },
        WatcherScope::TEAM,
        /*valid_id=*/0
    );
    return EffectResult::kOk;
}

// ── B：回合开始，己方在场无异常 → 忽略对手 10 点速度 ────────────────────
EffectResult chenxi_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (!ctx->has_any_active_abnormal_status(owner)) {
        ctx->ws.speed_ignore_points[1 - owner] += 10;
    }
    return EffectResult::kOk;
}

// ── C：己方造成攻击伤害提升 10%（体力>1/2 时 20%）——加算通用增伤槽 ─────
EffectResult chenxi_attack_damage(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    ElfPet& attacker = on_stage_pet(ctx, owner);
    ctx->ws.damage_add_pct[owner] += hp_above_half(attacker) ? 20 : 10;
    return EffectResult::kOk;
}

}  // namespace

// 注册：early 节点（安装）+ ROUND_START（速度忽略）+ 双方 ATTACK_DAMAGE（增伤）。
// ATTACK_DAMAGE 桶只跑 mover 一侧 → 己方攻击时两个节点只有一个会执行，恰好一次。
SUIT_PROGRAM(kSuitChenxi,
             SoulMarkNodeRef(State::BATTLE_ROUND_START, &chenxi_install,
                             /*once=*/false, /*early=*/true),
             SoulMarkNodeRef(State::BATTLE_ROUND_START, &chenxi_round_start),
             SoulMarkNodeRef(State::BATTLE_FIRST_ATTACK_DAMAGE, &chenxi_attack_damage),
             SoulMarkNodeRef(State::BATTLE_SECOND_ATTACK_DAMAGE, &chenxi_attack_damage))
