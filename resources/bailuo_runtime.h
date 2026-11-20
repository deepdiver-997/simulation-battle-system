#ifndef BAILUO_RUNTIME_H
#define BAILUO_RUNTIME_H

// 百罗鬼帝 4444（魂印 2085）—— 魂印侧（soul_lib/bailuo_guidi_4444.cpp）与技能侧
// （moves_lib/bailuo_skills.cpp 的 2473）共享的 **plugin_storage int 槽约定**。
//
// 为什么走 int 槽：鬼祰的**权威状态**必须同时被两个 dylib 读写——
//   · 魂印侧：登场时的"按第五 PP 获得/失去"批量增删；
//   · 技能侧 2473（尸走魂嚎）：「获得 1 种自身没有的鬼祰」。
// std::any 存自定义类型跨 dylib typeinfo 不统一必炸（kongyuan/tianqi/qiling 同款坑），
// 故权威状态 = 几个 int 槽 + 本头文件的 inline 助手（两侧共用同一份逻辑）。
//
// ⚠️ 键段 44440000：新增前已 `grep -rn plugin_storage resources/ src/` 扫键段，
//    4444xxxx 无既有占用（交接文档「plugin_storage 键段会撞」第 2 次的教训）。
// ⚠️ 口径假设：**每边至多一只百罗鬼帝**（引擎魂印唯一性惯例）——槽按 side 索引，
//    不按槽位。
//
// 鬼祰口径（语料 idx=73《机制解析—百罗鬼帝》+ effect_icon 2085 tips）：
//   · 四种：魑/魅/魍/魉（bit0..3）；"四个鬼祰基本上都是默认2回合的效果"
//     → 每种持有 = 一个 2 回合窗口（获得时刷新），过期判定用 RuleCenter::window_expired
//     同式：roundCount - 获得回合 >= 2 即过期（懒清理，读时过滤+回收槽）。
//   · 获得顺序 = 魑魅魍魉（官方未指明，取tips行文序）；失去顺序 = 反序（同上，取对称）。
//   · 上限 = 4 种（"获得1种自身没有的"隐含种类集合语义）。
#include <fsm/battleContext.h>

namespace bailuo {

// 持有种类位图：bit0=魑 bit1=魅 bit2=魍 bit3=魉（+ side → 每方一个）。
constexpr int kGuixiaoHeldBase = 44440000;
// 每种一个过期回合槽：+ side*4 + kind → 绝对过期回合（held ⇔ roundCount < 该值）。
constexpr int kGuixiaoExpireBase = 44441000;
constexpr int kGuixiaoKindCount = 4;
// 鬼祰持有窗口（"四个鬼祰基本上都是默认2回合的效果"，语料 idx=73）。
constexpr int kGuixiaoWindowRounds = 2;

constexpr const char* kGuixiaoKindName[4] = {"魑", "魅", "魍", "魉"};

inline int held_key(int side) { return kGuixiaoHeldBase + side; }
inline int expire_key(int side, int kind) { return kGuixiaoExpireBase + side * 4 + kind; }

inline int get_int(const std::map<int, std::any>& storage, int key) {
    const auto it = storage.find(key);
    return it == storage.end() ? 0 : std::any_cast<int>(it->second);
}

// 过期种类回收 + 读当前持有位图（懒清理：读到过期 → 清位 + 删过期槽）。
inline int held_mask(BattleContext* ctx, int side) {
    int mask = get_int(ctx->plugin_storage, held_key(side));
    for (int kind = 0; kind < kGuixiaoKindCount; ++kind) {
        if ((mask & (1 << kind)) == 0) {
            continue;
        }
        const int expire = get_int(ctx->plugin_storage, expire_key(side, kind));
        if (ctx->roundCount - expire >= 0) {
            // roundCount >= expire：窗口结束（获得回合 = expire - kGuixiaoWindowRounds）
            mask &= ~(1 << kind);
            ctx->plugin_storage.erase(expire_key(side, kind));
        }
    }
    if (mask != get_int(ctx->plugin_storage, held_key(side))) {
        ctx->plugin_storage[held_key(side)] = mask;
    }
    return mask;
}

inline int kind_count(BattleContext* ctx, int side) {
    const int mask = held_mask(ctx, side);
    return ((mask & 1) != 0) + ((mask & 2) != 0) + ((mask & 4) != 0) + ((mask & 8) != 0);
}

// 持有一种鬼祰（获得时刷新窗口）。kind 越界忽略；已持有只刷新过期。
// 返回 true = 本次调用后该种处于持有状态。
inline bool hold_kind(BattleContext* ctx, int side, int kind) {
    if (kind < 0 || kind >= kGuixiaoKindCount) {
        return false;
    }
    ctx->plugin_storage[expire_key(side, kind)] = ctx->roundCount + kGuixiaoWindowRounds;
    ctx->plugin_storage[held_key(side)] = held_mask(ctx, side) | (1 << kind);
    return true;
}

// 按魑魅魍魉序获得 n 种（"自身第五技能每有1点PP自身获得1种鬼祰"）：
// 未持有的按序补，全持有后继续调用 → 刷新最早过期的（等价"获得1种"落在已持有上）。
inline void gain_kinds(BattleContext* ctx, int side, int n) {
    for (int i = 0; i < n; ++i) {
        const int mask = held_mask(ctx, side);
        int chosen = -1;
        for (int kind = 0; kind < kGuixiaoKindCount; ++kind) {
            if ((mask & (1 << kind)) == 0) {
                chosen = kind;
                break;
            }
        }
        if (chosen < 0) {
            // 已集齐：找过期回合最早的刷新（保持"获得"有可观察效果）
            chosen = 0;
            int oldest = get_int(ctx->plugin_storage, expire_key(side, 0));
            for (int kind = 1; kind < kGuixiaoKindCount; ++kind) {
                const int e = get_int(ctx->plugin_storage, expire_key(side, kind));
                if (e < oldest) {
                    oldest = e;
                    chosen = kind;
                }
            }
        }
        hold_kind(ctx, side, chosen);
    }
}

// 按魉魍魅魑（反序）失去 n 种（"对手第五技能每有1点PP值自身失去1种鬼祰"）；
// 超出持有数的部分忽略（钳 0）。
inline void lose_kinds(BattleContext* ctx, int side, int n) {
    for (int i = 0; i < n; ++i) {
        const int mask = held_mask(ctx, side);
        int chosen = -1;
        for (int kind = kGuixiaoKindCount - 1; kind >= 0; --kind) {
            if ((mask & (1 << kind)) != 0) {
                chosen = kind;
                break;
            }
        }
        if (chosen < 0) {
            return;   // 已空
        }
        int mask2 = held_mask(ctx, side);
        mask2 &= ~(1 << chosen);
        ctx->plugin_storage[held_key(side)] = mask2;
        ctx->plugin_storage.erase(expire_key(side, chosen));
    }
}

// 「获得1种自身没有的鬼祰」（2473）：按魑魅魍魉序取第一种未持有的；全持有返回 false。
inline bool acquire_kind_not_held(BattleContext* ctx, int side) {
    const int mask = held_mask(ctx, side);
    for (int kind = 0; kind < kGuixiaoKindCount; ++kind) {
        if ((mask & (1 << kind)) == 0) {
            hold_kind(ctx, side, kind);
            return true;
        }
    }
    return false;
}

}  // namespace bailuo

#endif  // BAILUO_RUNTIME_H
