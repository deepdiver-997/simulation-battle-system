# tools/db_patches — 认证数据层的本地修正补丁

**官方镜像数据 ≠ 正确数据**：`side_effect.arg_count` 等官方表存在既存错误（R1 夜跑
三方互证修了 13 处），而数据管线每次重建都会把官方表刷回官方原值。

## 规约（2026-10-01 起强制）

```
fetch → decode → import_seer_unity_sqlite.py → tools/db_patches/apply_all.sh → tools/night/build.sh
```

漏掉第三步的症状：切片敏感场景（871/872/878/880 等覆盖 R1 修正效果的）成批转红。

## 目录

- `r1/` — R1 夜跑切片双表修正 13 处（2026-09-30，每处证据见 R 组工单核对表）
- `x2/` — 小修批 X2：effect 1248 custom_effect_programs 异常码+概率接线（2026-10-01）

## 补丁文件格式

每个 `.sql`：头部证据注释 + 回滚语句（注释）+ UPDATE（带 AND 防御条件）+ `[verify]` 查询。
`apply_all.sh` 按目录序全量重放，幂等（UPDATE 自带前置条件）。
