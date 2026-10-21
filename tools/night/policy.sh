#!/bin/bash
# 停止策略档（用户可写、agent 只读）——用法：
#   policy.sh show                 # 读当前策略（agent 每次判定前念一遍）
#   policy.sh set mode=run|stop|window [allow_overtime=true|false] [stop_at=HHMM] [window_start=HHMM] [note=...]
#   policy.sh path                 # 策略档路径（用户直接编辑这个文件也行）
set -u
. "$(dirname "$0")/lib.sh"
exec python3 "$NIGHT_ROOT/policy.py" "$@"
