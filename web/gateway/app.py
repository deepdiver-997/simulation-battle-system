"""对战调试台网关：浏览器(JSON/WS) ⟷ sim_server(二进制 TCP) 之间的翻译层。

- REST：目录搜索/详情（4399 展示优先、数据库回退）、覆盖率、示例阵容、图片代理。
- WS   /ws/battle：一条浏览器连接 ⟷ 一条 sim_server solo 会话，帧双向转发；
  浏览器侧消息是 JSON（{"type":"action","robot":0,"actionType":1,"index":2}），
  网关负责编成线协议帧；服务端推送（TAPE/INPUT_REQUIRED/…）原样转 JSON 回传。
- 门禁：--auth-token 启用共享 token 鉴权 + 按 IP 限流/封禁（web/gateway/auth.py），
  未配置时门禁关闭（本机开发零配置）。

必须在仓库根目录启动（相对路径找 sqlite / jsonl / build/）：
    web/.venv/bin/python -m web.gateway.app --port 8070
对外服务：--host 0.0.0.0 --auth-token '各人一把,逗号分隔' [--tls-certfile crt --tls-keyfile key]
前端构建产物在 web/frontend/dist 时自动托管。
"""

import argparse
import asyncio
import hashlib
import json
import logging
import os
import re
import ssl
import sys
import time

from aiohttp import ClientSession, ClientTimeout, web
import aiohttp

from . import proto
from .auth import AuthGuard, client_ip
from .visit_stats import VisitStats
from .catalog import Catalog
from .sim_client import SimClient

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(name)s %(message)s")
log = logging.getLogger("gateway")

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# sim_server FSM 动作类型（线 payload 用；0=换宠 1=技能 2=药剂）
ACTION_CHOOSE_PET = 0
ACTION_SKILL = 1
ACTION_MEDICINE = 2


def expand_lineup(lineup: dict) -> dict:
    """宽容化阵容：某侧给数组时接受 1~6 只（上场少于 6，低位填充、空位垫底——
    引擎 parse_party 校验 0 号位非空）；给单对象则原样透传。"""
    if not isinstance(lineup, dict):
        raise ValueError("lineup must be an object with side1/side2")
    out = {}
    for key in ("side1", "side2"):
        if key not in lineup:
            raise ValueError(f"lineup missing {key}")
        side = lineup[key]
        if isinstance(side, dict):
            out[key] = side
            continue
        if not isinstance(side, list) or not side:
            raise ValueError(f"{key} must be a non-empty array or a single pet object")
        if len(side) > 6:
            raise ValueError(f"{key} must contain at most 6 pets, got {len(side)}")
        out[key] = side
    if "equip" in lineup:
        out["equip"] = lineup["equip"]
    # 待命背包（BP 模式，格劳瑞自爆等"赛前读者"）、药剂库存、阵容级抗性默认
    #（培养线 2026-09-27："off"/"opened"/"maxed_damage"，引擎只填未显式配抗性的精灵）
    for key in ("standby", "medicines", "resistDefault"):
        if key in lineup:
            out[key] = lineup[key]
    return out


def env_path(*env_keys, default_parts):
    for key in env_keys:
        val = os.environ.get(key)
        if val:
            return val if os.path.isabs(val) else os.path.join(REPO_ROOT, val)
    return os.path.join(REPO_ROOT, *default_parts)


class Gateway:
    def __init__(self, args):
        self.sim_host = args.sim_host
        self.sim_port = args.sim_port
        self.catalog = Catalog(
            db_path=env_path("SBS_DB", default_parts=("scripts/data/processed/seer_unity.sqlite",)),
            pets_jsonl=env_path("SBS_PETS_JSONL",
                                default_parts=("reference/scrape_4399/out/pets_full.jsonl",)),
            hunyin_jsonl=env_path("SBS_HUNYIN_JSONL",
                                  default_parts=("reference/scrape_4399/out/hunyin.jsonl",)),
            registry_path=env_path("SBS_REGISTRY_JSON",
                                   default_parts=("build/registry.json",)),
        )
        self.dist_dir = env_path("SBS_FRONTEND_DIST",
                                 default_parts=("web/frontend/dist",))
        self.img_cache_dir = os.path.join(REPO_ROOT, "web/.imgcache")
        os.makedirs(self.img_cache_dir, exist_ok=True)
        self._refresh_lock = asyncio.Lock()
        self._refresh_at = float("-inf")
        self._http: ClientSession | None = None
        self.guard = AuthGuard(
            [t for t in re.split(r"[,\s]+", args.auth_token or "") if t],
            trust_proxy=args.trust_proxy,
            public_tokens=tuple(
                t for t in re.split(r"[,\s]+", args.public_token or "") if t),
        )
        self.refresh_cooldown = args.refresh_cooldown
        # 访客统计（2026-10-01）：self=持**私有** token；公共渠道令牌归访客
        # （2026-10-19 修正——此前公共令牌请求被计成主人自己）；loopback 不计访客。
        self.trust_proxy = args.trust_proxy
        self.stats = VisitStats(os.path.join(REPO_ROOT, "web", ".stats.json"))
        self.stats.load()
        # 版本指纹基线（2026-10-01）：网关进程生命期内第一次见到的引擎指纹。
        # 之后每次 HELLO 与它比对，变化即大字报警——引擎被单独重建/重启而网关还旧。
        self.engine_fp_baseline: dict | None = None

    # ------------------------------------------------------------ 版本指纹

    async def engine_fingerprint(self) -> dict | None:
        """当前引擎指纹（临时连接探一次 HELLO）。meta/health 用；战斗连接的指纹
        走 SimClient 自带握手，两边同源。"""
        probe = SimClient(self.sim_host, self.sim_port)
        try:
            await probe.connect()
            return probe.fingerprint
        except OSError:
            return None
        finally:
            await probe.close()

    def note_engine_fp(self, fp: dict | None) -> dict | None:
        """登记一次指纹并与基线比对：变化 = 响亮报警（引擎单独更新而网关还旧）。"""
        if not fp:
            return fp
        if self.engine_fp_baseline is None:
            self.engine_fp_baseline = fp
            return fp
        if fp != self.engine_fp_baseline:
            log.critical(
                "=" * 62 + "\n"
                "[engine-fingerprint] 引擎指纹与本网关进程启动时不再一致！\n"
                "  基线: %s\n  当前: %s\n"
                "  引擎已被单独重建/重启——显示面可能落后于引擎（新字段/新枚举漏显）。\n"
                "  修复：重启网关 web/up.sh；浏览器端请硬刷新。\n" + "=" * 62,
                json.dumps(self.engine_fp_baseline, sort_keys=True),
                json.dumps(fp, sort_keys=True))
        return fp

    # ------------------------------------------------------------ REST

    async def health(self, request):
        return web.json_response({
            "ok": True,
            "sim": {"host": self.sim_host, "port": self.sim_port},
            "registry": {k: len(v) for k, v in self.catalog.registry.items()},
            "pets4399": len(self.catalog.pets4399),
            "engineFingerprint": self.engine_fp_baseline,
        })

    async def pets(self, request):
        q = request.query.get("q", "")
        limit = min(int(request.query.get("limit", "60")), 200)
        teamable = request.query.get("teamable", "1") != "0"
        rows = await asyncio.to_thread(self.catalog.search_pets, q, limit, teamable)
        return web.json_response({"pets": rows})

    async def pet_detail(self, request):
        pet_id = int(request.match_info["pet_id"])
        detail = await asyncio.to_thread(self.catalog.pet_detail, pet_id)
        if detail is None:
            raise web.HTTPNotFound(text=json.dumps({"error": f"pet {pet_id} not found"}),
                                   content_type="application/json")
        return web.json_response(detail)

    async def moves(self, request):
        q = request.query.get("q", "")
        limit = min(int(request.query.get("limit", "60")), 200)
        rows = await asyncio.to_thread(self.catalog.search_moves, q, limit)
        return web.json_response({"moves": rows})

    async def move_detail(self, request):
        move_id = int(request.match_info["move_id"])
        detail = await asyncio.to_thread(self.catalog.move_detail, move_id)
        if detail is None:
            raise web.HTTPNotFound(text=json.dumps({"error": f"move {move_id} not found"}),
                                   content_type="application/json")
        return web.json_response(detail)

    async def move_effects(self, request):
        move_id = int(request.match_info["move_id"])
        data = await asyncio.to_thread(self.catalog.move_effects, move_id)
        if data is None:
            raise web.HTTPNotFound(text=json.dumps({"error": f"move {move_id} not found"}),
                                   content_type="application/json")
        return web.json_response(data)

    async def traits(self, request):
        q = request.query.get("q", "")
        limit = min(int(request.query.get("limit", "50")), 200)
        rows = await asyncio.to_thread(self.catalog.search_traits, q, limit)
        return web.json_response({"traits": rows})

    async def meta(self, request):
        """时点/异常对照表 + 可用药剂清单（断点下拉、事件带中文化、嗑药编辑器）。
        附带当前引擎指纹——前端启动时存 localStorage，之后与 WS hello 比对。"""
        data = await asyncio.to_thread(self.catalog.meta)
        data["medicines"] = await asyncio.to_thread(self.catalog.medicine_items)
        data["engineFingerprint"] = self.note_engine_fp(await self.engine_fingerprint())
        return web.json_response(data)

    async def effect_index(self, request):
        """效果显示索引（控制台全量状态翻译层，开工文档 §2.1）：技能/魂印两个
        注册空间的 id → 显示文本表。量级 ~4200 条，客户端开局拉一次。"""
        data = await asyncio.to_thread(self.catalog.effect_index)
        return web.json_response(data)

    async def coverage(self, request):
        data = await asyncio.to_thread(self.catalog.coverage)
        return web.json_response(data)

    async def boss_invalid_effects(self, request):
        """boss 无效效果清单（resources/boss_invalid_effects.json，2026-09-26 禁用基建）。
        来源 = effect_icon.tips 尾注"（boss无效）"解析；前端做徽标 + 一键预填禁用。"""
        import json as _json
        path = os.path.join(REPO_ROOT, "resources", "boss_invalid_effects.json")
        try:
            with open(path, encoding="utf-8") as f:
                return web.json_response(_json.load(f))
        except FileNotFoundError:
            return web.json_response({"count": 0, "ids": [],
                                      "note": "清单未生成（tools 脚本可重建）"})

    async def sample_lineup(self, request):
        data = await asyncio.to_thread(self.catalog.sample_lineup)
        return web.json_response(data)

    # ------------------------------------------------------------ 培养（feat/peiyang）

    async def cultivation_meta(self, request):
        data = await asyncio.to_thread(self.catalog.cultivation_meta)
        return web.json_response(data)

    async def cultivation_inscriptions(self, request):
        q = request.query.get("q", "")
        limit = min(int(request.query.get("limit", "60")), 200)
        data = await asyncio.to_thread(self.catalog.search_inscriptions, q, limit)
        return web.json_response(data)

    async def cultivation_suits(self, request):
        q = request.query.get("q", "")
        limit = min(int(request.query.get("limit", "30")), 100)
        data = await asyncio.to_thread(self.catalog.search_suits, q, limit)
        return web.json_response(data)

    async def cultivation_equips(self, request):
        q = request.query.get("q", "")
        limit = min(int(request.query.get("limit", "30")), 100)
        data = await asyncio.to_thread(self.catalog.search_equips, q, limit)
        return web.json_response(data)

    async def cultivation_preview(self, request):
        try:
            cfg = await request.json()
        except Exception:
            raise web.HTTPBadRequest(text="bad json")
        data = await asyncio.to_thread(self.catalog.cultivation_preview, cfg)
        return web.json_response(data)

    async def coverage_refresh(self, request):
        """重跑 sim_server --dump-registry-exit 刷新注册表，再重读。
        会拉起子进程，是公开部署时最贵的端点：除鉴权外再设全局冷却。"""
        async with self._refresh_lock:
            elapsed = time.monotonic() - self._refresh_at
            if elapsed < self.refresh_cooldown:
                return web.json_response({
                    "ok": False,
                    "error": f"刷新冷却中，约 {int(self.refresh_cooldown - elapsed)}s 后可重试",
                }, status=429)
            sim_bin = env_path("SBS_SIM_BIN", default_parts=("build/sim_server",))
            if not os.path.exists(sim_bin):
                return web.json_response({"ok": False, "error": f"{sim_bin} 不存在"}, status=400)
            self._refresh_at = time.monotonic()
            proc = await asyncio.create_subprocess_exec(
                sim_bin, "--dump-registry-exit", self.catalog.registry_path,
                cwd=REPO_ROOT,
                stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.STDOUT,
            )
            out, _ = await proc.communicate()
            ok = proc.returncode == 0 and self.catalog.load_registry()
            return web.json_response({
                "ok": ok,
                "registry": {k: len(v) for k, v in self.catalog.registry.items()},
                "output": out.decode("utf-8", "replace")[-500:],
            })

    ALLOWED_IMG_HOSTS = (".4399.com", "4399.com", ".niaogebiji.com")

    async def img_proxy(self, request):
        url = request.query.get("u", "")
        if not url.startswith(("http://", "https://")):
            raise web.HTTPBadRequest(text="bad url")
        host = url.split("/", 3)[2].lower()
        if not any(host == h or host.endswith(h) for h in self.ALLOWED_IMG_HOSTS):
            raise web.HTTPBadRequest(text="host not allowed")
        key = hashlib.sha1(url.encode()).hexdigest()
        cache_path = os.path.join(self.img_cache_dir, key)
        if os.path.exists(cache_path) and os.path.getsize(cache_path) > 0:
            with open(cache_path, "rb") as f:
                data = f.read()
            ctype = "image/png" if data[:4] == b"\x89PNG" else \
                    "image/jpeg" if data[:2] == b"\xff\xd8" else "image/gif"
            return web.Response(body=data, content_type=ctype)
        if self._http is None:
            self._http = ClientSession(timeout=ClientTimeout(total=10))
        try:
            async with self._http.get(url) as resp:
                if resp.status != 200:
                    raise web.HTTPBadGateway(text=f"upstream {resp.status}")
                data = await resp.read()
        except aiohttp.ClientError as exc:
            raise web.HTTPBadGateway(text=f"fetch failed: {exc}")
        with open(cache_path, "wb") as f:
            f.write(data)
        ctype = resp.headers.get("Content-Type", "image/png").split(";")[0]
        return web.Response(body=data, content_type=ctype)

    # ------------------------------------------------------------ WS 桥

    async def ws_battle(self, request):
        ip, err = self.guard.try_ws(request)
        if err is not None:
            return err
        tc = self.guard.request_token_class(request)
        self.stats.record_ws(ip, tc)
        ws = web.WebSocketResponse(heartbeat=30)
        await ws.prepare(request)
        sim: SimClient | None = None
        forward_task: asyncio.Task | None = None

        async def pump_sim_to_ws():
            assert sim is not None
            while True:
                frame = await sim.recv()
                if frame is None:
                    await ws.send_json({"type": "sim_closed"})
                    return
                if frame["cmd"] == proto.S_BATTLE_OVER:
                    # 真实游玩指标（2026-10-25）：一局完整打完
                    self.stats.record_battle_finish(ip, tc)
                name = proto.CMD_NAMES.get(frame["cmd"], f"cmd_{frame['cmd']}")
                payload = frame["payload"]
                data: dict = {"type": name}
                if payload:
                    try:
                        data["data"] = json.loads(payload.decode("utf-8"))
                    except (ValueError, UnicodeDecodeError):
                        data["dataHex"] = payload.hex()
                await ws.send_json(data)

        try:
            async for msg in ws:
                if msg.type != aiohttp.WSMsgType.TEXT:
                    continue
                try:
                    event = json.loads(msg.data)
                except ValueError:
                    await ws.send_json({"type": "error", "data": {"text": "bad json"}})
                    continue
                etype = event.get("type", "")

                if etype == "init":
                    if sim is None:
                        try:
                            sim = SimClient(self.sim_host, self.sim_port)
                            await sim.connect()
                        except OSError as exc:
                            await ws.send_json({"type": "error", "data": {
                                "text": f"连不上 sim_server({self.sim_host}:{self.sim_port})：{exc}"}})
                            sim = None
                            continue
                        forward_task = asyncio.create_task(pump_sim_to_ws())
                        # 版本握手（连接内已在 SimClient.connect 完成）：登记 + 通知前端。
                        # 前端拿它与启动时 /api/meta 的指纹比——引擎在会话中被更新就弹横幅。
                        await ws.send_json({"type": "hello",
                                            "data": self.note_engine_fp(sim.fingerprint)})
                        # 真实游玩指标（2026-10-25）：开局成功（一条 WS 对一局 solo）
                        self.stats.record_battle_start(ip, tc)
                    lineup = expand_lineup(event.get("lineup"))
                    payload = json.dumps(lineup, ensure_ascii=False).encode("utf-8")
                    await sim.send(proto.C_INIT_BATTLE_JSON, payload)

                elif sim is None:
                    await ws.send_json({"type": "error",
                                        "data": {"text": "尚未开局（先发 init）"}})

                elif etype == "action":
                    await sim.send(proto.C_SELECT_SKILL, proto.build_action(
                        int(event["robot"]), int(event["actionType"]), int(event["index"])))
                elif etype == "sync":
                    await sim.send(proto.C_SYNC_STATE)
                elif etype == "fullstate":
                    await sim.send(proto.C_DEBUG_FULLSTATE)
                elif etype == "legal":
                    player = event.get("player")
                    await sim.send(proto.C_LEGAL_ACTIONS,
                                   proto.build_i32(player) if player is not None else b"")
                elif etype == "step":
                    await sim.send(proto.C_DEBUG_STEP)
                elif etype == "continue":
                    await sim.send(proto.C_DEBUG_CONTINUE)
                elif etype == "mutate":
                    payload = json.dumps(event.get("ops_payload", {}), ensure_ascii=False)
                    await sim.send(proto.C_DEBUG_MUTATE, payload.encode("utf-8"))
                elif etype == "breakpoint":
                    await sim.send(proto.C_DEBUG_BREAKPOINT,
                                   proto.build_i32(int(event["state"])))
                elif etype == "ping":
                    await sim.send(proto.C_HEARTBEAT)
                else:
                    await ws.send_json({"type": "error",
                                        "data": {"text": f"unknown type {etype}"}})
        finally:
            if forward_task:
                forward_task.cancel()
            if sim:
                await sim.close()
            self.guard.release_ws(ip)
        return ws

    # ------------------------------------------------------------ 静态与入口

    def _index_html(self):
        index = os.path.join(self.dist_dir, "index.html")
        if os.path.exists(index):
            with open(index, "rb") as f:
                return web.Response(body=f.read(), content_type="text/html")
        return web.Response(
            text="<h3>前端未构建</h3><p>cd web/frontend && npm install && npm run build，"
                 "或开发期另跑 <code>npm run dev</code>（代理已配到本网关）。</p>",
            content_type="text/html")

    async def index(self, request):
        return self._index_html()

    def build_app(self):
        guard = self.guard

        @web.middleware
        async def stats_mw(request, handler):
            """请求统计（2026-10-25 口径）：REST 拿到真实响应后记（401/403 归
            rejected）；WS 是长连接、handler 返回时连接已结束——握手前按令牌
            预判记录。UA 只记请求头 User-Agent（浏览器本就发送，标准日志）。"""
            ip = client_ip(request, self.trust_proxy)
            path = request.path
            ua = request.headers.get("User-Agent", "")
            tc = guard.request_token_class(request)
            if path == "/ws/battle":
                self.stats.record_request(ip, path, tc,
                                          rejected=guard.enabled and tc is None, ua=ua)
                return await handler(request)
            resp = await handler(request)
            self.stats.record_request(ip, path, tc,
                                      rejected=resp.status in (401, 403), ua=ua)
            return resp

        @web.middleware
        async def guard_mw(request, handler):
            err = guard.check_request(request)
            if err is not None:
                return err
            return await handler(request)

        app = web.Application(middlewares=[stats_mw, guard_mw])
        app.on_shutdown.append(lambda a: self.stats.save())
        app.router.add_get("/api/health", self.health)
        app.router.add_get("/api/pets", self.pets)
        app.router.add_get("/api/pets/{pet_id}", self.pet_detail)
        app.router.add_get("/api/moves", self.moves)
        app.router.add_get("/api/moves/{move_id}", self.move_detail)
        app.router.add_get("/api/moves/{move_id}/effects", self.move_effects)
        app.router.add_get("/api/traits", self.traits)
        app.router.add_get("/api/meta", self.meta)
        app.router.add_get("/api/effect-index", self.effect_index)
        app.router.add_get("/api/coverage", self.coverage)
        app.router.add_get("/api/boss-invalid-effects", self.boss_invalid_effects)
        app.router.add_post("/api/coverage/refresh", self.coverage_refresh)
        app.router.add_get("/api/sample-lineup", self.sample_lineup)
        app.router.add_get("/api/cultivation/meta", self.cultivation_meta)
        app.router.add_get("/api/cultivation/inscriptions", self.cultivation_inscriptions)
        app.router.add_get("/api/cultivation/equips", self.cultivation_equips)
        app.router.add_get("/api/cultivation/suits", self.cultivation_suits)
        app.router.add_post("/api/cultivation/preview", self.cultivation_preview)
        app.router.add_get("/api/img", self.img_proxy)
        app.router.add_get("/ws/battle", self.ws_battle)
        app.router.add_get("/", self.index)
        if os.path.isdir(os.path.join(self.dist_dir, "assets")):
            app.router.add_static("/assets", os.path.join(self.dist_dir, "assets"))
        return app


def main(argv=None):
    parser = argparse.ArgumentParser(description="sbs 对战调试台网关")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8070)
    parser.add_argument("--sim-host", default=os.environ.get("SBS_SIM_HOST", "127.0.0.1"))
    parser.add_argument("--sim-port", type=int,
                        default=int(os.environ.get("SBS_SIM_PORT", "4399")))
    parser.add_argument("--auth-token", default=os.environ.get("SBS_AUTH_TOKEN", ""),
                        help="共享访问令牌，逗号分隔可发多把（每人一把便于日志归因）；"
                             "留空=关闭鉴权，仅建议本机开发")
    parser.add_argument("--public-token", default=os.environ.get("SBS_PUBLIC_TOKEN", ""),
                        help="渠道公共令牌（逗号分隔可多把）：门禁同放行，访客统计归访客；"
                             "缺省用 auth.PUBLIC_TOKENS 内置值")
    parser.add_argument("--trust-proxy", action="store_true",
                        help="前面有隧道/反代时按 X-Forwarded-For 末跳取客户端 IP")
    parser.add_argument("--tls-certfile", default=os.environ.get("SBS_TLS_CERT", ""),
                        help="PEM 证书；与 --tls-keyfile 同时给出时网关直接上 HTTPS")
    parser.add_argument("--tls-keyfile", default=os.environ.get("SBS_TLS_KEY", ""))
    parser.add_argument("--refresh-cooldown", type=int, default=300,
                        help="覆盖率对账（拉子进程）的全局冷却秒数")
    args = parser.parse_args(argv)

    if bool(args.tls_certfile) != bool(args.tls_keyfile):
        parser.error("--tls-certfile 与 --tls-keyfile 必须同时给出")
    ssl_context = None
    if args.tls_certfile:
        ssl_context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        ssl_context.load_cert_chain(args.tls_certfile, args.tls_keyfile)

    gw = Gateway(args)
    app = gw.build_app()
    scheme = "https" if ssl_context else "http"
    log.info("网关就绪 %s://%s:%d（sim=%s:%d，dist=%s，鉴权=%s）",
             scheme, args.host, args.port, args.sim_host, args.sim_port, gw.dist_dir,
             f"{len(gw.guard.tokens)} 把 token" if gw.guard.enabled else "关闭")
    if not gw.guard.enabled and args.host not in ("127.0.0.1", "localhost", "::1"):
        log.warning("对外监听但未启用鉴权（--auth-token）：任何能连到端口的人都可开局/对账")
    web.run_app(app, host=args.host, port=args.port, ssl_context=ssl_context, print=None)


if __name__ == "__main__":
    sys.exit(main())
