// K313 回合末回血+随机三项族 —— effect 686（1 宏 / 1 行）
//
// 行清单（tools/soul_effects_dump.py 686，2026-10-21）：
//   686 天玄界王 2976 args[200,3,1]（kind=[5,11]）：「每个战斗阶段结束时恢复200HP,
//     随机使自身3项属性+1」
//     → **db[0]=200 = 回血量**（第一个子句，tips 字面"恢复200HP"自证）；
//       **db[1]=3 = 随机项数**（第二个子句，tips 字面"3项"自证）；
//       **db[2]=1 = 属性增量**（第二个子句，tips 字面"+1"自证）。
//     ⚠️ **"每子句一参"布局的参数自证（关键锚定）**：690 args[**200,3,1**]
//       「每个战斗阶段结束时附加200点固定伤害，随机使自身3项属性+1」——**args 与本行
//       完全相同**，但两个子句的**内容不同**（本行=回血，690=固伤）：
//       → db[0] 是**第一个子句**的数值（回血量/固伤量，同位同值 200）；
//         db[1]/db[2] 是**第二个子句**（随机项数/增量）。
//         另有 1691 args[80,50,200,2,1,1]「…战斗阶段结束时恢复200点体力并造成等量
//         固定伤害，…随机吸取对手2项属性-1」——第二子句的"项数 2"在 db[3] 位。
//       → 三行互锚 → 本行三参**全部自证**，无废位。
//
// ⚠️ 实现形态（T4 类型化行参 + db_at 原序读行参）：
//   ⚠️ **必须走 db_at**（读宿主 pet.soulMark.args.owned_int_args 原序），不能走
//      a.row（类型化 SoulNodeArgs）——后者在**真实对局里是空的**（bind_soulmark_args
//      丢弃 owned_int_args 前两位，而真实对局的 owned_int_args 就是 DB args 原样，
//      通常 2~8 位）→ a.row = owned[2:] 常常为空。存量 17 个用 a.row_or 的文件全靠
//      **fallback 硬编码**兜底，测试能过是因为场景按 {0,0,...行参} 构造（多带两位
//      占位）。本单实现与场景**两边都对**（K257 起的夜跑口径）。
//
//   时点 = BATTLE_ROUND_END（"每个**战斗阶段**结束时"字面；K178 351「战阶结束」、
//     K163 261「每回合恢复1/10最大体力」同款 ROUND_END 桶）。
//   效果 = ①heal_amount(自身, db[0])（自身回血通道，K163 261 / K10 637 同款；
//     吃封回血钳 max）
//          ②随机 db[1] 个**互不相同**的槽，各 +db[2]（自身增益通道 stat_change，
//     **不查免弱**；K4 622 同款）。
//     ⚠️ **互不相同**：官方"随机3项属性"——若两次抽样落在同一槽，实际只加了 1 项，
//       与"3项"不符。故用 K267 645 同款去重取法（先抽 first，再从剩余 k-1 个里抽
//       并跳过 first），保证槽相异。
//     ⚠️ 槽位范围 = 0..5（含命中槽 5，K267 645 同款口径——官方"属性"未排除命中）。
//
// ⚠️ 三查记录（author a02-kprod 2026-10-21）：①内核 grep 686 无魂印行为硬编码；
//    ②effect_icon kind=[5,11]、tips 为权威文（dump 原文已核；effect_info 无 686
//    技能模板）；③registry soul 集合无 686、soul_lib 全库 grep 无同 id 注册——真缺口。
// 先例：k267_random_stat_up 645（**随机互异槽 stat_change(+1)，直系同款**）、
//   k_round_recover_family 637（ROUND_START heal 桶）、K178 351（ROUND_END 桶）、
//   k_round_stat 622（stat_change 自增益通道）。
// 场景回归：test/scenario/scenario_4690_round_heal_randstat_686.cpp（号段 4690-4699）。

#include "common.h"

#include <array>

namespace {

constexpr int kMarkId = 686;
constexpr int kHealFallback = 200;   // tips 字面"恢复200HP"（db[0]）
constexpr int kCountFallback = 3;    // tips 字面"3项"（db[1]）
constexpr int kDeltaFallback = 1;    // tips 字面"+1"（db[2]）

// ── 宿主行参数（db_at 原序读 owned_int_args——真实对局与场景语义一致）────────
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

// 随机 n 个互不相同的槽（0..5）——K267 645 同款去重取法。
std::array<int, BattleContext::kAbilityLevelSlotCount> random_distinct_slots(SbsRng& rng, int n) {
    std::array<int, BattleContext::kAbilityLevelSlotCount> picked{};
    const int total = BattleContext::kAbilityLevelSlotCount;
    const int count = (n < 0) ? 0 : ((n > total) ? total : n);
    bool used[BattleContext::kAbilityLevelSlotCount] = {false};
    for (int i = 0; i < count; ++i) {
        int slot = 0;
        // 在未选中的槽里等概率抽（拒绝采样；6 槽规模下期望次数极小）。
        for (int attempt = 0; attempt < 64; ++attempt) {
            slot = rng.next() % total;
            if (!used[slot]) {
                break;
            }
        }
        used[slot] = true;
        picked[i] = slot;
    }
    return picked;
}

// ROUND_END：恢复 db[0] HP + 随机 db[1] 项属性各 +db[2]。
EffectResult k313_686_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->heal_amount || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    const int heal = db_at(row, 0, kHealFallback);   // db[0] = 200
    const int count = db_at(row, 1, kCountFallback);  // db[1] = 3
    const int delta = db_at(row, 2, kDeltaFallback);  // db[2] = 1
    // ①自身回血（K163 261 / K10 637 同款通道；封回血钳 max）。
    if (heal > 0) {
        (void)g_core->heal_amount(ctx, owner, heal);
    }
    // ②随机互异槽 stat_change(+delta)（自身增益，不查免弱）。
    if (count > 0 && delta != 0) {
        const std::array<int, BattleContext::kAbilityLevelSlotCount> slots =
            random_distinct_slots(ctx->rng(), count);
        for (int i = 0; i < count; ++i) {
            (void)g_core->stat_change(ctx, owner, slots[i], delta);
        }
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（686 入账，只增不改）──────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::BATTLE_ROUND_END, &k313_686_round_end,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
