# D 组夜跑协议（多会话过夜批产，2026-09-28）

> 目标：睡前开 N 个会话（16GB 机器建议 **3~4 个**），整夜自动接力领完 D 组看板；
> 全部完成或到早晨停止窗口（默认 **08:25**）自动收摊，09:00 额度窗口前干净退出。
> 构建限流、防重复领取、场景编号防撞、队列空滞留，全部由 `tools/night/` 脚本承担。

## 给用户：睡前三步

1. （可选）`cp tools/night/night.conf.example tools/night/night.conf` 后调参：
   `SLOTS`（构建在飞上限，默认 2）、`STOP_AT`（停止窗口，默认 0825）。
2. 确认看板（`D组启动看板.md`）里任务都是「待领取」；想临时全停，`touch build/night/STOP` 即可。
3. 开 N 个会话，每个粘贴下面这段（把 night-1 依次换成 night-2、night-3…）：

```
你是今晚的批产会话，编号 night-1。仓库 /Users/zhuhongrui/code/c++/project/sbs-local。
读取 docs_local/docs/05-任务清单/已收官/D组夜跑协议.md，进入其「会话主循环」，直到终止条件。
实现规则、必读文档、硬性禁令一律以 docs_local/docs/05-任务清单/已收官/D组提示词模板.md 为准；
E 组（批量效果家族）任务另读 docs_local/docs/05-任务清单/已收官/E组批量效果工单.md 的会话协议差异节——
动工前必查内核硬编码（697/699 假阳性教训），解包一律走 EFFECT_ARG_GUARD。
构建只准走 tools/night/build.sh（自动限流，排队发生在脚本内不消耗 API）；
领取只准走 tools/night/claim.sh night-1 --wait；收尾只准走 tools/night/finish.sh。
队列空时用 --wait 滞留而不是退出；不合并 main-local、不 push。
```

## 会话主循环（每个夜跑会话照此执行）

```
┌ 1. bash tools/night/should_stop.sh   → exit 0：按「到点收摊」收尾，结束会话
│
├ 2. bash tools/night/claim.sh night-N --wait
│     · stdout 有 CLAIMED 块 → 记下 worktree / 分支 / 场景编号段（只准用这段编号！）
│     · 输出 ALL_DONE       → 全部完成，汇报后结束会话
│     · 输出 STOP …         → 到点收摊，结束会话
│     · 退出码 3            → 滞留超时，回到第 1 步
│
├ 3. 实现（worktree 内，规则全按 D组提示词模板.md 第二/三步 + 硬性禁令）：
│     · cd <worktree>；按工单读必读文档（只读本任务相关的页，控制上下文）
│     · 效果实现放【一个新文件】 resources/moves_lib/d5_<Dn>.cpp（如 d5_D2.cpp）
│       ——⚠️ 夜跑修订：不用模板里的公共 d_fifth_skills.cpp！moves_lib 已 GLOB 化免登记，
│       每任务一个独立文件 = 合并零冲突；公共文件会让 28 个分支全在文件尾打架
│     · 场景文件只用 claim 分配的编号段（夜跑全局自动顺延，D1 已占 170，夜跑从 180 起；
│       手选编号会撞号）；范式照模板（batch_a_skills.cpp + scenario_14*）
│     · 新文件 git add -f（ignore 静默吞文件的老教训）
│
├ 4. bash tools/night/build.sh   （限流构建 + 168 场景全量回归 + registry dump）
│     · 编译/回归失败 → 读 build/night-build.log、build/night-regression-fails.txt，修了再跑
│       （增量构建很快，且排队在脚本内不耗额度）
│     · 同一任务连修 3 次仍失败 → 不要恋战（夜里没人拍板）：提交现场，
│       bash tools/night/finish.sh <Dn> blocked "失败原因"  → 回到第 1 步
│
├ 5. 每只精灵一个 commit（message 含：效果 id→实现方式一句话、跳过项及原因）
│   bash tools/night/finish.sh <Dn> ok "效果清单一句话"
│
└ 回到第 1 步
```

**到点收摊**（STOP/08:25 后）：当前任务若已到半成品——提交现场并
`finish.sh <Dn> blocked "到点收摊，WIP 已提交（进度：…）"`，早晨把状态改回「待领取」即可续做；
已完成的照常 `finish.sh ok`。然后结束会话，不领新任务。

## 上下文纪律（让自动压缩无损）

- ZCode 上下文满会自动压缩——**任务状态只落在盘上**：领取/完工写看板，成果写 commit
  message，异常写 finish 备注。压缩丢了记忆，盘上状态照样接得上。
- 文档按需读：每任务只开工单表 + 明细对应条目，通读交接文档一次即可。
- 每任务收尾时在 finish 备注里写一句话结论，等于给下一个任务（可能被压缩后的自己）留交接。

## 机制速查（脚本替你做了什么）

| 脚本 | 作用 | 要点 |
|---|---|---|
| `claim.sh <名> --wait` | 领号：hub 锁内翻看板状态→提交→分配场景编号段→建 worktree→cmake 配置 | 领取登记在 hub 提交，防重复领取；队列空则脚本内滞留轮询（零 API） |
| `build.sh` | 限流构建 + 全量回归 + registry dump | mkdir 信号量限在飞 ≤SLOTS；排队在脚本内阻塞，不耗额度；日志在 `build/night-build.log` |
| `finish.sh <Dn> ok\|blocked` | 看板翻 ✅/⛔ + 收官记录 | 自动附分支/commit；worktree 保留待晨间合并 |
| `should_stop.sh` | 终止判定 | STOP 哨兵或早晨窗口（默认 08:25）；只拦新任务，手头活收尾 |
| `status.sh` | 晨报（只读） | 战况、流水、worktree、磁盘、ccache |
| `touch build/night/STOP` | 全员急停 | 下次循环判定时生效 |

## 早晨收摊（用户/协调会话）

1. `bash tools/night/status.sh` 看战况；
2. 按 Dn 序把 ✅ 分支逐个并回 main-local：`git merge --no-ff feat/d-Dn` → hub 全量构建回归
   （此时无并发，可全量跑）→ 有问题该分支退回；
3. ⛔ 行逐条人工复核，改回「待领取」重入队（或白天会话处理）；
4. 验收完清理：`git worktree remove ../agents/d-Dn --force`（D1 同理收尾）；
5. 顺手 `git worktree prune`。

## 与 daytime 纪律的关系

- 白天单会话/双会话照旧走原看板协议（手动错峰）；夜跑协议只是把错峰、防撞号、防重领
  脚本化了，两者共用同一张看板与状态集。
- D1 已收官，夜跑会话从最小「待领取」号（D2）自动接手。
