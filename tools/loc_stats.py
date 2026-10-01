#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
loc_stats.py —— 内核 vs 插件（resources）效果代码行数统计。

分组（与仓库分层对应）：
  内核-效果系统   src/effects + include/effects
  内核-效果原语   src/primitives + include/primitives
  插件-moves_lib  resources/moves_lib
  插件-soul_lib   resources/soul_lib
  插件-suit_lib   resources/suit_lib

用法：python3 tools/loc_stats.py [--top N]     （--top 每组列最大的 N 个文件，默认 3）
"""
import argparse
import os

GROUPS = [
    ("内核-效果系统", ["src/effects", "include/effects"]),
    ("内核-效果原语", ["src/primitives", "include/primitives"]),
    ("插件-moves_lib", ["resources/moves_lib"]),
    ("插件-soul_lib", ["resources/soul_lib"]),
    ("插件-suit_lib", ["resources/suit_lib"]),
]


def count_dir(base, rel):
    path = os.path.join(base, rel)
    files = []
    if not os.path.isdir(path):
        return files
    for name in sorted(os.listdir(path)):
        if name.endswith((".cpp", ".h")):
            full = os.path.join(path, name)
            if os.path.isfile(full):
                with open(full, encoding="utf-8", errors="replace") as f:
                    files.append((name, sum(1 for _ in f)))
    return files


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--top", type=int, default=3)
    args = ap.parse_args()

    grand = 0
    for title, dirs in GROUPS:
        all_files = []
        for d in dirs:
            all_files += count_dir(os.getcwd(), d)
        total = sum(n for _, n in all_files)
        grand += total
        print(f"── {title}：{len(all_files)} 文件 / {total} 行")
        for name, n in sorted(all_files, key=lambda x: -x[1])[: args.top]:
            print(f"     {n:>6}  {name}")
        print()
    print(f"═══ 总计：{grand} 行")


if __name__ == "__main__":
    main()
