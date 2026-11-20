"""嗑药 + BP 待命背包端到端回归：带 medicines/standby 开局 →
我方合法动作集含真实药剂名与库存。跑法：web/.venv/bin/python web/gateway/med_bp_smoke.py
（需 sim_server 与网关已起）。打印 MED-BP SMOKE OK 即通过。"""
import asyncio
import json

import aiohttp

LINEUP = {
    "side1": {"petId": 12, "skills": [10001, 20002, 10008, 10009, 20015],
              "base": [0] * 6, "traitId": 0},
    "side2": {"petId": 15, "skills": [10001, 20004, 20006, 10011, 10040],
              "base": [0] * 6, "traitId": 0},
    "medicines": {"side1": {"300011": 2, "300016": 1}, "side2": {}},
    "standby": {"side1": [5000, 12], "side2": [2532]},
}


async def main():
    async with aiohttp.ClientSession() as http:
        ws = await http.ws_connect("http://127.0.0.1:8070/ws/battle")
        await ws.send_str(json.dumps({"type": "init", "lineup": LINEUP}))
        while True:
            msg = await asyncio.wait_for(ws.receive(), 8)
            if msg.type != aiohttp.WSMsgType.TEXT:
                print("non-text:", msg.type)
                return
            ev = json.loads(msg.data)
            if ev["type"] == "error":
                print("ERROR:", ev["data"])
                return
            if ev["type"] == "input_required" and ev["data"]["player"] == 0:
                meds = ev["data"]["legal"]["medicines"]
                stocked = [(m["name"], m["count"], m["usable"])
                           for m in meds if m["count"] > 0]
                print("我方药剂:", stocked)
                assert ("初级体力药剂", 2, True) in stocked, "体力药剂库存/可用性不对"
                assert any(n == "初级活力药剂" for n, _, _ in stocked), "活力药剂缺失"
                print("MED-BP SMOKE OK")
                return


asyncio.run(main())
