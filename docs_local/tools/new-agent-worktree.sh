#!/bin/sh
# new-agent-worktree.sh —— 为某个机制（或某个 agent）派生一个独立工作区。
#
# 为什么用 worktree 而不是人手一份 clone：共享对象库（省磁盘）、共享分支视图
# （合并就在 hub 里做），同时**每个工作区有自己的工作目录**——插件源码、场景测试、
# build/ 全部独立。原流程最大的坑是"两个 agent 改同一个未跟踪文件，谁后写谁赢、
# git 不给任何警告"，worktree 从根上消除它。
#
# 用法（在 hub 里跑）：docs_local/tools/new-agent-worktree.sh <名字> [基线分支]
#   docs_local/tools/new-agent-worktree.sh vanish
#   docs_local/tools/new-agent-worktree.sh extra-spirit main-local
#
# 建立后：~/.agent-workspaces/workspaces/sbs-local/agents/<名字>/（2026-10-24 迁移）分支 feat/<名字>
set -e
NAME="$1"
BASE="${2:-main-local}"
if [ -z "$NAME" ]; then
    echo "用法: $0 <名字> [基线分支]" >&2
    exit 1
fi

HUB="$(git rev-parse --show-toplevel)"
PROJECT_DIR="$(dirname "$HUB")"
WT="${SBS_AGENTS_DIR:-$HOME/.agent-workspaces/workspaces/sbs-local/agents}/$NAME"

# 原仓库位置：从 hub 里的 scripts 软链反推（软链是我们建的，指向原仓库）
SBS_SRC="${SBS_SRC:-$(dirname "$(readlink "$HUB/scripts" 2>/dev/null || echo "$PROJECT_DIR/Simulation-battle-system/scripts")")}"
if [ ! -d "$SBS_SRC/scripts/data/processed" ]; then
    echo "⚠️ 找不到原仓库的 scripts/data/processed（推出为 ${SBS_SRC}）" >&2
    echo "   用 SBS_SRC=/path/to/Simulation-battle-system $0 $NAME 指定" >&2
    exit 1
fi

if [ -e "$WT" ]; then
    echo "已存在：$WT" >&2
    exit 1
fi

mkdir -p "$PROJECT_DIR/agents"
git -C "$HUB" worktree add "$WT" -b "feat/$NAME" "$BASE"

# 只读大件必须软链进来：运行期按**相对路径**加载数据库（scripts/data/...）与插件
# （resources/<lib>/...），而它们不随 worktree 物化（未跟踪），所以每个工作区各建一次。
ln -sfn "$SBS_SRC/scripts" "$WT/scripts"
ln -sfn "$SBS_SRC/reference" "$WT/reference"

cat <<EOF
✅ 工作区已建立
   路径：${WT}
   分支：feat/${NAME}（基线 ${BASE}）

下一步：
   cd ${WT}
   cmake -B build && cmake --build build -j8

⚠️ 构建与运行都必须在**这个目录内**：运行时按相对路径加载
   scripts/data/processed/seer_unity.sqlite 和 resources/<lib>/liblib_1.dylib，
   CWD 决定用哪一份——这正是各 agent 互不干扰的原因。
EOF
