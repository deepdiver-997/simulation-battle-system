// K489 · effect 480：{0}回合内自身所有攻击威力为两倍（args_num=1）。
// 权威口径 〔源: docs_local/docs/05-任务清单/已收官/E组批量效果工单.md:94〕
// 数据来源 https://github.com/SeerAPI/seer-unity-assets （本区 SQLite 只读现查 2026-11-23）。
// 当前载体全链切片（side_effect.arg_count 优先，五链均零剩余）：
// 24029:707[4,3,3]/480[2]/764[3,3]；24083:462[3,200]/480[2]；
// 24162:585[1,1,0,1,1,1]/57[3,4]/480[2]；24315:48[4]/128[1]/480[2]/198[2,2]；
// 26449:480[2]（纯链，行为场景主载体）。两表对 480 均为一参。
// 三查：src/include 无 480 行为硬编码，effect_meta 无行，moves/soul 注册均无 480；
// custom_effect_overrides/programs/selection_routes 无 480，未见拆分迁移/退役记录。
//
// 威力不是伤害 AMP，也不是 K456 式掷点。PowerBoost 单元是平直加法；这里复用
// SkillView 的效果侧赋值通道，把“当前视图”乘二（不取静态 power，不置常数）。
// 〔源: include/fsm/battleWorkspace.h:81〕
// 双攻击伤害桶位于技能效果物化/改写之后、最终伤害重算之前，故 179 的平直加威力、
// 118 的随机改值均先完成，再乘二；不会被另一技能的绝对改写覆盖。
// 〔源: src/fsm/battleFsm.cpp:2093〕〔源: src/fsm/battleFsm.cpp:2098〕
// 〔源: src/fsm/battleFsm.cpp:2099〕
// 窗口沿 65 双桶形：InRounds，先手含施放回合，后手 N>=2 顺延；同源复用刷新不叠乘。
// 〔源: resources/moves_lib/h_elem_attr_cond.cpp:105〕
// 〔源: include/fsm/battleContext.h:935〕〔源: include/fsm/battleContext.h:936〕
// ON_STAGE 桶自带到期/断回合/离场清理，args 保留 [owner,target,{0}] 供 fullstate 模板显示。
// 〔源: resources/moves_lib/common.h:202〕〔源: include/effects/continuousEffect.h:117〕
// 无 plugin_storage：机制全在有显示身份的 480 桶内，无审计键段/G4 storage 新债。
// 同单 1935 另行 blocked：次数反弹需内核接口补足，本文件不注册它。

#include "common.h"
#include <numerical-calculation/calculation.h>

namespace {

EffectResult power_double_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1 || !executed_skill_is_attack(ctx, owner)) {
        return EffectResult::kOk;
    }
    auto& view = ctx->ws.skill_power_view[owner];
    if (view.value < 0) {
        return EffectResult::kOk;  // -1 尚未物化；攻击威力 0 合法，仍按变威力契约写入。
    }
    view = checked_pct_mul(view.value, 100, "effect_480");  // ×2，并置 rewritten。
    return EffectResult::kOk;
}

EffectResult effect_480_power_double_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 480, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 480, args,
                     "480 布局=[owner,target,回合]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target == 1 - owner, 480, args,
                     "需要 owner/target 为对向双方");
    EFFECT_ARG_GUARD(rounds > 0, 480, args, "需要 回合 > 0");
    register_round_effect(ctx, State::BATTLE_FIRST_ATTACK_DAMAGE, owner, 480, rounds,
                          {owner, target, rounds}, &power_double_tick);
    register_round_effect(ctx, State::BATTLE_SECOND_ATTACK_DAMAGE, owner, 480, rounds,
                          {owner, target, rounds}, &power_double_tick);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(480, effect_480_power_double_window)
