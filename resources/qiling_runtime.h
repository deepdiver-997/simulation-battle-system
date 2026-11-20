#ifndef QILING_RUNTIME_H
#define QILING_RUNTIME_H

// 启灵元神 3986 —— 魂印侧（soul_lib/qiling_1581.cpp）与技能侧
// （moves_lib/qiling_skills.cpp）共享的 **plugin_storage 键约定**。
//
// 为什么需要：神殇的"放逐"状态要被第五技能 33894 的效果 1534 读取
// （「令对手随机 1 个**被放逐的精灵**所有技能 PP 值 -1」），而魂印与技能在两个
// RTLD_LOCAL dylib 里，只能经 `ctx->plugin_storage` 的 **int 槽**通信
// （std::any 存自定义类型跨 dylib typeinfo 不统一必炸——kongyuan/tianqi 同款坑）。
//
// ⚠️ 新增键先 grep 全库 plugin_storage 键段（15810000 起 8 位段，无既有占用）。
#include <map>

namespace qiling {

// 被放逐位图：bit(slot) = 该槽位精灵本轮被神殇放逐（不能主动切换上场）。
// 魂印侧每回合 ROUND_START 重掷并整字重写；技能侧（1534）只读。
// ⚠️ 键段 15820000：与 qiling_1581.cpp 的 armed_flag（15810001 + owner*10 + salt）
//    错开——首版取 15810000 时 side=1 的位图键 15810001 与 armed_flag(owner0,salt0)
//    同键，bool 撞 int → bad_any_cast（交接文档「plugin_storage 键段会撞」坑再现）。
constexpr int kExilePublishBase = 15820000;   // + side (0/1)

inline int exile_mask(const std::map<int, std::any>& storage, int side) {
    const auto it = storage.find(kExilePublishBase + side);
    return it == storage.end() ? 0 : std::any_cast<int>(it->second);
}

inline void publish_exile_mask(std::map<int, std::any>& storage, int side, int mask) {
    storage[kExilePublishBase + side] = mask;
}

// ── 神印层数发布槽（技能侧 1532/1536 读）──
// 神印层数本体存在**受害者** pet 的 soulmark_storage[1581]（ShenyinStorage，
// soul_lib/qiling_1581.cpp 私有结构）——moves_lib 直接 any_cast 必炸（跨 dylib
// typeinfo 不统一）。魂印侧在对方每次切换登场时（EVENT_SWAP watcher，挂层路径）
// 发布"刚登场那只"的当前层数；键 = base + **受害者 side**。
// 语义：值 = 该 side 当前**在场**精灵身上的神印层数（0 = 不存在神印）——
// 1532「对手不存在神印/每存在1层」、1536「对手存在神印时翻倍」都按它判。
// ⚠️ 键段 15830000：与 armed_flag（15810001+owner*10+salt）、放逐位图（15820000+side）
//    错开（撞键坑已两次，见上）。
constexpr int kShenyinStacksPublishBase = 15830000;   // + side (0/1)

inline int shenyin_stacks(const std::map<int, std::any>& storage, int side) {
    const auto it = storage.find(kShenyinStacksPublishBase + side);
    return it == storage.end() ? 0 : std::any_cast<int>(it->second);
}

inline void publish_shenyin_stacks(std::map<int, std::any>& storage, int side, int stacks) {
    storage[kShenyinStacksPublishBase + side] = stacks;
}

}  // namespace qiling

#endif  // QILING_RUNTIME_H
