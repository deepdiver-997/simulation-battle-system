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
· `status.sh`（晨报）· `author.sh`/`author_done.sh`（自主立单：唯一制定权抢占 +
失败者条件等待，2026-10-02）。参数经 `night.conf`（参考 `night.conf.example`）。

## 使用

工具链与本仓库的任务看板/工单/协议文档（`docs_local/`，不入库）配套——
单独拿走脚本跑不起来，设计可直接借鉴。入口：会话读取夜跑协议 → 循环
`should_stop → claim --wait → 实现 → build.sh → finish`。

## v4（2026-10-13）：会话身份 + 用户可写的停止策略

此前夜跑会话**没有稳定身份**（worker 名各会话自选、重复领取靠运气），且"何时停"
散落在协议文字与 `STOP_AT` 常量里——用户临时有"限时免费模型想多跑"这类临时诉求时，
agent 只能靠记忆里"早上 8 点停"的约定行事，既不精确也容易漂。v4 把两件事都外置：

| 新件 | 路径 | 谁写 | 谁读 | 作用 |
|---|---|---|---|---|
| **停止策略档** | `build/night/policy.json` | **用户**（`policy.sh set` 或直接改文件） | agent（每次判定重读） | `mode`=run/stop/window、`allow_overtime`、`stop_at`/`window_start`、`note` |
| **agent 注册表** | `build/night/agents.json` | agent 领/放（`agent_id.sh`），用户可 disable | 用户 + 脚本 | 唯一会话 ID、状态、心跳；用户 disable 可让单个会话立刻收摊 |

**判定优先级**（lib.sh `check_stop`）：
STOP 哨兵 > 策略档 `mode=stop` > 本 agent 被 disable > window 模式窗口 > 旧晨窗口径。

```bash
# agent 侧：领身份 → 干活 → 心跳 → 收摊释放
export SBS_AGENT_ID=$(sh tools/night/agent_id.sh claim kprod "备注")
export TASKCTL_WORKER=$SBS_AGENT_ID
sh tools/night/agent_id.sh heartbeat $SBS_AGENT_ID     # 每轮判定前
sh tools/night/agent_id.sh release  $SBS_AGENT_ID       # **只在真收摊时**（v5 起 finish 不再代劳）

# agent 侧（v5）：谁在做哪一单
sh tools/night/agent_id.sh busy    $SBS_AGENT_ID K260   # claim 自动做，手工补登记用
sh tools/night/agent_id.sh idle    $SBS_AGENT_ID        # 完工/退回时清（finish 自动做）
sh tools/night/agent_id.sh owner-of K260                # 输出活属主，无则空行
sh tools/night/agent_id.sh claimable $SBS_AGENT_ID      # 1=手上有活单 0=空闲

# 用户侧：看状态 / 改策略 / 停某会话
sh tools/night/agent_id.sh list
sh tools/night/policy.sh show
sh tools/night/policy.sh set mode=run allow_overtime=true note="限时免费模型，随便跑"
sh tools/night/agent_id.sh disable a03-kprod            # 让某个会话立刻收摊
```

设计约束：**用户写、agent 只读**——策略档与停用开关是用户侧控制面；agent 的写入口
只有 `policy.sh set`（需用户明确指示）与注册表的领/放。坏档（JSON 损坏/非法 mode）
一律回退"run 不限窗口"（宁可多跑，不静默停机）并显式提示。
首次使用：把 `policy.json.example` 拷成 `build/night/policy.json`（或直接调
`policy.sh set`，会自动创建）。

## v5（2026-10-21）：抢单防护（busy 属主校验）

**事故**：a01-kprod 做完 K259 时 `finish.sh` 自动 release 了它的 agent ID（注册表
随即显示 `released`），但它同一时刻又领了 K260 继续干；另一会话据此判定"遗弃单"→
`release` + `claim` 抢占 → **两个 agent 在同一 worktree 对同一文件并发写**。

**根因**：`released` 被当成了"会话死了"的判据，而它实际只表示"不接新单"；
`taskctl` 的 CAS 只断言行状态、不校验属主是否活着；`claim.sh` 见目录存在就复用。

| 新件 | 位置 | 作用 |
|---|---|---|
| `busy` 字段 | `agents.json` | `{"task":"K260","since":"..."}`——"手上有单"的唯一事实源 |
| `agent_id.py busy/idle` | agent 侧 | claim 登记、done 清除（按单号清，不依赖 worker 名） |
| `owner-of <单号>` | agent 侧 | 输出活着的属主（无则空行），供 claim.sh 判闸 |
| claim 属主校验 | `taskctl.py` | 该单有活属主（心跳 ≤15min 且未 disabled）→ 拒领 |
| release 属主校验 | `taskctl.py` | 🔄 行属主非本人 → 拒代退；`--force` 可越，写 `forced_from` 进 tasklog |
| worktree 在用校验 | `claim.sh` | 目录在 + 活属主 → 退回该单，不接管 |
| finish 不再 release | `finish.sh` | 完工只 `idle`（清 busy），**identity 留到真收摊** |

**语义定版**：`status` = "是否接新单"（active/released/disabled）；`busy` + 心跳 =
"是否正在干活"。**判生死只认心跳**，`released` 不作废 busy。
接管一单的正当前提只有三个：原属主主动释放 / 心跳超时 15 分钟 / 被用户 disable。

自测：`python3 tools/taskctl.py selftest`（原流程）+ 8 项端到端断言（抢单防护 +
busy 生命周期，`SBS_AGENTS_REG` 隔离注册表）——见 `docs_local/docs/06-交接/夜跑抢单事故-2026-10-21.md`。

## v2（2026-09-29）

- 看板操作全部切 `tools/taskctl.py`（`v2_claim/v2_done/v2_release/v2_pending*`），
  夜跑默认板 = F（`SBS_NIGHT_BOARD` 可覆盖）；D 组看板已归档至
  `docs_local/docs/05-任务清单/已收官/`。
- `finish.sh blocked` 语义变化：v1 是"标 ⛔ 等早晨人工改回"，v2 直接
  `taskctl release` **自动重入队**（原因落行末）；真要人工挂起就别跑 finish，留 🔄。
- 场景编号段：F 组看板行已自带段（610-709），v2 的 claim 不再调分配器
  （`scenario_alloc` 保留给无段看板）。
- v1→v2 兼容：旧 awk 函数（kanban_*）保留未删，仅 claim/finish/status 不再用。

## fuzz 夜战挂件（fuzz_night.sh，2026-10-02）

对战引擎 sanitizer 战役（test/fuzz，见其 README）：每晚 cron 02:30 跑默认档
（3 块 × 2000 seed，ASan+UBSan 统一消毒构建）。自包含：专用 worktree
`agents/night-fuzz`（detach main-local，ASan 插件 deploy 与主工作区完全隔离），
随机 seed 区间分块，块崩=发现 → 归档 `build/night/fuzz/crash-*/`（完整日志+seed）
后继续。摘要：`build/night/fuzz/last_night.txt`。手工：`fuzz_night.sh --smoke`
（连通性）/ `--chunks N`。与 claim/build/finish 协议互不依赖；cron 晨报 grep
last_night.txt + crash-* 目录即得。

## v3（2026-10-02）：自主立单（author 协议）

- 看板清空≠收摊：`author.sh` 抢唯一制定权（mkdir 锁 + **时长过期接管** AUTHOR_MAX，
  不用 60s pid 回收——持有者退出脚本后用 agent 会话探索，pid 必然"已死"）；
  争夺失败者在本次工具调用内阻塞轮询（零 API），谓词=看板非空 / 锁空轮转 / 停止点。
- 立单走 `taskctl alloc`（场景号段全局计数器，build/night/scenario.alloc，
  SBS_SCENARIO_BASE 默认 970——旧值 455/180 已与存量场景撞号作废）+ `taskctl add`
  （行前缀 + 自动接" 待领取 |"，同号去重、禁自带状态 cell）。
- 同会话卸任 AUTHOR_COOLDOWN 冷却（author.last），轮转防止一人霸占探索权。
