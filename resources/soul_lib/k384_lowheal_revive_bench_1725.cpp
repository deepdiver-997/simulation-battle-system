// K384 低血回血+承伤减半窗+重生兜底族 —— effect 1725（1 宏 / 2 行）
//（愈灵·克丽娅 row1362 args[3,2,50,50,3,50,300,1] 本体 + row1787 args[2,2,50,50,3,50,5000,1]
//  伙伴行；号段 7350-7359，worktree ../agents/k384，2026-10-24）
//
// 行清单（tools/soul_effects_dump.py 1725，2026-10-24）：
//   row1362 本体：「战斗阶段结束时，若自身体力低于回合开始时则恢复自身最大体力的1/3，
//     若体力低于最大体力的1/2则下回合受到的攻击伤害减少50%；|自身死亡后50%重生一次，
//     重生时令自身3回合内受到的攻击伤害减少50%，未重生则恢复己方所有不在场精灵300点
//     体力值」（boss无效）
//   row1787 伙伴行：同文 1/3→1/2、300点→全部体力（db6=5000 全部哨兵）。
//
// ⚠️ 三查记录（author 立单 2026-10-24 / a79-jprod 动工同日复核）：
//   ①内核 grep \b1725\b 无魂印行为硬编码；②effect_info 1725 异文（承伤反打模板
//   ——tips 权威）；③registry soul 无 1725、soul_lib 全库 grep 无同 id 注册——真缺口。
//
// 行参解码（两行同构互锚）：
//   db[0]=①回血分母（3→1/3、2→1/2）、db[1]=②阈值分母（2→1/2）、db[2]=②减伤50、
//   db[3]=③重生概率50、db[4]=③减伤窗长3、db[5]=③减伤50、db[6]=⑤不在场回复量
//   （300点/5000=全部哨兵）、db[7]=1 旗（不消费——K1 纪律）。
//
// 实现（板行处方）：
//   ① ROUND_END：hp < 回合初快照 → heal_amount(max/db[0])。快照 = plugin_storage
//     账本（990384100+own），ROUND_START 节点 + ENTER/换人 snap 节点双点刷新
//     （中途上场以入场时刻为"回合开始"，动工口径记档）。
//   ② ROUND_END：hp（**回血前**值）< max/db[1] → 写下回合减半账本
//     （990384200+own = db[2]），否则写 0——每回合末重写（2492 形），无窗后残留；
//     票（REDUCE_PCT + MITIGATE，K229 1422 同款票形）只读账本。取前值口径：伙伴行
//     heal=1/2 时"回血后"判定恒 ≥ 半血、② 成死代码（记档）。
//   ③ 死亡钩子：EVENT_TAKE_DAMAGE watcher（K11 304 同款——致死门 hp==0 + 一次性
//     计数 soulmark_storage）→ roll_percent(db[3])：
//     · 中 → revive_pet 回满（官方未给重生血量，"重生=满状态"通例，304 记档）+
//       写减半窗账本 990384300+own = roundCount + db[4]（票查 roundCount < until——
//       含重生当回合余段，动工口径记档）；
//     · 歪 → 己方所有不在场存活精灵：db[6] ≥ 5000 哨兵 → 回满，否则 hp+db[6]
//       （钳 max；直写——bench 无治疗原语通道，k_stack_reduce 遍历先例）。
//   票形：REDUCE_PCT + DamageEffectCategory::MITIGATE 只红伤（"受到的**攻击**伤害"
//   ——粉/真不沾），pct = max(②账本, ③窗活跃 ? db[5] : 0)。
// 注册：SOUL_PROGRAM(1725) 四节点（arm_once + snap + ROUND_START + ROUND_END），
// SOUL_PROGRAM 总数 +1（只增不改）；tips 尾注（boss无效）→ 全节点 boss_invalid=true
// （boss 局节点不注册 → 票/watcher 均不武装）。
// 场景回归：test/scenario/scenario_7350_lowheal_revive_bench_1725.cpp（7350-7359）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 1725;

// ── 账本键（plugin_storage，K229 1422 同款 +own 分位）────────────────────────
constexpr int kWiredKeyBase = 990384000;   // +owner：票/watcher 已武装旗标
constexpr int kSnapKeyBase = 990384100;    // +owner：回合初体力快照
constexpr int kNextPctKeyBase = 990384200; // +owner：下回合减半%（ROUND_END 重写）
constexpr int kReviveUntilKeyBase = 990384300;  // +owner：重生减半窗 until 回合（0=无）

// 重生一次性计数（per-pet，soulmark_storage 容器——304 同款）
struct K384Revive {
    int used = 0;
};

K384Revive& k384_revive(BattleContext* ctx, int owner) {
    auto& storage = ctx->getPet(owner).soulmark_storage;
    if (!storage.count(kMarkId)) {
        storage[kMarkId] = K384Revive{};
    }
    return std::any_cast<K384Revive&>(storage[kMarkId]);
}

// ── 宿主行参数（db_at 读 owned_int_args 原序——K279/K382 同款）────────────────
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

void write_int(BattleContext* ctx, int key, int v) {
    ctx->plugin_storage[key] = v;
}

int read_int(BattleContext* ctx, int key) {
    const auto it = ctx->plugin_storage.find(key);
    return it == ctx->plugin_storage.end() ? 0 : any_int_or(it->second, 0);
}

// ── arm_once：武装减半票 + 死亡 watcher（一次性）─────────────────────────────
EffectResult k384_1725_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->revive_pet) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int key = kWiredKeyBase + owner;
    if (ctx->plugin_storage.count(key)) {
        return EffectResult::kOk;   // early 双触发防重（K226/K382 同款）
    }
    ctx->plugin_storage.emplace(key, 1);

    // 减半票：只红伤（"受到的**攻击**伤害"），pct = max(② 下回合账本, ③ 重生窗)
    ctx->register_damage_effect(
        DamagePhase::REDUCE_PCT, owner, DamageEffectCategory::MITIGATE,
        [owner](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner) {
                return;
            }
            DamageSnapshot& d = c->resolvedDamage;
            if (d.defenderId != owner || !d.isRed || d.final <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            int pct = read_int(c, kNextPctKeyBase + owner);
            const int until = read_int(c, kReviveUntilKeyBase + owner);
            if (until > 0 && c->roundCount < until) {
                const int slot = c->find_pet_with_soulmark(owner, kMarkId);
                if (slot >= 0 && slot <= 5) {
                    const auto& db =
                        c->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
                    if (db.size() > 5 && db[5] > pct) {
                        pct = db[5];
                    }
                }
            }
            if (pct <= 0) {
                return;
            }
            d.final = d.final * (100 - pct) / 100;
        });

    // 死亡 watcher（K11 304 同款：致死门 + 一次性计数 + 掷点分叉）
    const HostRow row = host_row(ctx, a, kMarkId);
    const int roll_pct = db_at(row, 3, 50);
    const int win_len = db_at(row, 4, 3);
    const int bench_amt = db_at(row, 6, 300);
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, roll_pct, win_len, bench_amt](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || ev.target != owner || !g_core || !g_core->revive_pet) {
                return;
            }
            ElfPet& self = wctx->getPet(owner);
            if (self.hp > 0) {
                return;   // 致死门（304/1067 同款：hp==0 即门）
            }
            K384Revive& rt = k384_revive(wctx, owner);
            if (rt.used >= 1) {
                return;   // 重生一次
            }
            rt.used = 1;
            if (roll_soul_percent(*wctx, kMarkId, 0, owner, owner, roll_pct)) {
                // 中 → 回满重生（"重生=满状态"通例，304 记档）+ 减半窗
                g_core->revive_pet(wctx, owner, wctx->on_stage[owner],
                                   resolve_pet_max_hp(self));
                write_int(wctx, kReviveUntilKeyBase + owner,
                          wctx->roundCount + win_len);
            } else {
                // 歪 → 己方所有不在场存活精灵回 300 点 / 全部（5000 哨兵）
                for (int i = 0; i < 6; ++i) {
                    if (i == wctx->on_stage[owner]) {
                        continue;
                    }
                    ElfPet& mate = wctx->seerRobot[owner].elfPets[i];
                    if (mate.hp <= 0) {
                        continue;   // 倒地不在"恢复"面（记档：非复生）
                    }
                    const int cap = resolve_pet_max_hp(mate);
                    mate.hp = (bench_amt >= 5000) ? cap
                                                  : std::min(mate.hp + bench_amt, cap);
                }
            }
        });
    return EffectResult::kOk;
}

// ── snap：体力快照刷新（ENTER/换人 + ROUND_START 双点共用）────────────────────
EffectResult k384_1725_snap(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    if (!onstage_has_soul_mark(ctx, a.owner, kMarkId)) {
        return EffectResult::kOk;
    }
    write_int(ctx, kSnapKeyBase + a.owner, ctx->getPet(a.owner).hp);
    return EffectResult::kOk;
}

// ── ROUND_END：① 低血回血 + ② 下回合减半账本重写 ─────────────────────────────
EffectResult k384_1725_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(owner).hp <= 0) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    const int heal_denom = db_at(row, 0, -1);
    const int gate_denom = db_at(row, 1, 2);
    const int mitigate_pct = db_at(row, 2, 50);
    ElfPet& self = ctx->getPet(owner);
    const int max_hp = resolve_pet_max_hp(self);
    const int snap = read_int(ctx, kSnapKeyBase + owner);
    const int hp_at_end = self.hp;   // ② 判基：回血前（动工口径记档——伙伴行 heal=1/2
                                     // 时"回血后"判定恒 ≥ 半血，② 成死代码，故取前值）

    // ① 体力低于回合开始 → 回 max 1/db[0]
    if (heal_denom > 0 && snap > 0 && self.hp < snap) {
        (void)g_core->heal_amount(ctx, owner, max_hp / heal_denom);
    }
    // ②（回血**前**判定，见上）体力低于 max 1/db[1] → 下回合减半
    write_int(ctx, kNextPctKeyBase + owner,
              hp_at_end < max_hp / gate_denom ? mitigate_pct : 0);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（1725 入账，只增不改）──────
// tips 尾注（boss无效）→ 全节点 boss_invalid=true。
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k384_1725_arm,
                        /*once=*/false, /*early=*/true, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k384_1725_snap,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_ROUND_START, &k384_1725_snap,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_ROUND_END, &k384_1725_round_end,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true)});
