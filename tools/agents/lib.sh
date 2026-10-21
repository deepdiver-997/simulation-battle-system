#!/bin/bash
# tools/agents/lib.sh — 多 agent 并行工作区的公共库
# 被 new-worktree.sh / remove-worktree.sh / status.sh / db_init.sh source，不单独执行。
#
# 设计要点（细节见同目录 README.md）：
#   * hub        = 主检出（人的集成仓库）；worktree 放在 ${AGENTS_DIR}/<name>，分支 feat/<name>
#   * 跨 worktree 共享的运行态全部放 <hub>/.git/agents/（永不入库，也不会被 build.sh clean 清掉）
#   * 每个 worktree 独占一段非标端口、一个 /tmp 根目录、一个 MySQL 库，记录在 worktree 的 .agent-env
#   * 变量一律写成 ${VAR}：macOS 自带 bash 3.2 在 UTF-8 下，中文紧邻 $VAR 会把字节吞进变量名
#   * 兼容 bash 3.2：不用关联数组 / mapfile / ${var,,}

set -o pipefail

AGENTS_LIB_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# ---- 输出 ----
log()  { printf '\033[0;34m[agents]\033[0m %s\n' "$*"; }
ok()   { printf '\033[0;32m[agents]\033[0m %s\n' "$*"; }
warn() { printf '\033[1;33m[agents]\033[0m %s\n' "$*" >&2; }
die()  { printf '\033[0;31m[agents]\033[0m %s\n' "$*" >&2; exit 1; }

# ---- git 路径解析（在 hub 或任意 worktree 内都正确）----
_git_abs_from() { # $1=目录 $2=rev-parse 选项 → 绝对路径或空
    local p
    p="$(git -C "$1" rev-parse --path-format=absolute "$2" 2>/dev/null || true)"
    if [ -z "${p}" ]; then
        p="$(git -C "$1" rev-parse "$2" 2>/dev/null || true)"
        if [ -n "${p}" ]; then
            p="$(cd "$1" && cd "${p}" 2>/dev/null && pwd || true)"
        fi
    fi
    printf '%s' "${p}"
}

# agents_init：解析 HUB / STATE_DIR，读取 agents.conf，补默认值。所有脚本开头调用。
agents_init() {
    GIT_COMMON_DIR="$(_git_abs_from "${AGENTS_LIB_DIR}" --git-common-dir)"
    [ -n "${GIT_COMMON_DIR}" ] || die "tools/agents 必须在 git 检出内运行"
    HUB="$(dirname "${GIT_COMMON_DIR}")"
    CUR_TOPLEVEL="$(git -C "${AGENTS_LIB_DIR}" rev-parse --show-toplevel 2>/dev/null || true)"
    STATE_DIR="${GIT_COMMON_DIR}/agents"
    ALLOC_DIR="${STATE_DIR}/alloc"
    mkdir -p "${ALLOC_DIR}"

    # ---- 两层 agents.env 配置（KEY=VALUE，bash 直接 source）----
    #   机器级  ~/.agent-parallel/tools/agents.env   一台机器一份（锁等待 / ccache / 端口段起点）
    #   项目级  <本项目>/tools/agents/agents.env     覆盖机器级（基准分支 / 资源命名 / 预算预估）
    # 都缺失不阻塞（走内置默认值），但按梯子提示怎么补；真正的硬闸是设备磁盘预算。
    # 工作区根目录刻意不在参数里：固定 ~/.agent-workspaces/workspaces/<项目名>/。
    AGENT_PARALLEL_HOME="${AGENT_PARALLEL_HOME:-$HOME/.agent-parallel}"
    local global_env="${AGENT_PARALLEL_HOME}/tools/agents.env"
    local local_env="${AGENTS_LIB_DIR}/agents.env"
    if [ ! -d "${AGENT_PARALLEL_HOME}" ]; then
        warn "未检测到全局骨架母本 ${AGENT_PARALLEL_HOME}：建议先安装一份，配置统一放一处不容易出错"
        echo "  参考 ~/.agent-parallel 的 README（或本项目 tools/agents/README.md「落地到新项目」）"
    elif [ -f "${global_env}" ]; then
        # shellcheck disable=SC1090
        . "${global_env}"
    fi
    if [ -f "${local_env}" ]; then
        # shellcheck disable=SC1090
        . "${local_env}"
    else
        warn "项目内没有 tools/agents/agents.env：本机/本项目参数将走默认值"
        echo "  自行写入：cp ${AGENTS_LIB_DIR}/agents.env.example ${local_env}  # 然后按项目去掉注释填写"
    fi

    # 工作区根：固定绝对路径，全机器只有这一处（与磁盘账本同家）。
    # sbs-local 适配（2026-10-21）：上游默认取 hub 父目录名当命名空间（面向
    # ~/<项目名>/<repo> 布局）；本仓在 code/c++/project/sbs-local 多级嵌套下父名
    # 恒为 "project" 会跨仓撞名 → 改为默认取 hub 自身目录名，并留 AGENTS_DIR 显式覆盖口。
    AGENTS_DIR="${AGENTS_DIR:-${HOME}/.agent-workspaces/workspaces/$(basename "${HUB}")}"
    BASE_BRANCH="${BASE_BRANCH:-main}"
    PORT_BLOCK_START="${PORT_BLOCK_START:-20000}"
    PORT_BLOCK_SIZE="${PORT_BLOCK_SIZE:-100}"
    TMP_ROOT_PREFIX="${TMP_ROOT_PREFIX:-/tmp/agent_ws_}"
    DB_NAME_PREFIX="${DB_NAME_PREFIX:-mail_agent_}"
    # 传给 build.sh 的旋钮（build.sh 直接读环境）
    export BUILD_LOCK_WAIT="${BUILD_LOCK_WAIT:-3600}"
    export USE_CCACHE="${USE_CCACHE:-auto}"
}

# 名字：小写字母/数字开头，只含 [a-z0-9._-]，最长 40（用作目录名、分支名、库名后缀的一部分）
validate_name() {
    [[ "$1" =~ ^[a-z0-9][a-z0-9._-]{0,39}$ ]] \
        || die "名字只能用小写字母、数字、'.' '_' '-'，并以字母或数字开头（最长 40）：'$1'"
}

worktree_path_for() { printf '%s/%s' "${AGENTS_DIR}" "$1"; }
branch_for()        { printf 'feat/%s' "$1"; }
alloc_record_for()  { printf '%s/%s.env' "${ALLOC_DIR}" "$1"; }

# 列出所有分配记录文件（无记录时输出空，不会把字面 glob 吐出来）
alloc_records() {
    local f
    for f in "${ALLOC_DIR}"/*.env; do
        [ -e "${f}" ] && printf '%s\n' "${f}"
    done
    return 0
}

# 从分配记录里读一个字段的裸值（去掉包裹的双引号）
alloc_field() { # $1=文件 $2=键
    sed -n "s/^$2=\"\{0,1\}\([^\"]*\)\"\{0,1\}\$/\1/p" "$1"
}

# ---- 资源分配 ----
alloc_find_free_index() {
    local used i f
    used=" "
    for f in $(alloc_records); do
        used="${used}$(alloc_field "${f}" AGENT_INDEX) "
    done
    for i in $(seq 1 99); do
        case "${used}" in *" ${i} "*) continue ;; esac
        printf '%s' "${i}"
        return 0
    done
    die "AGENT_INDEX 1..99 已全部占用，先 remove-worktree 释放一些"
}

# 一段端口是否全部空闲（127.0.0.1 bind 探测，能发现别的项目正在监听的端口）。
# 全空闲 → 退出 0；否则输出第一个被占的端口并退出 1。
ports_free() { # $1=起始端口 $2=数量
    python3 - "$1" "$2" <<'PY'
import socket, sys
base, n = int(sys.argv[1]), int(sys.argv[2])
for p in range(base, base + n):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        s.bind(("127.0.0.1", p))
    except OSError:
        print(p)
        sys.exit(1)
    finally:
        s.close()
sys.exit(0)
PY
}

port_base_taken() { # 这段端口是否已分配给别的 worktree
    local f
    for f in $(alloc_records); do
        [ "$(alloc_field "${f}" AGENT_PORT_BASE)" = "$1" ] && return 0
    done
    return 1
}

# alloc_resources <name> <worktree> <branch>：分配 index / 端口段 / tmp 根 / 库名，
# 填充 AGENT_* 变量并写入分配记录。端口段先按 index 取默认段，被占（别的项目或别的 agent）
# 则整段后移，直到找到完全空闲的一段。
alloc_resources() {
    local name="$1" wt="$2" br="$3" idx base tries busy
    idx="$(alloc_find_free_index)"
    base=$(( PORT_BLOCK_START + idx * PORT_BLOCK_SIZE ))
    tries=0
    while :; do
        if port_base_taken "${base}"; then
            log "端口段 ${base} 已分配给其他 worktree，后移"
        elif busy="$(ports_free "${base}" "${PORT_BLOCK_SIZE}")"; then
            break
        else
            log "端口段 ${base}-$(( base + PORT_BLOCK_SIZE - 1 )) 内 ${busy} 被占用，后移"
        fi
        base=$(( base + PORT_BLOCK_SIZE ))
        tries=$(( tries + 1 ))
        [ "${tries}" -lt 300 ] || die "找不到空闲端口段（从 ${PORT_BLOCK_START} 起试了 ${tries} 段）"
    done

    AGENT_NAME="${name}"
    AGENT_INDEX="${idx}"
    AGENT_WORKTREE="${wt}"
    AGENT_BRANCH="${br}"
    AGENT_PORT_BASE="${base}"
    AGENT_TMP_ROOT="${TMP_ROOT_PREFIX}${idx}"
    AGENT_DB="${DB_NAME_PREFIX}${idx}"
    AGENT_TABLE_PREFIX="a${idx}_"
    write_agent_env "$(alloc_record_for "${name}")"
}

# write_agent_env <文件>：以 KEY="VALUE" 写出当前 AGENT_* 资源（可直接 source）。
# 值一律加双引号（路径可能含空格）；注释独立成行，便于 apply_agent_env.py 等简单解析器读取。
write_agent_env() {
    local b="${AGENT_PORT_BASE}"
    cat > "$1" <<EOF
# 本 worktree 的 agent 身份与独占资源 —— 由 tools/agents/new-worktree.sh 生成，gitignored。
# 用法：set -a; . .agent-env; set +a    （或逐个读取）
AGENT_NAME="${AGENT_NAME}"
AGENT_INDEX="${AGENT_INDEX}"
AGENT_WORKTREE="${AGENT_WORKTREE}"
AGENT_BRANCH="${AGENT_BRANCH}"
AGENT_HUB="${HUB}"
AGENT_CREATED_AT="$(date '+%Y-%m-%dT%H:%M:%S')"

# 端口：独占 ${PORT_BLOCK_START}+ 起的一段 ${PORT_BLOCK_SIZE} 个非标端口；下面是约定好的槽位。
# 测试配置用（setup_test_env.py / test/config/*_test.json 的 2525 8465 8587 1414 8993 → base+0..4）
AGENT_PORT_BASE="${b}"
AGENT_PORT_SMTP_TCP="$(( b + 0 ))"
AGENT_PORT_SMTP_SSL="$(( b + 1 ))"
AGENT_PORT_SMTP_SUBMISSION="$(( b + 2 ))"
AGENT_PORT_IMAP_TCP="$(( b + 3 ))"
AGENT_PORT_IMAP_SSL="$(( b + 4 ))"
AGENT_PORT_METRICS="$(( b + 5 ))"
AGENT_PORT_WEBMAIL="$(( b + 6 ))"
AGENT_PORT_E2E_IMAP="$(( b + 7 ))"
# 生产风格配置副本用（config/*.json 的 25 465 587 143 993 110 995 8080 → base+10..17）
AGENT_PORT_SMTP="$(( b + 10 ))"
AGENT_PORT_SMTPS="$(( b + 11 ))"
AGENT_PORT_SUBMISSION="$(( b + 12 ))"
AGENT_PORT_IMAP="$(( b + 13 ))"
AGENT_PORT_IMAPS="$(( b + 14 ))"
AGENT_PORT_POP3="$(( b + 15 ))"
AGENT_PORT_POP3S="$(( b + 16 ))"
AGENT_PORT_REGISTER="$(( b + 17 ))"

# 临时目录 / 数据库（账号沿用共享测试账号，不单独分配）
AGENT_TMP_ROOT="${AGENT_TMP_ROOT}"
AGENT_DB="${AGENT_DB}"
AGENT_TABLE_PREFIX="${AGENT_TABLE_PREFIX}"

# 直接喂给 test/scripts/setup_test_env.py 的环境变量
## 变量名为中性命名；项目测试工具若要求特定变量名，在项目策略层映射
TEST_TMP_ROOT="${AGENT_TMP_ROOT}/test_env"
TEST_PORT_BASE="${b}"
EOF
}

# 读取某个 worktree 的分配记录到当前 shell（存在则返回 0）
load_alloc() { # $1=name
    local f
    f="$(alloc_record_for "$1")"
    [ -f "${f}" ] || return 1
    # shellcheck disable=SC1090
    . "${f}"
}

release_resources() { # $1=name
    rm -f "$(alloc_record_for "$1")"
}

# ---- 构建锁状态（与 build.sh 的实现对应，只读）----
build_lock_info() {
    local lock="${STATE_DIR}/build.lock" pid wt cmd since alive
    if [ ! -d "${lock}" ]; then
        printf '空闲\n'
        return 0
    fi
    pid="$(cat "${lock}/pid" 2>/dev/null || true)"
    wt="$(cat "${lock}/worktree" 2>/dev/null || true)"
    cmd="$(cat "${lock}/cmd" 2>/dev/null || true)"
    since="$(cat "${lock}/since" 2>/dev/null || true)"
    if [ -n "${pid}" ] && kill -0 "${pid}" 2>/dev/null; then alive="存活"; else alive="已退出(待 60s 陈旧清扫)"; fi
    printf '占用中 pid=%s(%s) 检出=%s 命令="%s" 已持有 %ss\n' \
        "${pid:-?}" "${alive}" "${wt:-?}" "${cmd}" "$(( $(date +%s) - ${since:-$(date +%s)} ))"
}

# ---- 杂项 ----
hub_has_uncommitted() { [ -n "$(git -C "${HUB}" status --porcelain 2>/dev/null)" ]; }

# 从 JSON 文件取一个顶层字段（无则空）
json_get() { # $1=文件 $2=键
    python3 -c 'import json,sys
d=json.load(open(sys.argv[1])); v=d.get(sys.argv[2], "")
print("" if v is None else v)' "$1" "$2" 2>/dev/null
}
