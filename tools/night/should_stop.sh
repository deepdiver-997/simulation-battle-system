#!/bin/bash
# 终止判定：exit 0=该停了（stdout 给原因）｜exit 1=继续跑
# 条件：STOP 哨兵（build/night/STOP，touch 即全员停）或进入早晨停止窗口（STOP_AT，默认 08:25）。
set -u
. "$(dirname "$0")/lib.sh"
if reason="$(check_stop)"; then
    echo "STOP：$reason"
    exit 0
fi
echo "GO：未到停止点（STOP_AT=${STOP_AT}，队列 $(kanban_pending_count) 待领取 / $(kanban_busy_count) 进行中）"
exit 1
