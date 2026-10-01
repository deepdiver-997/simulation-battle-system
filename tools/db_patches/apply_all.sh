#!/bin/sh
# apply_all.sh —— 数据管线重建后，把全部本地 DB 补丁重放到 seer_unity.sqlite
#
# 背景（2026-10-01 管线刷新实证）：import_seer_unity_sqlite.py 从官方 JSON 重建时，
# side_effect/effect_info 等**官方表**会回到官方原值——R1 夜跑打的 13 处切片修正
# 全被冲掉，4 个切片敏感场景（871/872/878/880）当场转红。引擎自有表
# （custom_effect_programs / custom_effect_overrides）是 CREATE TABLE IF NOT EXISTS，
# 重建后保留，无需重放。
#
# 规约：**每次跑完 import_seer_unity_sqlite.py 必须接一条 apply_all.sh**，然后跑
# tools/night/build.sh 全量回归确认绿。新增补丁按批次入目录（r1/ x2/ …），
# 每个 .sql 自带证据注释 + 回滚语句 + [verify] 查询。
set -eu
DB="${1:-scripts/data/processed/seer_unity.sqlite}"
HERE="$(cd "$(dirname "$0")" && pwd)"

if [ ! -f "$DB" ]; then
    echo "❌ DB 不存在：$DB（在仓库根目录跑，或传路径参数）" >&2
    exit 1
fi

n=0
for dir in "$HERE"/*/; do
    for f in "$dir"*.sql; do
        [ -f "$f" ] || continue
        echo "── apply $(basename "$dir")/$(basename "$f")"
        sqlite3 "$DB" < "$f"
        n=$((n + 1))
    done
done
echo "✅ 已重放 $n 个补丁 → $DB"
