# AGENTS.md — AI 贡献者上手地图

本文件面向 AI agent 与新贡献者。架构细节见 `CLAUDE.md`；贡献流程与合规边界见
`CONTRIBUTING.md`；动手写效果前**必读** `docs/07-工具与测试/效果实现指南.md`。

## 0. 一句话

数据（SQLite）→ 组装层 → 效果内核（注册表 + 时点桶）→ FSM 逐时点驱动 → 效果读写 BattleContext。
**机制是数据，不是 struct**：新效果一律写成 `resources/` 插件 + 场景测试，不改内核。

## 1. 环境与构建

```bash
cmake -B build && cmake --build build -j8      # 目标：sim_server / sim_smoke_test / sim_scenario_*
./build/sim_server --dump-registry-exit build/registry.json   # 注册表导出（覆盖率对账数据源）
```

数据库**不入库**，从社区公开镜像构建（镜像每 30 分钟同步官方配置）：

```bash
python3 scripts/fetch_seer_unity_bytes.py      # 拉 .bytes 到 scripts/data/raw/
python3 scripts/decode_seer_bytes.py           # 解码入库 → scripts/data/processed/seer_unity.sqlite
tools/db_patches/apply_all.sh                  # 重放本地修正补丁
```

## 2. 分层与改动边界（CI 自动分类，混合 PR 会被拒）

| 你的 PR 只碰 | 分类 | 合并通道 |
|---|---|---|
| `resources/`（效果/魂印/套装插件）+ `test/scenario/`（**新增**文件） | 效果层·纯新增 | gate 绿 + 带引注 → 自动合并 |
| 同上但**修改既有**效果/场景期望 | 效果层·改口径 | `needs-ruling` 标签，维护者终审 |
| `src/`、`include/` | 内核层 | 必须先有获批 issue + `kernel-approved` 标签 |
| 两者混碰 | mixed | 自动拒绝，请拆成两个 PR |

效果 id 与内核耦合较深的（先制路由/连击谓词/申报制异常等）见
`docs/07-工具与测试/内核耦合效果清单.md`——这批请开 issue 而不是直接提 PR。

## 3. 效果实现循环（每一步都可机器验证）

1. 查库：`effect_info` 拿模板句与 `args_num`，`moves.side_effect` 拿载体清单（指南 §1 有现成 SQL）；
2. 实现：`SKILL_EFFECT(id)` 声明即注册，id 写错编译不过；注释 = 模板短句锚点 +
   你自己的实现理由 + 证据链接（**允许引一句模板**，禁整段原文照搬与文本语料化）；
3. 场景：`test/scenario/scenario_<id>_<名>.cpp`，真实 FSM + assert，头注写口径依据；
4. 变异：摘除你的实现，场景必须转红（MUT），还原复绿；
5. 全量门：`tools/agents/gate.sh`（或直接全量构建+场景回归）；
6. PR：按模板填场景号 / MUT 证据 / gate 结论 / 引注链接。

## 4. 硬红线（违反 = 直接关 PR）

- 不提交任何官方数据、美术素材、构建产物；不把官方文本**成体系**入库或照搬进
  代码/数据文件（语料化）。注释里引一句效果模板短句作锚点是允许的；
- 不发明效果 id——id 必须来自 `moves.side_effect` / `effect_info`；
- 不在效果层裸写内核存储——走效果原语与注册表；
- 一效果一注册点（撞号会让 `SBS_ID_COLLISION_STRICT` 构建直接失败）。
