#!/bin/bash
# tools/agents/night_supervisor.sh —— 夜跑监督循环（编排会话用）
#
# 职责（用户 2026-10-22 口径）：
#   ① 保证并发 agent 数量始终 ≥2（不足就补起）；
#   ② 不打爆硬盘（磁盘预算 + 每单 worktree 用完即清）；
#   ③ 定期把 kit 工作区分支汇入 integration 批（验收侧一次过门）；
#   ④ 收敛重复身份/幽灵单（多进程 resume 造成的残留）。
#
# 用法：tools/agents/night_supervisor.sh <轮次>   （一轮 ≈ 一次巡检）
set -u
HUB="/Users/zhuhongrui/code/c++/project/sbs-local"
WT_ROOT="$HOME/.agent-workspaces/workspaces/sbs-local"
LOG="$HUB/.git/agents/night_supervisor.log"
mkdir -p "$(dirname "$LOG")"

log() { printf '%s %s\n' "$(date '+%Y-%m-%d %H:%M:%S')" "$*" | tee -a "$LOG"; }

# ── ① 磁盘：超阈值就清 worktree 的 build/（源码留着，提交在分支上）────────
free_gb=$(df -g / | awk 'NR==2 {print $4}')
log "磁盘剩余 ${free_gb}G"
if [ "${free_gb:-0}" -lt 40 ]; then
    log "⚠️ 磁盘不足 40G —— 清理各单 worktree 的 build/ 目录（提交已在 feat 分支上）"
    for d in "$HUB"/../agents/*/ "$WT_ROOT"/*/agents/*/; do
        [ -d "${d}build" ] && { rm -rf "${d}build" && log "  - 清 ${d}build"; }
    done
fi

# ── ② agent 数量：进程数不足 2 就补起 ────────────────────────────────
# 逐个 agent 判活（按派发记录的 PID 探活，不靠 pgrep 匹配命令行——命令行里带
# "名字：xxx"，能区分同名不同实例）。**只补真正死掉的那个**：
# 早先版本对 spec 列表 break 在第一项 → 永远只重启 jbatch，kbatch 死了也不管；
# 且不判活就补 → 给活着的 agent 再起一个同身份实例（重复身份事故的源头）。
AGENT_SPECS="jbatch:claude kbatch:claude"

agent_alive() {  # $1=agent 名 → 0=活 1=死/无记录
    local name="$1" meta pid
    meta="$(readlink "${HUB}/.git/agents/runs/${name}/current" 2>/dev/null)" || return 1
    [ -f "${meta}/meta" ] || return 1
    pid="$(sed -n 's/^PID="\(.*\)"$/\1/p' "${meta}/meta" 2>/dev/null)"
    [ -n "${pid}" ] || return 1
    kill -0 "${pid}" 2>/dev/null
}

alive=0
for spec in ${AGENT_SPECS}; do
    aname="${spec%%:*}"
    if agent_alive "${aname}"; then
        alive=$((alive + 1))
    fi
done
log "存活 agent：${alive}（按派发记录 PID 判活）"
if [ "${alive}" -lt 2 ]; then
    log "补起 agent（目标并发 2）"
    for spec in ${AGENT_SPECS}; do
        aname="${spec%%:*}"; harness="${spec##*:}"
        [ "${alive}" -ge 2 ] && break
        [ -d "$WT_ROOT/$aname" ] || continue
        if agent_alive "${aname}"; then
            continue   # 活着的不碰
        fi
        ( cd "$WT_ROOT/$aname" && "$HUB/tools/agents/spawn.sh" start "$harness" "$aname" task.md ) \
            >>"$LOG" 2>&1 && { log "  + 启动 ${aname}（${harness}）"; alive=$((alive + 1)); } \
                         || log "  ! 启动 ${aname} 失败"
    done
fi

# ── ③ 产出汇入 integration 批 ────────────────────────────────────────
for b in jbatch kbatch; do
    br="feat/$b"
    git -C "$HUB" rev-parse --verify "refs/heads/$br" >/dev/null 2>&1 || continue
    ahead=$(git -C "$HUB" rev-list --count "main-local..$br" 2>/dev/null || echo 0)
    [ "${ahead:-0}" -gt 0 ] || continue
    # 汇入（冲突时 collect.sh 会打印解法并退出，不自动解）
    if "$HUB/tools/agents/collect.sh" "$b" >>"$LOG" 2>&1; then
        log "✓ $br 已汇入 integration 批（领先 main-local ${ahead} 提交）"
    else
        log "⚠️ $br 汇入冲突——需人工/agent 在 staging 里解（见 .git/agents/integration.lock 同目录日志）"
    fi
done

# ── ④ 身份卫生：released 且无 busy 的多余身份清掉；🔄 行属主已释放的退回 ──
python3 - <<'PY' 2>/dev/null || true
import json, os
p = os.path.join(os.environ.get("HUB", "/Users/zhuhongrui/code/c++/project/sbs-local"),
                 "build/night/agents.json")
if not os.path.exists(p):
    raise SystemExit(0)
d = json.load(open(p))
agents = d.get("agents", d)
n = 0
for aid, a in (agents.items() if isinstance(agents, dict) else []):
    if a.get("status") == "released" and a.get("busy"):
        a["busy"] = None
        n += 1
if n:
    json.dump(d, open(p, "w"), ensure_ascii=False, indent=2)
    print(f"清理 {n} 个已释放身份的残留 busy")
PY

log "巡检结束"
