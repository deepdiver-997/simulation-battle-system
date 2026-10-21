#!/bin/bash
# tools/agents/budget.sh — agent 工作区的设备级磁盘预算（并行骨架的生产修复 #1）
#
# 背景：sbs 夜跑从 k253 开到 k344 共 91 个 worktree、每份躺着一个 2.7G 的构建副本没人删，
# agents 目录涨到 234G。教训：磁盘占用上限必须是（a）开始前强制检查、（b）设备级共享——
# 否则每个项目看"剩余空间还多"各拿 200G，一起把盘吃穿。
#
# 设备级状态（所有项目共用，不在任何仓库里）：
#   ~/.agent-workspaces/budget.conf   人类显式设定的总预算（GB）+ 设定时的实测快照
#   ~/.agent-workspaces/ledger.tsv    各项目各 worktree 的占用登记（repo / 路径 / KB / 时间）
#
# 用法：
#   budget.sh set <GB>       人工设定设备总预算；用 df 实测校验（≤ 当前可用空间的 90%）
#   budget.sh suggest        打印建议值（当前可用空间的 30%，仅供人参考，不代替人决定）
#   budget.sh unset          撤销预算（new-worktree/build 将全部拒绝）
#   budget.sh check <GB>     退出 0=够用；1=超预算或卷空间不足；2=预算未设置
#   budget.sh register <wt路径> <预估GB>   开工作区时登记
#   budget.sh record <wt路径> [KB]         用真实 du 刷新一行（省略 KB 则现 du）
#   budget.sh release <wt路径>             删工作区时释放
#   budget.sh refresh                      全账本重算（顺带清掉已不存在的行）
#   budget.sh show                         预算 / 已用 / 卷剩余 / 分项目明细
#
# 环境变量 AGENT_WORKSPACE_HOME 可把状态目录挪走（测试用）。

set -e

BUDGET_HOME="${AGENT_WORKSPACE_HOME:-$HOME/.agent-workspaces}"
CONF="${BUDGET_HOME}/budget.conf"
LEDGER="${BUDGET_HOME}/ledger.tsv"

log()  { printf '\033[0;34m[budget]\033[0m %s\n' "$*"; }
ok()   { printf '\033[0;32m[budget]\033[0m %s\n' "$*"; }
warn() { printf '\033[1;33m[budget]\033[0m %s\n' "$*" >&2; }
die()  { printf '\033[0;31m[budget]\033[0m %s\n' "$*" >&2; exit 1; }

# 卷剩余空间（GB），按参数路径所在卷实测
free_gb() { df -k "${1:-$HOME}" 2>/dev/null | awk 'NR==2{print int($4/1048576)}'; }

budget_gb() {
    [ -f "${CONF}" ] || return 1
    # shellcheck disable=SC1090
    . "${CONF}"
    [ -n "${AGENT_DISK_BUDGET_GB:-}" ] || return 1
    printf '%s' "${AGENT_DISK_BUDGET_GB}"
}

used_kb() {
    [ -f "${LEDGER}" ] || { printf 0; return 0; }
    awk -F'\t' '{ s += $3 } END { printf "%d", s + 0 }' "${LEDGER}"
}

# check <需要的GB>：算上全设备账本已用量之后还够不够
cmd_check() {
    local need="${1:-0}"
    case "${need}" in ''|*[!0-9]*) die "check 需要 <GB>（整数）" ;; esac
    local b free used_kb_val used_gb need_kb
    if ! b="$(budget_gb)"; then
        warn "设备级磁盘预算未设置（~/.agent-workspaces/budget.conf 不存在）"
        echo "  无人监管的并行工作区在预算设定前一律拒绝启动。人工执行："
        echo "    tools/agents/budget.sh suggest            # 看参考值"
        echo "    tools/agents/budget.sh set <GB>           # 人显式设定（会做 df 实测校验）"
        exit 2
    fi
    free="$(free_gb)"
    used_kb_val="$(used_kb)"
    used_gb=$(( ( used_kb_val + 1048575 ) / 1048576 ))
    need_kb=$(( need * 1048576 ))
    if [ $(( used_gb + need )) -gt "${b}" ]; then
        warn "设备 agent 磁盘预算不足：预算 ${b}G，已登记 ${used_gb}G，本次还需 ${need}G"
        echo "  先合并/删掉收尾的 worktree（tools/agents/remove-worktree.sh），或由人工调高预算：tools/agents/budget.sh set <更大的GB>"
        exit 1
    fi
    if [ -n "${free}" ] && [ "${need_kb}" -gt $(( free * 1048576 * 90 / 100 )) ]; then
        warn "卷剩余空间不足：实测剩 ${free}G，本次需要 ${need}G（且要求 ≤ 剩余的 90%）"
        exit 1
    fi
    echo "预算 ${b}G：已登记 ${used_gb}G + 本次 ${need}G ≤ 上限；卷剩余 ${free}G"
    exit 0
}

cmd_set() {
    local gb="${1:-}"
    case "${gb}" in ''|*[!0-9]*) die "set 需要 <GB>（正整数）" ;; esac
    [ "${gb}" -ge 1 ] || die "预算至少 1G"
    local prev free
    prev="$(budget_gb 2>/dev/null || true)"
    free="$(free_gb)"
    if [ "${gb}" -gt $(( free * 90 / 100 )) ]; then
        die "拒绝：${gb}G 超过当前实测可用空间 ${free}G 的 90%。预算必须是经实测验证的安全值——先清盘或给一个更小的数"
    fi
    mkdir -p "${BUDGET_HOME}"
    {
        echo "# 由人显式设定（budget.sh set），设定时做过 df 实测校验；所有项目共享这一份"
        echo "AGENT_DISK_BUDGET_GB=${gb}"
        echo "VERIFIED_AT=\"$(date '+%Y-%m-%d %H:%M:%S')\""
        echo "FREE_GB_AT_VERIFICATION=${free}"
    } > "${CONF}"
    if [ -n "${prev}" ]; then
        ok "预算已从 ${prev}G 调整为 ${gb}G（设定时卷剩余 ${free}G）"
    else
        ok "设备磁盘预算设为 ${gb}G（设定时卷剩余 ${free}G）"
    fi
}

cmd_suggest() {
    local free
    free="$(free_gb)"
    echo "$(( free * 30 / 100 ))G（= 当前卷剩余 ${free}G 的 30%；仅供参考，预算仍需人工 set）"
}

cmd_register() { # $1=worktree路径 $2=预估GB
    [ -n "$1" ] && [ -n "$2" ] || die "register 需要 <worktree路径> <预估GB>"
    record_row "$1" $(( ${2} * 1048576 ))
    log "已登记 $1（预估 ${2}G）"
}

cmd_record() { # $1=worktree路径 $2=KB（省略则现 du）
    local kb="$2"
    if [ -z "${kb}" ]; then
        [ -d "$1" ] || die "目录不存在：$1"
        kb="$(du -sk "$1" 2>/dev/null | awk '{print $1}')"
    fi
    record_row "$1" "${kb}"
}

record_row() { # $1=路径 $2=KB：按路径 upsert
    local path kb tmp
    path="$1"; kb="$2"
    mkdir -p "${BUDGET_HOME}"
    touch "${LEDGER}"
    tmp="${LEDGER}.tmp"
    awk -F'\t' -v p="${path}" '$2 != p { print }' "${LEDGER}" > "${tmp}"
    printf '%s\t%s\t%s\t%s\n' "$(dirname "$(dirname "${path}")")" "${path}" "${kb}" "$(date '+%Y-%m-%d %H:%M')" >> "${tmp}"
    mv "${tmp}" "${LEDGER}"
}

cmd_release() {
    [ -n "${1:-}" ] || die "release 需要 <worktree路径>"
    [ -f "${LEDGER}" ] || return 0
    local tmp="${LEDGER}.tmp"
    awk -F'\t' -v p="$1" '$2 != p { print }' "${LEDGER}" > "${tmp}"
    mv "${tmp}" "${LEDGER}"
    log "已释放 $1 的登记"
}

cmd_refresh() {
    [ -f "${LEDGER}" ] || { echo "账本为空"; return 0; }
    local tmp="${LEDGER}.tmp" path kb
    : > "${tmp}"
    while IFS="$(printf '\t')" read -r repo path kb updated; do
        [ -n "${path}" ] || continue
        if [ -d "${path}" ]; then
            kb="$(du -sk "${path}" 2>/dev/null | awk '{print $1}')"
            printf '%s\t%s\t%s\t%s\n' "${repo}" "${path}" "${kb}" "$(date '+%Y-%m-%d %H:%M')" >> "${tmp}"
        else
            log "清理失效行：${path}"
        fi
    done < "${LEDGER}"
    mv "${tmp}" "${LEDGER}"
    ok "账本已刷新"
}

cmd_show() {
    local b used_gb free
    if b="$(budget_gb)"; then
        used_gb=$(( ( $(used_kb) + 1048575 ) / 1048576 ))
        free="$(free_gb)"
        echo "设备预算 : ${b}G（$(conf_verified_note)）"
        echo "已登记   : ${used_gb}G，余量 $(( b - used_gb ))G"
        echo "卷剩余   : ${free}G"
        if [ -s "${LEDGER}" ]; then
            echo "分项目   :"
            awk -F'\t' '{ s[$1] += $3; n[$1]++ } END { for (r in s) printf "  %-60s %6.1fG  (%d 个)\n", r, s[r]/1048576, n[r] }' \
                "${LEDGER}" | sort
        fi
    else
        echo "设备预算 : 未设置（并行工作区一律拒绝启动）"
        echo "参考值   : $(cmd_suggest)"
    fi
}
conf_verified_note() {
    [ -f "${CONF}" ] && sed -n 's/^VERIFIED_AT="\{0,1\}\([^"]*\)"\{0,1\}$/设定于 \1/p' "${CONF}" | head -1 || true
}

case "${1:-}" in
    set)     shift; cmd_set "$@" ;;
    suggest) cmd_suggest ;;
    unset)   rm -f "${CONF}"; ok "预算已撤销（并行工作区将被拒绝）" ;;
    check)   shift; cmd_check "$@" ;;
    register) shift; cmd_register "$@" ;;
    record)  shift; cmd_record "$@" ;;
    release) shift; cmd_release "$@" ;;
    refresh) cmd_refresh ;;
    show)    cmd_show ;;
    *)       sed -n '2,/^$/p' "$0" | sed 's/^# \{0,1\}//'; [ "${1:-}" = "-h" ] || exit 1 ;;
esac
