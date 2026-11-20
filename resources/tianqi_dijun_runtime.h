#ifndef TIANQI_DIJUN_RUNTIME_H
#define TIANQI_DIJUN_RUNTIME_H

// 天启帝君 3550（魂印 effect_icon 1128）运行时 —— 魂印侧（soul_lib/tianqi_dijun_3550.cpp）
// 与技能侧（moves_lib/tianqi_dijun_skills.cpp）共享的载荷 + 键约定。
// 放独立头：场景测试 090 也要按同一类型 any_cast 读盘面（同名匿名结构体是两个类型，
// any 按类型存取，镜像定义会 bad_any_cast）—— tianqi_runtime.h（4677）同款理由。
#include <any>
#include <map>

// ── 罚/赐 登场削弱状态 ────────────────────────────────────────────
// 存放位置 = **帝君自己那只 pet** 的 soulmark_storage（下场保留：登场计数跨切换累计，
// "下次登场不削弱"（1308 纵横三千界击败对手授予）也要活过下场再登场）。
// 键 = kDijunStorageKey（= 魂印 id 1128；一只帝君一份，不需要 side 维度）。
// ⚠️ pet.soulmark_storage 是**每只 pet 各一份的 map**——镜像对战两边各有一只帝君时
//    天然按 pet 隔离（0号的旗标串不到 1号身上，场景 090 M 组锁定）。别迁移到
//    ctx->plugin_storage：那才是需要 side 维度的形态（4677 星盘运行时如此）。
struct TianqiDijunRuntime {
    int entry_count = 0;               // 登场次数（部署与换宠都算，1 = 首次）
    bool next_entry_no_decay = false;  // 1308：下次登场时帝君之罚/赐不削弱（1/3 档）
    // ── 以下为魂印侧 arm 节点的幂等标志（技能/场景侧只读）──
    bool battle_start_done = false;    // 部署"首次登场"只做一次（EVENT_ENTER_STAGE 不覆盖部署）
    bool deploy_stamped = false;       // 帝君之援：战斗开始部署宠的满体力戳只打一次
    bool watchers_registered = false;  // 三个 watcher（ENTER_STAGE/DEATH/VANISH）只挂一次
};

constexpr int kDijunStorageKey = 1128;

// 便捷访问（可写引用；缺 key = 首次触碰，就地构造零值）。
inline TianqiDijunRuntime& dijun_rt(std::map<int, std::any>& storage) {
    auto it = storage.find(kDijunStorageKey);
    if (it == storage.end()) {
        it = storage.emplace(kDijunStorageKey, TianqiDijunRuntime{}).first;
    }
    return std::any_cast<TianqiDijunRuntime&>(it->second);
}

inline const TianqiDijunRuntime* dijun_rt_find(const std::map<int, std::any>& storage) {
    auto it = storage.find(kDijunStorageKey);
    if (it == storage.end()) {
        return nullptr;
    }
    // any_cast(const any*) 传**非指针 T** → 返回 const T*（传指针类型会得 const T**，编不过）
    return std::any_cast<TianqiDijunRuntime>(&it->second);
}

// ── 帝君之援：满体力登场先制+1 的"本次上场"戳 ─────────────────────
// 存放位置 = **登场的那只己方 pet** 的 on_stage_storage（引擎在它下场时清空，
// 天然"本次上场"生命周期）；键同 kDijunStorageKey。魂印侧 watcher 写入、
// MOVE_RIGHT 节点消费（同在 soul_lib，放共享头只为场景 090 可读）。
struct DijunAidStamp {
    int round = -1;      // 登场时的 roundCount（含部署 = 0）
    bool full_hp = false;  // 登场那一刻是否满体力
};

inline void dijun_stamp_entry(std::map<int, std::any>& on_stage_storage, int round, bool full_hp) {
    on_stage_storage[kDijunStorageKey] = DijunAidStamp{round, full_hp};
}

inline const DijunAidStamp* dijun_stamp_find(const std::map<int, std::any>& on_stage_storage) {
    auto it = on_stage_storage.find(kDijunStorageKey);
    if (it == on_stage_storage.end()) {
        return nullptr;
    }
    return std::any_cast<DijunAidStamp>(&it->second);
}

#endif  // TIANQI_DIJUN_RUNTIME_H
