"""无头端到端：起好 sim_server 与网关后跑本脚本，验证 搜索→详情→WS 开局→自动打完一局。

    web/.venv/bin/python -m web.gateway.smoke_e2e [--port 8070] [--token TOK]

--token：对开鉴权的网关做自检——无/错 token 的 REST 应 401、WS 握手应被拒，
然后带 token 跑完整流程（REST 走 Authorization 头，WS 走 query）。
全部通过打印 SMOKE OK；任何一步失败抛异常非零退出。
"""

import argparse
import asyncio
import json
import time

import aiohttp


def check_guard_unit() -> None:
    """AuthGuard 纯内存口径：token 恒时比较、失败封禁、限速窗口、WS 并发上限。
    不走 HTTP——免得把冒烟用的 IP 真拉黑。"""
    from .auth import AuthGuard

    g = AuthGuard(["tok-a", "tok-b"])
    assert g.enabled
    assert g._match("tok-b") and not g._match("tok-c") and not g._match("")
    now = time.monotonic()
    for i in range(g.failure_threshold):
        g._record_failure("203.0.113.9", now)
        assert g._banned("203.0.113.9", now) == (i == g.failure_threshold - 1)

    g2 = AuthGuard(["t"], api_max=3, api_window=60)
    for _ in range(3):
        assert not g2._api_over_rate("198.51.100.7", now)
    assert g2._api_over_rate("198.51.100.7", now)

    g3 = AuthGuard(["t"], ws_per_ip=1, ws_global=2)
    req = type("R", (), {"headers": {}, "query": {"token": "t"}, "remote": "192.0.2.7",
                         "path": "/ws/battle"})()
    ip1, e1 = g3.try_ws(req)
    _, e2 = g3.try_ws(req)
    assert e1 is None and e2 is not None, "每 IP 并发上限未生效"
    g3.release_ws(ip1)
    _, e3 = g3.try_ws(req)
    assert e3 is None, "release 后应可再连"
    print("  AuthGuard 口径 ok：token 比较 / 失败封禁 / 限速窗口 / WS 并发上限")


async def main(port: int, token: str) -> None:
    base = f"http://127.0.0.1:{port}"
    headers = {"Authorization": f"Bearer {token}"} if token else {}

    async with aiohttp.ClientSession() as http:
        if token:
            async with http.get(f"{base}/api/pets", params={"q": "波克尔"}) as r:
                assert r.status == 401, f"无 token 应 401，得到 {r.status}"
            async with http.get(f"{base}/api/pets",
                                params={"q": "波克尔", "token": "wrong"}) as r:
                assert r.status == 401, f"错 token 应 401，得到 {r.status}"
            print("  鉴权 ok：无/错 token 的 REST 均被拒（401）")

        async with http.get(f"{base}/api/health", headers=headers) as r:
            health = await r.json()
            assert health["ok"], health

        async with http.get(f"{base}/api/pets", params={"q": "波克尔"},
                            headers=headers) as r:
            pets = (await r.json())["pets"]
            assert pets, "搜不到波克尔"
            pet_id = pets[0]["petId"]

        async with http.get(f"{base}/api/pets/{pet_id}", headers=headers) as r:
            detail = await r.json()
            assert detail["learnableMoves"], "learnableMoves 为空"
            skills = [m["id"] for m in detail["learnableMoves"][:5]]
            assert len(skills) == 5, "可学技能不足 5"
            print(f"  搜索+详情 ok：{detail['name']} 技能 {skills}")

        async with http.get(f"{base}/api/sample-lineup", headers=headers) as r:
            lineup = await r.json()

        async with http.get(f"{base}/api/coverage", headers=headers) as r:
            cov = await r.json()
            print("  覆盖率 ok：",
                  {k: (v["implemented"], v["unimplemented"]) for k, v in cov.items()})

    # ── WS：开局并自动打完 ──
    async with aiohttp.ClientSession() as http:
        if token:
            try:
                await http.ws_connect(f"{base}/ws/battle")
                raise AssertionError("无 token 的 WS 握手应被拒")
            except aiohttp.WSServerHandshakeError as exc:
                assert exc.status == 401, f"WS 无 token 应 401，得到 {exc.status}"
            print("  鉴权 ok：无 token 的 WS 握手被拒（401）")

        ws_qs = f"?token={token}" if token else ""
        ws = await http.ws_connect(f"{base}/ws/battle{ws_qs}")
        await ws.send_json({"type": "init", "lineup": lineup})

        rounds = 0
        over = None
        while over is None and rounds < 500:
            msg = await asyncio.wait_for(ws.receive(), timeout=30)
            if msg.type != aiohttp.WSMsgType.TEXT:
                continue
            event = json.loads(msg.data)
            etype = event.get("type")
            if etype == "input_required":
                data = event["data"]
                player = data["player"]
                legal = data["legal"]
                if legal.get("mustChoosePet"):
                    slot = next(p["slot"] for p in legal["pets"] if p["usable"])
                    action = {"type": "action", "robot": player,
                              "actionType": 0, "index": slot}
                else:
                    usable = [s for s in legal["skills"] if s["usable"]]
                    assert usable, f"无可用技能：{legal}"
                    action = {"type": "action", "robot": player,
                              "actionType": 1, "index": usable[0]["index"]}
                await ws.send_json(action)
            elif etype == "battle_over":
                over = event["data"]
            elif etype == "error":
                raise AssertionError(f"服务端错误：{event}")
        assert over is not None, "500 轮内未结束"
        print(f"  对局 ok：winner={over['winner']} round={over['round']} reason={over['reason']}")
        await ws.close()

    print("SMOKE OK")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, default=8070)
    parser.add_argument("--token", default="",
                        help="网关开了 --auth-token 时传入其中一把做鉴权自检")
    args = parser.parse_args()
    check_guard_unit()
    asyncio.run(main(args.port, args.token))
