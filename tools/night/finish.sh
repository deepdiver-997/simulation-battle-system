#!/bin/bash
# 收尾登记：finish.sh <Dn> ok|blocked [一句话备注]
#   ok     → 看板 ✅ 完成 + 收官记录加行
#   blocked → 看板 ⛔ 阻塞（早晨人工看过原因后手动把状态改回「待领取」即可重入队）
# 自动附带分支与 commit 号；worktree 保留（早晨合并验收用，勿删）。
set -u
. "$(dirname "$0")/lib.sh"

DN="${1:-}"; VERDICT="${2:-}"; NOTE="${3:-}"
if [ -z "$DN" ] || { [ "$VERDICT" != ok ] && [ "$VERDICT" != blocked ]; }; then
    echo "用法：finish.sh <Dn> ok|blocked [备注]"; exit 64
fi

hub_lock || exit 9
wt="$AGENTS_DIR/d-$DN"; info=""
if [ -e "$wt/.git" ]; then
    br="$(git -C "$wt" rev-parse --abbrev-ref HEAD 2>/dev/null)"
    sha="$(git -C "$wt" rev-parse --short HEAD 2>/dev/null)"
    info="${br:-?} ${sha:-?}"
fi
if [ "$VERDICT" = ok ]; then
    [ -n "$NOTE" ] && NOTE="；$NOTE"
    kanban_set "$DN" "✅ 完成" "$info$NOTE"
    git_commit_kanban "chore(看板): $DN ✅ 完成（${info}）"
    printf '| %s | %s 完成（%s）%s |\n' "$(date +%Y-%m-%d)" "$DN" "$info" "${NOTE#；}" >> "$KANBAN"
else
    [ -n "$NOTE" ] || NOTE="原因未填"
    kanban_set "$DN" "⛔ 阻塞" "${info}（${NOTE}——早晨人工复核后把状态改回「待领取」重入队）"
    git_commit_kanban "chore(看板): $DN ⛔ 阻塞——${NOTE}"
fi
hub_unlock
log "已登记：$DN ${VERDICT}（${info}）——worktree 保留在 ${wt}，合并验收后再删"
