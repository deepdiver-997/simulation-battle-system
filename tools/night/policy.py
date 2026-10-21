#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""夜跑策略档读写（policy.json）——用户写、agent 读。

设计要点（2026-10-13）：
- **策略档与代码分离**：停止条件、窗口、是否允许超时，都放在 build/night/policy.json，
  用户随时改 → agent 下一次判定立即生效（不必重启会话、不必改脚本）。
- **agent 只读**：本模块的写入口需要显式 --set（= 用户或用户授权的动作）；
  agent 日常只调 `tools/night/policy.sh show` 读。
- **原子写**：临时文件 + os.replace，避免读到半截 JSON。
- **坏档当默认**：JSON 解析失败时回退到"run 不限窗口"（宁可多跑，不静默停机），
  并在输出里显式提示。

用法：
  python3 tools/night/policy.py show                      # 读（agent 日常用）
  python3 tools/night/policy.py set mode=run             # 写（用户/授权动作）
  python3 tools/night/policy.py set allow_overtime=true
  python3 tools/night/policy.py path
"""

import datetime
import json
import os
import sys
import tempfile

DEFAULTS = {
    "mode": "run",            # run | stop | window
    "window_start": "0000",
    "stop_at": "0825",
    "allow_overtime": False,
    "note": "",
    "updated_at": "",
}

BOOLEAN_KEYS = {"allow_overtime"}
MODE_VALUES = {"run", "stop", "window"}


def hub_root() -> str:
    import subprocess
    d = subprocess.run(
        ["git", "-C", os.path.dirname(os.path.abspath(__file__)),
         "rev-parse", "--path-format=absolute", "--git-common-dir"],
        capture_output=True, text=True, check=False).stdout.strip()
    return os.path.dirname(d) if d else os.path.dirname(
        os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def policy_path() -> str:
    return os.path.join(hub_root(), "build", "night", "policy.json")


def load() -> dict:
    """读策略档；不存在/损坏 → 回退默认（run）。返回 (policy, warning)。"""
    p = policy_path()
    warn = ""
    data = dict(DEFAULTS)
    if not os.path.exists(p):
        return data, f"策略档不存在（{p}），按默认 run 处理"
    try:
        with open(p, "r", encoding="utf-8") as f:
            raw = json.load(f)
        if isinstance(raw, dict):
            data.update({k: v for k, v in raw.items() if k in DEFAULTS})
        else:
            warn = "策略档不是 JSON 对象，按默认处理"
    except (ValueError, OSError) as e:
        warn = f"策略档读取失败（{e}），按默认 run 处理"
    if data.get("mode") not in MODE_VALUES:
        warn = f"mode 非法（{data.get('mode')}），按 run 处理"
        data["mode"] = "run"
    return data, warn


def save(policy: dict) -> str:
    """原子写策略档。"""
    p = policy_path()
    os.makedirs(os.path.dirname(p), exist_ok=True)
    fd, tmp = tempfile.mkstemp(dir=os.path.dirname(p), prefix=".policy-", suffix=".tmp")
    with os.fdopen(fd, "w", encoding="utf-8") as f:
        json.dump(policy, f, ensure_ascii=False, indent=2)
        f.write("\n")
    os.replace(tmp, p)
    return p


def parse_bool(v: str) -> bool:
    return str(v).strip().lower() in ("1", "true", "yes", "on")


def main() -> int:
    args = sys.argv[1:]
    if not args or args[0] == "show":
        data, warn = load()
        if warn:
            print(f"[policy] 提示：{warn}")
        print(json.dumps(data, ensure_ascii=False))
        return 0
    if args[0] == "path":
        print(policy_path())
        return 0
    if args[0] == "set":
        data, warn = load()
        if warn:
            print(f"[policy] 提示：{warn}")
        for kv in args[1:]:
            if "=" not in kv:
                print(f"[policy] 参数格式应为 key=value：{kv}", file=sys.stderr)
                return 64
            k, v = kv.split("=", 1)
            k = k.strip()
            if k not in DEFAULTS:
                print(f"[policy] 未知字段：{k}（可用：{', '.join(sorted(DEFAULTS))}）",
                      file=sys.stderr)
                return 64
            if k in BOOLEAN_KEYS:
                data[k] = parse_bool(v)
            else:
                data[k] = v.strip()
        data["updated_at"] = datetime.date.today().isoformat()
        p = save(data)
        print(f"[policy] 已写入 {p}")
        print(json.dumps(data, ensure_ascii=False))
        return 0
    print(__doc__)
    return 64


if __name__ == "__main__":
    sys.exit(main())
