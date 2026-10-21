#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""夜跑 agent 注册表（build/night/agents.json）——领取式 agent ID。

问题（2026-10-13 用户提出）：夜跑会话此前**没有稳定身份**，worker 名是各会话自选的
（j-prod-1/k-prod-5…），重复领取靠运气、事后对账靠翻 tasklog；"这个会话该不该停"
也无从表达。

设计：
- **领取制**：会话开工第一步调 `agent_id.sh <前缀>` 领一个**唯一 ID**（如 `a07-kprod`），
  登记进 build/night/agents.json（原子写 + flock）。同一前缀再领 → 拿到下一个序号。
- **心跳**：会话在每次主循环判定前刷新 last_seen（is_heartbeat=1）。
- **停用/释放**：会话收摊时 `agent_id.sh release <id>` 注销（status=released）；
  用户可 `agent_id.sh disable <id>` 强制停用某会话（status=disabled）。
- **停止策略联动**：策略档 policy.json 的 "stop" 模式 + 每个 agent 自己的
  `stop_at` 覆盖 → should_stop 判定时按 agent ID 查表（agent 只读策略，判定权在脚本）。
- **用户可读可写**：用户能看全部 agent（谁在跑、领的哪个 ID、上次心跳）。

⚠️ status 的语义（2026-10-21 抢单事故后修订）：
  `released` **不等于"这个会话不干活了"**——它只表示"完成当前单、暂不接新单"。
  2026-10-21 实证：a01-kprod 领完 K259 时 finish.sh 自动 release 了它的 agent ID，
  注册表随即显示 released；而它**同一时刻**又领了 K260 并继续干到 01:03。
  另一个会话据此判定"这单被遗弃"→ 抢占 → 两 agent 在同一 worktree 同文件并发写。
  故：**"是否还在干活"只认心跳（last_seen），不认 status**。status 只表达
  "是否还愿意接新单 / 是否被用户停用"。新增 `busy` 字段显式记录"手上有单"，
  由 claim/done 自动维护，心跳超时的 busy 记录视为失效（可被接管）。

文件结构（build/night/agents.json）：
  {"agents": {"a07-kprod": {"id","prefix","status","first_seen","last_seen","note",
                            "busy": {"task":"K260","since":"..."}|null}}}
"""

import datetime
import fcntl
import json
import os
import subprocess
import sys
import tempfile
import time

STATUS_ACTIVE = "active"
STATUS_RELEASED = "released"
STATUS_DISABLED = "disabled"
HEARTBEAT_STALE_SEC = 1800   # 30 分钟无心跳视为失联（仅提示，不自动释放）
# 抢单保护：🔄 行属主心跳在此秒数内 = 属主活着，不许接管（2026-10-21 事故修）
BUSY_CLAIM_GUARD_SEC = 900    # 15 分钟：属主心跳新鲜即视为"正在做这一单"


def hub_root() -> str:
    d = subprocess.run(
        ["git", "-C", os.path.dirname(os.path.abspath(__file__)),
         "rev-parse", "--path-format=absolute", "--git-common-dir"],
        capture_output=True, text=True, check=False).stdout.strip()
    return os.path.dirname(d) if d else os.path.dirname(
        os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def night_dir() -> str:
    return os.path.join(hub_root(), "build", "night")


def registry_path() -> str:
    # SBS_AGENTS_REG 覆盖：selftest/隔离测试用（与 taskctl.py 同名同义）。
    # 两边必须认同一个覆盖，否则 taskctl 在隔离测试里读 A 文件、agent_id 写 B 文件
    # → busy 登记/清除静默失效（2026-10-21 自测实测）。
    return os.environ.get("SBS_AGENTS_REG",
                          os.path.join(night_dir(), "agents.json"))


def lock_path() -> str:
    if os.environ.get("SBS_AGENTS_REG"):
        return registry_path() + ".lock"
    return os.path.join(night_dir(), "agents.lock")


def now_iso() -> str:
    return datetime.datetime.now().isoformat(timespec="seconds")


def load_unlocked() -> dict:
    p = registry_path()
    if not os.path.exists(p):
        return {"agents": {}}
    try:
        with open(p, "r", encoding="utf-8") as f:
            d = json.load(f)
        if not isinstance(d, dict) or not isinstance(d.get("agents"), dict):
            return {"agents": {}}
        return d
    except (ValueError, OSError):
        return {"agents": {}}


def save_unlocked(data: dict) -> None:
    p = registry_path()
    os.makedirs(os.path.dirname(p), exist_ok=True)
    fd, tmp = tempfile.mkstemp(dir=os.path.dirname(p), prefix=".agents-", suffix=".tmp")
    with os.fdopen(fd, "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False, indent=1)
        f.write("\n")
    os.replace(tmp, p)


class RegistryLock:
    """flock 临界区：领 ID/改状态/写档 全程互斥（跨会话）。"""

    def __enter__(self):
        os.makedirs(os.path.dirname(lock_path()), exist_ok=True)
        self.fd = os.open(lock_path(), os.O_CREAT | os.O_RDWR, 0o644)
        fcntl.flock(self.fd, fcntl.LOCK_EX)
        return self

    def __exit__(self, *exc):
        fcntl.flock(self.fd, fcntl.LOCK_UN)
        os.close(self.fd)
        return False


def next_id(prefix: str, agents: dict) -> str:
    """给前缀分配下一个序号 ID（a01-<prefix> / a02-<prefix>…）。"""
    n = 0
    for aid in agents:
        if aid.startswith("a") and "-" + prefix in aid:
            try:
                n = max(n, int(aid[1:aid.index("-")]))
            except ValueError:
                continue
    return f"a{n + 1:02d}-{prefix}"


def cmd_claim(prefix: str, note: str = "") -> int:
    prefix = prefix.strip() or "agent"
    with RegistryLock():
        data = load_unlocked()
        agents = data["agents"]
        aid = next_id(prefix, agents)
        agents[aid] = {
            "id": aid, "prefix": prefix, "status": STATUS_ACTIVE,
            "first_seen": now_iso(), "last_seen": now_iso(), "note": note}
        save_unlocked(data)
    print(aid)
    return 0


def cmd_heartbeat(aid: str, note: str = "") -> int:
    with RegistryLock():
        data = load_unlocked()
        a = data["agents"].get(aid)
        if not a:
            print(f"[agents] 未登记的 ID：{aid}（先用 agent_id.sh {aid.split('-')[-1]} 领取）",
                  file=sys.stderr)
            return 3
        a["last_seen"] = now_iso()
        if note:
            a["note"] = note
        save_unlocked(data)
    print(f"[agents] 心跳已更新：{aid}")
    return 0


def cmd_release(aid: str) -> int:
    with RegistryLock():
        data = load_unlocked()
        a = data["agents"].get(aid)
        if not a:
            print(f"[agents] 未登记的 ID：{aid}", file=sys.stderr)
            return 3
        a["status"] = STATUS_RELEASED
        a["last_seen"] = now_iso()
        save_unlocked(data)
    print(f"[agents] 已释放：{aid}")
    return 0


# ── busy：手上有单（2026-10-21 抢单事故新增）────────────────────────────────
# 语义：busy 记录"这个 agent 正在做哪一单"。**是否还在干活只认心跳，不认 status**
# （released 只表示"不接新单"，见文件头注）。抢单防护据此判定"🔄 行能不能接管"。

def cmd_busy(aid: str, task: str) -> int:
    """登记"手上有单"。"""
    with RegistryLock():
        data = load_unlocked()
        a = data["agents"].get(aid)
        if not a:
            print(f"[agents] 未登记的 ID：{aid}", file=sys.stderr)
            return 3
        a["busy"] = {"task": task, "since": now_iso()}
        a["last_seen"] = now_iso()
        save_unlocked(data)
    print(f"[agents] {aid} 手上登记：{task}")
    return 0


def cmd_idle(aid: str) -> int:
    """清掉 busy 标记（收工/交还单号时调）。"""
    with RegistryLock():
        data = load_unlocked()
        a = data["agents"].get(aid)
        if not a:
            print(f"[agents] 未登记的 ID：{aid}", file=sys.stderr)
            return 3
        a["busy"] = None
        a["last_seen"] = now_iso()
        save_unlocked(data)
    print(f"[agents] {aid} 已清 busy")
    return 0


def busy_is_live(a: dict) -> bool:
    """busy 记录是否仍然有效（属主还活着）。

    判定 = 心跳新鲜（last_seen 在 BUSY_CLAIM_GUARD_SEC 内）**且** status 不是
    disabled。status=released **不**作废 busy——这正是 2026-10-21 事故的成因
    （finish.sh 自动 release → 注册表显 released → 另一会话误判为遗弃单）。
    """
    if not a:
        return False
    if a.get("status") == STATUS_DISABLED:
        return False
    if not a.get("busy"):
        return False
    last = a.get("last_seen", "")
    try:
        ts = datetime.datetime.fromisoformat(last)
    except (ValueError, TypeError):
        return False
    return (datetime.datetime.now() - ts).total_seconds() <= BUSY_CLAIM_GUARD_SEC


def cmd_claimable(aid: str) -> int:
    """查询：该 agent 的 busy 记录是否还活着（1=活，0=死/无）。

    stdout 只输出 0/1，exit 恒 0——供 taskctl/claim.sh 做抢单防护判定。
    """
    data = load_unlocked()
    a = data.get("agents", {}).get(aid)
    print("1" if busy_is_live(a) else "0")
    return 0


def cmd_owner_of_task(task: str) -> int:
    """查询：当前是谁在做这一单（busy.task == task 且记录有效）。

    stdout 输出 agent id（无则空行）。供 taskctl 在 CAS 之外做"属主活跃"判定。
    """
    data = load_unlocked()
    for aid, a in sorted(data.get("agents", {}).items()):
        if busy_is_live(a) and a.get("busy", {}).get("task") == task:
            print(aid)
            return 0
    print("")
    return 0


def cmd_disable(aid: str) -> int:
    """用户侧强制停用某会话（agent 自身不该调用）。"""
    with RegistryLock():
        data = load_unlocked()
        a = data["agents"].get(aid)
        if not a:
            print(f"[agents] 未登记的 ID：{aid}", file=sys.stderr)
            return 3
        a["status"] = STATUS_DISABLED
        a["last_seen"] = now_iso()
        save_unlocked(data)
    print(f"[agents] 已停用：{aid}")
    return 0


def cmd_enable(aid: str) -> int:
    with RegistryLock():
        data = load_unlocked()
        a = data["agents"].get(aid)
        if not a:
            print(f"[agents] 未登记的 ID：{aid}", file=sys.stderr)
            return 3
        a["status"] = STATUS_ACTIVE
        a["last_seen"] = now_iso()
        save_unlocked(data)
    print(f"[agents] 已启用：{aid}")
    return 0


def cmd_list() -> int:
    data = load_unlocked()
    agents = data.get("agents", {})
    if not agents:
        print("[agents] 注册表为空（尚无会话领取 ID）")
        return 0
    print(f"[agents] 注册表（{len(agents)} 个）—— "
          f"最后修改 {now_iso()}")
    for aid, a in sorted(agents.items()):
        last = a.get("last_seen", "")
        stale = ""
        try:
            ts = datetime.datetime.fromisoformat(last)
            if (datetime.datetime.now() - ts).total_seconds() > HEARTBEAT_STALE_SEC:
                stale = "  ⚠️ 心跳超时"
        except (ValueError, TypeError):
            stale = "  ⚠️ 时间戳异常"
        busy = a.get("busy")
        if busy:
            live = "活" if busy_is_live(a) else "失效"
            busy_txt = f"  手上={busy.get('task','?')}({live})"
        else:
            busy_txt = ""
        print(f"  {aid:<16} {a.get('status', '?'):<9} 前缀={a.get('prefix', '?'):<10} "
              f"首见={a.get('first_seen', '?')[:19]} 末次={last[:19]}{stale}{busy_txt}"
              f"{'  ' + a.get('note', '') if a.get('note') else ''}")
    return 0


def cmd_get(aid: str) -> int:
    data = load_unlocked()
    a = data.get("agents", {}).get(aid)
    if not a:
        print(f"[agents] 未登记的 ID：{aid}", file=sys.stderr)
        return 3
    print(json.dumps(a, ensure_ascii=False))
    return 0


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__)
        return 64
    cmd = sys.argv[1]
    rest = sys.argv[2:]
    if cmd == "claim":
        return cmd_claim(rest[0] if rest else "agent",
                         rest[1] if len(rest) > 1 else "")
    if cmd == "heartbeat":
        return cmd_heartbeat(rest[0] if rest else "",
                              rest[1] if len(rest) > 1 else "")
    if cmd == "release":
        return cmd_release(rest[0] if rest else "")
    if cmd == "busy":
        return cmd_busy(rest[0] if rest else "", rest[1] if len(rest) > 1 else "")
    if cmd == "idle":
        return cmd_idle(rest[0] if rest else "")
    if cmd == "claimable":
        return cmd_claimable(rest[0] if rest else "")
    if cmd == "owner-of":
        return cmd_owner_of_task(rest[0] if rest else "")
    if cmd == "disable":
        return cmd_disable(rest[0] if rest else "")
    if cmd == "enable":
        return cmd_enable(rest[0] if rest else "")
    if cmd == "list":
        return cmd_list()
    if cmd == "get":
        return cmd_get(rest[0] if rest else "")
    if cmd == "path":
        print(registry_path())
        return 0
    print(__doc__)
    return 64


if __name__ == "__main__":
    sys.exit(main())
