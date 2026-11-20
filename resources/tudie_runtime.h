#ifndef TUDIE_RUNTIME_H
#define TUDIE_RUNTIME_H

// 涂蝶 4861 —— 魂印侧（soul_lib/tudie_4861.cpp）与技能侧（未来的染色效果
// 2391「将{0}幕的颜色改为{1}」/ 2392 虚之彩召唤）共享的 **plugin_storage 键约定**。
//
// 为什么需要：幕是**场座位**状态（双方各一个槽），不是宠物身上的东西——宿主上下场、
// 阵亡都不清幕（官方文本无清除子句）；且魂印与技能在两个 RTLD_LOCAL dylib 里，
// 只能经 `ctx->plugin_storage` 的 **int 槽**通信（std::any 存自定义类型跨 dylib
// typeinfo 不统一必炸——qiling/kongyuan/tianqi 同款纪律）。
//
// ⚠️ 新增键先 grep 全库 plugin_storage 键段（4861xxxx 段无既有占用；bool 与 int
//    分键段存放——「armed_flag bool 撞 int」坑已发生 2 次）。

#include <any>
#include <map>
#include <string>

namespace tudie {

// ── 颜色枚举（引擎内部口径）──
// DB effect 2391 的官方颜色编号未完整提取；语料 idx=121 实证官方颜色 = 黑红黄蓝
// 四色。白 = 无颜色（「没有任何颜色的幕」判定的分界线：<=kWhite 即无色）。
constexpr int kWuMu   = -1;  // 无幕（键不存在）
constexpr int kWhite  = 0;   // 白幕（无颜色）
constexpr int kRed    = 1;   // 赤/红
constexpr int kYellow = 2;   // 黄
constexpr int kBlue   = 3;   // 蓝
constexpr int kBlack  = 4;   // 黑

// 幕颜色槽：kCurtainColorBase + side（int）。
constexpr int kCurtainColorBase = 48610000;
// 最近一次「颜色改变」发生的回合（kNoChange = 本场尚无）：「任意一方幕的颜色改变时，
// **当回合**结束后…」的判定口。故意不用消耗式旗标——双方镜像对局时两个 holder
// 都要在同一回合各自结算回复，消耗式会漏掉后结算的一方。
constexpr int kChangeRoundKey = 48610100;
constexpr int kNoChange = -1;
// 本场战斗出现过的颜色位图：kCurtainSeenBase + side（bit = 1<<颜色枚举）。
// 效果 2392「若本场战斗己方场上分别出现过红幕、黄幕、蓝幕…」的查询口（虚之彩线备用）。
constexpr int kCurtainSeenBase = 48610200;
// 登场 watcher 只挂一次旗：kArmedFlagBase + owner（**bool**，独立键段防撞型）。
constexpr int kArmedFlagBase = 48610300;

// 读幕颜色（键不存在 = 无幕）。
inline int curtain_color(const std::map<int, std::any>& storage, int side) {
    const auto it = storage.find(kCurtainColorBase + side);
    return it == storage.end() ? kWuMu : std::any_cast<int>(it->second);
}

// 「出现过」位图读 / 写。
inline int seen_colors(const std::map<int, std::any>& storage, int side) {
    const auto it = storage.find(kCurtainSeenBase + side);
    return it == storage.end() ? 0 : std::any_cast<int>(it->second);
}

// 最近一次颜色改变的回合（kNoChange = 本场尚无）。
inline int change_round(const std::map<int, std::any>& storage) {
    const auto it = storage.find(kChangeRoundKey);
    return it == storage.end() ? kNoChange : std::any_cast<int>(it->second);
}

// 技能名颜色字符 → 颜色枚举（UTF-8 多字节子串查找；无颜色字符返回 kWhite）。
// 颜色集合 = 语料 idx=121 实证的官方四色：赤/红、黄、蓝、黑。
inline int name_color(const std::string& name) {
    if (name.find("赤") != std::string::npos || name.find("红") != std::string::npos) {
        return kRed;
    }
    if (name.find("黄") != std::string::npos) {
        return kYellow;
    }
    if (name.find("蓝") != std::string::npos) {
        return kBlue;
    }
    if (name.find("黑") != std::string::npos) {
        return kBlack;
    }
    return kWhite;
}

// 染色统一口（魂印的战斗开始染色、白幕授予与将来的技能效果 2391 都走这里）：
//   ① 写颜色槽 + 真颜色记入「出现过」位图；
//   ② 颜色值变化且**涉及真颜色**（无幕→白幕的初始授予不算改变）→ 记变色回合。
// 返回是否发生「颜色改变」（回合末回复子句的触发条件）。
inline bool dye_curtain(std::map<int, std::any>& storage, int side, int color, int round) {
    const int old = curtain_color(storage, side);
    storage[kCurtainColorBase + side] = color;
    if (color > kWhite) {
        storage[kCurtainSeenBase + side] = seen_colors(storage, side) | (1 << color);
    }
    if (old != color && (old > kWhite || color > kWhite)) {
        storage[kChangeRoundKey] = round;
        return true;
    }
    return false;
}

// 白幕授予（「为己方/对方添加白幕」）：无幕→白幕**不算**颜色改变（初始授予），
// 已是白幕幂等。有真颜色的幕不会被本函数覆盖——调用方先查 `curtain_color() < 1`。
inline void grant_white(std::map<int, std::any>& storage, int side) {
    dye_curtain(storage, side, kWhite, kNoChange);
}

}  // namespace tudie

#endif  // TUDIE_RUNTIME_H
