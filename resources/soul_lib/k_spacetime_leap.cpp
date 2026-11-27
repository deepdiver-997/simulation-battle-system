// K7 · 时空跃迁/叠层减伤复杂族 —— 1639/1903（时空界神 4050 双宏同模板）+ 1673（迦曼吉 4080）
//                                                                          （2026-10-04，worktree ../agents/k7）
//
// 行清单（tools/soul_effects_dump.py 1639 1903 1673 1057，2026-10-04）：
//   1639 row1299 / 1903 row1543（时空界神 4050，args 同为 [3,100,2,1,2,1,3,1]）：
//     ①"受到的攻击伤害≥自身当前体力值 → 时空跃迁：免疫本次攻击伤害+解除自身异常，
//       下3回合闪避率提升至100%（每场战斗仅触发1次/2次）"——次数差异为**宏常量**
//       （1639=1 / 1903=2，args 不区分，工单行口径）；②"回合开始时体力高于 1/2 →
//       当回合攻击必暴+随机1技PP归零；低于 1/2 → 当回合全技能先制+1+吸取对手最大
//       体力 1/3（boss无效）"
//   1673 row1323（迦曼吉 4080，args [10,10,5,3,3,1]）：
//     ①"每持续战斗1回合 → 对手造成伤害-10%且攻击命中-10%，最高叠5层"；
//     ②"当回合对手攻击未命中 → 战斗阶段结束时减对手最大体力1/3；命中 → 恢复自身
//       最大体力1/3（boss无效）"
//   1057 row774（赤渊之蛇 3458，args [250,70,1]）补子句（K3 ①反弹已落地，②③本片补）：
//     ②"先出手时当回合自身闪避率提升70%" ③"后出手时自身攻击技能必暴（BOSS无效）"
//
// 实现口径：
//   - 跃迁检测 = **GUARD_DETECT 链首快照** ws.guard_detect_damage[owner] ≥ 当前体力
//     （犀牛 1217/K4 612 同口径：增伤前）；免伤落 **BLOCK 阶段**整段归零（138 同款）。
//     每场次数 = plugin_storage 计数（键 990163900+宏，战斗级）；解异常 =
//     dispel_active_anomalies；闪避窗 = NextRounds(3) ROUND_START soulmark tick 写
//     ws.dodge_rate=1.0（下3回合起——NextRounds 不含当回合）。
//   - 1639/1903 clause B = ROUND_START soulmark 节点：体力×2 vs max 分支 →
//     ws.must_crit（chaos_mozun 通道）+ 随机1技 PP=0（pet.skills[idx].pp 直改，
//     bailuo/crawford 先例）｜ ws.preemptive_level+1 + drain_hp(1/3)（args[6]=3）。
//   - 1673 叠层 = ROUND_START soulmark 节点：层数++（cap5，plugin_storage 持久）→
//     ws.damage_add_pct[opp] -= 10×层（AMP 通道=对手输出降）+
//     ws.hit_rate_mod_attack[opp] = 1-0.1×层（S1-P0 乘区，线性叠层记档）；
//     clause B = MISS/HIT 旗（EVENT_SKILL_INVALID/EVENT_HIT + 1320 攻击技过滤）→
//     ROUND_END soulmark 节点结算：reduce_max_hp_pct(1/3→33 取整记档) / heal(max/3)。
//   - 1057 补子句 = MOVE_RIGHT 路由节点追加进 k_thorn_reflect 的 SOUL_PROGRAM(1057)
//     列表（宏按 id 命名静态符号，同 id 只能一处注册）→ 每回合注册 FIRST/SECOND
//     ACTION_START 一次性 tick（1527 两级注册同款）：先手域 → dodge_rate=0.7f；
//     后手域 → must_crit（is_boss_challenge 手动门，diwang 判例）。
//
// 动工前置三查（2026-10-04，工单行已核）：①内核 grep 1639/1903/1673 无硬编码
//   ②effect_icon kind 已核 ③registry soul=75 无四 id、未合并分支无实现。
// 先例：K4 612（链首快照消费）、J82 138（BLOCK 整段归零）、chaos_mozun（must_crit）、
//   J155 725（reduce_max_hp_pct）、J136 1527（两级注册）、J129 177（1320 攻击技过滤）。
// 场景回归：test/scenario/scenario_2840_spacetime_leap.cpp（2840 号段）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

// ── 宿主行参数（"行≠宏"：全量 DB args + monster_id，K2 同款私副本）──────────
struct HostRow {
    const std::vector<int>* db = nullptr;
    int monster_id = 0;
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

bool watcher_wired(BattleContext* ctx, int macro, int owner) {
    const int key = 9901673900 + macro * 10 + owner;
    if (ctx->plugin_storage.count(key)) {
        return true;
    }
    ctx->plugin_storage.emplace(key, 1);
    return false;
}

// 魂印侧 acting 判据（vanish_kongyuan 同款 currentState 区间）。
bool k7_acting_side_is_first(const BattleContext* ctx) {
    if (!ctx) {
        return false;
    }
    const int v = static_cast<int>(ctx->currentState);
    return v >= static_cast<int>(State::BATTLE_FIRST_ACTION_START)
        && v <= static_cast<int>(State::BATTLE_FIRST_MOVER_DEATH);
}

// 每场跃迁计数（键 = 基址 + 宏 id；战斗级）。
constexpr int kLeapUsesBase = 990163900;
// 闪避窗账本（键 = 基址 + 宏；战斗级）：[start / end / pct] 三键显式记窗。
constexpr int kLeapWinBase = 990163950;
// 1673 的层/旗（键 = 基址 + owner；战斗级）。
constexpr int k1673StacksBase = 990167300;
constexpr int k1673MissBase   = 990167310;
constexpr int k1673HitBase    = 990167320;

constexpr int kLeapDodgePctArg   = 1;   // args[1]=100 闪避率
constexpr int kLeapDodgeRoundsArg = 0;  // args[0]=3 下3回合
constexpr int kLeapDrainDenomArg  = 6;  // args[6]=3 吸取 1/3

// ── 1639/1903 共用：跃迁免伤（BLOCK 阶段回调）───────────────────────────────
void leap_block_cb(BattleContext* c, int owner, int macro, int max_uses,
                   int dodge_pct, int dodge_rounds) {
    fprintf(stderr, "[dbg-leap] block cb owner=%d macro=%d guard=%d hp=%d\n", owner, macro,
            c->ws.guard_detect_damage[owner], c->getPet(owner).hp);
    DamageSnapshot& d = c->resolvedDamage;
    if (d.defenderId != owner || !d.isRed) {
        return;   // 只认打向自身的攻击伤害
    }
    // 跃迁条件：链首快照伤害 ≥ 当前体力（犀牛 612 同口径）
    const int cur_hp = c->getPet(owner).hp;
    if (c->ws.guard_detect_damage[owner] < cur_hp || cur_hp <= 0) {
        return;
    }
    const int uses_key = kLeapUsesBase + macro;
    const int used = any_int_or(c->plugin_storage[uses_key]);
    if (used >= max_uses) {
        return;   // 每场次数用尽
    }
    c->plugin_storage[uses_key] = used + 1;
    // 免疫本次：整段归零（138 同款）
    d.base = 0;
    d.afterAdd = 0;
    d.afterMul = 0;
    d.final = 0;
    d.addPct = 0;
    d.mulCoef = 0.0;
    // 解除自身异常状态
    if (g_core && g_core->dispel_active_anomalies) {
        (void)g_core->dispel_active_anomalies(c, owner);
    }
    // 下3回合闪避 100%（下3回合起，不含当回合）：窗界入账本（[start, end) + pct），
    // 由 ROUND_START soul 节点每回合**显式**写/清——不依赖 ContinuousEffect 的桶生命周期
    // （实测：R4 窗外仍 miss 的教训——soulmark 桶对过期/once 的处理与 moves 侧不等价）。
    const int win_start = c->roundCount + 1;
    c->plugin_storage[kLeapWinBase + macro] = win_start;
    c->plugin_storage[kLeapWinBase + 100 + macro] = win_start + dodge_rounds;
    c->plugin_storage[kLeapWinBase + 200 + macro] = dodge_pct;
}

// 1639/1903 的 arm（ENTER_EXIT_STAGE）：只挂 BLOCK 免伤回调（once 语义由 caller 保证）。
template <int MACRO, int MAX_USES>
EffectResult leap_arm_impl(BattleContext* ctx, const EffectArgs& args) {
    fprintf(stderr, "[dbg-leap] arm-enter macro=%d\n", MACRO);
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (!onstage_has_soul_mark(ctx, owner, MACRO) || !g_core
        || !g_core->dispel_active_anomalies) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, MACRO);
    const int dodge_pct = db_at(row, kLeapDodgePctArg, 100);
    const int dodge_rounds = db_at(row, kLeapDodgeRoundsArg, 3);
    if (dodge_pct <= 0 || dodge_rounds <= 0) {
        return EffectResult::kOk;
    }
    fprintf(stderr, "[dbg-leap] arm owner=%d onstage=%d\n", owner,
            onstage_has_soul_mark(ctx, owner, MACRO) ? 1 : 0);
    ctx->register_damage_effect(
        DamagePhase::BLOCK, owner, DamageEffectCategory::BLOCK,
        [owner, macro = MACRO, max_uses = MAX_USES, dodge_pct, dodge_rounds](
            BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner) {
                return;
            }
            leap_block_cb(c, owner, macro, max_uses, dodge_pct, dodge_rounds);
        });
    return EffectResult::kOk;
}

// 1639/1903 的 ROUND_START 节点（闪避窗显式结算 + clause B：体力阈值分支）。
template <int MACRO>
EffectResult leap_round_open(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !g_core || !g_core->drain_hp) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (!onstage_has_soul_mark(ctx, owner, MACRO)) {
        return EffectResult::kOk;
    }
    // 闪避窗显式结算：窗内写满闪避，窗外显式清（残留自净——reset 语义以实测为准）
    if (MACRO == 1639) {
        fprintf(stderr, "[dbg-open] rc=%d hp=%d win=[%d,%d) dodge_now=%.1f\n", ctx->roundCount,
                ctx->getPet(owner).hp,
                any_int_or(ctx->plugin_storage[kLeapWinBase + MACRO]),
                any_int_or(ctx->plugin_storage[kLeapWinBase + 100 + MACRO]),
                ctx->ws.dodge_rate[owner]);
    }
    {
        const int win_start = any_int_or(ctx->plugin_storage[kLeapWinBase + MACRO]);
        const int win_end =
            any_int_or(ctx->plugin_storage[kLeapWinBase + 100 + MACRO]);
        const int pct = any_int_or(ctx->plugin_storage[kLeapWinBase + 200 + MACRO]);
        if (win_start > 0 && ctx->roundCount >= win_start && ctx->roundCount < win_end) {
            ctx->ws.dodge_rate[owner] = static_cast<float>(pct) / 100.0f;
        } else {
            ctx->ws.dodge_rate[owner] = 0.0f;
        }
    }
    if (ctx->is_boss_challenge) {
        return EffectResult::kOk;   // clause B：boss 无效
    }
    const HostRow row = host_row(ctx, args, MACRO);
    const int drain_denom = db_at(row, kLeapDrainDenomArg, 3);
    if (drain_denom <= 0) {
        return EffectResult::kOk;
    }
    const ElfPet& self = ctx->getPet(owner);
    const int cur = self.hp;
    const int max_hp = resolve_pet_max_hp(self);
    const int opp = 1 - owner;
    if (cur * 2 > max_hp) {
        // 高体：当回合攻击必暴 + 随机1技 PP 归零
        ctx->ws.must_crit[owner] = true;
        ElfPet& foe = ctx->getPet(opp);
        std::vector<int> pp_slots;
        for (int i = 0; i < static_cast<int>(foe.skills.size()); ++i) {
            if (foe.skills[static_cast<std::size_t>(i)].pp > 0) {
                pp_slots.push_back(i);
            }
        }
        if (!pp_slots.empty()) {
            foe.skills[static_cast<std::size_t>(
                pp_slots[ctx->rng().next() % pp_slots.size()])].pp = 0;
        }
        return EffectResult::kOk;
    }
    // 低体：当回合全技能先制+1 + 吸取对手最大体力 1/3
    ctx->ws.preemptive_level[owner] += 1;
    (void)g_core->drain_hp(ctx, /*actor=*/owner, /*target=*/opp, drain_denom);
    return EffectResult::kOk;
}

EffectResult soulmark_1639_arm(BattleContext* ctx, const EffectArgs& args) {
    return leap_arm_impl<1639, 1>(ctx, args);
}
EffectResult soulmark_1639_round_open(BattleContext* ctx, const EffectArgs& args) {
    return leap_round_open<1639>(ctx, args);
}
EffectResult soulmark_1903_arm(BattleContext* ctx, const EffectArgs& args) {
    return leap_arm_impl<1903, 2>(ctx, args);
}
EffectResult soulmark_1903_round_open(BattleContext* ctx, const EffectArgs& args) {
    return leap_round_open<1903>(ctx, args);
}

// ── 1673 迦曼吉 ─────────────────────────────────────────────────────────────

// ROUND_START：层数++ 并写对手输出降/命中降（ws 每回合 reset → 每回合重写）。
EffectResult soulmark_1673_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (!onstage_has_soul_mark(ctx, owner, 1673)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, 1673);
    const int dmg_down = db_at(row, 0, 10);    // 对手造成伤害 -10%/层
    const int hit_down = db_at(row, 1, 10);    // 对手攻击命中 -10%/层
    const int max_stacks = db_at(row, 2, 5);   // 最高 5 层
    if (dmg_down <= 0 || hit_down <= 0 || max_stacks <= 0) {
        return EffectResult::kOk;
    }
    const int stacks_key = k1673StacksBase + owner;
    const int stacks = std::min(any_int_or(ctx->plugin_storage[stacks_key]) + 1, max_stacks);
    ctx->plugin_storage[stacks_key] = stacks;
    const int opp = 1 - owner;
    ctx->ws.damage_add_pct[opp] -= static_cast<float>(dmg_down * stacks);
    ctx->ws.hit_rate_mod_attack[opp] = 1.0f - static_cast<float>(hit_down * stacks) / 100.0f;
    return EffectResult::kOk;
}

// ROUND_END：miss → 削对手最大体力 1/3；hit → 回自身最大体力 1/3（旗清零）。
EffectResult soulmark_1673_round_end(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !g_core || !g_core->reduce_max_hp_pct || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (!onstage_has_soul_mark(ctx, owner, 1673)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, 1673);
    const int denom = db_at(row, 3, 3);        // 1/3
    if (denom <= 0) {
        return EffectResult::kOk;
    }
    const int opp = 1 - owner;
    const int miss_key = k1673MissBase + owner;
    const int hit_key = k1673HitBase + owner;
    const bool missed = any_int_or(ctx->plugin_storage[miss_key]) == 1;
    const bool hit = any_int_or(ctx->plugin_storage[hit_key]) == 1;
    ctx->plugin_storage.erase(miss_key);
    ctx->plugin_storage.erase(hit_key);
    if (missed && !hit) {
        // 削对手最大体力 1/3（1/3 → 33% 整数取整，记档）
        (void)g_core->reduce_max_hp_pct(ctx, opp, ctx->on_stage[opp], 100 / denom, 0);
    } else if (hit && !missed) {
        const int max_hp = resolve_pet_max_hp(ctx->getPet(owner));
        (void)g_core->heal_amount(ctx, owner, max_hp / denom);
    }
    return EffectResult::kOk;
}

// 旗 watcher（arm 时挂一次）：对手攻击技 miss/hit → 旗。
void j152_style_flag_watchers(BattleContext* ctx, int owner) {
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_INVALID, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.actor != 1 - owner) {
                return;
            }
            const int idx = c->roundChoice[1 - owner][1];
            const auto& skills = c->getPet(1 - owner).skills;
            if (idx < 0 || idx >= static_cast<int>(skills.size())
                || skills[static_cast<std::size_t>(idx)].type == SkillType::Attribute) {
                return;   // 只认攻击技（1320 过滤）
            }
            c->plugin_storage[k1673MissBase + owner] = 1;
            c->plugin_storage.erase(k1673HitBase + owner);
        },
        WatcherScope::TEAM);
    ctx->event_center_.register_watcher(
        EventType::EVENT_HIT, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.actor != 1 - owner) {
                return;
            }
            const int idx = c->roundChoice[1 - owner][1];
            const auto& skills = c->getPet(1 - owner).skills;
            if (idx < 0 || idx >= static_cast<int>(skills.size())
                || skills[static_cast<std::size_t>(idx)].type == SkillType::Attribute) {
                return;
            }
            c->plugin_storage[k1673HitBase + owner] = 1;
            c->plugin_storage.erase(k1673MissBase + owner);
        },
        WatcherScope::TEAM);
}

EffectResult soulmark_1673_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (watcher_wired(ctx, 1673, owner)) {
        return EffectResult::kOk;
    }
    j152_style_flag_watchers(ctx, owner);
    return EffectResult::kOk;
}

// ── 1057 补子句路由（追加进 k_thorn_reflect 的 SOUL_PROGRAM(1057) 列表）──────

EffectResult soulmark_1057_route(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (!onstage_has_soul_mark(ctx, owner, 1057)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, 1057);
    const int dodge_pct = db_at(row, 1, 70);
    if (dodge_pct <= 0) {
        return EffectResult::kOk;
    }
    const float dodge_frac = static_cast<float>(dodge_pct) / 100.0f;
    // 先手 tick：当回合闪避 +{pct}%（ws 每回合 reset，ACTION_START 写覆盖本回合）
    Effect first_tick(1057, 0, owner, 1,
                      EffectArgs(std::vector<int>{owner, dodge_pct}),
                      [](BattleContext* c, const EffectArgs& a) -> EffectResult {
                          if (!c || !a.int_args || a.int_count < 2
                              || !k7_acting_side_is_first(c)) {
                              return EffectResult::kOk;
                          }
                          c->ws.dodge_rate[a.int_args[0]] +=
                              static_cast<float>(a.int_args[1]) / 100.0f;
                          return EffectResult::kOk;
                      });
    auto ce_first = std::make_unique<ContinuousEffect>(
        first_tick, State::BATTLE_FIRST_ACTION_START, owner, /*duration=*/1,
        ctx->roundCount);
    ce_first->source_id_ = 1057;
    ctx->register_soulmark_effect(State::BATTLE_FIRST_ACTION_START, owner,
                                  std::move(ce_first));
    // 后手 tick：当回合攻击技必暴（BOSS 无效——is_boss_challenge 手动门，diwang 判例）
    Effect second_tick(1057, 0, owner, 1,
                       EffectArgs(std::vector<int>{owner}),
                       [](BattleContext* c, const EffectArgs& a) -> EffectResult {
                           if (!c || !a.int_args || a.int_count < 1) {
                               return EffectResult::kOk;
                           }
                           if (c->is_boss_challenge || k7_acting_side_is_first(c)) {
                               return EffectResult::kOk;
                           }
                           c->ws.must_crit[a.int_args[0]] = true;
                           return EffectResult::kOk;
                       });
    auto ce_second = std::make_unique<ContinuousEffect>(
        second_tick, State::BATTLE_SECOND_ACTION_START, owner, /*duration=*/1,
        ctx->roundCount);
    ce_second->source_id_ = 1057;
    ctx->register_soulmark_effect(State::BATTLE_SECOND_ACTION_START, owner,
                                  std::move(ce_second));
    return EffectResult::kOk;
}



}  // namespace

// K7 补子句路由入口（全局作用域：被 k_thorn_reflect.cpp 的 SOUL_PROGRAM(1057) 引用）。
EffectResult soulmark_1057_leap_route(BattleContext* ctx, const EffectArgs& args) {
    return soulmark_1057_route(ctx, args);
}

// ── 注册（+3 宏：1639/1903/1673；1057 追加节点在 k_thorn_reflect.cpp）──────────
// clause B（回合开始阈值分支）带「（boss无效）」尾注 → boss_invalid=true；
// 跃迁子句无尾注 → boss 对局照常。
SOUL_PROGRAM(1639, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_1639_arm,
                     /*once=*/false, /*early=*/true),
                    SoulMarkNodeRef(State::BATTLE_ROUND_START, &soulmark_1639_round_open,
                     /*once=*/false, /*early=*/false, SoulScope::STAGE, /*boss_invalid=*/true)});
SOUL_PROGRAM(1903, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_1903_arm,
                     /*once=*/false, /*early=*/true),
                    SoulMarkNodeRef(State::BATTLE_ROUND_START, &soulmark_1903_round_open,
                     /*once=*/false, /*early=*/false, SoulScope::STAGE, /*boss_invalid=*/true)});
SOUL_PROGRAM(1673, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_1673_arm,
                     /*once=*/false, /*early=*/true),
                    SoulMarkNodeRef(State::BATTLE_ROUND_START, &soulmark_1673_round_start,
                     /*once=*/false, /*early=*/false),
                    SoulMarkNodeRef(State::BATTLE_ROUND_END, &soulmark_1673_round_end,
                     /*once=*/false, /*early=*/false, SoulScope::STAGE, /*boss_invalid=*/true)});
