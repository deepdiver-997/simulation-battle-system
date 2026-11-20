// K360 护盾反击族 —— effect 813（1 宏 / 2 行）
//（莱茵哈特 3174；号段 7050-7059，worktree ../agents/k360，2026-10-24）
//
// 行清单（tools/soul_effects_dump.py 813，2026-10-24）：
//   row505 莱茵哈特 3174 args[300,300,1]：
//     「当回合自身能力提升状态被消除则战斗阶段结束时为自身附加300护盾；
//       自身护盾被击破时给对手附加300点固定伤害（BOSS无效）」
//   row530 莱茵哈特（大地魂印进化）args[400,400,1]：同文，300→400。
//   → db[0]=护盾值（两行 300/400 自证）；db[1]=破盾反击固伤（与 db[0] 同值同源）；
//     db[2]=1 标志位（两行恒 1，语义存疑不消费——K1 纪律）。
// ⚠️ 三查记录（author a22-kprod 2026-10-24）：①内核 grep \b813\b 无魂印行为硬编码；
//    ②effect_info 813 = 技能空间模板「将自身的能力下降状态双倍反馈给对手」（异文，
//      魂印以 effect_icon tips 为权威）；③registry soul 无 813、soul_lib 全库 grep
//      无同 id 注册——真缺口。
// 实现（板行处方）：
//   ①「当回合…被消除→战斗阶段结束时附盾」= EVENT_STAT_BOOSTS_LOST watcher
//     （target=自身、stat_loss_kind=0 消除——被吸取(kind=1)不算"被消除"）记
//     **当回合旗标**（plugin_storage 存 roundCount，ROUND_END 对表）；战斗阶段结束
//     = BATTLE_ROUND_END 节点（canglan 2343 phase_end / 1011 round_end 同款时点），
//     旗标为本回合 → shield_bank_.add(kSoulShieldPriority, db[0], source=813,
//     duration=0)（j_taken_to_shield / canglan 先例通道）。
//   ②「自身护盾被击破→对手 db[1] 固伤」= EVENT_SHIELD_BROKEN watcher（target=自身、
//     actor=对手且范围守卫——k319 同款含 -1 守卫）→ g_core->fixed_damage（1754
//     破盾反击先例同通道，吃护盾/事件）。⚠️ 事件**不分盾/罩**（护罩破碎同发）——
//     过滤走 batch_c_skills 1754 同款 v1 近似：本印的 813 盾还在（quantity_of>0）
//     → 破的不是它 → 跳过。boss 无效 = 节点 boss_invalid 一等公民旗（arm 不注册
//     则 watcher 全不挂，无半初始化面）。
//   回合计数：advanceRound 在 ROUND_COMPLETION 末尾 → 全回合（含 ROUND_END）
//   roundCount 稳定，watcher 记的回合与 ROUND_END 节点所见必同回合或已过期。
//   双 watcher 在 arm 内一次挂齐、共享 wired 旗标（再登场不重复注册）。
// 行参：db_at 原序读 owned_int_args（K279 口径：a.row 在真实对局为空，不可用）。
// 注册：SOUL_PROGRAM(813) 双节点（arm + ROUND_END），SOUL_PROGRAM 总数 +1（只增不改）。
// 场景回归：test/scenario/scenario_7050_shield_breakback_813.cpp（号段 7050-7059）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 813;
constexpr int kShieldSource = 813;            // 破盾过滤键（quantity_of）
constexpr int kWiredKeyBase = 990813000;      // +owner：watcher 已挂旗标（防再登场双挂）
constexpr int kClearedRoundBase = 990813100;  // +owner：当回合被消除旗标（值=roundCount）
constexpr int kShieldFallback = 300;          // db[0] 兜底（tips 字面）
constexpr int kBreakFallback = 300;           // db[1] 兜底（tips 字面）

// ── 宿主行参数（db_at 原序读 owned_int_args——K279 同款）────────────────────
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

// ── 登场 arm：一次挂齐双 watcher（①被消除旗标 + ②破盾反击）────────────────
EffectResult k360_813_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int wired = kWiredKeyBase + owner;
    if (ctx->plugin_storage.count(wired)) {
        return EffectResult::kOk;   // 已挂过（再登场不重复注册）
    }
    ctx->plugin_storage.emplace(wired, 1);

    // ①「当回合自身能力提升状态被消除」旗标：EVENT_STAT_BOOSTS_LOST（消除成功
    //   路径 emit，target=失去方、stat_loss_kind=0 消除 / 1 被吸取——只认消除）。
    ctx->event_center_.register_watcher(
        EventType::EVENT_STAT_BOOSTS_LOST, owner, ctx->roundCount,
        /*duration_rounds=*/0, /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != owner || ev.amount <= 0) {
                return;
            }
            if (ev.stat_loss_kind != 0) {
                return;   // 被吸取不算"被消除"（官方分句用词）
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            c->plugin_storage[kClearedRoundBase + owner] = c->roundCount;
        },
        WatcherScope::TEAM);

    // ②「自身护盾被击破→对手 db[1] 固伤」：EVENT_SHIELD_BROKEN。
    ctx->event_center_.register_watcher(
        EventType::EVENT_SHIELD_BROKEN, owner, ctx->roundCount,
        /*duration_rounds=*/0, /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c || !g_core || !g_core->fixed_damage) {
                return;
            }
            // ⚠️ ev.actor 可能是 -1（consume_shield "视为被击破"路径）——范围守卫
            //   （k319 同款）：只认对手打破的盾。
            if (ev.target != owner || ev.actor < 0 || ev.actor > 1 ||
                ev.actor == owner) {
                return;
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            // 事件不分盾/罩（1754 同款 v1 近似）：本印 813 盾还在 → 破的不是它。
            if (c->getPet(owner).shield_bank_.quantity_of(kShieldSource) > 0) {
                return;
            }
            int slot = c->find_pet_with_soulmark(owner, kMarkId);
            if (slot < 0 || slot > 5) {
                return;
            }
            const auto& db =
                c->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
            int dmg = kBreakFallback;
            if (db.size() > 1 && db[1] > 0) {
                dmg = db[1];
            }
            const ElfPet& foe = c->getPet(ev.actor);
            if (foe.hp <= 0 || dmg <= 0) {
                return;
            }
            (void)g_core->fixed_damage(c, ev.actor, dmg);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 战斗阶段结束：当回合旗标命中 → 附加 db[0] 护盾（持久，duration=0）───────
EffectResult k360_813_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const auto it = ctx->plugin_storage.find(kClearedRoundBase + owner);
    if (it == ctx->plugin_storage.end() ||
        any_int_or(it->second, -1) != ctx->roundCount) {
        return EffectResult::kOk;   // 本回合未被消除（或旗标过期）
    }
    ctx->plugin_storage[kClearedRoundBase + owner] = -1;   // 一次性消费
    const HostRow row = host_row(ctx, a, kMarkId);
    const int shield = db_at(row, 0, kShieldFallback);
    if (shield <= 0) {
        return EffectResult::kOk;
    }
    ctx->getPet(owner).shield_bank_.add(ShieldBank::kSoulShieldPriority, shield,
                                        kShieldSource, /*duration=*/0);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（813 入账，只增不改）──────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k360_813_arm,
                        /*once=*/false, /*early=*/true, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_ROUND_END, &k360_813_round_end,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true)});
