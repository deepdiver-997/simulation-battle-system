// K180 免疫低伤+反弹族 —— effect 608（1 宏 2 行）
//
// 行清单（tools/soul_effects_dump.py 608，2026-10-07）：
//   608 斯蒂丽亚 2851 两行 args[阈值, 分母, boss旗]：
//     「免疫低于300的攻击伤害；反弹1/{1}受到的伤害给对手(boss无效)」
//     [300,3,1] / [300,2,1]（伙伴效果行分母 3→2，come=伙伴效果）
//
// 落法（K15 133 直系换门：isCrit 门 → final<阈值 门，k_reflect_tail_family.cpp）：
//   enter(early) 节点每 epoch 挂一次 DamagePhase::BLOCK 回调：
//     d.final < db[0] → 归零（免疫，133 同位全字段清零）且**不反弹**；
//     d.final ≥ db[0] → 不免，按受到伤害 d.final / db[1] 对攻方 FIXED 反弹。
//   ⚠️ 口径记档：官方文本两个子句分号独立——"受到的伤害"=实际受到；
//   低伤被免（final=0）不触发反弹（区别于 133"免疫**并反弹**"同子句绑定、
//   按免疫前快照全额反弹的口径）。歧义待官方实证，动工口径取字面。
//   boss无效旗 db[2]：PvP 引擎按 S1 §七忽略（766 头注同款）。
//
// 三查记录（2026-10-07）：①内核 grep 608 无魂印行为硬编码 ②effect_info 608 =
// 技能空间同号模板（"若对手体力低于1/2，该技能先制额外+1"），魂印以 effect_icon
// tips 为权威 ③soul_lib/registry（soul=119 基线）无注册——真缺口。4399✓2851。
// 场景回归：test/scenario/scenario_3130_k180_effect608.cpp（号段 3130-3139）。

#include "common.h"

namespace {

// ── 宿主行参数（方案 B：db_at 读 owned_int_args **原序**，与真实对局一致）─────
// ⚠️ 历史沿革：本文件原用 a.row_or(i, …) 读 SoulNodeArgs.row，但 bind_soulmark_args
//    （soul_mark_manager.cpp:351）会丢弃 owned_int_args 前两位 → 真实对局里
//    a.row = DB args[2:]（多数情况为空）→ 读值错位/落 fallback。
//    2026-10-21 按方案 B 迁移为 db_at（读 owned_int_args 原序）；场景同步改为
//    按 DB args 原样构造（不再补 {0,0} 占位）。
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


// ── enter(early)：挂 BLOCK 相位回调（每 epoch 一次）─────────────────────────
EffectResult k180_608_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    // arm 幂等守卫（K1/K9 同款 int 槽 epoch 旗；先 find 后 insert）
    ElfPet& pet = ctx->getPet(owner);
    constexpr int kEpochKey = 608 * 100 + 9;
    auto it = pet.soulmark_storage.find(kEpochKey);
    if (it == pet.soulmark_storage.end()) {
        it = pet.soulmark_storage.emplace(kEpochKey, int{-1}).first;
    }
    if (int* epoch = std::any_cast<int>(&it->second)) {
        if (*epoch == ctx->pipeline_valid_id[owner]) {
            return EffectResult::kOk;
        }
        *epoch = ctx->pipeline_valid_id[owner];
    }
    const HostRow row = host_row(ctx, a, 608);
    const int threshold = db_at(row, 0, 300);
    const int denom = db_at(row, 1, 3);
    if (threshold <= 0 || denom <= 0) {
        return EffectResult::kOk;   // 无参不挂（K1 纪律）
    }
    ctx->register_damage_effect(
        DamagePhase::BLOCK, owner, DamageEffectCategory::BLOCK,
        [threshold, denom](BattleContext* c, int bucket_owner) {
            DamageSnapshot& d = c->resolvedDamage;
            if (d.defenderId != bucket_owner || !d.isRed || d.final <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(c, bucket_owner, 608)) {
                return;
            }
            if (d.final < threshold) {
                // 低伤：免疫归零（133 同位全字段清零）；"受到的伤害"=0 → 不反弹
                d.base = 0;
                d.afterAdd = 0;
                d.afterMul = 0;
                d.final = 0;
                d.addPct = 0;
                d.mulCoef = 0.0;
                return;
            }
            // 高伤：正常受伤，按实际受伤额 1/N 反弹攻方（133 反弹同位，比例版）
            if (d.attackerId >= 0 && d.attackerId <= 1 && g_core
                && g_core->deal_pink_damage) {
                const int reflected = d.final / denom;
                if (reflected > 0) {
                    g_core->deal_pink_damage(c, d.attackerId, reflected,
                                             DamageKind::FIXED, bucket_owner);
                }
            }
        });
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（只增不改）────────────────
SOUL_PROGRAM(608, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k180_608_arm,
                             /*once=*/false, /*early=*/true)});
