# tools/night — 多 agent 夜跑工具链（agent-ops）

让多个 AI 编码会话（ZCode agent）在一台笔记本上**无人值守接力跑通宵**：
领取任务 → 各自 worktree 实现 → 限流构建回归 → 登记完工 → 领下一个，直到
任务清空或到点收摊。D 组 29 个精灵技能批产中一夜完成 24 个（191/191 回归绿）
就是这套跑出来的。

## 设计要点

| 问题 | 解法 |
|---|---|
| 多会话同时构建打满 CPU | `lib.sh` mkdir 原子信号量限流（在飞 ≤SLOTS，排队阻塞在脚本内，不消耗 agent 的 API 额度） |
| 多会话重复领取同一任务 | **v2：看板操作走 `tools/taskctl.py`**（flock 临界区 + CAS 改前断言 + 原子写 + tasklog 审计）——claim.sh 调 `taskctl claim`，抢失败响亮退出重试；v1 的 awk 文本扫描已废（板列结构变更即碎，E 板状态列在第 5 列就扫不到） |
| 并发任务撞测试编号 | `claim.sh` 按全局计数器给每任务分配场景编号段 |
| 会话做完一批就闲置 | `claim.sh --wait` 在**一次工具调用内** sleep 轮询队列（阻塞期零 API 消耗），有活立刻醒 |
| 到点/清空自动收摊 | `should_stop.sh`：STOP 哨兵文件 + 早晨停止窗口（给额度窗口留缓冲）+ 看板清空三路判定 |
| 解包失败假绿 | 场景 harness 开 `effect_arg_fail_fast`：FSM 时点跳转检查点写参数排布报告后终止对局（见 `include/fsm/battleContext.h` EffectArgFault） |
| 崩溃残留死锁 | 锁目录带 pid，持有者死后 60s 自动回收 |

## 文件

`lib.sh`（信号量/互斥/**v2 看板函数**）· `claim.sh`（领取+worktree）· `build.sh`
（限流构建+全量回归）· `finish.sh`（完工/阻塞登记）· `should_stop.sh`（终止判定）
· `status.sh`（晨报）。参数经 `night.conf`（参考 `night.conf.example`）。

## 使用

工具链与本仓库的任务看板/工单/协议文档（`docs_local/`，不入库）配套——
单独拿走脚本跑不起来，设计可直接借鉴。入口：会话读取夜跑协议 → 循环
`should_stop → claim --wait → 实现 → build.sh → finish`。

## v2（2026-09-29）

- 看板操作全部切 `tools/taskctl.py`（`v2_claim/v2_done/v2_release/v2_pending*`），
  夜跑默认板 = F（`SBS_NIGHT_BOARD` 可覆盖）；D 组看板已归档至
  `docs_local/docs/05-任务清单/已收官/`。
- `finish.sh blocked` 语义变化：v1 是"标 ⛔ 等早晨人工改回"，v2 直接
  `taskctl release` **自动重入队**（原因落行末）；真要人工挂起就别跑 finish，留 🔄。
- 场景编号段：F 组看板行已自带段（610-709），v2 的 claim 不再调分配器
  （`scenario_alloc` 保留给无段看板）。
- v1→v2 兼容：旧 awk 函数（kanban_*）保留未删，仅 claim/finish/status 不再用。
