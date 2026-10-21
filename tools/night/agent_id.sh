#!/bin/bash
# agent ID 领取/心跳/释放（agent 侧入口）——用法：
#   agent_id.sh claim <前缀> [备注]   # 领一个唯一 ID（stdout 就是 ID）
#   agent_id.sh heartbeat <id> [备注] # 刷新心跳（主循环每轮前调）
#   agent_id.sh release <id>          # 收摊注销
#   agent_id.sh list                  # 看全部（用户/agent 都能看）
#   agent_id.sh get <id>              # 查单个
# 用户侧：agent_id.sh disable <id> / enable <id>（停用/恢复某会话）
set -u
. "$(dirname "$0")/lib.sh"
exec python3 "$NIGHT_ROOT/agent_id.py" "$@"
