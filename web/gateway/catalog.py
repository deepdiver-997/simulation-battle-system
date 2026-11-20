"""目录与覆盖率数据层：SQLite（权威数据）+ 4399 爬取 jsonl（展示优先）+ 注册表导出（徽标）。

展示口径（用户 2026-09-22 定）：文本展示**4399 图鉴优先**，数据库文本只做回退
（4399 没有的神谕/新魂印之类）。“未实现”徽标 = 数据引用的 effect id 对
sim_server --dump-registry 导出的已注册 id 集合做差集；
custom_effect_programs 里的离线程序算已实现，overrides 的 ignore/dead_column 算无需实现。

所有查询方法是同步的（sqlite3），由 app.py 包 asyncio.to_thread 调用；
共用一把锁串行化，读多写无，量级都在毫秒级。
"""

import json
import logging
import os
import sqlite3
import threading

log = logging.getLogger("catalog")

CATEGORY_NAMES = {1: "物理", 2: "特殊", 3: "属性"}

# 徽标状态：ok=全部实现 / partial=部分实现 / none=完全未实现 / plain=无效果(纯威力)
# / ignored=官方死列无需实现（由 overrides 标注）
STATUS_LABEL = {
    "ok": "已实现",
    "partial": "部分未实现",
    "none": "未实现",
    "plain": "无特殊效果",
    "ignored": "无需实现",
}

# 通用特性已实现名单（引擎 include/effects/trait_state.h 的 trait_kind_from_name 同源镜像；
# ⚠️ 引擎侧增删特性时同步这里）。特性不在 moves/soul/suit 注册表里——按 effect_id 查
# 注册表会永远"未实现"（2026-09-27 修：DS-001 线排查时误导），故按**名字**判定。
IMPLEMENTED_TRAIT_NAMES = frozenset({
    "瞬杀", "坚硬", "精神", "强袭", "强攻", "强念",
    "静电", "颤栗", "战栗", "火热", "极寒",          # 主动毒（66/67 家族）
    "带电", "高热", "冰冷", "阴森",                  # 被动毒（6 家族）
    "叶绿", "流水", "炎火", "飞空", "蓄电", "机能", "碎裂", "平衡",
    "冰霜", "魔幻", "战意", "光环", "黑夜", "奇异", "威严",  # 单属性增伤 15
    "吸收",
    "反抗", "反驳", "忽略", "草率", "慌张",          # 被动属性降低
    "反击", "抵抗", "反攻", "坚韧", "借风",          # 被动属性提升
    "顽强", "回神", "精准", "回避", "会心", "免爆", "虚无",
})


def _parse_id_array(text):
    """'[5, 15]' → [5, 15]；'[]'/空/坏值 → []。"""
    if not text:
        return []
    try:
        data = json.loads(text)
    except (ValueError, TypeError):
        return []
    if not isinstance(data, list):
        return []
    return [int(x) for x in data if isinstance(x, (int, float))]


class Catalog:
    def __init__(self, db_path, pets_jsonl, hunyin_jsonl, registry_path):
        self.db_path = db_path
        self.pets_jsonl = pets_jsonl
        self.hunyin_jsonl = hunyin_jsonl
        self.registry_path = registry_path
        self._lock = threading.Lock()
        self._db = sqlite3.connect(f"file:{db_path}?mode=ro", uri=True,
                                   check_same_thread=False)
        self._db.row_factory = sqlite3.Row

        self.type_names = {}       # skill_types.id → 中文系名
        self.effect_info = {}      # effect_info.id → info 文本
        self.moves = {}            # moves.id → 行（不含 raw_json）
        self.effect_moves = {}     # effect_id → 引用它的 move_id 集合
        self.teamable = set()      # learnable >= 5 的精灵 id
        self.traits = []           # new_se stat=1
        self.soulmarks_by_pet = {}  # monster id → [effect_icon 行]
        self.soul_effect_tips = {}  # effect_id → effect_icon.tips（魂印空间的显示文本）
        self.soul_effect_kind = {}  # effect_id → effect_icon.kind（魂印/技能/隐藏分类）
        self.custom_programs = set()   # custom_effect_programs.effect_id
        self.map_to = {}           # effect_id → map_value（override_type=map_to）
        self.ignored = set()       # override_type ∈ ignore/dead_column 的 effect_id

        # 4399 爬取
        self.pets4399 = {}         # pet_id(int) → 记录
        self.pets4399_names = {}   # 名字 → [pet_id]（同名多形态）
        self.hunyin4399 = {}       # pet_id(int) → 记录
        self.skills4399_by_name = {}  # 技能名 → [4399 技能条目]

        self.registry = {"moves": set(), "soul": set(), "suit": set()}
        # 注册表导出附带的对照表（states/anomalies：[{id,name}]，老导出文件没有）
        self.meta_lists = {"states": [], "anomalies": []}
        self._load_everything()

    # ------------------------------------------------------------ 装载

    def _load_everything(self):
        with self._lock:
            for row in self._db.execute("SELECT id, cn FROM skill_types"):
                self.type_names[row["id"]] = row["cn"]
            self.effect_args_num = {}  # effect_info.id → args_num（词条参数切分/编辑用）
            for row in self._db.execute("SELECT id, info, args_num FROM effect_info"):
                self.effect_info[row["id"]] = row["info"] or ""
                self.effect_args_num[row["id"]] = int(row["args_num"] or 0)
            for row in self._db.execute(
                "SELECT id, name, type_id, category, power, accuracy, priority,"
                " max_pp, must_hit, info, side_effect, friend_side_effect FROM moves"
            ):
                self.moves[row["id"]] = dict(row)
                for eid in _parse_id_array(row["side_effect"]) + _parse_id_array(
                    row["friend_side_effect"]
                ):
                    self.effect_moves.setdefault(eid, set()).add(row["id"])
            for row in self._db.execute(
                "SELECT monster_id FROM monster_learnable_moves"
                " GROUP BY monster_id HAVING COUNT(*) >= 5"
            ):
                self.teamable.add(row["monster_id"])
            self.traits = [
                dict(r) for r in self._db.execute(
                    "SELECT idx, effect_id, args, desc, intro FROM new_se"
                    " WHERE stat = 1 ORDER BY idx"
                )
            ]
            for row in self._db.execute(
                "SELECT id, pet_id, effect_id, kind, args, tips, come, analyze"
                " FROM effect_icon"
            ):
                rec = dict(row)
                # 魂印空间显示索引（控制台全量状态翻译用）：同 effect_id 多行
                # （多精灵共享印记）时保留最长 tips——描述最全的那条。
                eid = rec["effect_id"]
                if eid:
                    prev = self.soul_effect_tips.get(eid, "")
                    tips = rec["tips"] or ""
                    if len(tips) > len(prev):
                        self.soul_effect_tips[eid] = tips
                        self.soul_effect_kind[eid] = rec["kind"] or ""
                for pid in _parse_id_array(rec["pet_id"]):
                    self.soulmarks_by_pet.setdefault(pid, []).append(rec)
            for row in self._db.execute(
                "SELECT DISTINCT effect_id FROM custom_effect_programs"
            ):
                self.custom_programs.add(row["effect_id"])
            for row in self._db.execute(
                "SELECT effect_id, override_type, map_value FROM custom_effect_overrides"
            ):
                if row["override_type"] in ("ignore", "dead_column"):
                    self.ignored.add(row["effect_id"])
                elif row["override_type"] == "map_to" and row["map_value"]:
                    try:
                        self.map_to[row["effect_id"]] = int(row["map_value"])
                    except (TypeError, ValueError):
                        pass

        self._load_scrape()
        self.load_registry()
        log.info(
            "目录装载完成：monsters 按需 / moves=%d / 4399=%d / 魂印精灵=%d / "
            "注册表 moves=%d soul=%d suit=%d",
            len(self.moves), len(self.pets4399), len(self.soulmarks_by_pet),
            len(self.registry["moves"]), len(self.registry["soul"]),
            len(self.registry["suit"]),
        )

    def _load_scrape(self):
        if self.pets_jsonl and os.path.exists(self.pets_jsonl):
            with open(self.pets_jsonl, encoding="utf-8") as f:
                for line in f:
                    line = line.strip()
                    if not line:
                        continue
                    try:
                        rec = json.loads(line)
                    except ValueError:
                        continue
                    pid = int(rec.get("pet_id") or 0)
                    if not pid:
                        continue
                    self.pets4399[pid] = rec
                    self.pets4399_names.setdefault(rec.get("name", ""), []).append(pid)
                    for skill in rec.get("skills") or []:
                        name = skill.get("name")
                        if name:
                            entry = dict(skill)
                            entry["petId"] = pid
                            entry["petName"] = rec.get("name", "")
                            self.skills4399_by_name.setdefault(name, []).append(entry)
        if self.hunyin_jsonl and os.path.exists(self.hunyin_jsonl):
            with open(self.hunyin_jsonl, encoding="utf-8") as f:
                for line in f:
                    line = line.strip()
                    if not line:
                        continue
                    try:
                        rec = json.loads(line)
                    except ValueError:
                        continue
                    pid = int(rec.get("pet_id") or 0)
                    if pid:
                        self.hunyin4399[pid] = rec

    def load_registry(self):
        """（重）读 sim_server --dump-registry 导出的注册表 JSON。"""
        try:
            with open(self.registry_path, encoding="utf-8") as f:
                data = json.load(f)
            reg, meta = {}, {}
            for key, value in data.items():
                if not isinstance(value, list):
                    continue
                if value and isinstance(value[0], dict):
                    meta[key] = value          # states / anomalies 对照表
                else:
                    reg[key] = set(value)      # moves / soul / suit id 集
            self.registry = reg
            self.meta_lists = meta
            return True
        except (OSError, ValueError):
            return False

    def meta(self):
        """时点/异常对照表（断点下拉、事件带中文化用）。"""
        return self.meta_lists

    def effect_index(self):
        """效果显示索引（控制台全量状态翻译层，开工文档 §2.1：协议只传 ID，
        名字客户端解析）。两个注册空间**数字重叠**（effect 1004 ≠ 魂印 1004），
        消费端按桶/票的 soul 标志选表：
        - effects: effect_info 技能效果空间 id → {info, argsNum}，模板 {n} 代入 args；
        - soulEffects: effect_icon 魂印空间 id → {tips, kind}，纯文本描述。"""
        return {
            "effects": {
                eid: {"info": info, "argsNum": self.effect_args_num.get(eid, 0)}
                for eid, info in self.effect_info.items()
            },
            "soulEffects": {
                eid: {"tips": tips, "kind": self.soul_effect_kind.get(eid, "")}
                for eid, tips in self.soul_effect_tips.items()
            },
        }

    def medicine_items(self):
        """battle_items 里可用的药剂（回体力/回 PP/清异常），组队嗑药编辑器数据源。"""
        rows = self._db.execute(
            "SELECT item_id, name, hp, pp, remove_all_mon_stat FROM battle_items"
            " WHERE hp > 0 OR pp > 0 OR remove_all_mon_stat = 1 ORDER BY item_id"
        ).fetchall()
        return [
            {"itemId": r["item_id"], "name": r["name"], "hp": r["hp"] or 0,
             "pp": r["pp"] or 0, "cureAll": bool(r["remove_all_mon_stat"])}
            for r in rows
        ]

    # ------------------------------------------------------------ 徽标

    def effect_status(self, effect_id, registry_key="moves"):
        """单个 effect id 的实现状态。"""
        if effect_id in self.ignored:
            return "ignored"
        if registry_key == "moves":
            implemented = effect_id in self.registry["moves"] or effect_id in self.custom_programs
        else:
            implemented = effect_id in self.registry[registry_key]
        if implemented:
            return "ok"
        target = self.map_to.get(effect_id)
        if target is not None and target in self.registry[registry_key]:
            return "ok"
        return "none"

    def move_badge(self, move_row):
        """技能行的效果实现徽标：{status, missing:[effect id], effects:[{id,info,status}]}"""
        effect_ids = _parse_id_array(move_row["side_effect"]) + _parse_id_array(
            move_row["friend_side_effect"]
        )
        if not effect_ids:
            return {"status": "plain", "missing": [], "effects": []}
        effects = []
        missing = []
        statuses = []
        for eid in effect_ids:
            st = self.effect_status(eid, "moves")
            statuses.append(st)
            if st == "none":
                missing.append(eid)
            effects.append({"id": eid, "info": self.effect_info.get(eid, ""), "status": st,
                            "statusLabel": STATUS_LABEL[st]})
        if all(s == "ignored" for s in statuses):
            status = "ignored"
        elif not missing:
            status = "ok"
        elif len(missing) == len(effect_ids):
            status = "none"
        else:
            status = "partial"
        return {"status": status, "missing": missing, "effects": effects}

    # ------------------------------------------------------------ 查询：精灵

    def soul_badge(self, effect_id):
        """魂印行实现徽标：registry.soul 含该 effect_id = 有程序/效果注册。
        粒度 = 整条魂印（程序含多子句，逐句实现度暂无结构化数据）。
        status: ok=已注册程序 / none=未实现。"""
        st = self.effect_status(effect_id, "soul")
        return {"status": st}

    def search_pets(self, q, limit=60, teamable_only=True):
        """q = 名字子串或 id 前缀（可空：返回前 limit 只可组队的）。"""
        q = (q or "").strip()
        results = {}  # id → [score, row]

        def offer(pid, name, score):
            cur = results.get(pid)
            row = {"petId": pid, "name": name}
            if cur is None or score < cur[0]:
                results[pid] = (score, row)

        if q:
            for pid, rec in self.pets4399.items():
                name = rec.get("name", "")
                if name == q:
                    offer(pid, name, 0)
                elif name.startswith(q):
                    offer(pid, name, 1)
                elif q in name:
                    offer(pid, name, 3)
                elif str(pid).startswith(q):
                    offer(pid, name, 2)
            for row in self._db.execute(
                "SELECT id, name FROM monsters WHERE name LIKE ? LIMIT 2000",
                (f"%{q}%",),
            ):
                offer(row["id"], row["name"], 1 if row["name"].startswith(q) else 3)
        else:
            for pid in sorted(self.teamable)[: limit * 3]:
                name_row = self._db.execute(
                    "SELECT name FROM monsters WHERE id = ?", (pid,)
                ).fetchone()
                if name_row:
                    offer(pid, name_row["name"], 9)

        merged = []
        for pid, (score, row) in results.items():
            if teamable_only and pid not in self.teamable:
                continue
            rec4399 = self.pets4399.get(pid)
            row["has4399"] = rec4399 is not None
            row["element"] = (rec4399 or {}).get("element") or self._db_element(pid)
            row["teamable"] = pid in self.teamable
            merged.append((score, pid, row))
        merged.sort(key=lambda t: (t[0], t[1]))
        return [row for _, _, row in merged[:limit]]

    def _db_element(self, pid):
        row = self._db.execute(
            "SELECT type, type2 FROM monsters WHERE id = ?", (pid,)
        ).fetchone()
        if not row:
            return ""
        names = [self.type_names.get(row["type"], ""), self.type_names.get(row["type2"], "")]
        return "/".join(n for n in names if n)

    def pet_detail(self, pet_id):
        row = self._db.execute(
            "SELECT id, name, type, type2, gender, hp, atk, def, sp_atk, sp_def, spd"
            " FROM monsters WHERE id = ?",
            (pet_id,),
        ).fetchone()
        rec4399 = self.pets4399.get(pet_id)
        if not row and not rec4399:
            return None

        detail = {"petId": pet_id, "teamable": pet_id in self.teamable}
        # 展示名/属性：4399 优先
        detail["name"] = (rec4399 or {}).get("name") or (row["name"] if row else f"#{pet_id}")
        detail["element"] = (rec4399 or {}).get("element") or self._db_element(pet_id)
        detail["has4399"] = rec4399 is not None

        if row:
            t1 = self.type_names.get(row["type"], "")
            t2 = self.type_names.get(row["type2"], "")
            detail["db"] = {
                "name": row["name"],
                "types": [t for t in (t1, t2) if t],
                "gender": {0: "无", 1: "雄性", 2: "雌性"}.get(row["gender"], "未知"),
                "stats": {
                    "体力": row["hp"], "攻击": row["atk"], "防御": row["def"],
                    "特攻": row["sp_atk"], "特防": row["sp_def"], "速度": row["spd"],
                },
            }

        if rec4399:
            keep = ("name", "gender", "level", "pet_type", "element", "race", "training",
                    "detail_url", "catch_url")
            detail["scrape"] = {k: rec4399.get(k) for k in keep if rec4399.get(k)}
            detail["scrape"]["skills"] = rec4399.get("skills") or []

        # 魂印展示：4399 人话优先，回退 effect_icon.tips
        seal_text, seal_source = "", "none"
        scrape_seal = ((rec4399 or {}).get("soul_seal") or {}).get("text")
        hunyin_seal = (self.hunyin4399.get(pet_id) or {}).get("soul_seal")
        hunyin_text = (hunyin_seal or {}).get("text")
        if scrape_seal:
            seal_text, seal_source = scrape_seal, "4399"
        elif hunyin_text:
            seal_text, seal_source = hunyin_text, "4399"
        detail["soulSeal4399"] = {"text": seal_text, "source": seal_source}

        marks = []
        for rec in sorted(self.soulmarks_by_pet.get(pet_id, []), key=lambda r: r["id"]):
            status = self.effect_status(rec["effect_id"], "soul")
            marks.append({
                "effectId": rec["effect_id"],
                "kind": rec["kind"],
                "tips": rec["tips"] or "",
                "status": status,
                "statusLabel": STATUS_LABEL[status],
            })
        for rec in marks:
            rec["badge"] = self.soul_badge(rec.get("effectId") or 0)
        detail["soulMarks"] = marks
        if marks and not seal_text:
            detail["soulSeal4399"] = {
                "text": "；".join(m["tips"] for m in marks if m["tips"]),
                "source": "db",
            }

        # 第五技能（hide_moves 表）：官方第五技能不走学习表（2240/2240 不在
        # learnable），如圣灵谱尼神灵救世光、莱茵哈特 王·天乖陆离；引擎已放行
        # "第 5 槽可用 hide_moves 技能"。learnable 的 tag 1..4 是另一套标记
        # （谱尼案例证明不是第五阶段），仅原样透传。
        hidden_rows = self._db.execute(
            "SELECT move_id FROM hide_moves WHERE pet_id = ? ORDER BY move_id",
            (pet_id,),
        ).fetchall()
        hidden_ids = {r["move_id"] for r in hidden_rows}

        # 可学技能（组队用，来自数据库 —— 战斗以库为准）
        learnable = []
        for lr in self._db.execute(
            "SELECT move_id, learning_lv, rec, tag FROM monster_learnable_moves"
            " WHERE monster_id = ? ORDER BY learning_lv, move_id",
            (pet_id,),
        ):
            mrow = self.moves.get(lr["move_id"])
            if not mrow:
                continue
            badge = self.move_badge(mrow)
            learnable.append({
                "id": mrow["id"],
                "name": mrow["name"],
                "element": self.type_names.get(mrow["type_id"], ""),
                "category": CATEGORY_NAMES.get(mrow["category"], "属性"),
                "power": mrow["power"],
                "pp": mrow["max_pp"],
                "accuracy": mrow["accuracy"],
                "priority": mrow["priority"],
                "learnLv": lr["learning_lv"],
                "rec": lr["rec"],
                "tag": lr["tag"] or 0,
                "badge": badge,
                "badgeLabel": STATUS_LABEL[badge["status"]],
                "info": mrow["info"] or "",
            })
        detail["learnableMoves"] = learnable

        fifths = []
        for hid in sorted(hidden_ids):
            mrow = self.moves.get(hid)
            if not mrow:
                continue
            badge = self.move_badge(mrow)
            fifths.append({
                "id": mrow["id"],
                "name": mrow["name"],
                "element": self.type_names.get(mrow["type_id"], ""),
                "category": CATEGORY_NAMES.get(mrow["category"], "属性"),
                "power": mrow["power"],
                "pp": mrow["max_pp"],
                "accuracy": mrow["accuracy"],
                "priority": mrow["priority"],
                "badge": badge,
                "badgeLabel": STATUS_LABEL[badge["status"]],
                "info": mrow["info"] or "",
            })
        detail["fifthCandidates"] = fifths

        # 神谕（精灵王线）：sp_hide_moves 有行 = 神谕波次精灵。引擎装配口径
        # （pet_factory.cpp）：oracle_on 选魂印版本行（低 icon=神谕前 / 高=神谕后）
        # 并解锁神谕新技——常规 1~4 槽（威力<155）与第五（威力>=155）。
        oracle_rows = self._db.execute(
            "SELECT s.moves, COALESCE(m.power, 0) AS power"
            " FROM sp_hide_moves s LEFT JOIN moves m ON m.id = s.moves"
            " WHERE s.monster = ? ORDER BY s.moves ASC",
            (pet_id,),
        ).fetchall()

        def _oracle_entry(mid):
            mrow = self.moves.get(mid)
            if not mrow:
                return None
            badge = self.move_badge(mrow)
            return {
                "id": mrow["id"], "name": mrow["name"],
                "element": self.type_names.get(mrow["type_id"], ""),
                "category": CATEGORY_NAMES.get(mrow["category"], "属性"),
                "power": mrow["power"], "pp": mrow["max_pp"],
                "accuracy": mrow["accuracy"], "priority": mrow["priority"],
                "badge": badge, "badgeLabel": STATUS_LABEL[badge["status"]],
                "info": mrow["info"] or "",
            }

        oracle_normal, oracle_fifth = [], []
        for r in oracle_rows:
            mid = r["moves"]
            if not mid or mid <= 0:
                continue
            entry = _oracle_entry(mid)
            if entry is None:
                continue
            (oracle_fifth if (r["power"] or 0) >= 155 else oracle_normal).append(entry)
        detail["oracle"] = {
            "hasOracle": bool(oracle_rows),
            # 多版本魂印行（>=2）且神谕波次 → 引擎按 oracle_on 选行（低=前/高=后）
            "soulMarkVersions": bool(oracle_rows) and len(marks) >= 2,
            "moves": [m["id"] for m in oracle_normal],
            "fifth": [m["id"] for m in oracle_fifth],
        }
        detail["oracleMoves"] = oracle_normal
        detail["oracleFifthCandidates"] = oracle_fifth
        return detail

    # ------------------------------------------------------------ 查询：技能

    def search_moves(self, q, limit=60):
        q = (q or "").strip()
        out = []
        if not q:
            return out
        rows = self._db.execute(
            "SELECT id FROM moves WHERE name LIKE ? ORDER BY id LIMIT 400",
            (f"%{q}%",),
        )
        for row in rows:
            m = self.moves.get(row["id"])
            if not m:
                continue
            badge = self.move_badge(m)
            out.append({
                "id": m["id"], "name": m["name"],
                "element": self.type_names.get(m["type_id"], ""),
                "category": CATEGORY_NAMES.get(m["category"], "属性"),
                "power": m["power"], "pp": m["max_pp"],
                "badgeLabel": STATUS_LABEL[badge["status"]],
                "status": badge["status"],
                "info": m["info"] or "",
            })
            if len(out) >= limit:
                break
        return out

    def move_detail(self, move_id):
        m = self.moves.get(move_id)
        if not m:
            return None
        badge = self.move_badge(m)
        # 展示文本：4399 技能表同名条目优先（人话），回退库 info（神谕/新技能等）
        text4399, source = None, "db"
        for entry in self.skills4399_by_name.get(m["name"], [])[:1]:
            if entry.get("effect") and entry["effect"] != "--":
                text4399 = entry["effect"]
                source = "4399"
        detail = {
            "id": m["id"], "name": m["name"],
            "element": self.type_names.get(m["type_id"], ""),
            "category": CATEGORY_NAMES.get(m["category"], "属性"),
            "power": m["power"], "accuracy": m["accuracy"],
            "priority": m["priority"], "pp": m["max_pp"],
            "mustHit": bool(m["must_hit"]),
            "badge": badge, "badgeLabel": STATUS_LABEL[badge["status"]],
            "display": {"text": text4399 or m["info"] or "", "source": source},
            "effects": badge["effects"],
        }
        return detail

    def move_effects(self, move_id):
        """词条元数据（调试台"词条编辑"线，2026-09-26）：本技能的效果列表，
        每条含 effect_info 模板文本与 DB 当前参数。参数下标 = 模板 {n} 占位符，
        扁平 side_effect_arg 按 args_num 顺次切分（与引擎装配同一约定）。"""
        with self._lock:
            row = self._db.execute(
                "SELECT side_effect, side_effect_arg FROM moves WHERE id = ?", (move_id,)
            ).fetchone()
        if row is None:
            return None
        effect_ids = _parse_id_array(row["side_effect"])
        flat = _parse_id_array(row["side_effect_arg"])
        out, pos = [], 0
        for eid in effect_ids:
            n = max(0, self.effect_args_num.get(eid, 0))
            args = flat[pos:pos + n]
            pos += n
            out.append({"effectId": eid, "argsNum": n,
                        "template": self.effect_info.get(eid, ""), "args": args})
        return {"moveId": move_id, "effects": out}

    # ------------------------------------------------------------ 查询：特性 / 魂印

    def search_traits(self, q, limit=50):
        q = (q or "").strip().lower()
        out = []
        for t in self.traits:
            if q and q not in (t["desc"] or "").lower() and q not in (t["intro"] or "").lower() \
                    and not str(t["idx"]).startswith(q):
                continue
            # 特性行为按**名字**实现（trait_state.h trait_kind_from_name），不在魂印
            # 注册表里——按 effect_id 查注册表会永远"未实现"（2026-09-27 修）。
            status = "ok" if (t["desc"] or "") in IMPLEMENTED_TRAIT_NAMES else "none"
            out.append({
                "idx": t["idx"], "name": t["desc"], "intro": t["intro"],
                "effectId": t["effect_id"],
                "status": status, "statusLabel": STATUS_LABEL[status],
            })
            if len(out) >= limit:
                break
        return out

    # ------------------------------------------------------------ 覆盖率

    def coverage(self):
        def summarize(ref_ids, registry_key, label):
            counts = {"ok": 0, "none": 0, "ignored": 0}
            unimplemented = []
            for eid in sorted(ref_ids):
                st = self.effect_status(eid, registry_key)
                counts[st if st in counts else "none"] += 1
                if st == "none":
                    move_names = [
                        self.moves[m]["name"] for m in sorted(self.effect_moves.get(eid, set()))[:3]
                    ] if registry_key == "moves" else []
                    unimplemented.append({
                        "effectId": eid,
                        "info": self.effect_info.get(eid, ""),
                        "moves": move_names,
                    })
            return {
                "label": label,
                "registered": len(self.registry[registry_key]),
                "referenced": len(ref_ids),
                "implemented": counts["ok"],
                "unimplemented": counts["none"],
                "ignored": counts["ignored"],
                "samples": unimplemented[:200],
            }

        move_refs = set(self.effect_moves.keys())
        soul_refs = {
            rec["effect_id"] for recs in self.soulmarks_by_pet.values() for rec in recs
        }
        suit_refs = {
            r["id"] for r in self._db.execute("SELECT id FROM suit")
        }
        return {
            "moves": summarize(move_refs, "moves", "技能效果"),
            "soul": summarize(soul_refs, "soul", "魂印效果"),
            "suit": summarize(suit_refs, "suit", "套装效果"),
        }

    # ------------------------------------------------------------ 示例阵容

    def sample_lineup(self):
        """开箱即跑的一组阵容：优先文档预设的 12 号（波克尔），双方镜像。"""
        skills = [
            r["move_id"] for r in self._db.execute(
                "SELECT move_id FROM monster_learnable_moves WHERE monster_id = 12"
                " ORDER BY learning_lv LIMIT 5"
            )
        ]
        pet_id = 12
        if len(skills) < 5:
            row = self._db.execute(
                "SELECT monster_id FROM monster_learnable_moves"
                " GROUP BY monster_id HAVING COUNT(*) >= 5 ORDER BY monster_id LIMIT 1"
            ).fetchone()
            pet_id = row["monster_id"]
            skills = [
                r["move_id"] for r in self._db.execute(
                    "SELECT move_id FROM monster_learnable_moves WHERE monster_id = ?"
                    " ORDER BY learning_lv LIMIT 5",
                    (pet_id,),
                )
            ]
        pet = {"petId": pet_id, "skills": skills, "base": [0] * 6, "traitId": 0}
        # 单对象简写：服务端把这一只重复 6 遍
        return {"side1": pet, "side2": pet}

    # ------------------------------------------------------------ 培养（feat/peiyang）

    # ⚠️ 与引擎 src/entities/pet_factory.cpp 的 kGuildBoost/kHpCapBonus 手动同步
    #（工作值 30/15×4/10 与 +20，待游戏内最终校——两边一起改）。
    GUILD_BOOST = [15, 15, 15, 15, 10, 30]   # 引擎序：攻 特攻 防 特防 速 体
    HP_CAP_BONUS = 20

    @staticmethod
    def _mintmark_to_engine_order(raw6):
        """官方 mintmark 六维序 [攻,防,特攻,特防,速,体] → 引擎序 [攻,特攻,防,特防,速,体]。
        与引擎 load_mintmark 的 to_engine_order 一致（圣·虚无实测钉死，见调查 §4.4）。"""
        raw = list(raw6) + [0] * (6 - len(raw6))
        return [raw[0], raw[2], raw[1], raw[3], raw[4], raw[5]]

    def cultivation_meta(self):
        """培养元数据：性格表（25 种）+ 上限常量（编辑器/预览共用）。"""
        natures = [
            {"id": r["id"], "name": r["name"], "des": r["des"] or "",
             # 引擎序五维修正（无体力项）：攻 特攻 防 特防 速
             "mult": [r["atk"], r["sp_atk"], r["def"], r["sp_def"], r["spd"]]}
            for r in self._db.execute(
                "SELECT id, name, des, atk, sp_atk, def, sp_def, spd"
                " FROM nature ORDER BY id")
        ]
        titles = [
            {"id": r["id"], "name": r["name"],
             # 引擎序六维：攻 特攻 防 特防 速 体
             "stats": [r["atk"], r["sp_atk"], r["def"], r["sp_def"], r["spd"], r["hp"]],
             "exclusive": r["target_monster"] or 0, "memo": r["memo"] or ""}
            for r in self._db.execute(
                "SELECT id, name, hp, atk, sp_atk, def, sp_def, spd, target_monster, memo"
                " FROM title_stats ORDER BY id")
        ]
        return {
            "natures": natures,
            "titles": titles,
            "guildBoost": list(self.GUILD_BOOST),
            "hpCapBonus": self.HP_CAP_BONUS,
            "evPerStatCap": 255,
            "evTotalCap": 510,
            "note": "预览公式为引擎 PetFactory::create_numerical_base 的镜像，"
                    "引擎为权威（场景 135 钉死）。培养面板含刻印/称号 flat（独立加法区间，"
                    "2026-09-30 实测）；套装百分比加成是最后的乘区、吃刻印/称号"
                    "（2026-09-30 实测，场景 874 钉死，预览走 cultivation-preview 三视角），"
                    "魂印效果在局内结算。",
        }

    def search_suits(self, q, limit=30):
        """套装搜索（一键穿戴，2026-09-29）：suit 表 + 部件清单 + 件数。"""
        like = f"%{q}%"
        rows = self._db.execute(
            "SELECT id, name, cloths, suitdes FROM suit"
            " WHERE name LIKE ? ORDER BY id LIMIT ?", (like, int(limit)),
        ).fetchall()
        out = []
        for r in rows:
            cloths = _parse_id_array(r["cloths"])
            pieces = []
            for cid in cloths:
                er = self._db.execute(
                    "SELECT name, substr(desc, 1, 60) AS d FROM equip WHERE item_id = ?",
                    (cid,)).fetchone()
                pieces.append({"id": cid, "name": er["name"] if er else f"#{cid}",
                               "desc": (er["d"] if er else "") or ""})
            out.append({"id": r["id"], "name": r["name"] or f"#{r['id']}",
                        "pieces": pieces, "suitdes": r["suitdes"] or ""})
        return {"suits": out}

    def search_equips(self, q, limit=30):
        """装备/套装部件搜索（培养线 2026-09-28）：equip 表 name/desc 双 LIKE。"""
        like = f"%{q}%"
        rows = self._db.execute(
            "SELECT item_id, name, quality, suit_id, desc FROM equip"
            " WHERE name LIKE ? OR desc LIKE ?"
            " ORDER BY item_id LIMIT ?", (like, like, int(limit)),
        ).fetchall()
        return {"equips": [
            {"id": r["item_id"], "name": r["name"], "quality": r["quality"],
             "suitId": r["suit_id"] or 0, "desc": r["desc"] or ""}
            for r in rows
        ]}

    def search_inscriptions(self, q, limit=60):
        """刻印目录（可装面板数值的 type 0/3，隐藏件不出）。返回引擎序数值。"""
        like = f"%{q}%"
        rows = self._db.execute(
            "SELECT id, name, effect_des, type, quality, grade, class_id,"
            " arg_json, base_json, extra_json, max_json, monster_ids"
            " FROM mintmark WHERE hide = 0 AND type IN (0, 3)"
            " AND (name LIKE ? OR effect_des LIKE ?)"
            " ORDER BY type, id LIMIT ?",
            (like, like, int(limit)),
        ).fetchall()
        out = []
        monster_names = {}
        for r in rows:
            exclusive = _parse_id_array(r["monster_ids"])
            # 面板生效值：系列成长刻印(type 3)=强化满 max；属性刻印(type 0)=arg
            raw_eff = _parse_id_array(r["max_json"] if r["type"] == 3 else r["arg_json"])
            stats = self._mintmark_to_engine_order(raw_eff)
            names = []
            for pid in exclusive[:3]:
                if pid not in monster_names:
                    row = self._db.execute(
                        "SELECT name FROM monsters WHERE id = ?", (pid,)).fetchone()
                    monster_names[pid] = row["name"] if row else f"#{pid}"
                names.append(monster_names[pid])
            out.append({
                "id": r["id"], "name": r["name"], "effectDes": r["effect_des"] or "",
                "type": r["type"], "quality": r["quality"],
                "stats": stats,                       # 引擎序：攻 特攻 防 特防 速 体
                "exclusive": exclusive, "exclusiveNames": names,
                "base": self._mintmark_to_engine_order(_parse_id_array(r["base_json"]))
                        if r["type"] == 3 else None,
            })
        return {"inscriptions": out}

    def cultivation_preview(self, cfg):
        """面板合成预览：引擎 PetFactory::create_numerical_base(cultivate 模式) 的镜像。
        错误口径与引擎一致（-ev 超限/未知性格/未知刻印/专属错配/隐藏件）。"""
        errors = []
        pet_id = int(cfg.get("petId") or 0)
        row = self._db.execute(
            "SELECT hp, atk, def, sp_atk, sp_def, spd FROM monsters WHERE id = ?",
            (pet_id,),
        ).fetchone()
        if not row:
            return {"ok": False, "errors": [f"精灵 {pet_id} 不存在"]}
        race = [row["atk"], row["sp_atk"], row["def"], row["sp_def"], row["spd"], row["hp"]]

        ev = [int(x) for x in (cfg.get("ev") or [0] * 6)]
        if len(ev) != 6 or any(e < 0 or e > 255 for e in ev):
            errors.append("学习力单项须在 0~255")
            ev = [min(max(e, 0), 255) for e in (ev + [0] * 6)[:6]]
        elif sum(ev) > 510:
            errors.append(f"学习力总和 {sum(ev)} > 510")

        mult = [1.0] * 5
        nature_id = cfg.get("natureId", -1)
        if nature_id is not None and int(nature_id) >= 0:
            nat = self._db.execute(
                "SELECT name, atk, sp_atk, def, sp_def, spd FROM nature WHERE id = ?",
                (int(nature_id),),
            ).fetchone()
            if not nat:
                errors.append(f"未知性格 id {nature_id}")
            else:
                mult = [nat["atk"], nat["sp_atk"], nat["def"], nat["sp_def"], nat["spd"]]

        keryin_flat = [0] * 6
        for kid in (cfg.get("inscriptions") or [])[:3]:
            if not kid or int(kid) <= 0:
                continue
            k = self._db.execute(
                "SELECT name, type, hide, monster_ids, arg_json, max_json, extra_json"
                " FROM mintmark WHERE id = ?", (int(kid),),
            ).fetchone()
            if not k:
                errors.append(f"未知刻印 id {kid}")
                continue
            if k["hide"]:
                errors.append(f"刻印 {k['name']}（#{kid}）未放出，不可装")
                continue
            exclusive = _parse_id_array(k["monster_ids"])
            if exclusive and pet_id not in exclusive:
                errors.append(f"刻印 {k['name']}（#{kid}）为专属，该精灵不可装")
                continue
            if k["type"] not in (0, 3):
                errors.append(f"刻印 {k['name']}（#{kid}）不带面板数值")
                continue
            # 强化满实际面板 = max + extra（官方 2026 改版附加属性；与引擎
            # load_mintmark 同口径——圣战之锋α [55,25,0,25,30,80]+[5,3,0,3,2,2]
            # = [60,28,0,28,32,82]，对账官方面板钉死，2026-09-28）。
            raw_max = _parse_id_array(k["max_json"] if k["type"] == 3 else k["arg_json"])
            raw_extra = _parse_id_array(k["extra_json"]) if k["type"] == 3 else []
            raw_eff = [m + e for m, e in zip(raw_max + [0] * 6, raw_extra + [0] * 6)]
            stats = self._mintmark_to_engine_order(raw_eff)
            keryin_flat = [a + b for a, b in zip(keryin_flat, stats)]

        hp_cap = bool(cfg.get("hpCap20", True))
        guild = bool(cfg.get("guild", True))
        guild_boost = self.GUILD_BOOST if guild else [0] * 6
        # 年费通用加成（2026-09-28 用户拍板）：六维+10 平加，不吃性格，与引擎同层。
        annual = 10 if bool(cfg.get("annual", False)) else 0

        # 称号（2026-09-28）：六维平加进括号（pvp/pve 双有效，与刻印/年费同层），
        # 专属校验对齐刻印口径。
        title_flat = [0] * 6
        title_id = int(cfg.get("titleId") or 0)
        if title_id > 0:
            t = self._db.execute(
                "SELECT name, hp, atk, sp_atk, def, sp_def, spd, target_monster"
                " FROM title_stats WHERE id = ?", (title_id,),
            ).fetchone()
            if not t:
                errors.append(f"未知称号 id {title_id}")
            elif t["target_monster"] and pet_id != t["target_monster"]:
                errors.append(f"称号 {t['name']}（#{title_id}）为专属，该精灵不可配")
            else:
                title_flat = [t["atk"], t["sp_atk"], t["def"], t["sp_def"], t["spd"], t["hp"]]

        # ── 装备/套装（2026-09-28 三视角）：镜像引擎 resolve_equipment ──────────
        # 行来源 custom_equip_stats（flat=add_way 0 / pct=add_way 1；per_piece 每件
        # 一次；per_suit 只在成套时生效且按行身份去重；target_monster 定向；
        # pvp/pve 维度 → 三视角：base=双 1 行、pvp=base+pvp 行、pve=base+pve 行）。
        # 落点口径（与引擎 suit.cpp 同）：flat 进括号（刻印/称号/战队同层），
        # pct 乘括号总和（先点数后百分比、int 向下取整）。
        def _equip_rows_for(view):
            """view ∈ base/pvp/pve → 该视图生效的聚合 (flat[6], pct[6])。"""
            # base=双 1 行；pvp 视图=pvp=1 的行（含 base+pvp-only）；pve 同理
            want_pvp, want_pve = {"base": (1, 1), "pvp": (1, None), "pve": (None, 1)}[view]
            flat = [0] * 6
            pct = [0] * 6
            item_ids = [int(x) for x in (cfg.get("equip") or []) if int(x or 0) > 0]
            if not item_ids:
                return flat, pct
            # 按套装归组件数 → 成套判定（cloths 全长）
            worn_suit = {}
            suit_count = {}
            for iid in item_ids:
                er = self._db.execute(
                    "SELECT suit_id FROM equip WHERE item_id = ?", (iid,)).fetchone()
                sid = er["suit_id"] if er else 0
                worn_suit[iid] = sid or 0
                if sid:
                    suit_count[sid] = suit_count.get(sid, 0) + 1
            active = set()
            for sid, cnt in suit_count.items():
                sr = self._db.execute(
                    "SELECT cloths FROM suit WHERE id = ?", (sid,)).fetchone()
                if sr and cnt >= len(_parse_id_array(sr["cloths"])):
                    active.add(sid)
            seen = set()
            for iid in item_ids:
                for row in self._db.execute(
                    "SELECT stat_index, amount, add_way, scope, target_monster,"
                    " COALESCE(pvp, 1) AS pvp, COALESCE(pve, 1) AS pve"
                    " FROM custom_equip_stats WHERE item_id = ?", (iid,),
                ).fetchall():
                    if row["stat_index"] not in range(6):
                        continue
                    if want_pvp is not None and row["pvp"] != want_pvp:
                        continue
                    if want_pve is not None and row["pve"] != want_pve:
                        continue
                    if row["target_monster"] not in (0, pet_id):
                        continue
                    if row["scope"] == "per_suit":
                        sid = worn_suit.get(iid, 0)
                        if sid not in active:
                            continue
                        key = (iid, row["stat_index"], row["add_way"], row["target_monster"])
                        if key in seen:
                            continue
                        seen.add(key)
                    if row["add_way"] == 0:
                        flat[row["stat_index"]] += row["amount"]
                    else:
                        pct[row["stat_index"]] += row["amount"]
            return flat, pct

        labels = ["攻击", "特攻", "防御", "特防", "速度", "体力"]
        ev_q = [e / 4.0 for e in ev]   # ÷4 浮点贯通（2026-09-28 口径，int()=floor）

        def _compose(view):
            """镜像引擎两级口径：①培养合成（性格取整后成品 + 各层 flat）；
            ②装备落成品（resolve_equipment：+flat → ×(100+pct)/100，int=floor）。"""
            equip_flat, equip_pct = _equip_rows_for(view)
            out = []
            for i in range(5):
                v = int((race[i] * 2 + ev_q[i] + 31 + 5) * mult[i])
                v += keryin_flat[i] + title_flat[i] + guild_boost[i] + annual + equip_flat[i]
                out.append(int(v * (100 + equip_pct[i]) / 100))
            hp = int(race[5] * 2 + ev_q[5] + 31 + 110)
            hp += (keryin_flat[5] + title_flat[5]
                   + (self.HP_CAP_BONUS if hp_cap else 0) + guild_boost[5] + annual
                   + equip_flat[5])
            out.append(int(hp * (100 + equip_pct[5]) / 100))
            return out

        views = {v: _compose(v) for v in ("base", "pvp", "pve")}
        stats = views["base"]   # 旧字段向后兼容：无装备时三视图相同

        return {
            "ok": not errors,
            "errors": errors,
            "race": dict(zip(labels, race)),
            "stats": [{"label": labels[i], "value": stats[i]} for i in range(6)],
            "views": {v: [{"label": labels[i], "value": views[v][i]} for i in range(6)]
                      for v in views},
            "breakdown": {
                "keryinFlat": keryin_flat,
                "titleFlat": title_flat,
                "guildBoost": guild_boost,
                "hpCapBonus": self.HP_CAP_BONUS if hp_cap else 0,
                "natureMult": mult,
            },
        }
