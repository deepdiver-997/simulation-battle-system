#!/usr/bin/env python3
"""生成场景测试索引（零维护：从各场景头注释提取标题行，按需运行）。

用法：python3 tools/scenario_index.py [> docs_local/docs/07-工具与测试/场景索引.md]
标题取每个文件头部注释里第一个非空注释行（约定 = "场景 NNN —— 描述"）。
"""
import glob
import re
import sys

rows = []
for path in sorted(glob.glob("test/scenario/scenario_*.cpp")):
    title = ""
    with open(path, encoding="utf-8") as f:
        for line in f.read().splitlines()[:40]:
            m = re.match(r"\s*//\s*(\S.*)", line)
            if m and not m.group(1).startswith(("场景化", "scenario_harness")):
                title = m.group(1).strip()
                break
    name = path.split("/")[-1].removesuffix(".cpp")
    rows.append((name, title or "（无头注释）"))

if len(sys.argv) > 1:
    out = ["# 场景测试索引（由 tools/scenario_index.py 生成，勿手改）", "",
           "| 场景 | 描述 |", "|---|---|"]
    out += [f"| {n} | {t} |" for n, t in rows]
    print("\n".join(out))
else:
    for n, t in rows:
        print(f"{n}\t{t}")
