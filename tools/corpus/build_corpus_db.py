#!/usr/bin/env python3
"""语料库检索：机制解析文章（reference/scrape/out/all_posts_final.jsonl）的
FTS5 全文索引 + 别名表。2026-09-27 开工（docs：控制台状态显示-开工.md 同批；
设计讨论定稿：向量化解决不了"圣逼"=格劳瑞这类黑话别名——别名是查表问题，
语义召回才向量的事，见 docs_local 2026-09-27 讨论）。

设计要点：
- 中文检索用**字间空格 + unicode61** 切词（"骑士对决"→"骑 士 对 决"），
  短语查询精确匹配子串，不依赖 FTS5 trigram（2 字别名"圣逼"trigram 匹配不了）。
- idx = all_posts_final.jsonl 的 **0-based 行号**——全库文档引用（idx=51 艾欧丽娅、
  idx=59 格劳恩斯）即此约定，已实测对齐。
- 别名表带出处（source），错别名可回溯——符合"口径要有证词"的仓库文化。
- 别名解析三规则（用户 2026-09-28）：①同名子串别名一般指序号大者；②"id 远大于 5000"
  是皮肤/boss 序号不理会；③图鉴漏记的新宠以 unity 库 monsters.def_name 为准
  （小写 def_name 与旧 DefName 混用——首版盘点名字全空即此因）。
- 向量层留了接口没上模型：语料 479 篇规模下 FTS5+别名覆盖 80%，等跨措辞
  检索需求真出现了再加 sqlite-vec/嵌入列（见工单讨论 2026-09-27）。

用法：
  python3 tools/corpus/build_corpus_db.py build    # 建库（幂等，重建）
  python3 tools/corpus/build_corpus_db.py query 圣逼 [--limit 10]
  python3 tools/corpus/build_corpus_db.py alias-add 圣光·格劳瑞 圣逼 --pet-id 4004 --source user:2026-09-27
"""
import argparse
import json
import re
import sqlite3
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
DEFAULT_JSONL = REPO / "reference/scrape/out/all_posts_final.jsonl"
DEFAULT_DB = REPO / "build/corpus_search.sqlite"

# 语料里已证实（有出处）的种子别名；后续由 LLM 扫描增量入库。
SEED_ALIASES = [
    # (canonical, pet_id, nickname, source)
    ("圣光·格劳瑞", 4004, "圣逼", "user:2026-09-27"),
    ("极渊DS-001", 4797, "001", "user:2026-09-27"),
    ("格劳恩斯", 3407, "火电", "corpus:idx59"),
    ("圣光莫妮卡", 3711, "圣莫", "corpus:research/精灵王技能组与魂印-调研报告"),
]

CJK = re.compile(r"([\u3400-\u4dbf\u4e00-\u9fff])")


def space_cjk(s: str) -> str:
    """字间加空格（unicode61 即切成单字 token）；拉丁串保持原样。"""
    return CJK.sub(r"\1 ", s)


def norm(s: str) -> str:
    return re.sub(r"\s+", "", s or "")


def build(db_path: Path, jsonl_path: Path) -> None:
    db_path.parent.mkdir(parents=True, exist_ok=True)
    if db_path.exists():
        db_path.unlink()
    con = sqlite3.connect(db_path)
    con.executescript(
        """
        CREATE TABLE posts(
            idx INTEGER PRIMARY KEY,   -- all_posts_final.jsonl 0-based 行号（全库引用约定）
            title TEXT, author TEXT, opus_id TEXT, url TEXT, pub_time TEXT, text TEXT);
        CREATE VIRTUAL TABLE posts_fts USING fts5(
            title_s, text_s, tokenize='unicode61');
        CREATE TABLE alias(
            nickname TEXT PRIMARY KEY, -- 规范化（去空白）后的别名
            canonical TEXT NOT NULL,   -- 官方名
            pet_id INTEGER,            -- 4399 pet_id（可空）
            source TEXT,               -- 出处：user:日期 / corpus:idxNN / auto:扫描批次
            added_at TEXT DEFAULT (datetime('now','localtime')));
        """
    )
    rows = []
    with open(jsonl_path, encoding="utf-8") as f:
        for idx, line in enumerate(f):
            d = json.loads(line)
            text = d.get("text") or ""

            def _s(v):
                """字段归一：opus_id 等列在部分行是 dict/None。"""
                if isinstance(v, str):
                    return v
                return json.dumps(v, ensure_ascii=False) if v else ""

            rows.append((idx, _s(d.get("title")), _s(d.get("author")),
                         _s(d.get("opus_id")), _s(d.get("pub_time")), text,
                         _s(d.get("url"))))
    con.executemany(
        "INSERT INTO posts(idx,title,author,opus_id,pub_time,text,url) VALUES(?,?,?,?,?,?,?)",
        rows,
    )
    con.executemany(
        "INSERT INTO posts_fts(rowid, title_s, text_s) VALUES(?,?,?)",
        [(i, space_cjk(t), space_cjk(x)) for i, t, _, _, _, x, _ in rows],
    )
    con.executemany(
        "INSERT OR REPLACE INTO alias(nickname, canonical, pet_id, source) VALUES(?,?,?,?)",
        [(norm(nick), cano, pid, src) for cano, pid, nick, src in SEED_ALIASES],
    )
    # 人工过目后的正式别名库（aliases_confirmed.json：用户批注 + 扫描 strong，见该文件头）。
    # 这是**持久真相源**——build 重建 DB 时自动带上；临时试验用 alias-add（DB 重建会丢）。
    confirmed_path = Path(__file__).parent / "aliases_confirmed.json"
    if confirmed_path.exists():
        conf = json.load(open(confirmed_path, encoding="utf-8"))
        con.executemany(
            "INSERT OR REPLACE INTO alias(nickname, canonical, pet_id, source) VALUES(?,?,?,?)",
            [(norm(a["nickname"]), a["canonical"], a.get("pet_id"),
              "confirmed:" + (a.get("sources") or ["?"])[0]) for a in conf["aliases"]],
        )
        n_alias = con.execute("SELECT COUNT(*) FROM alias").fetchone()[0]
        print(f"建库完成 {db_path}: 文章 {len(rows)} 篇, 别名 {n_alias} 条（含 confirmed {len(conf['aliases'])}）")
        con.commit()
        return
    con.commit()
    n_alias = con.execute("SELECT COUNT(*) FROM alias").fetchone()[0]
    print(f"建库完成 {db_path}: 文章 {len(rows)} 篇, 别名 {n_alias} 条")


def expand_phrases(con: sqlite3.Connection, query: str) -> tuple[list[str], list[str]]:
    """别名展开：返回 (查询短语列表, 命中的别名说明)。"""
    phrases, notes = [query], []
    row = con.execute(
        "SELECT canonical FROM alias WHERE nickname = ?", (norm(query),)
    ).fetchone()
    if row:
        canonical = row[0]
        phrases.append(canonical)
        notes.append(f"{norm(query)} → {canonical}（别名表）")
        for (nick,) in con.execute(
            "SELECT nickname FROM alias WHERE canonical = ? AND nickname != ?",
            (canonical, norm(query)),
        ):
            phrases.append(nick)
    return phrases, notes


def snippet(raw: str, phrases: list[str], width: int = 70) -> str:
    pos = -1
    flat = re.sub(r"\s+", "", raw)
    for p in sorted(phrases, key=len, reverse=True):
        pos = flat.find(norm(p))
        if pos >= 0:
            break
    if pos < 0:
        return flat[:width] + "…"
    lo, hi = max(0, pos - 25), min(len(flat), pos + len(norm(phrases[0])) + width - 25)
    return ("…" if lo else "") + flat[lo:hi] + ("…" if hi < len(flat) else "")


def query(db_path: Path, q: str, limit: int) -> int:
    con = sqlite3.connect(db_path)
    phrases, notes = expand_phrases(con, q)
    for n in notes:
        print(f"[别名] {n}")
    match = " OR ".join('"%s"' % space_cjk(p).strip() for p in phrases)
    sql = (
        "SELECT p.idx, p.title, p.author, p.url, "
        "(SELECT COUNT(*) FROM posts_fts f WHERE f.rowid = p.idx AND posts_fts MATCH ?) "
        "FROM posts_fts f JOIN posts p ON p.idx = f.rowid WHERE posts_fts MATCH ? "
        "ORDER BY 5 DESC, p.idx LIMIT ?"
    )
    hits = con.execute(sql, (match, match, limit)).fetchall()
    if not hits:
        print("（无命中）")
        return 1
    for idx, title, author, url, cnt in hits:
        raw = con.execute("SELECT text FROM posts WHERE idx=?", (idx,)).fetchone()[0]
        print(f"\n[idx{idx}] {title}  — {author}  (词频 {cnt})\n  {url}\n  {snippet(raw, phrases)}")
    print(f"\n共 {len(hits)} 篇")
    return 0


def alias_add(db_path: Path, canonical: str, nickname: str, pet_id: int, source: str) -> int:
    con = sqlite3.connect(db_path)
    con.execute(
        "INSERT OR REPLACE INTO alias(nickname, canonical, pet_id, source) VALUES(?,?,?,?)",
        (norm(nickname), canonical, pet_id, source),
    )
    con.commit()
    print(f"别名入库：{norm(nickname)} → {canonical} (pet {pet_id}, {source})")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("用法：")[0])
    ap.add_argument("--db", type=Path, default=DEFAULT_DB)
    sub = ap.add_subparsers(dest="cmd", required=True)
    b = sub.add_parser("build")
    b.add_argument("--jsonl", type=Path, default=DEFAULT_JSONL)
    q = sub.add_parser("query")
    q.add_argument("q")
    q.add_argument("--limit", type=int, default=10)
    a = sub.add_parser("alias-add")
    a.add_argument("canonical")
    a.add_argument("nickname")
    a.add_argument("--pet-id", type=int, default=None)
    a.add_argument("--source", default="user")
    args = ap.parse_args()
    if args.cmd == "build":
        build(args.db, args.jsonl)
        return 0
    if args.cmd == "query":
        return query(args.db, args.q, args.limit)
    return alias_add(args.db, args.canonical, args.nickname, args.pet_id, args.source)


if __name__ == "__main__":
    sys.exit(main())
