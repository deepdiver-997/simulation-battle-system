#!/bin/bash
# 限流构建 + 全量场景回归。在 worktree 根目录调用（或 WORKTREE=路径 环境变量指定）。
# 构建在飞数由信号量限到 SLOTS（默认 2）：排队阻塞发生在本脚本这一次工具调用内，不消耗 API。
# 退出码：0 全过｜1 编译失败｜2 回归失败｜3 registry dump 失败｜75 等槽超时
set -u
. "$(dirname "$0")/lib.sh"

WT="${WORKTREE:-$PWD}"
[ -f "$WT/build/CMakeCache.txt" ] || { log "$WT 下没有 build/——先跑 claim.sh"; exit 64; }
cd "$WT"

slot="$(sem_acquire)" || exit $?
trap 'sem_release "$slot"' EXIT

log "开始构建 $(basename "$WT")（-j${JOBS}）"
if ! cmake --build build -j"$JOBS" > build/night-build.log 2>&1; then
    tail -40 build/night-build.log
    log "编译失败，完整日志：$WT/build/night-build.log"
    exit 1
fi
log "构建通过，跑全量场景回归…"

fails=0; total=0
: > build/night-regression-fails.txt
for bin in build/sim_scenario_*; do
    total=$((total + 1))
    if ! "./$bin" >/dev/null 2>&1; then
        echo "FAIL $(basename "$bin")" >> build/night-regression-fails.txt
        fails=$((fails + 1))
    fi
done
if [ "$fails" -gt 0 ]; then
    log "回归失败 $fails/${total}（明细 build/night-regression-fails.txt）："
    cat build/night-regression-fails.txt
    exit 2
fi
if ! ./build/sim_server --dump-registry-exit build/registry.json; then
    log "registry dump 失败"; exit 3
fi
log "ALL PASS：$total 个场景回归 + registry dump OK"
