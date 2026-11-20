#ifndef MISTIKE_RUNTIME_H
#define MISTIKE_RUNTIME_H

// 神觉·米斯蒂克 4676 共享常量与发布槽 —— 魂印侧（soul_lib/shenjue_mistike_4676.cpp）与
// 技能侧（moves_lib/mistike_skills.cpp）共用。
//
// ⚠️ 跨 dylib 只发布 **int**（std::any 存自定义结构体在 RTLD_LOCAL 两个插件 dylib 之间
//   typeinfo 不统一，any_cast 必失败——见 tianqi_runtime.h 的说明）。各自内部的
//   运行时结构体（魂印的窗口/票柄、2212 的窗口）留在各自 .cpp 里，不跨库共享。
#include <any>
#include <map>
#include <string>

// 官方印记名：蚩庸之锁（魂印 + 技能 2211/2214 共用同一套层，用户 2026-09-20 确认）。
inline const char* mistike_lock_name() {
    return "蚩庸之锁";
}
constexpr int kMistikeLockCap = 3;   // 官方：最高叠加 3 道

// ⚠️ 注册键 = effect_icon.effect_id = 2268（装配层 SoulMark.id 按 effect_id 查程序表）。
//    4676 是 monster id，只用于命名与注释——错键会让程序在真实对局挂不上（2026-09-27 修复）。
constexpr int kMistikeMarkId = 2268;
constexpr int kMistikeFatigueId = 7; // 疲惫（2212/1471/449 的控场异常）

// ── 发布槽（int，双方读取）─────────────────────────────────────────
// 锁层数：kMistikePubLockBase + 对方 side（魂印/2211 写，2214/测试读）。
constexpr int kMistikePubLockBase = 4676000;
// 持锁槽位追踪：kMistikePubLockSlotBase + 对方 side（-1 = 无锁）。
// 魂印与 2211 都经它登记"锁在哪个槽"——中切清空/衰减/命中视图都按它定位（跨 dylib 共享）。
constexpr int kMistikePubLockSlotBase = 4676500;

inline void mistike_publish_locks(std::map<int, std::any>& storage, int side, int locks) {
    storage[kMistikePubLockBase + side] = locks;
}

inline int mistike_pub_locks(const std::map<int, std::any>& storage, int side) {
    auto it = storage.find(kMistikePubLockBase + side);
    return it == storage.end() ? 0 : std::any_cast<int>(it->second);
}

inline void mistike_publish_lock_slot(std::map<int, std::any>& storage, int side, int slot) {
    storage[kMistikePubLockSlotBase + side] = slot;
}

inline int mistike_pub_lock_slot(const std::map<int, std::any>& storage, int side) {
    auto it = storage.find(kMistikePubLockSlotBase + side);
    return it == storage.end() ? -1 : std::any_cast<int>(it->second);
}

#endif  // MISTIKE_RUNTIME_H
