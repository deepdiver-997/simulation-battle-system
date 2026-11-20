// 重生之翼 2987 技能组 —— 神耀能量加/耗层族（effect_info 860/862/865/866）
// （2026-09-23，锚点窗口式魂免族配套；机制口径见
//   docs_local/docs/06-交接/重生之翼与王之哈莫-魂免窗口与下场保留-调研.md）
//
//   860 获得{0}层神耀能量          —— 黎羽幻生 24640
//   862 消耗自身所有神耀能量，每消耗1层此技能威力提升{0}
//                                  —— 正义天启歌 19103（args 50）
//      ⚠️ ≥6 层时第五技能（槽位 4）威力提升**不消耗**层数（魂印第 6 层子句）
//   865 击败对手则获得{0}层神耀能量 —— 无上天命剑 19102
//      挂 BATTLE_AFTER_DEFEATING_OPPONENT 时点（2397"击败对手时…"同款注册面，
//      该状态只在发生过击杀的回合进入）；归因读 pet 槽 kWingKillRoundKey
//      （魂印侧 EVENT_DEATH watcher 写，跨 dylib int 发布槽）。
//   866 若打出致命一击则获得{0}层, 否则获得{1}层 —— 银雾之翼 19100（args 2,1）
//      ⚠️ 与魂印程序同 id：魂印走 SoulMarkManager 程序表、技能走 EffectFactory
//      技能表，两张注册表互不相干，无冲突。
//
// 层变动一律走 hamo_wing_runtime.h 的 wing_adjust_layers（越 ≥2 线授/撤闭环票）。
// 遗留（未接，见调研文档）：863 后出手令对手下N回合攻击无效、864 本回合未击败则
// 下N回合反弹 1/N、807（无上天命剑末条）。
#include "common.h"

#include "../hamo_wing_runtime.h"

namespace {

// 本次技能的使用者是否为"重生之翼本尊"（按在场宠的魂印 id 判）。
// ⚠️ 层数存在**使用者自己**那只 pet 的 soulmark_storage 上（866 魂印持有者）。
bool wing_host(BattleContext* ctx, int owner, int* out_slot = nullptr) {
    if (!ctx || owner < 0 || owner > 1) {
        return false;
    }
    const int slot = ctx->on_stage[owner];
    if (ctx->seerRobot[owner].elfPets[slot].soulMark.id != 866) {
        return false;
    }
    if (out_slot) {
        *out_slot = slot;
    }
    return true;
}

}  // namespace

// ── 860 获得{0}层 ──
EffectResult effect_860_gain_layers(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int n = args.int_args[2];
    int slot = -1;
    if (n <= 0 || !wing_host(ctx, owner, &slot)) {
        return EffectResult::kOk;
    }
    wing_adjust_layers(ctx, owner, slot, n);
    return EffectResult::kOk;
}

// ── 862 消耗全部层，每层威力+{0}（≥6 层：第五技能不消耗）──
EffectResult effect_862_consume_layers_power(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int per_layer = args.int_args[2];
    int slot = -1;
    if (per_layer <= 0 || !wing_host(ctx, owner, &slot)) {
        return EffectResult::kOk;
    }
    ElfPet& host = ctx->seerRobot[owner].elfPets[slot];
    const int layers = wing_layers(host.soulmark_storage);
    if (layers <= 0) {
        return EffectResult::kOk;
    }
    const bool fifth_exempt = layers >= 6 && ctx->executing_skill_slot(owner) == 4;
    if (!fifth_exempt) {
        wing_consume_all_layers(ctx, owner, slot);
    }
    // 威力提升走技能威力视图（SkillView::+= 自动打变威力标记 → 官方"威力修改"语义）。
    // 豁免档同样吃提升（"威力提升不再消耗层数"——提升照常、层数保留）。
    ctx->ws.skill_power_view[owner] += per_layer * layers;
    return EffectResult::kOk;
}

// ── 865 击败对手则获得{0}层（注册在 BATTLE_AFTER_DEFEATING_OPPONENT，见文件尾说明）──
EffectResult effect_865_gain_on_kill(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int n = args.int_args[2];
    int slot = -1;
    if (n <= 0 || !wing_host(ctx, owner, &slot)) {
        return EffectResult::kOk;
    }
    ElfPet& host = ctx->seerRobot[owner].elfPets[slot];
    if (hamo_wing_get(host.soulmark_storage, kWingKillRoundKey, -1) != ctx->roundCount) {
        return EffectResult::kOk;   // 本回合没有由它完成的击杀
    }
    wing_adjust_layers(ctx, owner, slot, n);
    return EffectResult::kOk;
}

// ── 866 打出致命一击获得{0}层，否则{1}层 ──
EffectResult effect_866_gain_by_crit(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int on_crit = args.int_args[2];
    const int otherwise = args.int_args[3];
    int slot = -1;
    if (!wing_host(ctx, owner, &slot)) {
        return EffectResult::kOk;
    }
    const int n = ctx->crit_happened[owner] ? on_crit : otherwise;
    if (n > 0) {
        wing_adjust_layers(ctx, owner, slot, n);
    }
    return EffectResult::kOk;
}

// ── 注册 ──
SKILL_EFFECT(860, effect_860_gain_layers);
SKILL_EFFECT(862, effect_862_consume_layers_power);
SKILL_EFFECT(866, effect_866_gain_by_crit);
// 865：击败与否在"击败对手后"时点才可知 → 该时点注册（2397 同款，src/entities/skills.cpp
// effect_register_state 白名单加一行；见内核改动说明）。
SKILL_EFFECT(865, effect_865_gain_on_kill);
