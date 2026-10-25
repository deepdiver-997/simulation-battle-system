#!/bin/bash
# D16 golden 链对比：Mac 基线（test/scenario/golden）vs Windows 产出（$1）
# 输出：MATCH 数 / DIFF 数（各报首个失配行号+两侧时点名）/ 单侧缺失数
set -u
WIN_DIR="${1:?usage: compare_golden.sh <wuying_chains_dir>}"
MAC_DIR="$(dirname "$0")/../../test/scenario/golden"
MAC_DIR="$(cd "$MAC_DIR" && pwd)"

match=0; diff_n=0; missing_win=0; missing_mac=0
DIFF_REPORT=""

for mac_chain in "$MAC_DIR"/*.chain; do
    name=$(basename "$mac_chain")
    win_chain="$WIN_DIR/$name"
    if [ ! -f "$win_chain" ]; then
        missing_win=$((missing_win+1)); continue
    fi
    if cmp -s "$mac_chain" "$win_chain"; then
        match=$((match+1))
    else
        diff_n=$((diff_n+1))
        # 首个失配行
        ln=$(diff "$mac_chain" "$win_chain" | grep -m1 '^[0-9]' | head -1)
        first=$(diff --unchanged-line-format= --old-line-format='MAC %dn: %L' --new-line-format='' --changed-line-format='MAC %dn: %L' "$mac_chain" "$win_chain" | head -1)
        firstw=$(diff --unchanged-line-format= --old-line-format='' --new-line-format='WIN %dn: %L' --changed-line-format='WIN %dn: %L' "$mac_chain" "$win_chain" | head -1)
        DIFF_REPORT+="=== $name"$'\n'"$first"$'\n'"$firstw"$'\n'
    fi
done
for win_chain in "$WIN_DIR"/*.chain; do
    name=$(basename "$win_chain")
    [ -f "$MAC_DIR/$name" ] || missing_mac=$((missing_mac+1))
done

echo "MATCH=$match DIFF=$diff_n MISSING_ON_WIN=$missing_win MISSING_ON_MAC=$missing_mac"
if [ -n "$DIFF_REPORT" ]; then
    echo "$DIFF_REPORT"
fi
