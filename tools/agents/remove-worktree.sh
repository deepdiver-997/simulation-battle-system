#!/bin/bash
# tools/agents/remove-worktree.sh — 收掉一个 agent 工作区，释放端口段 / tmp 目录 / 分配记录
#
# 用法：
#   tools/agents/remove-worktree.sh <name> [--force] [--delete-branch] [--drop-db] [--yes]
#   --force          忽略"有未提交改动 / 分支有未合并提交"的保护，强行删除
#   --delete-branch  连 feat/<name> 分支一起删（默认保留，便于事后审计）
#   --drop-db        连独立库 mail_agent_N 一起删（默认保留；配 --yes 免确认）
#
# 保护（sbs-local 的教训："没做完就别删——worktree 里未提交的东西 git 救不回来"）：
#   * worktree 有未提交改动 → 拒绝
#   * feat/<name> 相对 BASE_BRANCH 有尚未合并的提交 → 拒绝，并提示先在 hub 里 git merge --no-ff
#   两者都可用 --force 覆盖。

set -e
# shellcheck disable=SC1091
. "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/lib.sh"
agents_init

NAME=""
FORCE=0
DELETE_BRANCH=0
DROP_DB=0
YES=0
while [ $# -gt 0 ]; do
    case "$1" in
        --force)         FORCE=1; shift ;;
        --delete-branch) DELETE_BRANCH=1; shift ;;
        --drop-db)       DROP_DB=1; shift ;;
        --yes|-y)        YES=1; shift ;;
        -h|--help)       sed -n '2,/^$/p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        -*)              die "未知参数：$1" ;;
        *)               [ -z "${NAME}" ] || die "只能给一个名字"; NAME="$1"; shift ;;
    esac
done
[ -n "${NAME}" ] || die "用法：remove-worktree.sh <name> [--force] [--delete-branch] [--drop-db] [--yes]"
validate_name "${NAME}"
WT="$(worktree_path_for "${NAME}")"
BR="$(branch_for "${NAME}")"
if load_alloc "${NAME}" 2>/dev/null; then
    WT="${AGENT_WORKTREE:-${WT}}"
    BR="${AGENT_BRANCH:-${BR}}"
fi

if [ ! -e "${WT}/.git" ]; then
    warn "worktree 不存在：${WT}"
else
    # ---- 保护检查 ----
    dirty="$(git -C "${WT}" status --porcelain 2>/dev/null | wc -l | tr -d ' ')"
    ahead=0
    collected_into=""
    if git -C "${HUB}" show-ref --verify --quiet "refs/heads/${BR}"; then
        ahead="$(git -C "${HUB}" rev-list --count "${BASE_BRANCH}..${BR}" 2>/dev/null || echo 0)"
        # sbs-local 集成收割（collect.sh）：分支已汇入某 integration/* 批分支 = 已交付
        # （验收侧对批分支一次过门后合回主干），此时不算"未合并"。
        if [ "${ahead}" -gt 0 ]; then
            for ib in $(git -C "${HUB}" for-each-ref --format='%(refname:short)' 'refs/heads/integration/*'); do
                if git -C "${HUB}" merge-base --is-ancestor "${BR}" "${ib}"; then
                    ahead=0; collected_into="${ib}"; break
                fi
            done
        fi
    fi
    if [ "${FORCE}" != "1" ]; then
        if [ "${dirty}" -gt 0 ]; then
            git -C "${WT}" status --short | head -20 >&2
            die "worktree 有 ${dirty} 处未提交改动，拒绝删除。先 commit / stash，或确认要丢弃再加 --force"
        fi
        if [ "${ahead}" -gt 0 ]; then
            git -C "${HUB}" log --oneline "${BASE_BRANCH}..${BR}" | head -20 >&2
            die "分支 ${BR} 有 ${ahead} 个提交尚未合并进 ${BASE_BRANCH}，拒绝删除。
  先汇入集成批：  tools/agents/collect.sh ${NAME}
  或直合主干：    cd ${HUB} && git merge --no-ff ${BR}
  确认要丢弃：    加 --force"
        fi
    else
        [ "${dirty}" -gt 0 ] && warn "--force：丢弃 ${dirty} 处未提交改动"
        [ "${ahead}" -gt 0 ] && warn "--force：分支 ${BR} 上 ${ahead} 个未合并提交仍保留在分支里$( [ "${DELETE_BRANCH}" = "1" ] && printf '（但你同时要求删分支，它们会一起消失）' )"
    fi
    [ -n "${collected_into}" ] && printf '\033[0;34m[agents]\033[0m 已汇入 ${collected_into}，按已交付收工作区（分支保留待验收审计）\n'

    # ---- 独立库 ----
    if [ "${DROP_DB}" = "1" ] && [ -f "${WT}/config/db_config.json" ]; then
        bash "${AGENTS_LIB_DIR}/db_init.sh" --db-config "${WT}/config/db_config.json" --drop $( [ "${YES}" = "1" ] && printf -- '--yes' ) \
            || warn "删库失败，请手动处理"
    fi

    # ---- 删 worktree ----
    if [ "${FORCE}" = "1" ]; then
        git -C "${HUB}" worktree remove --force "${WT}"
    else
        git -C "${HUB}" worktree remove "${WT}"
    fi
    ok "已删除 worktree ${WT}"
fi
git -C "${HUB}" worktree prune

# ---- 释放设备磁盘账本登记 ----
bash "${AGENTS_LIB_DIR}/budget.sh" release "${WT}" 2>/dev/null || true

# ---- tmp 目录（只删我们自己前缀下的）----
if [ -n "${AGENT_TMP_ROOT:-}" ] && [[ "${AGENT_TMP_ROOT}" == ${TMP_ROOT_PREFIX}* ]] && [ -d "${AGENT_TMP_ROOT}" ]; then
    rm -rf "${AGENT_TMP_ROOT}"
    log "已清理 ${AGENT_TMP_ROOT}"
fi

# ---- 分配记录 ----
release_resources "${NAME}"
log "已释放 index/端口段/库名分配记录"

# ---- 分支 ----
if [ "${DELETE_BRANCH}" = "1" ] && git -C "${HUB}" show-ref --verify --quiet "refs/heads/${BR}"; then
    git -C "${HUB}" branch -D "${BR}" >/dev/null
    log "已删除分支 ${BR}"
elif git -C "${HUB}" show-ref --verify --quiet "refs/heads/${BR}"; then
    log "分支 ${BR} 保留（需要时 git branch -D ${BR}）"
fi
ok "完成"
