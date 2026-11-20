// K179 攻击害怕+免死锁血族 —— effect 400（1 宏 / 2 行）
//
// 行清单（tools/soul_effects_dump.py 400，2026-10-07）：
//   400 夜魔之神·布莱克 2833 args[1, 30, 6, 1, 1, 1]：「每次使用攻击技能都有30%概率
//     令对手害怕；受到致命伤害时有1次机会残留1点体力（boss无效）」/ 进化行
//     args[1, 40, 6, 1, 1, 1]（40%，"进化"行，同构互锚）
//     → db[0]/db[3] 恒 1（官方 tips 无对应子句，不消费——记档）、db[1]=害怕概率、
//       db[2]=异常码（6 害怕）、db[4]=免死次数、db[5]=残留体力。
//
// ⚠️ 实现形态（T4 类型化行参）：双子句**尾注同挂全句**（"（boss无效）"收尾覆盖两支，
//    与 718 的子句中缀不同）→ 单 enter 节点接线双 watcher，boss_invalid=true：
//      ①攻击害怕：EVENT_SKILL_USED own+!used_attribute → apply_anomaly_checked(db[2],
//        chance=db[1])——K176 同窗（声明类型事件面，§四点五 判据）；
//      ②免死锁血：EVENT_TAKE_DAMAGE 受击（1067 艾恩斯 legacy_guards 直系）——
//        hp==0 且次数未尽 → 残留 db[5] 点体力、扣减次数（soulmark_storage 运行时，
//        切换保留）。追加 onstage 宿主门：bench 上的 400 不替队友挡致命伤
//        （1067 为 ROSTER 语义未加此门，本行默认 STAGE 语义从紧）。
//
// ⚠️ 三查记录（author 2026-10-07 / k-prod-2 复核同日）：①内核无 400 魂印行为硬编码
//    ②effect_info 400=技能空间同号（魂印以 effect_icon tips 为权威；soul_lib 内 400
//    撞号甄别=护盾常量，非注册）③soul_lib 并集无同 id 注册——真缺口。
// 先例：undying_1067（免死锁血运行时/受击窗全形态）、K176（SKILL_USED 概率申报窗）。
// 场景回归：test/scenario/scenario_3120_fear_undying.cpp（号段 3120-3129）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>
#include <effects/rule_center.h>

namespace {

// ── 宿主行参数（方案 B：db_at 读 owned_int_args **原序**，与真实对局一致）─────
// ⚠️ 历史沿革：本文件原用 a.row_or(i, …) 读 SoulNodeArgs.row，但 bind_soulmark_args
//    （soul_mark_manager.cpp:351）会丢弃 owned_int_args 前两位 → 真实对局里
//    a.row = DB args[2:]（多数情况为空）→ 读值错位/落 fallback。
//    2026-10-21 按方案 B 迁移为 db_at（读 owned_int_args 原序）；场景同步改为
//    按 DB args 原样构造（不再补 {0,0} 占位）。
struct HostRow {
    const std::vector<int>* db = nullptr;
    int monster_id = 0;
    bool valid = false;
};

HostRow host_row(BattleContext* ctx, const SoulNodeArgs& a, int mark_id) {
    HostRow row;
    if (!ctx) {
        return row;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return row;
    }
    int slot = a.host_slot;
    if (slot < 0 || slot > 5) {
        slot = ctx->find_pet_with_soulmark(owner, mark_id);
    }
    if (slot < 0 || slot > 5) {
        return row;
    }
    const ElfPet& host = ctx->seerRobot[owner].elfPets[slot];
    row.db = &host.soulMark.args.owned_int_args;
    row.monster_id = host.soulMark.monster_id;
    row.valid = true;
    return row;
}

int db_at(const HostRow& row, int idx, int fallback) {
    if (row.db && idx >= 0 && idx < static_cast<int>(row.db->size())) {
        return (*row.db)[idx];
    }
    return fallback;
}


constexpr int kMarkId = 400;

struct K179Runtime {
    bool watcher_armed = false;   // 双 watcher 只 arm 一次（切换不丢，1067 同款）
    int saves_left = 0;           // db[4]：免死剩余次数
};

K179Runtime& k179_runtime(BattleContext* ctx, int owner) {
    auto& storage = ctx->getPet(owner).soulmark_storage;
    if (!storage.count(kMarkId)) {
        storage[kMarkId] = K179Runtime{};
    }
    return std::any_cast<K179Runtime&>(storage[kMarkId]);
}

// ── enter 节点：接线①攻击害怕 + ②免死锁血（两子句 boss无效，节点统一置位）──
EffectResult k179_400_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !g_core
        || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    K179Runtime& rt = k179_runtime(ctx, owner);
    if (rt.watcher_armed) {
        return EffectResult::kOk;
    }
    rt.watcher_armed = true;
    const HostRow row = host_row(ctx, a, 400);
    rt.saves_left = db_at(row, 4, 1);
    // 行参走类型化 SoulNodeArgs（T4）：db[1]=概率、db[2]=异常码、db[5]=残留体力。
    const int chance = db_at(row, 1, -1);
    const int fear_id = db_at(row, 2, static_cast<int>(AbnormalStatusId::Fear));
    const int residual = db_at(row, 5, 1);
    const int opp = ctx->opponent(owner);

    // ① 每次使用攻击技能 {db[1]}% 令对手害怕（K176 同窗：声明类型事件面）。
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, opp, chance, fear_id](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || chance < 0) {
                return;
            }
            if (ev.actor != owner || ev.used_attribute) {
                return;   // own + 攻击技（属性技不触发）
            }
            if (!onstage_has_soul_mark(wctx, owner, kMarkId)) {
                return;
            }
            (void)g_core->apply_anomaly_checked(wctx, opp, fear_id,
                                                random_anomaly_duration(), owner,
                                                chance, ChanceSource::SoulMark);
        },
        WatcherScope::TEAM);

    // ② 受致命伤 {db[4]} 次机会残留 {db[5]} 点体力（1067 直系 + onstage 宿主门）。
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, residual](BattleContext* wctx, const BattleEvent& ev) {
            if (ev.target != owner || ev.actor == owner) {
                return;   // 仅"对手出手"受击（自伤/反弹不耗次数，1067 同门）
            }
            if (!onstage_has_soul_mark(wctx, owner, kMarkId)) {
                return;   // bench 不替队友挡致命伤
            }
            ElfPet& self = wctx->getPet(owner);
            K179Runtime& rt = k179_runtime(wctx, owner);
            if (self.hp != 0 || rt.saves_left <= 0) {
                return;
            }
            --rt.saves_left;   // 先耗次数（幂等，1067 同序）
            self.hp = residual;
        },
        WatcherScope::TEAM);

    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（400 入账，只增不改）────────
SOUL_PROGRAM(kMarkId, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k179_400_arm,
                   /*once=*/false, /*early=*/true, SoulScope::STAGE,
                   /*boss_invalid=*/true)});
