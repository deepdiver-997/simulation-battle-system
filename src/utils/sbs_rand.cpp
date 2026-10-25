// 引擎自有确定性 RNG 实现（见 include/utils/sbs_rand.h 与决策 D13/D15）。
//
// 历史：M1 先用 -Wl,--wrap 链接期改道（彼时符号名 __wrap_rand/__wrap_srand）达成 Mac 序列
// 复现；D15 起改源码级替换（sed 全量迁移，符号即本文件的 sbs_rand/sbs_srand），对任意
// 工具链（含 MSVC）成立，wrap 机制退役。SHARED 库形态保留：宿主 exe 与插件 DLL 必须共享
// 单一状态（对齐 macOS 单一 libc rand 语义；若各自持有状态，插件内 roll 与宿主内 roll
// 会走在两条序列上，场景即红）。
//
// 算法：MINSTD（a=16807, m=2^31-1），`state = state*16807 mod m` 直接返回；srand(0) 与撞 0
// 时存储侧重置为 123459876（当次调用仍返回 0）。2026-10-25 于 macOS libc 实证拟合：
// 10 种子 × 1000 值全等（含 seed=0、seed=2^31-1 边角）。
#include "utils/sbs_rand.h"

#include <cstdint>

namespace {

constexpr std::uint64_t kMod = 2147483647ULL;      // 2^31-1
constexpr std::uint64_t kZeroFix = 123459876ULL;   // srand(0)/撞 0 的重置常数（macOS libc 实证拟合）

// Apple libc rand.c 的静态初值：未 srand 即调用时对齐其行为
// ⚠️ 全程 uint64_t：Windows 的 unsigned long 是 32 位——16807UL*g_ctx 会溢出 mod 2^32，
//    序列悄悄变质（2026-10-25 D16 快照抓获：610 字段唯 rand 不同，823 绿掩盖）
std::uint64_t g_ctx = 1;

} // namespace

int sbs_rand(void) {
    const std::uint64_t r = (16807ULL * g_ctx) % kMod;
    g_ctx = r ? r : kZeroFix;
    return static_cast<int>(r);
}

void sbs_srand(unsigned int seed) {
    g_ctx = seed ? seed : kZeroFix;
}

std::uint64_t sbs_rand_state(void) {
    return g_ctx;
}
