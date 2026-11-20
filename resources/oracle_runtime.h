#ifndef ORACLE_RUNTIME_H
#define ORACLE_RUNTIME_H

// 神谕古王（洪荒·列奥尼达神谕 4125）运行时 —— 魂印侧（soul_lib/oracle_leonidas_4125.cpp）
// 与技能侧（moves_lib/oracle_skills.cpp）共享的 plugin_storage 键约定。
//
// ⚠️ 跨 dylib 纪律（天启星魂线踩坑，见 tianqi_runtime.h 同款注释）：std::any 存自定义
// 类型跨 RTLD_LOCAL 插件 dylib 的 typeinfo 不统一 → 跨魂印/技能两侧共享的状态一律走
// **int 发布槽**（fundamental type 的 typeinfo 全局唯一）。soul 内部的票柄/在场标志
// 用 struct（仅 soul 一个 dylib 访问，不跨界）。

#include <any>
#include <map>

#include <fsm/battleContext.h>

// ── soul 内部运行时（票柄/在场标志；仅 soul_lib 一个 dylib 访问，不跨 dylib cast）──
struct OracleRuntime {
    bool first_use_consumed = false;      // 本次在场首用层已拿
    bool gained_this_presence = false;    // 本次在场是否获得过日晷
    int pp_restore_budget = 0;            // 每层 1 次（登场时按当前层数重置）
    bool pp_restored_this_round = false;
    int lock_triggers_used = 0;           // pp1 锁已触发次数
    bool arm_heal_phase_end = false;      // ②-else：阶段末双方按护盾值回血
    bool arm_weaken_phase_end = false;    // ③：阶段末给对手挂衰弱
    int seal_handle = 0;                  // pp1 锁票
    bool watchers_registered = false;
    int enter_round = 0;                  // 本次登场时的 roundCount（"前 N 回合"基准）
};

// ── int 发布槽（键 = 基数 + side；any<int>）─────────────────────
// 旧日之晷层数（0..5，绑定自身、下场保留——ctx 级槽天然跨切换）
constexpr int kOraclePubSundial = 4680000;
// 宙变之殢层数（0..5，绑定**持有方**：key 加 holder side；对手切换即清零）
constexpr int kOraclePubZhie = 4680100;
// 双方消逝计数（战斗级，2401 的阈值/增伤用）
constexpr int kOraclePubVanish = 4680200;
// 2400"消除自身当回合技能使用记录"请求标志（0/1；moves 置位，soul 的行动后节点消费）
constexpr int kOraclePubClearFirstUse = 4680300;
// 2398"己方上只在场精灵"槽位（-1 = 无；soul 在己方登场事件时写入，2398 读）
constexpr int kOraclePubPrevAlly = 4680400;
// 2396 天衍星霜使用次数（战斗级累计，威力+30%/次 至多 +120%）
constexpr int kOraclePubStarUses = 4680500;

inline int oracle_pub_get(const std::map<int, std::any>& storage, int key) {
    auto it = storage.find(key);
    return it == storage.end() ? 0 : std::any_cast<int>(it->second);
}

inline void oracle_pub_set(std::map<int, std::any>& storage, int key, int value) {
    storage[key] = value;
}

// 宙变之殢层数变更后必须同步冻结标志（层数>0 → 持有方的异常回合数冻结）
inline void oracle_sync_freeze(BattleContext* ctx, int zhie_holder_side, int layers) {
    if (!ctx || zhie_holder_side < 0 || zhie_holder_side > 1) {
        return;
    }
    ctx->set_anomaly_rounds_frozen(zhie_holder_side, layers > 0);
}

#endif  // ORACLE_RUNTIME_H
