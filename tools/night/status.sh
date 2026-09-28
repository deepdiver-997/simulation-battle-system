#!/bin/bash
# 晨报：夜跑战况一览（只读，随时可跑）。
set -u
. "$(dirname "$0")/lib.sh"

echo "═══ 看板战况 ═══"
awk -F'|' '/^\| D[0-9]/ {
    st=$4; gsub(/ /,"",st);
    k=$2; gsub(/ /,"",k);
    cnt[st]++
    if (st=="✅完成" || st=="⛔阻塞" || st=="🔄进行中") {
        note=$5; gsub(/^ +| +$/,"",note);
        printf "%-4s %-8s %s\n", k, st, note
    }
} END {
    printf "\n待领取 %d ｜ 进行中 %d ｜ 完成 %d ｜ 阻塞 %d\n", cnt["待领取"]+0, cnt["🔄进行中"]+0, cnt["✅完成"]+0, cnt["⛔阻塞"]+0
}' "$KANBAN"

echo
echo "═══ 领取/完工流水（最近 10 条）═══"
git -C "$HUB" log --oneline -10 -- "$KANBAN_REL" 2>/dev/null || true

echo
echo "═══ worktree / 磁盘 ═══"
git -C "$HUB" worktree list
du -sh "$(dirname "$HUB")/agents" 2>/dev/null || true

echo
echo "═══ 运行时状态 ═══"
[ -f "$STOP_FILE" ] && echo "STOP 哨兵：存在（删掉才会继续跑）" || echo "STOP 哨兵：无"
ls "$SEM_DIR" 2>/dev/null | grep -v pid >/dev/null 2>&1 && echo "构建槽占用：$(ls "$SEM_DIR" | tr '\n' ' ')" || true
command -v ccache >/dev/null 2>&1 && echo "ccache：$(ccache -s 2>/dev/null | head -3 | tr '\n' ' ')" || echo "ccache：未安装（brew install ccache 可让后续 worktree 冷构建近免费）"
echo "停止窗口：STOP_AT=${STOP_AT} ｜ 构建在飞上限 SLOTS=${SLOTS}（每槽 -j${JOBS}）"
