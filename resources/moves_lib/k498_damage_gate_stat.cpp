// K498 · 518 伤害门自身能力变化（2026-11-23）；2084 定向 PP 原语缺口，不注册。
// 工单 〔源: docs_local/docs/05-任务清单/已收官/E组批量效果工单.md:103〕
// 数据来源 https://github.com/SeerAPI/seer-unity-assets ，本区只读库现查：
// 518 args_num/arg_count=3/3，14732=[300,0,1]、15543/16476=[250,0,1]、
// 16543=[300,2,1]、31076=[300,0,2]；2084两表=1/1，28474/29053/37678=[5]、
// 38091=[2]。九载体全链零剩余，friend_side_effect无两id。
// 三查：src/include无同号逻辑，effect_meta无覆盖（518 Other、2084 Drain）；
// custom overrides/programs/selection_routes/combo_rules均无行，parser无对应条件句；
// 基线registry 1616/417/4无两id；全worktree无moves/soul注册或键段占用。
// db_patches/tasklog/交接无拆分迁移或退役记录；K442/444/446仅将518列待做。
//
// 518短句锚点：“若伤害高于{0}，则自身{1}+{2}”。伤害=本次攻击红伤的实际扣血，
// 严格 >（不是 >=），不取公式裸伤、管线final、累计受伤、附带粉伤或对方打来的伤害。
// SKILL_EFFECT先武装；TAKE_DAMAGE过滤actor/target、NORMAL、ATTACK_DAMAGE、
// 当轮当次skill_use_seq；匹配首笔即自拆（门闭也消费），门开走stat_change自身。
// 连击在内核是一次公式×N后单笔结算，不在此另累加。
// 〔源: resources/moves_lib/k470_attack_end_heal_and_hp_drain.cpp:327-349〕
// 〔源: src/fsm/battleFsm.cpp:805-811〕
// {1}官方槽0攻1防2特攻3特防4速5命中 → 引擎{0,2,1,3,4,5}；{2}原样透传。
// 16543文案“自身特攻+1”交叉证 https://wiki.biligame.com/seer/技能:16543 。
// 〔源: resources/moves_lib/k475_anomaly_cond_stat_change.cpp:173-175〕
// stat_change原生处理禁提升/越界拒写（+5再+2不钳到6）、同步视图与变化事件。
// 〔源: src/primitives/battle_primitives.cpp:1462-1485〕
//
// 生命周期：once=false，过滤后手工自拆，避免同链粉伤先到而耗掉once；当次没有
// 红伤（全挡/全盾）也由AFTER_ACTION一次性桥撤监听。duration=1+ON_STAGE+显式
// valid_id兜底，round/seq防跨行动串用；换宠由epoch清理，强化等级由内核离场清零。
// 〔源: include/effects/event_center.h:498-513〕
// 无plugin_storage/on_stage_storage/soulmark_storage键；候选1005180000/1020840000
// 均未使用。soul同号数据不等于运行时实现，基线无同号soul，无需子块拆分。
// 无跨行动驻留窗，只有在途监听/一次性清理桥与等级变化事件；不填空描述。
// 〔源: docs/07-工具与测试/效果显示gate规范.md:34-45〕
//
// 2084 blocked提案：pp_reduce只支持全槽减N，pp_zero_slot只支持指定槽全清；无
// “指定槽减N”CoreApi。全扣再补会为未选槽发假事件；先清再补会把减少量报成全量，
// 且错误套用琼华之庇清零保留。禁止直写PP/手工补事件冒充原语，不注册半实现。
// 〔源: include/plugin/core_api.h:122-140〕
// 〔源: src/primitives/battle_primitives.cpp:1944-1955〕
// 最小内核后续：pp_reduce_slot(ctx,target,slot,amount,actor)，返回实际扣量、单槽
// 单次EVENT_PP_REDUCED（钳零、无限槽不动、调整不套清零免疫），再按实扣恢复收益。
// 选择面另需定案：当前39条DB选择路由不含2084/2083，普通SKILL_EFFECT不是选择期；
// roundChoice能读点选槽，但无选择事件，不能以EVENT_SKILL_USED代替“选择”（被控
// 仍已选择）。must_hit凭证合并静态/条件/强制执行；对方未执行时不能读旧凭证，
// 需明确选择快照及条件必中时序，确定是施放期条件还是选择期固有效果后再议路由。
// 〔源: src/entities/skills.cpp:340-350〕〔源: src/entities/skills.cpp:595-599〕
// 本单不改DB/路由/内核。续作验收：双方×先后手、选择但被控/NONE/换宠、静态/条件
// 必中与accuracy100差分、非零所选槽/第五槽、实扣与收益上限、单槽PP事件、离场。

#include "common.h"

#include <memory>

namespace {
constexpr int kOfficialToEngine[6] = {0, 2, 1, 3, 4, 5};

EffectResult remove_damage_watch(BattleContext* ctx, const EffectArgs& args) {
    if (ctx && args.int_args && args.int_count >= 2) {
        ctx->event_center_.remove_watcher(args.int_args[1]);
    }
    return EffectResult::kOk;
}

EffectResult effect_518_damage_gate_stat(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 518, args, "需要ctx");
    EFFECT_ARG_GUARD(args.int_args && args.int_count >= 5, 518, args,
                     "518布局=[owner,target,阈值,官方能力槽,变化量]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int threshold = args.int_args[2];
    const int official_slot = args.int_args[3];
    const int delta = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner < 2 && target >= 0 && target < 2 && owner != target,
                     518, args, "需要不同的双方");
    EFFECT_ARG_GUARD(threshold >= 0 && official_slot >= 0 && official_slot < 6,
                     518, args, "阈值非负，官方能力槽0..5");
    if (!g_core || !g_core->stat_change || delta == 0 || ctx->getPet(owner).hp <= 0) {
        return EffectResult::kOk;
    }
    const int stat = kOfficialToEngine[official_slot];
    const int round = ctx->roundCount;
    const int seq = ctx->skill_use_seq[owner];
    // 回调被EventCenter复制后调用：句柄必须共享，不能依赖mutable闭包局部状态。
    const auto watch_id = std::make_shared<int>(0);
    *watch_id = ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, round, /*duration_rounds=*/1, /*once=*/false,
        [owner, target, threshold, stat, delta, round, seq, watch_id]
        (BattleContext* c, const BattleEvent& ev) {
            if (!c || c->roundCount != round || c->skill_use_seq[owner] != seq) return;
            if (ev.actor != owner || ev.target != target) return;
            if (ev.damage_kind != static_cast<int>(DamageKind::NORMAL)) return;
            if (ev.state != static_cast<int>(State::BATTLE_FIRST_ATTACK_DAMAGE)
                && ev.state != static_cast<int>(State::BATTLE_SECOND_ATTACK_DAMAGE)) return;
            c->event_center_.remove_watcher(*watch_id);
            if (ev.amount > threshold && c->getPet(owner).hp > 0) {
                (void)g_core->stat_change(c, owner, stat, delta);
            }
        }, WatcherScope::ON_STAGE, ctx->watcher_valid_id[owner]);

    // 全挡不发TAKE_DAMAGE：用一次性行动桥清走未消费监听，不遗留“下次攻击”窗口。
    for (State st : {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect cleanup(518, 0, owner, /*left_round=*/0,
                       EffectArgs(std::vector<int>{owner, *watch_id}), &remove_damage_watch);
        auto ce = std::make_unique<ContinuousEffect>(cleanup, st, owner, 1, round);
        ce->source_id_ = 518;
        ctx->register_skill_effect(st, owner, std::move(ce));
    }
    return EffectResult::kOk;
}
}  // namespace

SKILL_EFFECT(518, effect_518_damage_gate_stat)
