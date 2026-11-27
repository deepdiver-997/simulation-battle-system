// 引擎自有确定性 RNG 实现（见 include/utils/sbs_rand.h 与决策 D13/D15）。
//
// 历史：M1 先用 -Wl,--wrap 链接期改道（彼时符号名 __wrap_rand/__wrap_srand）达成 Mac 序列
// 复现；D15 起改源码级替换（sed 全量迁移，符号即本文件的 sbs_rand/sbs_srand），对任意
// 工具链（含 MSVC）成立，wrap 机制退役。内核和插件显式使用对局持有的实例；
// 旧的无参 API 操作独立的非对局兼容序列。
//
// 算法：MINSTD（a=16807, m=2^31-1），`state = state*16807 mod m` 直接返回；srand(0) 与撞 0
// 时存储侧重置为 123459876（当次调用仍返回 0）。2026-10-25 于 macOS libc 实证拟合：
// 10 种子 × 1000 值全等（含 seed=0、seed=2^31-1 边角）。
#include "utils/sbs_rand.h"

#include <cstdint>

namespace {

constexpr std::uint64_t kMod = 2147483647ULL;      // 2^31-1
constexpr std::uint64_t kZeroFix = 123459876ULL;   // srand(0)/撞 0 的重置常数（macOS libc 实证拟合）

thread_local SbsRng compatibility_rng;

} // namespace

SbsRng::SbsRng(unsigned int value) noexcept : state_(value ? value : kZeroFix) {}

SbsRng& SbsRng::operator=(const SbsRng& other) noexcept {
    if (this != &other) {
        state_ = other.state_;
        ++revision_;
    }
    return *this;
}

void SbsRng::seed(unsigned int value) noexcept {
    state_ = value ? value : kZeroFix;
    ++revision_;
}

int SbsRng::next() noexcept {
    const std::uint64_t r = (16807ULL * state_) % kMod;
    state_ = r ? r : kZeroFix;
    ++draw_count_;
    return static_cast<int>(r);
}

int sbs_rand(void) { return compatibility_rng.next(); }
void sbs_srand(unsigned int seed) { compatibility_rng.seed(seed); }
std::uint64_t sbs_rand_state(void) { return compatibility_rng.state(); }
