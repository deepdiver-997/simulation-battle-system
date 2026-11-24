// K491 · 2204（args_num=2）：威力减少{0}%，对手异常时改为提升{1}%。
// 工单 〔源: docs_local/docs/05-任务清单/已收官/E组批量效果工单.md:96〕
// 数据来源 https://github.com/SeerAPI/seer-unity-assets （本区 SQLite 只读现查 2026-11-23）。
// 四载体全链切片（side_effect.arg_count 优先、effect_info.args_num 兜底，均零剩余）：
// 37758:2143[]/1711[100,5]/2204[25,50]；
// 38084:1395[]/881[]/1054[19]/2204[50,50]/2341[]（2341 无 side_effect 行，兜底0）；
// 38257:2204[50,50]/1281[]/2117[2,22]；38396:547[10,3]/2204[50,50]/672[]。
// 三查：src/include 无两 id 行为硬编码，effect_meta 无条目；DB overrides/programs/
// selection_routes 无两 id；moves/soul 均无两 id 运行时注册，无拆分迁移/故意退役记录。
// soul 数据有同号（effect_icon.effect_id=2204/2102），没有运行时实现，不占键段。
//
// 复用 K489 SkillView 的赋值协议，不复制其跨回合窗口：2204 是本次命中威力修正，
// 默认 SKILL_EFFECT 时点已经物化威力；638 条件百分比威力亦在这里改写，随后由
// ATTACK_DAMAGE 收尾重算。不能把此单发效果注册成后续技能的驻留双桶窗。
// 〔源: resources/moves_lib/k489_power_double_window.cpp:43〕
// 〔源: resources/moves_lib/j_cond_power_amp.cpp:72〕
// 无异常 p*(100-{0})/100；有异常 p*(100+{1})/100，整积后截断。
// “改为”是互斥选档，绝非先减再增、除以(100+{0})或伤害 AMP。
// 写 SkillView 自动置 rewritten；0%/0威力也须写（改过就重算，不以值是否变化为门）。
// 〔源: include/fsm/battleWorkspace.h:81〕〔源: include/fsm/battleWorkspace.h:87〕
// 对手异常判据沿637：全合法码的 has_active_anomaly（含附属类、吃异常视图遮蔽），
// 不把“异常”误缩成控制类或弱化类；本体槽不能替代查询面。
// 〔源: resources/moves_lib/k475_anomaly_cond_stat_change.cpp:183〕
// 〔源: resources/moves_lib/k475_anomaly_cond_stat_change.cpp:184〕
// 无 plugin_storage：2204 段1022040000未占用；2102段1021020000也不分配。
// 一次性无驻留显示态，不造空描述/伪窗口；G4 storage/watchers 均无新增债。
// 〔源: docs/07-工具与测试/效果显示gate规范.md:39〕
//
// 2102 本单 blocked，不注册：当前 battle_effects(44,group_type=0,efftype=1) 与
// effect_des(575,kind=2,icon=44) 都把魇昧归为弱化类；内核却只支持0..43，44被
// apply_anomaly_checked 拒绝。不能静默截短随机池；需先走 kernel-approved 扩表与机制。
// 〔源: include/abnormal-system/abnormal-types.h:13〕
// 〔源: src/primitives/battle_primitives.cpp:178〕
// 本区证据 build/k491-2102-blocked/；effect_des icon3 寄生与 battle_effects 的3=特殊
// “寄生对手”不一致，实落地寄生槽=4，后续码池按 battle_effects 现表消歧，不改共享表。

#include "common.h"
#include <numerical-calculation/calculation.h>

namespace {

EffectResult effect_2204_conditional_power(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2204, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 2204, args,
                     "2204 布局=[owner,target,减少%,提升%]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int reduction = args.int_args[2];
    const int increase = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target == 1 - owner, 2204, args,
                     "需要 owner/target 为对向双方");
    EFFECT_ARG_GUARD(reduction >= 0 && reduction <= 100 && increase >= 0
                         && increase <= std::numeric_limits<int>::max() - 100,
                     2204, args, "需要 减少%∈[0,100]、提升%非负且可安全加100");
    if (!executed_skill_is_attack(ctx, owner) || !g_core || !g_core->has_active_anomaly) {
        return EffectResult::kOk;
    }
    auto& view = ctx->ws.skill_power_view[owner];
    if (view.value < 0) {
        return EffectResult::kOk;  // -1未物化；0是合法攻击威力。
    }
    bool abnormal = false;
    for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
        if (g_core->has_active_anomaly(ctx, target, id)) {
            abnormal = true;
            break;
        }
    }
    const int delta = abnormal ? increase : -reduction;
    view = checked_pct_mul(view.value, delta, "effect_2204");
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(2204, effect_2204_conditional_power)
