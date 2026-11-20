#include <utils/sbs_rand.h>
// K369 对手随机降属+阶段末条件异常族 —— effect 1224（1 宏 / 2 行）
//（方块公主 3649 row935 + 贝利弗 4619 row1827；号段 7140-7149，
//  worktree ../agents/k369，2026-10-24）
//
// 行清单（tools/soul_effects_dump.py 1224，2026-10-24）：
//   row935 方块公主 3649 args[2,1,80,20,50,1] kind[3,12]：
//     「每回合令对手随机2项能力值-1；战斗阶段结束时若对手处于能力下降状态则80%
//       令对手失明，若对手不处于能力下降状态则下回合受到的攻击伤害减少50%（boss无效）」
//   row1827 贝利弗 4619 args[6,1,60,6,50,1] kind[12,10]：
//     「回合开始时令对手全属性-1；战斗阶段结束时，若对手处于能力下降状态则60%
//       令对手害怕，若对手不处于能力下降状态则下回合受到的攻击伤害减少50%（boss无效）」
//
// ⚠️ 三查记录（a77-jprod 立单 2026-10-24 / a76-jprod 动工同日复核）：
//   ①内核 grep 1224 无魂印行为硬编码；②effect_info 1224 技能空间模板异文
//   （tips 为权威）；③registry soul 无 1224、soul_lib 全库 grep 无同 id 注册——真缺口。
//   ⚠️ K4 旧记档「失明未建模」已被现状推翻：abnormal-types.h Blind=20 已建模、
//   skills.cpp ①(b) 失明命中门已消费（非必中必 miss/必中 50% 失效）——SOP §六.3
//   「记档壁垒不是终审」；场景 7140 以施加面实证。
//
// 行参解码（两行同构自证 + k893 [项数,幅度,…] 布局同源）：
//   db[0]=降属项数（2=随机2项去重 / 6=全六项；0=关闸——在位即消费纪律，测试隔离用）
//   db[1]=降属幅度（两行恒 1）
//   db[2]=异常申报概率%（80/60）、db[3]=异常码（20失明/6害怕，官方异常码表）
//   db[4]=补偿减伤%（50/50）、db[5]=1 标志位（两行恒 1，不消费——K1 纪律）
//
// 实现（板行处方）：
//   ① ROUND_START：随机去重选 db[0] 个槽（k893 893 同款 picked[6] guard 循环，
//     db[0]=6 即全六项）→ g_core->stat_drop(对手, 槽, db[1], owner)（附加禁令/
//     免弱/钳 ±6 原语内查；AT_FLOOR 槽照常计入项数）。
//   ② ROUND_END（=「战斗阶段结束时」，chaos_sorensen ④ 同款时点）：对手六槽任一
//     < 0（chaos_sorensen self_has_stat_boost 反向判据）→ apply_anomaly_checked
//     (对手, db[3], random_anomaly_duration(), owner, db[2], SoulMark)
//     （概率申报内核单掷，禁本地 roll 双掷——k_attack_prob 口径）；
//     条件假 → 回合戳补偿旗（soulmark_storage，K363/K367 同款容器）。
//   ③ 下回合 ROUND_START 读戳（== roundCount-1，哨兵 -1 首回合防撞）→
//     ws.damage_reduce_add[owner] 首个空槽 = db[4]（REDUCE_PCT 正值=减免，
//     MITIGATE 不可被挡伤失效无视——chaos_sorensen ③ 同槽）。
//     ws 每回合 reset → 窗恰为单回合。
// 注册：SOUL_PROGRAM(1224) 双节点，SOUL_PROGRAM 总数 +1（只增不改）；
// 两行 tips 尾注均带（boss无效）→ 双节点统一 boss_invalid。
// 场景回归：test/scenario/scenario_7140_rand_drop_cond_anomaly.cpp（7140-7149）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 1224;
constexpr int kStatSlotCount = 6;   // 0攻/1特攻/2防/3特防/4速/5命中（体力无等级）

// ── 跨回合补偿旗标（soulmark_storage 容器，K218Runtime/K363 同款）──────────────
struct K369Runtime {
    int comp_round = -1;   // 最近一次"条件假→补偿"判定发生的回合
};

K369Runtime& k369_runtime(BattleContext* ctx, int owner) {
    auto& storage = ctx->getPet(owner).soulmark_storage;
    if (!storage.count(kMarkId)) {
        storage[kMarkId] = K369Runtime{};
    }
    return std::any_cast<K369Runtime&>(storage[kMarkId]);
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

// 对手是否处于能力下降状态（六槽任一 < 0；chaos_sorensen self_has_stat_boost 反向）。
bool foe_has_stat_drop(const BattleContext* ctx, int opp) {
    if (!ctx || opp < 0 || opp > 1) {
        return false;
    }
    for (int i = 0; i < kStatSlotCount; ++i) {
        if (ctx->ability_levels[opp][i] < 0) {
            return true;
        }
    }
    return false;
}

// 首个空减伤槽（chaos_sorensen ③ / K344 / K367 同款）。
void write_reduce_slot(BattleContext* ctx, int owner, int reduce_pct) {
    int(&slots)[4] = ctx->ws.damage_reduce_add[owner];
    for (int i = 0; i < 4; ++i) {
        if (slots[i] == 0) {
            slots[i] = reduce_pct;
            return;
        }
    }
}

// ── ROUND_START：①随机降属 + ③补偿窗重写 ────────────────────────────────────
EffectResult k369_1224_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const int opp = 1 - owner;
    if (ctx->getPet(owner).hp <= 0 || ctx->getPet(opp).hp <= 0) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMarkId);

    // ③ 下回合补偿窗（回合戳 == roundCount-1；哨兵 -1 首回合防撞同 K363）。
    const K369Runtime& rt = k369_runtime(ctx, owner);
    if (rt.comp_round >= 0 && rt.comp_round == ctx->roundCount - 1) {
        const int reduce = db_at(row, 4, 0);
        if (reduce > 0) {
            write_reduce_slot(ctx, owner, reduce);
        }
    }

    // ① 随机去重 db[0] 项降属（k893 picked[6] guard 循环；db[0]=0 关闸）。
    const int drop_count = db_at(row, 0, 0);
    const int drop_amount = db_at(row, 1, 1);
    if (drop_count > 0 && drop_amount != 0 && g_core && g_core->stat_drop) {
        int picked[kStatSlotCount] = {};
        int done = 0;
        int guard = 0;
        while (done < drop_count && guard < 100) {
            ++guard;
            const int slot = sbs_rand() % kStatSlotCount;
            if (picked[slot]) {
                continue;
            }
            picked[slot] = 1;
            ++done;
            (void)g_core->stat_drop(ctx, opp, slot, drop_amount, owner);
        }
    }
    return EffectResult::kOk;
}

// ── ROUND_END：②条件异常（条件假→补偿旗）────────────────────────────────────
EffectResult k369_1224_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const int opp = 1 - owner;
    if (ctx->getPet(owner).hp <= 0) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    if (foe_has_stat_drop(ctx, opp)) {
        // 条件真：概率申报给原语（内核单掷、亮节管辖）——失败无补偿（本行文
        // 的补偿支挂在"条件假"上，不是"掷点失败"上，与 1383 未触发族不同源）。
        const int pct = db_at(row, 2, 0);
        const int anomaly = db_at(row, 3, 0);
        if (pct > 0 && anomaly > 0 && g_core && g_core->apply_anomaly_checked) {
            (void)g_core->apply_anomaly_checked(ctx, opp, anomaly, random_anomaly_duration(),
                                                owner, pct, ChanceSource::SoulMark);
        }
    } else {
        // 条件假：下回合受到的攻击伤害减少 db[4]%（回合戳，下轮 ROUND_START 消费）。
        k369_runtime(ctx, owner).comp_round = ctx->roundCount;
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（1224 入账，只增不改）──────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::BATTLE_ROUND_START, &k369_1224_round_start,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_ROUND_END, &k369_1224_round_end,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true)});
