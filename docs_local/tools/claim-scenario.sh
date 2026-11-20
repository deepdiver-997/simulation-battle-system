#!/bin/sh
# claim-scenario.sh —— 场景号并发安全申请器（2026-09-26）
#
# 解决的问题：两个 agent 各自的 worktree 都用"下一号"建场景文件，合并 main-local 时
# 撞号（128 实际撞过一次：web-console vs yanmie，后者被迫改号 132）。
#
# 用法（hub 或任意 worktree 里都行，占位文件始终落在 hub/main-local 上）：
#   docs_local/tools/claim-scenario.sh zhakes_4762          # 申请并提交占位
#   docs_local/tools/claim-scenario.sh --dry-run zhakes_4762 # 只看会分配到几号
#
# 机制：
#   · 序号 = hub 的 test/scenario/ 里 scenario_NNN_*.cpp 的 max(NNN)+1（以 main-local
#     为唯一权威——所以"先占号、后开发"：开工前申请，分支合并永远不撞号）；
#   · 并发安全 = mkdir 原子锁（扫描+创建+提交在临界区内完成；git 提交本身也被
#     index.lock 串行化，双保险）；
#   · 占位文件是**可编译的最小骨架**（空文件会让场景 GLOB 编出一个无 main 的 target，
#     全量回归直接挂）——领号后把断言填进这个文件，合并时是你自己分支上的内容覆盖。
#   · 前缀规则：[a-z0-9_]，不以数字开头；同前缀重复申请会被拒绝。
set -eu

DRY_RUN=0
if [ "$1" = "--dry-run" ]; then
    DRY_RUN=1
    shift
fi

PREFIX="$1"
if [ -z "$PREFIX" ]; then
    echo "用法: $0 [--dry-run] <场景前缀>   例: $0 zhakes_4762" >&2
    exit 1
fi
case "$PREFIX" in
    *[!a-z0-9_]*|[0-9]*|"")
        echo "❌ 前缀只允许 [a-z0-9_] 且不以数字开头：$PREFIX" >&2
        exit 1
        ;;
esac

# 定位 hub（common dir = hub 的 .git；在 hub 里跑则返回 ./.git）
COMMON_DIR="$(git rev-parse --path-format=absolute --git-common-dir)"
HUB="$(dirname "$COMMON_DIR")"
SCEN_DIR="$HUB/test/scenario"

if [ ! -d "$SCEN_DIR" ]; then
    echo "❌ hub 场景目录不存在：$SCEN_DIR" >&2
    exit 1
fi

LOCK="$HUB/.scenario_claim.lock"
if ! mkdir "$LOCK" 2>/dev/null; then
    echo "❌ 另一个申请正在进行（$LOCK 存在）；若确认是残留锁：rmdir $LOCK" >&2
    exit 1
fi
trap 'rmdir "$LOCK" 2>/dev/null || true' EXIT

# ① 分配：现有 max(NNN)+1（零填充 3 位，>=1000 自然扩位）
MAX_NUM=$(ls "$SCEN_DIR" 2>/dev/null \
    | grep -oE '^scenario_[0-9]+' | grep -oE '[0-9]+' | sort -n | tail -1)
MAX_NUM="${MAX_NUM:-0}"
NEXT_NUM=$((MAX_NUM + 1))
NUM_FMT=$(printf '%03d' "$NEXT_NUM")

TARGET="$SCEN_DIR/scenario_${NUM_FMT}_${PREFIX}.cpp"

if ls "$SCEN_DIR" | grep -q "_${PREFIX}\.cpp$"; then
    echo "❌ 前缀已存在（同前缀重复申请）：*_${PREFIX}.cpp" >&2
    exit 1
fi
if [ -e "$TARGET" ]; then
    echo "❌ 目标文件已存在：$TARGET" >&2
    exit 1
fi

if [ "$DRY_RUN" = "1" ]; then
    echo "dry-run: 将分配 ${NUM_FMT} → ${TARGET}"
    exit 0
fi

# ② 占位骨架（可编译、回归可通过；内容留待领号者填写）
cat > "$TARGET" <<EOF
// 场景 ${NUM_FMT} —— ${PREFIX}（占位骨架，claim-scenario.sh 生成）
// 领号后在此填写：锁定的官方口径 / 依据（idx / 实测日期）/ 断言组。
#include "scenario_harness.h"

int main() {
    // TODO: 场景断言（参考 scenario_harness.h 头注释的用法）
    return 0;
}
EOF

# ③ 提交到 hub 的 main-local（ignored 路径需 -f；先 add 再 cd，路径按绝对写）
git -C "$HUB" add -f "$TARGET"
git -C "$HUB" commit -m "chore(scenario): 占号 ${NUM_FMT} ${PREFIX}（claim-scenario 占位骨架）" \
    --no-gpg-sign --quiet

echo "✅ 已占号 ${NUM_FMT}"
echo "   文件：${TARGET}"
echo "   分支：$(git -C "$HUB" rev-parse --abbrev-ref HEAD)（占位提交已完成，回归骨架可通过）"
echo "   下一步：在你自己的 worktree 里 fill 这个文件的断言内容。"
