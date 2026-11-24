// K502 · 1583 后手轮次固伤 / 1554 低体力自异常且次数免伤（2026-11-23）。
// 工单 〔源: docs_local/docs/05-任务清单/已收官/E组批量效果工单.md:107〕
// 数据来源 https://github.com/SeerAPI/seer-unity-assets ，本区只读库现查：
// 1583 args_num/arg_count=1/1：29079[20]、34206[15]、35837[20]、37878[20]；
// 1554 两表=3/3：28337[2,32,1]、34033[2,15,1]、37726[3,15,1]、37852[2,7,1]。
// 伙伴链无载体；34033 flat 有10项而链消耗5项，尾部重复5项不参与装配。保留原库，
// 场景锁真实loader输出，不谎称八条全链均耗尽、不自行改DB。
// 〔源: src/db/official_data_repository.cpp:170-186〕
// 三查：src/include 无同号硬编码；effect_meta 两者关键词分类Other、无覆盖；
// custom overrides/programs/selection_routes/combo_rules 均无两id；同文模板各唯一。
// moves/soul 无两id运行时注册，soul仅effect_icon同号数据（1554行1219、1583行1245）；
// db_patches/任务历史未见拆分迁移或故意退役。1554旧parser只识别“令自身”而丢前置门
// 与免伤后半，属于不完整兜底，不是完整实现；注册后由本插件接管，不修改冻结parser。
// 〔源: src/effects/effect_unit_parser.cpp:75-103〕
// 〔源: src/effects/effect_unit_parser.cpp:199-208〕
//
// 1583短句锚点：“后出手时附加当前战斗回合数”。后手用真实桶序，非裸速度比较。
// 内核roundCount从0起、ROUND_COMPLETION末才+1；“当前战斗回合数”按正在进行的
// 日历轮次=roundCount+1，非已完成轮数。沉淀29079纯链按第1/2/3轮给20/40/60；
// 不是施放次数或本宠在场轮数，换宠/跳过出手均不重置。沿2574绝对轮号及K449计数
// 的+1先例定案；wiki仅确认公式、未明说0/1基，无官方首轮实测，保留此证据边界。
// https://wiki.biligame.com/seer/技能:29079
// 〔源: resources/moves_lib/common.h:370-379〕
// 〔源: src/fsm/battleContext.cpp:90〕〔源: src/fsm/battleFsm.cpp:2503-2508〕
// 〔源: resources/moves_lib/j_gaia_stance.cpp:435,453〕
// 〔源: resources/soul_lib/k449_gaia_wudao_2377.cpp:156-161〕
// FIXED走粉伤原语，吃固定抗性/护罩/免粉并记actor；乘法先扩long long防溢出。
// 〔源: include/plugin/core_api.h:113-116〕
//
// 1554短句锚点：“低于最大体力的1/{0}”。严格hp×除数<max_hp，恰门闭，整数直乘。
// {1}不是K475能力六槽：battle_effects官方码7=疲惫、15=冰封、32=凝滞；37852的
// wiki文本“令自身疲惫”及28337凝滞例互证。现代apply_anomaly(actor=self)，无概率
// 字面故不申报100%给概率闸门；时长未明写沿通用2回合。异常与免伤以“且”并列，
// 自异常被免疫不取消授票（不是2371“触发成功”门）。0次数不授永久票。
// https://wiki.biligame.com/seer/技能:37852
// 〔源: resources/moves_lib/j_cond_priority.cpp:45-48〕
// 〔源: include/primitives/battle_primitives.h:67-69〕
// 免伤=570/K484的DAMAGE面，但本号是counts={2},rounds=0的次数票；不封技、不免粉，
// MISS/无效攻击不进红伤BLOCK消费面。票即刻生效，ON_STAGE换宠清，同源重施刷新不叠。
// source_id=0让RuleCenter按(owner,effect,type)分配，不能拿1554冒充全局句柄串镜像。
// 〔源: resources/moves_lib/effect_batch8.cpp:507-522〕
// 〔源: include/fsm/battleContext.h:1237-1255〕
// 无watcher/行为storage/审计槽：候选键1015830000/1015540000整段不占，soul虽有数据
// 无运行时实现，不需子块。驻留态只用原生异常/RuleTicket，fullstate+快照锁source1554。
// 〔源: docs/07-工具与测试/效果显示gate规范.md:191-198〕

#include "common.h"

#include <limits>

namespace {

EffectResult effect_1583_second_round_fixed(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1583, args, "需要ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1583, args,
                     "1583布局=[owner,target,每回合固伤]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int per_round = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner < 2 && target >= 0 && target < 2 && owner != target,
                     1583, args, "需要不同的双方");
    EFFECT_ARG_GUARD(per_round >= 0 && ctx->roundCount >= 0, 1583, args,
                     "每回合固伤与内核轮号须非负");
    if (acting_side_is_first(ctx) || per_round == 0
        || ctx->getPet(owner).hp <= 0 || ctx->getPet(target).hp <= 0
        || !g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const long long battle_round = static_cast<long long>(ctx->roundCount) + 1;
    const long long amount = battle_round * per_round;
    EFFECT_ARG_GUARD(amount <= std::numeric_limits<int>::max(), 1583, args,
                     "回合固伤乘积超int");
    (void)g_core->deal_pink_damage(ctx, target, static_cast<int>(amount), DamageKind::FIXED,
                                  owner);
    return EffectResult::kOk;
}

EffectResult effect_1554_lowhp_self_anomaly_and_immunity(BattleContext* ctx,
                                                        const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1554, args, "需要ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1554, args,
                     "1554布局=[owner,target,体力除数,异常码,次数]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int denominator = args.int_args[2];
    const int anomaly = args.int_args[3];
    const int counts = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner < 2 && target >= 0 && target < 2 && owner != target,
                     1554, args, "需要不同的双方");
    EFFECT_ARG_GUARD(denominator > 0 && is_valid_abnormal_status_id(anomaly) && counts >= 0,
                     1554, args, "需要正除数、合法异常码及非负次数");
    if (ctx->getPet(owner).hp <= 0
        || static_cast<long long>(ctx->getPet(owner).hp) * denominator >= max_hp_of(ctx, owner)
        || !g_core || !g_core->apply_anomaly) {
        return EffectResult::kOk;
    }
    (void)g_core->apply_anomaly(ctx, owner, anomaly, random_anomaly_duration(), owner);
    if (counts > 0) {
        ctx->grant_immunity(owner, ImmunityType::DAMAGE, coverage_all(), /*anomaly_mask=*/0,
                            /*duration_rounds=*/0, /*source_id=*/0, /*soul_immunity=*/false,
                            EffectScope::ON_STAGE, counts, /*source_effect_id=*/1554,
                            ImmunityTier::Modern);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1583, effect_1583_second_round_fixed)
SKILL_EFFECT(1554, effect_1554_lowhp_self_anomaly_and_immunity)
