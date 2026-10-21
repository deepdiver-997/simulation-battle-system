#!/bin/bash
# 领取：hub 看板上最小序号的【待领取】任务。领取动作 = 锁内「翻状态 + 提交 hub + 分配场景编号段 + 建 worktree + cmake 配置」。
# 用法：claim.sh <会话名> [--wait]
#   --wait  队列空时在本进程内滞留轮询（sleep 阻塞，零 API 消耗），直到有任务/到停止点
# 退出码：0 已领取（stdout 有 CLAIMED 块）｜0 且输出 ALL_DONE 全部完成｜1 队列空且未 --wait
#         2 触发停止条件（输出 STOP 原因）｜3 滞留超时仍无任务（可再次调用）｜>=8 基础设施故障
set -u
. "$(dirname "$0")/lib.sh"

# 会话名：优先用传入值；否则用已领取的 agent ID（SBS_AGENT_ID）——保证看板行的
# worker 字段与会话注册表对得上（2026-10-13 协议改造）。
SESSION="${1:-${SBS_AGENT_ID:-night-x}}"
if [ -z "${1:-}" ] && [ -n "${SBS_AGENT_ID:-}" ]; then
    log "会话名沿用已领取的 agent ID：${SESSION}"
fi
# TASKCTL_WORKER 与 SESSION 对齐：不设的话 taskctl 会退化用 `zhr@pidNNN`，
# 与注册表 agent ID 对不上 → 属主校验/ busy 登记全部失效（2026-10-21）。
[ -n "${SBS_AGENT_ID:-}" ] && export TASKCTL_WORKER="${TASKCTL_WORKER:-$SBS_AGENT_ID}"
WAIT=0; [ "${2:-}" = "--wait" ] && WAIT=1

claim_one() { # rc: 0=已领取 1=队列空 9=锁失败 8=worktree 失败 7=提交失败
    # v2：领取 = taskctl claim（内部 flock+CAS+提交，看板行即登记）。
    local out dn wt br row
    if ! out="$(v2_claim)"; then
        log "taskctl claim 失败（被抢/无任务）"; return 1
    fi
    dn="$(printf '%s' "$out" | python3 -c 'import json,sys; print(json.load(sys.stdin)["id"])')"
    wt="$AGENTS_DIR/$(printf '%s' "$out" | python3 -c 'import json,sys; print(json.load(sys.stdin)["id"].lower())')"
    br="$(printf '%s' "$out" | python3 -c 'import json,sys; print(json.load(sys.stdin)["branch"])')"
    row="$(printf '%s' "$out" | python3 -c 'import json,sys; print(json.load(sys.stdin)["row"])')"
    # worktree（taskctl 已登记，这里建实体）：目录在→复用；分支在→挂回；否则 -b 新建
    #
    # ⚠️ 2026-10-21 抢单事故修：**目录已存在不等于"这单没人做"**——上一会话
    #   可能正在这个 worktree 里写文件。复用前先问注册表"有没有活着的属主"：
    #   有 → 退回这一单（不抢），等对方收工或心跳超时。
    #   （判据在 taskctl claim 内部也做了一道；这里是第二道闸，防绕路。）
    if [ -e "$wt/.git" ]; then
        live_owner="$(python3 "$NIGHT_ROOT/agent_id.py" owner-of "$dn" 2>/dev/null | head -1)"
        if [ -n "$live_owner" ] && [ "$live_owner" != "$SESSION" ]; then
            v2_release "$dn" "worktree 在用（属主 $live_owner 活着），自动退回"
            log "worktree 在用且属主 $live_owner 活着 → 退回 $dn，不抢占"
            return 1
        fi
        log "worktree 已存在，复用：$wt"
    elif ! git -C "$HUB" worktree add -b "$br" "$wt" main-local >/dev/null 2>&1 \
       && ! git -C "$HUB" worktree add "$wt" "$br" >/dev/null 2>&1; then
        v2_release "$dn" "worktree 创建失败，自动退回"
        log "worktree 创建失败：$wt"; return 8
    fi
    # 数据软链（同提示词模板；scripts/ 不入库，worktree 需自建）
    mkdir -p "$wt/scripts"
    ln -sfn "$HUB/scripts/data" "$wt/scripts/data"
    # cmake 配置（冷目录才配；有 ccache 则挂上，后续 worktree 冷构建近免费）
    if [ ! -f "$wt/build/CMakeCache.txt" ]; then
        if command -v ccache >/dev/null 2>&1; then
            ( cd "$wt" && cmake -B build -DCMAKE_CXX_COMPILER_LAUNCHER=ccache ) >/dev/null 2>&1
        else
            ( cd "$wt" && cmake -B build ) >/dev/null 2>&1
        fi
    fi
    echo "CLAIMED $dn"
    echo "worktree: $wt"
    echo "branch:   $(git -C "$wt" rev-parse --abbrev-ref HEAD 2>/dev/null || echo "$br")"
    echo "scenarios: ${alloc% *}-${alloc#* }"
    echo "row: $row"
    return 0
}

while :; do
    claim_one; rc=$?
    if [ "$rc" -eq 0 ]; then exit 0; fi
    if [ "$rc" -ge 7 ]; then sleep 60; continue; fi   # 基础设施故障：歇一会重试
    # rc=1 队列空
    if [ "$(v2_pending_count)" -eq 0 ] && [ "$(v2_busy_count)" -eq 0 ]; then
        echo "ALL_DONE 看板已无待领取且无进行中任务——全部完成"
        exit 0
    fi
    if [ "$WAIT" -eq 0 ]; then
        echo "队列空（或有他人进行中）"; exit 1
    fi
    if reason="$(check_stop)"; then echo "STOP $reason"; exit 2; fi
    log "队列空，滞留轮询 ${WAIT_TIMEOUT}s（此间无 API 消耗）…"
    waited=0
    while [ "$waited" -lt "$WAIT_TIMEOUT" ]; do
        sleep 30; waited=$((waited + 30))
        if reason="$(check_stop)"; then echo "STOP $reason"; exit 2; fi
        if [ "$(v2_pending_count)" -eq 0 ] && [ "$(v2_busy_count)" -eq 0 ]; then
            echo "ALL_DONE 看板已清空——全部完成"; exit 0
        fi
        [ -n "$(v2_pending)" ] && break
    done
    # 超时自然回到外层再试一轮（每轮 WAIT_TIMEOUT 才醒来一次，开销极小）
done
