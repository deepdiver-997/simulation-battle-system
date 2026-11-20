#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
coverage_recompute.py —— 覆盖率四步复算（百科 05 章 §3.2 算法的可重放落盘，S17 交付）。

背景（05 章待验证 #1）：链级可用率 87.6%（11-02 口径）vs 87.9%（11-04 复算）的漂移
归因需要可重算留痕——本脚本把 §3.2 四步落成 tools/ 脚本，每次 registry dump / DB 重建后
重跑一次即得带时间戳的快照行，杜绝"分析不落盘、漂移不可归因"复发。

算法四步（05 章 §3.2）：
  1. sqlite 读全量 moves.side_effect，JSON 拍平 → 引用面全集 + 每技链；
  2. side_effect 定义表为定义面、registry.json moves 为已实现集（严格口径）；
  3. 链级可用率 = 全链 id ⊆ 已实现集的载体技数 ÷ 载体技总数（分母口径敏感，三种并列）；
  4. 差集三分类：引用面∩已实现 / 引用面−已实现（长尾）/ 引用面−定义面（数据缺口候选）。

用法：
  python3 tools/coverage_recompute.py            # 仓库根运行，打印快照块
  python3 tools/coverage_recompute.py --json     # 追加机器可读 JSON 行（尾行）
输出：人类可读快照块（可直接粘贴进文档"现查"注记）+ 可选 JSON 尾行。
"""
import argparse
import datetime
import json
import os
import sqlite3
import sys

DB = os.path.join("scripts", "data", "processed", "seer_unity.sqlite")
REGISTRY = os.path.join("build", "registry.json")


def flatten_ids(side_effect: str):
    """moves.side_effect 是 JSON 数组文本（管线约定，不做空格拼接）。"""
    if not side_effect:
        return []
    try:
        v = json.loads(side_effect)
    except (ValueError, TypeError):
        return []
    if not isinstance(v, list):
        return []
    return [int(x) for x in v if str(x).lstrip("-").isdigit()]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--db", default=DB)
    ap.add_argument("--registry", default=REGISTRY)
    ap.add_argument("--json", action="store_true", help="末尾追加一行 JSON")
    args = ap.parse_args()

    con = sqlite3.connect(args.db)
    cur = con.cursor()
    rows = cur.execute("SELECT id, side_effect FROM moves").fetchall()

    total = len(rows)
    chains = []          # (move_id, ids) 仅非空链
    ref_ids = set()      # 引用面全集
    for rid, se in rows:
        ids = flatten_ids(se)
        if ids:
            chains.append((rid, ids))
            ref_ids.update(ids)
    nonempty = len(chains)
    empty = total - nonempty

    def_face = set(r[0] for r in cur.execute("SELECT DISTINCT id FROM side_effect").fetchall())
    info_ids = set(r[0] for r in cur.execute("SELECT id FROM effect_info").fetchall())
    reg = json.load(open(args.registry))
    reg_moves = set(reg.get("moves", []))

    ok_nonempty = sum(1 for _, ids in chains if set(ids) <= reg_moves)
    ok_all_vacuous = ok_nonempty + empty  # 空链真空可用口径
    ref_only = ref_ids - def_face
    def_only = def_face - ref_ids

    snap = {
        "snapshot_time": datetime.datetime.now().strftime("%Y-%m-%d %H:%M"),
        "carriers_total": total,
        "carriers_nonempty": nonempty,
        "carriers_empty": empty,
        "ref_ids": len(ref_ids),
        "def_ids": len(def_face),
        "registry_moves": len(reg_moves),
        "def_and_ref": len(def_face & ref_ids),
        "def_only": len(def_only),
        "ref_only": len(ref_only),
        "ref_only_with_info": len(ref_only & info_ids),
        "ref_only_in_registry": len(ref_only & reg_moves),
        "reg_not_in_ref": len(reg_moves - ref_ids),
        "unregistered_tail": len(ref_ids - reg_moves),
        "chain_ok_nonempty": ok_nonempty,
        "chain_rate_nonempty_pct": round(100.0 * ok_nonempty / nonempty, 2) if nonempty else 0.0,
        "chain_rate_all_vacuous_pct": round(100.0 * ok_all_vacuous / total, 2) if total else 0.0,
        "chain_rate_hybrid_pct": round(100.0 * ok_nonempty / total, 2) if total else 0.0,
        "id_rate_vs_def_pct": round(100.0 * len(reg_moves & def_face) / len(def_face), 2) if def_face else 0.0,
        "id_rate_vs_ref_pct": round(100.0 * len(reg_moves & ref_ids) / len(ref_ids), 2) if ref_ids else 0.0,
    }

    t = snap["snapshot_time"]
    print(f"== 覆盖率快照（现查 {t}；DB={args.db} registry={args.registry}）==")
    print(f"载体技 {snap['carriers_total']}（非空链 {snap['carriers_nonempty']} / 空链 {snap['carriers_empty']}）")
    print(f"引用面 {snap['ref_ids']} id ｜ 定义面 {snap['def_ids']} id ｜ registry moves {snap['registry_moves']}")
    print(f"交集结构：定义∩引用 {snap['def_and_ref']}｜定义独有 {snap['def_only']}｜引用独有 {snap['ref_only']}"
          f"（其中有 effect_info 模板行 {snap['ref_only_with_info']}、已在 registry {snap['ref_only_in_registry']}）")
    print(f"registry 未引用 {snap['reg_not_in_ref']}｜未注册长尾 {snap['unregistered_tail']}")
    print(f"链级可用率三口径并列：")
    print(f"  非空链分母   {snap['chain_ok_nonempty']}/{snap['carriers_nonempty']} = {snap['chain_rate_nonempty_pct']}%")
    print(f"  全载体(空链真空可用) {snap['chain_ok_nonempty']}+{snap['carriers_empty']}/{snap['carriers_total']} = {snap['chain_rate_all_vacuous_pct']}%")
    print(f"  混搭(非空OK÷全载体，11-04 快照口径) {snap['chain_ok_nonempty']}/{snap['carriers_total']} = {snap['chain_rate_hybrid_pct']}%")
    print(f"id 级注册率：定义面口径 {snap['id_rate_vs_def_pct']}%｜引用面口径 {snap['id_rate_vs_ref_pct']}%")
    print("口径警示：引用任一百分比必须带本快照时刻与分母口径（05 章 §3.2 步骤 3）。")

    if args.json:
        print(json.dumps(snap, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    sys.exit(main())
