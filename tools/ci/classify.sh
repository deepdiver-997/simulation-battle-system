#!/usr/bin/env bash
# classify.sh —— PR 路径分类（纯 git/grep，无 LLM）。
# 用法：classify.sh <BASE_SHA> <HEAD_SHA> [PR 编号]
# 输出（GITHUB_OUTPUT）：tier = effects-new | effects-ruling | kernel | kernel-approved | mixed | other
# 同时把可读摘要写 GITHUB_STEP_SUMMARY；引注缺失时输出 citation_missing=1。
# 本地模拟：git fetch origin main-local 后 `tools/ci/classify.sh origin/main-local HEAD`
set -euo pipefail

BASE="${1:?usage: classify.sh BASE_SHA HEAD_SHA [PR_NUMBER]}"
HEAD="${2:?usage: classify.sh BASE_SHA HEAD_SHA [PR_NUMBER]}"
PR="${3:-}"

KERNEL_RE='^(src|include)/'
EFFECTS_RE='^(resources/|test/scenario/)'

mapfile -t ALL < <(git diff --name-only "$BASE" "$HEAD")
mapfile -t ADDED < <(git diff --name-only --diff-filter=A "$BASE" "$HEAD")

kernel=0; effects=0
kernel_files=(); effect_files=()
for f in "${ALL[@]}"; do
  if [[ $f =~ $KERNEL_RE ]]; then kernel=$((kernel+1)); kernel_files+=("$f");
  elif [[ $f =~ $EFFECTS_RE ]]; then effects=$((effects+1)); effect_files+=("$f"); fi
done

# 引注检查：新增的效果/场景文件头注释必须带证据链接（http），E0 直接拒
citation_missing=0
missing_list=()
for f in "${ADDED[@]}"; do
  [[ $f =~ $EFFECTS_RE ]] || continue
  [[ $f =~ \.(cpp|h)$ ]] || continue
  if ! head -40 "$f" 2>/dev/null | grep -qE 'https?://'; then
    citation_missing=1; missing_list+=("$f")
  fi
done

tier=other
if (( kernel > 0 && effects > 0 )); then tier=mixed
elif (( kernel > 0 )); then tier=kernel
elif (( effects > 0 )); then
  # 是否只新增（无修改/删除既有效果层文件）
  modified=0
  while IFS= read -r st; do
    [[ $st =~ ^[AM]\s ]] || continue
    case "$st" in A*) ;; *) modified=1 ;; esac
  done < <(git diff --name-status "$BASE" "$HEAD" | grep -E "$EFFECTS_RE" || true)
  if (( modified )); then tier=effects-ruling; else tier=effects-new; fi
fi

# kernel-approved 标签放行（workflow 侧把 label 传进来）
if [[ $tier == kernel && "${KERNEL_APPROVED:-0}" == "1" ]]; then tier=kernel-approved; fi

# ---- 摘要 ----
sum="${GITHUB_STEP_SUMMARY:-/dev/stdout}"
{
  echo "## PR 分类结果"
  echo "- tier: **$tier**（内核文件 $kernel，效果层文件 $effects，总计 ${#ALL[@]}）"
  (( ${#kernel_files[@]} )) && { echo "- 内核侧（需 kernel-approved 标签）："; printf '  - %s\n' "${kernel_files[@]}" | head -20; } || true
  (( citation_missing )) && { echo "- ⚠ 引注缺失（新增效果/场景文件头 40 行内无 http 链接）："; printf '  - %s\n' "${missing_list[@]}"; } || true
  case $tier in
    mixed)      echo "- **处置：自动拒绝**——效果层与内核层混碰，请拆成两个 PR。" ;;
    kernel)     echo "- **处置：等待 kernel-approved 标签**（先开 issue 讨论设计）。" ;;
    effects-new)  echo "- **处置：CI 全绿 + 引注齐 → 自动合并（tier0）。**" ;;
    effects-ruling) echo "- **处置：needs-ruling**——修改了既有效果/场景期望值（口径变更），维护者终审。" ;;
  esac
} >> "$sum"

if [[ -n "${GITHUB_OUTPUT:-}" ]]; then
  { echo "tier=$tier"; echo "citation_missing=$citation_missing"; } >> "$GITHUB_OUTPUT"
fi
[[ $tier == mixed || $tier == kernel ]] && exit 1
[[ $citation_missing == 1 ]] && exit 2
exit 0
