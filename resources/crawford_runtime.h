#ifndef CRAWFORD_RUNTIME_H
#define CRAWFORD_RUNTIME_H

// 克雷弗德 4644 运行时 —— 魂印侧（soul_lib/crawford_deleermo_4644.cpp）与技能侧
// （moves_lib/crawford_skills.cpp）共享的存储布局 + 键约定。
// 放独立头：场景测试 090 也要按同一类型 any_cast 读盘面（同名匿名结构体是两个类型，
// any 按类型存取，镜像定义会 bad_any_cast）。
//
// 存储布局（2026-09-20 迁移定稿）：
//   ★ **构造位图 + 登场期状态** → `ElfPet::on_stage_storage`（"本次上场"槽：换宠由引擎
//     清空 = "构造数下场重置"天然成立，无需手动复位）。
//     - kCrawfordOnStageConstructsKey → **int** 位图（bit0=I..bit3=IV）：moves 侧点亮、
//       soul 侧读——跨 dylib 只走 fundamental type（int 的 typeinfo 全局唯一）。
//     - kCrawfordOnStageRuntimeKey → CrawfordRuntime（**仅 soul 侧 any_cast**；
//       两个 RTLD_LOCAL 插件之间自定义类型 typeinfo 不统一，互读必 bad_any_cast）。
//   ★ 跨战斗持久的监听句柄 → `ElfPet::soulmark_storage`（下场保留槽）。
//   ★ 技能侧窗口状态（CrawfordMovesState）→ 仍走 plugin_storage（moves 侧独占）。
//   ★ 协议II·限的"对手下次攻击 -50%" 待响应旗 → plugin_storage int 槽（跨 dylib）。
//     **绑定克雷自身**（2026-09-20 口径）：主动切换/阵亡离场即撤销（soul 侧
//     EVENT_SWAP / EVENT_DEATH 监听 revoke_opp_reduce），再上场不恢复；
//     已 pending 时重复获得（超越式复读）只覆盖不叠加（moves 侧幂等）。
//   ★ 不需要 CoreApi 开槽：on_stage_storage / soulmark_storage 是 ElfPet 公有成员，
//     插件经头文件直取（canglan soulmark_storage / heal_drain on_stage_storage 同款先例）。
#include <any>
#include <map>

// ════════════════════════════ 魂印侧（soul_lib 独占）════════════════════════════
// 德莱赫默伴生运行时：守备/激发回合快照、PP 恢复、视图钳制快照。
// 生命周期 = "本次上场"（on_stage_storage）：换宠/阵亡下场由引擎清空，重开即全新。
struct CrawfordRuntime {
    bool excited_snapshot = false;  // 回合开始时快照：true=激发（≥2 构造）/ false=守备
    int snapshot_count = 0;         // 回合开始时的构造数（守备减伤按 4-count 叠加）
    bool pp_restore_pending = false;  // 守备回合：回合结束恢复所有攻击技能 1 PP
    bool view_clamped = false;  // 超越式(≥2 构造)本回合已钳制视图（AFTER_ACTION 恢复用）
    int clamped_self[6]{};      // 钳制前的自身视图等级（六槽快照）
    int clamped_opp[6]{};       // 钳制前的对手视图等级
};

// ════════════════════════════ 技能侧（moves_lib 独占）═════════════════════════
// 各持续窗口技能（1400/1475/1600）的窗口端点与一次性注册守卫。
// 窗口为回合制（"N回合内"），**跨在场保留** → 放 plugin_storage 而不是 on_stage_storage。
struct CrawfordMovesState {
    int break_watch_until = -1;      // effect 1400：回合类效果被消除→麻痹 的窗口末回合
    bool break_watch_up = false;     // effect 1400：监听器只挂一次旗
    bool pink_1475_up = false;       // effect 1475：粉伤减伤条目只注册一次旗
    int pink_1475_until = -1;        // effect 1475：固伤百分比-60% 的窗口末回合
    int swap_watch_until = -1;       // effect 1600：对手主动切换→登场者麻痹 的窗口末回合
    bool swap_watch_up = false;      // effect 1600：监听器只挂一次旗
    int tune_1574_pct = 10;          // effect 1574：附加粉伤比例（每次使用 +15，封顶 40）
};

// ═══════════════════════════════ 键约定 ═══════════════════════════════════
// ElfPet::on_stage_storage（本次上场）：
constexpr int kCrawfordOnStageRuntimeKey = 4644;     // CrawfordRuntime（soul any_cast）
constexpr int kCrawfordOnStageConstructsKey = 4645;  // int 构造位图（跨 dylib）
// ElfPet::soulmark_storage（下场保留）：
constexpr int kCrawfordSoulmarkWatcherKey = 4646;    // EVENT_TAKE_DAMAGE 监听句柄（int）
// plugin_storage（跨 dylib int 槽 + moves 独占结构体）：
constexpr int kCrawfordMovesKeyBase = 4645;          // CrawfordMovesState 键 = *100 + side
constexpr int kCrawfordPubOppReduceBase = 4644100;   // +side：限的"对手下次攻击-50%"待响应
                                                     // +100+side：写进的减伤桶槽位（撤除用）

// ── "-50%"消费监听句柄（soulmark_storage，int——跨在场保留）──
inline int crawford_take_damage_watcher(const ElfPet& pet) {
    auto it = pet.soulmark_storage.find(kCrawfordSoulmarkWatcherKey);
    return it == pet.soulmark_storage.end() ? 0 : std::any_cast<int>(it->second);
}

inline void crawford_set_take_damage_watcher(ElfPet& pet, int id) {
    pet.soulmark_storage[kCrawfordSoulmarkWatcherKey] = id;
}

// ── 构造位图（on_stage_storage，int——跨 dylib 读写）──
inline int crawford_constructs(const ElfPet& pet) {
    auto it = pet.on_stage_storage.find(kCrawfordOnStageConstructsKey);
    return it == pet.on_stage_storage.end() ? 0 : std::any_cast<int>(it->second);
}

inline void crawford_set_constructs(ElfPet& pet, int bitfield) {
    pet.on_stage_storage[kCrawfordOnStageConstructsKey] = bitfield;
}

// ── 魂印侧登场期运行时（on_stage_storage，soul 独占 any_cast）──
inline CrawfordRuntime& crawford_rt(ElfPet& pet) {
    auto it = pet.on_stage_storage.find(kCrawfordOnStageRuntimeKey);
    if (it == pet.on_stage_storage.end()) {
        it = pet.on_stage_storage.emplace(kCrawfordOnStageRuntimeKey, CrawfordRuntime{}).first;
    }
    return std::any_cast<CrawfordRuntime&>(it->second);
}

// ── 技能侧窗口状态（plugin_storage，moves 独占 any_cast；键 per side 镜像隔离）──
inline CrawfordMovesState& crawford_moves_rt(std::map<int, std::any>& storage, int side) {
    auto it = storage.find(kCrawfordMovesKeyBase * 100 + side);
    if (it == storage.end()) {
        it = storage.emplace(kCrawfordMovesKeyBase * 100 + side, CrawfordMovesState{}).first;
    }
    return std::any_cast<CrawfordMovesState&>(it->second);
}

// ── "对手下次攻击 -50%" 待响应旗（plugin_storage int 槽；故意不随下场清）──
inline void crawford_pub_opp_reduce(std::map<int, std::any>& storage, int side, bool pending) {
    storage[kCrawfordPubOppReduceBase + side] = pending ? 1 : 0;
}

inline bool crawford_pub_opp_reduce(const std::map<int, std::any>& storage, int side) {
    auto it = storage.find(kCrawfordPubOppReduceBase + side);
    return it != storage.end() && std::any_cast<int>(it->second) != 0;
}

inline int crawford_count_constructs(int bitfield) {
    int n = 0;
    while (bitfield) {
        n += bitfield & 1;
        bitfield >>= 1;
    }
    return n;
}

#endif  // CRAWFORD_RUNTIME_H
