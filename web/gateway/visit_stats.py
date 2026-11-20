"""网关访客统计（2026-10-01；2026-10-25 扩展真实游玩指标）——除自家 token 请求外的访客计数。

定位：轻量内建计数（RackNerd 这类小 VPS 上不想再挂一层访问日志分析）。
- **self** = 持**私有** token 的请求（主人自己）——单独立账，不进访客数；
- **public** = 渠道公共令牌（发给访客的那把，见 auth.PUBLIC_TOKENS）——门禁同放行，
  但统计**归访客**（2026-10-19 修正：此前任何有效令牌都记 self，公共令牌访客
  全混进了"自己"，10-19 单日 self=510 即此因）；
- **loopback**（127.0.0.1/::1）= 本机开发——不进访客数；
- 其余 = 访客：按 IP 记次数/首末时间/活跃日/路径分布/UA，按日汇总。

分类口径（2026-10-25 起，回应"访客数可解释性"）：
- `/`（打开控制台）单列 **page_views**；`/assets/*`、`/favicon.ico` 静态资源单列
  **static_requests**——都不计入"访客请求数"（此前 2681 次里混着大量静态与被拒请求）；
- 未通过令牌门禁（401/403）的请求单列 **rejected_requests**，不进访客请求数，
  但 per-IP 仍记其路径（看清被拒者在打什么）；
- **真实游玩指标**：WS 开局成功记 battles_started，转发到 BATTLE_OVER 记
  battles_finished；打过至少一把的去重 IP = players（"真实玩家数"的诚实口径，
  比 IP 请求数有意义得多）。loopback 的对局单列 loopback_battles，不进玩家数。
- /api/health 不计（监控探针噪音）。
- UA 只记请求头里的 User-Agent（浏览器本就发送的标准日志，非指纹采集）；
  per-IP 最多留 8 个。

持久化：web/.stats.json（原子写；节流 = 距上次落盘 ≥15s 才真写；on_shutdown 强制写）。
查看：**只走落盘档，不开 HTTP 口**——访客 IP 半敏感，而控制台公共令牌发到每个
访客手里，任何"鉴权保护"的 HTTP 口实际都对全体持令牌者开放（2026-10-06 用户
拍板下线原 /api/stats）。运维用本地脚本 web/stats.py（不入库）ssh 拉档，或手动
ssh cat /opt/sbs/web/.stats.json。
"""

import datetime
import json
import os
import tempfile
import time

LOOPBACK = {"127.0.0.1", "::1"}
SAVE_MIN_INTERVAL = 15.0
TOP_N = 20
MAX_UAS = 8          # per-IP UA 上限（防怪 UA 撑档）
MAX_PATHS = 16       # per-IP 路径前缀上限
UA_MAXLEN = 80


def _today() -> str:
    return datetime.date.today().isoformat()


def _now_iso() -> str:
    return datetime.datetime.now().isoformat(timespec="seconds")


def _path_prefix(path: str) -> str:
    # /api/pets/123 → /api/pets；/ws/battle → /ws/battle；/ → /
    parts = [p for p in path.split("/") if p]
    if not parts:
        return "/"
    return "/" + "/".join(parts[:2]) if parts[0] == "api" else "/" + parts[0]


def _clip_dict(d: dict, cap: int) -> dict:
    """超限时按计数保留 top cap 项（UA/路径档防膨胀）。"""
    if len(d) <= cap:
        return d
    keep = sorted(d.items(), key=lambda kv: -kv[1])[:cap]
    return dict(keep)


class VisitStats:
    def __init__(self, store_path: str):
        self.store_path = store_path
        self.since = _now_iso()
        self.visitors: dict[str, dict] = {}   # ip → 见 _visitor()
        self.days: dict[str, dict] = {}       # date → {"visitor":n,"self":n,"played":n}
        self.paths: dict[str, int] = {}       # 路径前缀 → 访客请求数
        self.players: dict[str, int] = {}     # 打过至少一把的玩家 ip → 完成局数
        self.self_requests = 0
        self.public_token_requests = 0   # 公共令牌请求累计（归访客，单列便于归因）
        self.loopback_requests = 0
        self.loopback_battles = 0        # 本机开发打的完成局数（不进玩家数）
        self.ws_visitor_sessions = 0
        self.page_views = 0              # GET /（打开控制台）
        self.static_requests = 0         # /assets/* + favicon（前端资源装载）
        self.rejected_requests = 0       # 未通过令牌门禁（401/403）
        self.battles_started = 0         # WS init 且 sim 连接成功
        self.battles_finished = 0        # 转发到 BATTLE_OVER
        self._last_save = 0.0
        self._dirty = False

    # ------------------------------------------------------------ 记录

    @staticmethod
    def _visitor() -> dict:
        return {"count": 0, "first": _now_iso(), "last": "", "days": {},
                "paths": {}, "uas": {}, "rejected": 0, "static": 0,
                "page_views": 0, "ws_sessions": 0,
                "battles_started": 0, "battles_finished": 0}

    def _note_ua(self, v: dict, ua: str) -> None:
        ua = (ua or "").strip()[:UA_MAXLEN]
        if not ua:
            return
        uas = v["uas"]
        uas[ua] = uas.get(ua, 0) + 1
        if len(uas) > MAX_UAS:
            v["uas"] = _clip_dict(uas, MAX_UAS)

    def record_request(self, ip: str, path: str, token_class: str | None,
                       *, rejected: bool = False, ua: str = "") -> None:
        """token_class：'private'=主人自己、'public'=渠道公共令牌（访客）、
        None=匿名访客。rejected=True = 请求未通过令牌门禁（401/403）。"""
        if path == "/api/health":
            return
        if ip in LOOPBACK:
            self.loopback_requests += 1
            self._dirty = True
            self.save_if_due()
            return
        today = _today()
        day = self.days.setdefault(today, {"visitor": 0, "self": 0, "played": 0})
        if token_class == "private" and not rejected:
            self.self_requests += 1
            day["self"] += 1
            self._dirty = True
            self.save_if_due()
            return
        v = self.visitors.setdefault(ip, self._visitor())
        self._note_ua(v, ua)
        v["last"] = _now_iso()
        if path == "/":
            v["page_views"] += 1
            self.page_views += 1
        elif path == "/favicon.ico" or path.startswith("/assets/"):
            v["static"] += 1
            self.static_requests += 1
        elif rejected:
            # 被拒单列，不进访客请求数；per-IP 路径仍记（看清被拒者在打什么）
            v["rejected"] += 1
            self.rejected_requests += 1
            v["paths"][_path_prefix(path)] = v["paths"].get(_path_prefix(path), 0) + 1
        else:
            if token_class == "public":
                self.public_token_requests += 1
            v["count"] += 1
            v["days"][today] = v["days"].get(today, 0) + 1
            day["visitor"] += 1
            self.paths[_path_prefix(path)] = self.paths.get(_path_prefix(path), 0) + 1
            v["paths"][_path_prefix(path)] = v["paths"].get(_path_prefix(path), 0) + 1
        v["paths"] = _clip_dict(v["paths"], MAX_PATHS)
        self._dirty = True
        self.save_if_due()

    def record_ws(self, ip: str, token_class: str | None) -> None:
        if ip in LOOPBACK or token_class == "private":
            return
        self.ws_visitor_sessions += 1
        self.visitors.setdefault(ip, self._visitor())["ws_sessions"] += 1
        self._dirty = True
        self.save_if_due()

    def record_battle_start(self, ip: str, token_class: str | None) -> None:
        if ip in LOOPBACK or token_class == "private":
            return   # 主人/本机开发的对局不进访客游玩指标
        v = self.visitors.setdefault(ip, self._visitor())
        v["battles_started"] += 1
        self.battles_started += 1
        self._dirty = True
        self.save_if_due()

    def record_battle_finish(self, ip: str, token_class: str | None) -> None:
        if ip in LOOPBACK or token_class == "private":
            self.loopback_battles += 1
            self._dirty = True
            self.save_if_due()
            return
        v = self.visitors.setdefault(ip, self._visitor())
        v["battles_finished"] += 1
        self.battles_finished += 1
        self.players[ip] = self.players.get(ip, 0) + 1
        self.days.setdefault(_today(), {"visitor": 0, "self": 0, "played": 0})["played"] += 1
        self._dirty = True
        self.save_if_due()

    # ------------------------------------------------------------ 持久化

    def save_if_due(self) -> None:
        if self._dirty and time.monotonic() - self._last_save >= SAVE_MIN_INTERVAL:
            self.save()

    def save(self) -> None:
        payload = {
            "since": self.since,
            "saved_at": _now_iso(),
            "visitors": self.visitors,
            "days": self.days,
            "paths": self.paths,
            "players": self.players,
            "self_requests": self.self_requests,
            "public_token_requests": self.public_token_requests,
            "loopback_requests": self.loopback_requests,
            "loopback_battles": self.loopback_battles,
            "ws_visitor_sessions": self.ws_visitor_sessions,
            "page_views": self.page_views,
            "static_requests": self.static_requests,
            "rejected_requests": self.rejected_requests,
            "battles_started": self.battles_started,
            "battles_finished": self.battles_finished,
        }
        try:
            os.makedirs(os.path.dirname(self.store_path) or ".", exist_ok=True)
            fd, tmp = tempfile.mkstemp(dir=os.path.dirname(self.store_path) or ".",
                                       prefix=".stats-", suffix=".tmp")
            with os.fdopen(fd, "w", encoding="utf-8") as f:
                json.dump(payload, f, ensure_ascii=False, indent=1)
            os.replace(tmp, self.store_path)
            self._last_save = time.monotonic()
            self._dirty = False
        except OSError:
            pass   # 统计失败不影响服务

    def load(self) -> None:
        if not os.path.exists(self.store_path):
            return
        try:
            with open(self.store_path, encoding="utf-8") as f:
                d = json.load(f)
            self.since = d.get("since", self.since)
            self.visitors = d.get("visitors", {})
            self.days = d.get("days", {})
            self.paths = d.get("paths", {})
            self.players = d.get("players", {})
            self.self_requests = d.get("self_requests", 0)
            self.public_token_requests = d.get("public_token_requests", 0)
            self.loopback_requests = d.get("loopback_requests", 0)
            self.loopback_battles = d.get("loopback_battles", 0)
            self.ws_visitor_sessions = d.get("ws_visitor_sessions", 0)
            self.page_views = d.get("page_views", 0)
            self.static_requests = d.get("static_requests", 0)
            self.rejected_requests = d.get("rejected_requests", 0)
            self.battles_started = d.get("battles_started", 0)
            self.battles_finished = d.get("battles_finished", 0)
        except (ValueError, OSError):
            pass   # 坏档当无档

    # ------------------------------------------------------------ 快照

    def snapshot(self) -> dict:
        top = sorted(self.visitors.items(), key=lambda kv: -kv[1].get("count", 0))[:TOP_N]
        return {
            "since": self.since,
            "totals": {
                "visitor_requests": sum(v.get("count", 0) for v in self.visitors.values()),
                "unique_visitor_ips": len(self.visitors),
                "self_requests": self.self_requests,
                "loopback_requests": self.loopback_requests,
                "ws_visitor_sessions": self.ws_visitor_sessions,
                "page_views": self.page_views,
                "static_requests": self.static_requests,
                "rejected_requests": self.rejected_requests,
                "battles_started": self.battles_started,
                "battles_finished": self.battles_finished,
                "players_played": len(self.players),
                "loopback_battles": self.loopback_battles,
            },
            "days": dict(sorted(self.days.items())[-14:]),
            "top_visitors": [
                {"ip": ip, "count": v.get("count", 0), "first": v.get("first", ""),
                 "last": v.get("last", ""), "active_days": len(v.get("days", {})),
                 "rejected": v.get("rejected", 0), "page_views": v.get("page_views", 0),
                 "battles_finished": v.get("battles_finished", 0),
                 "ws_sessions": v.get("ws_sessions", 0),
                 "uas": v.get("uas", {}),
                 "top_paths": dict(sorted(v.get("paths", {}).items(),
                                          key=lambda kv: -kv[1])[:4])}
                for ip, v in top
            ],
            "players": dict(sorted(self.players.items(), key=lambda kv: -kv[1])),
            "paths": dict(sorted(self.paths.items(), key=lambda kv: -kv[1])),
        }
