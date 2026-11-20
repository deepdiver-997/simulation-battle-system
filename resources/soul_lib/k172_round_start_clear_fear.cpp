// K172 回合开始消强+害怕/固伤分支族 —— effect 808（1 宏 / 2 行）
//
// 行清单（tools/soul_effects_dump.py 808，2026-10-07）：
//   808 霸王项羽 3164 args[50, 300, 1]：「回合开始时，若对手处于能力提升状态，则消除
//     对手能力提升状态并有50%概率使对手害怕，未触发害怕则附加300点固定伤害（BOSS无效）」
//     / 进化行 args[60, 350, 1]（60%/350，"霸王项羽魂印进化活动"，同构互锚）
//     → db[0]=害怕概率、db[1]=未触发补偿固伤、db[2] 恒 1（两行同值，疑次数/开关旗，
//       官方 tips 无对应子句，不消费——记档）。
//
// ⚠️ 实现形态（T4 类型化行参）：BATTLE_ROUND_START 声明节点（STAGE 作用域，每回合
//    经更新器重注册；开局 activate 即注册 → 首回合 ROUND_START 已在桶内）。分支：
//    ①门 = 对手任一能力等级槽 > 0（K165 opp_gate_drop 同构取反）；过门先**无条件**
//    clear_stat_boosts（官方"则消除…并有50%概率"——消强不掷骰）；
//    ②apply_anomaly_checked(Fear, chance=db[0], SoulMark)——概率申报版：亮节闸门管辖
//    + ROLL_FAILED 单档回执。**未触发补偿只认 ROLL_FAILED**（battle_primitives.h 档位
//    口径：免疫/抗性是别的档，不走"未触发则…"兜底）→ fixed_damage(db[1])（吃护罩，
//    K165 303 附加固伤同原语）。
//    害怕时长 random_anomaly_duration()（施控族惯例，mortarion 同款）。
//
// ⚠️（BOSS无效）尾注两行都有 → 节点 boss_invalid 置位（boss 挑战对局不注册不执行）。
//
// ⚠️ 三查记录（author k-prod-3 2026-10-07 / k-prod-2 复核同日）：①内核无 808 魂印行为
//    硬编码 ②effect_info 808 = 技能空间同号（魂印以 effect_icon tips 为权威）③soul_lib
//    并集无同 id 注册——真缺口。
// 先例：chaos_sorensen（ROUND_START 桶每回合语义）、K165（T4 + opp 门循环）、
//   anomaly_channel/j_gaia_stance（apply_anomaly_checked 通道）、mortarion（Fear 时长）。
// 场景回归：test/scenario/scenario_3060_round_start_clear_fear.cpp（号段 3060-3069）。

#include "common.h"

#include <effects/rule_center.h>
#include <primitives/battle_primitives.h>

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


constexpr int kFearId = static_cast<int>(AbnormalStatusId::Fear);   // 害怕 6

// 对手处于能力提升状态 = 任一等级槽 > 0（K165 负槽门同构取反）。
bool opp_has_boost(const BattleContext* ctx, int opp) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[opp][i] > 0) {
            return true;
        }
    }
    return false;
}

// ── ROUND_START 节点：对手有提升 → 消强 + {0}% 害怕 / 未触发 → +{1} 固伤 ──
EffectResult k172_808_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->clear_stat_boosts || !g_core->apply_anomaly_checked
        || !g_core->fixed_damage) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 808)) {
        return EffectResult::kOk;
    }
    const int opp = a.opp;
    if (!opp_has_boost(ctx, opp)) {
        return EffectResult::kOk;   // 门闭：无提升 → 整句不触发
    }
    (void)g_core->clear_stat_boosts(ctx, opp);   // 消强不掷骰（官方"则消除…并有50%概率"）
    const HostRow row = host_row(ctx, a, 808);
    const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
        ctx, opp, kFearId, random_anomaly_duration(), owner,
        /*chance_pct=*/db_at(row, 0, 50), ChanceSource::SoulMark);
    if (r == ApplyAnomalyResult::ROLL_FAILED) {
        // 未触发害怕补偿档（免疫/抗性是别的档，不给补偿——battle_primitives.h 口径）。
        (void)g_core->fixed_damage(ctx, opp, db_at(row, 1, 300));
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（808 入账，只增不改）────────
SOUL_PROGRAM(808, {SOUL_NODE(State::BATTLE_ROUND_START, &k172_808_round_start,
                   /*once=*/false, /*early=*/false, SoulScope::STAGE,
                   /*boss_invalid=*/true)});
