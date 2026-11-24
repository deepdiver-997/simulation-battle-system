// K496 · 1934（args_num=1）：己方场下每死亡一只，攻击伤害提升{0}%。
// 工单 〔源: docs_local/docs/05-任务清单/已收官/E组批量效果工单.md:101〕
// 数据来源 https://github.com/SeerAPI/seer-unity-assets （2026-11-23 本区只读 SQLite）。
// 当前四载体全链重切（side_effect.arg_count 优先，args_num 兜底，均零剩余）：
// 36470:1318[10,1]/1013[1,50]/1773[50]/1934[50]；
// 36553:699[]/1568[]/1934[20]/1972[100]；
// 36558:1964[]/1305[100,100]/1934[30]；36944:978[]/1305[80,80]/1934[20]。
// 三查：src/include 含 effect_meta 无1934/1981硬编码；DB overrides/programs/combo/
// selection_routes 无两id；两注册空间均无运行时同号实现，无拆分迁移/退役记录。
// soul1934有effect_icon1575（宠4351）但无运行时实现；1981无同号soul数据。
//
// 读面是当前阵容，不是累计击败事件：count_dead(OFF_FIELD) 排除在场、消逝与额外精灵，
// 复活即时减计数；无归因死亡照算，不要求 EVENT_OPPONENT_DEFEATED.actor。
// 〔源: include/primitives/battle_primitives.h:829-836〕
// 〔源: src/primitives/battle_primitives.cpp:2073-2093〕
// 工单的 SkillView 提示不适用：“攻击伤害”≠“威力”。沿K493的通用 AMP helper 加算，
// 不改威力/rewritten，不走非通用 AMP_EXTRA；本次命中节点采样，无跨回合窗口。
// 〔源: resources/moves_lib/common.h:257-267〕
// 无 plugin_storage：1019340000/1019810000 两段均不分配，无soul子块冲突或G4新增债。
// 一次性无驻留显示态，不造空描述/伪窗。
// 〔源: docs/07-工具与测试/效果显示gate规范.md:39〕
//
// 1981 本单 blocked，不注册：同文双封锁的正式通道是 ATTACK_NULLIFY 一票两用，
// 不是MISS/SEAL_ATTACK（会触发无效补偿），也不是独立两张HIT_INVALID次数票。
// 〔源: src/entities/skills.cpp:1108-1120〕〔源: src/entities/skills.cpp:1276-1286〕
// 但最小真实FSM探针已锁两处内核缺口：①②.6 未判 is_attribute，20017高速移动被剥离；
// ②ATTACK_NULLIFY不在pure-query表，轮末remaining递减、查询又按绝对窗比较，N=2在r1即失效。
// 〔源: src/entities/skills.cpp:1116-1120〕
// 〔源: include/effects/rule_center.h:295-300〕〔源: include/effects/rule_center.h:1863-1870〕
// 〔源: include/effects/rule_center.h:1943-1945〕
// kFullNull次数票不能替代：它在管线后归零，raw台账仍非零；ATTACK_NULLIFY在管线前归零。
// 〔源: src/fsm/battleFsm.cpp:1016-1028〕
// 可复跑证据 build/k496-evidence/run_probe.sh + probe_1981.cpp；不把错误行为固化为golden。
// 待内核单：补攻击类型门，统一票的时钟/起算门，再验状态门30/16/37、N=1/2先后手、
// 红粉/真伤/force/补偿差分、断回合、双方离场与显示出口；不以rounds+常数规避缺口。

#include "common.h"

namespace {

EffectResult effect_1934_offfield_dead_amp(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1934, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1934, args,
                     "1934 布局=[owner,target,每只死亡增伤%]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int per_dead = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target == 1 - owner, 1934, args,
                     "需要 owner/target 为对向双方");
    EFFECT_ARG_GUARD(per_dead >= 0
                         && per_dead <= (std::numeric_limits<int>::max() - 100) / 5,
                     1934, args, "需要非负步长且最多五只的增伤可安全表示");
    if (!executed_skill_is_attack(ctx, owner) || !g_core || !g_core->count_dead) {
        return EffectResult::kOk;
    }
    const int dead = g_core->count_dead(ctx, owner, DeadScope::OFF_FIELD);
    const int bonus = dead * per_dead;
    return effect_set_damage_amp(ctx, EffectArgs(std::vector<int>{owner, target, bonus}));
}

}  // namespace

SKILL_EFFECT(1934, effect_1934_offfield_dead_amp)
