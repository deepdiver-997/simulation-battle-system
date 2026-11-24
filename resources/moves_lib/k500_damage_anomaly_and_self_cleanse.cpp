// K500 · 1074 高伤双层掷点回退 / 2181 解除自身异常（2026-11-23）。
// 工单 〔源: docs_local/docs/05-任务清单/已收官/E组批量效果工单.md:105〕
// 数据来源 https://github.com/SeerAPI/seer-unity-assets ，当前只读库双表重切：
// 1074 args_num/arg_count=6/6，30192[280,80,6,2,50,6]（纯链）、
// 32405[280,100,6,2,80,7]、34014[300,100,6,2,50,6]、
// 34282[300,100,5,1,100,20]、38370[280,100,24,2,100,26]；
// 2181两表=0/0，28731与37997两载体；七条全链零剩余，无friend载体。
// 交叉文案 http://news.4399.com/gonglue/seer/tujian/916274.htm （34282）、
// http://news.4399.com/gonglue/seer/tujian/1005181.htm （37997）、
// http://news.4399.com/gonglue/seer/tujian/993813.htm （28731）。
// 三查：src/include无同号硬编码；effect_meta无覆盖，两id启发式为Other；
// custom overrides/programs/selection_routes/combo_rules均无行（路由总数39不动）。
// moves/soul无同号注册，无拆分迁移/故意退役记录。注意1074不是原先完全惰性：
// 废弃parser会把“使对手{5}”误作立即概率异常，忽略伤害门与下N回合；正式注册接管，
// 不扩parser。2181无parser解除分支。场景对低伤/同回合/未选择载体均设负测。
// 〔源: src/effects/effect_unit_parser.cpp:73-104〕
// 〔源: src/effects/effect_unit_parser.cpp:199-229〕
//
// 1074短句锚点：“造成的伤害大于{0}”。第一层沿K498 518：TAKE_DAMAGE的超杀钳制
// 实扣红伤，不是DETECT final；严格>，门闭整段不结算（含回退，沿K474）。
// actor/target、NORMAL、ATTACK_DAMAGE、当轮当次seq全过滤；首笔匹配即自拆，
// 全挡/全盾无事件时AFTER_ACTION一次性桥清理，不遗留监听给下一刀。
// 〔源: resources/moves_lib/k498_damage_gate_stat.cpp:84-105〕
// 〔源: resources/moves_lib/k474_lowdmg_fallback_and_purge_delta.cpp:309-335〕
// “未触发”定案为未落地宽口径：SUCCESS/REPLACED_EXISTING/DURATION_EXTENDED/
// CONVERTED算触发；免疫/抗性/反弹/掷歪/禁附加算未触发。四条100%主支仍明载回退，
// 尤其34282的100%冻伤→100%失明，支持不是仅ROLL_FAILED的解读；沿K474/K497，
// 与K457遗留张力同档记账：这是载体上下文建模，非官方对拍裁决。
// 〔源: resources/moves_lib/k474_lowdmg_fallback_and_purge_delta.cpp:207-213〕
// 击倒目标时原语返回TARGET_DEFEATED亦属未落地，施放者存活才保留下轮回退；
// 阈值仍只看实扣，低HP超杀不会把管线final误当过门。此死亡分支无官方实测。
// 第二层沿K474：NextRounds [R+1,R+N]，显式armed_round挡施放当回合；
// 自身成功执行攻击技后每回合申报{4}%（miss/被盔算使用，NONE/被控/属性技不算）。
// 两层均apply_anomaly_checked + ChanceSource::Skill，0/100不补默认、不预掷。
// 〔源: resources/moves_lib/k474_lowdmg_fallback_and_purge_delta.cpp:234-244〕
// 〔源: resources/moves_lib/common.h:218-228〕
// 重施放失败刷新同源窗、不叠加；本次成功不取消前次尚有效窗。离场/断回合由ON_STAGE
// epoch/桶原生作废。清理桥source_id=0，避免和同1074的AFTER_ACTION回退窗互相覆盖。
// 〔源: include/effects/timed_bucket.h:38-47〕
//
// 2181短句锚点：“解除自身所处的异常状态”。零参无类别限定，调用正式
// dispel_active_anomalies自身侧，一次性清全部有效异常（包括附属类），非减少回合、
// 非清等级/清对手/授免疫；继承原语的本体解除面，不拿虚拟“视为无异常”遮蔽当免解。
// 37997链2137→2181→3：先采样增伤，再解除异常，最后消自身下降；不拖到伤害后。
// 〔源: src/primitives/battle_primitives.cpp:883-896〕
// 〔源: resources/moves_lib/k493_anomaly_round_amp.cpp:9-16〕
//
// 显示：回退窗effectId/sourceId=1074，args前六位保留官方参数、尾部放owner/armed_round，
// 与客户端直接模板代入一致；一次性清理桥/2181无驻留描述义务，不填空描述。
// 〔源: web/frontend/src/effectText.js:32-35〕
// 〔源: docs/07-工具与测试/效果显示gate规范.md:34-49〕
// 键审计：不分配plugin/on_stage/soulmark storage，候选1010740000/1021810000全段零占用。
// soul同号数据分别effect_icon790/pet3477、1789/pet4578，但均无运行时实现/键段，
// 按K481判据无需拆子块；本单亦无新增G4 storage债务。

#include "common.h"

#include <memory>

namespace {
bool anomaly_landed(ApplyAnomalyResult r) {
    return r == ApplyAnomalyResult::SUCCESS
        || r == ApplyAnomalyResult::REPLACED_EXISTING
        || r == ApplyAnomalyResult::DURATION_EXTENDED
        || r == ApplyAnomalyResult::CONVERTED;
}

// payload=[官方六参,owner,armed_round]；描述的{0}..{5}无需跳过内部前缀。
EffectResult fallback_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 8 || !g_core
        || !g_core->apply_anomaly_checked) return EffectResult::kOk;
    const int owner = args.int_args[6];
    const int armed_round = args.int_args[7];
    if (owner < 0 || owner > 1 || ctx->roundCount <= armed_round
        || ctx->getPet(owner).hp <= 0) return EffectResult::kOk;
    if (!executed_skill_this_round(ctx, owner) || !executed_skill_is_attack(ctx, owner)) {
        return EffectResult::kOk;
    }
    (void)g_core->apply_anomaly_checked(ctx, 1 - owner, args.int_args[5],
                                      random_anomaly_duration(), owner, args.int_args[4],
                                      ChanceSource::Skill);
    return EffectResult::kOk;
}

EffectResult remove_damage_watch(BattleContext* ctx, const EffectArgs& args) {
    if (ctx && args.int_args && args.int_count >= 1) {
        ctx->event_center_.remove_watcher(args.int_args[0]);
    }
    return EffectResult::kOk;
}

EffectResult effect_1074_damage_anomaly_fallback(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1074, args, "需要ctx");
    EFFECT_ARG_GUARD(args.int_args && args.int_count >= 8, 1074, args,
                     "1074布局=[owner,target,阈值,概率,异常码,回合,概率,异常码]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner < 2 && target == 1 - owner, 1074, args,
                     "需要对向双方");
    const std::vector<int> params(args.int_args + 2, args.int_args + 8);
    EFFECT_ARG_GUARD(params[0] >= 0 && params[1] >= 0 && params[1] <= 100
                         && params[3] > 0 && params[4] >= 0 && params[4] <= 100,
                     1074, args, "阈值非负、概率0..100、回合为正");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(params[2])
                         && is_valid_abnormal_status_id(params[5]),
                     1074, args, "需要有效异常码");
    if (!g_core || !g_core->apply_anomaly_checked || ctx->getPet(owner).hp <= 0) {
        return EffectResult::kOk;
    }
    const int round = ctx->roundCount;
    const int seq = ctx->skill_use_seq[owner];
    const auto watch_id = std::make_shared<int>(0);
    *watch_id = ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, round, /*duration_rounds=*/1, /*once=*/false,
        [owner, target, params, round, seq, watch_id](BattleContext* c, const BattleEvent& ev) {
            if (!c || c->roundCount != round || c->skill_use_seq[owner] != seq) return;
            if (ev.actor != owner || ev.target != target) return;
            if (ev.damage_kind != static_cast<int>(DamageKind::NORMAL)) return;
            if (ev.state != static_cast<int>(State::BATTLE_FIRST_ATTACK_DAMAGE)
                && ev.state != static_cast<int>(State::BATTLE_SECOND_ATTACK_DAMAGE)) return;
            c->event_center_.remove_watcher(*watch_id);
            if (ev.amount <= params[0] || c->getPet(owner).hp <= 0) return;
            const auto result = g_core->apply_anomaly_checked(
                c, target, params[2], random_anomaly_duration(), owner, params[1],
                ChanceSource::Skill);
            if (anomaly_landed(result)) return;
            std::vector<int> payload = params;
            payload.push_back(owner);
            payload.push_back(round);
            for (State st : {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
                register_next_rounds_effect(c, st, owner, 1074, params[3], payload, &fallback_tick);
            }
        }, WatcherScope::ON_STAGE, ctx->watcher_valid_id[owner]);

    for (State st : {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect cleanup(1074, 0, owner, /*left_round=*/0,
                       EffectArgs(std::vector<int>{*watch_id}), &remove_damage_watch);
        auto ce = std::make_unique<ContinuousEffect>(cleanup, st, owner, 1, round);
        ce->source_id_ = 0;  // 一次性桥不和1074驻留窗同键覆盖。
        ctx->register_skill_effect(st, owner, std::move(ce));
    }
    return EffectResult::kOk;
}

EffectResult effect_2181_self_cleanse(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2181, args, "需要ctx");
    EFFECT_ARG_GUARD(args.int_args && args.int_count >= 2, 2181, args,
                     "2181布局=[owner,target]，官方零参");
    const int owner = args.int_args[0];
    EFFECT_ARG_GUARD(owner >= 0 && owner < 2 && args.int_args[1] == 1 - owner, 2181, args,
                     "需要对向双方");
    if (g_core && g_core->dispel_active_anomalies) {
        (void)g_core->dispel_active_anomalies(ctx, owner);
    }
    return EffectResult::kOk;
}
}  // namespace

SKILL_EFFECT(1074, effect_1074_damage_anomaly_fallback)
SKILL_EFFECT(2181, effect_2181_self_cleanse)
