# tools/agents/ —— sbs-local 的多 agent 并行工作区（agent-workspace-kit 接入）

> 上游：`~/.agent-parallel/`（机器级母本）。本目录是项目副本 + **本项目策略层**。
> kit 不是任务框架——方向由人定；它只保证 N 个 agent 并行时不出事故
> （磁盘预算 / 全局构建锁 / 独占资源 / 无头派发的 run 目录契约）。

## 日常命令

```bash
# 开工作区（worktree + feat/<name> 分支 + 资源分配 + 预算登记；--build 顺手过门）
tools/agents/new-worktree.sh <name> [--from main-local] [--build]

# 无头派发（task.md 开头抄 task-template.md；resume 追加纠偏，不实时插话）
tools/agents/spawn.sh start claude <name> <task.md>
tools/agents/spawn.sh status [name]
tools/agents/spawn.sh resume  <name> "追加指令"
tools/agents/spawn.sh stop    <name>

# 收工作区（脏树/未合并提交有保护；预算账本同步注销）
tools/agents/remove-worktree.sh <name>

# 全量门（全局构建锁版：agent 侧统一入口；人手工跑 tools/night/build.sh 也行）
tools/agents/gate.sh

# 集成收割（夜跑产出的统一归宿，2026-10-21 加入——别留一堆待审核分支/工作区）
tools/agents/collect.sh <name>          # feat/<name> --no-ff 汇入 integration/<当日>
tools/agents/collect.sh status          # 批分支内容清单
# 验收 = 对批分支一次 gate + 一次合回 main-local；remove-worktree 认账 integration/*
```

## 本项目策略层（接一个新项目唯一要动脑的部分，这里是 sbs 的答案）

| 件 | 本项目的做法 |
|---|---|
| `agents.env` | BASE_BRANCH=main-local；DISK_RESERVE_GB=8（build 实测 5.6G）；BUILD/TEST_CMD=gate.sh（本仓库构建与测试是一体的全量门） |
| `apply_agent_env.py` | **无配置改写**：sqlite 只读共享、sim_server 自动挑空闲口、build/ 天然 per-worktree——预留端口段仅备用 |
| 数据库 | 无 per-worktree 库（上游邮件项目的 MySQL 建库步骤在本项目不适用） |
| `gate.sh` | mkdir 原子全局锁（.git/agents/build.lock，pid+60s 陈旧清扫，超时 exit 75）包装 tools/night/build.sh |
| `task-template.md` | **项目特色策略核心**：spawn 任务起手式（必读四文档、立单三查+跨 worktree grep、号段 alloc、db_at 行参、pct 在位即消费、红伤相管线口径、checked_pct_mul、taskctl 留痕、验收=过门、fail-closed 红线） |

## 与既有夜跑设施的关系

- `tools/night/build.sh`（限流门）保持原样——gate.sh 只是给它加全局锁壳；
  night 信号量（在飞≤2）继续管本仓库内部并发，全局锁管跨项目/跨机器用户。
- 看板/领单协议不变（夜跑总入口 + taskctl + tasklog）；kit 只替换
  worktree 手工管理（旧的 agents/kNNN 散落模式）与后台会话派发。
- 合并验收仍由集成会话做（--no-ff 按任务序 + 合并记录 + worktree 清理，
  见 06-交接/合并记录-* 先例）。

## 两层配置

机器级 `~/.agent-parallel/tools/agents.env`（锁等待/端口段起点）+ 项目级本目录
`agents.env`（项目覆盖机器）。`agents.env`/`harnesses.conf.local` 被 gitignore——
从 `.example` 复制后填。设备磁盘预算全机一份：`budget.sh set <GB>`（人工设定）。
