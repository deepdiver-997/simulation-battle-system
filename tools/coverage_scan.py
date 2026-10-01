#!/usr/bin/env python3
"""coverage_scan —— 效果覆盖率扫描（F 组立牌依据，可重跑）。

口径（承 a38b620 记账补录）：
  已实现 = build/registry.json 的 moves 集合 + 内核硬编码黑名单
  （697/699 穿透凭证等——registry 缺席≠没实现，动工前仍须 grep 内核，见 E 组工单 ⚠️）。
  引用 = moves.side_effect 数组展开的效果 id（一技能记一次）。
输出：未实现效果按引用次数降序（top N），附最近携带技能样例。
"""
import json
import sqlite3
import sys
from collections import Counter

DB = "/Users/zhuhongrui/code/c++/project/Simulation-battle-system/scripts/data/processed/seer_unity.sqlite"
REGISTRY = "build/registry.json"

# 内核硬编码黑名单（registry 缺席但功能存在——697/699 教训，见 a38b620）
KERNEL_HARDCodeD = {697, 699}


def effect_ids_of(side_effect: str):
    """'[2392, 2175]' -> [2392, 2175]；解析失败返回 []。"""
    try:
        v = json.loads(side_effect)
        return [int(x) for x in v if isinstance(x, (int, float))]
    except Exception:
        return []


def main(top=40):
    reg = json.load(open(REGISTRY))
    implemented = {m if isinstance(m, int) else m.get("id") for m in reg.get("moves", [])}

    con = sqlite3.connect(DB)
    refs = Counter()
    carriers = {}  # eid -> [(move_id, name, power), ...] 最近 3 个载体
    for mid, name, power, se in con.execute("SELECT id, name, power, side_effect FROM moves"):
        for eid in effect_ids_of(se or "[]"):
            refs[eid] += 1
            carriers.setdefault(eid, [])
            if len(carriers[eid]) < 3:
                carriers[eid].append((mid, name, power))

    missing = [(c, eid) for eid, c in refs.items()
               if eid not in implemented and eid not in KERNEL_HARDCodeD]
    missing.sort(reverse=True)

    print(f"registry 已实现 {len(implemented)} ｜ DB 引用效果 id {len(refs)} ｜ 未实现 {len(missing)}\n")
    print(f"{'引用':>4}  {'效果id':>6}  官方模板 / 最近载体")
    info = {i: (n, a) for i, n, a in con.execute("SELECT id, info, args_num FROM effect_info")}
    for c, eid in missing[:top]:
        tmpl = info.get(eid, ("（effect_info 无行）", 0))[0]
        cc = ", ".join(f"{n}({mid})" for mid, n, _ in carriers[eid][:2])
        print(f"{c:>4}  {eid:>6}  {tmpl[:46]} ｜ {cc[:40]}")


if __name__ == "__main__":
    main(int(sys.argv[1]) if len(sys.argv) > 1 else 40)
