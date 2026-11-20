// D2 · 重生之翼 2987 第五技「正义天启歌 19103」缺失效果 863（夜跑 night-2，2026-09-28）
//
// 863 官方模板（effect_info，args_num=1）："后出手则下{0}回合令对手使用的攻击技能无效"；
// 4399 图鉴（正义天启歌）："必中；消耗自身所有神耀能量，每消耗1层此技能威力提升50；
// 无视对手伤害限制效果；后出手则下回合免疫受到的攻击"。
// 切片：moves.side_effect=[862,697,863]、side_effect_arg=[50,1]，按 args_num 顺序切：
//   862=[50]（wing_skills.cpp 已接，无魂印 866 时空转）、697=[]（内核穿透凭证，引擎已接）、
//   863=[1]（本文件）。
//
// 实现 = SKILL_EFFECT 时点判"后出手"（acting_side_is_first，919 同款判据），成立则授
// SEAL_ATTACK 回合票。"下{0}回合"走 NextRounds 语义：授 rounds+1 票 + condition 门
// 只放行 roundCount >= 授予回合+1（1055 定版范式——SEAL 票是 tick 模型，直接授 rounds
// 会在授予当回合末被 tick 清空，下回合反而没封；多授的一格被本回合吃掉，condition 把
// 本回合挡掉，含本回合在途的反击/追加技，官方"下回合"不含它们）。
// 场景回归：test/scenario/scenario_180_chongsheng_2987_863.cpp。

#include "common.h"

namespace {

// ── 863 —— 后出手则下{0}回合令对手使用的攻击技能无效 ──
EffectResult effect_863_second_mover_seal_attack(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int rounds = args.int_args[2];   // [owner, target, 1]
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || rounds <= 0
        || acting_side_is_first(ctx)) {
        return EffectResult::kOk;   // 本方先手 → 不是"后出手"，不武装
    }
    const int start = ctx->roundCount + 1;
    ctx->rule_center_.grant_seal(
        owner, /*source_slot=*/-1, /*source_effect_id=*/863, target, SealKind::SEAL_ATTACK,
        /*counts=*/0, /*rounds=*/rounds + 1, /*penetrable=*/true, EffectScope::ON_STAGE,
        [start](BattleContext* c, int, int, bool) {
            return c && c->roundCount >= start;
        },
        ctx->round_effect_valid_id[owner]);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(863, effect_863_second_mover_seal_attack);
