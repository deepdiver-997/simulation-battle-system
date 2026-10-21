#!/bin/bash
# tools/agents/new-worktree.sh — 给一个 agent 开独立工作区：worktree + 配置播种 + 独占资源
#
# 用法：
#   tools/agents/new-worktree.sh <name> [--from <ref>] [--build <Debug|Release|SafeRelease|Asan>]
#                                       [--no-db] [--no-test-env]
#   <name>        小写字母/数字/._-，例如 smtp-dedup、k2、refactor-fsm
#   --from <ref>  基准分支/提交（默认 agents.conf 的 BASE_BRANCH，即 main）
#   --build <T>   建完顺手构建一次（走 tools/agents/gate.sh 全量门，自动排队全局构建锁；
#                 <T> 仅作标记记录，sbs 构建类型由 CMake 预设决定）
#   --no-db       不建独立 MySQL 库
#   --no-test-env 不生成 test/config 测试配置
#
# 做了什么（顺序）：
#   1. git worktree add ${AGENTS_DIR}/<name> -b feat/<name> <ref>（已存在则复用）
#   2. 把 hub 里 gitignored 但存在的 config/ 文件（db_config.json、smtpsConfig.json、证书、DKIM…）
#      拷进 worktree；缺证书则生成自签证书。只拷 config/（构建产物与运行时目录不在拷贝范围）
#   3. 分配 AGENT_INDEX / 端口段 / tmp 根目录 / 库名 → 写 <worktree>/.agent-env
#   4. apply_agent_env.py 把这些写进 worktree 自己的 config/*.json、test/config/*_test.json 副本；
#      被跟踪的那几份标 skip-worktree，端口改动不会出现在 git status、不会被误提交
#   5. 用独占端口/目录重新生成 test/config 测试配置（setup_test_env.py）
#   6. 建独立库 mail_agent_N 并建表（db_init.sh；失败只警告）
#   7. 可选 --build

set -e
# shellcheck disable=SC1091
. "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/lib.sh"
agents_init

NAME=""
FROM=""
BUILD_TYPE=""
NO_DB=0
NO_TEST_ENV=0
while [ $# -gt 0 ]; do
    case "$1" in
        --from)        FROM="$2"; shift 2 ;;
        --build)       BUILD_TYPE="$2"; shift 2 ;;
        --no-db)       NO_DB=1; shift ;;
        --no-test-env) NO_TEST_ENV=1; shift ;;
        -h|--help)     sed -n '2,/^$/p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        -*)            die "未知参数：$1" ;;
        *)             [ -z "${NAME}" ] || die "只能给一个名字：'${NAME}' 与 '$1'"; NAME="$1"; shift ;;
    esac
done
[ -n "${NAME}" ] || die "用法：new-worktree.sh <name> [--from <ref>] [--build <type>] [--no-db] [--no-test-env]"
validate_name "${NAME}"
FROM="${FROM:-${BASE_BRANCH}}"
WT="$(worktree_path_for "${NAME}")"
BR="$(branch_for "${NAME}")"

# ---- 设备级磁盘预算（生产修复 #1）：没设预算或余量不足，一律不开工 ----
# 账本是全设备共享的（~/.agent-workspaces/），多个项目不会各自领一份额度。
BUDGET_TOOL="${AGENTS_LIB_DIR}/budget.sh"
DISK_RESERVE_GB="${DISK_RESERVE_GB:-5}"
budget_rc=0
budget_msg="$(bash "${BUDGET_TOOL}" check "${DISK_RESERVE_GB}")" || budget_rc=$?
if [ "${budget_rc}" != "0" ]; then
    printf '%s\n' "${budget_msg}" >&2
    die "磁盘预算检查未通过(rc=${budget_rc})。由人工设定：bash ${BUDGET_TOOL} set <GB>（参考值：bash ${BUDGET_TOOL} suggest）"
fi
log "磁盘预算 OK：${budget_msg}"

log "hub        : ${HUB}"
log "worktree   : ${WT}"
log "branch     : ${BR}（基于 ${FROM}）"

if hub_has_uncommitted; then
    warn "hub 有未提交改动：worktree 只包含 ${FROM} 已提交的内容，不含这些改动"
fi

# ---- 1. worktree ----
mkdir -p "${AGENTS_DIR}"
if [ -e "${WT}/.git" ]; then
    log "worktree 已存在，复用"
elif [ -e "${WT}" ]; then
    die "${WT} 已存在但不是 worktree，请先处理掉这个目录"
elif git -C "${HUB}" show-ref --verify --quiet "refs/heads/${BR}"; then
    log "分支 ${BR} 已存在，挂到新 worktree"
    git -C "${HUB}" worktree add "${WT}" "${BR}"
else
    git -C "${HUB}" rev-parse --verify --quiet "${FROM}^{commit}" >/dev/null || die "基准 '${FROM}' 不存在"
    git -C "${HUB}" worktree add -b "${BR}" "${WT}" "${FROM}"
fi

# ---- 2. 播种 gitignored 配置 ----
log "播种 hub 里 gitignored 的 config/ 文件"
seeded=0
while IFS= read -r rel; do
    [ -n "${rel}" ] || continue
    case "${rel}" in
        *.DS_Store|*.bak|*.old|*.orig|*~|*/__pycache__/*) continue ;;
    esac
    src="${HUB}/${rel}"
    dst="${WT}/${rel}"
    [ -f "${src}" ] || continue
    if [ ! -e "${dst}" ]; then
        mkdir -p "$(dirname "${dst}")"
        cp -p "${src}" "${dst}"
        seeded=$(( seeded + 1 ))
    fi
done < <(git -C "${HUB}" ls-files --others --ignored --exclude-standard -- config/)
log "拷贝了 ${seeded} 个文件"

# ---- 3. 资源分配 ----
if load_alloc "${NAME}" 2>/dev/null; then
    log "已有资源分配记录（index=${AGENT_INDEX} port_base=${AGENT_PORT_BASE} db=${AGENT_DB}），复用"
else
    alloc_resources "${NAME}" "${WT}" "${BR}"
    ok "分配 index=${AGENT_INDEX} 端口段=${AGENT_PORT_BASE}-$(( AGENT_PORT_BASE + PORT_BLOCK_SIZE - 1 )) tmp=${AGENT_TMP_ROOT} db=${AGENT_DB}"
fi
cp "$(alloc_record_for "${NAME}")" "${WT}/.agent-env"
mkdir -p "${AGENT_TMP_ROOT}"
bash "${BUDGET_TOOL}" register "${WT}" "${DISK_RESERVE_GB}"

# ---- 4. 把资源写进 worktree 自己的配置副本 ----
apply_out="$(python3 "${AGENTS_LIB_DIR}/apply_agent_env.py" "${WT}")"
printf '%s\n' "${apply_out}" | grep -v '^TOUCHED='
touched="$(printf '%s\n' "${apply_out}" | sed -n 's/^TOUCHED=//p')"
skipped=""
for rel in ${touched}; do
    if git -C "${WT}" ls-files --error-unmatch "${rel}" >/dev/null 2>&1; then
        git -C "${WT}" update-index --skip-worktree "${rel}"
        skipped="${skipped} ${rel}"
    fi
done
if [ -n "${skipped}" ]; then
    log "被跟踪的配置副本已标 skip-worktree（git status 不显示、不会误提交）：${skipped}"
fi

# ---- 5. 测试配置 ----
if [ "${NO_TEST_ENV}" = "0" ] && [ -f "${WT}/test/scripts/setup_test_env.py" ]; then
    log "用独占端口/目录生成 test/config 测试配置"
    (
        set -a
        # shellcheck disable=SC1091
        . "${WT}/.agent-env"
        set +a
        cd "${WT}" && python3 test/scripts/setup_test_env.py | grep -E '^\s+(监听端口|运行时目录)' || true
    )
fi

# ---- 6. 独立数据库 ----
if [ "${NO_DB}" = "0" ] && [ -f "${WT}/config/db_config.json" ]; then
    if ! bash "${AGENTS_LIB_DIR}/db_init.sh" --db-config "${WT}/config/db_config.json" \
            --sql "${WT}/config/sql/create_tables.sql"; then
        warn "独立库未建成（不影响编译和纯单测）。稍后可重试："
        warn "  bash ${AGENTS_LIB_DIR}/db_init.sh --db-config ${WT}/config/db_config.json"
    fi
fi

# ---- 7. 可选构建 ----
if [ -n "${BUILD_TYPE}" ]; then
    log "构建（tools/agents/gate.sh：全局构建锁 + 全量场景回归 + registry dump）"
    ( cd "${WT}" && tools/agents/gate.sh )
fi

cat <<EOF

================ CREATED ================
  name      : ${NAME}
  worktree  : ${WT}
  branch    : ${BR}  (base ${FROM})
  build dir : ${WT}/build          （cd 进去跑 tools/agents/gate.sh；全局同一时刻只跑一个构建）
  identity  : ${WT}/.agent-env     （index=${AGENT_INDEX}）
  ports     : ${AGENT_PORT_BASE}-$(( AGENT_PORT_BASE + PORT_BLOCK_SIZE - 1 ))  (预留段——本项目 sim_server 自动挑空闲口/场景二进制无端口，策略层需要时取用)
  tmp root  : ${AGENT_TMP_ROOT}
  database  : （无——本项目 sqlite 只读共享，不做 per-worktree 库）
  rules     : ${HUB}/CLAUDE.md 与 tools/agents/task-template.md（项目特色策略）
  next      : cd ${WT} && tools/agents/gate.sh
=========================================
EOF
