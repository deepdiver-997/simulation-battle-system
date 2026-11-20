"""词条编辑 A/B 端到端回归：同一阵容打两场——
A 档（effectParams 把圣光吟诵 936 {0} 概率 100→0）→ 对手**不**出现疲惫；
B 档（不传覆盖 = 库值 100%）→ 对手出现疲惫（被击败掩蔽时两档都无，判定不成立）。
跑法：web/.venv/bin/python web/gateway/effect_param_ab_smoke.py（需 sim_server+网关已起）。
打印 EFFECT-PARAM A/B SMOKE OK 即通过。"""
import asyncio
import json

import aiohttp

# 圣灵谱尼(5000) 带圣光吟诵(25678) vs 高血波克尔（base 体力 4000，扛住一击让异常分支可见）
SIDE1 = {"petId": 5000, "skills": [25678, 10993, 10994, 10995, 10996],
         "base": [1, 1, 0, 0, 0, 0], "traitId": 0}  # 攻/特攻压到 1，避免打死目标掩蔽异常
SIDE2 = {"petId": 12, "skills": [10001, 20002, 10008, 10009, 20015],
         "base": [0, 0, 0, 0, 0, 4000], "traitId": 0}
OVERRIDE = {"side1": [dict(SIDE1)], "side2": [dict(SIDE2)]}
OVERRIDE["side1"][0]["effectParams"] = [
    {"skill": 25678, "effect": 936, "index": 0, "value": 0}  # 疲惫概率 100% → 0%
]
NO_OVERRIDE = {"side1": [dict(SIDE1)], "side2": [dict(SIDE2)]}


async def run_battle(http, lineup):
    """打一局：我方出圣光吟诵，对方空操作。返回事件带文本。"""
    ws = await http.ws_connect("http://127.0.0.1:8070/ws/battle")
    tape = []
    try:
        await ws.send_str(json.dumps({"type": "init", "lineup": lineup}))
        submitted = False
        while True:
            msg = await asyncio.wait_for(ws.receive(), 8)
            if msg.type != aiohttp.WSMsgType.TEXT:
                break
            ev = json.loads(msg.data)
            t = ev["type"]
            if t == "error":
                raise RuntimeError(f"server error: {ev['data']}")
            if t == "tape":
                for s in ev["data"].get("samples", []):
                    tape.append(json.dumps(s, ensure_ascii=False))
            if t == "battle_over":
                break
            if t == "input_required":
                player = ev["data"]["player"]
                if player == 0 and not submitted:
                    # 找到圣光吟诵的下标（合法动作集按可用技能列出）
                    skills = ev["data"]["legal"]["skills"]
                    entry = next(s for s in skills if s.get("name") == "圣光吟诵")
                    await ws.send_str(json.dumps(
                        {"type": "action", "robot": 0, "actionType": 1, "index": entry["index"]}))
                    submitted = True
                elif player == 1:
                    await ws.send_str(json.dumps(
                        {"type": "action", "robot": 1, "actionType": 3, "index": 0}))  # 空操作
                elif player == 0 and submitted:
                    break
    finally:
        await ws.close()
    return "\n".join(tape)


async def main():
    async with aiohttp.ClientSession() as http:
        tape_a = await run_battle(http, OVERRIDE)
        tape_b = await run_battle(http, NO_OVERRIDE)

        def fatigue_applied(tape):
            # tape 是原始 JSON（异常事件里 amount=异常 id，前端才转中文名）——
            # 疲惫 = 异常 7：EVENT_ANOMALY_APPLIED 且 amount==7
            return any('"EVENT_ANOMALY_APPLIED"' in line and '"amount": 7' in line
                       for line in tape.splitlines())
        a_fatigue = fatigue_applied(tape_a)
        b_fatigue = fatigue_applied(tape_b)
        print(f"A 档(0%)  疲惫出现: {a_fatigue}")
        print(f"B 档(100%) 疲惫出现: {b_fatigue}")
        assert not a_fatigue, "0% 档不应出现疲惫——覆盖未生效！"
        assert b_fatigue, "100% 档没出现疲惫——判定不成立"
        print("EFFECT-PARAM A/B SMOKE OK")


asyncio.run(main())
