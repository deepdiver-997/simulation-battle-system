#!/usr/bin/env python3
"""一键全量场景回归（批量生成的质量闸门入口）：构建 → 逐场景运行 → 结构化汇总。

为什么需要这个脚本：全量回归原先是文档里的一行 for 循环人肉汇总——没有超时、
没有结构化结果，占位骨架（零断言、可编译可通过）也会被计进 "N/N 全绿"。
批量生成效果的质量闸门必须机器可判定，这是它的地基。

用法：
  python3 tools/run_regression.py                  # 构建 + 全量场景 + 汇总
  python3 tools/run_regression.py --clean-first    # 全量重建后再跑（怀疑陈旧 dylib 时）
  python3 tools/run_regression.py --filter 045     # 只跑名字含 045 的场景
  python3 tools/run_regression.py --strict         # 零断言场景按失败计
  python3 tools/run_regression.py --with-smoke     # 连 sim_smoke_test 一起跑

构建保证：总是先 `cmake --build`——CMakeLists 的 ALL 常驻部署 target 会在每次
构建时把插件 dylib 重新拷回源码目录（POST_BUILD 增量不拷贝 → 陈旧 dylib →
字段偏移错位的历史事故，见 CLAUDE-full §5.13），所以"跑之前先构建"是硬前提。

判定：
  - 退出码 0 = 通过；非 0 / 超时 = 失败。
  - harness 场景退出前打印 SBS_SCENARIO_CHECKS=<n>；n==0，或非 harness 场景
    （无标记）源码里也没有 CHECK(/CHECK_EQ(/REQUIRE( 字样 → 记零断言
    （默认警告不拦门，--strict 按失败计）。

报告：build/regression_report.json（每场景退出码/耗时/断言数），供机器消费。
"""
import argparse
import concurrent.futures
import glob
import json
import os
import re
import subprocess
import sys
import time

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD = os.path.join(REPO, "build")
REPORT = os.path.join(BUILD, "regression_report.json")
MARKER_RE = re.compile(r"SBS_SCENARIO_CHECKS=(\d+)")
STATIC_ASSERT_RE = re.compile(r"\b(CHECK|CHECK_EQ|REQUIRE)\s*\(")
STDERR_TAIL = 40


def run(cmd, **kw):
    return subprocess.run(cmd, cwd=REPO, capture_output=True, text=True, **kw)


def build(jobs, clean_first):
    if not os.path.exists(os.path.join(BUILD, "CMakeCache.txt")):
        r = run(["cmake", "-B", os.path.relpath(BUILD, REPO)])
        if r.returncode:
            return False, r.stderr[-4000:]
    cmd = ["cmake", "--build", BUILD, "-j", str(jobs)]
    if clean_first:
        cmd.append("--clean-first")
    t0 = time.time()
    r = run(cmd)
    return r.returncode == 0, (r.stderr if r.returncode else "")[-4000:] + \
        f"\n[build {time.time()-t0:.1f}s]"


def static_has_assertions(cpp_path):
    try:
        with open(cpp_path, encoding="utf-8", errors="replace") as f:
            return bool(STATIC_ASSERT_RE.search(f.read()))
    except OSError:
        return False


def run_one(binary, timeout):
    t0 = time.time()
    try:
        r = subprocess.run([binary], capture_output=True, text=True, timeout=timeout)
        exit_code, err = r.returncode, r.stderr
        out = r.stdout
    except subprocess.TimeoutExpired as e:
        exit_code, err, out = -999, (e.stderr or b"").decode(errors="replace"), \
            (e.stdout or b"").decode(errors="replace")
    m = MARKER_RE.search(out)
    return {
        "name": os.path.basename(binary),
        "exit": exit_code,
        "seconds": round(time.time() - t0, 2),
        "checks": int(m.group(1)) if m else None,   # None = 非 harness 场景
        "stderr": err[-4000:],
    }


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[1])
    ap.add_argument("--clean-first", action="store_true", help="构建前 clean 全量重建")
    ap.add_argument("--jobs", type=int, default=min(8, os.cpu_count() or 4))
    ap.add_argument("--timeout", type=int, default=120, help="单场景超时秒数")
    ap.add_argument("--filter", default="", help="只跑名字含该子串的场景")
    ap.add_argument("--strict", action="store_true", help="零断言场景按失败计")
    ap.add_argument("--with-smoke", action="store_true", help="连 sim_smoke_test 一起跑")
    args = ap.parse_args()

    ok, log = build(args.jobs, args.clean_first)
    if not ok:
        print("❌ 构建失败，回归中止：\n" + log, file=sys.stderr)
        return 2

    pats = [os.path.join(BUILD, "sim_scenario_*")]
    if args.with_smoke:
        pats.append(os.path.join(BUILD, "sim_smoke_test"))
    binaries = sorted(p for pat in pats for p in glob.glob(pat)
                      if os.access(p, os.X_OK) and args.filter in os.path.basename(p))
    if not binaries:
        print("❌ 没找到可执行场景（先构建，或检查 --filter）", file=sys.stderr)
        return 2

    print(f"▶ {len(binaries)} 个场景，jobs={args.jobs}，timeout={args.timeout}s …")
    t0 = time.time()
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as ex:
        results = list(ex.map(lambda b: run_one(b, args.timeout), binaries))

    # 零断言判定：harness 计数 n==0，或非 harness 且源码无断言字样
    for r in results:
        if r["exit"] != 0:
            continue
        if r["checks"] is not None:
            r["zeroAssertion"] = r["checks"] == 0
        else:
            cpp = os.path.join(REPO, "test", "scenario", r["name"] + ".cpp")
            r["zeroAssertion"] = os.path.exists(cpp) and not static_has_assertions(cpp)

    fails = [r for r in results if r["exit"] != 0]
    zeros = [r for r in results if r.get("zeroAssertion")]
    strict_fails = fails + zeros if args.strict else fails

    for r in fails:
        print(f"\n❌ {r['name']} exit={r['exit']} ({r['seconds']}s)\n{r['stderr'][-STDERR_TAIL*80:]}")
    if zeros:
        names = ", ".join(r["name"].removeprefix("sim_") for r in zeros)
        tag = "❌" if args.strict else "⚠️ "
        print(f"\n{tag} 零断言场景（占位骨架/空壳，{'按失败计' if args.strict else '不拦门'}）：{names}")

    report = {
        "generatedAt": time.strftime("%Y-%m-%dT%H:%M:%S"),
        "cleanFirst": args.clean_first,
        "totals": {
            "total": len(results), "pass": len(results) - len(fails),
            "fail": len(fails), "zeroAssertion": len(zeros),
            "seconds": round(time.time() - t0, 1),
        },
        "scenarios": [{k: v for k, v in r.items() if k != "stderr"} for r in results],
    }
    os.makedirs(BUILD, exist_ok=True)
    with open(REPORT, "w", encoding="utf-8") as f:
        json.dump(report, f, ensure_ascii=False, indent=1)

    print(f"\n{'✅' if not strict_fails else '❌'} "
          f"{len(results) - len(fails)}/{len(results)} 通过"
          f"，失败 {len(fails)}，零断言 {len(zeros)}，耗时 {report['totals']['seconds']}s"
          f"（报告：build/regression_report.json）")
    return 1 if strict_fails else 0


if __name__ == "__main__":
    sys.exit(main())
