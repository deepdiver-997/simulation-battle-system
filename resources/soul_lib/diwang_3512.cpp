// 帝皇之御 3512 魂印 —— 御（effect_icon 1106，2026-09-30；场景回归 scenario_781_diwang_3512）
//
// 官方 tips 全文（effect_icon.pet_id=[3512]，args={2,2,2,1,2,50,1}）：
//   首次登场之后前2回合免疫异常状态且自身所有技能先制+2，首次登场之后前2回合受到致命伤害时
//   残留1点体力，同时将受到的攻击伤害反弹给对手；自身死亡时，使己方下只出场精灵出场后2回合内
//   受到的攻击伤害和固定伤害减少50%（免疫异常和残留体力效果BOSS有效，其余效果BOSS无效）
//
// 口径（2026-09-30 用户实测定档 + 语料）：
//   ★ 首登窗口 = 登场回合 R 起的 [R, R+1]（"到登场后第3回合的回合开始时结束"，幻影蝶帖
//     opus 875513621413625925 明说表姐/碟子同款）。**直接按登场回合做绝对回合窗，不走
//     round_effect_start_round 的后手顺延**——那是"效果注册"的折算，登场窗口以登场事实
//     起算（登场当回合被打通场攻击也要在保护内）。复活/再登场**不重置不重授**（用户实测：
//     复活后仍剩 1 回合；赛学选修9 opus 947280935819149312 把帝皇之御归入"范围检测"免死）。
//   ★ 免死 = 死亡拦截器（68/盖亚姊妹档）：cause==DAMAGE 才拦；HP_CONSUME（自爆族）/
//     秒杀（force_hp_to_zero，非伤害）/EFFECT 不拦——"自爆不触发免死""0 红伤秒杀直接死"
//     （死或生 opus 866772495396503575：攻击伤害为 0 不触发免死）。拦下写回 1 点体力走
//     heal_amount 原语（封回血覆盖当前时点则拉不起来，死亡照常成立）；窗口是**时限不是
//     次数**——用户 2026-09-30 实测：一回合内受到多少次致命伤害都不会死，逐次拦。
//   ★ 反弹 = 拦截成功时把 last_landed_hit 等量弹回攻击方，**固定伤害通道**
//     （deal_pink_damage FIXED，吃护罩/免粉/减伤）——与机盖（圣甲·盖亚 3056）死亡拦截器里
//     deal_true_damage 直通的区别：语料"魂印高伤吸血1/3可以跳表姐"（帝录天锋
//     opus 1202899642484785154）——弹伤在攻击方吸血结算**之后**落地（死亡漏斗本就跑在
//     出手流程结束时点），固定伤再吃减免 → 吸血/减伤族可避免被弹死；机盖真伤直通则不可。
//     kind 门 = NORMAL/ATTRIBUTE（红伤/属性直伤；粉伤/真伤斩杀不弹，盖亚同款）。
//     反弹后对手 hp<=0 → 显式 defeat_pet（粉伤不自动过死亡漏斗，盖亚 2026-09-24 批 9 同款）。
//     （boss 无效子句 → 反弹半边手动门 is_boss_challenge；免死半边 boss 有效。）
//   ★ 先制+2：MOVE_RIGHT 节点窗口内写 ws.preemptive_level（boss 无效 → 节点标注）。
//   ★ 免异常：Ancient 层全 mask 票（2016 老精灵按"老魂免"档，主动毒一起挡；2 回合窗口、
//     STAGE 域、窗口内再登场按剩余回合重授；boss 有效）。
//   ★ 死亡传承：EVENT_DEATH（己方 3512 阵亡）→ 己方下一只登场（W 场景 once=false+自删
//     监控同款）→ 2 回合内受到攻击伤害 -50%（ws.damage_reduce_add 每回合重写）+ 固定伤害
//     -50%（pink REDUCE_EXTRA，tier==FIXED 门——百分比伤害不减）；窗口同为登场回合起
//     [R, R+1]（boss 无效 → watcher 手动门）。
#include "common.h"

namespace {

constexpr int kDiwangPetId = 3512;
constexpr int kMarkId = 1106;

// plugin_storage 段 3512xxxx（与 moves_lib diwang_3512_skills.cpp 的 351201xx 错开）。
constexpr int kWatchersKey = 35120200;      // +side：watcher 接线标记
constexpr int kWindowArmedKey = 35120202;   // +side：首登窗口已武装（复活/再登场不重置）
constexpr int kWindowEndKey = 35120203;     // +side：窗口最后有效回合
constexpr int kReflectGuardKey = 35120204;  // +side：拦截求解中守卫（防同归互导重入）
constexpr int kSelfDestructMarkKey = 35120208; // +side：守御八方自爆待决印记（moves_lib 1275 置、本文件 SECOND_MOVER_DEATH 结算）
constexpr int kLegacyPendingKey = 35120205; // +side：待传承标记（阵亡时置、登场时消费）
constexpr int kLegacyEndKey = 35120206;     // +side：传承窗口最后有效回合（0=无）
constexpr int kLegacyPinkKey = 35120207;    // +side：传承粉伤钩子接线标记

// 免疫票固定句柄（重授同句柄 = 幂等刷新）。
constexpr int kHandleAnomaly = 11061;

bool window_active(const BattleContext* ctx, int side) {
    if (!ctx || side < 0 || side > 1) {
        return false;
    }
    auto armed = ctx->plugin_storage.find(kWindowArmedKey + side);
    auto end = ctx->plugin_storage.find(kWindowEndKey + side);
    return armed != ctx->plugin_storage.end()
        && end != ctx->plugin_storage.end()
        && any_int_or(end->second) >= ctx->roundCount;
}

bool legacy_active(const BattleContext* ctx, int side) {
    if (!ctx || side < 0 || side > 1) {
        return false;
    }
    auto end = ctx->plugin_storage.find(kLegacyEndKey + side);
    return end != ctx->plugin_storage.end() && any_int_or(end->second) >= ctx->roundCount;
}

bool on_stage_is_diwang(BattleContext* ctx, int side) {
    return ctx && side >= 0 && side <= 1 && ctx->on_stage[side] >= 0
        && ctx->on_stage[side] < 6 && ctx->getPet(side).id == kDiwangPetId;
}

// 免异常票：Ancient 层全 mask，STAGE 域（随下场掉、窗口内再登场重授）。
void grant_window_anomaly_immunity(BattleContext* ctx, int owner, int rounds) {
    if (rounds <= 0) {
        return;
    }
    ctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(),
                        /*anomaly_mask=*/0, /*duration_rounds=*/rounds, kHandleAnomaly,
                        /*soul_immunity=*/true, EffectScope::ON_STAGE, /*counts=*/0,
                        /*source_effect_id=*/kMarkId, ImmunityTier::Ancient);
}

void arm_first_entrance_window(BattleContext* ctx, int owner) {
    ctx->plugin_storage[kWindowArmedKey + owner] = 1;
    ctx->plugin_storage[kWindowEndKey + owner] = ctx->roundCount + 1;   // [R, R+1]
    grant_window_anomaly_immunity(ctx, owner, /*rounds=*/2);
}

// ── 免死（②范围即时检测，2026-09-30 迁移定稿）────────────────────────────
// 信号回调型：EVENT_HP_EXHAUSTED（伤害通道落血 ≤0 或 **0 血状态被伤害结算命中**）→
// 窗口内拉起 1 点 + 反弹。原①死亡拦截器退役——官方名单（battleFsm.cpp:62）帝皇之御
// 归②"命中即检"；信号模型下自爆（体力修正通道）不发信号 → 天然不触发免死；
// 秒杀（EVENT_HP_TO_ZERO 自有信号面）同理不触发；免死不限次数（每信号各答各的）。
// 拉起走 heal_amount 原语（封回血覆盖当前时点则拉不起来 → 死亡照常成立，盖亚同款）。
void diwang_on_hp_exhausted(BattleContext* c, int side, const BattleEvent& ev) {
    const int slot = c->on_stage[side];
    if (slot < 0 || slot >= 6) {
        return;
    }
    if (c->seerRobot[side].elfPets[slot].id != kDiwangPetId || slot != c->on_stage[side]) {
        return;
    }
    if (!window_active(c, side)) {
        return;
    }
    if (c->plugin_storage.count(kReflectGuardKey + side)) {
        return;   // 求解中守卫（反弹致死对面 → 对面结算链不重入）
    }
    // 残留 1 点体力（恢复原语：封回血覆盖当前时点则拉不起来）。
    if (g_core && g_core->heal_amount) {
        g_core->heal_amount(c, side, 1);
    }
    if (c->seerRobot[side].elfPets[slot].hp <= 0) {
        return;   // 拉不起来：免死失败，不反弹
    }
    // 反弹（boss 无效子句 → 手动门）：last_landed_hit 等量 → **固定伤害**弹回攻击方。
    // kind 门 = 红伤/属性直伤（粉伤/真伤不弹）；0 血尸体被命中（无新落血记录）时
    // 记录仍是上一次落血——kind 门排除粉伤固伤（哈莫③固伤触发免死不反弹的依据）。
    if (!c->is_boss_challenge && g_core && g_core->deal_pink_damage) {
        const BattleContext::LastLandedHit& hit = c->last_landed_hit[side][slot];
        const int actor = hit.actor;
        if (hit.valid && hit.amount > 0 && actor >= 0 && actor <= 1 && actor != side
            && (hit.kind == static_cast<int>(DamageKind::NORMAL)
                || hit.kind == static_cast<int>(DamageKind::ATTRIBUTE))) {
            c->plugin_storage[kReflectGuardKey + side] = 1;
            g_core->deal_pink_damage(c, actor, hit.amount, DamageKind::FIXED, side);
            // 粉伤不自动过死亡漏斗 → 显式判死（盖亚 2026-09-24 批 9 同款；defeat_pet 幂等）。
            if (g_core->defeat_pet
                && c->seerRobot[actor].elfPets[c->on_stage[actor]].hp <= 0) {
                g_core->defeat_pet(c, actor, c->on_stage[actor], side, DefeatCause::DAMAGE);
            }
            c->plugin_storage.erase(kReflectGuardKey + side);
        }
    }
}

// ── 传承：红伤 -50%（ROUND_START tick，ws 加算减伤槽每回合重写）──────────────
EffectResult diwang_legacy_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1 || !legacy_active(ctx, owner)) {
        return EffectResult::kOk;
    }
    int(&slots)[4] = ctx->ws.damage_reduce_add[owner];
    for (int i = 0; i < 4; ++i) {
        if (slots[i] == 0) {
            slots[i] = 50;
            break;
        }
    }
    return EffectResult::kOk;
}

// ── 传承：固定伤害 -50%（pink REDUCE_EXTRA 战斗级钩子，tier==FIXED 门）────────
void ensure_legacy_pink_hook(BattleContext* ctx, int owner) {
    if (!ctx || ctx->plugin_storage.count(kLegacyPinkKey + owner)) {
        return;
    }
    ctx->plugin_storage.emplace(kLegacyPinkKey + owner, 1);
    ctx->register_default_pink_effect(
        PinkDamagePhase::REDUCE_EXTRA, owner,
        [owner](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner) {
                return;
            }
            PinkDamageResolved& p = c->resolvedPink;
            if (p.target != owner || p.final <= 0 || p.tier != PinkDamageTier::FIXED) {
                return;
            }
            if (!legacy_active(c, owner)) {
                return;
            }
            p.final -= p.final * 50 / 100;
        });
}

// ── 传承落地：给刚登场的己方精灵挂 2 回合减伤包 ─────────────────────────────
void grant_legacy_package(BattleContext* ctx, int owner) {
    ctx->plugin_storage[kLegacyEndKey + owner] = ctx->roundCount + 1;   // [R, R+1]
    // 红伤 -50%：ROUND_START tick（登场回合起算、不随后手顺延——与免死窗口同口径）。
    Effect e(1106, 0, owner, /*duration=*/2, EffectArgs(std::vector<int>{owner}),
             &diwang_legacy_round_start);
    auto ce = std::make_unique<ContinuousEffect>(e, State::BATTLE_ROUND_START, owner, 2,
                                                 ctx->roundCount);
    ce->source_id_ = kMarkId;
    ctx->register_skill_effect(State::BATTLE_ROUND_START, owner, std::move(ce));
    ensure_legacy_pink_hook(ctx, owner);
}

// ── 节点①：ENTER_EXIT_STAGE（ROSTER）—— 接线 + 部署首登 ────────────────────
EffectResult diwang_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int host_slot = ctx->find_mark_in_roster(owner, kMarkId);
    if (host_slot < 0) {
        return EffectResult::kOk;   // 已消逝：不在任何空间
    }

    if (!ctx->plugin_storage.count(kWatchersKey + owner)) {
        ctx->plugin_storage.emplace(kWatchersKey + owner, 1);

        // 换宠登场：首登武装 / 窗口内重授免疫 / 传承消费（任何己方登场都查一遍）。
        ctx->event_center_.register_watcher(
            EventType::EVENT_ENTER_STAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [owner](BattleContext* c, const BattleEvent& ev) {
                if (!c || ev.actor != owner) {
                    return;
                }
                ElfPet& entrant = c->seerRobot[owner].elfPets[c->on_stage[owner]];
                if (entrant.id == kDiwangPetId) {
                    if (!c->plugin_storage.count(kWindowArmedKey + owner)) {
                        arm_first_entrance_window(c, owner);   // 换宠登场 = 首次登场
                    } else if (window_active(c, owner)) {
                        // 窗口内再登场：免疫票随 STAGE 作用域掉过 → 按剩余回合重授。
                        auto end = c->plugin_storage.find(kWindowEndKey + owner);
                        const int remaining =
                            any_int_or(end->second) - c->roundCount + 1;
                        grant_window_anomaly_immunity(c, owner, remaining);
                    }
                }
                // 传承：己方任一宠登场时消费（表姐自己不会再登场——已死；复活登场也算
                // "下只出场精灵"，与"复活后仍剩 1 回合免死"实测并存）。
                auto pend = c->plugin_storage.find(kLegacyPendingKey + owner);
                if (pend != c->plugin_storage.end()
                    && any_int_or(pend->second) == 1) {
                    c->plugin_storage.erase(pend);
                    grant_legacy_package(c, owner);
                }
            },
            WatcherScope::TEAM);

        // 阵亡 → 挂传承待决标记（boss 无效子句 → 手动门）+ 清自爆待决印记
        // （印记随宿主真死作废——阵亡后不追杀复活体）。
        ctx->event_center_.register_watcher(
            EventType::EVENT_DEATH, owner, ctx->roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [owner, host_slot](BattleContext* c, const BattleEvent& ev) {
                if (!c || ev.target != owner || ev.slot != host_slot) {
                    return;
                }
                c->plugin_storage.erase(kSelfDestructMarkKey + owner);
                if (c->is_boss_challenge) {
                    return;
                }
                c->plugin_storage[kLegacyPendingKey + owner] = 1;
            },
            WatcherScope::TEAM);

        // ②免死信号回调（伤害通道 0 血检测面，见 diwang_on_hp_exhausted 注）。
        ctx->event_center_.register_watcher(
            EventType::EVENT_HP_EXHAUSTED, owner, ctx->roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [owner](BattleContext* c, const BattleEvent& ev) {
                if (!c || ev.target != owner) {
                    return;
                }
                diwang_on_hp_exhausted(c, owner, ev);
            },
            WatcherScope::TEAM);
    }

    // 部署兜底：战斗开始这只在场 = 它的"首次登场"。
    if (!ctx->plugin_storage.count(kWindowArmedKey + owner)
        && ctx->on_stage[owner] == host_slot && on_stage_is_diwang(ctx, owner)) {
        arm_first_entrance_window(ctx, owner);
    }
    return EffectResult::kOk;
}

// ── 节点②：FIRST_MOVE_RIGHT（STAGE，boss 无效）—— 窗口内先制+2 ─────────────
EffectResult diwang_move_right(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !on_stage_is_diwang(ctx, owner)) {
        return EffectResult::kOk;
    }
    if (!window_active(ctx, owner)) {
        return EffectResult::kOk;
    }
    ctx->ws.preemptive_level[owner] += 2;
    return EffectResult::kOk;
}

// ── 节点③：SECOND_MOVER_DEATH（STAGE）—— 自爆待决印记结算 ──────────────────
// 守御八方的自爆改为印记形态（2026-09-30 哈莫链复刻，用户定稿机制链）：自爆当回合
// 消耗体力=清 0（体力修正通道，无信号 → 免死不触发），同时挂"待决死亡印记"——
// **后出手流程结束后的死亡检查时点**结算：印记把体力清 0（同样无信号）→ 随后的
// 死亡漏斗看到 0 → 死亡成立。若先手死亡检查前被哈莫③固伤（伤害通道）把免死激活
// 拉起 → 检查看 1 → 后手照常行动 → 印记在本节点收尾（"表姐出手又被拉起来直到
// 回合结束才死"的复刻落点）。印记只在本节点结算（不在 FIRST_MOVER_DEATH——那里
// 是固伤救场的窗口）；宿主真死时由 EVENT_DEATH 清旗（不追杀复活体）。
EffectResult diwang_self_destruct_mark_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !on_stage_is_diwang(ctx, owner)) {
        return EffectResult::kOk;
    }
    auto mark = ctx->plugin_storage.find(kSelfDestructMarkKey + owner);
    if (mark == ctx->plugin_storage.end()) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.erase(mark);
    ElfPet& self = ctx->getPet(owner);
    if (self.hp <= 0) {
        return EffectResult::kOk;   // 已倒地（未获救情形早死于第一检查点，不会到这）
    }
    self.hp = 0;   // 体力修正通道：无预死亡信号 → 免死不触发 → 随后死亡漏斗看到 0
    return EffectResult::kOk;
}

}  // namespace

SOUL_PROGRAM(kMarkId,
             {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &diwang_arm,
                              /*once=*/false, /*early=*/false, SoulScope::ROSTER),
              SoulMarkNodeRef(State::BATTLE_FIRST_MOVE_RIGHT, &diwang_move_right,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE,
                              /*boss_invalid=*/true),
              SoulMarkNodeRef(State::BATTLE_SECOND_MOVER_DEATH,
                              &diwang_self_destruct_mark_tick,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE)});
