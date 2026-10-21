#!/bin/bash
# tools/night/fuzz_night.sh — 夜间 fuzz 战役挂件（2026-10-02）
#
# 自包含、cwd 无关、不污染主工作区：
#   - 专用 worktree agents/night-fuzz（detach 到 main-local）：ASan 插件 deploy
#     只写进该 worktree 的 resources/*_lib，主工作区部署永不被 sanitizer 污染；
#   - SBS_FUZZ_SANITIZERS=ON 统一消毒构建（增量，缓存跨夜复用）；
#   - 随机 seed 区间分块战役；块失败（崩溃/UB）= 发现 → 归档完整日志+seed 后继续
#     下一块（发现不清零，战役不中断）；
#   - 摘要落 build/night/fuzz/last_night.txt，崩溃归档在 build/night/fuzz/crash-*/
#
# 用法：
#   fuzz_night.sh                # 默认 3 块 × 2000 seed（约 8-12 分钟）
#   fuzz_night.sh --smoke        # 1 块 × 60 seed，验证管线连通
#   fuzz_night.sh --chunks 6     # 自定块数
#
# cron 建议：每晚 02:30 跑默认档；晨间 grep last_night.txt + crash-* 目录即晨报。
# ⚠️ 与 agent 夜跑协议（claim/build/finish）互不依赖；构建并发高时自行错峰。

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(dirname "$(dirname "$SCRIPT_DIR")")"
WT="$REPO/../agents/night-fuzz"
LOGDIR="$REPO/build/night/fuzz"
SHELL_BIN=/bin/bash

CHUNKS=3
CHUNK_SEEDS=2000
if [[ "${1:-}" == "--smoke" ]]; then
    CHUNKS=1; CHUNK_SEEDS=60
elif [[ "${1:-}" == "--chunks" && -n "${2:-}" ]]; then
    CHUNKS=$2
fi

mkdir -p "$LOGDIR"
STAMP="$(date +%Y%m%d-%H%M%S)"
NIGHT_LOG="$LOGDIR/last_night.txt"
: > "$NIGHT_LOG"
log() { echo "$@" | tee -a "$NIGHT_LOG"; }

log "[fuzz-night] $STAMP repo=$REPO chunks=$CHUNKS x $CHUNK_SEEDS seeds"

# ── 1. worktree 就位（detach 到 main-local，丢弃残留调试态）─────────────────
if [[ -d "$WT" ]]; then
    git -C "$WT" checkout --detach --force main-local >/dev/null 2>&1 \
        || { log "[fuzz-night] FATAL: worktree reset failed"; exit 9; }
else
    git -C "$REPO" worktree add --detach "$WT" main-local >/dev/null 2>&1 \
        || { log "[fuzz-night] FATAL: worktree add failed"; exit 9; }
fi
# 机器本地软链（不入库）
[[ -e "$WT/scripts" ]] || ln -s /Users/zhuhongrui/code/c++/project/Simulation-battle-system/scripts "$WT/scripts"
[[ -e "$WT/reference" ]] || ln -s /Users/zhuhongrui/code/c++/project/Simulation-battle-system/reference "$WT/reference"

# ── 2. 统一消毒构建（增量）───────────────────────────────────────────────
cmake -S "$WT" -B "$WT/build-fuzz" -DSBS_FUZZ_SANITIZERS=ON > /dev/null 2>&1 \
    || { log "[fuzz-night] FATAL: configure failed"; exit 9; }
BUILD_OUT=$(cmake --build "$WT/build-fuzz" --target sim_fuzz_battle \
    moves_plugin_deploy soul_plugin_deploy suit_plugin_deploy -j 8 2>&1)
if [[ $? -ne 0 ]]; then
    log "[fuzz-night] FATAL: build failed（尾部输出见 $LOGDIR/build_fail.log）"
    echo "$BUILD_OUT" | tail -30 > "$LOGDIR/build_fail.log"
    exit 9
fi
BIN="$WT/build-fuzz/sim_fuzz_battle"
[[ -x "$BIN" ]] || { log "[fuzz-night] FATAL: binary missing"; exit 9; }

# ── 3. 分块战役 ─────────────────────────────────────────────────────────
export ASAN_OPTIONS=detect_container_overflow=1
export UBSAN_OPTIONS=print_stacktrace=1
TOTAL_SEEDS=0; TOTAL_FAULT=0; CRASHES=0

for i in $(seq 1 "$CHUNKS"); do
    FROM=$(( (RANDOM * 32768 + RANDOM) % 900000 ))
    CHUNK_LOG="$LOGDIR/chunk_${STAMP}_${i}.log"
    ( cd "$WT" && "$BIN" --from "$FROM" --count "$CHUNK_SEEDS" ) > "$CHUNK_LOG" 2>&1
    RC=$?
    SUMMARY=$(grep -oE "SBS_FUZZ_SUMMARY.*" "$CHUNK_LOG" | tail -1)
    FAULT_SEEDS=$(grep -oE "SBS_FUZZ_ANY_CAST_FAULT_SEEDS.*" "$CHUNK_LOG" | tail -1)
    SEEDS_DONE=$(echo "$SUMMARY" | grep -oE "seeds=[0-9]+" | cut -d= -f2)
    FAULTS=$(echo "$SUMMARY" | grep -oE "any_cast_fault=[0-9]+" | cut -d= -f2)
    SEEDS_DONE=${SEEDS_DONE:-0}; FAULTS=${FAULTS:-0}
    TOTAL_SEEDS=$((TOTAL_SEEDS + SEEDS_DONE)); TOTAL_FAULT=$((TOTAL_FAULT + FAULTS))
    if [[ $RC -ne 0 ]]; then
        CRASHES=$((CRASHES + 1))
        CRASH_DIR="$LOGDIR/crash-${STAMP}_${i}"
        mkdir -p "$CRASH_DIR"
        cp "$CHUNK_LOG" "$CRASH_DIR/full.log"
        FAILLINE=$(grep -oE "SBS_FUZZ_FAIL seed=[0-9]+ reason=[^ ]+|SBS_FUZZ_HANG seed=[0-9]+" "$CHUNK_LOG" | tail -1)
        log "[fuzz-night] CHUNK $i CRASH rc=$RC $FAILLINE → $CRASH_DIR"
    else
        log "[fuzz-night] CHUNK $i ok from=$FROM $SUMMARY"
    fi
    [[ -n "$FAULT_SEEDS" ]] && log "[fuzz-night]   $FAULT_SEEDS"
done

log "[fuzz-night] TOTAL seeds=$TOTAL_SEEDS any_cast_fault=$TOTAL_FAULT crash_chunks=$CRASHES"
if [[ $CRASHES -gt 0 ]]; then
    log "[fuzz-night] ⚠️ 有 $CRASHES 个块崩溃——crash-* 目录含完整日志与 seed，优先排查"
    exit 1
fi
exit 0
