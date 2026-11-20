# scripts/patches — 外部解码依赖（seerapi-solaris）的修复补丁

数据管线对 `moves.bytes` 的解码依赖外部 PyPI 包 `seerapi-solaris`。本目录存放
该外部安装包的补丁与安装说明（补丁本体进仓库，环境侧按需应用）。

## moves.bytes 2026-11-06 布局变更（K450）

**变更**：2026-11-06 游戏更新（镜像 ConfigPackage `20261106181039` 起），
`moves.bytes` 每个 move item 在 `side_effect_arg` 之后、`type` 之前插入一个
int 字段。上游 seerapi-solaris **106.12.2（2026-11-07 发布）已修复**，字段命名
`soercemove`（变体/联动源技能 id：多数为 0；变体技指向母本技能，如
13739 痴愚 → 37460 无相谛）。

**安装包修复方式（三选一）**：

1. **升级（推荐，本机 <106.12.2 时）**：
   ```bash
   pip3 install -U "seerapi-solaris>=106.12.2" -i https://pypi.tuna.tsinghua.edu.cn/simple
   ```
   注意升级会整体换包（254 个解析器），若需锁版本用方式 2。

2. **对 106.10.x–106.12.1 应用 backport 补丁**（本目录
   `seerapi-solaris-106.10.x-moves-soercemove-backport.patch`，字节级等价于
   上游 106.12.2 的修复）：
   ```bash
   patch /opt/homebrew/lib/python3.14/site-packages/solaris/parse/parsers/moves.py \
         scripts/patches/seerapi-solaris-106.10.x-moves-soercemove-backport.patch
   ```
   ⚠️ backport 与上游一样**只解新布局**：补丁后的包对 pre-11-06 旧 moves.bytes
   会解码失败（与 106.12.2 行为一致）。需要新旧双布局兼容时用方式 3。

3. **什么都不动**：`scripts/decode_seer_bytes.py` 已内置仓库内 vendored 双布局
   解析器（`scripts/solaris_overrides/moves.py`，逐文件探测新旧布局、字段名沿用
   上游 `soercemove`），对任何已安装版本都能正确解码两代数据。**当前管线默认走
   这条路**——环境侧无需任何改动。

**管线现状（2026-11-10，K450）**：本机安装为 106.10.2；未改 site-packages，
decode 步骤经 vendored 覆盖层解码新旧两代 moves.bytes 均验证通过
（27394 / 27414 items，EOF 精确）。升级到 ≥106.12.2 后，新布局的字段名和语义
与官方实现一致，但**不能仅因升级就删除覆盖层**：106.12.2 仍不兼容旧布局。
只有不再需要解码旧快照，或后续上游的新旧双布局兼容已通过同一组 fixture 验证，
才可评估移除 vendored 层。
