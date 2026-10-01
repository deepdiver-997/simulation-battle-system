#!/bin/bash
# 收尾登记（v2）：finish.sh <任务id> ok|blocked [一句话备注]
#   ok     → taskctl done（✅ 完成 + summary 落行末）
#   blocked → taskctl release（退回待领取，原因落行末——v2 语义：直接自动重入队，
#             不再有"早晨人工改回"环节；真要挂起就别 release，留 🔄 即人工挂起）
# 自动附带分支与 commit 号；worktree 保留（早晨合并验收用，勿删）。
set -u
. "$(dirname "$0")/lib.sh"

DN="${1:-}"; VERDICT="${2:-}"; NOTE="${3:-}"
if [ -z "$DN" ] || { [ "$VERDICT" != ok ] && [ "$VERDICT" != blocked ]; }; then
    echo "用法：finish.sh <Dn> ok|blocked [备注]"; exit 64
fi

hub_lock || exit 9
# worktree 目录：D 组时代是 d-$DN；taskctl v2 claim 建的是 agents/$(id小写)（claim.sh）。
# 第三退路=脚本自身所在 repo 根（worktree 副本里调 finish 时 lib.sh 的 HUB/AGENTS_DIR
# 会因 NIGHT_ROOT 上溯解析错误而指偏，见 agents/agents 事故）——脚本就在待收官 worktree 里。
wt="$AGENTS_DIR/d-$DN"
[ -e "$wt/.git" ] || wt="$AGENTS_DIR/$(printf '%s' "$DN" | tr 'A-Z' 'a-z')"
if [ ! -e "$wt/.git" ]; then
    wt="$(cd "$(dirname "$0")/../.." && git rev-parse --show-toplevel 2>/dev/null || true)"
    [ -n "$wt" ] && [ -e "$wt/.git" ] || wt="/nonexistent"
fi
info=""
if [ -e "$wt/.git" ]; then
    br="$(git -C "$wt" rev-parse --abbrev-ref HEAD 2>/dev/null)"
    sha="$(git -C "$wt" rev-parse --short HEAD 2>/dev/null)"
    info="${br:-?} ${sha:-?}"
fi
if [ "$VERDICT" = ok ]; then
    v2_done "$DN" "$info" "$sha" "$NOTE"
else
    [ -n "$NOTE" ] || NOTE="原因未填"
    v2_release "$DN" "${info} ${NOTE}"
fi
hub_unlock
log "已登记：$DN ${VERDICT}（${info}）——worktree 保留在 ${wt}，合并验收后再删"
