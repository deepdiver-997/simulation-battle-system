#!/usr/bin/env python3
"""Measure paired scenario object builds, then compare the two binaries for each case."""

import argparse
from concurrent.futures import ThreadPoolExecutor
from dataclasses import asdict
import json
import os
from pathlib import Path
import re
import shutil
from statistics import mean
import subprocess
from time import monotonic

from run_full_trial import EXCLUSIVE, digest, print_failure, run_scenario


# Keep CMake's quoting intact while inspecting the generated GCC command lines.
WORD = re.compile(r'(?:[^\s"]|"[^"]*")+')
OBJECT_STEP = re.compile(r"^\[\d+/\d+\] Building CXX object (.+)$", re.MULTILINE)
BUILD_STEP = re.compile(r"^\[[1-9]\d*/\d+\] (.+)$", re.MULTILINE)


def output_arg(words, build):
    path = Path(words[words.index("-o") + 1].strip('"'))
    return (path.relative_to(build) if path.is_absolute() else path).as_posix()


def fingerprint(words, pch_header):
    words = words.copy()
    at = words.index("-o")
    del words[at:at + 2]
    if pch_header is not None:
        at = words.index("-include")
        actual = words[at + 1].strip('"').replace("\\", "/")
        if not actual.endswith("/" + pch_header):
            raise ValueError(f"Different PCH requested: {actual}")
        del words[at:at + 2]
        words.remove("-Werror=invalid-pch")
        words.remove("-Winvalid-pch")
    return words


def layout(build, names):
    database = json.loads((build / "compile_commands.json").read_text(encoding="utf-8"))
    entries = {}
    pch = []
    for entry in database:
        cmd = WORD.findall(entry["command"])
        path = output_arg(cmd, build)
        parts = Path(path).parts
        if path.endswith("/cmake_pch.hxx.gch"):
            pch.append(path)
        elif len(parts) >= 3 and parts[1].startswith(("sim_scenario_", "sim_nopch_scenario_")):
            target = parts[1].removesuffix(".dir")
            if target in entries:
                raise ValueError(f"More than one object for {target}")
            entries[target] = (path, cmd, Path(entry["file"]).stem)
    if len(pch) != 1:
        raise ValueError(f"Expected one generated PCH, found: {pch}")
    header = pch[0].removesuffix(".gch")
    arms = {"off": [], "on": []}
    for name in names:
        off, off_cmd, off_src = entries[f"sim_nopch_{name}"]
        on, on_cmd, on_src = entries[f"sim_{name}"]
        if off_src != name or on_src != name or fingerprint(off_cmd, None) != fingerprint(on_cmd, header):
            raise ValueError(f"Mismatched non-PCH compiler options or sources for {name}")
        arms["off"].append(off)
        arms["on"].append(on)
    return pch[0], arms, entries[f"sim_{names[0]}"][1][0].strip('"')


def check_graph(ninja, build, targets, expected, direct_only=False):
    cmd = [ninja, "-t", "commands", *(["-s"] if direct_only else []), *targets]
    output = subprocess.check_output(cmd, cwd=build, text=True, encoding="utf-8", errors="replace")
    actual = [output_arg(WORD.findall(line), build) for line in output.splitlines() if line]
    if sorted(actual) != sorted(expected):
        raise ValueError(f"Timed Ninja graph includes unexpected work: {sorted(set(actual) - set(expected))}")


def build_phase(ninja, build, output, jobs, label, targets, expected=None):
    path = output / f"build-{label}.log"
    print(f"{label}: starting Ninja ({len(targets)} targets, -j{jobs})", flush=True)
    start = monotonic()
    with path.open("wb") as log:
        env = dict(os.environ, NINJA_STATUS="[%f/%t] ")
        code = subprocess.run([ninja, "-j", str(jobs), *targets], cwd=build, env=env,
                              stdout=log, stderr=subprocess.STDOUT).returncode
    seconds = round(monotonic() - start, 3)
    if (build / ".ninja_log").is_file():
        shutil.copyfile(build / ".ninja_log", output / f"ninja-{label}.log")
    text = path.read_text(encoding="utf-8", errors="replace")
    if code:
        print("\n".join(text.splitlines()[-50:]), flush=True)
        raise RuntimeError(f"Ninja {label} failed with exit {code}")
    if expected is not None:
        actual = [name.strip().replace("\\", "/") for name in OBJECT_STEP.findall(text)]
        if sorted(actual) != sorted(expected) or len(BUILD_STEP.findall(text)) != len(expected):
            raise RuntimeError(f"Ninja {label} did not perform exactly its planned object builds")
    print(f"{label}: {seconds:.3f}s", flush=True)
    return seconds


def paired_results(repo, build, output, names, jobs):
    for directory in ("logs", "traces", "tmp"):
        (output / directory).mkdir()
    results = {}
    suffix = ".exe" if os.name == "nt" else ""
    # Keep each set separate: some tests use hardcoded fixture paths relative to the repo.
    with ThreadPoolExecutor(max_workers=max(1, min(8, jobs - 1))) as pool:
        for arm, prefix in (("off", "sim_nopch_"), ("on", "sim_")):
            futures = [pool.submit(run_scenario, repo, build / f"{prefix}{name}{suffix}",
                                   name, output, 120, arm) for name in names]
            results[arm] = [future.result() for future in futures]
    failures = []
    traced = 0
    golden = repo / "test" / "scenario" / "golden"
    for off, on in zip(results["off"], results["on"], strict=True):
        for arm, result in (("off", off), ("on", on)):
            if result.status != "pass":
                print_failure(result, output, arm)
        if off.status != "pass" or on.status != "pass":
            failures.append(f"{off.name}: off={off.status}, on={on.status}")
        elif off.chain_sha256 != on.chain_sha256:
            failures.append(f"{off.name}: trace presence or hash mismatch")
        elif on.chain_sha256 is not None:
            traced += 1
        elif (golden / f"{on.name}.chain").is_file():
            failures.append(f"{on.name}: both variants unexpectedly missing a trace")
    return {arm: [asdict(item) for item in rows] for arm, rows in results.items()}, traced, failures


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=Path("build"))
    parser.add_argument("--jobs", type=int, default=os.cpu_count() or 1)
    parser.add_argument("--ninja", required=True)
    parser.add_argument("--compile-only", action="store_true")
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    repo = Path(__file__).resolve().parents[2]
    build = (repo / args.build_dir).resolve()
    manifest = build / "pch-trial-scenarios.txt"
    names = manifest.read_text(encoding="utf-8").splitlines()
    if not names or len(names) != len(set(names)) or EXCLUSIVE in names:
        raise ValueError("Empty, duplicate or DB-writing PCH trial selection")
    output = build / "windows-pch"
    output.mkdir()
    for path in (manifest, build / "compile_commands.json"):
        shutil.copyfile(path, output / path.name)
    pch, arms, compiler = layout(build, names)
    if (build / pch).exists() or any((build / path).exists() for paths in arms.values() for path in paths):
        raise ValueError("Use a clean build directory for first PCH and object timing")

    check_graph(args.ninja, build, [pch], [pch])
    check_graph(args.ninja, build, arms["off"], arms["off"])
    check_graph(args.ninja, build, arms["on"], arms["on"], direct_only=True)
    pch_seconds = build_phase(args.ninja, build, output, args.jobs, "pch", [pch], [pch])
    # REUSE_FROM adds an order-only dependency on the donor executable. Build it
    # and sim_core once, then keep the donor out of the timed object batches.
    owner = Path(pch).parts[1].removesuffix(".dir")
    prereq_seconds = build_phase(args.ninja, build, output, args.jobs, "prerequisites", [owner])
    batches = {"off": [], "on": []}
    for index, arm in enumerate(("off", "on", "on", "off"), 1):
        paths = arms[arm]
        for path in paths:
            (build / path).unlink(missing_ok=True)
        seconds = build_phase(args.ninja, build, output, args.jobs, f"{index}-{arm}", paths, paths)
        batches[arm].append(seconds)
    report = {
        "commit": os.environ.get("GITHUB_SHA"),
        "runner_image": os.environ.get("ImageVersion"),
        "compiler": subprocess.check_output([compiler, "-dumpfullversion"], text=True).strip(),
        "cmake": subprocess.check_output(["cmake", "--version"], text=True).splitlines()[0],
        "ninja": subprocess.check_output([args.ninja, "--version"], text=True).strip(),
        "jobs": args.jobs,
        "samples": names,
        "pch_seconds": pch_seconds,
        "prerequisites_seconds": prereq_seconds,
        "batches": batches,
        "off_mean_seconds": round(mean(batches["off"]), 3),
        "on_mean_seconds": round(mean(batches["on"]), 3),
    }
    report_path = output / "report.json"

    def save():
        report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")

    save()
    summary = [
        "### Scenario PCH trial (same compiler, same Release options)",
        f"- {len(names)} paired scenario objects; Ninja -j{args.jobs}; one PCH created in {pch_seconds}s.",
        f"- PCH off: {batches['off']}s; PCH on: {batches['on']}s (excludes PCH creation).",
    ]
    failed = False
    if not args.compile_only:
        db = repo / "scripts/data/processed/seer_unity.sqlite"
        db_hash = digest(db)
        if db_hash is None:
            raise FileNotFoundError(db)
        linked = [prefix + name for name in names for prefix in ("sim_", "sim_nopch_")]
        shared = ["sim_server", "sim_smoke_test", "moves_plugin_deploy", "soul_plugin_deploy", "suit_plugin_deploy"]
        report["runtime_build_seconds"] = build_phase(args.ninja, build, output, args.jobs, "runtime", shared + linked)
        results, traced, failures = paired_results(repo, build, output, names, args.jobs)
        if digest(db) != db_hash:
            failures.append("Shared DB mutated during paired execution")
        report.update(db_sha256=db_hash, executions=results, matched_traces=traced, failures=failures)
        failed = bool(failures)
        summary.append(f"- Paired execution: {len(names)} pairs, {traced} matching traces, {len(failures)} issues.")
        summary.extend(f"  - {failure}" for failure in failures)
        save()
    print("\n".join(summary), flush=True)
    if os.environ.get("GITHUB_STEP_SUMMARY"):
        with open(os.environ["GITHUB_STEP_SUMMARY"], "a", encoding="utf-8") as stream:
            stream.write("\n".join(summary) + "\n")
    return int(failed)


if __name__ == "__main__":
    raise SystemExit(main())
