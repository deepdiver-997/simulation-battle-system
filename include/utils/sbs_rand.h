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
// 实现在 src/utils/sbs_rand.cpp（SHARED 库：宿主 exe 与插件 DLL 进程内共享单一状态，
// 对齐 macOS 单一 libc rand 状态语义）。算法 2026-10-25 于 macOS libc 实证拟合：
// 10 种子 × 1000 值全等（含 srand(0)=123459876 重置与撞 0 边角）。
int sbs_rand(void);
void sbs_srand(unsigned int seed);

// 快照/诊断用：当前内部状态（不消耗序列）。M1.5 golden 快照以此为首要漂移探测器——
// 跨平台任一处 rand 消费次数漂移，下一个时点的 state 立即不同。
// ⚠️ 用 uint64_t：Windows 的 unsigned long 是 32 位（位宽敏感算术一律 uint64_t，见坑 14）。
#include <cstdint>
std::uint64_t sbs_rand_state(void);

#endif // SBS_RAND_H
