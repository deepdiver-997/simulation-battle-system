#ifndef NUTAO_RUNTIME_H
#define NUTAO_RUNTIME_H

// 怒涛·沧岚（神谕 3740）运行时 —— 魂印侧（soul_lib/canglan_pink.cpp 的 2343 补完段）
// 与技能侧（moves_lib/canglan_nutao_skills.cpp）共享的水/泪计数。
//
// ⚠️ 跨 dylib 纪律（oracle_runtime.h 同款）：std::any 存自定义类型跨 RTLD_LOCAL 插件
// dylib 的 typeinfo 不统一 → 跨魂印/技能两侧共享的状态一律走 **int 发布槽**。
//
// 口径（effect_icon 1932 / effect 2343 官方全文 + 语料 idx=263）：
//   永恒之水 = 沧岚自身道数（0..8）。<8 道本次在场累计、下场清；达 8 道后本场下场保留。
//   蚀砚之泪 = **沧岚方**计数（"4泪还能给队友享用"——队友攻击同样吃收益，idx=263）。
//              沧岚不处于护盾状态时攻击后 +1 滴（0..4），下场保留；沧岚被击败时清零。
//              ≥3 滴：对手护盾效果失效（soul 侧 MOVE_RIGHT 置 ws.ignore_shield）；
//              ≥4 滴：对手不可免疫/转化沧岚方的攻击、攻击伤害
//                     （soul 侧授予 PENETRATE_ATTACK 窗口票，ignore_attack_immunity）。

#include <any>
#include <fsm/battleContext.h>

constexpr int kNutaoPubWater = 4690000;   // + side：永恒之水有效道数（0..8，含下场保留部分）
constexpr int kNutaoPubWaterLock = 4690100;  // + side：达 8 道后锁定的下场保留道数（0/8）
constexpr int kNutaoPubTears = 4690200;   // + side：蚀砚之泪滴数（0..4）

inline int nutao_pub_get(const std::map<int, std::any>& store, int key) {
    auto it = store.find(key);
    if (it == store.end()) {
        return 0;
    }
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : 0;
}

inline void nutao_pub_set(std::map<int, std::any>& store, int key, int value) {
    store[key] = value;
}

// 有效永恒之水 = max(本次在场, 8 道锁定)（魂印/技能两侧共用）。
inline int nutao_effective_water(const std::map<int, std::any>& store, int side) {
    const int cur = nutao_pub_get(store, kNutaoPubWater + side);
    const int lock = nutao_pub_get(store, kNutaoPubWaterLock + side);
    return cur > lock ? cur : lock;
}

#endif // NUTAO_RUNTIME_H
