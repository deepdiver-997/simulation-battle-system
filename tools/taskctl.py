#!/usr/bin/env python3
"""taskctl —— D/E 组任务看板的并发安全领取器。

为什么需要：多个 agent 会话共享同一 main-local 检出（hub），"读看板→改状态→git commit"
是复合操作，git 的 index.lock 只保护单条命令、不保护复合动作——两个会话同时读到同一行
"待领取"再各自提交，后提交者静默覆盖前者 = 双领取。本工具用 flock 把复合操作变成
临界区，并用 CAS（改前断言状态未变）把双领取从"静默覆盖"变成"响亮失败"。

协议（2026-09-29 起）：agent 可以随意【读】看板，但领取/翻✅/退回一律走本工具，
不再手改看板行后自行 git 提交。

子命令：
  claim  [--board D|E] [TASK_ID]   领取：省略 TASK_ID = 吐出序号最小的待领取任务。
                                   stdout 输出一行 JSON（id/worktree/行内容），供会话直接消费。
  done   TASK_ID --commit SHA --summary "..."   翻 ✅：状态 cell 改 ✅ 完成，summary 追加到行末备注 cell。
  release TASK_ID                  退回：🔄 行回退为 待领取（误领/停止窗口用）。
  list   [--board D|E]             看板快照 + 🔄 行滞留时长。
  selftest                         在临时文件上跑全流程断言（不碰真看板、不 git）。

并发安全设计：
  ① flock(.git/taskctl.lock) 包住"读→改→写→commit"整个临界区（所有会话同机，文件锁足够）；
  ② CAS：替换前断言目标 cell 仍处于期望状态（claim 要求 待领取 / done 要求 🔄），
     不满足即非 0 退出——抢不到就重跑，绝不静默覆盖；
  ③ 原子写：临时文件 + os.replace，杜绝半行写入；
  ④ 全部动作追加 docs_local/tasklog.jsonl（审计/滞留分析）。
"""
import argparse
import datetime
import fcntl
import json
import os
import re
import subprocess
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BOARDS = {
    "D": "docs_local/docs/05-任务清单/已收官/D组启动看板.md",
    "E": "docs_local/docs/05-任务清单/已收官/E组批量效果工单.md",
    "F": "docs_local/docs/05-任务清单/已收官/F组批量效果工单.md",
    "G": "docs_local/docs/05-任务清单/已收官/G组批量效果工单.md",
    "H": "docs_local/docs/05-任务清单/已收官/H组批量效果工单.md",
    "J": "docs_local/docs/05-任务清单/J组批量效果工单.md",
    "K": "docs_local/docs/05-任务清单/已收官/K组魂印批产工单.md",
    "R": "docs_local/docs/05-任务清单/已收官/R组修复工单.md",
    "S": "docs_local/docs/05-任务清单/已收官/S组魂印机制调研工单.md",
}
LOCK_PATH = os.path.join(REPO, ".git", "taskctl.lock")
LOG_PATH = os.path.join(REPO, "docs_local", "tasklog.jsonl")

PAT_WAIT = re.compile(r"待领取")
PAT_WIP = re.compile(r"🔄")
PAT_DONE = re.compile(r"✅")
PAT_TS = re.compile(r"(\d{4}-\d{2}-\d{2} \d{2}:\d{2})")


def die(msg, code=1):
    print(f"taskctl: {msg}", file=sys.stderr)
    sys.exit(code)


def board_path(board, override):
    return os.path.join(REPO, override) if override else os.path.join(REPO, BOARDS[board])


def git(args):
    subprocess.run(["git", *args], cwd=REPO, check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def append_log(entry):
    os.makedirs(os.path.dirname(LOG_PATH), exist_ok=True)
    with open(LOG_PATH, "a", encoding="utf-8") as f:
        f.write(json.dumps({"ts": datetime.datetime.now().isoformat(timespec="seconds"),
                            **entry}, ensure_ascii=False) + "\n")


def split_row(line):
    """'| a | b | c |' -> cells with delimiters preserved: ['',' a ',' b ',' c ','']."""
    return line.split("|")


def status_cell_index(cells):
    for i, c in enumerate(cells):
        if PAT_WAIT.search(c) or PAT_WIP.search(c) or PAT_DONE.search(c):
            return i
    return -1


def task_id_of(line):
    cells = split_row(line)
    return cells[1].strip() if len(cells) > 1 else ""


def load(path):
    with open(path, encoding="utf-8") as f:
        return f.read().splitlines()


def store(path, lines):
    dirn = os.path.dirname(path)
    fd, tmp = tempfile.mkstemp(dir=dirn, prefix=".taskctl-", suffix=".tmp")
    with os.fdopen(fd, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    os.replace(tmp, path)


def rewrite_cell(line, new_cell, idx):
    cells = split_row(line)
    cells[idx] = new_cell
    return "|".join(cells)


def pick_waiting(lines, task_id):
    """返回 (行号, 状态cell下标)。task_id=None 时取文件顺序第一个待领取行。"""
    found = []
    for n, line in enumerate(lines):
        if not line.startswith("| ") or PAT_DONE.search(line):
            continue
        idx = status_cell_index(split_row(line))
        if idx < 0 or not PAT_WAIT.search(split_row(line)[idx]):
            continue
        found.append((n, idx))
        if task_id is None:
            return n, idx
        if task_id_of(line) == task_id:
            return n, idx
    if task_id is not None:
        die(f"{task_id}: 不是待领取状态（不存在/已被领取/已完成）")
    die("看板上没有待领取任务")


def pick_wip(lines, task_id):
    for n, line in enumerate(lines):
        if line.startswith(f"| {task_id} |") or (task_id_of(line) == task_id and line.startswith("|")):
            idx = status_cell_index(split_row(line))
            if idx >= 0 and PAT_WIP.search(split_row(line)[idx]):
                return n, idx
    die(f"{task_id}: 没有 🔄 进行中 行（不存在/状态不符）")


class Locked:
    """flock 临界区：覆盖 读→改→写→commit 全程。"""

    def __enter__(self):
        self.fd = open(LOCK_PATH, "w")
        fcntl.flock(self.fd, fcntl.LOCK_EX)
        return self

    def __exit__(self, *exc):
        fcntl.flock(self.fd, fcntl.LOCK_UN)
        self.fd.close()


def worker_id(args):
    if args.worker:
        return args.worker
    if os.environ.get("TASKCTL_WORKER"):
        return os.environ["TASKCTL_WORKER"]   # 稳定会话标识（agent 波次用，pid 每次调用都换）
    try:
        name = subprocess.run(["git", "config", "user.name"], cwd=REPO,
                              capture_output=True, text=True, check=True).stdout.strip()
    except Exception:
        name = "unknown"
    return f"{name}@pid{os.getpid()}"


def commit_if_needed(args, msg):
    if args.no_git:
        return None
    rel = os.path.relpath(args.path, REPO)
    git(["add", rel])
    git(["commit", "-m", msg])
    sha = subprocess.run(["git", "rev-parse", "--short", "HEAD"], cwd=REPO,
                         capture_output=True, text=True, check=True).stdout.strip()
    return sha


def now():
    return datetime.datetime.now().strftime("%Y-%m-%d %H:%M")


def cmd_claim(args):
    with Locked():
        lines = load(args.path)
        n, idx = pick_waiting(lines, args.task_id)
        tid = task_id_of(lines[n])
        ts = now()
        worker = worker_id(args)
        cell = f" 🔄 进行中（{worker} {ts} taskctl，worktree ../agents/{tid.lower()}）"
        lines[n] = rewrite_cell(lines[n], cell, idx)
        store(args.path, lines)
        sha = commit_if_needed(args, f"chore(看板): {tid} 领取（taskctl，{worker} {ts}）")
    append_log({"action": "claim", "id": tid, "worker": worker, "board": args.board,
                "commit": sha, "file": os.path.basename(args.path)})
    print(json.dumps({"id": tid, "worker": worker, "worktree": f"../agents/{tid.lower()}",
                      "branch": f"feat/{tid.lower()}", "commit": sha,
                      "row": lines[n].strip()}, ensure_ascii=False))


def cmd_done(args):
    with Locked():
        lines = load(args.path)
        n, idx = pick_wip(lines, args.task_id)
        cells = split_row(lines[n])
        cells[idx] = " ✅ 完成"
        if args.summary:
            # 行末真实内容列（split 尾部有一个空元素，cells[-1] 是它；D 板 = 备注列，
            # E 板 = 状态列自身——两板惯例都兼容）
            cells[-2] = (cells[-2].rstrip() + f"；{args.summary}").lstrip()
        lines[n] = "|".join(cells)
        store(args.path, lines)
        sha = commit_if_needed(args, f"chore(看板): {args.task_id} ✅ 完成（taskctl）")
    append_log({"action": "done", "id": args.task_id, "worker": worker_id(args),
                "board": args.board, "commit": sha,
                "impl_commit": args.commit, "summary": args.summary,
                "file": os.path.basename(args.path)})
    print(json.dumps({"id": args.task_id, "commit": sha}, ensure_ascii=False))


def cmd_release(args):
    with Locked():
        lines = load(args.path)
        n, idx = pick_wip(lines, args.task_id)
        if args.summary:
            cells = split_row(lines[n])
            cells[-2] = (cells[-2].rstrip() + f"；{args.summary}").lstrip()
            lines[n] = "|".join(cells)
        lines[n] = rewrite_cell(lines[n], " 待领取 ", idx)
        store(args.path, lines)
        sha = commit_if_needed(args, f"chore(看板): {args.task_id} 退回待领取（taskctl）")
    append_log({"action": "release", "id": args.task_id, "worker": worker_id(args),
                "board": args.board, "commit": sha, "file": os.path.basename(args.path)})
    print(json.dumps({"id": args.task_id, "commit": sha}, ensure_ascii=False))


def cmd_list(args):
    lines = load(args.path)
    now_dt = datetime.datetime.now()
    for line in lines:
        if not line.startswith("| "):
            continue
        cells = split_row(line)
        idx = status_cell_index(cells)
        if idx < 0:
            continue
        tid, status = task_id_of(line), cells[idx].strip()
        extra = ""
        if PAT_WIP.search(cells[idx]):
            m = PAT_TS.search(cells[idx])
            if m:
                started = datetime.datetime.strptime(m.group(1), "%Y-%m-%d %H:%M")
                hours = (now_dt - started).total_seconds() / 3600
                extra = f"  [滞留 {hours:.1f}h" + ("，超 8h 建议人工复核]" if hours > 8 else "]")
            else:
                extra = "  [手改领取（无 taskctl 时间戳）]"
        print(f"{tid:>4}  {status[:40]}{extra}")


def cmd_selftest(args):
    """临时文件全流程断言，不碰真看板、不 git、不写审计日志。"""
    globals()["append_log"] = lambda entry: None   # selftest 动作不污染真实 tasklog
    tmp = tempfile.NamedTemporaryFile(mode="w", suffix=".md", delete=False, encoding="utf-8")
    tmp.write("# 测试看板\n\n| 序号 | 内容 | 状态 | 备注 |\n|---|---|---|---|\n"
              "| E5 | 暴击触发族 | 待领取 | |\n"
              "| E6 | PP 族 | 待领取 | |\n"
              "| E7 | 先制族 | 待领取 | |\n")   # 行号：0标题 1空 2表头 3分隔 4=E5 5=E6 6=E7
    tmp.close()
    ns = argparse.Namespace(board="E", path=tmp.name, no_git=True, worker="selftest",
                            task_id=None, summary=None, commit=None)
    # ① claim 省略 id → 最小序号 E5
    ns.task_id = None
    out = json.loads(capture_claim(ns))
    assert out["id"] == "E5", out
    # ② 再 claim → E6
    ns.task_id = None
    assert json.loads(capture_claim(ns))["id"] == "E6"
    # ③ 显式 claim 已领取的 E5 → 必须响亮失败
    ns.task_id = "E5"
    try:
        capture_claim(ns)
        raise AssertionError("重复领取未被拒绝")
    except SystemExit as e:
        assert e.code != 0
    # ④ done E5 → ✅ + summary 落行末内容列（cells[-2]）
    ns.task_id = "E5"
    ns.summary, ns.commit = "测试摘要 feat/x 1234567", "1234567"
    cmd_done(ns)
    rows = load(tmp.name)
    got = re.sub(r"\s*", "", rows[4])
    assert got == "|E5|暴击触发族|✅完成|；测试摘要feat/x1234567|", rows[4]
    # ⑤ release E6 → 回到 待领取
    ns.task_id, ns.summary = "E6", None
    cmd_release(ns)
    rows = load(tmp.name)
    assert "待领取" in rows[5], rows[5]
    # ⑥ done E7 前先 claim（done 只接受 🔄）
    ns.task_id = "E7"
    assert json.loads(capture_claim(ns))["id"] == "E7"
    os.unlink(tmp.name)
    print("selftest PASS (claim/done/release/CAS拒绝/最小序号分发)")


def capture_claim(ns):
    import io
    import contextlib
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        cmd_claim(ns)
    return buf.getvalue().strip()


def main():
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    sub = p.add_subparsers(dest="cmd", required=True)

    def common(sp):
        sp.add_argument("--board", default="E", choices=list(BOARDS))
        sp.add_argument("--file", dest="path", default=None, help="覆盖看板路径（selftest 用）")
        sp.add_argument("--no-git", action="store_true", help="只改文件不提交")
        sp.add_argument("--worker", default=None)

    c = sub.add_parser("claim", help="领取（省略 id = 最小待领取）")
    common(c)
    c.add_argument("task_id", nargs="?", default=None)
    c.set_defaults(fn=cmd_claim)

    d = sub.add_parser("done", help="翻 ✅")
    common(d)
    d.add_argument("task_id")
    d.add_argument("--commit", required=True, help="实现分支上的 commit 号")
    d.add_argument("--summary", default="", help="一句话实现清单（追加到行末备注 cell）")
    d.set_defaults(fn=cmd_done)

    r = sub.add_parser("release", help="退回待领取")
    common(r)
    r.add_argument("task_id")
    r.add_argument("--summary", default="", help="退回原因（追加到行末内容列）")
    r.set_defaults(fn=cmd_release)

    l = sub.add_parser("list", help="看板快照")
    common(l)
    l.set_defaults(fn=cmd_list)

    s = sub.add_parser("selftest", help="临时文件全流程自测")
    s.set_defaults(fn=cmd_selftest)

    args = p.parse_args()
    if args.cmd != "selftest" and getattr(args, "path", None) is None:
        args.path = board_path(args.board, None)
    args.fn(args)


if __name__ == "__main__":
    main()
