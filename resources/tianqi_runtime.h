#ifndef TIANQI_RUNTIME_H
#define TIANQI_RUNTIME_H

// 天启星魂 4677 星盘运行时 —— 魂印侧（soul_lib/tianqi_xinghun_4677.cpp）与技能侧
// （moves_lib/tianqi_skills.cpp）共享的 plugin_storage 载荷 + 键约定。
// 放独立头：场景测试 089 也要按同一类型 any_cast 读盘面（同名匿名结构体是两个类型，
// any 按类型存取，镜像定义会 bad_any_cast）。
#include <any>
#include <map>

struct TianqiRuntime {
    int dial = 0;             // 星盘刻度（0..12）
    int shield_round = -1;    // 转盘后下一回合（roundCount 值）：受伤-99% 生效回合
    int re_star_at = -1;      // 同回合收尾：PP归零 + 再次星启
    bool returning = false;   // 回返期间（到过 12、未回 0）——2208/2209 的翻倍档 + 免疫/额外行动开关
    bool pink_registered = false;  // 粉伤 -99% 管线条目只注册一次
    int watcher_id = 0;       // 到期事件监听句柄（0 = 未挂）
    int atk_immune_handle = 0;  // 满盘/回返期"攻击无效"封属票（0 = 未授；可被 699 族穿透）
    int last_turn_round = -1; // 本回合已转过盘（官方：一回合最多转一次，多个异常同回合结束只算一次）
};

// plugin_storage 键：kTianqiStorageKeyBase * 100 + side（每方一份，镜像对战互不串）。
constexpr int kTianqiStorageKeyBase = 4677;

// 便捷访问（魂印侧用可写引用；技能/测试侧用只读指针，缺 key = 星盘未启动）。
inline TianqiRuntime& tianqi_rt(std::map<int, std::any>& storage, int side) {
    auto it = storage.find(kTianqiStorageKeyBase * 100 + side);
    if (it == storage.end()) {
        it = storage.emplace(kTianqiStorageKeyBase * 100 + side, TianqiRuntime{}).first;
    }
    return std::any_cast<TianqiRuntime&>(it->second);
}

inline const TianqiRuntime* tianqi_rt_find(const std::map<int, std::any>& storage, int side) {
    auto it = storage.find(kTianqiStorageKeyBase * 100 + side);
    return it == storage.end() ? nullptr : std::any_cast<const TianqiRuntime*>(&it->second) != nullptr
                                            ? *std::any_cast<const TianqiRuntime*>(&it->second)
                                            : nullptr;
}

// ── 跨 dylib 发布槽（moves_lib / 测试 只读）───────────────────────
// ⚠️ std::any 存自定义类型时，typeinfo 在两个 RTLD_LOCAL 插件 dylib 之间不统一
//   （test↔插件 经主程序全局符号表可统一，soul↔moves 不行）→ any_cast 必失败。
//   发布一律用 **int**（fundamental type 的 typeinfo 在 libc++ 内全局唯一）。
// dial 键 4677000+side、returning 键 4677100+side（返回 0/1）。结构体本体
// （shield_round 等票柄）只在 soul_lib 内部使用，不走发布槽。
constexpr int kTianqiPubDialBase = 4677000;
constexpr int kTianqiPubReturningBase = 4677100;

inline void tianqi_publish(std::map<int, std::any>& storage, int side, int dial, bool returning) {
    storage[kTianqiPubDialBase + side] = dial;
    storage[kTianqiPubReturningBase + side] = returning ? 1 : 0;
}

inline int tianqi_pub_dial(const std::map<int, std::any>& storage, int side) {
    auto it = storage.find(kTianqiPubDialBase + side);
    return it == storage.end() ? 0 : std::any_cast<int>(it->second);
}

inline bool tianqi_pub_returning(const std::map<int, std::any>& storage, int side) {
    auto it = storage.find(kTianqiPubReturningBase + side);
    return it != storage.end() && std::any_cast<int>(it->second) != 0;
}

#endif  // TIANQI_RUNTIME_H
