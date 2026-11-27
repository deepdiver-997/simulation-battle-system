// 圣光莫妮卡（神谕 3711）魂印 2492 —— 宁界之律 / 琼华之庇 / 最优系别（精灵王线 2026-09-24）
//
// 官方全文（effect_icon 2072 / effect 2492，**boss 有效**）：
//   出战时，自身获得4道宁界之律，然后自身消耗对应位置技能的全部PP值并恢复己方上只在场
//   精灵PP值为0的技能全部PP值，触发成功则为其附加1道琼华之庇
//   | 自身拥有宁界之律时，选择使用技能不受PP值限制 且回合结束时，若对手当回合未选择技能
//   则消耗1层并令对手沉默2回合，未触发则2回合内自身将对手、对方上只在场精灵的最优系别
//   作为对手系别计算克制系数，自身宁界之律层数达到5时前述所有回合数+1 |（boss无效→此处 boss 有效）
//
// 触发成功口径（语料 idx=94 + 用户补充）：上只在场精灵存在且存活，且其有 pp==0 的技能位
// 可恢复；"0/0 技能"（无限 PP，maxPP<=0）不算可恢复——**第五位（槽 4）0/0 永远算成功**。
// 无论恢复几个技能，琼华之庇只 +1。琼华之庇数值效果（保留 PP/增减伤）官方文本未给值——
// 先只记账（side 级计数），待实测补。
// 最优系别 = 伤害 snapshot 处在 {对手在场系别, 对方上只在场系别} 候选取克制最优
//（battleFsm 593 消费 ws.optimal_elem_rounds，K7）。
#include "common.h"
#include "../monika_runtime.h"

namespace {

constexpr int kMark = 2492;

inline bool onstage_2492(BattleContext* ctx, int owner) {
    return ctx->on_stage[owner] >= 0 && ctx->on_stage[owner] < 6
        && ctx->getPet(owner).soulMark.id == kMark;
}

// 出场 PP 转移链：成功返回 true 并给上只 ally +1 庇（side 计数）。
bool pp_transfer_chain(BattleContext* ctx, int owner) {
    const int prev_slot = ctx->prev_on_stage_slot[owner];
    if (prev_slot < 0 || prev_slot > 5) {
        return false;   // 不存在上只（首发）
    }
    ElfPet& ally = ctx->seerRobot[owner].elfPets[prev_slot];
    if (ally.hp <= 0) {
        return false;   // 上只死亡
    }
    bool any_qualifying = false;
    for (int i = 0; i < 5; ++i) {
        const bool qualifying = ally.skills[i].pp == 0
            && (i == 4 || ally.skills[i].maxPP > 0);   // 0/0 不算；第五位 0/0 永远算
        if (!qualifying) {
            continue;
        }
        any_qualifying = true;
        // 自身消耗对应位置技能的全部 PP（有多少耗多少；无限 PP 槽不动）
        ElfPet& me = ctx->getPet(owner);
        if (me.skills[i].pp > 0) {
            me.skills[i].pp = 0;
        }
        if (g_core && g_core->pp_restore_slot) {
            g_core->pp_restore_slot(ctx, owner, prev_slot, i);
        }
    }
    if (!any_qualifying) {
        return false;
    }
    int bi = moni_pub_get(ctx->plugin_storage, kMoniPubBi + owner);
    if (bi < 4) {
        moni_pub_set(ctx->plugin_storage, kMoniPubBi + owner, bi + 1);
    }
    // PP 保留标记（语料 idx=94"挂着1层印记可以跳掉帝天战佛"）：庇绑定被恢复的队友
    // 槽位——对手来源的清除/归零对该槽技能无效（pp_zero_slot 消费），被清时按
    // blocked=true 发事件供下方 watcher 挂对手先制限制。庇不掉层 → 标记不清。
    ctx->pp_retain_pet[owner][prev_slot] = true;
    return true;
}

// 登场（战斗开始 + 每次切换上场）：钩子由 activate_soul_mark 显式调用——
// 中场 CHOOSE_PET 换宠也保证触发（OPERATION_ENTER_EXIT_STAGE 节点在换宠路径不跑）。
void monika_on_enter(BattleContext* ctx, int owner) {
    if (!ctx || owner < 0 || owner > 1) {
        return;
    }
    // 钩子对替补位激活也会跑（activate 扫 6 槽），且每回合更新器会重复触发——
    // ① 只有"被激活的那只当前在场"才看出战链；② 每次上场（on_stage_storage 生命周期）
    // 只执行一次：换宠下场清槽 → 再上场重新看出战链，正合"出战时"语义。
    if (!onstage_2492(ctx, owner)) {
        return;
    }
    auto& oss = ctx->getPet(owner).on_stage_storage;
    if (oss.count(kMark)) {
        return;
    }
    oss[kMark] = true;
    // 出战律 = max(当前, 4)：4 为重置值，已到 5 则保留 5（用户口径，max 函数）
    const int cur = moni_pub_get(ctx->plugin_storage, kMoniPubLv + owner);
    moni_pub_set(ctx->plugin_storage, kMoniPubLv + owner, cur > 4 ? cur : 4);
    pp_transfer_chain(ctx, owner);

    // 琼华之庇：持有方（本 side）任意精灵 PP 被效果降低/清零 → 对手"下回合先制无法高于0"
    //（以 priority_nullified 清零语义近似——负先制也会被抬到 0，差异记交接文档待实测）
    ctx->event_center_.register_watcher(
        EventType::EVENT_PP_REDUCED, owner, ctx->roundCount, 0, /*once=*/false,
        [owner](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || ev.amount <= 0 || ev.actor < 0 || ev.actor > 1
                || ev.actor == owner) {
                return;
            }
            if (moni_pub_get(wctx->plugin_storage, kMoniPubBi + owner) <= 0) {
                return;   // 己方没有任何琼华之庇在身
            }
            auto& oss = wctx->getPet(owner).on_stage_storage;
            const int key = 2492300 + ev.actor;
            int until = oss.count(key) ? any_int_or(oss[key]) : -1;
            until = wctx->roundCount + 1;   // "下回合"先制受限
            oss[key] = until;
        },
        WatcherScope::TEAM);
}

EffectResult monika_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_2492(ctx, owner)) {
        return EffectResult::kOk;
    }
    const int lv = moni_pub_get(ctx->plugin_storage, kMoniPubLv + owner);
    // 有律 → 选技能不受 PP 限制（ctx 级信号，无为觉者 2260 同款）
    ctx->ignore_pp[owner] = lv > 0;
    // 非同系攻击免疫门（待落①，用户 2026-09-25 口径）：有律即武装——对手攻击技能
    // 系别 ∉ 自身系别集 → 技能无效（可被穿透凭证穿）。ws 每回合 reset，此处重写。
    ctx->ws.nonnative_immune_armed[owner] = lv > 0;
    // 概率免控（待落②，用户 2026-09-25 口径）：每层 25% 免疫每次**现代**异常施加。
    // 回合窗口票（ROUND_START 重授，固定句柄刷新）；掷骰在 condition 里现场读律层数
    // （施加时点为准，回合中途耗律立即降概率）；主动毒走古早层（apply_anomaly_ancient
    // 只查 Ancient）天然不受影响——"挡不住主动毒"口径。
    if (lv > 0) {
        ctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(),
                            /*anomaly_mask=*/0, /*duration_rounds=*/1, /*source_id=*/2492100,
                            /*soul_immunity=*/false, EffectScope::ON_STAGE,
                            /*counts=*/0, /*source_effect_id=*/2492, ImmunityTier::Modern,
                            /*condition=*/nullptr, [owner](BattleContext* c, int) {
                                const int n =
                                    moni_pub_get(c->plugin_storage, kMoniPubLv + owner);
                                return n > 0 && roll_soul_gate(*c, kMark, 0, owner, owner, 25 * n, 100);
                            });
    }
    // 最优系别视图按到期回合重写 ws（ws 每回合 reset）
    if (ctx->roundCount <= moni_pub_get(ctx->plugin_storage, kMoniPubOptUntil + owner)) {
        ctx->ws.optimal_elem_rounds[owner] = 1;
    }
    // 琼华之庇先制限制落地（ROUND_START 写 ws，赶在 MOVE_RIGHT 先制结算之前）
    {
        auto& oss = ctx->getPet(owner).on_stage_storage;
        for (int side = 0; side < 2; ++side) {
            const int key = 2492300 + side;
            if (oss.count(key)) {
                const int until = any_int_or(oss[key]);
                if (ctx->roundCount <= until) {
                    ctx->ws.priority_nullified[side] = true;
                }
            }
        }
    }
    return EffectResult::kOk;
}

EffectResult monika_phase_end(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_2492(ctx, owner)) {
        return EffectResult::kOk;
    }
    int lv = moni_pub_get(ctx->plugin_storage, kMoniPubLv + owner);
    if (lv <= 0) {
        return EffectResult::kOk;
    }
    const int opp = 1 - owner;
    const bool selected = ctx->roundChoice[opp][0] == 1 /*SELECT_SKILL*/;
    // "成功用出技能" = 行动流程真正走到技能结算（skill_pp_cost_consumed 落账）。
    // 嗑药/切换 = 没选技；被控跳过 = 选了技但流程没走。三者都是"没有成功用出技能"。
    const bool opp_used_skill = selected && ctx->ws.skill_pp_cost_consumed[opp];
    const int extra = (lv >= 5) ? 1 : 0;   // 满 5 层：前述所有回合数 +1
    if (!selected) {
        // 未选择技能：耗 1 律 + 沉默（**5 律不消耗**，用户口径）
        if (lv < 5) {
            moni_pub_set(ctx->plugin_storage, kMoniPubLv + owner, lv - 1);
        }
        if (g_core && g_core->apply_anomaly) {
            g_core->apply_anomaly(ctx, opp, static_cast<int>(AbnormalStatusId::Silence),
                                  2 + extra, owner);
        }
    } else if (!opp_used_skill) {
        // 选了技但被控跳过：只耗 1 律（5 律不消耗），无沉默、无最优系别
        if (lv < 5) {
            moni_pub_set(ctx->plugin_storage, kMoniPubLv + owner, lv - 1);
        }
    } else {
        moni_pub_set(ctx->plugin_storage, kMoniPubOptUntil + owner,
                     ctx->roundCount + 2 + extra);
    }
    return EffectResult::kOk;
}

}  // namespace

SOUL_PROGRAM(2492, {SoulMarkNodeRef(State::BATTLE_ROUND_START, &monika_round_start),
                    SoulMarkNodeRef(State::BATTLE_ROUND_END, &monika_phase_end)});
SOUL_MARK_HOOKS(2492, &monika_on_enter, nullptr);
