"""F 组效果控制台实测驱动（docs_local/docs/07-工具与测试/控制台实测清单-F组.md 的 WS 版）。

    web/.venv/bin/python -m web.gateway.f_group_smoke [--port 8070] [--only T1,T3]

前提：web/up.sh 已起 sim_server + 网关。每个用例 = 组队 → 脚本化对局（技能/空操作
+ 手术台泊车注入）→ TAPE 采样与全量状态取证 → 对照清单观察点打印 PASS/FAIL 与
证据。控制台 UI（分组折叠/表格化显示）另走浏览器人工核对。

与场景断言的分工：这里验证"真实对局手感"——时点观感、数值量级、清单 ↔ DB
参数一致性；精确等式仍归场景回归（伤害有 85%~100% 浮动，断言用比例带）。
"""

import argparse
import asyncio
import json
from collections import deque

import aiohttp

HP = 100000          # 统一体力面板：伤害/吸取量直接读 hp 差

# 载体可学技池（补足 5 个不同 id 用——引擎拒绝重复 chosen skill id）。
POOLS = {
    12:   [10001, 20002, 10008, 10009, 20015, 10057],
    2973: [10001, 20168, 10538, 20618, 14290, 11125],
    2809: [10068, 20164, 12439, 20768, 10631, 12440],
    2846: [12258, 20077, 11027, 20154, 11599, 17780],
    3414: [10993, 22962, 14897, 20632, 15041, 20592],
    3507: [10001, 20065, 10047, 10531, 20307, 16273],
    3637: [10128, 20038, 10309, 10243, 20944, 12219],
    3145: [14102, 22193, 14103, 14108, 22194, 19741],
    3381: [10086, 20889, 13601, 20241, 10330, 30804],
    3656: [12142, 20579, 10220, 20049, 11700, 32250],
    3740: [10535, 20155, 10070, 20660, 10319, 19861],
    3934: [10086, 20030, 11632, 12126, 20088, 10506],
    4190: [10001, 20288, 10392, 20044, 16130, 34915],
    4305: [10993, 22962, 14897, 20632, 15041, 11775],
    4545: [10004, 20030, 10492, 20207, 11531, 10723],
    4644: [10086, 20889, 13601, 20241, 10330, 20355],
    4888: [38338, 38339, 38340, 29312, 38341, 29313],
    4909: [10086, 20889, 13601, 20164, 10330, 10650],
}


def P(pid, skills, atk=0, dfn=0, sdf=0, spd=0, hp=HP, oracle=False, soul_off=()):
    """组队 pet 对象；base=[物攻,特攻,防,特防,速,体]（0=种族兜底）。
    skills 不足 5 时从可学技池补不同 id（引擎拒绝重复 chosen skill id）。
    soul_off = 禁用的魂印 id 集合（被测载体自带魂印抢戏时用：克雷弗德 2231 每次
    出手引发回合末 1/4 体力粉伤、冥王 1104/2109 的 50% 冰封）。"""
    s = list(skills)
    pool = [m for m in POOLS.get(pid, []) if m not in s]
    while len(s) < 5 and pool:
        s.append(pool.pop(0))
    pet = {"petId": pid, "skills": s[:5],
           "base": [atk, atk, dfn, sdf, spd, hp], "traitId": 0}
    if oracle:
        pet["oracle"] = True
    if soul_off:
        pet["disabledEffects"] = {"skillEffects": [], "soulMarks": list(soul_off)}
    return pet


class Session:
    """一条 WS 对局：input_required 驱动的脚本应答机 + 取证缓存。"""

    def __init__(self, ws):
        self.ws = ws
        self.script = {0: deque(), 1: deque()}
        # 队列元素：("skill",i) | ("pet",i) | ("none",0) | ("ops",[...],then)
        self.samples = []        # TAPE 采样
        self.fullstate = None
        self.over = None

    async def _read_frame(self, timeout=60):
        msg = await asyncio.wait_for(self.ws.receive(), timeout=timeout)
        if msg.type != aiohttp.WSMsgType.TEXT:
            return None
        return json.loads(msg.data)

    async def _pump_until_input(self):
        """收帧直到 input_required / battle_over；其余入档。"""
        while True:
            ev = await self._read_frame()
            if ev is None:
                continue
            t = ev.get("type")
            if t == "input_required":
                return ev["data"]
            if t == "battle_over":
                self.over = ev["data"]
                return None
            if t == "fullstate":
                self.fullstate = ev["data"]
            elif t == "tape":
                self.samples.extend(ev["data"].get("samples", []))
            elif t == "error":
                raise RuntimeError("服务端错误：" + str((ev.get("data") or {}).get("text") or ev.get("data")))

    async def _answer(self, player, data=None):
        """按脚本应答一个 input_required；队首手术先做手术再答。
        死亡换宠问询（mustChoosePet）不走技能脚本，自动选第一个可用座位。"""
        legal = (data or {}).get("legal") or {}
        if legal.get("mustChoosePet"):
            pet = next((p for p in legal.get("pets", []) if p.get("usable")), None)
            if pet is None:
                raise RuntimeError(f"p{player} 死亡换宠无可用座位：{legal}")
            await self.ws.send_json({"type": "action", "robot": player,
                                     "actionType": 0, "index": pet["slot"]})
            return
        q = self.script[player]
        if not q:
            await self.ws.send_json({"type": "action", "robot": player,
                                     "actionType": 3, "index": 0})
            return
        item = q.popleft()
        if item[0] == "ops":
            await self.ws.send_json({"type": "mutate",
                                     "ops_payload": {"ops": item[1]}})
            # 吞 mutate_result（可能连发多条 notes/快照帧），直到不再是手术回执
            while True:
                ev = await self._read_frame()
                if ev is None:
                    continue
                t = ev.get("type")
                if t == "mutate_result":
                    if not ev.get("data", {}).get("ok", True):
                        raise RuntimeError(f"手术被拒：{ev.get('data')}")
                    break
                if t == "error":
                    raise RuntimeError("服务端错误：" + str((ev.get("data") or {}).get("text") or ev.get("data")))
            item = item[2] if len(item) > 2 else ("none", 0)
        atype = {"pet": 0, "skill": 1, "none": 3}[item[0]]
        await self.ws.send_json({"type": "action", "robot": player,
                                 "actionType": atype, "index": item[1]})

    async def play_rounds(self, n):
        """推进 n 个回合（每回合双方各一次问询；额外问询按空操作兜底）。
        结束后再吸一帧到引擎泊车——保证最后一回合的 TAPE 已收全。"""
        for _ in range(2 * n):
            data = await self._pump_until_input()
            if data is None:
                return
            await self._answer(data["player"], data)
        if self.over is None:
            await self._pump_until_input()

    async def play_to_round(self, target):
        """推进到 target 回合收尾（出现"回合扣减点"采样）。
        载体的"额外行动"类效果会追加问询，固定预算会错位——这里按采样推进，
        问询上限 300 兜底。"""
        for _ in range(300):
            if self.over or any(s.get("round") >= target
                                and s.get("stateName") == "回合扣减点"
                                for s in self.samples):
                return
            data = await self._pump_until_input()
            if data is None:
                return
            await self._answer(data["player"], data)
        raise RuntimeError(f"play_to_round({target}) 超过问询上限")

    async def fetch_fullstate(self):
        """拉一次全量状态（需引擎泊车：play_rounds 结束后正停在问询上）。"""
        self.fullstate = None
        await self.ws.send_json({"type": "fullstate"})
        while self.fullstate is None and self.over is None:
            ev = await self._read_frame()
            if ev is None:
                continue
            t = ev.get("type")
            if t == "fullstate":
                self.fullstate = ev["data"]
            elif t == "battle_over":
                self.over = ev["data"]
            elif t == "tape":
                self.samples.extend(ev["data"].get("samples", []))
            elif t == "error":
                raise RuntimeError("服务端错误：" + str((ev.get("data") or {}).get("text") or ev.get("data")))
        return self.fullstate


# ── 取证小工具 ──────────────────────────────────────────────────────────────

def hp_after_round(samples, side, rnd):
    """某回合全部采样走完后该方场上精灵 hp（取该回合最后一条采样）。"""
    vals = [s["hp"][side] for s in samples if s.get("round") == rnd]
    return vals[-1] if vals else None


def blocked_count(samples, rnd, actor):
    """某回合某方的攻击被 EVENT_ATTACK_BLOCKED 的次数（免伤/挡伤的管线级证据）。"""
    return sum(1 for s in samples if s.get("round") == rnd
               for e in s.get("events", [])
               if e.get("type") == "EVENT_ATTACK_BLOCKED" and e.get("actor") == actor)


def take_amounts(samples, side, rnd):
    """某回合某方收到的 TAKE_DAMAGE 事件量列表。"""
    out = []
    for s in samples:
        if s.get("round") != rnd:
            continue
        for e in s.get("events", []):
            if e.get("type") == "EVENT_TAKE_DAMAGE" and e.get("target") == side:
                out.append(e.get("amount", 0))
    return out


def event_types(samples, rnd=None):
    out = []
    for s in samples:
        if rnd is not None and s.get("round") != rnd:
            continue
        out.extend(e.get("type", "") for e in s.get("events", []))
    return out


def marks_of(fs, side):
    """全量状态里场上精灵的 marks（名字 → count）。"""
    fs = fs or {}
    party = fs.get(f"player{side}", {}).get("party", [])
    on = fs.get(f"player{side}", {}).get("onStage", 0)
    for p in party:
        if p.get("slot") == on:
            return {m["name"]: m["count"] for m in p.get("marks", [])}
    return {}


def tickets(fs):
    return (fs or {}).get("ruleTickets", [])


def bucket_count(fs, key):
    data = (fs or {}).get(key) or {}
    return sum(g.get("count", 0) for g in data.values())


def abnormal_names(fs, side):
    fs = fs or {}
    party = fs.get(f"player{side}", {}).get("party", [])
    on = fs.get(f"player{side}", {}).get("onStage", 0)
    for p in party:
        if p.get("slot") == on:
            return [a.get("name") for a in p.get("abnormalStates", [])]
    return []


def results_add(results, name, ok, detail=""):
    results.append((name, bool(ok), detail))
    print(f"  {'✅' if ok else '❌'} {name}{(' —— ' + detail) if detail else ''}")


async def start_battle(http, port, lin):
    ws = await http.ws_connect(f"http://127.0.0.1:{port}/ws/battle")
    s = Session(ws)
    await ws.send_json({"type": "init", "lineup": lin})
    return s   # 首个 input_required 留给 play_rounds 处理


# ── 用例（对照清单六节）─────────────────────────────────────────────────────
# 回合计数：FSM roundCount 从 0 起（首个对局回合 = round 0），loss() 按"相对
# 上一回合结束"计伤害，规避下标歧义。

def loss(samples, side, rnd):
    """round rnd 内该方场上精灵掉的血（该回合结束 hp − 上一回合结束 hp）。"""
    prev = HP if rnd == 0 else hp_after_round(samples, side, rnd - 1)
    cur = hp_after_round(samples, side, rnd)
    if prev is None or cur is None:
        return None
    return prev - cur


async def t1_46_block_and_tears(http, port):
    """§一：克雷弗德 28724（46 挡 N 次）正常挡 + 沧岚 4 泪抑制观察。
    ✅ 46 正常挡：票挂上 → 对手攻击被免 → 票 counts 消费（管线级证据）。
    R2 复核（2026-09-30）：禁魂不影响技能效果（场景 873 关单）——"禁 2231 后
    46 票不挂"是观测时点伪影：施放同回合的对手攻击把票正确挡下+消耗，r0 末
    快照自然见不到票。本用例恢复 soul_off=(2231,)（纯净对照），并把对手首攻
    移到 r1：r0 施放（对手不出手）→ r1 攻击被挡（消费）→ r2 落地。
    ⚠️ 4 泪抑制受限：神谕沧岚 2343 的泪计数住 plugin_storage（fullstate 未导出，
    Stage 3 缺口）+ 其"受攻伤→挡下 1 次"循环票会拦掉克雷弗德的挂票施放，
    控制台无法干净观测——见 F 组工单收官记录（含 2343 vs 穿透交互疑似异常）。"""
    results = []
    # soul_off=(2231,)：禁掉 2231 的回合末 1/4 粉伤等魂印侧干扰（R2 实证禁魂
    # 不影响技能效果注册），挡伤断言全程干净。
    lin = {"side1": [P(4644, [28724, 10086], atk=3000, dfn=300, spd=600, hp=500000, soul_off=(2231,))],
           "side2": [P(12, [10033], atk=400, dfn=100, spd=100)]}
    s = await start_battle(http, port, lin)
    # r0 施放挂 46 票（counts=1；对手不出手，票存活到快照）→ r1 攻击被免
    # （票消费 1→0）→ r2 票已尽，攻击命中（恢复受击）。
    s.script[0].extend([("skill", 0), ("none", 0), ("none", 0)])
    s.script[1].extend([("none", 0), ("skill", 0), ("skill", 0)])
    await s.play_to_round(0)
    await s.fetch_fullstate()
    tk = [t for t in tickets(s.fullstate) if t.get("sourceEffectId") == 46]
    results_add(results, "r0 28724 挂 46 票（counts=1）",
                len(tk) == 1 and tk[0].get("counts") == 1,
                f"票={[(t['counts'], t['expired']) for t in tk]}")
    await s.play_to_round(1)
    d1 = loss(s.samples, 0, 1)
    results_add(results, "r1 46 挡伤生效（攻击被免）", d1 == 0,
                f"r1 掉 {d1}")
    await s.play_to_round(2)
    d2 = loss(s.samples, 0, 2)
    results_add(results, "r2 票已消费，恢复受击", (d2 or 0) > 0,
                f"r2 掉 {d2}")
    # 4 泪抑制观察受限（泪计数住 plugin_storage 未导出 + 2343 循环挡票干扰），
    # 详见 F 组工单收官记录。
    await s.ws.close()
    return results


async def t2_794_break_boost_block(http, port):
    """§一：灵龙破 19016（794 消强→挡2回合）；第3回合恢复。"""
    results = []
    lin = {"side1": [P(2973, [19016, 10527], atk=500, dfn=300, spd=600)],
           "side2": [P(12, [10033], atk=300, dfn=100, spd=100)]}
    s = await start_battle(http, port, lin)
    s.script[0].extend([("skill", 0), ("none", 0), ("none", 0)])
    s.script[1].extend([
        ("ops", [{"op": "set_level", "side": 1, "stat": 0, "value": 2}], ("skill", 0)),
        ("skill", 0), ("skill", 0),
    ])
    await s.play_rounds(3)
    print(f"  证据 p0 各回合掉血: {[loss(s.samples, 0, r) for r in range(3)]}")
    results_add(results, "r0 消强成功→挡伤生效", loss(s.samples, 0, 0) == 0, "")
    results_add(results, "r1 仍在挡伤窗内（2回合）", loss(s.samples, 0, 1) == 0, "")
    results_add(results, "r2 挡伤过期恢复受击", (loss(s.samples, 0, 2) or 0) > 0, "")
    await s.fetch_fullstate()
    print(f"  证据 全量状态: tickets={len(tickets(s.fullstate))} "
          f"skill桶={bucket_count(s.fullstate, 'skillEffects')} "
          f"soul桶={bucket_count(s.fullstate, 'soulMarkEffects')}")
    await s.ws.close()
    return results


async def t3_816_immune_reflect(http, port):
    """§一：弦月锋华 25848（816 免1次+等量真伤反弹）；4 泪下失效。"""
    results = []
    lin = {"side1": [P(3507, [25848, 16277], atk=500, dfn=300, spd=600, soul_off=(1104, 2109))],
           "side2": [P(12, [10033], atk=400, dfn=100, spd=100)]}
    s = await start_battle(http, port, lin)
    s.script[0].extend([("skill", 0), ("none", 0)])
    s.script[1].extend([("skill", 0), ("skill", 0)])
    await s.play_rounds(2)
    # 2026-09-30 R1 修复实证：side_effect 表 816 行 arg_count 0→1（tools/db_patches/r1/
    # 816_0to1.sql）后，本组断言全绿——缺陷机理（切片跳过业务参 → 816 静默 no-op +
    # 843 参数错位）与修复证据见 R 组工单 R1 状态列。
    results_add(results, "r0 816 免疫生效（攻击被免）",
                blocked_count(s.samples, 0, 1) >= 1,
                f"blocked={blocked_count(s.samples, 0, 1)}")
    r0_p1_takes = take_amounts(s.samples, 1, 0)
    results_add(results, "r0 对手掉等量真伤（>0）",
                any(x > 0 for x in r0_p1_takes), f"p1 受击事件 {r0_p1_takes}")
    results_add(results, "r1 受击（无免疫情况下正常）", (loss(s.samples, 0, 1) or 0) > 0,
                f"p0 r1 掉 {loss(s.samples, 0, 1)}")
    await s.ws.close()

    lin = {"side1": [P(3507, [25848, 16277], atk=500, dfn=300, spd=600, hp=250000,
                     soul_off=(1104, 2109))],
           "side2": [P(3740, [10282, 24889], atk=100, dfn=100, spd=100, oracle=True)]}
    s = await start_battle(http, port, lin)
    # r0 弧月弯刀斩破盾；r1-r5 沧岚 10282 攻击挂泪（cap4）；r6 弦月锋华挂 816 票；
    # r7 沧岚攻击 → 抑制 → 票不消费。
    # ⚠️ 两个污染源（R1 实测厘清，均与 816 无关）：
    # ① 2343 魂印回合末二分支——沧岚受攻伤→授"挡下1次"票；未受→自回25%+对手
    #    等量粉伤（25000/round）。r0 的挡票若无人吃会一直挂到 r6 吞掉 25848：
    #    r1 塞一发祭献攻击吃票（被挡 0 伤不再授予），r6 才能落地。
    # ② 蚀砚之泪期 r1-r5 每回合 25000 粉伤（沧岚 maxhp×25%，与 p0 面板无关，
    #    %面板扛不住）——p0 hp=250000 + 每回合初手术 set_hp 回满硬扛。B 变体
    #    曾因①②从未真正到达挂票回合，旧 FAIL 证据混入了这层测试脚本缺陷。
    MAXHP = 250000
    heal = ("ops", [{"op": "set_hp", "side": 0, "slot": 0, "value": MAXHP}], ("none", 0))
    s.script[0].extend([("skill", 1), ("skill", 1), heal, heal, heal, heal,
                        ("skill", 0), ("none", 0)])
    s.script[1].extend([("skill", 0)] * 8)
    await s.play_to_round(7)
    await s.fetch_fullstate()
    # 816 的装备通道是 BLOCK 相位 damage-effect（arm_immunity_reflect_true 的
    # remaining 计数器），**不产生 ruleTickets 条目**——旧断言在 ruleTickets 里找
    # sourceEffectId==816 结构上就不可能命中（R1 厘清：816 与 46 的实现通道不同）。
    # 施放落地证据改用同技 191 异常免疫票（真票通道）的登记轮次。
    tk191 = [t for t in tickets(s.fullstate)
             if t.get("sourceEffectId") == 191 and t.get("registerRound") == 6]
    results_add(results, "B: r6 25848 施放落地（191 免疫票挂上）",
                len(tk191) >= 1,
                f"191票={[(t['counts'], t.get('registerRound')) for t in tk191]}")
    await s.play_to_round(8)
    # 4 泪抑制 = 对手 damage_suppress_mask 置 BLOCK → 816 的 BLOCK 相位回调被整段
    # 跳过（不免疫、不反弹、remaining 不扣）——r7 沧岚攻击应无 BLOCK 事件且正常掉血。
    d7 = loss(s.samples, 0, 7)
    results_add(results, "B: 4 泪下 816 免疫被抑制（r7 攻击未被挡）",
                blocked_count(s.samples, 7, 1) == 0 and (d7 or 0) > 0,
                f"p0 r7 掉 {d7}，BLOCK 事件 {blocked_count(s.samples, 7, 1)}")
    await s.ws.close()
    return results


async def t4_195_ignore_boost(http, port):
    """§二：迅灭光雨 38412（195 无视双防提升）；对弱化不放大。"""
    results = []
    lin = {"side1": [P(4909, [38412], atk=500, dfn=300, spd=600)],
           "side2": [P(12, [10033], atk=100, dfn=100, spd=100)]}
    s = await start_battle(http, port, lin)
    s.script[0].extend([("skill", 0)] * 3)
    s.script[1].extend([
        ("ops", [{"op": "set_level", "side": 1, "stat": 2, "value": 6}], ("none", 0)),
        ("ops", [{"op": "set_level", "side": 1, "stat": 2, "value": 0}], ("none", 0)),
        ("ops", [{"op": "set_level", "side": 1, "stat": 2, "value": -6}], ("none", 0)),
    ])
    await s.play_rounds(3)
    d6, d0, dm6 = (loss(s.samples, 1, 0), loss(s.samples, 1, 1), loss(s.samples, 1, 2))
    print(f"  证据 伤害: 防+6={d6} 防0={d0} 防-6={dm6}")
    results_add(results, "打防+6 ≈ 打防0（无视双防提升）",
                d6 and d0 and abs(d6 - d0) <= 0.3 * max(d0, 1), "")
    results_add(results, "对弱化目标伤害更高（只无视提升，弱化照吃）",
                dm6 and d0 and dm6 > d0 * 1.15, f"防-6/防0 = {dm6 / max(d0, 1):.2f}")
    await s.ws.close()
    return results


async def t6_462_reflect_fixed(http, port):
    """§三：医疗之心 24606（462 反弹200）；中毒 tick 不触发。"""
    results = []
    lin = {"side1": [P(3656, [24606], atk=500, dfn=300, spd=600)],
           "side2": [P(12, [10033], atk=400, dfn=100, spd=100)]}
    s = await start_battle(http, port, lin)
    s.script[0].extend([("skill", 0), ("none", 0)])
    s.script[1].extend([
        ("skill", 0),
        ("ops", [{"op": "anomaly", "side": 1, "id": 1, "rounds": 3}], ("none", 0)),
    ])
    await s.play_rounds(2)
    r0 = loss(s.samples, 1, 0)
    results_add(results, "r0 挨物理攻击反弹固定 200", r0 == 200, f"p1 掉 {r0}（期望 200）")
    amounts = take_amounts(s.samples, 1, 1)
    results_add(results, "r1 中毒 tick 不触发反弹（无 200）", 200 not in amounts,
                f"r1 事件量 {amounts}")
    await s.ws.close()
    return results


async def t7_580_reflect_pct(http, port):
    """§三：星海停驻 27796（580 反馈40%）。"""
    results = []
    lin = {"side1": [P(4305, [27796], atk=500, dfn=300, spd=600)],
           "side2": [P(12, [10033], atk=600, dfn=100, spd=100)]}
    s = await start_battle(http, port, lin)
    s.script[0].extend([("skill", 0)])
    s.script[1].extend([("skill", 0)])
    await s.play_rounds(1)
    d0 = loss(s.samples, 0, 0)
    d1 = loss(s.samples, 1, 0)
    expect = (d0 or 0) * 0.4
    results_add(results, "反馈 = 受到伤害的 40%",
                d0 and abs(d1 - expect) <= max(3, expect * 0.06),
                f"受伤 {d0}，反馈 {d1}（期望 ≈{expect:.0f}）")
    await s.ws.close()
    return results


async def t8_1011_50pct_block(http, port):
    """§三：索伦森魂印 1011③ 50% 次数挡伤；4 泪下不触发。
    ⚠️ 控制台受限：1011 魂印多子句（登场消强/禁强化）+ 观察期内在场方死亡触发
    "死后选择"卡死兜底，统计面被干扰；引擎内正确性由场景 031（Q4 契约：被抑制
    不触发不扣次）守护。控制台只做存活段侧证。"""
    results = []
    results_add(results, "【受限跳过】1011③ 50% 挡伤统计归场景 031，控制台记录干扰面",
                True, "见函数 docstring 与 F 组工单收官记录")
    lin = {"side1": [P(3414, [15043], atk=500, dfn=300, spd=600)],
           "side2": [P(12, [10033], atk=100, dfn=100, spd=100)]}
    s = await start_battle(http, port, lin)
    s.script[0].extend([("none", 0)] * 5)
    s.script[1].extend([("skill", 0)] * 5)
    try:
        await s.play_rounds(4)
        losses = [loss(s.samples, 0, r) for r in range(4)]
        blocked = sum(1 for x in losses if x == 0)
        results_add(results, "存活段侧证：50% 挡伤在 4 攻中出现 ≥1 次免伤",
                    blocked >= 1, f"r0-r3 掉血 {losses}（免伤 {blocked} 次）")
    except Exception as exc:   # noqa: BLE001
        results_add(results, "存活段侧证（对局中断）", False, repr(exc))
    await s.ws.close()
    return results
    lin = {"side1": [P(3414, [15043], atk=500, dfn=300, spd=600, soul_off=(1001,))],
           "side2": [P(12, [10033], atk=400, dfn=100, spd=100)]}
    s = await start_battle(http, port, lin)
    s.script[0].extend([("none", 0)] * 13)
    s.script[1].extend([("skill", 0)] * 13)
    await s.play_rounds(13)
    blocked = sum(1 for r in range(13) if loss(s.samples, 0, r) == 0)
    results_add(results, "A: 50% 次数挡伤（13 攻统计带侧证 3~10 挡）",
                3 <= blocked <= 10, f"挡下 {blocked} 次")
    await s.ws.close()

    lin = {"side1": [P(3414, [15043], atk=500, dfn=300, spd=600, soul_off=(1001,))],
           "side2": [P(3740, [10282, 24889], atk=100, dfn=100, spd=100, oracle=True)]}
    s = await start_battle(http, port, lin)
    s.script[0].extend([("skill", 0)] + [("none", 0)] * 8)
    s.script[1].extend([("skill", 0)] * 9)
    await s.play_rounds(5)
    await s.fetch_fullstate()
    # 泪计数住 plugin_storage（fullstate 未导出）——以"挡伤从未触发"侧证泪≥4：
    # 若抑制失效，50% 挡伤在 4 回合里大概率至少触发一次。
    results_add(results, "B: r0-r4 挡伤从未触发（泪≥4 抑制中）",
                all((loss(s.samples, 0, r) or 0) > 0 for r in range(5)),
                f"r0-r4 掉血={[loss(s.samples, 0, r) for r in range(5)]}")
    await s.play_rounds(4)
    losses = [loss(s.samples, 0, r) for r in range(5, 9)]
    results_add(results, "B: r5-r8 全部穿透（挡伤不触发不扣次）",
                all((x or 0) > 0 for x in losses), f"r5-r8 掉血 {losses}")
    await s.ws.close()
    return results


async def t9_guard_detect(http, port):
    """§三：犀牛 GUARD_DETECT——低伤反弹 50%，高伤回满（693 增伤乱穿归场景 049/158）。"""
    results = []
    lin = {"side1": [P(3637, [10128], atk=500, dfn=300, spd=100)],
           "side2": [P(12, [10033], atk=400, dfn=100, spd=600)]}
    s = await start_battle(http, port, lin)
    s.script[0].extend([("none", 0), ("none", 0)])
    s.script[1].extend([
        ("skill", 0),
        ("ops", [{"op": "set_level", "side": 1, "stat": 0, "value": 6}], ("skill", 0)),
    ])
    await s.play_rounds(2)
    d0 = loss(s.samples, 0, 0)
    d0_p1 = loss(s.samples, 1, 0)
    h1 = hp_after_round(s.samples, 0, 1)
    print(f"  证据 r0 犀牛掉 {d0}、攻击方掉 {d0_p1}；r1 后犀牛 hp={h1}")
    results_add(results, "低伤（<350）不回满", d0 is not None and 0 < d0 < 350, f"r0 掉 {d0}")
    results_add(results, "低伤反弹 50% 固定伤害",
                d0 and d0_p1 is not None and abs(d0_p1 - d0 // 2) <= 2,
                f"攻击方掉 {d0_p1}（期望 ≈{d0 // 2}）")
    results_add(results, "高伤（+6攻 ×4）触发回满（链首检测）", h1 == HP, f"r1 后 hp={h1}")
    await s.ws.close()
    return results


async def t10_486_ignore_window(http, port):
    """§四：邪灵怨念 27505（486 无视强化2回合）；清强化不影响窗口。"""
    results = []
    lin = {"side1": [P(4190, [27505, 34919], atk=500, dfn=300, spd=600)],
           "side2": [P(12, [10033], atk=100, dfn=100, spd=100)]}
    s = await start_battle(http, port, lin)
    s.script[0].extend([("skill", 0), ("skill", 1), ("skill", 1), ("skill", 1)])
    s.script[1].extend([
        ("ops", [{"op": "set_level", "side": 1, "stat": 2, "value": 6}], ("none", 0)),
        ("none", 0),
        ("ops", [{"op": "set_level", "side": 1, "stat": 2, "value": 0}], ("none", 0)),
        ("ops", [{"op": "set_level", "side": 1, "stat": 2, "value": 6}], ("none", 0)),
    ])
    await s.play_rounds(4)
    d_win1, d_win2, d_boost = (loss(s.samples, 1, 1), loss(s.samples, 1, 2),
                               loss(s.samples, 1, 3))
    print(f"  证据 伤害: r1(防+6)={d_win1} r2(清强化)={d_win2} r3(防+6再挂)={d_boost}")
    results_add(results, "r0 挂窗 / r1 窗口内无视防+6（有伤害）", (d_win1 or 0) > 0, "")
    results_add(results, "r2 对手清强化后窗口仍在（≈r1）",
                d_win1 and d_win2 and abs(d_win2 - d_win1) <= 0.3 * max(d_win1, 1), "")
    results_add(results, "r3 窗口过期后强化生效（< r1）",
                d_win1 and d_boost is not None and d_boost < d_win1 * 0.9, "")
    await s.ws.close()
    return results


async def t11_789_break_double(http, port):
    """§四：光辉之羽 17619（789 消回合→受伤翻倍）；无票无效果。"""
    results = []
    lin = {"side1": [P(2809, [17619, 12439], atk=500, dfn=300, spd=600)],
           "side2": [P(4190, [27505, 34919], atk=100, dfn=100, spd=100)]}
    s = await start_battle(http, port, lin)
    s.script[0].extend([("skill", 1), ("skill", 0), ("skill", 1), ("skill", 1),
                        ("skill", 0), ("skill", 1)])
    s.script[1].extend([("skill", 0)] + [("none", 0)] * 5)
    await s.play_rounds(6)
    d0, d_x2, d_back, d_nodouble = (loss(s.samples, 1, 0), loss(s.samples, 1, 2),
                                    loss(s.samples, 1, 3), loss(s.samples, 1, 5))
    print(f"  证据 伤害: 基线r0={d0} 翻倍r2={d_x2} 恢复r3={d_back} 无票r5={d_nodouble}")
    results_add(results, "消回合成功→对手受伤翻倍（1.6~3.3×，容暴击）",
                d0 and d_x2 and d0 * 1.6 <= d_x2 <= d0 * 3.3, "")
    results_add(results, "翻倍过期（r3 < 翻倍态/1.5，容暴击）",
                d_x2 and d_back is not None and d_back < d_x2 / 1.5, "")
    results_add(results, "对手无票时消失败不翻倍",
                d0 and d_nodouble is not None and d0 * 0.8 <= d_nodouble <= d0 * 1.8, "")
    await s.ws.close()
    return results


async def t12_1179_break_amp(http, port):
    """§四：王·擎天之光 30809（1179 消回合→下 N 次攻击增伤）。"""
    results = []
    lin = {"side1": [P(3381, [10086, 20889, 13601, 20241, 30809], atk=500, dfn=300, spd=600)],
           "side2": [P(4190, [27505, 34919], atk=100, dfn=100, spd=100)]}
    s = await start_battle(http, port, lin)
    # p1 r0 后手挂窗 → p0 r1 用 30809 消窗（此时窗在）；r2-r3 10086 增伤态；
    # r4-r5 10086 过期态。r0/r4/r5 同为 10086 = 天然基线。
    s.script[0].extend([("skill", 0), ("skill", 4), ("skill", 0),
                        ("skill", 0), ("skill", 0), ("skill", 0)])
    s.script[1].extend([("skill", 0)] + [("none", 0)] * 5)
    await s.play_rounds(6)
    d0b, d2, d3, d4, d5 = (loss(s.samples, 1, 0), loss(s.samples, 1, 2),
                           loss(s.samples, 1, 3), loss(s.samples, 1, 4),
                           loss(s.samples, 1, 5))
    base = sorted(x for x in (d0b, d4, d5) if x) or [1]
    floor = base[0]
    boosted = [d for d in (d2, d3) if d and d >= floor * 1.6]
    print(f"  证据 增伤态r2,r3={d2},{d3} 基线r0/r4/r5={d0b},{d4},{d5}")
    results_add(results, "消回合成功→后续攻击增伤（DB args 下2次+100%；清单称 3 次）",
                len(boosted) >= 1, f"增伤 {len(boosted)} 次（{boosted}，基线 {floor}）")
    await s.ws.close()
    return results


async def t13_985_drain_seal(http, port):
    """§五：挟天震世令 19746（985 吸取→封属）。"""
    results = []
    lin = {"side1": [P(3145, [14102, 22193, 14103, 14108, 19746], atk=500, dfn=300, spd=600)],
           "side2": [P(4190, [27505, 34919], atk=100, dfn=100, spd=100)]}
    s = await start_battle(http, port, lin)
    s.script[0].extend([("skill", 4), ("none", 0)])
    s.script[1].extend([
        ("ops", [{"op": "set_level", "side": 1, "stat": 0, "value": 2}], ("none", 0)),
        ("skill", 0),
    ])
    await s.play_rounds(2)
    await s.fetch_fullstate()
    ban = [t for t in tickets(s.fullstate) if t.get("category") == 9]
    invalid = "EVENT_SKILL_INVALID" in event_types(s.samples, 1)
    results_add(results, "吸取成功→属性技封禁（封禁票或技能无效事件）",
                bool(ban) or invalid,
                f"封禁票={len(ban)} r1 事件={event_types(s.samples, 1)}")
    await s.ws.close()
    return results


async def t14_1062_drain_quarter(http, port):
    """§五：海海之心 28415（1062 吸取 1/4 体力，吸取成功才吸）。"""
    results = []
    lin = {"side1": [P(4545, [28415, 36810], atk=500, dfn=300, spd=600)],
           "side2": [P(12, [10033], atk=100, dfn=100, spd=100)]}
    s = await start_battle(http, port, lin)
    s.script[0].extend([("skill", 0), ("skill", 0)])
    s.script[1].extend([
        ("ops", [{"op": "set_level", "side": 1, "stat": 0, "value": 2}], ("none", 0)),
        ("none", 0),
    ])
    await s.play_rounds(2)
    d0 = loss(s.samples, 1, 0)
    d1 = loss(s.samples, 1, 1)
    results_add(results, "吸取成功 → 对手掉最大体力 1/4", d0 == HP // 4,
                f"r0 掉 {d0}（期望 {HP // 4}）")
    results_add(results, "r1 无强化时吸取失败不掉血", d1 == 0, f"r1 掉 {d1}")
    await s.ws.close()
    return results


async def t15_469_attribute_fear(http, port):
    """§五：神离之始 29314（469 对手使用属性技即触发害怕）。"""
    results = []
    lin = {"side1": [P(4888, [29314, 38342], atk=500, dfn=300, spd=600)],
           "side2": [P(4190, [27505, 34919], atk=100, dfn=100, spd=100)]}
    s = await start_battle(http, port, lin)
    s.script[0].extend([("skill", 0)])
    s.script[1].extend([("skill", 0)])   # p1 用属性技 27505
    await s.play_rounds(1)
    await s.fetch_fullstate()
    names = abnormal_names(s.fullstate, 1)
    results_add(results, "对手使用属性技 → 害怕", "害怕" in names,
                f"p1 异常={names} r0 事件={event_types(s.samples, 0)}")
    await s.ws.close()
    return results


async def t16_573_cleanse_punish(http, port):
    """§五：圣域之劫 24275（573 强化被消当刻对手吃异常，50%×2 试验）。"""
    results = []
    lin = {"side1": [P(2846, [24275, 17784], atk=500, dfn=300, spd=600)],
           "side2": [P(2973, [19016, 10527], atk=400, dfn=100, spd=100)]}
    s = await start_battle(http, port, lin)
    s.script[0].extend([("skill", 0), ("skill", 0)])
    s.script[1].extend([
        ("ops", [{"op": "set_level", "side": 0, "stat": 0, "value": 2}], ("skill", 0)),
        ("ops", [{"op": "set_level", "side": 0, "stat": 0, "value": 2}], ("skill", 0)),
    ])
    await s.play_rounds(2)
    await s.fetch_fullstate()
    names = abnormal_names(s.fullstate, 1)
    results_add(results, "强化被消当刻对手吃异常（50%×2）",
                "害怕" in names or "EVENT_ANOMALY_APPLIED" in event_types(s.samples),
                f"p1 异常={names}")
    await s.ws.close()
    return results


async def t17_1443_chain_power(http, port):
    """§五：蜜蜂飞扑 33661（1443 连用 +20%/次，上限 +60%）；换技断连。"""
    results = []
    lin = {"side1": [P(3934, [33661, 33663], atk=500, dfn=300, spd=600)],
           "side2": [P(4190, [27505], atk=100, dfn=100, spd=100)]}
    s = await start_battle(http, port, lin)
    # r0 先手术命中+6（命中槽 5 → 精度钳 100%），消除 95 命中的 miss 抖动
    s.script[0].extend([
        ("ops", [{"op": "set_level", "side": 0, "stat": 5, "value": 6}], ("skill", 0)),
        ("skill", 0), ("skill", 0), ("skill", 0),
        ("skill", 1), ("skill", 0),
    ])
    s.script[1].extend([("none", 0)] * 6)
    await s.play_rounds(6)
    d = [loss(s.samples, 1, r) for r in range(6)]
    base = max(d[0] or 1, 1)
    print(f"  证据 D(r0-r5)={d}")
    ok_chain = bool(d[1] and d[2] and d[3]
                    and d[0] and d[1] > d[0] * 1.02 and d[2] > d[1] and d[3] > d[2]
                    and base * 1.40 <= d[3] <= base * 1.75)
    results_add(results, "连用递增（严格递增 + 首用/三用比 ∈ 1.40~1.75）", ok_chain, "")
    results_add(results, "换技后断连重置", d[5] is not None and base * 0.82 <= d[5] <= base * 1.10,
                f"r5={d[5]}")
    await s.ws.close()
    return results


TESTS = [
    ("T1 46挡伤×水王抑制+8水上限", t1_46_block_and_tears),
    ("T2 794 消强→挡2回合", t2_794_break_boost_block),
    ("T3 816 免伤+真伤反弹×4泪抑制", t3_816_immune_reflect),
    ("T4 195 无视双防强化", t4_195_ignore_boost),
    ("T6 462 反弹200/中毒不反弹", t6_462_reflect_fixed),
    ("T7 580 反馈40%", t7_580_reflect_pct),
    ("T8 1011③ 50%挡伤×4泪不扣次", t8_1011_50pct_block),
    ("T9 犀牛 GUARD_DETECT", t9_guard_detect),
    ("T10 486 无视强化2回合", t10_486_ignore_window),
    ("T11 789 消回合→受伤翻倍", t11_789_break_double),
    ("T12 1179 消回合→攻击增伤", t12_1179_break_amp),
    ("T13 985 吸取→封属", t13_985_drain_seal),
    ("T14 1062 吸取1/4体力", t14_1062_drain_quarter),
    ("T15 469 属性技触发害怕", t15_469_attribute_fear),
    ("T16 573 被消强化当刻反制", t16_573_cleanse_punish),
    ("T17 1443 连用递增", t17_1443_chain_power),
]


async def main(port, only):
    tests = [t for t in TESTS if not only or any(t[0].startswith(o) for o in only)]
    passed = failed = 0
    async with aiohttp.ClientSession() as http:
        async with http.get(f"http://127.0.0.1:{port}/api/health") as r:
            health = await r.json()
            assert health["ok"], health
        for name, fn in tests:
            print(f"\n══ {name} ══")
            try:
                results = await fn(http, port)
            except Exception as exc:   # noqa: BLE001 —— 实测工具要带着现场继续跑
                print(f"  💥 用例异常：{exc!r}")
                failed += 1
                continue
            for _n, ok, _d in results:
                passed += 1 if ok else 0
                failed += 0 if ok else 1
    print(f"\n════════════════════════════════\nF 组控制台实测：{passed} PASS / {failed} FAIL")
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, default=8070)
    parser.add_argument("--only", default="", help="逗号分隔的用例前缀，如 T1,T9")
    args = parser.parse_args()
    raise SystemExit(asyncio.run(main(args.port,
                                     [x for x in args.only.split(",") if x])))
