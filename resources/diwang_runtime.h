#ifndef DIWANG_RUNTIME_H
#define DIWANG_RUNTIME_H

// 混沌地王莱茵哈特（神谕 3516）运行时 —— 魂印侧（soul_lib/hundun_diwang_2411.cpp）
// 与技能侧（moves_lib/hundun_diwang_skills.cpp）共享的嶂/蛊层数。
//
// ⚠️ 跨 dylib 纪律（oracle_runtime.h 同款）：跨魂印/技能两侧共享的状态一律走 int 发布槽。
//
// 口径（effect_icon 1985 / effect 2411 官方全文 + 语料 idx=190 解析）：
//   封邪之嶂 = 混地自身层数（soulmark_storage 下场保留；发布槽供技能侧读）。
//             每层：己方护盾承伤 +10%（ws.shield_absorb_bonus_pct，混地本人暂同享——
//             解析"本人不收益"的精确排除待实测，当前按方生效，见交接文档）。
//   留魂之蛊 = **对方**层数（side 级计数，简化自"对方每个精灵最多4层"，见交接文档）。
//             对方每次恢复体力消耗 1 层 → 混地方对其插入 40 点真实伤害；每回合至多 4 次。

#include <any>
#include <fsm/battleContext.h>

constexpr int kDiwangPubZhang = 4691000;   // + side：封邪之嶂层数（0..4）
constexpr int kDiwangPubGu = 4691100;      // + side：留魂之蛊层数（0..4，挂在对方身上的总数）
constexpr int kDiwangPubGuCap = 4691200;   // + side：蛊当回合已触发次数（回合界清）

constexpr int kDiwangGuHealDmg = 40;       // 每层蛊插入的真伤
constexpr int kDiwangGuRoundCap = 4;       // 每回合插入次数上限
constexpr int kDiwangShieldUnit = 400;     // 魂印盾单位（阶段末 盾值=400 正整数倍 → 种蛊）

inline int diwang_pub_get(const std::map<int, std::any>& store, int key) {
    auto it = store.find(key);
    if (it == store.end()) {
        return 0;
    }
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : 0;
}

inline void diwang_pub_set(std::map<int, std::any>& store, int key, int value) {
    store[key] = value;
}

#endif // DIWANG_RUNTIME_H
