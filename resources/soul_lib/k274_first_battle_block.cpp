// K274 首战挡两次伤害族 —— effect 620（1 宏 / 1 行）
//
// 行清单（tools/soul_effects_dump.py 620，2026-10-21）：
//   620 斗魂·拉塞尔 2868 args[2,1]（kind=[6]）：「首次出战时可以抵挡两次攻击伤害
//     （boss无效）」
//     → **db[0] = 抵挡次数 2**（tips 字面 "两次" ↔ 2）；db[1]=1 语义存疑不消费、记档
//       （tips 无数值子句与之对应）。
//
// ⚠️ 实现形态（T4 类型化行参 + db_at 原序读行参 + 登场节点）：
//   触发 = **OPERATION_ENTER_EXIT_STAGE 登场节点**（early 语义：战斗开始 / 切换
//     上场时执行；SOP §一.1）——官方"首次出战时"锚在登场事件，不在回合节点。
//     ⚠️ 早于本单的方案是 on_enter 钩子（plugin_interface.h:63），但纯钩子实现
//     会让 SOUL_PROGRAM 为空 → registry.soul 收不到该 id（registry 账本靠
//     SOUL_PROGRAM 注册）→ 改用**空转的登场节点**承载（early 节点 + 账本防重）。
//   效果 = **次数型 DAMAGE 免疫票**：
//     ctx->grant_immunity(owner, ImmunityType::DAMAGE, attack_damage_coverage(),
//                          /*anomaly_mask=*/0, /*duration_rounds=*/0,
//                          /*source_id=*/0, /*soul_immunity=*/false,
//                          EffectScope::ON_STAGE, /*counts=*/db[0],
//                          /*source_effect_id=*/kMarkId)
//     ——counts>0 = 次数型（"免疫下N次某威胁"）：命中后由 consume_immune 扣一次、
//     扣到 0 注销（battleContext.h:1230 注释）。走 RuleCenter DAMAGE 票 + 伤害
//     管线 BLOCK 阶段 → 与"挡伤失效"（沧岚 蚀砚之泪≥4 滴）正确交互：被压制时
//     既不触发也不扣次数（chaos_sorensen 87 记档 / 场景 031 Q4 契约）。
//     先例：canglan_pink（counts=1 抵挡下 1 次攻击）、chaos_sorensen（counts=1
//     次数型免伤）。
//   ⚠️ **"首次出战" = 整场战斗一次**（不是每只精灵一次）：用**战斗级** plugin_storage
//     旗标（跨回合保留、战斗结束/clearAllEffects 清）→ 换宠再上场不重挂。
//   ⚠️ boss 无效：tips 尾注带（boss无效）→ 节点内查 ctx->is_boss_challenge
//     → boss 挑战对局不挂票（**手动门**：因节点还要承载"首次出战"挂票动作，
//     统一用 boss_invalid=false + 手动门，避免整节点在 boss 局不注册导致
//     registry 口径与实际注册面不一致）。
//
// ⚠️ 三查记录（author a07-kprod 2026-10-21）：①内核 grep 620 无魂印行为硬编码
//    （moves_lib 的 620 是**技能空间**同号：batch_b_skills "620 {0}回合内致命一击
//    率上升{1}/16"——不同注册空间，不算已实现）；②effect_icon kind=[6]、tips 为
//    权威文（dump 原文已核，effect_info 无 620 技能模板）；③registry soul 集合
//    无 620、soul_lib 全库 grep 无同 id 注册——真缺口。
// 先例：canglan_pink（counts=1 抵挡一次攻击）、chaos_sorensen（次数型免伤票 +
//   挡伤失效交互契约）、k_guard_family（BLOCK 通道）。
// 场景回归：test/scenario/scenario_4300_first_battle_block_620.cpp（号段 4300-4309）。

#include "common.h"

namespace {

constexpr int kMarkId = 620;
constexpr int kCountsFallback = 2;   // tips 字面"两次"（db[0] 读不到时回退）
constexpr int kFirstBattleFlagBase = 990274000;   // +owner：整场"已挂首次票"旗

// 宿主行参数（db_at 原序读 owned_int_args——真实对局与场景语义一致）。
struct HostRow {
    const std::vector<int>* db = nullptr;
    int monster_id = 0;
    bool valid = false;
};

HostRow host_row(BattleContext* ctx, int owner, int mark_id) {
    HostRow row;
    if (!ctx) {
        return row;
    }
    int slot = ctx->find_pet_with_soulmark(owner, mark_id);
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

// 登场节点（early）：首次出战 → 挂 db[0] 次的 DAMAGE 免疫票。
EffectResult k274_620_enter(BattleContext* ctx, const SoulNodeArgs& a) {
    const int owner = a.owner;
    if (!ctx || owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (ctx->is_boss_challenge) {
        return EffectResult::kOk;   // (boss无效) 手动门：boss 挑战对局不挂票
    }
    const int flag_key = kFirstBattleFlagBase + owner;
    if (ctx->plugin_storage.count(flag_key)) {
        return EffectResult::kOk;   // 整场已挂过（换宠再上场不重挂）
    }
    const HostRow row = host_row(ctx, owner, kMarkId);
    if (!row.valid) {
        return EffectResult::kOk;
    }
    const int counts = db_at(row, 0, kCountsFallback);
    if (counts <= 0) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.emplace(flag_key, 1);
    // 次数型 DAMAGE 免疫票：命中后 consume_immune 扣一次、扣到 0 注销。
    ctx->grant_immunity(owner, ImmunityType::DAMAGE, attack_damage_coverage(),
                         /*anomaly_mask=*/0, /*duration_rounds=*/0,
                         /*source_id=*/0, /*soul_immunity=*/false,
                         EffectScope::ON_STAGE, /*counts=*/counts,
                         /*source_effect_id=*/kMarkId);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（620 入账，只增不改）──────
// 登场钩子挂首次票；无回合节点（on_enter 之外不做事）。
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k274_620_enter,
                        /*once=*/false, /*early=*/true, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
