#!/bin/bash
# sbs 夜跑共享库：配置、构建信号量（mkdir 槽位）、hub 互斥锁、看板读写。
# 运行时状态全部落在 $HUB/build/night/（build/ 已 gitignore，对仓库零侵入）。
# 调参：写 tools/night/night.conf（KEY=VALUE，参考 night.conf.example），或环境变量覆盖。
# 兼容 macOS 自带 bash 3.2（无关联数组/mapfile）。

set -u

NIGHT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# 仓根动态解析（兼容任意检出路径）；SBS_HUB 环境变量可覆盖。
# 用 git-common-dir：hub 副本与 worktree 副本都指向主仓 .git。按 NIGHT_ROOT/../..
# 上溯 show-toplevel 在 worktree 副本里会解析出 agents/agents 歪根（2026-09-30 R1 收官事故：
# taskctl 锁/看板与 AGENTS_DIR 全部指偏）。
HUB="${SBS_HUB:-$(d="$(git -C "$NIGHT_ROOT" rev-parse --path-format=absolute --git-common-dir 2>/dev/null)" && [ -n "$d" ] && dirname "$d" || echo "")}"
[ -d "$HUB" ] || { echo "[night] 无法定位仓根（在仓库外运行？设 SBS_HUB）" >&2; exit 64; }
[ -f "$NIGHT_ROOT/night.conf" ] && . "$NIGHT_ROOT/night.conf"

CORES="$(sysctl -n hw.ncpu)"
SLOTS="${SLOTS:-2}"                                                # 构建在飞上限
JOBS="${JOBS:-$(( CORES / SLOTS > 3 ? CORES / SLOTS : 3 ))}"       # 每槽并行度
STOP_AT="${STOP_AT:-0825}"                                         # HHMM：此后不再领新任务（给 09:00 额度起点留缓冲）
WAIT_TIMEOUT="${WAIT_TIMEOUT:-1800}"                               # 队列空时脚本内部滞留秒数（期间零 API 消耗）
SEM_WAIT="${SEM_WAIT:-7200}"                                       # 等构建槽的最长秒数，超时 exit 75
NIGHT_DIR="$HUB/build/night"
SEM_DIR="$NIGHT_DIR/sem/build"
HUB_LOCK="$NIGHT_DIR/hub.lock"
STOP_FILE="$NIGHT_DIR/STOP"
# D 组已收官（看板移入 05-任务清单/已收官/）——活跃看板改为 E 组工单（2026-09-29）。
# 通用入口仍可用 SBS_KANBAN 环境变量覆盖。
KANBAN_REL="docs_local/docs/05-任务清单/E组批量效果工单.md"
KANBAN="${SBS_KANBAN:-$HUB/$KANBAN_REL}"
AGENTS_DIR="${SBS_AGENTS_DIR:-$(dirname "$HUB")/agents}"
SCENARIO_ALLOC="$NIGHT_DIR/scenario.alloc"
SCENARIO_BASE="${SCENARIO_BASE:-180}"                              # D1 已占 170（batch 组用到 168），夜跑从 180 起留出安全间隔
SCENARIO_BLOCK="${SCENARIO_BLOCK:-10}"                             # 每个任务预留的编号段长度

mkdir -p "$SEM_DIR"

# 日志一律走 stderr：stdout 留给函数返回值 / claim 输出解析
log() { printf '[night] %s\n' "$*" >&2; }

# ── 陈旧锁清理：持有者 pid 已死且目录足够老 → 移除（防崩溃后死锁）──
_is_old() { # $1=path $2=秒
    local mtime now
    mtime="$(stat -f %m "$1" 2>/dev/null || echo 0)"
    now="$(date +%s)"
    [ $(( now - mtime )) -gt "$2" ]
}
_sweep_stale() { # $1=槽位容器目录
    local d p
    for d in "$1"/*/; do
        [ -d "$d" ] || continue
        p="$(cat "$d/pid" 2>/dev/null || true)"
        if [ -n "$p" ] && kill -0 "$p" 2>/dev/null; then continue; fi
        # 无 pid 或 pid 已死：目录 60 秒以上才抢（防抢到刚建、pid 未写入的目录；
        # 60s 足够覆盖 mkdir→写 pid 的毫秒级窗口，kill -9 泄漏最多自愈一分钟）
        if _is_old "${d%/}" 60; then rm -rf "$d"; fi
    done
}

# ── 构建信号量：N 个 mkdir 槽位；stdout 返回槽号；等待超 SEM_WAIT exit 75 ──
sem_acquire() {
    local waited=0 i
    while :; do
        _sweep_stale "$SEM_DIR"
        for i in $(seq 1 "$SLOTS"); do
            if mkdir "$SEM_DIR/$i" 2>/dev/null; then
                echo $$ > "$SEM_DIR/$i/pid"
                log "拿到构建槽 $i/${SLOTS}（在飞≤${SLOTS}，本槽 -j${JOBS}）"
                echo "$i"
                return 0
            fi
        done
        if [ "$waited" -ge "$SEM_WAIT" ]; then log "等构建槽超时（${SEM_WAIT}s）"; exit 75; fi
        sleep 15; waited=$((waited + 15))
    done
}
sem_release() { rm -rf "$SEM_DIR/$1"; }

# ── hub 互斥锁：看板改+提交、worktree 创建、编号分配都在锁内（防会话间竞态）──
hub_lock() {
    local waited=0 p
    while :; do
        if mkdir "$HUB_LOCK" 2>/dev/null; then echo $$ > "$HUB_LOCK/pid"; return 0; fi
        p="$(cat "$HUB_LOCK/pid" 2>/dev/null || true)"
        if { [ -z "$p" ] || ! kill -0 "$p" 2>/dev/null; } && _is_old "$HUB_LOCK" 60; then
            rm -rf "$HUB_LOCK"; continue
        fi
        if [ "$waited" -ge 300 ]; then log "等 hub 锁超时"; return 1; fi
        sleep 2; waited=$((waited + 2))
    done
}
hub_unlock() { rm -rf "$HUB_LOCK"; }

# ── 看板操作（状态列：待领取 / 🔄 进行中 / ✅ 完成 / ⛔ 阻塞）──
kanban_pending() { # 最小序号的待领取任务，如 D2；无则空
    awk -F'|' '$4 ~ /待领取/ {k=$2; gsub(/ /,"",k); sub(/^D/,"",k); print k}' "$KANBAN" \
        | sort -n | head -1 | awk '{print "D"$1}'
}
kanban_pending_count() { awk -F'|' '$4 ~ /待领取/ {n++} END{print n+0}' "$KANBAN"; }
kanban_busy_count()   { awk -F'|' '$4 ~ /进行中/  {n++} END{print n+0}' "$KANBAN"; }
kanban_row() { # $1=Dn 原样输出该行
    awk -F'|' -v dn="$1" '{k=$2; gsub(/ /,"",k); if(k==dn){print; exit}}' "$KANBAN"
}
kanban_set() { # $1=Dn $2=状态 $3=备注（勿含 |）
    local tmp="$KANBAN.tmp$$"
    awk -F'|' -v dn="$1" -v st="$2" -v note="$3" -v OFS='|' '
        {
            k=$2; gsub(/ /,"",k);
            if (k==dn && NF>=6) { $4=" " st " "; $5=(note=="" ? " " : " " note " "); }
            print;
        }' "$KANBAN" > "$tmp" && mv "$tmp" "$KANBAN"
}
git_commit_kanban() { # $1=提交说明；只提交看板文件，不动工作区其它改动
    ( cd "$HUB" && git commit -m "$1" -- "$KANBAN_REL" ) >/dev/null 2>&1 && return 0
    sleep 3
    ( cd "$HUB" && git commit -m "$1" -- "$KANBAN_REL" ) >/dev/null 2>&1
}

# ── v2（2026-09-29）：看板操作走 tools/taskctl.py（flock+CAS，看板对脚本只读）──
# 夜跑默认打 F 板（E 组已收官；SBS_NIGHT_BOARD 可覆盖）。
NIGHT_BOARD="${SBS_NIGHT_BOARD:-F}"
taskctl() { python3 "$HUB/tools/taskctl.py" "$@"; }

v2_pending() { # 最小序号的待领取任务 id（如 F1）；无则空
    taskctl list --board "$NIGHT_BOARD" 2>/dev/null | awk '$2 ~ /待领取/ {print $1; exit}'
}
v2_pending_count() {
    taskctl list --board "$NIGHT_BOARD" 2>/dev/null | awk '$2 ~ /待领取/ {n++} END {print n+0}'
}
v2_busy_count() {
    taskctl list --board "$NIGHT_BOARD" 2>/dev/null | awk '$2 ~ /进行中/ {n++} END {print n+0}'
}
v2_claim() { # 领取（flock+CAS 在 taskctl 内部）；stdout 一行 JSON，失败非 0
    taskctl claim --board "$NIGHT_BOARD"
}
v2_done() { # $1=id $2=分支 $3=sha $4=备注
    taskctl done "$1" --board "$NIGHT_BOARD" --commit "${3:-?}" --summary "${2:-} ${4:-}"
}
v2_release() { # $1=id $2=原因
    taskctl release "$1" --board "$NIGHT_BOARD" --summary "${2:-}"
}

# ── 场景编号段分配：每任务独占一段（防多会话撞号），锁外勿调 ──
scenario_alloc() {
    local start
    [ -f "$SCENARIO_ALLOC" ] || echo "$SCENARIO_BASE" > "$SCENARIO_ALLOC"
    start="$(cat "$SCENARIO_ALLOC")"
    echo "$(( start + SCENARIO_BLOCK ))" > "$SCENARIO_ALLOC"
    echo "$start $(( start + SCENARIO_BLOCK - 1 ))"
}

# ── 终止判定：exit 0=该停了（stdout 给原因）──
check_stop() {
    if [ -f "$STOP_FILE" ]; then echo "STOP 哨兵存在（${STOP_FILE}）"; return 0; fi
    local now
    now=$((10#$(date +%H%M)))
    # 只在早晨窗口生效（STOP_AT~11:59），避免晚 20 点启动被 2000>0825 误停
    if [ "$now" -ge "$((10#$STOP_AT))" ] && [ "$now" -le 1159 ]; then
        echo "进入停止窗口（now=$(date +%H:%M) ≥ ${STOP_AT}，09:00 前留缓冲）"
        return 0
    fi
    return 1
}
