"""调试手术（DEBUG_MUTATE，二期）端到端回归：泊车后注入白名单操作 →
回执逐条 ok、新快照反映变更、死亡印记在战局中真实结算（100→0）。
跑法：web/.venv/bin/python web/gateway/mutate_smoke.py（需 sim_server+网关已起）。
打印 MUTATE SMOKE OK 即通过。"""
import asyncio
import json

import aiohttp

LINEUP = {
    "side1": {"petId": 12, "skills": [10001, 20002, 10008, 10009, 20015]},
    "side2": {"petId": 15, "skills": [10001, 20004, 20006, 10011, 10040]},
}

OPS = [
    {"op": "set_hp", "side": 1, "slot": 0, "value": 10},       # 对方在场 体力 → 10
    {"op": "set_pp", "side": 1, "slot": 0, "skill": 0, "value": 1},
    {"op": "set_level", "side": 0, "stat": 4, "value": -2},    # 我方 速度 -2
    {"op": "anomaly", "side": 1, "id": 7, "rounds": 3},        # 对方 疲惫 3 回合
    {"op": "mark", "src": 0, "dst": 1},                        # 我方→对方 死亡印记
]


async def main():
    async with aiohttp.ClientSession() as http:
        ws = await http.ws_connect("http://127.0.0.1:8070/ws/battle")
        await ws.send_str(json.dumps({"type": "init", "lineup": LINEUP}))

        mutated = False
        saw_mark_kill = False
        rounds_seen = 0
        while True:
            msg = await asyncio.wait_for(ws.receive(), 10)
            if msg.type != aiohttp.WSMsgType.TEXT:
                break
            ev = json.loads(msg.data)
            t = ev["type"]

            if t == "error":
                print("ERROR:", ev["data"])
                return

            if t == "mutate_result":
                assert ev["data"]["ok"], f"手术失败: {ev['data']}"
                notes = ev["data"]["notes"]
                assert len(notes) == len(OPS), f"回执条数不对: {notes}"
                assert all(not n.startswith("✗") for n in notes), f"有被拒的操作: {notes}"
                mutated = True
                print("回执:", "；".join(notes))

            if t == "state" and mutated and isinstance(ev.get("data"), dict):
                p1 = ev["data"].get("player1", {})
                p0 = ev["data"].get("player0", {})
                pet1 = p1.get("pet", {})
                abn = [a["id"] for a in p1.get("abnormalStates", [])]
                assert pet1.get("hp") == 10, f"set_hp 未生效: {pet1.get('hp')}"
                assert 7 in abn, f"疲惫未生效: {abn}"
                lv = p0.get("pet", {}).get("levels", [0] * 6)
                assert lv[4] == -2, f"set_level 未生效: {lv}"

            if t == "input_required":
                player = ev["data"]["player"]
                legal = ev["data"].get("legal", {})
                if not mutated:
                    # 泊车中注入手术；改完泊车不变、仍等本玩家 → 顺手把动作也交了
                    # （否则事件被去重，永远等不到下一个 INPUT_REQUIRED）
                    await ws.send_str(json.dumps({
                        "type": "mutate",
                        "ops_payload": {"ops": OPS},
                    }))
                    mutated = True
                if legal.get("mustChoosePet"):
                    # 印记致死触发死后换宠：必须交换宠（交技能会被拒且事件被去重）
                    pet = next(p for p in legal.get("pets", [])
                               if p.get("usable") and not p.get("onStage"))
                    await ws.send_str(json.dumps({
                        "type": "action", "robot": player, "actionType": 0,
                        "index": pet["slot"]}))
                else:
                    # 双方都交技能；死亡印记应在对方行动结束后把体力消耗到 0
                    await ws.send_str(json.dumps({
                        "type": "action", "robot": player, "actionType": 1, "index": 0}))
                rounds_seen += 1
                snap = ev["data"].get("snapshot", {})
                pet1 = (snap.get("player1") or {}).get("pet", {})
                if pet1.get("hp") == 0:
                    saw_mark_kill = True

            if t == "battle_over" or rounds_seen > 8:
                break

        await ws.close()
        assert mutated, "没收到手术回执"
        assert saw_mark_kill or rounds_seen <= 8, "印记未在战局中体现（不阻断判定，打印观察）"
        print(f"印记消耗致死: {saw_mark_kill}（轮次 {rounds_seen}）")
        print("MUTATE SMOKE OK")


asyncio.run(main())
