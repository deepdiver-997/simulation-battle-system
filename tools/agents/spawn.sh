#!/bin/bash
# tools/agents/spawn.sh — 无头派发：把粗粒度任务交给任意 harness（claude/codex/gemini/dummy），
# 在它自己的 worktree 里后台执行；人（或编排会话）用 resume 追加指令，用 status 收割结果。
#
# 用法：
#   spawn.sh start  <harness> <name> <task.md | ->   [--new] [--fg] [--light] [-- 透传harness参数...]
#   spawn.sh resume <name> "<追加指令>" | @文件       [--session <id>]
#   spawn.sh status [name]
#   spawn.sh stop   <name>
#
# 设计立场：编排是哑的（本脚本不做任何智能决策），任务粒度是粗的（一次调用=一个有完成标准的
# 任务），纠偏靠 resume 而不是实时插话；跨 agent 的沟通媒介是 git 分支 + 磁盘上的报告文件。
#
# 每个 run 一个目录 .git/agents/runs/<name>/<ts>/：
#   task.md 任务原文   prompt.txt 实发提示词   stream.log harness输出(JSONL)
#   meta    harness/pid/started/worktree/type   exit  退出码(结束后写入)
#   report.md  agent 按提示词约定写的最终报告       last-message.txt (codex -o 落的最终消息)
# `current` 符号链接永远指向该 name 最近一次活动。

set -e
# shellcheck disable=SC1091
. "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/lib.sh"
agents_init

CONF="${AGENTS_LIB_DIR}/harnesses.conf"
# shellcheck disable=SC1090
. "${CONF}"
if [ -f "${AGENTS_LIB_DIR}/harnesses.conf.local" ]; then
    # shellcheck disable=SC1090
    . "${AGENTS_LIB_DIR}/harnesses.conf.local"
fi

RUNS_DIR="${STATE_DIR}/runs"
mkdir -p "${RUNS_DIR}"

usage() { sed -n '2,/^$/p' "$0" | sed 's/^# \{0,1\}//'; }

available_harnesses() {
    declare -F | sed -n 's/declare -f harness_//;s/_cmd$//p' | grep -v resume | tr '\n' ' '
}

run_dir_for() { printf '%s/%s' "${RUNS_DIR}" "$1"; }
meta_get() { # $1=run目录 $2=键
    alloc_field "$1/meta" "$2"
}

extract_session_id() { # $1=stream.log → 打印 session id（可能为空）
    python3 - "$1" <<'PY'
import re, sys
try:
    with open(sys.argv[1], errors="ignore") as f:
        for line in f:
            m = re.search(r'"(session_id|thread_id|conversation_id)"\s*:\s*"([^"]+)"', line)
            if m:
                print(m.group(2))
                break
except OSError:
    pass
PY
}

build_prompt() { # $1=name $2=worktree $3=run目录 $4=任务文件 → stdout
    # 完成标准里的构建/测试命令可在 agents.env 里按项目改写
    local build_cmd="${BUILD_CMD:-./build.sh Release}"
    local test_cmd="${TEST_CMD:-bash test/run_all_tests.sh --no-build}"
    local criteria
    if [ "${SPAWN_LIGHT:-0}" = "1" ]; then
        criteria='- 以任务自身的要求为完成标准（本 run 为轻任务，不强制全量构建/单测）
- 把最终报告写到：'"$3"'/report.md'
    else
        criteria="- 全量门绿：${build_cmd}（=构建 + 全量场景回归 + registry dump；排队全局构建锁是正常现象）
- 所有改动已提交到 feat/$1（魂印/技能注册数只增不改；场景号段已 alloc）
- 看板留痕完整（taskctl 领单/完成 + tasklog 台账）
- 把最终报告写到：$3/report.md，小节：结果 / 做了什么 / 提交列表 / 断言口径与实测 / 勘误记档 / 遗留问题"
    fi
    cat <<EOF
你是本仓库（sbs-local 回合制对战引擎）的一个无头 agent 会话（同一时刻只做这一个任务）。
名字：$1    分支：feat/$1    worktree：$2

开工前必做：
1. 读 CLAUDE.md（总纲）与 tools/agents/task-template.md（项目特色策略：必读四文档、
   立单三查、实现套路收敛、fail-closed 红线）并严格遵守。
2. 执行 set -a; . .agent-env; set +a —— 里面是你的身份与独占资源（本项目无 per-worktree 库）。
3. 本任务做完即止：不自行立项，不进其他 worktree 或 hub 改东西，不 push，不动 main/main-local，合并由集成会话做。

---TASK---
$(cat "$4")
---TASK 结束---

完成标准（全部满足才算完成）：
$criteria
EOF
}

write_wrapper() { # $1=wrapper路径 $2=worktree $3=run目录  (HARNESS_CMD 已就绪)
    local cmdline
    cmdline="$(printf '%q ' "${HARNESS_CMD[@]}")"
    {
        echo '#!/bin/bash'
        echo "# 由 spawn.sh 生成；退出码写入 $3/exit"
        echo "trap 'rc=\$?; printf \"%s\" \"\$rc\" > \"$3/exit\" 2>/dev/null; exit \$rc' EXIT"
        printf 'cd %q || exit 9\n' "$2"
        printf '%s\n' "${cmdline}"
    } > "$1"
    chmod +x "$1"
}

new_run_dir() { # $1=name → 打印 run 目录并建好
    local ts rd
    ts="$(date '+%Y%m%d-%H%M%S')"
    rd="${RUNS_DIR}/$1/${ts}-$$"
    mkdir -p "${rd}"
    ln -sfn "${rd}" "${RUNS_DIR}/$1/current"
    printf '%s' "${rd}"
}

cmd_start() {
    local harness="" name="" task_src="" new_wt=0 fg=0
    while [ $# -gt 0 ]; do
        case "$1" in
            --new) new_wt=1; shift ;;
            --fg)  fg=1; shift ;;
            --light) SPAWN_LIGHT=1; shift ;;
            --)    shift; break ;;
            -*)    die "未知参数：$1" ;;
            *)     if [ -z "${harness}" ]; then harness="$1"
                   elif [ -z "${name}" ]; then name="$1"
                   elif [ -z "${task_src}" ]; then task_src="$1"
                   else die "多余参数：$1"; fi
                   shift ;;
        esac
    done
    [ -n "${harness}" ] && [ -n "${name}" ] && [ -n "${task_src}" ] \
        || { usage; die "start 需要 <harness> <name> <task>"; }
    validate_name "${name}"
    declare -F "harness_${harness}_cmd" >/dev/null 2>&1 \
        || die "没有 ${harness} 适配器。可用：$(available_harnesses)"

    local wt
    wt="$(worktree_path_for "${name}")"
    if [ ! -e "${wt}/.git" ]; then
        [ "${new_wt}" = "1" ] || die "worktree 不存在：${wt}（加 --new 让我先用 new-worktree.sh 建）"
        bash "${AGENTS_LIB_DIR}/new-worktree.sh" "${name}"
    fi

    local rd
    rd="$(new_run_dir "${name}")"
    if [ "${task_src}" = "-" ]; then
        cat > "${rd}/task.md"
    elif [ -f "${task_src}" ]; then
        cp -p "${task_src}" "${rd}/task.md"
    else
        die "任务文件不存在：${task_src}（任务原文用 - 从 stdin 读）"
    fi
    build_prompt "${name}" "${wt}" "${rd}" "${rd}/task.md" > "${rd}/prompt.txt"

    export SPAWN_PROMPT
    SPAWN_PROMPT="$(cat "${rd}/prompt.txt")"
    export SPAWN_RUN_DIR="${rd}"
    harness_"${harness}"_cmd
    # 透传参数（-- 之后的部分）追加到命令尾
    if [ $# -gt 0 ]; then
        HARNESS_CMD+=("$@")
    fi

    cat > "${rd}/meta" <<EOF
NAME="${name}"
HARNESS="${harness}"
TYPE="start"
PID=""
STARTED="$(date '+%Y-%m-%d %H:%M:%S')"
WORKTREE="${wt}"
RESUME_SUPPORTED="${HARNESS_RESUME:-0}"
EOF
    write_wrapper "${rd}/wrapper.sh" "${wt}" "${rd}"

    if [ "${fg}" = "1" ]; then
        log "前台运行：${harness} in ${wt}（run: ${rd}）"
        local rc=0
        bash "${rd}/wrapper.sh" > "${rd}/stream.log" 2>&1 || rc=$?
        note_result "${rd}"
        return "${rc}"
    fi

    nohup bash "${rd}/wrapper.sh" > "${rd}/stream.log" 2>&1 &
    local pid=$!
    sed -i '' -e "s/^PID=\"\"/PID=\"${pid}\"/" "${rd}/meta" 2>/dev/null \
        || sed -i "s/^PID=\"\"/PID=\"${pid}\"/" "${rd}/meta"
    cat <<EOF

$(ok "已派发")  name=${name} harness=${harness} pid=${pid}
  run     : ${rd}
  follow  : tail -f ${rd}/stream.log
  steer   : tools/agents/spawn.sh resume ${name} "追加指令"
  harvest : tools/agents/spawn.sh status ${name}
  stop    : tools/agents/spawn.sh stop ${name}
EOF
}

note_result() { # $1=run目录：结束后打印摘要
    local sid exit_code rtype
    sid="$(extract_session_id "$1/stream.log")"
    exit_code="$(cat "$1/exit" 2>/dev/null || echo '?')"
    rtype="$(meta_get "$1" TYPE)"
    [ -n "${sid}" ] && log "session id: ${sid}"
    if [ "${exit_code}" != "0" ]; then
        warn "run 结束 exit=${exit_code}，查看 $1/stream.log"
    elif [ "${rtype}" = "start" ]; then
        if [ -f "$1/report.md" ]; then
            ok "run 结束（exit=0）。报告：$1/report.md"
        else
            warn "run 结束（exit=0）但没找到 $1/report.md —— agent 可能没按契约写报告，看 stream.log"
        fi
    else
        ok "resume 结束（exit=0）"
    fi
}

cmd_resume() {
    local name="" text="" want_session=""
    while [ $# -gt 0 ]; do
        case "$1" in
            --session) want_session="$2"; shift 2 ;;
            -*) die "未知参数：$1" ;;
            *)  if [ -z "${name}" ]; then name="$1"
                elif [ -z "${text}" ]; then text="$1"
                else die "多余参数：$1"; fi
                shift ;;
        esac
    done
    [ -n "${name}" ] && [ -n "${text}" ] || { usage; die "resume 需要 <name> <指令>"; }

    local cur rd harness sid
    cur="${RUNS_DIR}/${name}/current"
    [ -L "${cur}" ] || die "没有 ${name} 的派发记录"
    rd="$(readlink "${cur}")"
    harness="$(meta_get "${rd}" HARNESS)"
    if [ -n "${want_session}" ]; then
        sid="${want_session}"
    else
        sid="$(extract_session_id "${rd}/stream.log")"
    fi
    [ -n "${sid}" ] || die "从 stream.log 提取不到 session id，无法续话（--session <id> 手工指定）"
    declare -F "harness_${harness}_resume_cmd" >/dev/null 2>&1 \
        || die "harness '${harness}' 的适配器不支持 resume"

    # @file 形式读追加指令
    case "${text}" in
        @*) [ -f "${text#@}" ] || die "文件不存在：${text#@}"; text="$(cat "${text#@}")" ;;
    esac

    local nrd
    nrd="$(new_run_dir "${name}")"
    printf 'NAME="%s"\nHARNESS="%s"\nTYPE="resume"\nPID=""\nSTARTED="%s"\nWORKTREE="%s"\nSESSION="%s"\nRESUME_OF="%s"\n' \
        "${name}" "${harness}" "$(date '+%Y-%m-%d %H:%M:%S')" "$(meta_get "${rd}" WORKTREE)" "${sid}" "${rd}" > "${nrd}/meta"
    printf '%s\n' "${text}" > "${nrd}/task.md"
    export SPAWN_RUN_DIR="${nrd}"
    harness_"${harness}"_resume_cmd "${sid}" "${text}"
    write_wrapper "${nrd}/wrapper.sh" "$(meta_get "${rd}" WORKTREE)" "${nrd}"

    log "resume ${name}（harness=${harness} session=${sid}）→ run: ${nrd}"
    local rc=0
    bash "${nrd}/wrapper.sh" > "${nrd}/stream.log" 2>&1 || rc=$?
    note_result "${nrd}"
    return "${rc}"
}

print_run_row() { # $1=name $2=run目录
    local pid alive exit_code sid last
    pid="$(meta_get "$2" PID)"
    if [ -n "${pid}" ] && kill -0 "${pid}" 2>/dev/null; then alive="运行中"; else alive="已结束"; fi
    exit_code="$(cat "$2/exit" 2>/dev/null || printf '-')"
    sid="$(extract_session_id "$2/stream.log")"
    last="$(tail -n 1 "$2/stream.log" 2>/dev/null | cut -c1-70)"
    printf '%-14s %-8s %-10s %-9s %-22s %s\n' "$1" "$(meta_get "$2" HARNESS)" "${alive}" "exit=${exit_code}" \
        "${sid:+$(printf '%.18s…' "${sid}")}${sid:-$(printf '%.18s' "$(meta_get "$2" STARTED)")}" "${last}"
}

cmd_status() {
    if [ -n "${1:-}" ]; then
        local rd
        rd="$(readlink "${RUNS_DIR}/$1/current" 2>/dev/null)" || die "没有 $1 的派发记录"
        print_run_row "$1" "${rd}"
        echo "  run    : ${rd}"
        echo "  task   : $(head -c 120 "${rd}/task.md" 2>/dev/null | tr '\n' ' ')"
        echo "  report : $([ -f "${rd}/report.md" ] && echo "有（${rd}/report.md）" || echo '还没有')"
        echo "  最近输出:"
        tail -n 5 "${rd}/stream.log" 2>/dev/null | cut -c1-120 | sed 's/^/    /'
        return 0
    fi
    printf '%-14s %-8s %-10s %-9s %-22s %s\n' "name" "harness" "状态" "" "session/started" "最新输出"
    local d n
    for d in "${RUNS_DIR}"/*/current; do
        [ -L "${d}" ] || continue
        n="$(basename "$(dirname "${d}")")"
        print_run_row "${n}" "$(readlink "${d}")"
    done
    return 0
}

cmd_stop() {
    [ -n "${1:-}" ] || { usage; die "stop 需要 <name>"; }
    local rd pid
    rd="$(readlink "${RUNS_DIR}/$1/current" 2>/dev/null)" || die "没有 $1 的派发记录"
    pid="$(meta_get "${rd}" PID)"
    [ -n "${pid}" ] && kill -0 "${pid}" 2>/dev/null || die "${1} 没有在运行的进程"
    kill -TERM "${pid}"
    local i=0
    while kill -0 "${pid}" 2>/dev/null && [ "${i}" -lt 10 ]; do sleep 1; i=$((i+1)); done
    if kill -0 "${pid}" 2>/dev/null; then kill -9 "${pid}"; warn "SIGKILL 兜底"; fi
    printf '143' > "${rd}/exit"   # 人工停止的标记（wrapper 的 trap 可能记成 0）
    log "已停止 ${1}（exit=143, run: ${rd}）"
}

case "${1:-}" in
    start)  shift; cmd_start "$@" ;;
    resume) shift; cmd_resume "$@" ;;
    status) shift; cmd_status "$@" ;;
    stop)   shift; cmd_stop "$@" ;;
    -h|--help|*) usage; [ "${1:-}" = "-h" ] || [ "${1:-}" = "--help" ] || exit 1 ;;
esac
