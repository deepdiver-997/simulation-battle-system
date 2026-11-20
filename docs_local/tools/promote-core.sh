#!/bin/sh
# promote-core.sh —— 把 hub 里合并好的成果，**只取内核路径**提升到 hub 的 main 分支。
#
# 取代"rsync 搬文件"：按路径、原子、可 review、可回滚。插件/场景测试/内部文档
# 天然留在 main-local，永不进 main（也就永不进公开仓库）。
#
# 用法（在 hub 里跑）：docs_local/tools/promote-core.sh <来源分支> ["提交说明"]
#   docs_local/tools/promote-core.sh main-local "feat: 消逝机制内核支持"
#
# ⚠️ 本脚本**不推送**。推送到 GitHub 由你在原仓库里 review 后自己做。
set -e
BRANCH="$1"
MSG="${2:-feat: 内核更新}"
if [ -z "$BRANCH" ]; then
    echo "用法: $0 <来源分支> [\"提交说明\"]" >&2
    exit 1
fi

HUB="$(git rev-parse --show-toplevel)"
PROJECT_DIR="$(dirname "$HUB")"
# 源仓（sbs-local）路径：优先环境变量 → 软链反推 → **按 hub 同级目录兜底**。
# ⚠️ 2026-10-22 修：软链在分支切换里被吞时，readlink 返回空 → dirname 得 "." →
#    SBS_SRC 变成当前目录，ensure_scripts_symlink 补出**指向错误位置**的链
#    （实测：hub 停在 main 分支、scripts/ 未跟踪 → 软链指向不存在的路径 →
#    引擎找不到 scripts/data/processed/seer_unity.sqlite → 673 场景里 3 个红）。
#    兜底：hub 的同级目录里找 sbs-* 仓库。
if [ -z "${SBS_SRC:-}" ]; then
    SBS_SRC="$(dirname "$(readlink "$HUB/scripts")")"
    [ -n "$SBS_SRC" ] && [ "$SBS_SRC" != "." ] || SBS_SRC=""
fi
if [ -z "$SBS_SRC" ]; then
    for cand in "$HUB"/../sbs-*; do
        if [ -d "$cand/.git" ] || [ -f "$cand/.git" ]; then
            SBS_SRC="$(cd "$cand" && pwd)"
            echo "  - 软链缺失，按同级目录定位源仓：$SBS_SRC" >&2
            break
        fi
    done
fi
[ -n "$SBS_SRC" ] || { echo "❌ 无法定位源仓（sbs-local）：设 SBS_SRC 环境变量后重试" >&2; exit 1; }

# 内核路径白名单：只有这些会进 main
# examples/（2026-10-21 加入）：公开示例包——免数据库的上手闭环（手工装配 + 自制魂印
# + 断言），**不含任何官方效果实现或场景语料**，是社区上手的入口。
# CONTRIBUTING.md（2026-10-22 加入）：公开 README 的「贡献」段与「数据与插件」段都
# 链到它（分批开放计划在里头），**之前没随仓发布 → 公开仓上那两个链接是死的**。
# 内容只有贡献规范/差异报告格式/法律边界，无官方数据、无效果实现。
CORE_PATHS="include src CMakeLists.txt tools docs examples README.md CONTRIBUTING.md CLAUDE.md PROVENANCE.md LICENSE report.md"

# 灰名单（2026-10-23 用户口径）：**个人开发流程**类工具不进公开仓。
#   · tools/official —— 官方对战采样/逆向工具（更早的口径）
#   · tools/night   —— 夜跑编排（身份注册表、抢单、策略档、停止窗口）
#   · tools/agents  —— 多 agent kit 的项目策略层（任务模板、worktree 生命周期、
#                      harness 参数、构建锁）——含本机协作约定，属个人工作流
# 这些路径在 CORE_PATHS 的 `tools` 之下，检出让它们先落地、再由下面的排除段退栈。
GREY_PATHS="tools/official tools/night tools/agents tools/taskctl.py"
# taskctl.py（2026-10-23 加入）：夜跑任务看板工具，硬编码 docs_local/ 看板路径
# （11 处）→ 公开检出里既跑不起来、又暴露个人任务流程。
# 其中 main-local **跟踪**的子集：退栈后可以连工作区一起删（收尾 checkout 会还原）；
# tools/official 不在其列（它被 gitignore，删了就真丢了，走 hold/restore）。
GREY_TRACKED="tools/night tools/agents tools/taskctl.py"

cd "$HUB"

# 刷新来源分支 ref（2026-10-22 补）：hub 是**独立仓库**，它的 `main-local` 只是上次
# fetch 的快照。源仓新增提交后若不刷新，本脚本会拿**旧快照**做 diff → 误报
# "main 无变化"，新内核改动被静默跳过（实测：3e3ea9f8 vs 3addc2d3，差 3 个提交）。
# 每次都从源仓强制更新该 ref。
# --update-head-ok：hub 此刻常正停在 ${BRANCH} 上，git 默认拒绝 fetch 更新当前
# 已检出分支；我们紧接着就 checkout main，工作区不会被这份 ref 影响。
git fetch -q --force --update-head-ok "$SBS_SRC" "+refs/heads/${BRANCH}:refs/heads/${BRANCH}" 2>/dev/null || {
    echo "❌ 无法从源仓刷新分支 ${BRANCH}（${SBS_SRC}）" >&2
    exit 1
}
echo "  - 已刷新来源分支：${BRANCH} = $(git rev-parse --short "$BRANCH")"

# 对齐工作区（2026-10-22 补）：--update-head-ok 只挪 ref，工作区/索引仍停在旧提交
# → 下一步 `git checkout main` 会报"local changes would be overwritten"。
# hub 是**纯镜像**（权威副本在源仓 sbs-local），此处 --hard 对齐是安全的；
# 未跟踪文件（tools/official、reference 等）不受 --hard 影响。
if [ "$(git rev-parse --abbrev-ref HEAD)" = "$BRANCH" ]; then
    git reset -q --hard "$BRANCH"
    echo "  - 工作区已对齐到 ${BRANCH}"
fi

# 安全检查（2026-10-01 放宽）：原仓库 main 必须是 hub main 的祖先（hub main 可以
# 领先——两侧各自提交后的汇合提交）；否则拒绝——防止把别处的内核改动回退掉。
HUB_MAIN="$(git rev-parse main)"
SRC_MAIN="$(git -C "$SBS_SRC" rev-parse main)"
if ! git merge-base --is-ancestor "$SRC_MAIN" "$HUB_MAIN"; then
    cat >&2 <<ERR
❌ 拒绝执行：原仓库 main 不是 hub main 的祖先（历史分叉）
   hub main     = $HUB_MAIN
   原仓库 main  = $SRC_MAIN
   先把原仓库 main 并入 hub main（临时 worktree merge），再跑本脚本。
ERR
    exit 1
fi

# 暂存目录：必须在切分支**之前**就可用——切 main 要先让开 scripts/
PROMOTE_HOLD="$(mktemp -d)"
# 兜底（2026-10-22）：任何**提前退出**（set -e 命中）都不得连暂存数据一起删——
# 中断时把数据留在原处并打红字，让人能手动移回。只有正常收尾（cleanup_holds）
# 才删暂存目录。
trap 'if [ -n "${SCRIPTS_HOLD:-}" ] && [ -d "${SCRIPTS_HOLD:-}" ]; then
          echo "⚠️ 中断：本地数据仍暂存在 ${SCRIPTS_HOLD}，请手动移回 scripts/data" >&2
      else
          rm -rf "${PROMOTE_HOLD}"
      fi' EXIT

resolve_path() { python3 -c 'import os,sys; print(os.path.realpath(sys.argv[1]))' "$1"; }

SCRIPTS_HOLD=""   # 非空 = 有一份被移出的 data 目录待归位

hold_scripts_data() {
    SCRIPTS_HOLD=""
    # 只暂存"实体目录里的 data"——软链的 data 属于目标，不归本次动
    [ -d scripts ] && [ ! -L scripts ] && [ -d scripts/data ] || return 0
    SCRIPTS_HOLD="${PROMOTE_HOLD}/scripts_data"
    mv scripts/data "$SCRIPTS_HOLD"
    echo "  - 暂存本地数据 scripts/data（$(du -sh "$SCRIPTS_HOLD" 2>/dev/null | cut -f1)）"
}

# 切 main 之前让开 scripts/（2026-10-22 补）：
#   hub 的 main 分支**跟踪** scripts/*.py（实体目录），而 main-local 收尾时
#   scripts 是指向源仓的**软链** → `git checkout main` 直接报
#   "untracked working tree files would be overwritten by checkout: scripts"。
#   处理：软链 → 只删链（目标数据不动）；实体目录带 data → 先移出数据再删目录。
pre_checkout_scripts() {
    if [ -L scripts ]; then
        rm -f scripts
        echo "  - 切主线前移除 scripts 软链（目标数据不动）"
    elif [ -d scripts ]; then
        hold_scripts_data
        rm -rf scripts
        echo "  - 切主线前移出 scripts/（数据已暂存）"
    fi
}

pre_checkout_scripts
git checkout -q main
# scripts 目录自愈（2026-10-01 初版；2026-10-22 重写 —— 修"自毁式软链"事故）
#
# 两侧对 scripts/ 的需求相反：
#   · hub 的 main 分支：scripts/ 是**跟踪的实体目录**（公开数据管线 .py）——
#     切到 main 时 git 会把 main-local 的软链换成实体目录；
#   · main-local / 源仓：scripts/data/ 要装**未入库的本地数据**（sqlite 等，
#     gitignored，**没有第二份备份**）。
#
# ⚠️ 2026-10-22 事故（真数据丢失）：旧实现 `rm -rf scripts && ln -s "$SBS_SRC/scripts"`
#    ① hub 的 scripts 是落着数据的**实体目录**时，`rm -rf` 删掉了唯一一份
#      `data/processed/seer_unity.sqlite`（36MB，重建要重跑 fetch→decode→import）；
#    ② 源仓的 scripts 本身指回 hub 时，新建的链成**环**（hub→src→hub），
#      引擎读库直接报 "Too many levels of symbolic links"，全量场景集体红。
#
# 新三条规矩：
#   ① 删任何东西之前，先把 `scripts/data`（未跟踪数据）**移出到 PROMOTE_HOLD**；
#   ② 目标必须解析成**实体目录**，且不能指回 hub（环检测 → 就地改造成实体目录）；
#   ③ 收尾时数据必须归位，否则报错退出（宁可中断也不静默丢数据）。
ensure_scripts_symlink() {
    local want="$SBS_SRC/scripts" real hub_real
    hub_real="$(resolve_path "$HUB")"

    # ① 环检测：源仓的 scripts 解析后落在 hub 里 = 建链必成环 → 就地改造成实体目录
    real="$(resolve_path "$want")"
    case "$real" in
        "$hub_real"|"$hub_real"/*)
            echo "  - ⚠️ 源仓 scripts 解析到 hub 内（建链会成环）——改造成实体目录：$want"
            rm -f "$want"
            mkdir -p "$want/data"
            real="$(resolve_path "$want")"
            ;;
    esac

    # ② hub 侧：实体目录先移出数据，再换成软链
    hold_scripts_data
    if [ -e scripts ] || [ -L scripts ]; then
        rm -rf scripts
    fi
    ln -s "$want" scripts
    echo "  - scripts 软链已还原（→ ${want}）"

    # ③ 数据归位：目标里没有 data 就把暂存的搬回去
    if [ -n "$SCRIPTS_HOLD" ]; then
        if [ -e "$real/data" ]; then
            echo "  - ⚠️ 目标已有 scripts/data，暂存副本保留在 ${SCRIPTS_HOLD}（请人工核对后删）"
        else
            mv "$SCRIPTS_HOLD" "$real/data"
            SCRIPTS_HOLD=""
            echo "  - 本地数据已归位（$real/data）"
        fi
    fi
    if [ -f scripts/data/processed/seer_unity.sqlite ]; then
        echo "  - 引擎数据检查：$(du -h scripts/data/processed/seer_unity.sqlite | cut -f1) OK"
    else
        echo "  - ⚠️ scripts/data/processed/seer_unity.sqlite 不存在——引擎跑不起来（若是全新检出属正常，先跑数据管线）"
    fi
}
# 逐路径处理（2026-09-27 修）：白名单里某路径在来源分支已删时（实测 report.md），
# 整串 `git checkout $BRANCH -- $CORE_PATHS` 直接 pathspec 报错退出（set -e），
# 且把工作树丢在 main 上。改为：存在于来源分支的正常检出；
# 只存在于 main 的顶层文件当场删除——顺带把"顶层文件删除同步"也补上
# （下面的目录删除同步只覆盖 DEL_DIRS 四个目录，管不到顶层文件）。
EXIST_PATHS=""
PROMOTE_TMP_FILES="$(mktemp)"
for p in $CORE_PATHS; do
    if git cat-file -e "$BRANCH:$p" 2>/dev/null; then
        EXIST_PATHS="$EXIST_PATHS $p"
    elif git ls-files --error-unmatch "$p" >/dev/null 2>&1; then
        git rm -q -f "$p"
        echo "  - 删除（来源分支已删）：$p"
    fi
done
if [ -n "$EXIST_PATHS" ]; then
    git checkout "$BRANCH" -- $EXIST_PATHS
    # ⚠️ **只 add 来源分支真正跟踪的文件**（2026-10-22 修）：
    #    `git checkout $BRANCH -- <path>` 会把**工作区里被 .gitignore 忽略**的文件
    #    也检出并落进暂存区（实测：tools/agents/agents.env 本机参数、tools/**/__pycache__/*.pyc
    #    编译缓存——它们不在 BRANCH 跟踪集里，却在 main 上被公开过一次）。
    #    提升面 = 白名单路径 ∩ 来源分支跟踪集。
    git reset -q
    git -c core.quotepath=false ls-tree -r --name-only "$BRANCH" -- $EXIST_PATHS > "$PROMOTE_TMP_FILES"
    while IFS= read -r f; do
        [ -n "$f" ] || continue
        # ⚠️ 跳过灰名单（2026-10-22/23）：见顶部 GREY_PATHS，与 pre-push FORBIDDEN 同口径
        #    同口径**不得进公开 main**。其中 tools/official/.gitignore 还是来源分支
        #    跟踪的文件 → 不跳过的话 `git add` 报 "ignored by .gitignore"（该 .gitignore
        #    正是它自己忽略自己）→ set -e 直接退出、提升中断。
        case "$f" in
            tools/official/*|tools/night/*|tools/agents/*|tools/taskctl.py) continue ;;
        esac
        git add "$f"
    done < "$PROMOTE_TMP_FILES"
    # 白名单里的顶层文件（如 report.md）不在 ls-tree -r 的目录输出里 → 单独补
    for p in $EXIST_PATHS; do
        if git cat-file -e "$BRANCH:$p" 2>/dev/null && ! git cat-file -t "$BRANCH:$p" | grep -q tree; then
            git add "$p"
        fi
    done
fi

# 灰色排除（2026-10-21 补，2026-10-22 修位置）：提升集含 tools，但 tools/official
# （官方对战采样/逆向工具）与 pre-push 护栏 FORBIDDEN 同口径**不得进公开 main**。
# ⚠️ 位置必须在上面的 `git add` 循环**之后**：原先放在前面时，后面的 add 又把它
# 重新加回暂存区 → `git add` 报 "ignored by .gitignore"（tools/official/.gitignore
# 是来源分支跟踪的）→ set -e 直接退出，提升中断。现在 add 完再整目录退栈。
# 切回来源分支前再从盘上清掉（来源分支跟踪着它们，检出按内容还原，删除无损）。
for gp in $GREY_PATHS; do
    git ls-files --cached -- "$gp" | grep -q . || continue
    git rm -q -r --cached "$gp"
    echo "  - 排除（灰色不入公开）：${gp}/"
done
# 被跟踪的灰名单子集：退栈后工作区还留着实体文件 → 收尾切回 main-local 时会与
# "未跟踪文件将被覆盖"撞车 → 这里一并删（main-local 跟踪它们，checkout 会还原）。
for gt in $GREY_TRACKED; do
    [ -e "$gt" ] && rm -rf "$gt" && echo "  - 清理灰名单工作区副本：${gt}/（切回分支时还原）"
done


# ⚠️ **删除同步**（2026-09-18 补）：`git checkout <ref> -- <paths>` 只会**覆盖/新增**，
# **不会删除**"main 有、来源分支已删"的文件（git 的 pathspec 检出没有删除语义）。
# 不补这一趟，公开面就会留下**被删过的死文件** —— 实测：服务端重写删掉的
# `src/fsm/iControlBlock.cpp` / `include/server.h` / `src/server/server.cpp` /
# boost 线程池两个头，第一次 promote 后全留在 main 上（CMakeLists 已不引用 → 不至于
# 构建失败，但公开面留着一份误导性的旧实现）。
# 只对**目录**类内核路径做这趟：顶层文件（CMakeLists.txt 等）不存在"被删"的场景。
DEL_DIRS="include src tools docs"
DEL_TMP_MAIN="$(mktemp)"
DEL_TMP_SRC="$(mktemp)"
# ⚠️ 2026-10-22 修：`git ls-tree` 默认对非 ASCII 路径做 **C 风格转义**
#    （"docs/00-\347\273..."）→ 出来的行带着字面引号与反斜杠 → 后续
#    `git rm <path>` 报 "pathspec did not match any files"，提升中断。
#    修法：全程关掉转义（-c core.quotepath=false）。
for d in $DEL_DIRS; do
    git -c core.quotepath=false ls-tree -r --name-only --full-tree main -- "$d"
done | sort > "$DEL_TMP_MAIN"
for d in $DEL_DIRS; do
    git -c core.quotepath=false ls-tree -r --name-only --full-tree "$BRANCH" -- "$d"
done | sort > "$DEL_TMP_SRC"
comm -23 "$DEL_TMP_MAIN" "$DEL_TMP_SRC" > "${DEL_TMP_MAIN}.del"
DEL_COUNT=0
while IFS= read -r f; do
    if [ -n "$f" ]; then
        git rm -q -f "$f"
        DEL_COUNT=$((DEL_COUNT + 1))
        echo "  - 删除（来源分支已删）：${f}"
    fi
done < "${DEL_TMP_MAIN}.del"
rm -f "$DEL_TMP_MAIN" "$DEL_TMP_SRC" "${DEL_TMP_MAIN}.del" "$PROMOTE_TMP_FILES"
if [ "$DEL_COUNT" -gt 0 ]; then
    echo "✅ 同步删除了 ${DEL_COUNT} 个死文件"
fi

# 灰色残留处理（2026-10-22 重写）：tools/official 退栈后是**未跟踪**文件，
# 切回来源分支会被 git 拦（"untracked would be overwritten"）。
# ⚠️ 旧实现是 `rm -rf tools/official`——那会**删掉未跟踪的官方线资产**（24 个文件，
#    不可从 git 恢复）。新实现：**移到临时目录 → 切回 → 原样还原**，永不删除。
#    同理，git checkout 检出的嵌套副本（tools/official/official/）一并清掉。
hold_official() {
    if [ -d tools/official ] && ! git ls-files --error-unmatch tools/official >/dev/null 2>&1; then
        mv tools/official "${PROMOTE_HOLD}/official"
        echo "  - 暂存未跟踪的 tools/official（切回后原样还原，不删除）"
    fi
    # checkout 造成的嵌套副本（如 tools/official/official/）——纯冗余，清掉
    if [ -d tools/official/official ]; then
        rm -rf tools/official/official
        echo "  - 清理嵌套冗余副本 tools/official/official/"
    fi
}
restore_official() {
    if [ -d "${PROMOTE_HOLD}/official" ]; then
        mkdir -p tools
        mv "${PROMOTE_HOLD}/official" tools/official
        echo "  - 已还原未跟踪的 tools/official"
    fi
}

# 收尾清理（2026-10-22 拆出）：必须在 ensure_scripts_symlink **之后**再删暂存目录——
# 否则会把待归位的 scripts/data 连暂存一起删掉（同一次事故的第二条腿）。
cleanup_holds() {
    [ -n "${SCRIPTS_HOLD:-}" ] && [ -d "${SCRIPTS_HOLD}" ] && {
        echo "  - ⚠️ 暂存数据未归位，保留 ${SCRIPTS_HOLD}（请人工处理）" >&2
        return 0
    }
    rm -rf "${PROMOTE_HOLD}"
}

if git diff --cached --quiet; then
    echo "main 无变化（$BRANCH 的内核部分与 main 相同）"
    hold_official
    git checkout -q "$BRANCH"
    restore_official
    ensure_scripts_symlink
    cleanup_holds
    exit 0
fi
git commit -q -m "$MSG"
NEW="$(git rev-parse --short main)"
hold_official
git checkout -q "$BRANCH"
restore_official
ensure_scripts_symlink
cleanup_holds

echo "✅ 内核已提升到 hub main：$NEW"
echo
git show --stat --oneline main | head -30
echo
cat <<EOF
────────────────────────────────────────────────────────
下一步（在原仓库里 review 后提交，本脚本不代推）：

  cd $SBS_SRC
  git fetch $HUB main
  git checkout FETCH_HEAD -- $CORE_PATHS
  git diff --cached          # ← 在这里 review
  git commit -m "$MSG"
  git push origin main       # ← 你自己确认后推

不想要了：git checkout HEAD -- $CORE_PATHS && git reset
────────────────────────────────────────────────────────
EOF
