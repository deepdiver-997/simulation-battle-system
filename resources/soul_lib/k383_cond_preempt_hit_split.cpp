// K383 条件先制+承伤分流族 —— effect 1864（1 宏 / 2 行）
//（阿特洛波斯 4278 row1502 args[1,30,300,50,300,3,1] 本体 + row1705 args[2,50,300,50,300,3,1]
//  光之女神伙伴效果行；号段 7340-7349，worktree ../agents/k383，2026-10-24 a24-kprod）
//
// 行清单（tools/soul_effects_dump.py 1864，2026-10-24）：
//   row1502：「回合开始时，若自身处于能力提升状态则当回合所有技能额外先制+1；对手当
//     回合使用攻击技能后则自身受到的固定伤害和百分比伤害降低30%，若对手造成的攻击伤害
//     大于300则下回合自身受到的攻击伤害减少50%，若造成的攻击伤害小于300则战斗阶段结束
//     时{吸取}对手最大{体力}的1/3（boss无效）」
//   row1705（光之女神伙伴效果）：同文 1→2、30→50，其余同构互锚。
//
// 行参解码（两行互锚；db_at 原序读 owned_int_args——K279/K363 口径）：
//   db[0]=①先制档（row1502=1 / row1705=2）、db[1]=②承伤降%（30/50）、
//   db[2]=③攻击伤害阈值（300）、db[3]=③下回合受攻减伤%（50，两行同值）、
//   db[4]=300 阈值回声（不消费——K1 纪律，记档）、db[5]=③吸取分母（3 → 1/3）、
//   db[6]=1 旗（不消费）。
//
// ⚠️ 三查记录（author a79-jprod 2026-10-24 / a24-kprod 动工复核）：
//   ①内核 grep \b1864\b 无魂印行为硬编码（1864 命中处均为精灵 id——SOP §六.2 口径）；
//   ②effect_info 1864 异文（消强+散布致命印记模板），tips 权威；
//   ③registry soul 无 1864、soul_lib 全库无 SOUL_PROGRAM(1864)——真缺口。
//
// 实现（板行处方逐子句）：
//   ① ROUND_START 桶读本体 ability_levels（K189 口径：读本体非视图）任一槽 >0 →
//     记 boost_round=rc；FMR 桶 boost_round==rc → ws.preemptive_level[owner] += db0。
//     ⚠️ pl 的 memset 在 MOVE_RIGHT（SOP §二.2）→ ROUND_START 直写会被冲，必须
//     快照@ROUND_START + 消费@FMR（K363 同位；当回合语义 ==rc 比较，无 K363 的
//     roundCount-1 哨兵相撞问题）。"所有技能"按 K218/K363 定例落回合级 pl 写（记档）。
//   ② 对手当回合使用攻击技能 → 立即旗（EVENT_SKILL_USED watcher，hamo_3809 同款：
//     ev.actor==对手 && !ev.used_attribute → stamp=rc）→ 粉伤 REDUCE_EXTRA 回调
//     （k_guard_family 同位）stamp==rc 时 p.final ×(100-db1)/100。watcher 在 execute
//     起点发（§四点五记档）→ 同一动作里后落的附加固伤也被窗口盖住。miss/被封算"使用"
//     （hamo 口径）；被控不出手不算（无事件）。
//   ③ DETECT 相位票（K381 爆次对账同位——对手攻击命中我方才进管线）：
//     d.attackerId==对手 && d.defenderId==owner && d.isRed && d.final>0 →
//     **d.base**（公式原价，见票注：防自窗削额后级联误分流）> db2 →
//     big_round=rc（下回合窗）；< db2 → small_round=rc（回合末支）；
//     ==300 两不触发（tips"大于/小于"字面）。
//     · >300 → REDUCE_PCT 回调（MITIGATE 桶，k_guard 同位）big_round>=0 &&
//       big_round==rc-1 → d.final ×(100-db3)/100。K363 式哨兵 -1 相撞防护（首回合
//       rc-1=-1）。处方原文写 T6 注册面窗界——T6 窗界只作用于状态桶节点，管线回调
//       无窗界可言 → 戳+门实现，记档（a24-kprod 2026-10-24）。
//     · <300 → ROUND_END 节点 small_round==rc → 吸取对手 max 体力/db5
//       （deal_pink_damage(FIXED)+heal_amount 等量拆原语，K302/K338 口径；不越过存量）。
//   boss 无效（tips 尾注挂整串）→ 状态节点全 boss_invalid=true；watcher/管线回调不随
//   节点 boss_invalid 走 → 手动 is_boss_challenge 门（hamo_3809 同款）。
// 注册：SOUL_PROGRAM(1864) 四状态节点（ENTER 接线 + ROUND_START ①快照 + FMR ①消费 +
//   ROUND_END ③吸取），管线三票与 watcher 在 ENTER 幂等接线——SOUL_PROGRAM 总数 +1
//   （只增不改）。
// 先例：k363 754（stamp→FMR 先制）、hamo_3809 1392（EVENT_SKILL_USED watcher+boss 门）、
//   k_guard_family 3/254（REDUCE_PCT/REDUCE_EXTRA 减伤回调）、k302 735（吸取拆原语）、
//   k381 2518（DETECT 对手命中票，本会话同日定版）。
// 场景回归：test/scenario/scenario_7340_cond_preempt_hit_split.cpp（7340-7349）。

#include "common.h"

#include <effects/pink_damage_pipeline.h>
#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 1864;
constexpr int kPreemptFallback = 1;    // db[0] 兜底（row1502 字面）
constexpr int kPinkCutFallback = 30;   // db[1] 兜底（row1502 字面）
constexpr int kThresholdFallback = 300;  // db[2] 兜底（tips 字面）
constexpr int kBigCutFallback = 50;    // db[3] 兜底（tips 字面）
constexpr int kDrainDenomFallback = 3; // db[5] 兜底（tips 字面 1/3）

// ── 跨回合/跨相位账本（soulmark_storage，K218/K363 同款容器）──────────────────
struct K383Runtime {
    int boost_round = -1;      // ① 最近一次"回合开始时有提升"的回合
    int opp_attack_round = -1; // ② 对手最近一次使用攻击技的回合
    int big_hit_round = -1;    // ③> 最近一次对手攻击伤害>阈值的回合
    int small_hit_round = -1;  // ③< 最近一次对手攻击伤害<阈值的回合
};

K383Runtime& k383_runtime(BattleContext* ctx, int owner) {
    auto& storage = ctx->getPet(owner).soulmark_storage;
    if (!storage.count(kMarkId)) {
        storage[kMarkId] = K383Runtime{};
    }
    return std::any_cast<K383Runtime&>(storage[kMarkId]);
}

// ── 宿主行参数（db_at 原序读 owned_int_args——K279/K363 同款）──────────────────
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

bool holder_onstage(BattleContext* ctx, int owner) {
    return onstage_has_soul_mark(ctx, owner, kMarkId);
}

bool boss_battle(const BattleContext* ctx) {
    return ctx && ctx->is_boss_challenge;
}

// ── 接线（幂等，ENTER 调）：②watcher + ②粉减 + ③>红减 + ③分流 DETECT 票 ──────
constexpr int kWiredKeyBase = 990383000;   // +owner

void ensure_wiring(BattleContext* ctx, int owner) {
    if (!ctx || ctx->plugin_storage.count(kWiredKeyBase + owner)) {
        return;
    }
    ctx->plugin_storage.emplace(kWiredKeyBase + owner, 1);

    // ② 触发器：对手使用技能事件（execute 起点）→ 攻击技（!used_attribute）立当回合旗。
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.actor != 1 - owner || boss_battle(c)
                || !holder_onstage(c, owner)) {
                return;
            }
            if (!ev.used_attribute) {
                k383_runtime(c, owner).opp_attack_round = c->roundCount;
            }
        },
        WatcherScope::TEAM);

    // ② 粉伤降（FIXED/PERCENT 全走粉管线）：当回合对手用过攻击技 → ×(100-db1)/100。
    {
        const HostRow row = host_row(ctx, SoulNodeArgs{owner}, kMarkId);
        const int cut = db_at(row, 1, kPinkCutFallback);
        ctx->register_default_pink_effect(
            PinkDamagePhase::REDUCE_EXTRA, owner,
            [owner, cut](BattleContext* c, int bucket_owner) {
                if (!c || bucket_owner != owner || boss_battle(c)
                    || !holder_onstage(c, owner)) {
                    return;
                }
                PinkDamageResolved& p = c->resolvedPink;
                if (p.target != bucket_owner || p.final <= 0) {
                    return;
                }
                const K383Runtime& rt = k383_runtime(c, owner);
                if (rt.opp_attack_round < 0 || rt.opp_attack_round != c->roundCount) {
                    return;   // "当回合使用攻击技能后"——旗只在当回合有效
                }
                p.final = p.final * (100 - cut) / 100;
            });
    }

    // ③> 下回合受攻减半窗：REDUCE_PCT（MITIGATE 桶），big_round==rc-1 消费。
    {
        const HostRow row = host_row(ctx, SoulNodeArgs{owner}, kMarkId);
        const int cut = db_at(row, 3, kBigCutFallback);
        ctx->register_default_damage_effect(
            DamagePhase::REDUCE_PCT, owner, DamageEffectCategory::MITIGATE,
            [owner, cut](BattleContext* c, int bucket_owner) {
                if (!c || bucket_owner != owner || boss_battle(c)
                    || !holder_onstage(c, owner)) {
                    return;
                }
                DamageSnapshot& d = c->resolvedDamage;
                if (d.defenderId != bucket_owner || !d.isRed || d.final <= 0) {
                    return;
                }
                const K383Runtime& rt = k383_runtime(c, owner);
                // ⚠️ big_round>=0：哨兵 -1 与首回合 rc-1=-1 相撞防护（K363 定例）。
                if (rt.big_hit_round < 0 || rt.big_hit_round != c->roundCount - 1) {
                    return;   // "下回合"单回合窗
                }
                d.final = d.final * (100 - cut) / 100;
            });
    }

    // ③ 分流检测票：对手攻击命中我方 → 按阈值记 big/small 回合戳。
    // ⚠️ 分流读 **d.base**（公式原价：含暴击/浮动、未叠双方增减伤）而非 d.final：
    //   DETECT 相位在 kOrder 尾部（REDUCE_PCT 消费旧戳之后）——若读 final，本印自己
    //   的">300 下回合减半窗"会把下一刀削到阈值之下 → 误入 <300 吸取支（a24-kprod
    //   实测级联：r2 刀 488→244 误触发吸取 266666）。base 全管线不改写（仅 BLOCK
    //   免疫归零——归零时 final<=0 门已拦），是干净的原价载体；阈值口径记档。
    ctx->register_default_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner || boss_battle(c)
                || !holder_onstage(c, owner)) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != 1 - owner || d.defenderId != owner || !d.isRed
                || d.final <= 0) {
                return;
            }
            const HostRow row = host_row(c, SoulNodeArgs{owner}, kMarkId);
            const int threshold = db_at(row, 2, kThresholdFallback);
            K383Runtime& rt = k383_runtime(c, owner);
            if (d.base > threshold) {
                rt.big_hit_round = c->roundCount;
            } else if (d.base < threshold) {
                rt.small_hit_round = c->roundCount;
            }
        });
}

// ── ENTER：接线（幂等）───────────────────────────────────────────────────────
EffectResult k383_enter(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    ensure_wiring(ctx, a.owner);
    return EffectResult::kOk;
}

// ── ① 快照：ROUND_START 读本体提升态（K189：读本体 ability_levels 非视图）──────
EffectResult k383_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !holder_onstage(ctx, a.owner)) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[owner][i] > 0) {
            k383_runtime(ctx, owner).boost_round = ctx->roundCount;
            return EffectResult::kOk;
        }
    }
    return EffectResult::kOk;
}

// ── ① 消费：FMR 桶（memset 后写，SOP §二.2）→ 当回合先制 +db0 ─────────────────
EffectResult k383_fmr(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !holder_onstage(ctx, a.owner)) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const K383Runtime& rt = k383_runtime(ctx, owner);
    if (rt.boost_round < 0 || rt.boost_round != ctx->roundCount) {
        return EffectResult::kOk;   // 回合开始时无提升 → 不触发
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    const int preempt = db_at(row, 0, kPreemptFallback);
    if (preempt > 0) {
        ctx->ws.preemptive_level[owner] += preempt;
    }
    return EffectResult::kOk;
}

// ── ③< 吸取：ROUND_END small_round==rc → 吸对手 max/db5，自身等量恢复 ──────────
EffectResult k383_round_end_drain(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->deal_pink_damage || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !holder_onstage(ctx, owner)) {
        return EffectResult::kOk;
    }
    const K383Runtime& rt = k383_runtime(ctx, owner);
    if (rt.small_hit_round < 0 || rt.small_hit_round != ctx->roundCount) {
        return EffectResult::kOk;   // 本回合无 <阈值 命中 → 不吸取
    }
    const int opp = 1 - owner;
    ElfPet& foe = ctx->getPet(opp);
    if (foe.hp <= 0 || ctx->getPet(owner).hp <= 0) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    const int denom = db_at(row, 5, kDrainDenomFallback);
    if (denom <= 0) {
        return EffectResult::kOk;
    }
    int dmg = resolve_pet_max_hp(foe) / denom;
    const int foe_hp = foe.hp;
    if (dmg > foe_hp) {
        dmg = foe_hp;   // 吸取不越过存量（K302/K338 口径）
    }
    if (dmg <= 0) {
        return EffectResult::kOk;
    }
    (void)g_core->deal_pink_damage(ctx, opp, dmg, DamageKind::FIXED, owner);
    (void)g_core->heal_amount(ctx, owner, dmg);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（1864 入账，只增不改）──────
// tips 尾注（boss无效）挂整串 → 全状态节点 boss_invalid=true（回调另有手动门）。
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k383_enter,
                        /*once=*/false, /*early=*/true, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_ROUND_START, &k383_round_start,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_FIRST_MOVE_RIGHT, &k383_fmr,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_ROUND_END, &k383_round_end_drain,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true)});
