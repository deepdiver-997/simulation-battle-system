#!/bin/bash
# 终止判定：exit 0=该停了（stdout 给原因）｜exit 1=继续跑
# 条件：STOP 哨兵（build/night/STOP，touch 即全员停）或进入早晨停止窗口（STOP_AT，默认 08:25）。
set -u
. "$(dirname "$0")/lib.sh"
if reason="$(check_stop)"; then
    echo "STOP：$reason"
    exit 0
fi
# 队列计数用 v2 看板（taskctl）——老 kanban_* 函数指向已归档的 E 组工单，报错且无意义
echo "GO：未到停止点（STOP_AT=${STOP_AT}，队列 $(v2_pending_count) 待领取 / $(v2_busy_count) 进行中）"
exit 1
