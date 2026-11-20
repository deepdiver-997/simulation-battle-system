// 粉伤 · 时点族（怒涛·沧岚 2343 护盾锁粉、箫澈 2099 悲龙之鸣）
//
// （2026-09-17 从 lib_1.cpp 按家族拆分；代码零改动，行为由全量场景回归守护。）
// （2026-09-24 精灵王线：2343 补完 —— 永恒之水/蚀砚之泪体系、护盾先制、
//   战斗阶段结束分支、泪的护盾失效/无视攻击免疫。官方全文见 effect_icon
//   行 id=1932（⚠️ 行 id ≠ effect_id）：pet_id='[3740]' JSON 文本列、icon 1932
//   （高行=神谕后）→ effect_id 2343；同 pet 低行 icon 990 → effect 1316（神谕前，
//   args 丰富）。神谕开关选行见 pet_factory K1；2343 行 args 仅「1 1 1」，
//   参数以 tips 文本为准硬编码——官方数据本身不带参数。）
#include "common.h"
#include "../nutao_runtime.h"

namespace {

// ── 怒涛·沧岚 2343（**本轮只做粉伤相关子句**）──// 原文（effect_icon.pet_id=[3740], effect_id=2343, args「1 1 1」）：
//   「登场时获得400点护盾，**此护盾存在时自身受到的固定伤害、百分比伤害不超过此护盾的数值**，
//     之后自身处于护盾状态时所有技能先制+1且攻击技能使用时获得1道永恒之水，
//     自身不处于护盾状态时所有技能不消耗PP值且攻击后为对手附加1滴蚀砚之泪」
//   另有 永恒之水（攻击伤害+25%/道，至多8道）/ 蚀砚之泪（≥3滴护盾效果失效、≥4滴不可免疫与转化）
//   与回合结束子句 —— **本轮未做**（挡伤失效掩码走 nutao_pub 泪计数；旧链路 9901004/9901005 仅存档）。
//
// ★ 这是粉伤管线 **CAP 阶段**的第一个真实生产者（"上限/锁伤"）。
// ★ 判据在回调里**实时求值**（`shield_bank_.total()` + "我还在不在场上"），不缓存数值：
//   护盾每回合刷新、蚀砚之泪≥3滴会让护盾效果失效——缓存一定会过期。
//   这与 693/525「注册一次 + 回调自判窗口」是同一套路。
// ★ 注册幂等靠 `soulmark_storage`（TEAM 槽，跨切换保留）：换宠回场不会重复注册。
constexpr int kCanglanMarkId = 2343;
constexpr int kCanglanEnterShield = 400;

// ── 2343 水泪计数（本轮补完段）──────────────────────────────────────
// 永恒之水：本次在场累计 + 8 道锁定（下场保留部分）。有效道数 = max(在场, 锁定)。
inline int nutao_effective_water(BattleContext* ctx, int side) {
    const int cur = nutao_pub_get(ctx->plugin_storage, kNutaoPubWater + side);
    const int lock = nutao_pub_get(ctx->plugin_storage, kNutaoPubWaterLock + side);
    return cur > lock ? cur : lock;
}

// 沧岚被击败 → 泪清零（watcher 挂 EVENT_DEATH）。
void nutao_clear_tears_on_death(BattleContext* ctx, int owner) {
    ctx->event_center_.register_watcher(
        EventType::EVENT_DEATH, owner, ctx->roundCount,
        /*duration_rounds=*/0, /*once=*/false,
        [owner](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || ev.target != owner) {
                return;
            }
            nutao_pub_set(wctx->plugin_storage, kNutaoPubTears + owner, 0);
            nutao_pub_set(wctx->plugin_storage, kNutaoPubWater + owner, 0);
            nutao_pub_set(wctx->plugin_storage, kNutaoPubWaterLock + owner, 0);
        },
        WatcherScope::TEAM);
}

// MOVE_RIGHT（先手权判定时点）：护盾先制 +1、水增伤、泪的无视（≥3 穿盾 / ≥4 穿攻击免疫）。
EffectResult soulmark_2343_move_right(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ElfPet& me = ctx->getPet(owner);
    if (me.soulMark.id != kCanglanMarkId) {
        return EffectResult::kOk;
    }
    // ① 护盾状态 → 所有技能先制 +1（同束缚点的清零序，此处晚于它天然不被废）。
    if (me.shield_bank_.total() > 0) {
        ctx->ws.preemptive_level[owner] += 1;
    }
    // ② 永恒之水：每道攻击伤害 +25%（通用增伤加算槽，8 道封顶）。
    const int water = nutao_effective_water(ctx, owner);
    if (water > 0) {
        ctx->ws.damage_add_pct[owner] += 25 * water;
    }
    // ③ 蚀砚之泪 ≥3 滴：对手护盾效果失效 → 本次攻击无视护盾（ws 每回合 reset，逐次置位）。
    const int tears = nutao_pub_get(ctx->plugin_storage, kNutaoPubTears + owner);
    if (tears >= 3) {
        ctx->ws.ignore_shield[owner] = true;
    }
    // ④ 蚀砚之泪 ≥4 滴：对手不可免疫/转化沧岚方攻击 → PENETRATE_ATTACK 窗口票
    //    （rounds=1 当回合窗口型：查询多次不失效、不消费；1926 同款通道）。
    if (tears >= 4 && g_core) {
        ctx->grant_penetration(owner, /*level=*/1, /*ignore_attack_immunity=*/true,
                               /*ignore_damage_limit=*/false, /*counts=*/0, /*rounds=*/1,
                               RuleCategory::PENETRATE_ATTACK, EffectScope::ON_STAGE,
                               /*source_effect_id=*/kCanglanMarkId);
    }
    return EffectResult::kOk;
}

// ROUND_START：不处于护盾状态 → 本回合所有技能不消耗 PP（把消耗乘数清零）。
EffectResult soulmark_2343_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ElfPet& me = ctx->getPet(owner);
    if (me.soulMark.id != kCanglanMarkId) {
        return EffectResult::kOk;
    }
    if (me.shield_bank_.total() <= 0) {
        ctx->ws.skill_pp_cost_multiplier[owner] = 0;
    }
    return EffectResult::kOk;
}

// AFTER_ACTION（FIRST/SECOND 两半场各挂一条）：
//   用了攻击技能 → 有盾 +1 永恒之水（达 8 锁定）；无盾 → 对手 +1 滴蚀砚之泪。
EffectResult soulmark_2343_after_action(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ElfPet& me = ctx->getPet(owner);
    if (me.soulMark.id != kCanglanMarkId) {
        return EffectResult::kOk;
    }
    // 本回合选的是不是攻击技能（roundChoice 面板下标 [0]=动作 [1]=技能**槽位**，
    // bailuo_guidi_4444.cpp 同款读法）。
    bool used_attack = false;
    if (ctx->roundChoice[owner][0] == 1 /*SELECT_SKILL*/) {
        const int idx = ctx->roundChoice[owner][1];
        if (idx >= 0 && idx < 5) {
            used_attack = me.skills[idx].type != SkillType::Attribute;
        }
    }
    if (!used_attack) {
        return EffectResult::kOk;
    }
    if (me.shield_bank_.total() > 0) {
        const int cur = nutao_pub_get(ctx->plugin_storage, kNutaoPubWater + owner);
        if (cur < 8) {
            nutao_pub_set(ctx->plugin_storage, kNutaoPubWater + owner, cur + 1);
            if (cur + 1 >= 8) {
                nutao_pub_set(ctx->plugin_storage, kNutaoPubWaterLock + owner, 8);
            }
        }
    } else {
        const int tears = nutao_pub_get(ctx->plugin_storage, kNutaoPubTears + owner);
        if (tears < 4) {
            nutao_pub_set(ctx->plugin_storage, kNutaoPubTears + owner, tears + 1);
        }
    }
    return EffectResult::kOk;
}

// 战斗阶段结束：受攻伤 → 抵挡下 1 次攻击；未受 → 恢复 25% + 等量百分比伤害。
// "当回合自身受到了攻击伤害" = red-flag：EVENT_TAKE_DAMAGE watcher 置位（红伤时点过滤）。
EffectResult soulmark_2343_arm_phase_end(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    auto& storage = ctx->getPet(owner).soulmark_storage;
    constexpr int kSlotKey = kCanglanMarkId * 100 + 92;
    if (storage.count(kSlotKey)) {
        return EffectResult::kOk;   // watcher 一次挂永久（TEAM）
    }
    storage[kSlotKey] = true;
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount,
        /*duration_rounds=*/0, /*once=*/false,
        [owner](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || ev.target != owner || ev.amount <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(wctx, owner, kCanglanMarkId)) {
                return;
            }
            wctx->ws.nutao_took_attack_damage[owner] = true;
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

EffectResult soulmark_2343_phase_end(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ElfPet& me = ctx->getPet(owner);
    if (me.soulMark.id != kCanglanMarkId) {
        return EffectResult::kOk;
    }
    const int max_hp = resolve_pet_max_hp(me);
    if (ctx->ws.nutao_took_attack_damage[owner]) {
        // 抵挡下 1 次攻击（次数型 DAMAGE 免疫票，红伤管线 BLOCK 阶段消费）。
        ctx->grant_immunity(owner, ImmunityType::DAMAGE, attack_damage_coverage(),
                            /*anomaly_mask=*/0, /*duration_rounds=*/0,
                            /*source_id=*/0, /*soul_immunity=*/false,
                            EffectScope::ON_STAGE, /*counts=*/1,
                            /*source_effect_id=*/kCanglanMarkId);
    } else {
        const int heal = max_hp * 25 / 100;
        if (heal > 0 && g_core && g_core->heal_amount) {
            g_core->heal_amount(ctx, owner, heal);
        }
        if (heal > 0 && g_core && g_core->deal_pink_damage) {
            g_core->deal_pink_damage(ctx, 1 - owner, heal, DamageKind::PERCENT_VALUE, owner);
        }
    }
    ctx->ws.nutao_took_attack_damage[owner] = false;   // 回合界清旗
    return EffectResult::kOk;
}

EffectResult soulmark_2343_on_enter(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    // early 节点防双跑守卫：开局 early（activate）+ ENTER_EXIT_STAGE 桶执行各跑一次，
    // on_stage_storage 下场即清 → 每次登场恰好执行一次（加盾/清水不幂等，必须挡）。
    auto& presence = ctx->getPet(owner).on_stage_storage;
    constexpr int kPresenceKey = kCanglanMarkId * 100 + 93;
    if (presence.count(kPresenceKey)) {
        return EffectResult::kOk;
    }
    presence[kPresenceKey] = true;
    // ① 登场时获得 400 点护盾。
    //    · **priority = kSoulShieldPriority（最高）**：与其他护盾叠加时**优先被消耗**
    //      —— 官方"优先消耗登场盾"（用户 2026-09-15）。
    //    · **source_id = 魂印 id**：下面那条限粉只认**这一笔**（`quantity_of`），
    //      不能被别人叠上来的护盾把上限一起抬高。duration=0 = 破盾前持续。
    ctx->getPet(owner).shield_bank_.add(ShieldBank::kSoulShieldPriority, kCanglanEnterShield,
                                        /*source_id=*/kCanglanMarkId, /*duration=*/0);
    // 补完段：本次在场的永恒之水清零（<8 道下场不保留；8 道锁定部分在独立槽不动）+
    // 挂"沧岚被击败 → 泪/水全清"watcher（官方"沧岚被击败时消失"）。
    nutao_pub_set(ctx->plugin_storage, kNutaoPubWater + owner, 0);
    nutao_clear_tears_on_death(ctx, owner);
    // ② 「受到的固定伤害、百分比伤害不超过此护盾的数值」→ 粉伤管线 CAP 阶段。
    auto& storage = ctx->getPet(owner).soulmark_storage;
    constexpr int kSlotKey = kCanglanMarkId * 100 + 91;
    if (storage.count(kSlotKey)) {
        return EffectResult::kOk;   // 已注册（换宠回场不重复注册）
    }
    storage[kSlotKey] = true;
    ctx->register_pink_damage_effect(
        PinkDamagePhase::CAP, owner,
        [owner](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner) {
                return;
            }
            if (!onstage_has_soul_mark(c, owner, kCanglanMarkId)) {
                return;   // 换宠/失去魂印 → 该条失效
            }
            PinkDamageResolved& r = c->resolvedPink;
            if (r.target != owner || r.final <= 0) {
                return;
            }
            // ★ **只认登场那一笔**（source_id = 魂印 id），不是 `total()`：
            //   条款原文是"不超过**此**护盾的数值"——别的来源叠上来的盾不该抬高上限。
            const int shield = c->getPet(owner).shield_bank_.quantity_of(kCanglanMarkId);
            if (shield <= 0) {
                return;   // 「此护盾存在时」——这笔盾被打光/清掉就不限制
            }
            if (r.final > shield) {
                r.final = shield;
            }
        },
        // TEAM 绑定：魂印条款不随切换作废（换宠回场时这笔盾还在）。
        // 场上与否由回调里的 `onstage_has_soul_mark` 实时判，不靠 epoch。
        EffectScope::TEAM);
    return EffectResult::kOk;
}

// ── 箫澈 2099 · 悲龙之鸣（**本轮只做粉伤触发的那一支**）──
// 原文（effect_icon.pet_id=[4496], effect_id=2099, args「300 1 3 300 1 3 1」）：
//   「每次自身受到≥300的攻击伤害则会触发哀霆之声：免疫下1次受到的固定伤害、百分比伤害
//     且附加对手最大体力1/3的百分比伤害；
//     每次自身受到≥300的固定伤害、百分比伤害则会触发悲龙之鸣：免疫下1次受到的攻击伤害
//     且恢复自身最大体力的1/3；
//     战斗阶段结束时，若当回合自身均未受到攻击伤害、固定伤害、百分比伤害，
//     则消除对手回合类效果和能力提升状态…（哀霆之声与悲龙之鸣在自身体力为0时不会触发）」
//
// ★ 这是 `EVENT_TAKE_PINK_DAMAGE` 的**首个消费者**，也是「**多段粉逐段判定**」的活例：
//   事件**每段各发一次**，于是"每次受到≥300"天然按段判定——
//   4 段各 300 会触发 4 次；合并成 1200 只触发 1 次。两者不是一回事。
// ★ 判据是事件里的 `amount` = **实际扣血量**（体力真的降了多少），不是抗性前的原始量：
//   用户 2026-09-15 口径——这类检测就是即时算一段粉、当场应用、再看体力有没有降。
//   推论（已被护罩吃掉的那一段**不发事件**）：靠护罩抵消掉的粉伤**不触发**悲龙之鸣，
//   而"护罩吃掉一部分、剩下一点落到本体"则会触发（触发量按**落到本体的那部分**算）。
// ★ 未做：哀霆之声（红伤触发，需要"下1次固定/百分比伤害免疫"的**次数型粉免**——
//   现有 `pink_immune` 只是一个 bool，没有次数语义）；战斗阶段结束子句。
// （`attack_damage_coverage()` 是跨家族共享件，拆分时上移到 common.h）

constexpr int kXiaocheMarkId = 2099;
constexpr int kXiaochePinkThreshold = 300;

EffectResult soulmark_2099_arm_beilong(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    auto& storage = ctx->getPet(owner).soulmark_storage;
    constexpr int kSlotKey = kXiaocheMarkId * 100 + 91;
    if (storage.count(kSlotKey)) {
        return EffectResult::kOk;   // arm 一次（TEAM 永久）
    }
    storage[kSlotKey] = true;
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_PINK_DAMAGE, owner, ctx->roundCount,
        /*duration_rounds=*/0, /*once=*/false,
        [owner](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || ev.target != owner) {
                return;
            }
            if (!onstage_has_soul_mark(wctx, owner, kXiaocheMarkId)) {
                return;   // 换宠/失去魂印后失效
            }
            if (wctx->getPet(owner).hp <= 0) {
                return;   // 「自身体力为0时不会触发」
            }
            if (ev.amount < kXiaochePinkThreshold) {
                return;   // ★ 逐段判定：本段没到 300 就不触发
            }
            // ① 免疫下1次受到的攻击伤害（次数型 DAMAGE 免疫，由红伤管线 BLOCK 阶段消费）。
            //    本段粉伤已经落地，故只作用于**之后**的攻击。
            wctx->grant_immunity(owner, ImmunityType::DAMAGE, attack_damage_coverage(),
                                 /*anomaly_mask=*/0, /*duration_rounds=*/0,
                                 /*source_id=*/0, /*soul_immunity=*/false,
                                 EffectScope::ON_STAGE, /*counts=*/1,
                                 /*source_effect_id=*/kXiaocheMarkId);
            // ② 恢复自身最大体力的 1/3（走 heal 原语：吃封回血 + 恢复修正）
            if (g_core && g_core->heal_amount) {
                g_core->heal_amount(wctx, owner,
                                    resolve_pet_max_hp(wctx->getPet(owner)) / 3);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表）──
    // 怒涛·沧岚 2343（2026-09-24 补完）：
    //   ENTER_EXIT_STAGE = 登场 400 盾 + 粉伤 CAP + 泪清零 watcher + 受击旗 watcher；
    //   ROUND_START = 无盾回合免 PP 消耗；MOVE_RIGHT = 盾先制/水增伤/泪无视；
    //   AFTER_ACTION×2 = 攻击后水+1 或 泪+1；ROUND_END = 受攻伤/未受分支。
    // ⚠️ on_enter 必须 early=true（官方"登场时获得400点护盾"= 每次登场重发；
    //    非 early 的 ENTER_EXIT_STAGE 节点只在开局执行一次，切上场后盾不再回来
    //    ——2026-09-26 精灵王线登场语义批次统一修正，见 scenario_130 A 组）。
    SOUL_PROGRAM(2343, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_2343_on_enter,
                         /*once=*/false, /*early=*/true),
                        SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_2343_arm_phase_end,
                         /*once=*/false, /*early=*/false),
                        SoulMarkNodeRef(State::BATTLE_ROUND_START, &soulmark_2343_round_start),
                        SoulMarkNodeRef(State::BATTLE_FIRST_MOVE_RIGHT, &soulmark_2343_move_right),
                        SoulMarkNodeRef(State::BATTLE_FIRST_AFTER_ACTION, &soulmark_2343_after_action),
                        SoulMarkNodeRef(State::BATTLE_SECOND_AFTER_ACTION, &soulmark_2343_after_action),
                        SoulMarkNodeRef(State::BATTLE_ROUND_END, &soulmark_2343_phase_end)});
    // 箫澈 2099（**只做悲龙之鸣——粉伤触发的那一支**）：
    //   OPERATION_ENTER_EXIT_STAGE 注册 EVENT_TAKE_PINK_DAMAGE watcher（TEAM 永久），
    //   判定 `ev.amount`（实际扣血）**逐段**是否 ≥300。
    SOUL_PROGRAM(2099, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_2099_arm_beilong,
                         /*once=*/false, /*early=*/false)});

}  // namespace
