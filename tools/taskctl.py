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
  alloc  [--block N]               分配场景编号段（全局计数器 build/night/scenario.alloc，
                                   夜跑自主立单用；SBS_SCENARIO_BASE/BLOCK 可覆盖默认 970/10）。
  add    --row "| ID | ... |"      立新任务：--row 给到【状态列之前】的完整行前缀，本工具
                                   自动接 " 待领取 |" 追加到任务表末尾（同号去重 + 格式校验）。
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
    "J": "docs_local/docs/05-任务清单/已收官/J组批量效果工单.md",
    "K": "docs_local/docs/05-任务清单/已收官/K组魂印批产工单.md",
    "R": "docs_local/docs/05-任务清单/已收官/R组修复工单.md",
    "S": "docs_local/docs/05-任务清单/已收官/S组魂印机制调研工单.md",
}
def _lock_dir():
    # 锁放 git 公共目录：worktree 里 .git 是文件不是目录，直写 <repo>/.git 必崩
    # NotADirectoryError（2026-10-08 实证）；与 lib.sh 仓根解析（61970b0）同一修法。
    # 并发语义要求所有副本抢同一把锁——common dir 全检出唯一，正合适。
    try:
        d = subprocess.run(["git", "-C", REPO, "rev-parse", "--git-common-dir"],
                           capture_output=True, text=True, check=True).stdout.strip()
        return os.path.abspath(os.path.join(REPO, d)) if d and not os.path.isabs(d) else os.path.abspath(d or os.path.join(REPO, ".git"))
    except Exception:
        return os.path.join(REPO, ".git")


LOCK_PATH = os.path.join(_lock_dir(), "taskctl.lock")
LOG_PATH = os.path.join(REPO, "docs_local", "tasklog.jsonl")
def scenario_alloc_path():
    # 运行时读 env：selftest 用 SBS_SCENARIO_ALLOC_PATH 隔离到临时文件
    return os.environ.get("SBS_SCENARIO_ALLOC_PATH",
                          os.path.join(REPO, "build", "night", "scenario.alloc"))

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


# ── 属主活跃校验（2026-10-21 抢单事故修）────────────────────────────────────
# 事故：a01-kprod 领 K260 后 finish.sh 自动 release 了它的 agent ID（注册表显
# released），另一会话据此判定"这单被遗弃"→ taskctl release + claim 抢占 →
# 两个 agent 在同一 worktree 同文件并发写。
# 根因：CAS 只断言"状态 == 待领取"，**不校验这一行有没有活着的属主**。
# 修法：
#   ① release 时，若 🔄 行的属主**心跳新鲜**（仍在干活）且不是请求者本人 → 拒退；
#   ② claim 时，若该单号在 agent 注册表里有**活着的 busy 记录** → 拒领；
#   ③ claim 成功后登记 busy（供 ② 判定）。
# "活着" = 心跳在 GUARD 秒内 + status 不是 disabled。**status=released 不作废**
# —— released 只表示"不接新单"，不表示"没在干活"（这正是事故的成因）。
BUSY_GUARD_SEC = 900   # 与 agent_id.py 的 BUSY_CLAIM_GUARD_SEC 保持一致
# 🔄 进行中（<worker> <ts> ...）——属主是括号内第一个 token。闭合括号常在行尾，
# 故不能用"括号之后还有内容"的写法（2026-10-21 自测踩过：那样一个都匹配不到）。
PAT_OWNER = re.compile(r"🔄\s*进行中（\s*([A-Za-z0-9_.@-]+)")


def agents_registry_path():
    # SBS_AGENTS_REG 覆盖：selftest/隔离测试用（同 SBS_SCENARIO_ALLOC_PATH 惯例）
    return os.environ.get("SBS_AGENTS_REG",
                          os.path.join(REPO, "build", "night", "agents.json"))


def load_agents():
    p = agents_registry_path()
    try:
        with open(p, encoding="utf-8") as f:
            d = json.load(f)
        return d.get("agents", {}) if isinstance(d, dict) else {}
    except (ValueError, OSError):
        return {}


def agent_is_live(a):
    """该 agent 是否仍在干活（心跳新鲜 + 未被停用）。"""
    if not a or a.get("status") == "disabled":
        return False
    last = a.get("last_seen", "")
    try:
        ts = datetime.datetime.fromisoformat(last)
    except (ValueError, TypeError):
        return False
    return (datetime.datetime.now() - ts).total_seconds() <= BUSY_GUARD_SEC


def owner_of_wip_line(line):
    """从 🔄 行文本解析出属主 worker（取状态 cell 里的第一个 token）。"""
    cells = split_row(line)
    idx = status_cell_index(cells)
    if idx < 0 or not PAT_WIP.search(cells[idx]):
        return ""
    m = PAT_OWNER.search(cells[idx])
    return m.group(1) if m else ""


def live_owner_of_task(task_id):
    """该单号当前的活属主（agent id）——查 agent 注册表的 busy 记录。"""
    for aid, a in sorted(load_agents().items()):
        if agent_is_live(a) and (a.get("busy") or {}).get("task") == task_id:
            return aid
    return ""


def mark_busy(agent_id, task_id):
    """登记/清除 busy（调 agent_id.py，与注册表单一事实源一致）。"""
    if not agent_id:
        return
    py = os.path.join(REPO, "tools", "night", "agent_id.py")
    if not os.path.exists(py):
        return
    try:
        subprocess.run([sys.executable, py, "busy", agent_id, task_id],
                       capture_output=True, check=False, timeout=10)
    except Exception:
        pass


def clear_busy(agent_id, task_id):
    """这一单完工/退回 → 清掉**任何**把该单号记在 busy 里的 agent。

    为什么按单号清而不只按 worker 清：finish.sh 里 TASKCTL_WORKER 未必设置，
    worker_id() 会退化成 `zhr@pidNNN`（不是注册表里的 agent），按 worker 清会
    静默失败 → 该单号被 busy 锁住 15 分钟才自愈。按单号清与"这一单结束了"的
    语义一致，且不依赖调用方环境。
    """
    py = os.path.join(REPO, "tools", "night", "agent_id.py")
    if not os.path.exists(py):
        return
    for aid, a in sorted(load_agents().items()):
        if (a.get("busy") or {}).get("task") != task_id:
            continue
        if agent_id and aid != agent_id:
            continue          # 指定了 agent 就只清它自己
        try:
            subprocess.run([sys.executable, py, "idle", aid],
                           capture_output=True, check=False, timeout=10)
        except Exception:
            pass


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
        # 抢单防护（2026-10-21）：这一单在 agent 注册表里有**活着的 busy 记录**
        # → 说明另一个会话正在做它 → 拒领（防止两 agent 同 worktree 并发写）。
        live = live_owner_of_task(tid)
        if live and live != worker:
            die(f"{tid}: 已被会话 {live} 领走且其心跳新鲜（正在做这一单）——"
                f"不许抢占。对方收工/心跳超时后再领。")
        cell = f" 🔄 进行中（{worker} {ts} taskctl，worktree ../agents/{tid.lower()}）"
        lines[n] = rewrite_cell(lines[n], cell, idx)
        store(args.path, lines)
        sha = commit_if_needed(args, f"chore(看板): {tid} 领取（taskctl，{worker} {ts}）")
    # busy 登记放锁外（agent_id.py 自带 flock，避免锁序倒挂）
    mark_busy(worker, tid)
    append_log({"action": "claim", "id": tid, "worker": worker, "board": args.board,
                "commit": sha, "file": os.path.basename(args.path)})
    print(json.dumps({"id": tid, "worker": worker, "worktree": f"../agents/{tid.lower()}",
                      "branch": f"feat/{tid.lower()}", "commit": sha,
                      "row": lines[n].strip()}, ensure_ascii=False))


def cmd_done(args):
    with Locked():
        lines = load(args.path)
        n, idx = pick_wip(lines, args.task_id)
        tid = task_id_of(args.task_id)
        cells = split_row(lines[n])
        cells[idx] = " ✅ 完成"
        if args.summary:
            # 行末真实内容列（split 尾部有一个空元素，cells[-1] 是它；D 板 = 备注列，
            # E 板 = 状态列自身——两板惯例都兼容）
            cells[-2] = (cells[-2].rstrip() + f"；{args.summary}").lstrip()
        lines[n] = "|".join(cells)
        store(args.path, lines)
        sha = commit_if_needed(args, f"chore(看板): {args.task_id} ✅ 完成（taskctl）")
    # 完工 = 这一单不再有人做 → 按单号清 busy（锁外；不依赖调用方 worker 名）
    clear_busy("", task_id_of(lines[n]))
    append_log({"action": "done", "id": args.task_id, "worker": worker_id(args),
                "board": args.board, "commit": sha,
                "impl_commit": args.commit, "summary": args.summary,
                "file": os.path.basename(args.path)})
    print(json.dumps({"id": args.task_id, "commit": sha}, ensure_ascii=False))


def cmd_release(args):
    with Locked():
        lines = load(args.path)
        n, idx = pick_wip(lines, args.task_id)
        tid = task_id_of(lines[n])
        worker = worker_id(args)
        # 抢单防护（2026-10-21）：🔄 行有**活着的属主**且不是请求者本人 → 拒退。
        # 事故成因：a01-kprod 正在做 K260，另一会话 release 后 claim 抢走 → 同
        # worktree 同文件并发写。release 是"退回重入队"，不是"踢走别人"。
        owner = owner_of_wip_line(lines[n])
        live = live_owner_of_task(tid)
        forced = ""
        if live and live != worker:
            if not getattr(args, "force", False):
                die(f"{tid}: 会话 {live} 正在做这一单（心跳新鲜）——不许代为退回。"
                    f"若确属误领，请先让该会话自己 finish/release；"
                    f"确知属主已死可加 --force。")
            forced = live
        if owner and owner != worker:
            # 行文本里的属主不在 busy 表里（如 busy 未登记的老数据）——但只要它
            # 不是请求者，就不代退，避免误伤；请求者可显式 --force。
            if not getattr(args, "force", False):
                die(f"{tid}: 该行属主是 {owner}，不是 {worker}——"
                    f"拒绝代为退回（确属误领请加 --force）。")
            forced = forced or owner
        if forced:
            # --force 是逃生口，必须留痕：谁抢的、抢的谁的，进 tasklog 供晨报复盘。
            print(f"taskctl: ⚠️ --force 强制退回 {tid}（原属主 {forced}，"
                  f"请求者 {worker}）", file=sys.stderr)
        cells = split_row(lines[n])
        if args.summary:
            cells[-2] = (cells[-2].rstrip() + f"；{args.summary}").lstrip()
        lines[n] = "|".join(cells)
        lines[n] = rewrite_cell(lines[n], " 待领取 ", idx)
        store(args.path, lines)
        sha = commit_if_needed(args, f"chore(看板): {args.task_id} 退回待领取（taskctl）")
    clear_busy("", tid)
    entry = {"action": "release", "id": args.task_id, "worker": worker_id(args),
             "board": args.board, "commit": sha, "file": os.path.basename(args.path)}
    if forced:
        entry["forced_from"] = forced   # --force 抢单留痕（晨报复盘用）
    append_log(entry)
    print(json.dumps({"id": args.task_id, "commit": sha}, ensure_ascii=False))


def cmd_alloc(args):
    """分配场景编号段：全局计数器串行递增（多会话不撞号）。"""
    base = int(os.environ.get("SBS_SCENARIO_BASE", "970"))
    block = int(os.environ.get("SBS_SCENARIO_BLOCK", str(args.block)))
    alloc_path = scenario_alloc_path()
    os.makedirs(os.path.dirname(alloc_path), exist_ok=True)
    with Locked():
        if not os.path.exists(alloc_path):
            with open(alloc_path, "w", encoding="utf-8") as f:
                f.write(str(base))
        with open(alloc_path, encoding="utf-8") as f:
            start = int(f.read().strip() or base)
        with open(alloc_path, "w", encoding="utf-8") as f:
            f.write(str(start + block))
    seg = f"{start}-{start + block - 1}"
    append_log({"action": "alloc", "segment": seg, "worker": worker_id(args)})
    print(seg)


def cmd_add(args):
    """立新任务：行前缀（状态列之前）+ 自动接" 待领取 |"，追加到任务表末尾。"""
    row_prefix = " ".join(args.row.split())
    m = re.match(r"^\|\s*([^|]+?)\s*\|", row_prefix)
    if not m:
        die('--row 必须以 "| 任务ID |" 开头')
    tid = (args.task_id or m.group(1)).strip()
    row_prefix = re.sub(r"^\|\s*[^|]+?\s*\|", f"| {tid} |", row_prefix, count=1)
    if any(pat.search(row_prefix) for pat in (PAT_WAIT, PAT_WIP, PAT_DONE)):
        die("--row 只给状态列之前的内容（不得自带状态 cell）")
    with Locked():
        lines = load(args.path)
        for line in lines:
            if line.startswith("| ") and task_id_of(line) == tid:
                die(f"{tid}: 看板上已存在同号任务")
        table_rows = [i for i, l in enumerate(lines) if l.startswith("| ")]
        if not table_rows:
            die("看板没有表格行（表头缺失？）")
        lines.insert(table_rows[-1] + 1, f"{row_prefix} 待领取 |")
        store(args.path, lines)
        sha = commit_if_needed(args, f"chore(看板): {tid} 立项（{worker_id(args)} {now()}）")
    append_log({"action": "add", "id": tid, "worker": worker_id(args), "board": args.board,
                "commit": sha, "row": row_prefix})
    print(json.dumps({"id": tid, "commit": sha}, ensure_ascii=False))


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
                            task_id=None, summary=None, commit=None, force=False)
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
    # ⑦ alloc：env 覆盖计数器路径与起点（不碰真计数器）
    os.environ["SBS_SCENARIO_ALLOC_PATH"] = tempfile.mktemp(suffix=".alloc")
    os.environ["SBS_SCENARIO_BASE"] = "900"
    os.environ["SBS_SCENARIO_BLOCK"] = "5"
    ns_alloc = argparse.Namespace(worker="selftest", block=5)
    assert cmd_alloc_capture(ns_alloc) == "900-904"
    assert cmd_alloc_capture(ns_alloc) == "905-909"
    # ⑧ add：行前缀 + 自动接 待领取；同号拒绝
    ns_add = argparse.Namespace(board="E", path=tmp.name, no_git=True, worker="selftest",
                                row="| E8 | 新族 21 宏 | 910-914 |", task_id=None)
    cmd_add(ns_add)
    rows = load(tmp.name)
    assert rows[-1] == "| E8 | 新族 21 宏 | 910-914 | 待领取 |", rows[-1]
    ns_add.row = "| E8 | 重复 | 915 |"
    try:
        cmd_add(ns_add)
        raise AssertionError("同号 add 未被拒绝")
    except SystemExit as e:
        assert e.code != 0
    ns_add.row = "| E9 | 带状态 | 待领取 |"
    try:
        cmd_add(ns_add)
        raise AssertionError("自带状态 cell 的 add 未被拒绝")
    except SystemExit as e:
        assert e.code != 0
    os.unlink(tmp.name)
    print("selftest PASS (claim/done/release/CAS拒绝/最小序号分发/alloc/add)")


def cmd_alloc_capture(ns):
    import io
    import contextlib
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        cmd_alloc(ns)
    return buf.getvalue().strip()


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
    r.add_argument("--force", action="store_true",
                   help="强制退回（越过属主活跃校验；仅限确知属主已死的场景）")
    r.set_defaults(fn=cmd_release)

    l = sub.add_parser("list", help="看板快照")
    common(l)
    l.set_defaults(fn=cmd_list)

    a = sub.add_parser("add", help="立新任务（--row 给状态列前的行前缀）")
    common(a)
    a.add_argument("--row", required=True, help='如 "| J12 | 新族 21 宏 | 先例 … | 970-979 |"')
    a.add_argument("--task-id", dest="task_id", default=None,
                   help="校验/覆盖 --row 首列的任务 id")
    a.set_defaults(fn=cmd_add)

    al = sub.add_parser("alloc", help="分配场景编号段")
    common(al)
    al.add_argument("--block", type=int, default=10, help="段长（默认 10）")
    al.set_defaults(fn=cmd_alloc)

    s = sub.add_parser("selftest", help="临时文件全流程自测")
    s.set_defaults(fn=cmd_selftest)

    args = p.parse_args()
    if args.cmd != "selftest" and getattr(args, "path", None) is None:
        args.path = board_path(args.board, None)
    args.fn(args)


if __name__ == "__main__":
    main()
