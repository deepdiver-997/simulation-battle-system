#ifndef MONIKA_RUNTIME_H
#define MONIKA_RUNTIME_H

// 圣光莫妮卡（神谕 3711）运行时 —— 魂印/技能共享 int 发布槽（跨 dylib 纪律同 oracle）。
// 口径（effect_icon 2072 / effect 2492，boss 有效 + 语料 idx=94）：
//   宁界之律 = 自身 0..5；出战 +4；回合末分支消耗；不受 PP 限制 gate。
//   琼华之庇 = side 级计数（官方绑"恢复成功 PP 的精灵"，per-pet 拆分待实测），0..4。
//   最优系别 = 到期回合号（roundCount < until 时伤害快照取候选最优）。
//   自然祝福 = **未神谕旧资源**（effect_icon 963 / effect 1287），0..4、下场清零——
//     旧技能族效果 1382/1383/1384/1385 专用。**与宁界之律互不相通**（用户 2026-09-25
//     口径：旧自然祝福不能和新的律混用，旧技能效果字面操作自然祝福）。
constexpr int kMoniPubLv = 4693000;      // + side
constexpr int kMoniPubBi = 4693100;      // + side
constexpr int kMoniPubOptUntil = 4693200;// + side：最优系别视图生效至该回合（含）
constexpr int kMoniPubNz = 4693300;      // + side：自然祝福（旧资源，0..4）

inline int moni_pub_get(const std::map<int, std::any>& store, int key) {
    auto it = store.find(key);
    if (it == store.end()) {
        return 0;
    }
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : 0;
}

inline void moni_pub_set(std::map<int, std::any>& store, int key, int value) {
    store[key] = value;
}

#endif // MONIKA_RUNTIME_H
