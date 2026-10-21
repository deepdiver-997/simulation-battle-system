#!/bin/bash
# tools/agents/status.sh — 一眼看清：哪些 worktree 在、各自分支状态、谁拿着构建锁、资源分配表
# 用法：tools/agents/status.sh        （在 hub 或任意 worktree 里跑都行，只读）

set -e
# shellcheck disable=SC1091
. "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/lib.sh"
agents_init

printf '\n== 多 agent 工作区状态 ==\n'
printf 'hub          : %s\n' "${HUB}"
printf 'worktrees    : %s\n' "${AGENTS_DIR}"
printf 'base branch  : %s\n' "${BASE_BRANCH}"
printf '构建锁       : %s\n' "$(build_lock_info)"
if [ -e "${STATE_DIR}/STOP" ]; then
    printf '\033[1;33mSTOP 哨兵    : 存在 —— worktree 内的新构建一律被拒（rm %s/STOP 解除）\033[0m\n' "${STATE_DIR}"
fi

printf '\n-- worktrees --\n'
printf '%-18s %-26s %-7s %-12s %-9s %-6s %-6s %s\n' "name" "branch" "dirty" "vs ${BASE_BRANCH}" "build" "idx" "ports" "db"
seen=" "
wt_path=""; wt_branch=""
emit() {
    local path="$1" branch="$2" name dirty counts behind ahead bsize idx pbase db
    [ -n "${path}" ] || return 0
    [ "${path}" != "${HUB}" ] || return 0
    name="$(basename "${path}")"
    seen="${seen}${name} "
    if [ -d "${path}" ]; then
        dirty="$(git -C "${path}" status --porcelain 2>/dev/null | wc -l | tr -d ' ')"
    else
        dirty="missing"
    fi
    if [ -n "${branch}" ] && git -C "${HUB}" show-ref --verify --quiet "refs/heads/${branch}"; then
        counts="$(git -C "${HUB}" rev-list --left-right --count "${BASE_BRANCH}...${branch}" 2>/dev/null || echo '? ?')"
        behind="${counts%%[[:space:]]*}"; ahead="${counts##*[[:space:]]}"
        counts="-${behind}/+${ahead}"
    else
        counts="detached"
    fi
    if [ -d "${path}/build" ]; then bsize="$(du -sh "${path}/build" 2>/dev/null | cut -f1)"; else bsize="-"; fi
    idx="-"; pbase="-"; db="-"
    if load_alloc "${name}" 2>/dev/null; then
        idx="${AGENT_INDEX}"; pbase="${AGENT_PORT_BASE}"; db="${AGENT_DB}"
    fi
    printf '%-18s %-26s %-7s %-12s %-9s %-6s %-6s %s\n' "${name}" "${branch:-?}" "${dirty}" "${counts}" "${bsize}" "${idx}" "${pbase}" "${db}"
}
while IFS= read -r line; do
    case "${line}" in
        "worktree "*) emit "${wt_path}" "${wt_branch}"; wt_path="${line#worktree }"; wt_branch="" ;;
        "branch refs/heads/"*) wt_branch="${line#branch refs/heads/}" ;;
        "detached") wt_branch="" ;;
    esac
done < <(git -C "${HUB}" worktree list --porcelain)
emit "${wt_path}" "${wt_branch}"

orphans=""
for f in $(alloc_records); do
    n="$(basename "${f}" .env)"
    case "${seen}" in *" ${n} "*) ;; *) orphans="${orphans} ${n}" ;; esac
done
if [ -n "${orphans}" ]; then
    printf '\n\033[1;33m孤儿分配记录（有资源记录、无 worktree）：%s\033[0m\n  释放：tools/agents/remove-worktree.sh <name>\n' "${orphans}"
fi

printf '\n-- 设备磁盘预算（所有项目共享一份，~/.agent-workspaces/）--\n'
bash "${AGENTS_LIB_DIR}/budget.sh" show 2>&1 | sed 's/^/  /'

printf '\n-- 无头会话（tools/agents/spawn.sh）--\n'
if ls "${STATE_DIR}/runs"/*/current >/dev/null 2>&1; then
    printf '%-14s %-8s %-10s %-8s %-22s %s\n' "name" "harness" "状态" "exit" "session" "最新输出"
    for d in "${STATE_DIR}/runs"/*/current; do
        [ -L "${d}" ] || continue
        n="$(basename "$(dirname "${d}")")"
        rd="$(readlink "${d}")"
        pid="$(alloc_field "${rd}/meta" PID)"
        if [ -n "${pid}" ] && kill -0 "${pid}" 2>/dev/null; then st="运行中"; else st="已结束"; fi
        ec="$(cat "${rd}/exit" 2>/dev/null || printf '-')"
        sid="$(grep -oE '"(session_id|thread_id|conversation_id)": *"[^"]*"' "${rd}/stream.log" 2>/dev/null | head -1 | cut -d'"' -f4)"
        last="$(tail -n 1 "${rd}/stream.log" 2>/dev/null | cut -c1-60)"
        printf '%-14s %-8s %-10s %-8s %-22s %s\n' "${n}" "$(alloc_field "${rd}/meta" HARNESS)" "${st}" "exit=${ec}" \
            "${sid:+$(printf '%.18s…' "${sid}")}${sid:-$(printf '%.18s' "$(alloc_field "${rd}/meta" STARTED)")}" "${last}"
    done
    printf '  详情: tools/agents/spawn.sh status <name>   续话: spawn.sh resume <name> "指令"\n'
else
    printf '  （没有派发记录）\n'
fi

printf '\n-- 最近构建（.git/agents/build-history.tsv）--\n'
if [ -f "${STATE_DIR}/build-history.tsv" ]; then
    tail -n 6 "${STATE_DIR}/build-history.tsv" | awk -F'\t' '{ n=split($2,a,"/"); printf "  %s  %-16s %-28s %-6s %s\n", $1, a[n], $3, $4, $5 }'
else
    printf '  （还没有记录）\n'
fi

if command -v ccache >/dev/null 2>&1; then
    printf '\n-- ccache --\n'
    ccache -s 2>/dev/null | grep -iE 'hits|misses|cache size|cacheable|local storage' | head -6 | sed 's/^/  /'
fi

if [ -d "${AGENTS_DIR}" ]; then
    printf '\n-- 磁盘 --\n  %s\n' "$(du -sh "${AGENTS_DIR}" 2>/dev/null | tr '\t' ' ')"
fi
printf '\n'
