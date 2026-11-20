#ifndef WUJI_SHENGWU_RUNTIME_H
#define WUJI_SHENGWU_RUNTIME_H

// ══════════════════════════════════════════════════════════════════════════
// 无极圣武 4800 跨 dylib 发布槽（魂印 ↔ 将来的技能批/亮节降零层共用；键段 4800xxx）。
// 生命周期遵循 elf-pet.h 的两槽约定（选错槽 = 该清的没清/该留的丢了，静默错）：
//   英雄之耀 → **soulmark_storage**（官方明写"印记下场保留"）；
//   亮节/威怯/武诛 → **on_stage_storage**（官方逐条"下场不保留"——换宠由引擎清空）。
// 回合模型：绝对失效回合（end），active = roundCount < end（与异常槽 has_active 同口径）。
// ══════════════════════════════════════════════════════════════════════════

#include <any>
#include <map>

// ⚠️ 注册键 = effect_icon.effect_id = 2436（装配层 SoulMark.id 按 effect_id 查程序表）。
//    4800 是 monster id，只用于命名与注释——错键会让程序在真实对局挂不上（2026-09-27 修复）。
//    存储键由本常量派生（2436xx 段），随常量自动迁移，魂印/技能/测试三方同源。
constexpr int kWujiMarkId = 2436;

constexpr int kWujiYaoltKey = kWujiMarkId * 100 + 1;     // 英雄之耀层数（int，soulmark_storage）
constexpr int kWujiLiangjieKey = kWujiMarkId * 100 + 2;  // 亮节失效回合（int，on_stage_storage）
constexpr int kWujiWeiqieKey = kWujiMarkId * 100 + 3;    // 威怯失效回合（int）
constexpr int kWujiWuzhuKey = kWujiMarkId * 100 + 4;     // 武诛失效回合（int；剩余回合数 = end - roundCount）

// J332（2026-11-06）：「英雄之耀达到3层时此技能不受PP值限制」per-skill IgnorePP
// 伪键（Skills::usabilityEffects 条目键；叵 kPoPpUsabilityId=魂印id*10 同段式）。
// 生产端 = wuji_shengwu_4800.cpp ROUND_START early 节点（层数≥3 → 三载体技
// 38086/29149/29150 置 IgnorePP active=true，<3 → active=false）；消费端 =
// skills.cpp skill_usable/query_selectable 的 PP=0 门。独立命名段（2436*10+1），
// 与任何 effect_id 语义无关（usabilityEffects 条目键只在本技向量内匹配）。
constexpr int kJ332PpFreeUsabilityId = kWujiMarkId * 10 + 1;   // = 24361

// 印记是否生效
inline bool wuji_mark_active(const std::map<int, std::any>& storage, int key, int round_count) {
    auto it = storage.find(key);
    return it != storage.end() && round_count < std::any_cast<int>(it->second);
}

// 授予/刷新印记（取 max：后到的短窗口不覆盖长窗口）
inline void wuji_mark_grant(std::map<int, std::any>& storage, int key, int end_round) {
    auto it = storage.find(key);
    if (it == storage.end() || end_round > std::any_cast<int>(it->second)) {
        storage[key] = end_round;
    }
}

#endif  // WUJI_SHENGWU_RUNTIME_H
