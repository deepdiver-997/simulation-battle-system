"""网关门禁：共享 token 鉴权 + 按 IP 限流/封禁（思路对齐 mail-system 的 IntrusionDetector）。

- token：`--auth-token a,b,c`（或 SBS_AUTH_TOKEN），逗号分隔 = 每人一把，便于日志归因；
  未配置时门禁关闭，本机开发零配置不变。
- 封禁：同一 IP 在窗口期内鉴权失败达阈值 → 拉黑一段时间，期间 403 直接拒。
- 限流：/api 滑动窗口限速；WS 并发按 IP 与全局设上限（一条 WS = 一条 sim 会话，
  是网关背后最贵的资源）。
- 全部内存态，重启清零。量级按"社区小范围试用"设计，不是抗 DDoS 方案——
  那一层交给前面的 Cloudflare Tunnel / 云安全组。
"""

import hmac
import time
from collections import defaultdict, deque

from aiohttp import web

# 免鉴权路径：静态壳（登录框要靠它渲染）与健康检查（探活用，无业务数据）
EXEMPT_PATHS = ("/api/health", "/", "/favicon.ico")
EXEMPT_PREFIXES = ("/assets/",)


def client_ip(request: web.Request, trust_proxy: bool) -> str:
    """trust_proxy 时取 X-Forwarded-For 最后一跳（cloudflared 会把真实客户端追加在末尾，
    客户端伪造的前置段被自然稀释）；直连场景一律用 TCP 对端地址，不信任头。"""
    if trust_proxy:
        xff = request.headers.get("X-Forwarded-For", "")
        parts = [p.strip() for p in xff.split(",") if p.strip()]
        if parts:
            return parts[-1]
    return request.remote or "?"


# 令牌获取/反馈入口（401 响应体携带——直接撞上 JSON 的访客也能看到指引）。
TOKEN_OBTAIN_URL = "https://seerinfo.yuyuqaq.cn/ad"
FEEDBACK_MAILTO = "feedback@scut.email"

# 渠道公共令牌（deploy 脚本随 sbs-gateway.env 发给访客的那把，值本身公开）：
# 门禁同样放行，但访客统计**归访客**而不是"自己"——2026-10-19 用户定：
# 此前公共令牌请求被 is_self 计成主人自己，10-19 单日 self=510 全是访客。
# 轮换公共令牌时用 --public-token 覆盖（逗号分隔可多把）。
PUBLIC_TOKENS = ("sbs-3021308f",)


class AuthGuard:
    def __init__(
        self,
        tokens: list[str],
        *,
        trust_proxy: bool = False,    # 有反代/隧道在前时信任 X-Forwarded-For
        failure_window: float = 600,   # 鉴权失败计数窗口（秒）
        failure_threshold: int = 10,   # 窗口内"带错 token"失败多少次触发封禁
        ban_seconds: float = 1800,     # 封禁时长
        api_window: float = 60,       # /api 限速窗口（秒）
        api_max: int = 240,           # 窗口内每 IP 最多 /api 请求数
        ws_per_ip: int = 3,           # 每 IP 并发对局连接上限
        ws_global: int = 32,          # 全局并发对局连接上限
        public_tokens: tuple[str, ...] = PUBLIC_TOKENS,   # 渠道公共令牌（统计归访客）
    ):
        self.tokens = [t for t in tokens if t]
        # 空 = 调用方没配 → 落回内置公共令牌表（调用方常传"env 未设"的空元组，
        # 默认参数会被它顶掉，必须在这里兜底）
        self.public_tokens = (
            tuple(t for t in public_tokens if t) or PUBLIC_TOKENS)
        self.trust_proxy = trust_proxy
        self.failure_window = failure_window
        self.failure_threshold = failure_threshold
        self.ban_seconds = ban_seconds
        self.api_window = api_window
        self.api_max = api_max
        self.ws_per_ip = ws_per_ip
        self.ws_global = ws_global

        self._failures: dict[str, deque[float]] = defaultdict(deque)
        self._banned_until: dict[str, float] = {}
        self._api_hits: dict[str, deque[float]] = defaultdict(deque)
        self._ws_by_ip: dict[str, int] = defaultdict(int)
        self.ws_total = 0

    @property
    def enabled(self) -> bool:
        return bool(self.tokens)

    # ------------------------------------------------------------ token

    @staticmethod
    def _extract_token(request: web.Request) -> str:
        auth = request.headers.get("Authorization", "")
        if auth.lower().startswith("bearer "):
            return auth[7:].strip()
        header = request.headers.get("X-SBS-Token", "")
        if header:
            return header.strip()
        # query 兜底：WS 握手与 <img> 无法带自定义头
        return request.query.get("token", "").strip()

    def _match(self, presented: str) -> bool:
        return self.token_class(presented) is not None

    def token_class(self, presented: str) -> str | None:
        """持的是哪类令牌：'private'=私有（统计=主人自己）、'public'=渠道公共
        （门禁同放行，统计=访客）、None=无/错。鉴权关闭时按 None。"""
        if not self.enabled or not presented:
            return None
        for t in self.tokens:
            if hmac.compare_digest(presented, t):
                return "public" if t in self.public_tokens else "private"
        return None

    def is_self(self, request: web.Request) -> bool:
        """兼容旧调用：该请求是否持**私有** token。"""
        return self.token_class(self._extract_token(request)) == "private"

    def request_token_class(self, request: web.Request) -> str | None:
        """统计用：请求携带的令牌分类（见 token_class）。"""
        return self.token_class(self._extract_token(request))

    # ------------------------------------------------------------ 封禁/限流

    def _prune(self, now: float) -> None:
        for ip, until in list(self._banned_until.items()):
            if until <= now:
                del self._banned_until[ip]
        for store in (self._failures, self._api_hits):
            for ip, hits in list(store.items()):
                while hits and hits[0] <= now - max(self.failure_window, self.api_window):
                    hits.popleft()
                if not hits:
                    del store[ip]

    def _banned(self, ip: str, now: float) -> bool:
        return self._banned_until.get(ip, 0) > now

    def _record_failure(self, ip: str, now: float) -> None:
        hits = self._failures[ip]
        hits.append(now)
        while hits and hits[0] <= now - self.failure_window:
            hits.popleft()
        if len(hits) >= self.failure_threshold:
            self._banned_until[ip] = now + self.ban_seconds
            hits.clear()

    def _api_over_rate(self, ip: str, now: float) -> bool:
        hits = self._api_hits[ip]
        hits.append(now)
        while hits and hits[0] <= now - self.api_window:
            hits.popleft()
        return len(hits) > self.api_max

    # ------------------------------------------------------------ 对外检查

    def check_request(self, request: web.Request) -> web.Response | None:
        """REST 中间件入口；返回 None 表示放行，否则为待回的错误响应。

        ⚠️ 判定顺序（2026-09-28 治理"输错一次就被全网锁 30 分钟"）：
        1. token **先于封禁**判定——持有效 token 的请求即使 IP 已被封也放行
           （封禁是给暴力猜 token 的，不是给忘记自己换过密码的人的）；
        2. 只有**确实带了 token**但校验失败才计失败——首次访问未登录时页面
           自己发出的那批无 token 请求（meta/培养元数据/面板徽标……一次刷新
           六七个）不计入，否则正常登录流程本身就能触发封禁。
        """
        path = request.path
        if path in EXEMPT_PATHS or any(path.startswith(p) for p in EXEMPT_PREFIXES):
            return None
        now = time.monotonic()
        self._prune(now)
        ip = client_ip(request, self.trust_proxy)
        if path.startswith("/api/"):
            if self._api_over_rate(ip, now):
                return web.json_response(
                    {"error": "请求过于频繁，请稍后再试"}, status=429,
                    headers={"Retry-After": str(int(self.api_window))})
        presented = self._extract_token(request)
        if self.enabled and not self._match(presented):
            if self._banned(ip, now):
                return web.json_response(
                    {"error": "尝试次数过多，已被暂时封禁"}, status=403)
            if presented:
                self._record_failure(ip, now)
            return web.json_response(
                {"error": "需要访问令牌（token）", "obtain_url": TOKEN_OBTAIN_URL,
                 "feedback": FEEDBACK_MAILTO}, status=401,
                headers={"WWW-Authenticate": "Bearer"})
        return None

    def try_ws(self, request: web.Request) -> tuple[str, web.Response | None]:
        """WS 握手门禁：token（先于封禁）→ 封禁 → 并发上限。
        返回 (ip, 错误响应)：err 为 None 表示放行并已占一个名额，须配对调用 release_ws(ip)。
        判定顺序同 check_request 注释。"""
        now = time.monotonic()
        self._prune(now)
        ip = client_ip(request, self.trust_proxy)
        presented = self._extract_token(request)
        if self.enabled and not self._match(presented):
            if self._banned(ip, now):
                return ip, web.json_response({"error": "已被暂时封禁"}, status=403)
            if presented:
                self._record_failure(ip, now)
            return ip, web.json_response(
                {"error": "需要访问令牌（token）", "obtain_url": TOKEN_OBTAIN_URL,
                 "feedback": FEEDBACK_MAILTO}, status=401,
                headers={"WWW-Authenticate": "Bearer"})
        if self.ws_total >= self.ws_global:
            return ip, web.json_response(
                {"error": "对局连接数已满，请稍后再试"}, status=429)
        if self._ws_by_ip[ip] >= self.ws_per_ip:
            return ip, web.json_response(
                {"error": "同一 IP 对局连接数过多"}, status=429)
        self._ws_by_ip[ip] += 1
        self.ws_total += 1
        return ip, None

    def release_ws(self, ip: str) -> None:
        self._ws_by_ip[ip] -= 1
        if self._ws_by_ip[ip] <= 0:
            self._ws_by_ip.pop(ip, None)
        self.ws_total = max(0, self.ws_total - 1)
