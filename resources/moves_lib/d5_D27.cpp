// D27 · 涂蝶 4861 第五技「混色神·虚之彩 29242」缺失效果 2392、2175
//（2026-09-29，worktree ../agents/d-D27，场景 430-439）
//
// 29242 链 = [2392, 2175]、两条均无参切片（effect_info args_num=0；
// moves.side_effect_arg=[]）——引擎注入 int_args[0]=owner、int_args[1]=1-owner
//（build_effect_args_for_skill 固定注入 + register_branch 绑定参与者）。
// 29242 本体：属性技（category=4、power 0）。
//
// 2392 "若本场战斗己方场上分别出现过红幕、黄幕、蓝幕，则在己方场上召唤1只虚之彩"：
//   ⚠️ 跳过记档（召唤子句**双缺**）：
//   · 引擎无"召唤额外单位"原语——唯一的上场通道是背包补位（handle_ChooseAfterDeath，
//     FSM 内部状态机，插件不可达）；
//   · monsters 表无"虚之彩"实体（全表无此名）——连补位体都无数据。
//   三幕齐判定口已就绪：resources/tudie_runtime.h 的 seen_colors(storage, side)
//   位图（魂印 2468 染色路径维护；"出现过红/黄/蓝" = 位图含 1|2|3 三位），
//   将来召唤原语落地后按位图补结算即可，无需重做判定。
//
// 2175 "技能无效时，随机附加2种非限制类异常状态"：
//   - "技能无效时"族第三条（同触发器：1794 回自身血 / 2203 免自身伤；已落地的同族
//     分支 2006/2501/2086/2126/136/2418）→ 分支登记于内核 default_branch_for_effect
//     （case 2175 → SKILL_INVALID，跨文件一行——同族既定工作流，非新原语）。
//   - 对象口径：异常状态全为负面 → 只可能施加给**对手**（惩罚无效方）；
//     1794/2203 的自身受益子句（回血/免疫）不改变这一判定。
//   - 随机附加走 attach_random_anomalies（1367 混沌吞噬字面同款通道：池⊕改写票 /
//     概率门 / 不重复挑选三段账收口，免疫/抗性/弹控全继承）。
//   - 池 = 全池 ⊖ 限制类（瘫痪 19 / 凝滞 32，is_restriction_abnormal_status——
//     官方语料 idx=355"目前只有凝滞和瘫痪"）。
//
// 场景回归：test/scenario/scenario_430_tudie_4861_2175.cpp。

#include "common.h"

namespace {

// 非限制类异常池：全池 ⊖ 限制类（瘫痪 19 / 凝滞 32）。
// anomaly_pool_all() 是头内联；过滤在本文件收口，异常表扩员（kOfficialAbnormalStatusMaxId
// 增量）自动跟随全池。
std::vector<int> non_restriction_pool() {
    std::vector<int> pool = anomaly_pool_all();
    pool.erase(std::remove_if(pool.begin(), pool.end(),
                              [](int id) {
                                  return is_restriction_abnormal_status(
                                      static_cast<AbnormalStatusId>(id));
                              }),
               pool.end());
    return pool;
}

// ── 2175 —— 技能无效时，随机附加2种非限制类异常状态 ──
// 分支 = SKILL_INVALID（default_branch_for_effect 已登记）：只在技能被无效（miss /
// 被盔 / 威 / 封属 / 沉默第五技，均归 SKILL_INVALID 补偿趟）时注册执行；
// 正常命中不跑（HIT 桶无本效果——见场景 430 T3）。
EffectResult effect_2175_random_anomalies_on_invalid(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core ||
        !g_core->attach_random_anomalies) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    (void)g_core->attach_random_anomalies(ctx, target, non_restriction_pool(),
                                          /*count=*/2, /*probability_pct=*/100, owner,
                                          ChanceSource::Skill);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(2175, effect_2175_random_anomalies_on_invalid)
