// 混沌魔尊 3683（effect_icon 968 / effect 1263）—— 登场随机控 + 裸伤阈值族
//
// 官方（**库内版本**；活动版子句"没出控制类补偿封属/天意类改写"不在配置快照——
// "天意"全库零命中，等游戏内实测文本后按池改写票接，见台账"随机附加异常状态原语"行）：
//   ① 首次登场100%令对手随机进入1种**控制类**异常状态
//   ② 之后每次登场100%令对手随机进入1种**任意**异常状态
//   ③ 自身攻击造成伤害高于280则100%令对手随机进入1种异常状态
//   ④ 自身攻击造成伤害低于280则下回合攻击先制+1且必定致命
//   （boss无效：PvP 引擎不建模）
//
// 机制（随机附加异常原语的第一个真实消费方，2026-09-19）：
//   · 池 = anomaly_pool_control() / anomaly_pool_all()，施加走 attach_random_anomalies
//     原语（解析池→概率门→单通道）；池改写票留给"范围改变"层，本魂印直接选池
//   · 登场计数：EVENT_ENTER_STAGE **只在换宠时发**（战斗开始部署不发）→ 部署的
//     "首次登场"由 ENTER_EXIT_STAGE 节点兜底（battle_start_done 幂等，重激活不重复），
//     换宠登场由 watcher 计数；entry_count==1 → 控制池，≥2 → 全池（语料 idx=456：
//     首发的首次登场控在战斗开始结算）
//   · ③④ 读**裸伤** ws.raw_attack_damage（effect 422 族口径——EVENT_TAKE_DAMAGE 的
//     amount 是落血，被减伤/护盾吃掉后判不出"造成伤害高于280"），时点过滤用
//     ev.state ∈ 两个攻击伤害时点（固定/真伤/粉伤不构成"攻击造成伤害"）
//   · ④ = storage 布尔 + 自有 MOVE_RIGHT 节点消费（下回合：先制+1 走攻击门控 +
//     ws.must_crit 必暴——681 同款：暴击判定只对攻击技能跑，无需再限定）
//   · 生命周期：魂印本体随宿主（消逝/阵亡停提供——更新器停刷新兜底）；活动版
//     "游戏开始注册不随消逝/死亡丢失"的 detach 组合是**另一层**，别混进本文件
#include "common.h"

#include <entities/skills.h>

namespace {

constexpr int kChaosMozunMarkId = 1263;

struct ChaosMozunRuntime {
    int entry_count = 0;              // 登场次数（部署与换宠都算）
    bool battle_start_done = false;   // 部署兜底只做一次（ENTER_EXIT_STAGE 桶重入幂等）
    bool watcher_registered = false;  // 两个 watcher 只注册一次
    bool next_round_boost = false;    // ④：本回合触发 → 下回合 MOVE_RIGHT 消费
};

// args（effect_icon "100 1 100 1 280 100 1 1"，bind 前三位 owner/target/host_slot，
// 2026-09-20 主体槽约定）：
//   [3]=首次概率 [4]=首次数量 [5]=之后概率 [6]=之后数量 [7]=伤害阈值 [8]=伤害概率 [9]=伤害数量
// 场景测试手工构造 SoulMark 无 args → 走默认值（= 官方字面数值）。
auto mark_arg_reader(const EffectArgs& args) {
    return [args](int idx, int fallback) {
        return (args.int_args && args.int_count > idx) ? args.int_args[idx] : fallback;
    };
}

void attach_random(BattleContext* ctx, int owner, int prob, int count,
                   const std::vector<int>& pool) {
    g_core->attach_random_anomalies(ctx, 1 - owner, pool, count, prob, owner,
                                    ChanceSource::SoulMark);
}

// ── 节点①：ENTER_EXIT_STAGE（ROSTER）—— 战斗开始部署的"首次登场"兜底 + watcher 注册 ──
EffectResult soulmark_1263_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int host_slot = ctx->find_pet_with_soulmark(owner, kChaosMozunMarkId);
    if (host_slot < 0) {
        return EffectResult::kOk;
    }
    const auto arg = mark_arg_reader(args);
    const int prob_first = arg(3, 100);
    const int count_first = arg(4, 1);
    const int prob_later = arg(5, 100);
    const int count_later = arg(6, 1);
    const int threshold = arg(7, 280);
    const int prob_dmg = arg(8, 100);
    const int count_dmg = arg(9, 1);

    ElfPet& host = ctx->seerRobot[owner].elfPets[host_slot];
    auto& storage = host.soulmark_storage;
    if (!storage.count(kChaosMozunMarkId)) {
        storage[kChaosMozunMarkId] = ChaosMozunRuntime{};
    }
    ChaosMozunRuntime& rt = std::any_cast<ChaosMozunRuntime&>(storage[kChaosMozunMarkId]);

    // 部署兜底：战斗开始这只在场 = 它的"首次登场"（EVENT_ENTER_STAGE 不覆盖部署）。
    // 节点随重激活重跑 → battle_start_done 幂等；袋里首发 → 留给首次换宠登场。
    if (!rt.battle_start_done && ctx->on_stage[owner] == host_slot) {
        rt.battle_start_done = true;
        rt.entry_count = 1;
        attach_random(ctx, owner, prob_first, count_first, anomaly_pool_control());
    }

    if (rt.watcher_registered) {
        return EffectResult::kOk;
    }
    rt.watcher_registered = true;

    // 换宠登场：entry_count++，1=控制池、≥2=全池。emit 时 on_stage 已指向新宠。
    ctx->event_center_.register_watcher(
        EventType::EVENT_ENTER_STAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, host_slot, prob_first, count_first, prob_later, count_later](
            BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || ev.actor != owner || wctx->on_stage[owner] != host_slot
                || wctx->is_vanished(owner, host_slot)) {
                return;
            }
            auto& st = wctx->seerRobot[owner].elfPets[host_slot].soulmark_storage;
            if (!st.count(kChaosMozunMarkId)) {
                return;
            }
            ChaosMozunRuntime& rt = std::any_cast<ChaosMozunRuntime&>(st[kChaosMozunMarkId]);
            ++rt.entry_count;
            if (rt.entry_count == 1) {
                attach_random(wctx, owner, prob_first, count_first, anomaly_pool_control());
            } else {
                attach_random(wctx, owner, prob_later, count_later, anomaly_pool_all());
            }
        },
        WatcherScope::TEAM);

    // ③④：自身攻击造成伤害后（裸伤阈值二选一）。
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, host_slot, threshold, prob_dmg, count_dmg](BattleContext* wctx,
                                                           const BattleEvent& ev) {
            if (!wctx || ev.actor != owner || ev.target == owner) {
                return;
            }
            // 只认攻击伤害两时点（红伤）。固定/真伤/粉伤的时点不同，天然滤掉。
            if (ev.state != static_cast<int>(State::BATTLE_FIRST_ATTACK_DAMAGE)
                && ev.state != static_cast<int>(State::BATTLE_SECOND_ATTACK_DAMAGE)) {
                return;
            }
            auto& st = wctx->seerRobot[owner].elfPets[host_slot].soulmark_storage;
            if (!st.count(kChaosMozunMarkId)) {
                return;
            }
            ChaosMozunRuntime& rt = std::any_cast<ChaosMozunRuntime&>(st[kChaosMozunMarkId]);
            const int raw = wctx->ws.raw_attack_damage[owner];   // 裸伤（422 族口径）
            if (raw > threshold) {
                attach_random(wctx, owner, prob_dmg, count_dmg, anomaly_pool_all());
            } else if (raw < threshold) {
                rt.next_round_boost = true;   // ④：下回合消费
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 节点②：BATTLE_FIRST_MOVE_RIGHT（ROSTER）—— 消费 ④ 的"下回合"窗口 ──
// 桶在 preemptive memset 之后跑（battleFsm handle_BattleFirstMoveRight）→ +1 不被冲掉；
// 束缚的"先制失效"在两桶之后统一清零（官方口径：束缚压得过靠后改先制的效果）。
EffectResult soulmark_1263_move_right_boost(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int host_slot = ctx->find_pet_with_soulmark(owner, kChaosMozunMarkId);
    if (host_slot < 0) {
        return EffectResult::kOk;
    }
    auto& st = ctx->seerRobot[owner].elfPets[host_slot].soulmark_storage;
    if (!st.count(kChaosMozunMarkId)) {
        return EffectResult::kOk;
    }
    ChaosMozunRuntime& rt = std::any_cast<ChaosMozunRuntime&>(st[kChaosMozunMarkId]);
    if (!rt.next_round_boost) {
        return EffectResult::kOk;
    }
    rt.next_round_boost = false;   // 下回合一次性
    // "攻击先制+1"：本次选的是攻击技能才吃先制（属性技能不吃——681/481 同款门）
    if (ctx->roundChoice[owner][0] == 1 /*SELECT_SKILL*/) {
        const int idx = ctx->roundChoice[owner][1];
        if (idx >= 0 && idx < static_cast<int>(ctx->getPet(owner).skills.size())
            && ctx->getPet(owner).skills[static_cast<std::size_t>(idx)].type
                   != SkillType::Attribute) {
            ctx->ws.preemptive_level[owner] += 1;
        }
    }
    ctx->ws.must_crit[owner] = true;   // 必定致命（暴击判定只对攻击技能跑）
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）──
// boss 无效标注（2026-09-26）：整条魂印单子句、尾注"（boss无效）"覆盖全部子动作
// （登场控/高伤控/低伤先制必暴）→ 两节点全标；watcher 由 arm 注册，节点被跳过即不挂。
SOUL_PROGRAM(kChaosMozunMarkId,
             {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_1263_arm,
                              /*once=*/false, /*early=*/false, SoulScope::ROSTER,
                              /*boss_invalid=*/true),
              SoulMarkNodeRef(State::BATTLE_FIRST_MOVE_RIGHT, &soulmark_1263_move_right_boost,
                              /*once=*/false, /*early=*/false, SoulScope::ROSTER,
                              /*boss_invalid=*/true)});
