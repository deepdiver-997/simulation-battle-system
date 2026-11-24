#!/usr/bin/env python3
"""display_gate.py —— 效果显示 G1 描述链静态对账器（2026-11-22）。

规范依据：《docs/07-工具与测试/效果显示gate规范.md》§4 G1。

## 为什么需要

显示面（控制台 / AI 观察空间）按"协议只传 ID，名字客户端解析"工作（开工文档 §2.1）：
桶条目/票面/印记只带 id，文案由消费端查表渲染。这条链有一个静态可验证的义务——
**注册表里的每个 id 必须能在描述源解析出文案**，否则该效果在战斗里真实参与结算、
在显示面上却是一个裸 id（对玩家是黑盒，对对拍是断链）。这跟覆盖率对账（registry vs
DB 的"实现了没有"）是两类问题：本脚本管"实现了的东西**说得出名字吗**"。

## 判定口径（与网关 catalog.py 同构，改两边要同步）

- moves 空间：`effect_info` 有行且 `info` 非空（模板 + `{n}` 占位符，args 桶条目自带）；
- soul 空间：`effect_icon.effect_id` → `tips` 非空（同 effect_id 多行取最长，
  与 catalog.py `soul_effect_tips` 一致）。**不回退 effect_info**——两个注册空间
  数字重叠（effect 106 ≠ 魂印 106，规范 C2 空间隔离），回退会渲染错文案；
- suit 空间：硬检查 `suit` 表存在性（id 不在 DB = 发明 id，直接红）+ 白名单登记
  ——DB `suit.suitdes` 是故事文本不是效果描述，效果文案的载体当前是
  `resources/suit_lib/*.cpp` 文件头注（官方全文+拆解），G4 display 段落地前走白名单。

## 白名单（tools/ci/display_gate_whitelist.json）

每条 = `{"<space>": {"<id>": "原因（须指认实现文件/口径依据）"}}`。三类条目：
  ① 官方 id 但 DB 无描述行（如 G8 考古效果 21/31/42——effect_info 无行）；
  ② 本地扩展/承接/私有号段 id（270210、900001、9901xxx——DB 本来就不该有行）；
  ③ G4 前的套装（无效果描述源，登记 = 有意承担显示缺口）。
白名单条目若已可解析（stale）或不在注册表（僵尸）→ 警告（不红），提示清理。

## 用法

    python3 tools/ci/display_gate.py                 # 报告（退出码见下）
    python3 tools/ci/display_gate.py --json          # 机器可读
    python3 tools/ci/display_gate.py --quiet         # 只报失败与警告
    python3 tools/ci/display_gate.py --registry build/registry.json   # 显式指定

registry 文件缺失时自动跑 `./build/sim_server --dump-registry-exit` 重新导出。

退出码：0 = 通过（全部 id 可解析或已登记白名单）；1 = 有白名单外解析失败；
2 = 环境问题（DB/registry 拿不到）。全量门挂点：tools/night/build.sh（registry
dump 之后，与 check_storage_keys.py 同型）。
"""
import argparse
import json
import os
import sqlite3
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DEFAULT_REGISTRY = os.path.join(REPO, "build", "registry.json")
DEFAULT_DB = os.path.join(REPO, "scripts", "data", "processed", "seer_unity.sqlite")
DEFAULT_WHITELIST = os.path.join(REPO, "tools", "ci", "display_gate_whitelist.json")
DEFAULT_SIM = os.path.join(REPO, "build", "sim_server")

SPACES = ("moves", "soul", "suit")


def load_registry(path: str) -> dict:
    """读注册表 dump；缺失时用 sim_server 现场导出。"""
    if not os.path.exists(path):
        if not os.path.exists(DEFAULT_SIM):
            raise EnvironmentError(
                f"registry 不存在（{path}）且找不到 {DEFAULT_SIM}——先构建（cmake --build build -j8）"
            )
        subprocess.run([DEFAULT_SIM, "--dump-registry-exit", path], check=True,
                       cwd=REPO, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    with open(path, encoding="utf-8") as f:
        data = json.load(f)
    out = {}
    for space in SPACES:
        ids = data.get(space)
        if not isinstance(ids, list):
            raise EnvironmentError(f"registry {path} 缺 {space} 段或不是数组")
        out[space] = {int(i) for i in ids}
    return out


def load_db(db_path: str) -> dict:
    """描述源三表（只读打开，与 catalog.py 同姿势）。"""
    if not os.path.exists(db_path):
        raise EnvironmentError(
            f"数据库不存在：{db_path}\n"
            "先构建：python3 scripts/fetch_seer_unity_bytes.py && "
            "python3 scripts/decode_seer_bytes.py && tools/db_patches/apply_all.sh"
        )
    db = sqlite3.connect(f"file:{db_path}?mode=ro", uri=True)
    effect_info = {}   # id → info（空串 = 无模板文本）
    for eid, info in db.execute("SELECT id, info FROM effect_info"):
        effect_info[int(eid)] = info or ""
    soul_tips = {}     # effect_icon.effect_id → 最长 tips（多精灵共享印记取最全那条）
    for eid, tips in db.execute("SELECT effect_id, tips FROM effect_icon"):
        eid = int(eid) if eid else 0
        tips = tips or ""
        if eid and len(tips) > len(soul_tips.get(eid, "")):
            soul_tips[eid] = tips
    suit_names = {}    # id → name（存在性检查用）
    for sid, name in db.execute("SELECT id, name FROM suit"):
        suit_names[int(sid)] = name or ""
    db.close()
    return {"effect_info": effect_info, "soul_tips": soul_tips, "suit_names": suit_names}


def load_whitelist(path: str) -> dict:
    """{"moves": {"21": "原因"}, ...}。键必须能转 int；原因不允许空。"""
    if not os.path.exists(path):
        print(f"[warn] 白名单缺失：{path}（按空白名单处理 = 已知缺口全部转红）")
        return {s: {} for s in SPACES}
    with open(path, encoding="utf-8") as f:
        raw = json.load(f)
    out = {}
    for space in SPACES:
        entries = {}
        for k, reason in (raw.get(space) or {}).items():
            try:
                eid = int(k)
            except (TypeError, ValueError):
                raise EnvironmentError(f"白名单 {space} 键不是数字：{k!r}")
            if not (reason or "").strip():
                raise EnvironmentError(f"白名单 {space}[{k}] 原因为空——每条必须指认实现文件/口径依据")
            entries[eid] = reason.strip()
        out[space] = entries
    extra = {k for k in raw if not k.startswith("_")} - set(SPACES)
    if extra:
        print(f"[warn] 白名单含未知空间段（忽略）：{sorted(extra)}")
    return out


def reconcile(registry: dict, db: dict, whitelist: dict) -> dict:
    """逐空间对账。suit 的 notInDb 是硬失败（发明 id），不吃白名单。"""
    def db_resolves(space: str, eid: int) -> bool:
        """不看白名单、只看 DB 描述源能否解析（stale 判定用）。"""
        if space == "moves":
            return bool(db["effect_info"].get(eid, "").strip())
        if space == "soul":
            return bool(db["soul_tips"].get(eid, "").strip())
        return False  # suit：DB 无效果描述源，白名单就是（G4 前的）解析面

    result = {}
    for space in SPACES:
        ids = registry[space]
        wl = whitelist[space]
        resolved, missing, whitelisted = [], [], []
        for eid in sorted(ids):
            if db_resolves(space, eid):
                resolved.append(eid)
            elif eid in wl:   # suit 空间白名单即（G4 前的）解析面，落白名单账
                whitelisted.append(eid)
            else:
                missing.append(eid)
        # 白名单卫生：stale（DB 源已能解析，白名单成了冗余）与僵尸（不在注册表）
        stale = sorted(e for e in wl if e in ids and db_resolves(space, e))
        zombie = sorted(e for e in wl if e not in ids)
        not_in_db = sorted(e for e in ids
                           if space == "suit" and e not in db["suit_names"]) if space == "suit" else []
        result[space] = {
            "total": len(ids), "resolved": resolved,
            "whitelisted": whitelisted, "missing": missing,
            "stale": stale, "zombie": zombie, "not_in_db": not_in_db,
        }
    return result


def main() -> int:
    ap = argparse.ArgumentParser(description="效果显示 G1 描述链静态对账")
    ap.add_argument("--registry", default=DEFAULT_REGISTRY)
    ap.add_argument("--db", default=DEFAULT_DB)
    ap.add_argument("--whitelist", default=DEFAULT_WHITELIST)
    ap.add_argument("--json", action="store_true", help="机器可读报告")
    ap.add_argument("--quiet", action="store_true", help="只报失败与警告")
    args = ap.parse_args()

    try:
        registry = load_registry(args.registry)
        db = load_db(args.db)
        whitelist = load_whitelist(args.whitelist)
    except EnvironmentError as e:
        print(f"[display-gate] 环境错误：{e}", file=sys.stderr)
        return 2
    report = reconcile(registry, db, whitelist)

    failures = {s: report[s]["missing"] + report[s]["not_in_db"] for s in SPACES}
    n_fail = sum(len(v) for v in failures.values())
    n_wl = sum(len(report[s]["whitelisted"]) for s in SPACES)
    n_stale = sum(len(report[s]["stale"]) for s in SPACES)
    n_zombie = sum(len(report[s]["zombie"]) for s in SPACES)

    if args.json:
        json.dump({"ok": n_fail == 0,
                   "spaces": {s: {**report[s],
                                  "db": {"effect_info": len(db["effect_info"]),
                                          "soul_tips": len(db["soul_tips"]),
                                          "suit": len(db["suit_names"])}} for s in SPACES}},
                  sys.stdout, ensure_ascii=False, indent=2)
        print()
        return 0 if n_fail == 0 else 1

    if not args.quiet:
        print("== 效果显示 G1 描述链对账（规范 docs/07-工具与测试/效果显示gate规范.md §4）==")
        print(f"registry: {os.path.relpath(args.registry, REPO)}"
              f"（moves={len(registry['moves'])} soul={len(registry['soul'])}"
              f" suit={len(registry['suit'])}）")
        print(f"DB: {os.path.relpath(args.db, REPO)}"
              f"（effect_info={len(db['effect_info'])}"
              f" effect_icon.tips={len(db['soul_tips'])} suit={len(db['suit_names'])}）")
        print()
        for space in SPACES:
            r = report[space]
            print(f"[{space}] 可解析={len(r['resolved'])} 白名单={len(r['whitelisted'])}"
                  f" 失败={len(r['missing']) + len(r['not_in_db'])}"
                  + (f" 不在DB={r['not_in_db']}" if r["not_in_db"] else ""))
        print()
    for space in SPACES:
        for eid in report[space]["missing"]:
            print(f"[FAIL] {space}#{eid} 无描述且未登记白名单"
                  f"（实现方补 DB 源不存在时登记 tools/ci/display_gate_whitelist.json 并给口径依据）")
        for eid in report[space]["not_in_db"]:
            print(f"[FAIL] {space}#{eid} 不在 DB suit 表——疑似发明 id（硬红线），白名单救不了")
    if not args.quiet:
        for space in SPACES:
            for eid in report[space]["stale"]:
                print(f"[warn] 白名单 {space}#{eid} 已可解析，建议移除")
            for eid in report[space]["zombie"]:
                print(f"[warn] 白名单 {space}#{eid} 不在注册表（僵尸条目），建议移除")
    print()
    print(f"G1 {'PASS' if n_fail == 0 else 'FAIL'}："
          f"失败={n_fail} 白名单={n_wl} stale={n_stale} 僵尸={n_zombie}")
    return 0 if n_fail == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
