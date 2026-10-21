#!/bin/bash
# author_done.sh —— 自主立单收尾：释放唯一制定权（author.sh 的配对件，2026-10-02）。
# 用法：author_done.sh <会话名>
#   - 立完单（taskctl add 至少一单）或探索后决定空手而归，都必须调本件收尾；
#   - 只释放本会话持有的锁（owner 匹配），误调不伤他人；
#   - 卸任时刻落 author.last → 同会话冷却（AUTHOR_COOLDOWN，见 author.sh），
#     等待者按"锁空→轮转接盘 / 队列非空→CLAIM"自然醒来，无需专门广播。
set -u
. "$(dirname "$0")/lib.sh"

SESSION="${1:-?}"
AUTHOR_LOCK="$NIGHT_DIR/author.lock"
AUTHOR_LAST="$NIGHT_DIR/author.last"

if [ -d "$AUTHOR_LOCK" ] && [ "$(cat "$AUTHOR_LOCK/owner" 2>/dev/null || echo "?")" = "$SESSION" ]; then
    rm -rf "$AUTHOR_LOCK"
    log "制定权已释放（${SESSION}）"
else
    log "本会话未持有制定权（无操作）"
fi
echo "$SESSION $(date +%s)" > "$AUTHOR_LAST"
