#ifndef SUIT_REG_H
#define SUIT_REG_H

// ════════════════════════════════════════════════════════════════════
// 套装插件注册面（2026-09-19 套装线）。
//
// 与 plugin_interface.h 的 plugin_reg 同构，但**刻意独立**：
//   ① SuitManager 不是 IEffectRegistry —— 套装有独立的注册 API（本头文件的
//      SuitPluginApi），插件经独立入口符号 suit_plugin_register 收到它。
//      不往 IEffectRegistry 加虚函数（那会牵动 moves_lib/soul_lib 两个既有注册面）。
//   ② 节点模型直接复用 SoulMarkNodeRef（时点 + 函数 + once/early/scope）——
//      套装效果的"多时点程序"与魂印程序同构，区别只在生命周期：套装效果
//      一律 TEAM（全队、整场、换宠/断回合/抑制都不作废，用户 2026-09-19 口径）。
//
// 用法（插件 .cpp）：
//     SUIT_PROGRAM(462, {SoulMarkNodeRef(State::BATTLE_ROUND_START, &fn)});
//     extern "C" void suit_plugin_register(void* api) { suit_reg::flush(api); }
// ════════════════════════════════════════════════════════════════════

#include <string>
#include <vector>

#include <plugin/core_api.h>
#include <plugin/id_collision.h>

// 可见性：套装注册表必须**每 dylib 一份**——同 plugin_interface.h 的说明
// （默认可见性下头文件 inline 的静态量是 weak 符号 → 动态链接器做符号插入 →
// 后加载的 dylib 读到先加载 dylib 的表）。见 plugin_interface.h 的详细注释。
#if defined(__GNUC__) || defined(__clang__)
#define SBS_SUIT_HIDDEN_VIS_BEGIN _Pragma("GCC visibility push(hidden)")
#define SBS_SUIT_HIDDEN_VIS_END _Pragma("GCC visibility pop")
#else
#define SBS_SUIT_HIDDEN_VIS_BEGIN
#define SBS_SUIT_HIDDEN_VIS_END
#endif

SBS_SUIT_HIDDEN_VIS_BEGIN
namespace suit_reg {

struct SuitEntry {
    int id;
    EffectFn fn;
    std::string source;   // 注册来源（"文件:行 [宏名]"）——撞号审计用
};
struct SuitProgramEntry {
    int id;
    std::vector<SoulMarkNodeRef> nodes;
    std::string source;   // 注册来源——撞号审计用
};

// 函数内静态：每 dylib 一份（inline 函数的函数内静态在各自 .so/.dylib 里独立实例，
// 与 plugin_reg 同款语义——插件之间不互相链接）。
inline std::vector<SuitEntry>& suits() {
    static std::vector<SuitEntry> table;
    return table;
}
inline std::vector<SuitProgramEntry>& suit_programs() {
    static std::vector<SuitProgramEntry> table;
    return table;
}

inline bool add_suit(int id, EffectFn fn, const std::string& source = {}) {
    suits().push_back(SuitEntry{id, fn, source});
    return true;
}
inline bool add_suit_program(int id, std::vector<SoulMarkNodeRef> nodes,
                             const std::string& source = {}) {
    suit_programs().push_back(SuitProgramEntry{id, std::move(nodes), source});
    return true;
}

// core → 插件 的初始化载荷：core 原语指针 + 套装注册口（本管理器的静态跳板）。
// 与 PluginInitApi 平行，不共享基类——套装插件的入口符号独立（kSuitPluginInitFn），
// 装错目录的 dylib 不会被误注册（找不到符号就只是被跳过）。
struct SuitPluginApi {
    const CoreApi* core;
    void (*register_suit)(int suit_id, EffectFn fn, const std::string& source);
    void (*register_suit_program)(int suit_id, const std::vector<SoulMarkNodeRef>& nodes,
                                  const std::string& source);
};

// 插件入口符号（SuitManager 在 dlopen 后 dlsym 它）。
constexpr const char* kSuitPluginInitFn = "suit_plugin_register";

// 插件侧标准 flush：把本 dylib 的两张表推给 core 传入的 API。
inline void flush(void* api) {
    SuitPluginApi* init = static_cast<SuitPluginApi*>(api);
    if (!init) {
        return;
    }
    for (const SuitEntry& e : suits()) {
        init->register_suit(e.id, e.fn, e.source);
    }
    for (const SuitProgramEntry& e : suit_programs()) {
        init->register_suit_program(e.id, e.nodes, e.source);
    }
}

}  // namespace suit_reg
SBS_SUIT_HIDDEN_VIS_END

// 静态自注册宏（语义同 plugin_interface.h 的 SKILL_EFFECT/SOUL_PROGRAM）：
// dlopen 静态初始化期入表，suit_plugin_register 时一次推送。表由插件推、id 留编译期。
#define SUIT(id, fn)                                                                    \
    namespace {                                                                         \
    [[maybe_unused]] const bool kSuitReg_##fn = ::suit_reg::add_suit((id), &(fn),       \
        ::id_collision::source_of(__FILE__, __LINE__, "SUIT"));                         \
    }
#define SUIT_PROGRAM(id, ...)                                                           \
    namespace {                                                                         \
    [[maybe_unused]] const bool kSuitProgramReg_##id =                                  \
        ::suit_reg::add_suit_program((id), std::vector<SoulMarkNodeRef>{__VA_ARGS__},    \
            ::id_collision::source_of(__FILE__, __LINE__, "SUIT_PROGRAM"));             \
    }

#endif  // SUIT_REG_H
