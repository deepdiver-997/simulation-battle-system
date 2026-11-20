#!/bin/sh
# local-snapshot.sh —— 把本地维护内容（插件/场景测试/内部文档）快照进 local 分支。
#
# 为什么不用 checkout 切分支：resources/*/lib_1.cpp、test/scenario/、docs_local/ 在 main
# 里是 .gitignore 的；若 local 分支追踪了它们，checkout 回 main 会把这些文件从工作区
# 删掉（构建当场断）。本脚本用底层命令（add -f → write-tree → commit-tree → update-ref）
# 在**不动工作区、不碰 main 历史**的前提下把快照挂到 local 分支上：
#   - main / origin 推送流程完全不变（内核提交本来就在 main）；
#   - local 分支从不 checkout，只用于查看历史：git log local / git show local:<path>；
#   - 不想追踪了：git update-ref -d refs/heads/local 即整体移除。
#
# 用法：docs_local/tools/local-snapshot.sh ["快照说明"]
# ⚠️ 故意不含 scripts/（数据管线 + 大体积 sqlite，二进制不入库）。
set -e
cd "$(git rev-parse --show-toplevel)"

git add -f -- resources/moves_lib resources/soul_lib test/scenario docs_local
TREE=$(git write-tree)
PARENT=$(git rev-parse --verify refs/heads/local 2>/dev/null || true)
STAMP="$(date +%Y-%m-%d) $(date +%H:%M)"
if [ -n "$PARENT" ]; then
    COMMIT=$(git commit-tree "$TREE" -p "$PARENT" -m "local: ${1:-snapshot} ($STAMP)")
else
    COMMIT=$(git commit-tree "$TREE" -m "local: ${1:-snapshot} ($STAMP)")
fi
git update-ref refs/heads/local "$COMMIT"
git reset -q --mixed HEAD    # 索引还原到 main HEAD，本地文件回到"未追踪"常态
echo "local @ $(git rev-parse --short local)  $COMMIT"
