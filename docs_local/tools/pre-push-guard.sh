#!/bin/sh
# pre-push 护栏 —— 本地-公开双仓同史镜像下，拦「真正不能见光」的路径。
#
# 装到哪：原仓库 .git/hooks/pre-push（那里 origin 才是 GitHub），
#         以及 hub 的 .git/hooks/pre-push（防以后给 hub 加了公开远端）。
# 装法：cp docs_local/tools/pre-push-guard.sh .git/hooks/pre-push && chmod +x .git/hooks/pre-push
#
# ⚠️ 2026-11-20 口径收窄（用户拍板：效果库/场景/内部文档开放贡献）：
#   resources/ test/ docs_local/ tools/agents|taskctl 已全部公开
#   （双仓同史镜像模型下它们随每次推送走，路径拦截对 tracked 路径无意义），
#   从拦截清单移除。
#   ⚠️ tools/night（夜跑工具链）**未并入公开仓**（hub 无此目录，oss-sync 同步时按
#   灰名单跳过）——它不在本 hook 拦截清单里是因为 hub 根本没有这目录可推，
#   注释口径 2026-11-21 更正。拦截清单只剩**未入库的数据大件 + 灰色逆向内容**：
#     reference/ scripts/data/   语料与官方数据（从未入库，保持如此）
#     tools/official/            官方服对接（本机自用，勿公开——增量即拦）
#     官方对战协议采样*.md        协议逆向文档（双保险）
#   注意：对 tracked 路径，本护栏只在「待推提交触碰它们」时强制人工确认
#   （ALLOW_LOCAL_PATHS_PUSH=1 放行）；要永久移除公开历史里的存量，得走
#   filter-repo 历史手术，那是另一个决定。

[ "${ALLOW_LOCAL_PATHS_PUSH:-0}" = "1" ] && exit 0

FORBIDDEN='^reference/|^scripts/data/|^tools/official/|官方对战协议采样'
ZERO=0000000000000000000000000000000000000000
violated=0

while read -r local_ref local_sha remote_ref remote_sha; do
    [ "$local_sha" = "$ZERO" ] && continue          # 删除远端分支，无事可查
    if [ "$remote_sha" = "$ZERO" ]; then
        # 新分支：查所有尚未存在于任何远端引用里的提交
        range="$local_sha --not --remotes"
    else
        range="$remote_sha..$local_sha"
    fi
    # core.quotePath=false：中文路径如实输出，否则被转义成 \xxx 匹配不到
    bad=$(git -c core.quotePath=false log --name-only --pretty=format: $range 2>/dev/null \
          | sed '/^$/d' | sort -u | grep -E "$FORBIDDEN" || true)
    if [ -n "$bad" ]; then
        violated=1
        echo "" >&2
        printf '❌ 推送被拒绝（%s -> %s）：提交里含只应留在本地的路径\n' \
            "$local_ref" "$remote_ref" >&2
        echo "$bad" | head -20 | sed 's/^/     /' >&2
    fi
done

if [ "$violated" = "1" ]; then
    cat >&2 <<'EOF'

   这些内容按 2026-11-20 口径仍不进公开仓库：
     reference/ scripts/data/   语料与官方数据大件（未入库，保持如此）
     tools/official/            官方服对接工具（本机自用；已在公开历史的存量
                                另行决定，新增增量一律拦下）
     官方对战协议采样*.md        协议逆向文档

   确实要推（一次性知情放行）：ALLOW_LOCAL_PATHS_PUSH=1 git push ...
EOF
    exit 1
fi
exit 0
