// K503 · effect 1935：免疫并反弹下{0}次自身受到的异常状态（args_num=1）。
// 数据来源 https://github.com/SeerAPI/seer-unity-assets ，2026-11-24 本区 SQLite 只读核验：
// 28252 [962,1935,1804]=[1|1|1]；28316 [1935,1973,945,1974]=[1|空|空|空]；
// 28758 [191,1935,570,1154]=[4|1|1|3,240,0]；36545 [1935]=[1]。
// 无 moves 既有注册、内核 id 特判或 DB override/program/selection；soul1935 属不同空间。
//
// K489 的独立次免+回合反弹会耗尽后残留；事件 watcher 又丢原时长并重入 depth=0。
// 本单只授原生组合票，不复刻反弹、不增加状态槽：同侧同源刷新，双方各持自己的次数。
// 〔源: include/effects/rule_center.h:1110〕〔源: include/effects/rule_center.h:1114〕
// 最后一次销票前已记下反弹；回合弹控与次弹独立响应，穿透只跳免疫半边。
// 〔源: include/effects/rule_center.h:642〕〔源: include/effects/rule_center.h:669〕
// 原生回程携带原异常/时长、不再掷概率或反弹；次弹票随本宠离场清理。
// 〔源: src/primitives/battle_primitives.cpp:284〕〔源: include/effects/rule_center.h:1895〕
// 驻留显示直接沿 ruleTickets（sourceEffectId/counts），无 plugin_storage/watcher。
// 〔源: src/fsm/battleContext.cpp:1981〕〔源: src/fsm/battleContext.cpp:1985〕

#include "common.h"

namespace {

EffectResult effect_1935_counted_anomaly_reflect(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1935, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1935, args,
                     "1935 布局=[owner,opp,次数]（int_count>=3）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int counts = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp == 1 - owner, 1935, args,
                     "需要 owner/opp 为对向双方");
    EFFECT_ARG_GUARD(counts > 0, 1935, args, "需要 次数 > 0");

    ctx->rule_center_.grant_reflect_counts(owner, ctx->on_stage[owner], 1935, counts,
                                           EffectScope::ON_STAGE);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1935, effect_1935_counted_anomaly_reflect)
