#ifndef KONGYUAN_RUNTIME_H
#define KONGYUAN_RUNTIME_H

// 空元行者 4586 —— 魂印侧（soul_lib/vanish_kongyuan.cpp）与技能侧
// （moves_lib/kongyuan_skills.cpp）共享的 **plugin_storage 键约定 + 发布槽**。
//
// 为什么要这一层：**诗章的权威状态在魂印侧**（`pet.soulmark_storage[2175]` 里的
// `KongYuanState`，见 vanish_kongyuan.cpp），而读/写它的技能（2092 告命诗途、2094 遗典·妄世律裁）
// 在 **另一个 dylib** 里——插件之间不互相链接，只能经 `ctx->plugin_storage` 这个共同面通信。
//
// ⚠️ **只放基本类型（int）**：`std::any` 存自定义类型时 typeinfo 在两个 RTLD_LOCAL 插件 dylib
//   之间不统一，soul↔moves 之间的 `any_cast` 必炸（天启星魂 4677 线 2026-09-20 踩过，
//   见 `tianqi_runtime.h` 同款注释）。结构体本体只在各自 dylib 内部使用。
//
// 两个方向：
//   · **publish**：魂印每次改动诗章后写 `kPoemsPublishBase + side`；技能侧只读。
//   · **command**：技能侧写 `kPoemsTargetBase + side`（"补充至至少 N 篇"）/
//     `kPoemsAddBase + side`（"额外书写 N 篇"）；魂印侧在下一次刷新节点消费并清命令
//     （ROUND_START + 之诗声明前各消费一次，故同回合内就能生效）。
//
// 放独立头（不进 common.h）：场景测试也要按同一键读，镜像定义会各算一个类型。
#include <any>
#include <map>

namespace kongyuan {

// ⚠️ 键段选 8 位段（21751000+）：`vanish_kongyuan.cpp` 自己的接线旗标用
//    `2175000 + owner*10 + salt`（armed_flag）——**曾撞键**：publish 槽取 2175000 时与
//    armed_flag(owner=0,salt=0) 同键，`any_cast<int&>` 撞上 bool → bad_any_cast 崩在
//    ROUND_START（场景 103 实测）。新增键一律先 grep 全库 `plugin_storage` 键段。
constexpr int kPoemsPublishBase = 21751000;   // + side → 空妄诗章篇数（权威值，只读）
constexpr int kPoemsTargetBase = 21751100;    // + side → "补充至至少 N 篇"（0 = 无命令）
constexpr int kPoemsAddBase = 21751200;       // + side → "额外书写 N 篇"（0 = 无命令）

// 空妄诗章上限（effect_des 240："最多累计 12 篇"）——魂印侧钳，这里也留一份给读者用。
constexpr int kMaxPoems = 12;

inline int& kongyuan_slot(std::map<int, std::any>& storage, int key) {
    auto it = storage.find(key);
    if (it == storage.end()) {
        it = storage.emplace(key, 0).first;
    }
    return std::any_cast<int&>(it->second);
}

inline void kongyuan_publish_poems(std::map<int, std::any>& storage, int side, int n) {
    kongyuan_slot(storage, kPoemsPublishBase + side) = n;
}

inline int kongyuan_pub_poems(const std::map<int, std::any>& storage, int side) {
    auto it = storage.find(kPoemsPublishBase + side);
    return it == storage.end() ? 0 : std::any_cast<int>(it->second);
}

// 技能侧发命令 → 魂印侧消费（消费后清 0）
inline void kongyuan_cmd_poems_target(std::map<int, std::any>& storage, int side, int n) {
    int& v = kongyuan_slot(storage, kPoemsTargetBase + side);
    v = v > n ? v : n;   // 同回合多条 → 取最高目标
}

inline void kongyuan_cmd_poems_add(std::map<int, std::any>& storage, int side, int n) {
    kongyuan_slot(storage, kPoemsAddBase + side) += n;
}

}  // namespace kongyuan

#endif  // KONGYUAN_RUNTIME_H
