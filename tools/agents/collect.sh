#!/bin/bash
# tools/agents/collect.sh — 集成收割器：agent 把自己过完门的 feat/<name> 汇入当日集成批分支
#
# 为什么存在（sbs-local 夜跑教训）：N 个 agent 一夜产出 N 个待审核分支 + N 个 5.6G
# 工作区，验收侧要 N 次合并 + N 次全量门。收敛为：**agent 完成即自汇入共享的
# integration/<日期> 批分支**（冲突在制造者手里、上下文最新时解决），验收侧对批分支
# **一次过门 + 一次合并进主干**；工作区当场可收（remove-worktree 认账 integration/*）。
#
# 用法：
#   tools/agents/collect.sh <name>            # feat/<name> --no-ff 汇入 integration/<今天>
#   tools/agents/collect.sh <name> --into <分支名>   # 指定批分支（如跨夜收官批）
#   tools/agents/collect.sh status            # 批分支内容清单（分支数/领先主干数/工作区状态）
#
# 机制：共享 staging 工作区（<工作区根>/.staging，检出到批分支，首次自动创建）+
# .git/agents/integration.lock 串行化（同刻只有一次汇入在跑）。冲突时不替你猜——
# 打印冲突文件与手动解法退出 1，解完重跑本命令即续。
set -e
# shellcheck disable=SC1091
. "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/lib.sh"
agents_init

LOCK_DIR="${STATE_DIR}/integration.lock"
STAGING="${AGENTS_DIR}/.staging"

usage() { sed -n '2,/^$/p' "$0" | sed 's/^# \{0,1\}//'; }
log()  { printf '\033[0;34m[collect]\033[0m %s\n' "$*"; }
die()  { printf '\033[0;31m[collect]\033[0m %s\n' "$*" >&2; exit 1; }

cmd="${1:-}"
if [ "${cmd}" = "status" ]; then
    shift
    BATCH="$(git -C "${HUB}" for-each-ref --format='%(refname:short)' 'refs/heads/integration/*' | sort | tail -1)"
    [ -n "${BATCH}" ] || { log "还没有任何 integration/* 批分支"; exit 0; }
    log "最新批分支：${BATCH}（基于 ${BASE_BRANCH}）"
    git -C "${HUB}" log --oneline "${BASE_BRANCH}..${BATCH}" | head -30
    log "已汇入分支数：$(git -C "${HUB}" branch --merged "${BATCH}" | grep -c 'feat/')；领先 ${BASE_BRANCH} $(git -C "${HUB}" rev-list --count "${BASE_BRANCH}..${BATCH}") 个提交"
    exit 0
fi

NAME="${1:-}"; [ -n "${NAME}" ] || { usage; exit 1; }
shift || true
BATCH=""
while [ $# -gt 0 ]; do
    case "$1" in
        --into) BATCH="$2"; shift 2 ;;
        -h|--help) usage; exit 0 ;;
        *) die "未知参数：$1" ;;
    esac
done
BATCH="${BATCH:-integration/$(date +%Y-%m-%d)}"
BR="feat/${NAME}"
git -C "${HUB}" show-ref --verify --quiet "refs/heads/${BR}" || die "分支 ${BR} 不存在"

# ---- 串行锁 ----
waited=0
until mkdir "${LOCK_DIR}" 2>/dev/null; do
    [ "${waited}" -ge 120 ] && die "等 integration.lock 超时（2 分钟）——另一汇入在跑，稍后重试"
    sleep 2; waited=$(( waited + 2 ))
done
echo "$$ ${NAME} $(date +%s)" > "${LOCK_DIR}/owner"
trap 'rm -rf "${LOCK_DIR}"' EXIT

# ---- staging 工作区（首次创建）----
if [ ! -e "${STAGING}/.git" ]; then
    if git -C "${HUB}" show-ref --verify --quiet "refs/heads/${BATCH}"; then
        git -C "${HUB}" worktree add "${STAGING}" "${BATCH}" >/dev/null
    else
        git -C "${HUB}" worktree add "${STAGING}" -b "${BATCH}" "${BASE_BRANCH}" >/dev/null
    fi
    log "staging 工作区就绪：${STAGING}（批分支 ${BATCH}）"
else
    # 已存在：若停在别批分支上则切到目标批
    cur="$(git -C "${STAGING}" branch --show-current)"
    if [ "${cur}" != "${BATCH}" ]; then
        git -C "${STAGING}" status --porcelain | head -1 | grep -q . && \
            die "staging 有未提交改动（停在 ${cur}），先处理再汇入"
        if git -C "${HUB}" show-ref --verify --quiet "refs/heads/${BATCH}"; then
            git -C "${STAGING}" checkout -q "${BATCH}"
        else
            git -C "${STAGING}" checkout -q -b "${BATCH}" "${BASE_BRANCH}"
        fi
    fi
fi
[ -z "$(git -C "${STAGING}" status --porcelain | grep -v '^??')" ] || \
    die "staging 有未提交改动（含冲突中间态？），先处理：cd ${STAGING} && git status"

# ---- 汇入 ----
if git -C "${STAGING}" merge --no-ff --no-edit "${BR}" >/dev/null 2>&1; then
    log "已汇入：${BR} → ${BATCH}（--no-ff）"
    git -C "${STAGING}" log --oneline -1
    log "批分支现领先 ${BASE_BRANCH} $(git -C "${STAGING}" rev-list --count "${BASE_BRANCH}..${BATCH}") 个提交"
    log "工作区可收：tools/agents/remove-worktree.sh ${NAME}（integration/* 已认账）"
    log "验收：cd ${STAGING} && tools/agents/gate.sh 一次过门 → 合回 ${BASE_BRANCH}"
    exit 0
fi

# ---- 冲突：配置/构建面自动裁决（ours = 批分支基线 = main-local 侧优先）----
# 判据：冲突文件属于**集成侧管辖面**（.gitignore / CMakeLists.txt / CLAUDE.md /
# README.md / docs_local/）→ 一律取 ours（基线侧更新），因为：
#   · 内核与构建配置由集成会话维护，agent 分支上的那份是**旧基线**（分支切出时
#     的快照），不是"agent 的产出"——取 theirs 会把集成侧的新配置回退掉；
#   · agent 的产出面是 resources/（效果实现）与 test/scenario/（场景），这两处
#     冲突会**如实报给人**（不自动解——那是真撞号/真逻辑冲突）。
# 其余冲突（效果/场景/内核代码）→ 不猜，报解法。
git -C "${STAGING}" merge --no-ff --no-edit "${BR}" >/dev/null 2>&1 || {
    conflict_files="$(git -C "${STAGING}" diff --name-only --diff-filter=U 2>/dev/null)"
    auto_resolved=""
    remaining=""
    for f in $conflict_files; do
        case "$f" in
            .gitignore|CMakeLists.txt|CLAUDE.md|README.md|docs_local/*)
                git -C "${STAGING}" checkout -q --ours "$f" 2>/dev/null && \
                    git -C "${STAGING}" add "$f" && auto_resolved="$auto_resolved $f" ;;
            *) remaining="$remaining $f" ;;
        esac
    done
    if [ -n "$auto_resolved" ] && [ -z "$remaining" ]; then
        git -C "${STAGING}" commit -q --no-edit
        log "配置面冲突已自动裁决（取基线侧）:${auto_resolved}"
        log "已汇入：${BR} → ${BATCH}（--no-ff）"
        git -C "${STAGING}" log --oneline -1
        log "批分支现领先 ${BASE_BRANCH} $(git -C "${STAGING}" rev-list --count "${BASE_BRANCH}..${BATCH}") 个提交"
        log "验收：cd ${STAGING} && tools/agents/gate.sh 一次过门 → 合回 ${BASE_BRANCH}"
        exit 0
    fi
    # 仍有真冲突 → 回滚到干净态并报解法
    git -C "${STAGING}" merge --abort 2>/dev/null || true
    conflicts="$(git -C "${STAGING}" diff --name-only --diff-filter=U 2>/dev/null | head -10)"
    [ -z "$conflicts" ] && conflicts="(merge 失败但无冲突文件——看 ${STAGING} 状态)"
    die "汇入冲突：${BR} × ${BATCH}
冲突文件：
${conflicts}
手动解法（在你的上下文里解，别在验收时爆）：
  cd ${STAGING}
  git merge --no-ff ${BR}        # 重放冲突
  # 逐文件解决后 git add → git commit --no-edit
  # 解完可再跑本命令核对
判例参考：docs_local/docs/06-交接/合并记录-*.md（看板 md 冲突按行级 ✅ 优先裁决）"
}
