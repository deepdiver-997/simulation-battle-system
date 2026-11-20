// K228 受击吸强/石化/封属复合族 —— effect 1456（1 宏 / 2 行）
//
// 行清单（tools/soul_effects_dump.py 1456，2026-10-08）：
//   1456 暗岩·格莱奥丹 3867（kind[5,10,13]）：
//     row 1139 args[3,2,1,50,2,1]        「…恢复自身最大体力的1/3…先制+1（boss无效）」
//     row 1447 args[2,2,2,50,2,1]        同上 1/2 / 先制+2（暗黑战联伙伴效果）
//   官方 tips（行权威）：
//     「自身受到攻击时吸取对手能力提升状态，若对手不处于能力提升状态则50%令对手石化，
//      未触发则使对手下2回合属性技能无效, 吸取成功则恢复自身最大体力的1/3并造成等量
//      固定伤害且自身下2回合所有技能先制+1（boss无效）」
//
// ⚠️ 行参布局（本宏 db[0..1] **不是** [own,opp] 前缀——同 Pet 两行仅 db[0]/db[2] 随行变，
//    与 tips 仅有的两处行差（1/3 vs 1/2、先制+1 vs +2）一一对应；按前缀论则行差无法
//    表达。1314（db[0]=命中降 35）/117（db[0]=pct%）同证：行参布局逐宏解码，无全局约定）
//    → SoulNodeArgs.row 自 db[2] 起（bind 丢前二），db[0] 必须经
//    soulMark.args.owned_int_args 原序读（K204 形）：
//     db[0]=恢复分母 N（1/N）｜db[1]=封属回合数｜db[2]=先制+N
//     db[3]=石化概率%｜db[4]=先制窗回合数｜db[5]=?（两行恒 1，无对应子句——不消费，记档）
//
// 实现：
//   - EVENT_TAKE_DAMAGE watcher（K199 同门 arm-once，TEAM 永久）：ev.target==own &&
//     ev.actor!=own && ev.amount>0（"受到攻击"=受击且确有伤害现值口径，零伤/被挡不触发
//     ——记档；own 已倒地整条不结算，记档）。
//   - 对手处于能力提升状态（6 槽判据，chaos_sorensen 同款）→ transfer_stat_boosts 吸取；
//     搬走 >0 = 吸取成功 → heal_amount(1/db0 maxHP)，"等量"=**实际**恢复量（吃封回血后
//     的 HP 差，chaos_sorensen ④/1005 heal_drain 口径）→ deal_pink_damage(FIXED)
//     （jiyuan_ds001 ⑤ 形：数据库口径固定伤害）→ 先制窗账本 until=round+db4。
//     被免消除强化挡下（搬走=0）= 吸取未成功：tips 无对应补偿分支，不结算（记档）。
//   - 对手无提升 → apply_anomaly_checked(石化9, db3%, SoulMark)（概率申报通道，闸门管辖）；
//     ROLL_FAILED（仅此档）→ 封属窗账本 until=round+db1（k172 口径：免疫/抗性是别的档，
//     不给补偿）。
//   - "下N回合"窗（SOP §二.2/§二.4）：ws.preemptive_level 在 MOVE_RIGHT memset →
//     BATTLE_FIRST_MOVE_RIGHT 桶节点读 plugin_storage 账本（until_round，K7 窗账本口径）：
//     先制窗每回合重写 += db2（k218/yanshou 1123 形，ws 每轮 memset 所以必须重写）；
//     封属窗**单次授予** duration=db1（notify 只认 remaining_rounds>0、tick 每轮 -1
//     自灭——对账口径见 move_right 节点内注释）。
//     措辞"无效"按 1217/936/1011 取 SEAL_ATTRIBUTE（非 SEAL_ATTRIBUTE_HIT——触发技能
//     无效补偿，chaos_sorensen 同款裁定）。
// ⚠️ 三查记录（2026-10-08）：①内核无 1456 魂印硬编码；②effect_info 1456 同号模板不同文
//    （args_num=2"消耗体力百分比伤害+下只宠免疫"，错位行——SOP §六.1，不消费模板）；
//    ③soul_lib/registry 无 1456 注册（boss_invalid_effects.json 为 boss 免疫清单、
//    j_crit_gates.cpp 为子串撞号——均非实现）。
// 场景回归：test/scenario/scenario_3720_hit_drain_petrify_seal.cpp（号段 3720-3729）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 1456;
constexpr int kLedgerPrioBase = 990760000;   // +owner：先制窗 until_round（本轮不算）
constexpr int kLedgerSealBase = 990760100;   // +owner：封属窗 until_round（本轮不算）
constexpr int kLedgerSealDoneBase = 990760200;  // +owner：封属已首授旗（单次授予，tick 自灭）

// chaos_sorensen 口径：等级提升 = 六项（攻/特攻/防/特防/速/命中），体力无等级。
constexpr int kCombatStatCount = 6;

struct K228Runtime {
    bool watcher_armed = false;
};

K228Runtime& k228_runtime(BattleContext* ctx, int owner) {
    auto& storage = ctx->getPet(owner).soulmark_storage;
    if (!storage.count(kMarkId)) {
        storage[kMarkId] = K228Runtime{};
    }
    return std::any_cast<K228Runtime&>(storage[kMarkId]);
}

// 行参解码（⚠️ db[0..1] 是真参，见文件头——必须原序读，禁走 bind 后的节点行参）。
// 越界/非法位回退行1缺省值（{3,2,1,50,2,1}），与"行参缺失按行1"口径一致。
struct K228Row {
    int heal_denom = 3;
    int seal_rounds = 2;
    int prio_amount = 1;
    int petrify_pct = 50;
    int prio_rounds = 2;
};

K228Row k228_read_row(BattleContext* ctx, int owner) {
    K228Row row;
    const int slot = ctx->find_pet_with_soulmark(owner, kMarkId);
    if (slot < 0 || slot > 5) {
        return row;
    }
    const auto& db = ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    if (db.size() > 0 && db[0] > 0) {
        row.heal_denom = db[0];
    }
    if (db.size() > 1 && db[1] > 0) {
        row.seal_rounds = db[1];
    }
    if (db.size() > 2 && db[2] > 0) {
        row.prio_amount = db[2];
    }
    if (db.size() > 3 && db[3] > 0) {
        row.petrify_pct = db[3];
    }
    if (db.size() > 4 && db[4] > 0) {
        row.prio_rounds = db[4];
    }
    return row;
}

bool side_has_any_boost(BattleContext* ctx, int side) {
    for (int i = 0; i < kCombatStatCount; ++i) {
        if (ctx->ability_levels[side][i] > 0) {
            return true;
        }
    }
    return false;
}

// ── watcher：受击 → 吸取 / 石化+封属补偿 ─────────────────────────────────────

EffectResult k228_1456_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    K228Runtime& rt = k228_runtime(ctx, owner);
    if (rt.watcher_armed) {
        return EffectResult::kOk;
    }
    rt.watcher_armed = true;
    // 行参在 arm 时点按宿主行捕获（同魂印每方唯一；数值随行定、不随对局变）。
    const K228Row row = k228_read_row(ctx, owner);
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, row](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || !g_core || !g_core->transfer_stat_boosts
                || !g_core->apply_anomaly_checked || !g_core->heal_amount
                || !g_core->deal_pink_damage) {
                return;
            }
            if (ev.target != owner || ev.actor == owner || ev.amount <= 0) {
                return;   // 仅对手出手且确有伤害（K199 同门口径）
            }
            if (ev.actor < 0 || ev.actor > 1) {
                return;
            }
            if (!onstage_has_soul_mark(wctx, owner, kMarkId)) {
                return;
            }
            ElfPet& self = wctx->getPet(owner);
            if (self.hp <= 0) {
                return;   // 已倒地不结算（记档）
            }
            if (side_has_any_boost(wctx, ev.actor)) {
                // 吸取：对手正等级整体搬到自身。
                if (g_core->transfer_stat_boosts(wctx, ev.actor, owner) <= 0) {
                    return;   // 被免消除强化挡下=吸取未成功，无补偿分支（记档）
                }
                // 吸取成功 → 恢复 1/N 最大体力 + 等量固定伤害（"等量"=实际恢复量）。
                const int max_hp = resolve_pet_max_hp(self);
                const int amount = row.heal_denom > 0 ? max_hp / row.heal_denom : 0;
                if (amount > 0) {
                    const int hp_before = self.hp;
                    (void)g_core->heal_amount(wctx, owner, amount);
                    const int healed = self.hp - hp_before;
                    if (healed > 0) {
                        (void)g_core->deal_pink_damage(wctx, ev.actor, healed,
                                                       DamageKind::FIXED, owner);
                    }
                }
                // 下 N 回合所有技能先制 +N（本回合不算，FMR 桶消费）。
                wctx->plugin_storage[kLedgerPrioBase + owner] =
                    wctx->roundCount + row.prio_rounds;
            } else {
                // 对手不处于能力提升状态 → 50% 石化；未触发 → 下 N 回合封属。
                const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
                    wctx, ev.actor, static_cast<int>(AbnormalStatusId::Petrify),
                    random_anomaly_duration(), owner, row.petrify_pct,
                    ChanceSource::SoulMark);
                if (r == ApplyAnomalyResult::ROLL_FAILED) {
                    wctx->plugin_storage[kLedgerSealBase + owner] =
                        wctx->roundCount + row.seal_rounds;
                }
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── FMR 桶：窗口账本消费（先制重写 / 封属重授）────────────────────────────────

EffectResult k228_1456_move_right(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !g_core || !g_core->seal_skill) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;   // 宿主下场：窗不供给（场下的他不提供先制/封属）
    }
    // 先制窗：账本 until >= 本回合 → 重写（ws.preemptive_level 已在 MOVE_RIGHT memset）。
    const auto prio = ctx->plugin_storage.find(kLedgerPrioBase + owner);
    if (prio != ctx->plugin_storage.end()
        && any_int_or(prio->second) >= ctx->roundCount) {
        const K228Row row = k228_read_row(ctx, owner);
        if (row.prio_amount > 0) {
            ctx->ws.preemptive_level[owner] += row.prio_amount;
        }
    }
    // 封属窗：窗内每回合重授（"每回合无脑重写"窗形；grant_seal 按
    // (source, effect_id, SEAL, kind) 键刷新，不追加）。⚠️ duration=**2** 不是 1：
    // tick_rounds 每轮减 1 并删 ≤0 票（实测一轮内减多次——Reduction 双 tick，
    // duration=1/2 活不过当轮完成点）；duration=3 + 每轮重授锚 register_round：
    // 窗内恒 active，过期轮 window_expired 先判死（3-2>=rem 恒真）+ tick 兜底清除，
    // 两种 tick 频率下语义均为精确"下N回合"、无跨轮过冲（2026-10-08 实测口径）。
    // 封属窗：窗首 FMR **单次授予** duration=seal_rounds，靠 tick 自灭收口。
    // ⚠️ 实测口径（2026-10-08，勿改回"每轮重授"）：grant_seal 不写 register_round（恒 0）
    // → notify 只认 remaining_rounds>0（tick 每轮 -1、≤0 删票），window_expired 在
    // notify 侧不生效。单授 D=N：R2..R(N+1) 动作期恒活（rem=N-(k-2)>0）、R(N+1) 回合末
    // tick 减到 0 自删、R(N+2) 动作期干净——恰为"下N回合"；每轮重授会让残票越过窗界
    // （R4 仍拦截实测翻车）。窗内重复受击不重授（官方无延长文句，保守单次——记档）。
    const K228Row row = k228_read_row(ctx, owner);
    const auto seal = ctx->plugin_storage.find(kLedgerSealBase + owner);
    const auto seal_done = ctx->plugin_storage.find(kLedgerSealDoneBase + owner);
    if (seal != ctx->plugin_storage.end() && seal_done == ctx->plugin_storage.end()
        && any_int_or(seal->second) >= ctx->roundCount) {
        (void)g_core->seal_skill(ctx, /*source=*/owner, /*target=*/ctx->opponent(owner),
                                 /*effect_id=*/kMarkId, /*attribute=*/true,
                                 /*attack=*/false, /*count=*/0,
                                 /*duration_rounds=*/row.seal_rounds,
                                 /*penetrable=*/true,
                                 /*source_slot=*/-1, EffectScope::ON_STAGE,
                                 /*hit_invalid=*/false, /*chance_pct=*/100,
                                 /*consumed_when_pierced=*/false);
        ctx->plugin_storage[kLedgerSealDoneBase + owner] = 1;
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（1456 入账，只增不改）────────
// tips 尾注"（boss无效）"→ 两节点均标 boss_invalid（boss 挑战对局不注册，watcher 不臂）。
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k228_1456_arm,
                        /*once=*/false, /*early=*/true, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_FIRST_MOVE_RIGHT, &k228_1456_move_right,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true)});
