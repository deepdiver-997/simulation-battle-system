#!/bin/bash
# tools/agents/gate.sh —— 全量门（全局构建锁版）
#
# agent-workspace-kit 契约：项目构建入口必须持有 .git/agents/build.lock（mkdir 原子 +
# pid 戳 + 陈旧清扫），保证**全机器同一时刻只跑一个构建**（夜跑 91 worktree OOM 事故
# 的机制化防线）。本脚本是 tools/night/build.sh（限流构建 + 673 场景全量回归 +
# registry dump + 孤儿二进制清理）的全局锁包装：拿到锁 → 透传；等锁超时 exit 75。
# 人手工跑 tools/night/build.sh 不经此锁——它自带 night 信号量（在飞≤2）；
# spawn 出去的 agent 一律走本入口（agents.env BUILD_CMD/TEST_CMD 已指向这里）。
set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GIT_COMMON_DIR="$(git -C "${SCRIPT_DIR}" rev-parse --path-format=absolute --git-common-dir 2>/dev/null)"
[ -n "${GIT_COMMON_DIR}" ] || { echo "[gate] 必须在 git 检出内运行" >&2; exit 1; }
LOCK_DIR="${GIT_COMMON_DIR}/agents/build.lock"
BUILD_LOCK_WAIT="${BUILD_LOCK_WAIT:-3600}"   # 秒；机器级 agents.env 可调

log() { printf '\033[0;34m[gate]\033[0m %s\n' "$*"; }

acquire() {
    local waited=0
    while true; do
        if mkdir "${LOCK_DIR}" 2>/dev/null; then
            echo "$$ ${PWD} $(date +%s)" > "${LOCK_DIR}/owner"
            return 0
        fi
        # 陈旧清扫：持有进程已死且超 60s
        # owner 文件是**一行**三字段："<pid> <pwd> <epoch>"（见上面 acquire 的写入）。
        # 旧实现按两行读（head/tail 各取一行）→ 读回整行，kill -0 必失败、算术里
        # 把路径词当变量名求值，set -u 下直接 "private: unbound variable" 崩掉，
        # 等锁语义失效（2026-10-22 a17-jprod 实证）。按字段取。
        local owner_pid since
        owner_pid="$(awk '{print $1}' "${LOCK_DIR}/owner" 2>/dev/null)"
        since="$(awk '{print $3}' "${LOCK_DIR}/owner" 2>/dev/null)"
        case "${since}" in ''|*[!0-9]*) since=0 ;; esac
        if [ -n "${owner_pid}" ] && ! kill -0 "${owner_pid}" 2>/dev/null \
            && [ -n "${since}" ] && [ $(( $(date +%s) - since )) -gt 60 ]; then
            rm -rf "${LOCK_DIR}" && continue
        fi
        if [ "${waited}" -ge "${BUILD_LOCK_WAIT}" ]; then
            return 75
        fi
        sleep 5; waited=$(( waited + 5 ))
    done
}

acquire
rc=$?
if [ "${rc}" != "0" ]; then
    log "等全局构建锁超时（${BUILD_LOCK_WAIT}s）——另一构建仍在跑（看 ${LOCK_DIR}/owner）"
    exit 75
fi
trap 'rm -rf "${LOCK_DIR}"' EXIT

log "拿到全局构建锁，进全量门（tools/night/build.sh）"
bash "${SCRIPT_DIR}/../night/build.sh"
exit $?
