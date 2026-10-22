#!/usr/bin/env python3
"""check_storage_keys.py —— `std::any` 存储槽**键撞号**静态扫描器（2026-10-22）。

## 为什么需要（与 id_collision.h 是两类问题，别混）

`id_collision.h` 管的是**注册表**（效果 id / 魂印 id / 套装 id …）：同 id 两处注册
= 后者静默覆盖前者。

本脚本管的是**运行期状态槽**：
    ctx->plugin_storage      （std::map<int, std::any>，战斗级共享）
    pet.soulmark_storage     （std::map<int, std::any>，每只精灵一份）
    pet.on_stage_storage     （std::map<int, std::any>，本场上场期）
这些 map 按 **int 键**索引，**没有任何注册环节** → 运行期撞号检测**抓不到**（没有
"注册"这个时点可盯）。而撞键的后果比注册表撞号更隐蔽：
  ① 语义串台：一个键被两个不相干的机制各写各的值 → 后写的盖掉先写的；
  ② **类型串台**：`std::any` 存的值类型不一致时，`any_cast<T>` 要么抛
     `std::bad_any_cast`（崩溃），要么静默取到错的值。仓库里已有前车之鉴：
     `resources/kongyuan_runtime.h` 记着"publish 槽取 2175000 时与 armed_flag
     (owner=0,salt=0) 同键，any_cast<int&> 撞上 bool → bad_any_cast 崩在
     ROUND_START（场景 103 实测）"。

## 判定口径

键是编译期常量（`constexpr int kXxx = N;`），所以**静态可查**。本脚本：
  1. 收集各源文件里 `constexpr int` 常量（含 `static const int`）→ 名字→数值。
     ⚠️ **同名常量在不同文件里是不同的量**（仓库里 `kGateBase` 在三个文件里是
     三个不同值）→ 常量表必须**按文件**建，不能全局按名字建。
  2. 扫 `xxx_storage[表达式]` 的使用点，把表达式里的常量按**本文件**解析成数值。
  3. 同一个（存储容器, 基础键）被**多个文件**使用 → 报出来（跨 dylib 撞键风险）；
     同一文件内多次使用不算（那是同一机制的正常多点读写）。
  4. 键值落在官方 effect_id 空间（1~3000）或与已知私有段重叠的，单独提示。

## 用法

    python3 tools/check_storage_keys.py            # 报告（退出码 1 = 有跨文件撞键）
    python3 tools/check_storage_keys.py --json     # 机器可读
    python3 tools/check_storage_keys.py --quiet    # 只报跨文件撞键

退出码：0 = 无跨文件撞键；1 = 有（门禁可用）。
"""
import argparse
import json
import os
import re
import sys
from collections import defaultdict

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCAN_DIRS = ("resources", "include", "src")

# 存储容器名 → 语义说明
CONTAINERS = {
    "plugin_storage": "战斗级共享（ctx）",
    "soulmark_storage": "每只精灵（下场保留）",
    "on_stage_storage": "本场上场期",
}

# 官方 effect_id 空间（effect_icon.effect_id 1~3000）——键落在这里极易与官方语义混淆
OFFICIAL_ID_MAX = 3000

# ── 已备案的**故意**跨 dylib 共用键（不算撞键）────────────────────────────────
# 有些键就是**设计上的交接面**：moves 侧写、soul 侧读/撤（同一份状态跨库传）。
# 这类必须显式备案，否则扫描器每次都报，变成噪声而被忽略。
# 键 = (容器, 基础键) → 交接说明。
INTENTIONAL_SHARED = {
    ("plugin_storage", 4644100):
        "intentional handoff：crawford 减伤桶槽位——moves 侧写 "
        "(kCrawfordPubOppReduceBase+100+owner)、soul 侧读并撤除 "
        "(+100+side)；见 resources/crawford_runtime.h 键约定段。",
}

RE_CONST = re.compile(
    r"\b(?:constexpr|static\s+const)\s+int\s+(\w+)\s*=\s*"
    r"(?:0x[0-9a-fA-F]+|\d+)\s*;")
RE_STORE = re.compile(
    r"\b(plugin_storage|soulmark_storage|on_stage_storage)\s*\[\s*([^\]]+?)\s*\]")
RE_IDENT = re.compile(r"\b[A-Za-z_]\w*\b")
# 表达式里已知的"非键"标识符（局部变量/参数/字段）——不参与常量解析
RE_KNOWN_NONKEY = re.compile(
    r"^(?:owner|side|self|slot|ctx|c|pet|kind|mark_id|it|actor|opp|"
    r"roundCount|n|idx|i|target|winner|_)$")


def strip_comments_and_strings(text: str) -> str:
    """去注释与字符串字面量——否则文档里的 'plugin_storage[2175]' 会被当成真使用点。

    ⚠️ **必须保护 `#include` 行**：朴素的块注释正则会把 `/* ... */` 之间的
    `#include "x.h"` 一起吃掉（头文件里常有跨行的大段注释），导致常量表解析不到
    共享头里的键 → 跨文件撞键漏报。
    """
    # 先把 #include 行换成等长占位符，注释剥离后再还原。
    keep = []

    def _stash(m):
        keep.append(m.group(0))
        return "\x00%d\x00" % (len(keep) - 1)

    text = re.sub(r'^\s*#\s*include\s+[^\n]*', _stash, text, flags=re.M)
    text = re.sub(r"//[^\n]*", "", text)
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    text = re.sub(r'"(?:\\.|[^"\\])*"', '""', text)
    for i, s in enumerate(keep):
        text = text.replace("\x00%d\x00" % i, s)
    return text


def collect_sources():
    """产出 (相对路径, 去注释后的文本) 列表。"""
    out = []
    for d in SCAN_DIRS:
        base = os.path.join(REPO, d)
        if not os.path.isdir(base):
            continue
        for root, dirs, files in os.walk(base):
            dirs[:] = [x for x in dirs if x not in (".git", "build", "__pycache__")]
            for f in sorted(files):
                if not f.endswith((".h", ".cpp")):
                    continue
                p = os.path.join(root, f)
                try:
                    raw = open(p, encoding="utf-8", errors="ignore").read()
                except OSError:
                    continue
                out.append((os.path.relpath(p, REPO), strip_comments_and_strings(raw)))
    return out


def consts_of(text: str):
    """本文件里的 constexpr int / static const int → {名字: 数值}。"""
    out = {}
    for m in RE_CONST.finditer(text):
        raw = m.group(2) if m.lastindex and m.lastindex >= 2 else m.group(0)
        val_txt = re.search(r"=\s*(0x[0-9a-fA-F]+|\d+)\s*;", m.group(0))
        if val_txt:
            out[m.group(1)] = int(val_txt.group(1), 0)
    return out


RE_INCLUDE = re.compile(r'^\s*#\s*include\s+"([^"]+)"', re.M)
RE_INCLUDE_SYS = re.compile(r"^\s*#\s*include\s+<([^>]+)>", re.M)


def header_consts(rel_path: str, text: str, cache):
    """本文件**可达**的常量表 = 本文件 + 它 include 的头（递归）。

    为什么需要：共享头里定义的键常量（例：`resources/gaia_runtime.h` 的
    `kGaiaResolvingKey`）在使用点所在 .cpp 的常量表里查不到 → 跨文件撞键会漏报。

    同名常量在可达集合里指向**不同数值**时（同名不同义的常见写法）→ 该名字
    标为歧义并从表里剔除（宁漏报也不错报）。
    """
    if rel_path in cache:
        return cache[rel_path]
    own = consts_of(text)
    merged = dict(own)
    ambiguous = set()
    for m in list(RE_INCLUDE.finditer(text)) + list(RE_INCLUDE_SYS.finditer(text)):
        inc = m.group(1)
        cand = _resolve_include(rel_path, inc)
        if not cand:
            continue
        for name, val in header_consts(cand[0], cand[1], cache).items():
            if name in merged and merged[name] != val:
                ambiguous.add(name)
            else:
                merged[name] = val
    for name in ambiguous:
        merged.pop(name, None)
    cache[rel_path] = merged
    return merged


def _resolve_include(rel_path: str, inc: str):
    """把 #include 的目标解析成 (相对路径, 内容)；解析不到返回 None。"""
    base = os.path.dirname(os.path.join(REPO, rel_path))
    cands = [os.path.join(base, inc), os.path.join(REPO, inc),
             os.path.join(REPO, "include", inc),
             os.path.join(REPO, "resources", inc)]
    for c in cands:
        if os.path.isfile(c):
            try:
                raw = open(c, encoding="utf-8", errors="ignore").read()
            except OSError:
                return None
            return os.path.relpath(c, REPO), strip_comments_and_strings(raw)
    return None


def resolve(expr: str, consts):
    """把下标表达式解析成 (基础键值, 是否完全解析)。

    完全解析 = 表达式里所有标识符都能在本文件的常量表里找到（或属于已知非键名）。
    只取**数值最大的常量**作基础键：形如 `kBase + owner` 的偏移量恒小于 base，
    基础键决定"占用哪一段键空间"。
    """
    idents = RE_IDENT.findall(expr)
    vals = []
    unknown = False
    for name in idents:
        if name in consts:
            vals.append(consts[name])
        elif RE_KNOWN_NONKEY.match(name):
            continue
        else:
            unknown = True
    if not vals:
        return None, False
    return max(vals), (not unknown)


def main():
    ap = argparse.ArgumentParser(description="std::any 存储槽键撞号静态扫描")
    ap.add_argument("--json", action="store_true", help="输出 JSON")
    ap.add_argument("--quiet", action="store_true", help="只报跨文件撞键")
    args = ap.parse_args()

    sources = collect_sources()
    # 键使用点：(容器, 基础键) → [(文件, 行, 常量名, 表达式)]
    uses = defaultdict(list)
    const_cache = {}
    for rel, text in sources:
        consts = header_consts(rel, text, const_cache)
        if not consts:
            continue
        for m in RE_STORE.finditer(text):
            container = m.group(1)
            expr = m.group(2)
            base, exact = resolve(expr, consts)
            if base is None:
                continue
            line = text[:m.start()].count("\n") + 1
            names = [n for n in RE_IDENT.findall(expr) if n in consts]
            uses[(container, base)].append(
                (rel, line, names[0] if names else "?", expr, exact))

    # 跨文件撞键：同一（容器, 基础键）出现在多个文件
    collisions = []
    for (container, base), lst in sorted(uses.items()):
        files = {rel for rel, _, _, _, _ in lst}
        if len(files) > 1 and (container, base) not in INTENTIONAL_SHARED:
            collisions.append({
                "container": container,
                "container_desc": CONTAINERS.get(container, ""),
                "base_key": base,
                "files": sorted(files),
                "sites": [{"file": r, "line": l, "const": c, "expr": e}
                          for r, l, c, e, _ in lst],
            })
    shared = [{"container": c, "base_key": b, "note": n}
              for (c, b), n in sorted(INTENTIONAL_SHARED.items())
              if (c, b) in uses]

    official = []
    for (container, base), lst in sorted(uses.items()):
        if base <= OFFICIAL_ID_MAX:
            files = {rel for rel, _, _, _, _ in lst}
            official.append({
                "container": container, "base_key": base,
                "files": sorted(files),
            })

    if args.json:
        print(json.dumps({"collisions": collisions,
                          "intentional_shared": shared,
                          "official_id_range": official},
                         ensure_ascii=False, indent=2))
        return 1 if collisions else 0

    print(f"扫描 {len(sources)} 个源文件，{len(uses)} 个（容器, 基础键）组合。")
    if collisions:
        print(f"\n⚠️ 跨文件同键（跨 dylib 撞键风险）{len(collisions)} 处：")
        for c in collisions:
            print(f"\n  {c['container']}[{c['base_key']}]"
                  f"（{c['container_desc']}）—— {len(c['files'])} 个文件：")
            for s in c["sites"]:
                print(f"      {s['file']}:{s['line']}  {s['expr']}  ({s['const']})")
        print("\n  处置：换到各自独占的键段（每个机制一段，别共用 base），"
              "或确认两处语义相同、故意共用并在注释里写明。")
    else:
        print("✓ 无跨文件同键。")

    if shared and not args.quiet:
        print(f"\n  已备案的故意共用键 {len(shared)} 处（不算撞键）：")
        for s in shared:
            print(f"      {s['container']}[{s['base_key']}]  {s['note']}")

    if official and not args.quiet:
        print(f"\n  另有 {len(official)} 个键落在官方 effect_id 空间"
              f"（1~{OFFICIAL_ID_MAX}）——易与官方语义混淆，建议迁到 9xxxx 私有段：")
        for o in official:
            print(f"      {o['container']}[{o['base_key']}]  {len(o['files'])} 个文件")

    return 1 if collisions else 0


if __name__ == "__main__":
    sys.exit(main())
