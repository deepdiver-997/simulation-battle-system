#!/bin/bash
# 领取：hub 看板上最小序号的【待领取】任务。领取动作 = 锁内「翻状态 + 提交 hub + 分配场景编号段 + 建 worktree + cmake 配置」。
# 用法：claim.sh <会话名> [--wait]
#   --wait  队列空时在本进程内滞留轮询（sleep 阻塞，零 API 消耗），直到有任务/到停止点
# 退出码：0 已领取（stdout 有 CLAIMED 块）｜0 且输出 ALL_DONE 全部完成｜1 队列空且未 --wait
#         2 触发停止条件（输出 STOP 原因）｜3 滞留超时仍无任务（可再次调用）｜>=8 基础设施故障
set -u
. "$(dirname "$0")/lib.sh"

SESSION="${1:-night-x}"
WAIT=0; [ "${2:-}" = "--wait" ] && WAIT=1

claim_one() { # rc: 0=已领取 1=队列空 9=锁失败 8=worktree 失败 7=提交失败
    hub_lock || return 9
    local dn row alloc note wt br
    dn="$(kanban_pending)"
    if [ -z "$dn" ]; then hub_unlock; return 1; fi
    alloc="$(scenario_alloc)"
    row="$(kanban_row "$dn")"
    note="worktree $AGENTS_DIR/d-${dn}，分支 feat/d-${dn}（$SESSION 领取于 $(date +%H:%M)，场景编号 ${alloc% *}-${alloc#* }）"
    kanban_set "$dn" "🔄 进行中" "$note"
    if ! git_commit_kanban "chore(看板): $dn 领取（${SESSION}，场景编号 ${alloc% *}-${alloc#* }）"; then
        ( cd "$HUB" && git checkout -- "$KANBAN_REL" )   # 回滚看板，登记失败不占号
        hub_unlock; log "看板提交失败（回滚，稍后重试）"; return 7
    fi
    # worktree（锁内建，防 git 元数据竞态）：目录在→复用；分支在（上次残留）→挂回；否则 -b 新建
    wt="$AGENTS_DIR/d-$dn"; br="feat/d-$dn"
    if [ -e "$wt/.git" ]; then
        log "worktree 已存在，复用：$wt"
    elif ! git -C "$HUB" worktree add -b "$br" "$wt" main-local >/dev/null 2>&1 \
       && ! git -C "$HUB" worktree add "$wt" "$br" >/dev/null 2>&1; then
        hub_unlock; log "worktree 创建失败：$wt"; return 8
    fi
    hub_unlock
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
    if [ "$(kanban_pending_count)" -eq 0 ] && [ "$(kanban_busy_count)" -eq 0 ]; then
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
        if [ "$(kanban_pending_count)" -eq 0 ] && [ "$(kanban_busy_count)" -eq 0 ]; then
            echo "ALL_DONE 看板已清空——全部完成"; exit 0
        fi
        [ -n "$(kanban_pending)" ] && break
    done
    # 超时自然回到外层再试一轮（每轮 WAIT_TIMEOUT 才醒来一次，开销极小）
done
