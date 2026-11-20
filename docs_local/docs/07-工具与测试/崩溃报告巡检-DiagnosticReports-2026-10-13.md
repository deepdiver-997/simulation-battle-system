# DiagnosticReports 崩溃报告巡检 — 2026-10-13

> 巡检范围：`~/Library/Logs/DiagnosticReports`（含 `Retired/` 归档）。
> 结论先行：**2026-10-13 00:04–01:00 窗口内的 23 份崩溃报告全部是预料之内的测试断言失败产物，无真实内存错误。**

## 一、本次窗口报告分类

| 数量 | 进程 | 信号 | 栈顶 | 性质 |
|---|---|---|---|---|
| 21 | `sim_scenario_3830/3870/3920/3930/3970_*` | SIGABRT | `abort ← main ← start` | 场景测试断言失败（TDD 红灯跑） |
| 2 | `gtimeout` | SIGABRT | `raise ← main ← start` | coreutils timeout 状态传播的伴生报告 |

没有 EXC_BAD_ACCESS / EXC_ARM_DA_ALIGN / data race / ASan 报告；所有报告的异常类型都是 `EXC_CRASH (SIGABRT)`，`asi` 一栏为 `abort() called`。

## 二、机制解释

### 1. `sim_scenario_*`：场景测试的红灯跑

- 每个场景是 `test/scenario/scenario_*.cpp` 编出的独立二进制（`build/sim_scenario_*`，GLOB 自动发现），**断言失败即 `abort()` → SIGABRT → 退出码 134**。这是本仓库场景测试的既定失败通道（先例：`docs/01-架构与设计/网络层与服务端形状.md` 记档 `sim_scenario_030_aigens_finish` exit 134）。
- macOS 的崩溃报告器对**任何**以致命信号退出的进程都会落一份 `.ips` 文件——包括测试框架故意 `abort()` 的场景。所以"红灯跑 = 一份 .ips"是副作用，不是故障。
- 窗口内的 5 个场景（3830/3870/3920/3930/3970）正好是夜跑 k-prod-5 / k-prod-6 当晚 K239/K243/K245/K246/K247 五张工单的新场景，源文件在各自 worktree 分支（`feat/k239`～`feat/k247`），尚未合入 main-local，故主线 `test/scenario/` 里搜不到。

### 2. `gtimeout`：同秒伴生，纯噪音

两份 `gtimeout` 报告（00:42:00、00:43:01）与 `sim_scenario_3930` 的两份报告**精确同秒**。coreutils `timeout` 在子进程死于信号时会给自己 `raise()` 同样信号以向 shell 传递退出状态（栈 `raise ← main` 可证），macOS 于是也给 `gtimeout` 落一份报告。属 coreutils 在 macOS 上的已知表面现象。

## 三、时间线对齐（tasklog.jsonl + git log）

| 场景（崩溃时间） | 对应工单 | 工单状态 | 备注 |
|---|---|---|---|
| 3830（00:04–00:05，2 份） | K239 1899 必发烧伤族 | 00:11 完成 | "回归 553 全绿" |
| 3870（00:29–00:30，6 份） | K243 1019 先后手双段固定伤 | 00:35 完成 | "回归 553 全绿" |
| 3920（00:41–00:45，10 份） | K245 798 低体麻痹+高体增伤窗 | 00:52 完成 | 红灯迭代次数最多 |
| 3930（00:42–00:43，2 份） | K246 826 低体害怕+高体免异常 | 00:5x 完成 | 两份 gtimeout 伴生在此 |
| 3970（01:00，1 份） | K247 739 烧伤门先制+威力翻倍 | 00:54 领取 | 开发中红灯 |

红灯报告数与工单的调试轮次吻合；每张工单收尾均有"回归 553 全绿"记档，最终态通过。

## 四、历史佐证

`Retired/` 归档 177 份中：`sim_scenario_*` 156 份、`gtimeout` 9 份、`sim_fuzz_battle` 若干（fuzz harness 找到异常主动退出的同款机制），其余为 Apple 系统诊断（`SFA-*.diag`、`triald` 等，系统自带噪音）。即：**这类报告自批产开跑以来反复出现，是常态而非新增异常。**

## 五、判据备忘（下次巡检照此筛）

**预期内（可忽略）**：`sim_scenario_*` / `sim_fuzz_battle` / `gtimeout` 的 SIGABRT，栈顶 `abort`/`raise` 且来自 `main`。

**需要关注（升级处理）**：
- 异常类型为 EXC_BAD_ACCESS、EXC_CRASH 以外的硬件异常，或 `asi` 出现 heap-buffer-overflow / ASan / leak 字样；
- 栈深入 `liblib_1.dylib` 内部帧（而非 `main` 直达 `abort`）——说明不是断言失败而是真崩溃；
- 崩溃进程不属于本仓库场景/fuzz 家族；
- 同一**已完成（✅）工单**的场景在收尾后再次出现红灯报告（说明合入后有回归，需比对 commit）。

## 六、处理建议

无需处理。可选降噪（均**未执行**，避免动到夜跑 agent 的环境）：
1. 回归脚本可给场景跑包一层 `crashreporter` 屏蔽或跑前清点窗口内 `.ips` 数量、跑后只报增量；
2. 或在夜跑收尾记档里带上"本次窗口预期 N 份红灯 .ips"，方便下一位巡检者对账。
