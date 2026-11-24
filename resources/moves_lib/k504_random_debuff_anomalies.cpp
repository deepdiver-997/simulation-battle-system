// K504 · 2061（args_num=1）：「随机附加{0}种弱化类异常状态」。
// 数据来源 https://github.com/SeerAPI/seer-unity-assets （2026-11-24 独立只读库）。
// K493 历史阻塞是44未入编，不是15码池口径；本基点已支持魇昧的无特效状态。
// 〔源: docs_local/docs/05-任务清单/已收官/E组批量效果工单.md:98〕
// 〔源: include/abnormal-system/abnormal-types.h:13-15〕
// 五载体实切：28589[2]、29125[3]、36923[2]、36968[2]、38658[2]。
// 按既有分类构池={1,2,4,5,10,11,13,16,20,28,29,30,31,32,36,44}，
// 不把3寄生对手误作4寄生，不剔除限制类32/44；场景精确核原语收到的基础池。
// 〔源: include/abnormal-system/abnormal-types.h:156-173〕
// 默认命中趟SKILL_EFFECT一次性，属性/攻击载体均适用；单次调用既有随机附加原语，
// 由其完成池改写、整子句100%申报、不重复挑选 min(种数,有效池大小)、逐只现代施加。
// 免疫/抗性/反弹/同种不新增均不补抽，次数是尝试种数而非保证新落地种数。
// 〔源: src/primitives/battle_primitives.cpp:507-508〕
// 〔源: src/primitives/battle_primitives.cpp:528-539〕
// 时长不来自{0}：原语以-1走默认时长，本基点恒2回合；同种只延长不缩短。
// 〔源: include/primitives/battle_primitives.h:67-69〕
// 〔源: src/primitives/battle_primitives.cpp:389-404〕
// 无补偿子句，不消费三段账、不新增storage/窗/票；2061一次性身份不伪造驻留窗。
// 〔源: docs/07-工具与测试/效果显示gate规范.md:39〕
// 2102及1935不在本插件注册范围。

#include "common.h"
#include <primitives/battle_primitives.h>

namespace {
EffectResult effect_2061_random_debuff_anomalies(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2061, args, "需要ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 2061, args,
                     "2061布局=[owner,target,种数]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int count = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target == 1 - owner, 2061, args,
                     "需要对向双方");
    EFFECT_ARG_GUARD(count >= 0, 2061, args, "需要非负种数");
    if (count == 0 || !g_core || !g_core->attach_random_anomalies) {
        return EffectResult::kOk;
    }
    std::vector<int> pool;
    for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
        if (abnormal_status_kind(id) == AbnormalStatusKind::Debuff) {
            pool.push_back(id);
        }
    }
    (void)g_core->attach_random_anomalies(ctx, target, pool, count, /*probability_pct=*/100,
                                          owner, ChanceSource::Skill);
    return EffectResult::kOk;
}
}  // namespace

SKILL_EFFECT(2061, effect_2061_random_debuff_anomalies)
