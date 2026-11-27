#ifndef SBS_RAND_H
#define SBS_RAND_H

// 引擎自有确定性 RNG（MINSTD，a=16807, m=2^31-1）——全工程源码级替换 std::rand/srand 的
// 唯一入口。为什么必须替换：std::rand 序列是实现定义相关（macOS libc 与 UCRT/glibc 不同），
// 场景断言锚定了 Mac 序列，社区要求任意平台同种子同序列（决策 D13/D15，见
// docs_local/docs/07-工具与测试/Windows-移植.md）。
//
// 硬规约：**全工程禁止再直接调用 <cstdlib> 的 rand/srand**（含 std:: 前缀与裸调用）。
// 审计命令（应仅命中 sbs_rand / sbs_srand 自身与本文档注释）：
//   grep -rnE '(^|[^_a-zA-Z0-9:])(std::)?(s?rand)\b' src include resources test/scenario examples
//
// 对局按值持有 SbsRng；内核与插件显式使用当前对局的实例。
// 算法保持 macOS libc 序列（包括零种子和撞零）。
#include <cstdint>

class SbsRng {
public:
    explicit SbsRng(unsigned int seed = 1) noexcept;
    // Copy construction snapshots both counters and the stream. Assignment
    // replaces the stream, preserving the destination's draw history.
    SbsRng(const SbsRng&) noexcept = default;
    SbsRng& operator=(const SbsRng&) noexcept;
    void seed(unsigned int seed) noexcept;
    int next() noexcept;
    std::uint64_t state() const noexcept { return state_; }
    // One increment per next(); reseeding never resets the draw count.
    std::uint64_t draw_count() const noexcept { return draw_count_; }
    // New instances start at zero. Explicit seed() and non-self assignment
    // advance this marker even if they replace the stream with the same state.
    std::uint64_t revision() const noexcept { return revision_; }
private:
    std::uint64_t state_;
    std::uint64_t draw_count_ = 0;
    std::uint64_t revision_ = 0;
};

// 非对局兼容序列（旧场景预播种及纯探针）。每线程独立，永不绑定对局。
// 省略 seed 的 BattleContext 仅在构造时复制此状态；已有对局不受这些函数影响。
int sbs_rand(void);
void sbs_srand(unsigned int seed);
std::uint64_t sbs_rand_state(void);

#endif // SBS_RAND_H
