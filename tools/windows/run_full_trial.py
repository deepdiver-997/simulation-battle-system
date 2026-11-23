#!/usr/bin/env python3
"""Run every built Windows scenario once, with isolated artifacts and a serial spot check."""

import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import asdict, dataclass
import hashlib
import json
import os
from pathlib import Path
import subprocess
from time import monotonic


# This scenario inserts and deletes rows in the shared SQLite database.
EXCLUSIVE = "scenario_086_suit_skeleton"
SERIAL_SPOT_CHECK = (
    "scenario_179_fullstate_stage2",
    "scenario_2460_lowdmg_gate3",
    "scenario_2520_window_roll_anomaly",
    "scenario_2550_untouched_gate",
    "scenario_2570_window_drain",
    "scenario_7910_combo_rules_data",
    "scenario_7930_selection_routes_data",
    "scenario_7931_selection_timing",
    "scenario_8510_k464",
    "scenario_8565_k469",
)


@dataclass
class Result:
    name: str
    status: str
    exit_code: int | None
    seconds: float
    chain_sha256: str | None


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest() if path.is_file() else None


def run_scenario(repo, exe, name, output, timeout, phase="full"):
    log = output / "logs" / f"{name}.{phase}.log"
    chain = output / "traces" / f"{name}.{phase}.chain"
    scratch = output / "tmp" / f"{name}.{phase}"
    scratch.mkdir()
    env = os.environ.copy()
    env.pop("SBS_SCENARIO_TRACE_FULL", None)
    env.update(
        SBS_SCENARIO_TRACE=str(chain),
        SBS_FULLSTATE_OUT=str(scratch / "state.json"),
        TEMP=str(scratch),
        TMP=str(scratch),
        TMPDIR=str(scratch),
    )
    start = monotonic()
    code = None
    with log.open("wb") as stream:
        try:
            code = subprocess.run(
                [str(exe)], cwd=repo, env=env, stdin=subprocess.DEVNULL,
                stdout=stream, stderr=subprocess.STDOUT, timeout=timeout,
            ).returncode
            status = "pass" if code == 0 else "fail"
        except subprocess.TimeoutExpired:
            # subprocess.run kills and waits for the failed child before returning control.
            status = "timeout"
        except OSError as exc:
            stream.write(f"Process launch failed: {exc}\n".encode("utf-8"))
            status = "launch_error"
    return Result(name, status, code, round(monotonic() - start, 3), digest(chain))


def print_failure(result, output, phase):
    print(f"{phase}: {result.status} {result.name} exit={result.exit_code}", flush=True)
    with (output / "logs" / f"{result.name}.{phase}.log").open("rb") as stream:
        stream.seek(max(0, os.fstat(stream.fileno()).st_size - 32768))
        for line in stream.read().decode("utf-8", errors="replace").splitlines()[-30:]:
            print(f"  | {line}")


def compare_baselines(scenario_dir, names, results, expected_drift):
    stats = {key: [] for key in ("match", "expected_drift", "drift", "no_baseline", "no_chain")}
    for name in names:
        current = results[name]
        if current.status != "pass":
            continue
        ref = digest(scenario_dir / "golden" / f"{name}.chain")
        actual = current.chain_sha256
        if ref is None and actual is None:
            continue  # Some legacy tests do not produce traces.
        if actual is None:
            kind = "no_chain"
        elif ref is None:
            kind = "no_baseline"
        elif ref == actual:
            kind = "match"
        elif name in expected_drift:
            kind = "expected_drift"
        else:
            kind = "drift"
        stats[kind].append(name)
    return stats


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=Path("build"))
    parser.add_argument("--output-dir", type=Path, default=Path("build/windows-full"))
    parser.add_argument("--jobs", type=int, default=max(1, min(8, (os.cpu_count() or 1) - 1)))
    parser.add_argument("--timeout", type=float, default=120)
    args = parser.parse_args()
    if args.jobs < 1 or args.timeout <= 0:
        parser.error("--jobs and --timeout must be positive")

    repo = Path(__file__).resolve().parents[2]
    scenario_dir = repo / "test" / "scenario"
    build = (repo / args.build_dir).resolve()
    output = (repo / args.output_dir).resolve()
    names = sorted(src.stem for src in scenario_dir.glob("scenario_*.cpp"))
    bins = {name: build / f"sim_{name}.exe" for name in names}
    missing = [name for name in names if not bins[name].is_file()]
    if not names or EXCLUSIVE not in bins:
        parser.error(f"Scenario manifest is empty or missing the DB writer: {EXCLUSIVE}")
    if missing:
        parser.error(f"Missing executables ({len(missing)}/{len(names)}): {', '.join(missing)}")
    output.mkdir(parents=True)  # Do not append traces to a previous run.
    for subdir in ("logs", "traces", "tmp"):
        (output / subdir).mkdir()

    print(f"Windows full: {len(names)} EXEs, {args.jobs} workers, {args.timeout:g}s per scenario", flush=True)
    started = monotonic()
    completed = []
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        pending = {
            pool.submit(run_scenario, repo, bins[name], name, output, args.timeout)
            for name in names if name != EXCLUSIVE
        }
        for future in as_completed(pending):
            result = future.result()
            completed.append(result)
            if result.status != "pass":
                print_failure(result, output, "full")
            if len(completed) % 100 == 0:
                print(f"Windows full: {len(completed)}/{len(names)} completed", flush=True)
    # The pooled children have all exited. Do not run the shared DB writer before
    # the pool: an aborted writer could leave rows visible to every other test.
    writer = run_scenario(repo, bins[EXCLUSIVE], EXCLUSIVE, output, args.timeout)
    completed.append(writer)
    if writer.status != "pass":
        print_failure(writer, output, "full")
    full_seconds = round(monotonic() - started, 2)
    if sorted(result.name for result in completed) != names:
        raise RuntimeError("Executed scenario set does not match the source manifest")
    results = {result.name: result for result in completed}

    serial = []
    mismatches = []
    started = monotonic()
    if writer.status == "pass":
        for name in SERIAL_SPOT_CHECK:
            if name not in bins:
                continue
            result = run_scenario(repo, bins[name], name, output, args.timeout, "serial")
            serial.append(result)
            if result.status != "pass":
                print_failure(result, output, "serial")
            pooled = results[name]
            if (result.status, result.exit_code, result.chain_sha256) != (
                pooled.status, pooled.exit_code, pooled.chain_sha256
            ):
                mismatches.append(name)
    else:
        print("Serial spot check skipped: the shared DB writer did not pass.", flush=True)
    serial_seconds = round(monotonic() - started, 2)

    drift_file = repo / "tools" / "windows" / "golden_expected_diff.txt"
    expected_drift = {line for line in drift_file.read_text(encoding="utf-8-sig").splitlines()
                      if line and not line.startswith("#")}
    golden = compare_baselines(scenario_dir, names, results, expected_drift)
    failed = [name for name in names if results[name].status != "pass"]
    launched = sum(result.status != "launch_error" for result in completed)
    summary = [
        "### Windows full scenario trial",
        f"- Manifest: {len(names)}; launched: {launched}; passed: {len(names) - len(failed)}; failed: {len(failed)}.",
        f"- Full execution: {full_seconds}s ({args.jobs} workers, one exclusive writer).",
        f"- Serial spot check: {len(serial)} repeated, {len(mismatches)} mismatches, {serial_seconds}s.",
        "- Repository golden (report-only, passed cases): "
        + ", ".join(f"{key}={len(value)}" for key, value in golden.items()),
    ]
    if failed:
        summary.append("- Failures: " + ", ".join(f"{name} ({results[name].status})" for name in failed))
    if mismatches:
        summary.append("- Serial/parallel mismatches: " + ", ".join(mismatches))
    print("\n".join(summary), flush=True)
    if os.environ.get("GITHUB_STEP_SUMMARY"):
        with open(os.environ["GITHUB_STEP_SUMMARY"], "a", encoding="utf-8") as stream:
            stream.write("\n".join(summary) + "\n")
    (output / "report.json").write_text(json.dumps({
        "commit": os.environ.get("GITHUB_SHA"),
        "expected": len(names),
        "launched": launched,
        "jobs": args.jobs,
        "full_seconds": full_seconds,
        "serial_seconds": serial_seconds,
        "full": [asdict(results[name]) for name in names],
        "serial": [asdict(result) for result in serial],
        "serial_mismatches": mismatches,
        "golden": golden,
    }, indent=2) + "\n", encoding="utf-8")
    return int(bool(failed or mismatches))


if __name__ == "__main__":
    raise SystemExit(main())
