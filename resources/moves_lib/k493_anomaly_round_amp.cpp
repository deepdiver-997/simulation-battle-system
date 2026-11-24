// K493 · 2137（args_num=2）：自身每有1回合异常状态造成伤害提升{0}%，最高{1}%。
// 工单 〔源: docs_local/docs/05-任务清单/已收官/E组批量效果工单.md:98〕
// 数据来源 https://github.com/SeerAPI/seer-unity-assets （2026-11-23 本区 SQLite 只读现查）。
// 四载体全链切片（side_effect.arg_count 优先、effect_info.args_num 兜底，均无剩余）：
// 37226:195[]/1187[100,28]/2137[30,150]；
// 37764:1114[2]/2174[70]/2137[10,70]/2232[1]；
// 37997:2137[30,90]/2181[]/3[]；38195:881[]/623[1]/2137[25,150]。
//
// 读面定案（官方无实测，按当前模板及载体上下文建模，不冒充实测）：
// “每有1回合”= 本次 SKILL_EFFECT 执行时，自身所有有效异常的剩余回合数合计。
// 不是“曾经中过几回合”的历史计数（无累计/本场战斗字样），也不是当前有无异常的
// 布尔值或异常种数（2073 明写“一种”，本族明写“1回合”）。多异常按回合数相加，
// 非取最大值。effect_des 无2137行/无同句独立定义；四载体均是攻击技能。
// 37997 数据链先2137后2181解除自身异常：在此节点取快照，不拖到伤害相位重读。
// 2181本基点未注册，不能宣称真实链已清异常；场景用后继被动桩调用正式解除原语
// 锁采样边界（未来2181实现后该幂等桩仍成立），生产2137不依赖/不扩做2181。
// 载体关联魂印数据：4716魂印2336会将自身麻痹回合+1/转为超频；4850魂印2460按
// 自身流血回合逐档增伤并延长流血，支持“当前剩余量”而非历史经历（本单不实现魂印）。
// 当前库证据与可复跑查询：build/k493-evidence/query_evidence.py、db-evidence.json。
// 查询通道沿2165：has_active_anomaly（尊重“视为不处于异常”）+ end_round-roundCount，
// 逐码计入正剩余量；不裸读异常槽绕过虚拟视图，不排除附属类异常。
// 〔源: resources/moves_lib/k477_anomaly_transfer_and_shield_purge.cpp:192-199〕
// 〔源: include/plugin/core_api.h:218-225〕
//
// 通道甄别：封顶的是“本效果增加的百分比”，不是攻击伤害绝对值，也不是威力。
// 模板无“额外”，用通用增伤 AMP 加算 helper，不用 AMP_EXTRA、REDUCE 或 CAP。
// 派发先例798/1834是承伤绝对值CAP，仅可参考封顶边界，不能照搬其管线阶段。
// 〔源: include/fsm/battleWorkspace.h:295-300〕
// 〔源: resources/moves_lib/common.h:257-267〕
// 〔源: resources/moves_lib/k466_cap_reflect_and_purge_amp.cpp:288〕
// 0步长/0封顶为合法零增伤档；每项先用64位乘积、逐项饱和，等价于非负总和封顶，
// 避免大回合数/大行参先在int溢出。无随机消费，无跨回合计数/窗口/票/watcher。
// 无plugin_storage：2137段1021370000、2061段1020610000均不占用；soul2061有数据
// （effect_icon1674，宠4462）但无运行时实现，soul2137亦无注册，不需拆键子块。
// 一次性无驻留显示态，不造空描述/伪窗、不新增G4 storage债务。
// 〔源: docs/07-工具与测试/效果显示gate规范.md:39〕
// 三查：src/include（含effect_meta）无同号硬编码；custom overrides/programs/
// selection_routes/combo_rules均无两id；moves/soul无两id注册，无拆分迁移/退役记录。
// 本单不改39行选择期路由或任何DB表。
//
// 2061 blocked，不注册：当前5载体28589[2]/29125[3]/36923[2]/36968[2]/38658[2]。
// 独立查battle_effects(group_type=0,efftype=1)得16码
// {1,2,4,5,10,11,13,16,20,28,29,30,31,32,36,44}；effect_des575也认定44魇昧为弱化。
// effect_des寄生icon3与battle_effects的3=特殊“寄生对手”存在歧义，落地槽为4，
// 不将3混入弱化池，也不把44静默删掉。内核上限43，44施入返回INVALID_PARAM。
// 〔源: include/abnormal-system/abnormal-types.h:13〕
// 〔源: src/primitives/battle_primitives.cpp:178-179〕
// 沿用K491 Patch A扩表/机制提案挂账，不重复起草；2061特有面是无回合窗、无概率参、
// 单次随机2/3种，须待完整码池可施入后再验选码去重/随机复现/申报制与镜像。
// 〔源: resources/moves_lib/k491_conditional_power.cpp:29-35〕

#include "common.h"

#include <cstdint>

namespace {

EffectResult effect_2137_anomaly_round_amp(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2137, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 2137, args,
                     "2137 布局=[owner,target,每回合增伤%,封顶%]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int per_round = args.int_args[2];
    const int cap = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target == 1 - owner, 2137, args,
                     "需要 owner/target 为对向双方");
    EFFECT_ARG_GUARD(per_round >= 0 && cap >= 0
                         && cap <= std::numeric_limits<int>::max() - 100,
                     2137, args, "需要非负步长/封顶，封顶可安全加100");
    if (per_round == 0 || cap == 0 || !g_core || !g_core->has_active_anomaly
        || !g_core->get_abnormal_status_end_round) {
        return EffectResult::kOk;
    }

    int bonus = 0;
    for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
        if (!g_core->has_active_anomaly(ctx, owner, id)) {
            continue;
        }
        const std::int64_t remaining = std::max<std::int64_t>(
            0, static_cast<std::int64_t>(g_core->get_abnormal_status_end_round(ctx, owner, id))
                   - ctx->roundCount);
        bonus = static_cast<int>(std::min<std::int64_t>(
            cap, bonus + remaining * per_round));
    }
    return effect_set_damage_amp(ctx, EffectArgs(std::vector<int>{owner, target, bonus}));
}

}  // namespace

SKILL_EFFECT(2137, effect_2137_anomaly_round_amp)
