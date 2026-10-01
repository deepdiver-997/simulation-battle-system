#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
soul_effects_dump.py —— 按 effect_id 导出 effect_icon 魂印行清单（K 组批产数据面）。

S1 调研（docs_local/docs/02-效果系统/魂印机制族聚类-调研.md）§七的配套工具：
"行≠宏"——同一 effect_id 的多行是同一效果宏的不同参数化实例，批产前先 dump 对行。

用法：
    python3 tools/soul_effects_dump.py 3 150 254      # 指定宏
    python3 tools/soul_effects_dump.py --all           # 全量（2149 行）
输出：JSON 到 stdout，结构
    { effect_id: [ {id, pet_id, pet_name, tips, args, kind, come}, … ] }
"""
import json
import os
import sqlite3
import sys

DB = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                  "..", "scripts", "data", "processed", "seer_unity.sqlite")


def main() -> int:
    args = sys.argv[1:]
    con = sqlite3.connect(DB)
    cur = con.cursor()
    cur.execute("SELECT id, name FROM monsters")
    names = dict(cur.fetchall())

    if args and args[0] != "--all":
        ids = [int(a) for a in args]
        rows = cur.execute(
            "SELECT id, effect_id, pet_id, tips, args, kind, come FROM effect_icon "
            f"WHERE effect_id IN ({','.join(['?'] * len(ids))})",
            ids).fetchall()
    else:
        rows = cur.execute(
            "SELECT id, effect_id, pet_id, tips, args, kind, come FROM effect_icon").fetchall()

    out: dict = {}
    for rid, eid, pet_id, tips, pargs, kind, come in rows:
        pids = json.loads(pet_id) if pet_id and pet_id.startswith("[") else (
            [int(pet_id)] if pet_id and str(pet_id).isdigit() else [])
        out.setdefault(eid, []).append({
            "row": rid,
            "pet_id": pids,
            "pet_name": [names.get(p, str(p)) for p in pids],
            "tips": tips or "",
            "args": pargs or "",
            "kind": kind,
            "come": come,
        })
    json.dump(out, sys.stdout, ensure_ascii=False, indent=1)
    print()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
