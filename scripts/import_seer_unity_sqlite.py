#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Import Seer Unity decoded JSON into SQLite (official structure).

数据管线（替换 H5）：
  fetch_seer_unity_bytes.py → decode_seer_bytes.py → 本脚本
输入：data/raw/unity_json/*.json（seerapi-solaris 解码产物）
输出：data/processed/seer_unity.sqlite

设计原则：新库以官方 Unity JSON 结构为准，C++ 引擎适配新结构。
- 数组字段（side_effect / side_effect_arg / param / en / att / pet_id / kind ...）
  一律存 JSON 数组文本，不做空格拼接，保留官方结构语义。
- learnable_moves 展开为 join 表 monster_learnable_moves。
- types_relation 是新增克制表（H5 缺失），倍率存 REAL（0.0/0.5/1.0/2.0）。

依赖：seerapi-solaris 仅用于解码（decode_seer_bytes.py）；本脚本只需标准库。
"""
from __future__ import annotations

import json
import os
import sqlite3
from pathlib import Path

ROOT = Path(os.path.dirname(os.path.abspath(__file__)))
SRC = ROOT / "data" / "raw" / "unity_json"
DB = ROOT / "data" / "processed" / "seer_unity.sqlite"


def load(name: str):
    with open(SRC / f"{name}.json", encoding="utf-8") as f:
        return json.load(f)


def json_text(value):
    """list -> JSON 数组文本；标量原样返回；None 保留。"""
    if value is None:
        return None
    if isinstance(value, (list, tuple, dict)):
        return json.dumps(value, ensure_ascii=False)
    return value


def json_list_text(value):
    """总是输出 JSON 数组文本（空数组输出 '[]'），用于官方数组字段。"""
    if value is None:
        value = []
    if isinstance(value, (list, tuple)):
        return json.dumps(list(value), ensure_ascii=False)
    return json.dumps([value], ensure_ascii=False)


def as_int(value, default=None):
    if value is None:
        return default
    try:
        return int(value)
    except (TypeError, ValueError):
        return default


SCHEMA = """
CREATE TABLE IF NOT EXISTS moves (
    id INTEGER PRIMARY KEY,
    name TEXT,
    type_id INTEGER,
    category INTEGER,
    power INTEGER,
    accuracy INTEGER,
    priority INTEGER,
    max_pp INTEGER,
    must_hit INTEGER,
    atk_type INTEGER,
    crit_rate INTEGER,
    mon_id INTEGER,
    info TEXT,
    ordinary INTEGER,
    side_effect TEXT,
    side_effect_arg TEXT,
    friend_side_effect TEXT,
    friend_side_effect_arg TEXT,
    raw_json TEXT
);

CREATE TABLE IF NOT EXISTS monsters (
    id INTEGER PRIMARY KEY,
    name TEXT,
    type INTEGER,
    type2 INTEGER,
    gender INTEGER,
    hp INTEGER,
    atk INTEGER,
    def INTEGER,
    sp_atk INTEGER,
    sp_def INTEGER,
    spd INTEGER,
    real_id INTEGER,
    soul_mark_id INTEGER,
    raw_json TEXT
);

CREATE TABLE IF NOT EXISTS monster_learnable_moves (
    monster_id INTEGER,
    move_id INTEGER,
    learning_lv INTEGER,
    rec INTEGER,
    tag INTEGER,
    PRIMARY KEY (monster_id, move_id)
);
CREATE INDEX IF NOT EXISTS idx_mlm_move ON monster_learnable_moves (move_id);

CREATE TABLE IF NOT EXISTS skill_types (
    id INTEGER PRIMARY KEY,
    cn TEXT,
    en TEXT,
    att TEXT,
    is_dou INTEGER,
    raw_json TEXT
);

CREATE TABLE IF NOT EXISTS side_effect (
    id INTEGER PRIMARY KEY,
    arg_count INTEGER,
    raw_json TEXT
);

CREATE TABLE IF NOT EXISTS effect_info (
    id INTEGER PRIMARY KEY,
    args_num INTEGER,
    info TEXT,
    param TEXT,
    analyze TEXT,
    type INTEGER,
    key TEXT,
    raw_json TEXT
);

CREATE TABLE IF NOT EXISTS battle_effects (
    id INTEGER,
    name TEXT,
    efftype INTEGER,
    group_type INTEGER,
    group_name TEXT,
    raw_json TEXT,
    PRIMARY KEY (id, group_type)
);

CREATE TABLE IF NOT EXISTS new_se (
    idx INTEGER PRIMARY KEY,
    stat INTEGER,
    effect_id INTEGER,
    args TEXT,
    desc TEXT,
    intro TEXT,
    star_level INTEGER,
    item_id INTEGER,
    addition_num INTEGER,
    addition_type INTEGER,
    raw_json TEXT
);

CREATE TABLE IF NOT EXISTS types_relation (
    attacker_type TEXT,
    defender_type TEXT,
    multiple REAL,
    raw_json TEXT,
    PRIMARY KEY (attacker_type, defender_type)
);

CREATE TABLE IF NOT EXISTS effect_icon (
    id INTEGER PRIMARY KEY,
    pet_id TEXT,
    icon_id INTEGER,
    effect_id INTEGER,
    kind TEXT,
    args TEXT,
    tips TEXT,
    come TEXT,
    intensify INTEGER,
    is_adv INTEGER,
    label INTEGER,
    target INTEGER,
    to_id INTEGER,
    limited_type INTEGER,
    analyze TEXT,
    raw_json TEXT
);

CREATE TABLE IF NOT EXISTS effect_des (
    id INTEGER PRIMARY KEY,
    kind INTEGER,
    kinddes TEXT,
    desc TEXT,
    raw_json TEXT
);

CREATE TABLE IF NOT EXISTS buff (
    id INTEGER PRIMARY KEY,
    desc TEXT,
    tag TEXT,
    desc_tag TEXT,
    icon TEXT,
    icontype INTEGER,
    raw_json TEXT
);

CREATE TABLE IF NOT EXISTS hide_moves (
    pet_id INTEGER,
    move_id INTEGER,
    move_name1 TEXT,
    move_name2 TEXT,
    raw_json TEXT,
    PRIMARY KEY (pet_id, move_id)
);

CREATE TABLE IF NOT EXISTS boss_effect_icon (
    eid INTEGER,
    args TEXT,
    icon_id INTEGER,
    tips TEXT,
    sort INTEGER,
    rows INTEGER,
    raw_json TEXT,
    PRIMARY KEY (eid, args)
);

CREATE TABLE IF NOT EXISTS effect_buff (
    id INTEGER PRIMARY KEY,
    name TEXT,
    kind INTEGER,
    desc TEXT,
    raw_json TEXT
);

CREATE TABLE IF NOT EXISTS item_type (
    type INTEGER,
    item INTEGER,
    raw_json TEXT
);

CREATE TABLE IF NOT EXISTS sp_hide_moves (
    id INTEGER,
    monster INTEGER,
    moves INTEGER,
    movetype INTEGER,
    item INTEGER,
    itemname TEXT,
    itemnumber INTEGER,
    movesname TEXT,
    kind TEXT,
    raw_json TEXT,
    PRIMARY KEY (id, monster, kind)
);

CREATE TABLE IF NOT EXISTS sp_hide_moves_bisaifu (
    id INTEGER,
    monster INTEGER,
    moves INTEGER,
    movetype INTEGER,
    item INTEGER,
    itemname TEXT,
    itemnumber INTEGER,
    movesname TEXT,
    kind TEXT,
    raw_json TEXT,
    PRIMARY KEY (id, monster, kind)
);

-- ════════════════════════════════════════════════════════════════════════
-- 装备/套装（equip.bytes + suit.bytes，2026-09-19 套装线入管线）。
-- Unity 端 equip 无 H5 的结构化数值字段（Attribute 六维/AddWay/BattleLv 已删），
-- 单件数值由 custom_equip_stats 离线编码；desc 逐件携带完整成套效果文本（人读校验源）。
-- ════════════════════════════════════════════════════════════════════════

-- 装备部件表：item_id 主键。suit_id=0 表示无套装归属的散件。
CREATE TABLE IF NOT EXISTS equip (
    item_id INTEGER PRIMARY KEY,
    name TEXT,
    quality INTEGER,
    suit_id INTEGER,
    rank_json TEXT,      -- 等级档位数组(JSON)：[{lv, desc}, ...]
    desc TEXT,           -- 单件描述（成套部件此列同时含成套效果全文）
    raw_json TEXT
);

-- 套装表：id = suit_id（equip.suit_id 外键）。cloths = 部件 item_id 数组(JSON)，
-- 成套激活判定 = 穿戴件数 ≥ cloths 长度（官方无独立"需求件数"字段，按部件清单全长）。
CREATE TABLE IF NOT EXISTS suit (
    id INTEGER PRIMARY KEY,
    name TEXT,
    cloths TEXT,         -- 部件清单(JSON 数组)
    suitdes TEXT,        -- 套装故事描述
    raw_json TEXT
);

-- ════════════════════════════════════════════════════════════════════════
-- 战斗内物品（Battleitem_*.json，2026-09-22 药剂线入管线）。
-- ⚠️ 源不在 unity_json（unity 端无此表），在 data/raw/official/ 官方快照（hash 文件名 → glob）。
-- 效果即官方字段（无独立效果表，itemsTip 的文字说明仅供人读校验）：
--   HP / PP               = 回复体力量 / 回复技能使用次数（官方 JSON 无该字段 = NULL = 无此效果）
--   RemoveMonStat         = 解除指定异常状态（状态码口径 = 引擎 AbnormalStatusId，如 2=烧伤）
--   RemoveAllMonStat      = 解除自身全部异常状态
--   RemoveBtLvDown        = 解除自身能力下降
--   Bonus                 = 捕捉加成（胶囊族，战斗模拟不用，原样保留）
-- 官方数据瑕疵（2026-09-22 用户拍板：**以效果字段为准**）：巅峰/极限活力药剂 ItemType 标 1
--   （体力类）但效果字段是 PP。精灵王 4 药剂（300758~761）效果字段全空（纯文字、场景限定）
--   → 引擎不做；蛋糕块×3 / 完全净化药剂在 itemType 分类里但不在 Battleitem 表 → 不入库。
CREATE TABLE IF NOT EXISTS battle_items (
    item_id INTEGER PRIMARY KEY,
    name TEXT,
    item_type INTEGER,            -- 官方 ItemType（0=胶囊 1=体力 2=活力；有错标，引擎不依赖）
    hp INTEGER,                   -- 回复体力量（NULL = 无此效果）
    pp INTEGER,                   -- 回复技能使用次数（NULL = 无此效果）
    remove_mon_stat INTEGER,      -- 解除指定异常状态（状态码，NULL = 无此效果）
    remove_all_mon_stat INTEGER,  -- 1 = 解除自身全部异常状态
    remove_bt_lv_down INTEGER,    -- 1 = 解除自身能力下降
    bonus REAL,                   -- 捕捉加成（胶囊族，原样保留）
    max_count INTEGER,            -- 官方 Max（背包上限，原样保留）
    raw_json TEXT
);

-- ════════════════════════════════════════════════════════════════════════
-- 认证数据层（custom_* 表）：只属于我们引擎的解释/差异，**不是官方字段**。
-- 特点：IF NOT EXISTS 保证不被 import 重刷；不在下方 IMPORTERS 里 → 数据跨 import 存活。
-- 引擎 loadSkills 读取顺序：官方事实 → custom_effect_overrides(差异纠偏) →
--   custom_effect_programs(离线编码程序, 命中则不再跑运行时文本 parser)。
-- 设计：docs/02-效果系统 认证数据层（待补）。
-- ════════════════════════════════════════════════════════════════════════

-- 离线编码的效果程序（组合语法/模板）。unit_json = 有序 EffectUnit 序列(JSON)，
-- 模型离线编码、引擎直接加载，替代"运行时解析官方文本"这条脆弱层。
-- 锚定二选一：effect_id(单效果) 或 skill_id(整招组合, 如无相谛 37460)。
CREATE TABLE IF NOT EXISTS custom_effect_programs (
    id INTEGER PRIMARY KEY,
    effect_id INTEGER,                       -- 锚定单效果(可空：组合型用 skill_id)
    skill_id  INTEGER,                       -- 锚定整招组合(可空：单效果型用 effect_id)
    kind      TEXT NOT NULL DEFAULT 'program',  -- 'combo' | 'template' | 'program'
    name_zh   TEXT,                          -- 人读名(如 无相谛-威力模板)
    unit_json TEXT NOT NULL,                 -- [{"condition": {...}, "action": {...}}, ...]
    memo      TEXT,                          -- 出处/设计说明
    UNIQUE(effect_id, skill_id, kind)
);

-- 官方与我们的差异纠偏：存"故意不一致"的决策, 免得下个会话误当 bug 重查。
-- 覆盖已知官方问题：monsters.soul_mark_id 死列、effect_info.param_type 过时、措辞变化等。
CREATE TABLE IF NOT EXISTS custom_effect_overrides (
    id INTEGER PRIMARY KEY,
    effect_id    INTEGER NOT NULL,
    override_type TEXT NOT NULL,             -- interpret_as_program | map_to | ignore | dead_column
    map_value    TEXT,                       -- override_type 的载荷(目标effect_id / 模板 / NULL)
    source_scope TEXT,                       -- 出处作用域(moves|effect_icon|hide_moves|...)
    rationale    TEXT,                       -- 为什么与官方不同(存决策, 很重要)
    UNIQUE(effect_id, override_type)
);

-- 单件装备数值加成（离线编码）。Unity equip.bytes 不再携带结构化数值（H5 的
-- Attribute 六维/AddWay 已删），只能从 desc 文本人工对齐后编码在此；口径实测变化时改表。
-- stat_index：0=攻击 1=特攻 2=防御 3=特防 4=速度 5=体力（引擎 NumericalPropertyIndex 序）。
-- add_way：0=点数 1=百分比（沿 H5 AddWay 语义）。
-- scope：'per_piece'（每穿一件算一次）/ 'per_suit'（成套后算一次，编码在每件上重复行）。
-- target_monster：0=背包内所有精灵；>0=只有该精灵 id 受益（六界战甲 414"背包内六界御神
--   …"定向条款，六界御神=4032）。
CREATE TABLE IF NOT EXISTS custom_equip_stats (
    item_id    INTEGER NOT NULL,
    stat_index INTEGER NOT NULL,
    amount     INTEGER NOT NULL,
    add_way    INTEGER NOT NULL DEFAULT 0,
    scope      TEXT NOT NULL DEFAULT 'per_piece',
    target_monster INTEGER NOT NULL DEFAULT 0,
    pvp        INTEGER NOT NULL DEFAULT 1,  -- 生效维度（2026-09-28 三视角）：官方限定词
    pve        INTEGER NOT NULL DEFAULT 1,  --   "仅限赛尔与赛尔间对战"=(1,0)、"赛尔间对战无效"=(0,1)
    memo       TEXT,
    UNIQUE(item_id, stat_index, add_way, scope, target_monster)
);

-- 刻印（培养线 2026-09-26）。官方 Unity mintmark.bytes，游戏内系统名"刻印"（mintmark）。
-- ⚠️ 本表六维数组存**官方原序 [攻击,防御,特攻,特防,速度,体力]**（圣·虚无 effect_des
-- 实测钉死：防在 index1、特攻恒 0 在 index2），与引擎 NumericalPropertyIndex 序
-- [攻,特攻,防,特防,速,体] 不同——引擎 load_mintmark 读取时已重排，直查本表注意别搞混。
-- 成长型数值口径：面板按"强化满"（max）计入——竞技环境默认满强化（与天赋 31 同理）；
--   extra_attri_value 为部分系列（class 12 等 342 条）的额外成长段，base+extra≤max，
--   是否独立于 max 叠加待实测，先落库不改数值。
-- type：0=属性刻印（微型/初级/全能/专属单体等，arg=六维固定加成）
--       1=技能刻印（660 条，绑定 move_id，arg=技能参数如先制+1；已绝版）
--       3=系列成长刻印（2788 条：base 强化前 → max 强化满；圣·虚无/雷之霆威等）
--       4=碎片素材（无属性）。
CREATE TABLE IF NOT EXISTS mintmark (
    id INTEGER PRIMARY KEY,
    name TEXT,               -- des
    effect_des TEXT,         -- 人读效果全文（"体力10/90,攻击5/55,…"格式）
    type INTEGER,
    grade INTEGER,           -- 档位（系列刻印=5，其余 0）
    quality INTEGER,
    rare INTEGER,
    rarity INTEGER,
    class_id INTEGER,        -- 系列（mintmark_class.id，0=无系列）
    arg_json TEXT,           -- 属性刻印=[6] 固定加成；技能刻印=技能参数
    base_json TEXT,          -- [6] 强化前（成长型）
    extra_json TEXT,         -- [6] 额外成长值（342 条成长型子集）
    max_json TEXT,           -- [6] 强化满（成长型）
    monster_ids TEXT,        -- JSON 数组：专属绑定精灵（2519 条非空；空=通用）
    move_ids TEXT,           -- JSON 数组：技能刻印绑定技能（660 条非空）
    level INTEGER,
    max_level INTEGER,       -- 官方字段 max
    total_consume INTEGER,
    connect INTEGER,
    hide INTEGER,            -- 84 条隐藏/未放出
    raw_json TEXT
);

CREATE TABLE IF NOT EXISTS mintmark_class (
    id INTEGER PRIMARY KEY,
    name TEXT,
    raw_json TEXT
);

-- 刻印升级链（泰坦之威等，20 条）：origin_mintmark → elevare_mintmark。
CREATE TABLE IF NOT EXISTS mintmark_elevare (
    id INTEGER PRIMARY KEY,
    origin_mintmark INTEGER,
    elevare_mintmark INTEGER,
    primum_mintmark INTEGER,
    type INTEGER,
    cost_json TEXT,          -- 升级材料（官方 cost 数组）
    desc TEXT,
    raw_json TEXT
);

-- 性格表（培养线 2026-09-26）。官方 nature.bytes：25 种（id 0-24），五维 ±10% 乘数，
-- 无体力项（性格不影响体力）；id 20-24 为中性（害羞/实干/坦率/浮躁/认真）。
-- 乘数 REAL（1.1/0.9/1.0），引擎合成时 atk×mult 并向下取整。
CREATE TABLE IF NOT EXISTS nature (
    id INTEGER PRIMARY KEY,
    name TEXT,
    des TEXT,
    atk REAL,
    sp_atk REAL,
    def REAL,
    sp_def REAL,
    spd REAL,
    raw_json TEXT
);
-- 称号加成（培养线 2026-09-28，title_stats 手工表）。⚠️ 官方 Unity/H5 配置均无
-- 称号属性表（configs 694 文件仅 TitleBg 背景图；achievements.ability_title 全 0；
-- medals 为军衔无加成）——与战队加成同款处境：本地静态表 + 工作值，游戏内核准后
-- 只改本表不改代码。列名即引擎序 [atk, sp_atk, def, sp_def, spd, hp]（nature 同款，
-- 无 mintmark 的官方序/引擎序重排问题）。加成口径：称号 flat 在性格修正**之后**
-- 平加（4399 公式"基础能力值+…+称号加成"同括号平加段），不受性格影响。
-- target_monster：0=通用；>0=专属（仅该精灵可佩戴，如"正义圣使"→重生之翼 2987）。
CREATE TABLE IF NOT EXISTS title_stats (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL,
    hp INTEGER NOT NULL DEFAULT 0,
    atk INTEGER NOT NULL DEFAULT 0,
    sp_atk INTEGER NOT NULL DEFAULT 0,
    def INTEGER NOT NULL DEFAULT 0,
    sp_def INTEGER NOT NULL DEFAULT 0,
    spd INTEGER NOT NULL DEFAULT 0,
    target_monster INTEGER NOT NULL DEFAULT 0,
    memo TEXT,
    UNIQUE(name)
);
"""

SEED_TITLES = [
    # 工作值样例（量级对标战队 15/30 与刻印 55-90 之间），全部待游戏内核准。
    {"id": 1, "name": "巅峰王者", "hp": 30, "atk": 10, "sp_atk": 10, "def": 10,
     "sp_def": 10, "spd": 10, "target_monster": 0, "memo": "工作值待校（巅峰之战赛季奖励类通用称号样例）"},
    {"id": 2, "name": "星际旅行者", "hp": 20, "atk": 0, "sp_atk": 0, "def": 0,
     "sp_def": 0, "spd": 15, "target_monster": 0, "memo": "工作值待校（探索类通用称号样例，部分项为 0）"},
    {"id": 3, "name": "正义圣使", "hp": 40, "atk": 25, "sp_atk": 25, "def": 0,
     "sp_def": 0, "spd": 0, "target_monster": 2987, "memo": "工作值待校（重生之翼 2987 专属称号，社区实锤存在专属称号机制）"},
]


def import_moves(conn: sqlite3.Connection) -> int:
    items = load("moves")["root"]["moves"]["move"]
    cur = conn.cursor()
    n = 0
    for it in items:
        mid = as_int(it.get("id"))
        if mid is None:
            continue
        raw = json.dumps(it, ensure_ascii=False)
        cur.execute(
            """INSERT INTO moves (id,name,type_id,category,power,accuracy,priority,max_pp,
               must_hit,atk_type,crit_rate,mon_id,info,ordinary,
               side_effect,side_effect_arg,friend_side_effect,friend_side_effect_arg,raw_json)
               VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)
               ON CONFLICT(id) DO UPDATE SET name=excluded.name,type_id=excluded.type_id,
                 category=excluded.category,power=excluded.power,accuracy=excluded.accuracy,
                 priority=excluded.priority,max_pp=excluded.max_pp,must_hit=excluded.must_hit,
                 atk_type=excluded.atk_type,crit_rate=excluded.crit_rate,mon_id=excluded.mon_id,
                 info=excluded.info,ordinary=excluded.ordinary,side_effect=excluded.side_effect,
                 side_effect_arg=excluded.side_effect_arg,friend_side_effect=excluded.friend_side_effect,
                 friend_side_effect_arg=excluded.friend_side_effect_arg,raw_json=excluded.raw_json""",
            (
                mid, it.get("name"), as_int(it.get("type")), as_int(it.get("category")),
                as_int(it.get("power")), as_int(it.get("accuracy")), as_int(it.get("priority"), 0),
                as_int(it.get("max_pp")), as_int(it.get("must_hit"), 0), as_int(it.get("atk_type"), 0),
                as_int(it.get("crit_rate"), 0), as_int(it.get("mon_id"), 0), it.get("info"),
                as_int(it.get("ordinary"), 0),
                json_list_text(it.get("side_effect")), json_list_text(it.get("side_effect_arg")),
                json_list_text(it.get("friend_side_effect")), json_list_text(it.get("friend_side_effect_arg")),
                raw,
            ),
        )
        n += 1
    return n


def import_monsters(conn: sqlite3.Connection) -> int:
    items = load("monsters")["monsters"]["monster"]
    cur = conn.cursor()
    n = 0
    for it in items:
        mid = as_int(it.get("id"))
        if mid is None:
            continue
        raw = json.dumps(it, ensure_ascii=False)
        # 魂印关联：新 Unity 结构无精灵→new_se 链接（add_se 字段全空），统一置 0
        cur.execute(
            """INSERT INTO monsters (id,name,type,type2,gender,hp,atk,def,sp_atk,sp_def,spd,
               real_id,soul_mark_id,raw_json) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?)
               ON CONFLICT(id) DO UPDATE SET name=excluded.name,type=excluded.type,
                 type2=excluded.type2,gender=excluded.gender,hp=excluded.hp,atk=excluded.atk,
                 def=excluded.def,sp_atk=excluded.sp_atk,sp_def=excluded.sp_def,spd=excluded.spd,
                 real_id=excluded.real_id,soul_mark_id=excluded.soul_mark_id,raw_json=excluded.raw_json""",
            (
                mid, it.get("def_name"), as_int(it.get("type")), as_int(it.get("type2")),
                as_int(it.get("gender"), 2), as_int(it.get("hp")), as_int(it.get("atk")),
                as_int(it.get("def")), as_int(it.get("sp_atk")), as_int(it.get("sp_def")),
                as_int(it.get("spd")), as_int(it.get("real_id"), 0), 0, raw,
            ),
        )
        n += 1
    return n


def import_monster_learnable_moves(conn: sqlite3.Connection) -> int:
    items = load("monsters")["monsters"]["monster"]
    cur = conn.cursor()
    n = 0
    for it in items:
        mid = as_int(it.get("id"))
        if mid is None:
            continue
        lm = it.get("learnable_moves") or {}
        moves = lm.get("move") or []
        if isinstance(moves, dict):
            moves = [moves]
        for m in moves:
            move_id = as_int(m.get("id"))
            if move_id is None:
                continue
            cur.execute(
                """INSERT INTO monster_learnable_moves (monster_id,move_id,learning_lv,rec,tag)
                   VALUES (?,?,?,?,?)
                   ON CONFLICT(monster_id,move_id) DO UPDATE SET
                     learning_lv=excluded.learning_lv,rec=excluded.rec,tag=excluded.tag""",
                (mid, move_id, as_int(m.get("learning_lv"), 0), as_int(m.get("rec"), 0),
                 as_int(m.get("tag"), 0)),
            )
            n += 1
    return n


def import_skill_types(conn: sqlite3.Connection) -> int:
    items = load("skillType")["root"]["item"]
    cur = conn.cursor()
    n = 0
    for it in items:
        tid = as_int(it.get("id"))
        if tid is None:
            continue
        raw = json.dumps(it, ensure_ascii=False)
        cur.execute(
            "INSERT INTO skill_types (id,cn,en,att,is_dou,raw_json) VALUES (?,?,?,?,?,?) "
            "ON CONFLICT(id) DO UPDATE SET cn=excluded.cn,en=excluded.en,att=excluded.att,"
            "is_dou=excluded.is_dou,raw_json=excluded.raw_json",
            (tid, it.get("cn"), json_list_text(it.get("en")), json_list_text(it.get("att")),
             as_int(it.get("is_dou"), 0), raw),
        )
        n += 1
    return n


def import_side_effect(conn: sqlite3.Connection) -> int:
    node = load("side_effect").get("side_effects", [])
    if isinstance(node, dict):
        node = node.get("side_effect", [])
    items = node if isinstance(node, list) else []
    cur = conn.cursor()
    n = 0
    for it in items:
        eid = as_int(it.get("id"))
        if eid is None:
            continue
        raw = json.dumps(it, ensure_ascii=False)
        cur.execute(
            "INSERT INTO side_effect (id,arg_count,raw_json) VALUES (?,?,?) "
            "ON CONFLICT(id) DO UPDATE SET arg_count=excluded.arg_count, raw_json=excluded.raw_json",
            (eid, as_int(it.get("side_effect_argcount")), raw),
        )
        n += 1
    return n


def import_effect_info(conn: sqlite3.Connection) -> int:
    items = load("effectInfo")["root"]["effect"]
    cur = conn.cursor()
    n = 0
    for it in items:
        eid = as_int(it.get("id"))
        if eid is None:
            continue
        raw = json.dumps(it, ensure_ascii=False)
        cur.execute(
            "INSERT INTO effect_info (id,args_num,info,param,analyze,type,key,raw_json) "
            "VALUES (?,?,?,?,?,?,?,?) "
            "ON CONFLICT(id) DO UPDATE SET args_num=excluded.args_num, info=excluded.info, "
            "param=excluded.param, analyze=excluded.analyze, type=excluded.type, "
            "key=excluded.key, raw_json=excluded.raw_json",
            (eid, as_int(it.get("args_num")), it.get("info"), json_list_text(it.get("param")),
             it.get("analyze"), as_int(it.get("type")), it.get("key"), raw),
        )
        n += 1
    return n


def import_battle_effects(conn: sqlite3.Connection) -> int:
    node = load("battleEffects").get("battle_effects", [])
    if isinstance(node, dict):
        node = node.get("battle_effect", [])
    groups = node if isinstance(node, list) else []
    cur = conn.cursor()
    n = 0
    for group in groups:
        group_type = group.get("type")
        group_name = group.get("name")
        subs = group.get("sub_effect", [])
        if isinstance(subs, dict):
            subs = [subs]
        for it in subs:
            eid = as_int(it.get("id"))
            if eid is None:
                continue
            raw = json.dumps(it, ensure_ascii=False)
            cur.execute(
                "INSERT INTO battle_effects (id,name,efftype,group_type,group_name,raw_json) "
                "VALUES (?,?,?,?,?,?) "
                "ON CONFLICT(id,group_type) DO UPDATE SET name=excluded.name, efftype=excluded.efftype, "
                "group_name=excluded.group_name, raw_json=excluded.raw_json",
                (eid, it.get("name"), it.get("efftype"), group_type, group_name, raw),
            )
            n += 1
    return n


def import_new_se(conn: sqlite3.Connection) -> int:
    items = load("new_se")["NewSe"]["NewSeIdx"]
    cur = conn.cursor()
    n = 0
    for it in items:
        idx = as_int(it.get("Idx"))
        if idx is None:
            continue
        raw = json.dumps(it, ensure_ascii=False)
        cur.execute(
            """INSERT INTO new_se (idx,stat,effect_id,args,desc,intro,star_level,item_id,
               addition_num,addition_type,raw_json) VALUES (?,?,?,?,?,?,?,?,?,?,?)
               ON CONFLICT(idx) DO UPDATE SET stat=excluded.stat, effect_id=excluded.effect_id,
                 args=excluded.args, desc=excluded.desc, intro=excluded.intro,
                 star_level=excluded.star_level, item_id=excluded.item_id,
                 addition_num=excluded.addition_num, addition_type=excluded.addition_type,
                 raw_json=excluded.raw_json""",
            (idx, as_int(it.get("Stat")), as_int(it.get("Eid")), it.get("Args"),
             it.get("Desc"), it.get("Intro"), as_int(it.get("StarLevel"), 0),
             as_int(it.get("ItemId"), 0), as_int(it.get("AdditionNum"), 0),
             as_int(it.get("AdditionType"), 0), raw),
        )
        n += 1
    return n


def import_types_relation(conn: sqlite3.Connection) -> int:
    items = load("typesRelation")["root"]["relation"]
    cur = conn.cursor()
    n = 0
    for rel in items:
        attacker = rel.get("type")
        if not attacker:
            continue
        for opp in rel.get("opponent", []) or []:
            defender = opp.get("type")
            if not defender:
                continue
            raw = json.dumps({"attacker": attacker, "defender": defender,
                              "multiple": opp.get("multiple")}, ensure_ascii=False)
            cur.execute(
                "INSERT INTO types_relation (attacker_type,defender_type,multiple,raw_json) "
                "VALUES (?,?,?,?) "
                "ON CONFLICT(attacker_type,defender_type) DO UPDATE SET "
                "multiple=excluded.multiple, raw_json=excluded.raw_json",
                (attacker, defender, opp.get("multiple"), raw),
            )
            n += 1
    return n


def import_effect_icon(conn: sqlite3.Connection) -> int:
    items = load("effectIcon")["root"]["effect"]
    cur = conn.cursor()
    n = 0
    for it in items:
        eid = as_int(it.get("id"))
        if eid is None:
            continue
        raw = json.dumps(it, ensure_ascii=False)
        cur.execute(
            """INSERT INTO effect_icon (id,pet_id,icon_id,effect_id,kind,args,tips,come,intensify,
               is_adv,label,target,to_id,limited_type,analyze,raw_json) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)
               ON CONFLICT(id) DO UPDATE SET pet_id=excluded.pet_id, icon_id=excluded.icon_id,
                 effect_id=excluded.effect_id, kind=excluded.kind, args=excluded.args,
                 tips=excluded.tips, come=excluded.come, intensify=excluded.intensify,
                 is_adv=excluded.is_adv, label=excluded.label, target=excluded.target,
                 to_id=excluded.to_id, limited_type=excluded.limited_type, analyze=excluded.analyze,
                 raw_json=excluded.raw_json""",
            (eid, json_list_text(it.get("pet_id")), as_int(it.get("icon_id")),
             as_int(it.get("effect_id")), json_list_text(it.get("kind")), it.get("args"),
             it.get("tips"), it.get("come"), as_int(it.get("intensify")),
             as_int(it.get("is_adv")), as_int(it.get("label")), as_int(it.get("target")),
             as_int(it.get("to")), as_int(it.get("limited_type")), it.get("analyze"), raw),
        )
        n += 1
    return n


def import_effect_des(conn: sqlite3.Connection) -> int:
    items = load("effectDes")["root"]["item"]
    cur = conn.cursor()
    n = 0
    for it in items:
        eid = as_int(it.get("id"))
        if eid is None:
            continue
        raw = json.dumps(it, ensure_ascii=False)
        cur.execute(
            "INSERT INTO effect_des (id,kind,kinddes,desc,raw_json) VALUES (?,?,?,?,?) "
            "ON CONFLICT(id) DO UPDATE SET kind=excluded.kind, kinddes=excluded.kinddes, "
            "desc=excluded.desc, raw_json=excluded.raw_json",
            (eid, as_int(it.get("kind")), it.get("kinddes"), it.get("desc"), raw),
        )
        n += 1
    return n


def import_buff(conn: sqlite3.Connection) -> int:
    items = load("buff").get("data", [])
    cur = conn.cursor()
    n = 0
    for it in items:
        bid = as_int(it.get("id"))
        if bid is None:
            continue
        raw = json.dumps(it, ensure_ascii=False)
        cur.execute(
            "INSERT INTO buff (id,desc,tag,desc_tag,icon,icontype,raw_json) VALUES (?,?,?,?,?,?,?) "
            "ON CONFLICT(id) DO UPDATE SET desc=excluded.desc, tag=excluded.tag, "
            "desc_tag=excluded.desc_tag, icon=excluded.icon, icontype=excluded.icontype, "
            "raw_json=excluded.raw_json",
            (bid, it.get("desc"), it.get("tag"), it.get("desc_tag"), json_list_text(it.get("icon")),
             as_int(it.get("icontype")), raw),
        )
        n += 1
    return n


def import_hide_moves(conn: sqlite3.Connection) -> int:
    items = load("hideMoves")["root"]["item"]
    cur = conn.cursor()
    n = 0
    for it in items:
        pet_id = as_int(it.get("pet_id"))
        move_id = as_int(it.get("move_id"))
        if pet_id is None or move_id is None:
            continue
        raw = json.dumps(it, ensure_ascii=False)
        cur.execute(
            "INSERT INTO hide_moves (pet_id,move_id,move_name1,move_name2,raw_json) "
            "VALUES (?,?,?,?,?) "
            "ON CONFLICT(pet_id,move_id) DO UPDATE SET move_name1=excluded.move_name1, "
            "move_name2=excluded.move_name2, raw_json=excluded.raw_json",
            (pet_id, move_id, it.get("move_name1"), it.get("move_name2"), raw),
        )
        n += 1
    return n


def import_boss_effect_icon(conn: sqlite3.Connection) -> int:
    items = load("bossEffectIcon")["root"]["boss_effect"]
    cur = conn.cursor()
    n = 0
    for it in items:
        eid = as_int(it.get("eid"))
        if eid is None:
            continue
        args = it.get("args") or ""
        raw = json.dumps(it, ensure_ascii=False)
        cur.execute(
            "INSERT INTO boss_effect_icon (eid,args,icon_id,tips,sort,rows,raw_json) "
            "VALUES (?,?,?,?,?,?,?) "
            "ON CONFLICT(eid,args) DO UPDATE SET icon_id=excluded.icon_id, tips=excluded.tips, "
            "sort=excluded.sort, rows=excluded.rows, raw_json=excluded.raw_json",
            (eid, args, as_int(it.get("icon_id")), it.get("tips"), as_int(it.get("sort")),
             as_int(it.get("rows")), raw),
        )
        n += 1
    return n


def import_effect_buff(conn: sqlite3.Connection) -> int:
    items = load("effectBuff")["root"]["buff"]
    cur = conn.cursor()
    n = 0
    for it in items:
        bid = as_int(it.get("id"))
        if bid is None:
            continue
        raw = json.dumps(it, ensure_ascii=False)
        cur.execute(
            "INSERT INTO effect_buff (id,name,kind,desc,raw_json) VALUES (?,?,?,?,?) "
            "ON CONFLICT(id) DO UPDATE SET name=excluded.name, kind=excluded.kind, "
            "desc=excluded.desc, raw_json=excluded.raw_json",
            (bid, it.get("name"), as_int(it.get("kind")), it.get("desc"), raw),
        )
        n += 1
    return n


def import_item_type(conn: sqlite3.Connection) -> int:
    items = load("itemType")["root"]["list"]
    cur = conn.cursor()
    n = 0
    for it in items:
        raw = json.dumps(it, ensure_ascii=False)
        cur.execute(
            "INSERT INTO item_type (type,item,raw_json) VALUES (?,?,?)",
            (as_int(it.get("type")), as_int(it.get("item")), raw),
        )
        n += 1
    return n


def import_sp_hide_moves(conn: sqlite3.Connection, name: str, table: str) -> int:
    items_show = load(name)["config"].get("show_moves", [])
    items_sp = load(name)["config"].get("sp_moves", [])
    cur = conn.cursor()
    n = 0
    for it in list(items_show) + list(items_sp):
        rid = as_int(it.get("id"))
        monster = as_int(it.get("monster"))
        if rid is None or monster is None:
            continue
        kind = "sp" if it in items_sp else "show"
        raw = json.dumps(it, ensure_ascii=False)
        cur.execute(
            f"""INSERT INTO {table} (id,monster,moves,movetype,item,itemname,itemnumber,movesname,kind,raw_json)
                VALUES (?,?,?,?,?,?,?,?,?,?)
                ON CONFLICT(id,monster,kind) DO UPDATE SET moves=excluded.moves,
                  movetype=excluded.movetype, item=excluded.item, itemname=excluded.itemname,
                  itemnumber=excluded.itemnumber, movesname=excluded.movesname, raw_json=excluded.raw_json""",
            (rid, monster, as_int(it.get("moves")), as_int(it.get("movetype")),
             as_int(it.get("item")), it.get("itemname"), as_int(it.get("itemnumber")),
             it.get("movesname"), kind, raw),
        )
        n += 1
    return n


def import_equip(conn: sqlite3.Connection) -> int:
    items = load("equip")["equips"]["equip"]
    cur = conn.cursor()
    n = 0
    for it in items:
        item_id = as_int(it.get("item_id"))
        if item_id is None:
            continue
        raw = json.dumps(it, ensure_ascii=False)
        cur.execute(
            """INSERT INTO equip (item_id,name,quality,suit_id,rank_json,desc,raw_json)
               VALUES (?,?,?,?,?,?,?)
               ON CONFLICT(item_id) DO UPDATE SET name=excluded.name,
                 quality=excluded.quality, suit_id=excluded.suit_id,
                 rank_json=excluded.rank_json, desc=excluded.desc, raw_json=excluded.raw_json""",
            (item_id, it.get("name"), as_int(it.get("quality")),
             as_int(it.get("suit_id")), json_text(it.get("rank")),
             it.get("desc"), raw),
        )
        n += 1
    return n


def import_suit(conn: sqlite3.Connection) -> int:
    items = load("suit")["root"]["item"]
    cur = conn.cursor()
    n = 0
    for it in items:
        sid = as_int(it.get("id"))
        if sid is None:
            continue
        raw = json.dumps(it, ensure_ascii=False)
        cur.execute(
            """INSERT INTO suit (id,name,cloths,suitdes,raw_json) VALUES (?,?,?,?,?)
               ON CONFLICT(id) DO UPDATE SET name=excluded.name, cloths=excluded.cloths,
                 suitdes=excluded.suitdes, raw_json=excluded.raw_json""",
            (sid, it.get("name"), json_list_text(it.get("cloths")),
             it.get("suitdes"), raw),
        )
        n += 1
    return n


def import_mintmark(conn: sqlite3.Connection) -> int:
    items = load("mintmark")["mint_marks"]["mint_mark"]
    cur = conn.cursor()
    n = 0
    for it in items:
        mid = as_int(it.get("id"))
        if mid is None:
            continue
        raw = json.dumps(it, ensure_ascii=False)
        cur.execute(
            """INSERT INTO mintmark (id,name,effect_des,type,grade,quality,rare,rarity,
               class_id,arg_json,base_json,extra_json,max_json,monster_ids,move_ids,
               level,max_level,total_consume,connect,hide,raw_json)
               VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)
               ON CONFLICT(id) DO UPDATE SET name=excluded.name, effect_des=excluded.effect_des,
                 type=excluded.type, grade=excluded.grade, quality=excluded.quality,
                 rare=excluded.rare, rarity=excluded.rarity, class_id=excluded.class_id,
                 arg_json=excluded.arg_json, base_json=excluded.base_json,
                 extra_json=excluded.extra_json, max_json=excluded.max_json,
                 monster_ids=excluded.monster_ids, move_ids=excluded.move_ids,
                 level=excluded.level, max_level=excluded.max_level,
                 total_consume=excluded.total_consume, connect=excluded.connect,
                 hide=excluded.hide, raw_json=excluded.raw_json""",
            (
                mid, it.get("des"), it.get("effect_des"), as_int(it.get("type")),
                as_int(it.get("grade")), as_int(it.get("quality")), as_int(it.get("rare")),
                as_int(it.get("rarity")), as_int(it.get("mintmark_class")),
                json_text(it.get("arg")), json_text(it.get("base_attri_value")),
                json_text(it.get("extra_attri_value")), json_text(it.get("max_attri_value")),
                json_text(it.get("monster_id")), json_text(it.get("move_id")),
                as_int(it.get("level")), as_int(it.get("max")),
                as_int(it.get("total_consume")), as_int(it.get("connect")),
                as_int(it.get("hide")), raw,
            ),
        )
        n += 1
    return n


def import_mintmark_class(conn: sqlite3.Connection) -> int:
    items = load("mintmark")["mint_marks"]["mintmark_class"]
    cur = conn.cursor()
    n = 0
    for it in items:
        cid = as_int(it.get("id"))
        if cid is None:
            continue
        raw = json.dumps(it, ensure_ascii=False)
        cur.execute(
            """INSERT INTO mintmark_class (id,name,raw_json) VALUES (?,?,?)
               ON CONFLICT(id) DO UPDATE SET name=excluded.name, raw_json=excluded.raw_json""",
            (cid, it.get("class_name"), raw),
        )
        n += 1
    return n


def import_mintmark_elevare(conn: sqlite3.Connection) -> int:
    items = load("mintmarkElevare")["mintmark_elevare"]["mintmark_elevare"]
    cur = conn.cursor()
    n = 0
    for it in items:
        eid = as_int(it.get("id"))
        if eid is None:
            continue
        raw = json.dumps(it, ensure_ascii=False)
        cur.execute(
            """INSERT INTO mintmark_elevare (id,origin_mintmark,elevare_mintmark,
               primum_mintmark,type,cost_json,desc,raw_json) VALUES (?,?,?,?,?,?,?,?)
               ON CONFLICT(id) DO UPDATE SET origin_mintmark=excluded.origin_mintmark,
                 elevare_mintmark=excluded.elevare_mintmark,
                 primum_mintmark=excluded.primum_mintmark, type=excluded.type,
                 cost_json=excluded.cost_json, desc=excluded.desc, raw_json=excluded.raw_json""",
            (eid, as_int(it.get("origin_mintmark")), as_int(it.get("elevare_mintmark")),
             as_int(it.get("primum_mintmark")), as_int(it.get("type")),
             json_text(it.get("cost")), it.get("desc"), raw),
        )
        n += 1
    return n


def import_nature(conn: sqlite3.Connection) -> int:
    items = load("nature")["root"]["nature"]
    cur = conn.cursor()
    n = 0
    for it in items:
        nid = as_int(it.get("id"))
        if nid is None:
            continue
        raw = json.dumps(it, ensure_ascii=False)
        cur.execute(
            """INSERT INTO nature (id,name,des,atk,sp_atk,def,sp_def,spd,raw_json)
               VALUES (?,?,?,?,?,?,?,?,?)
               ON CONFLICT(id) DO UPDATE SET name=excluded.name, des=excluded.des,
                 atk=excluded.atk, sp_atk=excluded.sp_atk, def=excluded.def,
                 sp_def=excluded.sp_def, spd=excluded.spd, raw_json=excluded.raw_json""",
            (nid, it.get("name"), it.get("des"),
             it.get("atk"), it.get("sp_atk"), it.get("def"),
             it.get("sp_def"), it.get("spd"), raw),
        )
        n += 1
    return n


def import_title_stats(conn: sqlite3.Connection) -> int:
    # 手工表：无官方数据源，种子在 SEED_TITLES（与 custom_equip_stats 同性质）。
    cur = conn.cursor()
    n = 0
    for it in SEED_TITLES:
        cur.execute(
            """INSERT INTO title_stats (id,name,hp,atk,sp_atk,def,sp_def,spd,target_monster,memo)
               VALUES (?,?,?,?,?,?,?,?,?,?)
               ON CONFLICT(id) DO UPDATE SET name=excluded.name, hp=excluded.hp,
                 atk=excluded.atk, sp_atk=excluded.sp_atk, def=excluded.def,
                 sp_def=excluded.sp_def, spd=excluded.spd,
                 target_monster=excluded.target_monster, memo=excluded.memo""",
            (it["id"], it["name"], it["hp"], it["atk"], it["sp_atk"], it["def"],
             it["sp_def"], it["spd"], it["target_monster"], it["memo"]),
        )
        n += 1
    return n


def import_battle_items(conn: sqlite3.Connection) -> int:
    # 官方 Battleitem 快照（hash 文件名不跨版本稳定 → glob 取最新一份）。
    snaps = sorted((ROOT / "data" / "raw" / "official").glob("Battleitem_*.json"))
    if not snaps:
        raise FileNotFoundError("data/raw/official/Battleitem_*.json 不存在，先抓官方快照")
    items = json.loads(snaps[-1].read_text(encoding="utf-8"))["Items"]["Item"]
    cur = conn.cursor()
    n = 0
    for it in items:
        iid = as_int(it.get("ID"))
        if iid is None:
            continue
        raw = json.dumps(it, ensure_ascii=False)
        cur.execute(
            """INSERT INTO battle_items (item_id,name,item_type,hp,pp,remove_mon_stat,
                 remove_all_mon_stat,remove_bt_lv_down,bonus,max_count,raw_json)
               VALUES (?,?,?,?,?,?,?,?,?,?,?)
               ON CONFLICT(item_id) DO UPDATE SET name=excluded.name,
                 item_type=excluded.item_type, hp=excluded.hp, pp=excluded.pp,
                 remove_mon_stat=excluded.remove_mon_stat,
                 remove_all_mon_stat=excluded.remove_all_mon_stat,
                 remove_bt_lv_down=excluded.remove_bt_lv_down, bonus=excluded.bonus,
                 max_count=excluded.max_count, raw_json=excluded.raw_json""",
            (iid, it.get("Name"), as_int(it.get("ItemType")), as_int(it.get("HP")),
             as_int(it.get("PP")), as_int(it.get("RemoveMonStat")),
             as_int(it.get("RemoveAllMonStat")), as_int(it.get("RemoveBtLvDown")),
             it.get("Bonus"), as_int(it.get("Max")), raw),
        )
        n += 1
    return n


IMPORTERS = [
    ("moves", import_moves),
    ("monsters", import_monsters),
    ("monster_learnable_moves", import_monster_learnable_moves),
    ("skill_types", import_skill_types),
    ("side_effect", import_side_effect),
    ("effect_info", import_effect_info),
    ("battle_effects", import_battle_effects),
    ("new_se", import_new_se),
    ("types_relation", import_types_relation),
    ("effect_icon", import_effect_icon),
    ("effect_des", import_effect_des),
    ("buff", import_buff),
    ("hide_moves", import_hide_moves),
    ("boss_effect_icon", import_boss_effect_icon),
    ("effect_buff", import_effect_buff),
    ("item_type", import_item_type),
    ("battle_items", import_battle_items),
    ("equip", import_equip),
    ("suit", import_suit),
    ("mintmark", import_mintmark),
    ("mintmark_class", import_mintmark_class),
    ("mintmark_elevare", import_mintmark_elevare),
    ("nature", import_nature),
    ("title_stats", import_title_stats),
    ("sp_hide_moves", lambda c: import_sp_hide_moves(c, "spHideMoves", "sp_hide_moves")),
    ("sp_hide_moves_bisaifu", lambda c: import_sp_hide_moves(c, "spHideMovesBisaifu", "sp_hide_moves_bisaifu")),
]


def main() -> int:
    if not SRC.is_dir():
        print(f"源目录不存在: {SRC}，先运行 fetch + decode")
        return 1
    DB.parent.mkdir(parents=True, exist_ok=True)
    conn = sqlite3.connect(DB)
    conn.executescript(SCHEMA)
    # 幂等迁移：已存在的旧库补三视角生效维度列（CREATE IF NOT EXISTS 不会加列）
    for col in ("pvp", "pve"):
        try:
            conn.execute(
                f"ALTER TABLE custom_equip_stats ADD COLUMN {col} INTEGER NOT NULL DEFAULT 1")
        except Exception:
            pass
    for table, _ in IMPORTERS:
        conn.execute(f"DELETE FROM {table}")
    counts = {}
    for table, fn in IMPORTERS:
        counts[table] = fn(conn)
        print(f"  {table}: {counts[table]}")
    conn.commit()
    conn.close()
    print(f"DB: {DB}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
