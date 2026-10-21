#!/bin/bash
# author.sh —— 夜跑"自主立单"调度器：唯一制定权抢占 + 失败者条件等待（2026-10-02）。
#
# 背景：看板清空且未收到停止信号时，多会话不再各自终止——抢占【唯一的探索制定权】；
# 抢到的会话去探索/立单（taskctl alloc + add），抢不到的在本次工具调用内阻塞轮询
# （零 API 消耗），直到 ①新任务落盘（谓词=看板非空）②拿到轮转到的制定权 ③到停止点。
#
# 用法：author.sh <会话名> [超时秒]        （默认超时 = WAIT_TIMEOUT）
# 退出码协议（与 claim.sh 同族）：
#   0 + stdout "AUTHOR" → 拿到唯一制定权：去探索 → taskctl alloc/add 立单 → author_done.sh
#   0 + stdout "CLAIM"  → 队列已有任务（有人立完/本来就剩活）→ 直接回主循环 claim
#   2 + stdout STOP 原因 → 触发停止条件
#   75                   → 等待超时（可再次调用）
#
# 制定权语义：
#   - 锁 = $NIGHT_DIR/author.lock（mkdir 原子 + pid + owner）。
#   - ⚠️ 不用 _sweep_stale 60s 回收：制定者拿到锁后 author.sh 就退出了（探索用的是
#     agent 会话本身，不是这个进程），持有者 pid 必然"已死"是常态——死锁回收改为
#     【时长过期】：锁目录超过 AUTHOR_MAX 秒视为遗弃，可被后来者接管（最坏情况=
#     双制定者各立一单，taskctl add 同号去重 + 行追加在 flock 内，无损坏）。
#   - 防霸占：author_done.sh 落 author.last（会话+时刻）；同会话冷却
#     AUTHOR_COOLDOWN 秒内不再接盘（轮转给别的等待者），冷却过点自然可再任。
set -u
. "$(dirname "$0")/lib.sh"

SESSION="${1:-night-x}"
TIMEOUT="${2:-$WAIT_TIMEOUT}"
AUTHOR_LOCK="$NIGHT_DIR/author.lock"
AUTHOR_LAST="$NIGHT_DIR/author.last"
AUTHOR_MAX="${AUTHOR_MAX:-1800}"          # 制定权最长持有秒数（过期可被接管）
AUTHOR_COOLDOWN="${AUTHOR_COOLDOWN:-600}" # 同会话卸任后再任的冷却秒数

author_expired() {  # 锁存在但足够老 → 遗弃接管
    [ -d "$AUTHOR_LOCK" ] && _is_old "$AUTHOR_LOCK" "$AUTHOR_MAX"
}

in_cooldown() {  # 上任者是本会话且卸任未满冷却
    [ -f "$AUTHOR_LAST" ] || return 1
    local last_owner last_ts now_s
    read -r last_owner last_ts <<< "$(cat "$AUTHOR_LAST" 2>/dev/null || echo "? 0")"
    [ "$last_owner" = "$SESSION" ] || return 1
    now_s="$(date +%s)"
    [ "$now_s" -lt "$(( last_ts + AUTHOR_COOLDOWN ))" ]
}

acquire() {  # 0=拿到 1=被占/冷却中（过期锁在此回收）
    if author_expired; then
        log "制定权过期接管（>$AUTHOR_MAX s）：$AUTHOR_LOCK"
        rm -rf "$AUTHOR_LOCK"
    fi
    mkdir "$AUTHOR_LOCK" 2>/dev/null || return 1
    echo $$ > "$AUTHOR_LOCK/pid"
    echo "$SESSION" > "$AUTHOR_LOCK/owner"
    if in_cooldown; then
        rm -rf "$AUTHOR_LOCK"   # 冷却中：让给别的等待者，留在循环里继续等
        return 1
    fi
    return 0
}

deadline=$(( $(date +%s) + TIMEOUT ))
while :; do
    # ① 谓词：有活先干活（有人立完单 / 本来就剩活）
    n="$(v2_pending_count)"
    if [ "$n" -gt 0 ]; then
        echo "CLAIM"
        exit 0
    fi
    # ② 停止判定（哨兵 / 停止窗口）
    if reason="$(check_stop)"; then
        log "$reason"
        echo "STOP"
        exit 2
    fi
    # ③ 抢唯一制定权
    if acquire; then
        log "拿到自主立单权（${SESSION}）——探索后 taskctl alloc/add，收尾调 author_done.sh"
        echo "AUTHOR"
        exit 0
    fi
    # ④ 阻塞等待（本进程内 sleep，零 API 消耗）
    now_s="$(date +%s)"
    if [ "$now_s" -ge "$deadline" ]; then
        log "author 等待超时（${TIMEOUT}s）——可再次调用"
        exit 75
    fi
    remain=$(( deadline - now_s ))
    sleep $(( remain > 15 ? 15 : remain > 1 ? remain : 1 ))
done
