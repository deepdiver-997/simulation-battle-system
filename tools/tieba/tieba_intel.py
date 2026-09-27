#!/usr/bin/env python3
"""tieba_intel.py — 百度贴吧「赛尔号吧」信息抓取与本地检索工具。

双后端（--backend auto|client|web，默认 auto）：
  client  aiotieba 客户端 API（推荐；结构化数据、自带精华区/楼中楼，与 Web 风控体系
          不同。需 python3 -m venv .venv && .venv/bin/pip install aiotieba，
          本体在 auto 模式下会自动换 .venv 解释器重执行）
  web     移动端 SSR 通道（/mo/q/m，零依赖正则解析）：
            吧主题列表:  https://tieba.baidu.com/mo/q/m?kw=<吧名>&pn=<页>
            精华区列表:  https://tieba.baidu.com/mo/q/m?kw=<吧名>&lm=4&pn=<页>
            主题详情:    https://tieba.baidu.com/mo/q/m?kz=<tid>&pn=<页>
          桌面版 /p/ 页面只返回 JS 壳、无 cookie 会被「百度安全验证」拦截，
          故统一用移动 UA + 预热 cookie（BAIDUID，访问 www.baidu.com 获取）。

用法概览：
  python3 tieba_intel.py update --pages 2            # 抓最新列表(+精华)，新帖自动补详情
  python3 tieba_intel.py update --pages 2 --no-fetch # 只更新列表，不抓详情
  python3 tieba_intel.py thread 8002241883 --lzl     # 抓指定主题全部楼层（--lzl 补楼中楼）
  python3 tieba_intel.py search 天启 --json          # 本地全文检索（标题+正文）
  python3 tieba_intel.py show 8002241883             # 终端阅读主题
  python3 tieba_intel.py stats                       # 库内概况

代理：默认直连。需要时 --proxy http://127.0.0.1:7897 或环境变量 TIEBA_PROXY。
"""

from __future__ import annotations

import argparse
import asyncio
import html as html_mod
import http.cookiejar
import json
import os
import random
import re
import sqlite3
import ssl
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path

UA_MOBILE = (
    "Mozilla/5.0 (iPhone; CPU iPhone OS 16_0 like Mac OS X) "
    "AppleWebKit/605.1.15 (KHTML, like Gecko) Version/16.0 Mobile/15E148 Safari/604.1"
)
DEFAULT_FORUM = "赛尔号"
DEFAULT_DB = Path(__file__).resolve().parent / "data" / "tieba.db"
WARMUP_URL = "https://www.baidu.com/"  # 预热拿 BAIDUID；直接访问 tieba 首页(手机UA)会被 403

RETRY_MAX = 3
VERIFY_MARKERS = ("百度安全验证", "安全验证", "wappass")


# --------------------------------------------------------------------------- #
# HTTP 会话
# --------------------------------------------------------------------------- #

class TiebaSession:
    """移动端会话：cookie 预热 + 可选代理 + 限速 + 重试。"""

    def __init__(self, proxy: str | None = None, delay: float = 1.5):
        self.delay = delay
        proxy = proxy or os.environ.get("TIEBA_PROXY")
        jar = http.cookiejar.CookieJar()
        handlers = [
            urllib.request.HTTPCookieProcessor(jar),
            urllib.request.HTTPSHandler(context=ssl.create_default_context()),
        ]
        if proxy:
            handlers.append(
                urllib.request.ProxyHandler({"http": proxy, "https": proxy})
            )
        self.opener = urllib.request.build_opener(*handlers)
        self.opener.addheaders = [
            ("User-Agent", UA_MOBILE),
            ("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8"),
            ("Accept-Language", "zh-CN,zh;q=0.9"),
        ]
        self.jar = jar
        self._warmed = False
        self._last_req = 0.0

    def warmup(self) -> None:
        """先访问首页拿 BAIDUID 等 cookie，显著降低触发安全验证的概率。"""
        if self._warmed:
            return
        try:
            self.get(WARMUP_URL, timeout=15)
        except (OSError, RuntimeError) as e:
            print(f"[warn] cookie 预热失败（继续尝试）: {e}", file=sys.stderr)
        self._warmed = True

    def _throttle(self) -> None:
        wait = self.delay + random.uniform(0, self.delay * 0.5) - (time.time() - self._last_req)
        if wait > 0:
            time.sleep(wait)

    def get(self, url: str, timeout: int = 20) -> str:
        last_err: Exception | None = None
        for attempt in range(1, RETRY_MAX + 1):
            self._throttle()
            self._last_req = time.time()
            body, status = "", 0
            try:
                with self.opener.open(url, timeout=timeout) as resp:
                    body = resp.read().decode("utf-8", errors="replace")
                    status = resp.status
            except urllib.error.HTTPError as e:
                # 4xx/5xx：opener 直接抛 HTTPError，验证页也可能裹在 403 body 里
                status = e.code
                last_err = RuntimeError(f"HTTP {status}")
                try:
                    body = e.read().decode("utf-8", errors="replace")
                except OSError:
                    pass
            except (OSError, urllib.error.URLError) as e:  # noqa: BLE001
                last_err = e
                time.sleep(attempt * 2)
                continue
            if any(m in body[:4000] for m in VERIFY_MARKERS):
                raise RuntimeError(
                    "触发百度安全验证（风控页）。请等几十分钟自愈、降低频率，"
                    "或换出口 IP：--proxy http://127.0.0.1:7897"
                )
            if status == 200:
                return body
            # 403/429/5xx 多为频率风控，退避更久；其余 4xx 重试无意义
            if status in (403, 429, 500, 502, 503):
                time.sleep(5 + attempt * 5)
            else:
                break
        raise RuntimeError(f"请求失败（重试 {RETRY_MAX} 次）: {url} — {last_err}")


# --------------------------------------------------------------------------- #
# HTML 解析（正则提取，避免第三方依赖）
# --------------------------------------------------------------------------- #

TAG_RE = re.compile(r"<[^>]+>")


def strip_tags(fragment: str) -> str:
    text = TAG_RE.sub("", fragment)
    text = html_mod.unescape(text)
    return re.sub(r"\s+", " ", text).strip()


def parse_forum_page(page: str) -> list[dict]:
    """解析吧列表页 → [{tid,title,is_top,is_good}]。"""
    items: list[dict] = []
    for m in re.finditer(r'<li\b[^>]*\bdata-tid="(\d+)"[^>]*>([\s\S]*?)</li>', page):
        tid, block = m.group(1), m.group(2)
        title_m = re.search(r'<div class="ti_title">([\s\S]*?)</div>', block)
        if not title_m:
            continue
        title = strip_tags(title_m.group(1))
        if not title:
            continue
        items.append(
            {
                "tid": int(tid),
                "title": title,
                "is_top": 1 if "ti_icon_zhiding" in block else 0,
                "is_good": 1 if "is_jingpost=1" in block else 0,
            }
        )
    # 同一 tid 去重（置顶与普通流可能重复出现），保留置顶/精华标记的并集
    merged: dict[int, dict] = {}
    for it in items:
        cur = merged.get(it["tid"])
        if cur is None:
            merged[it["tid"]] = it
        else:
            cur["is_top"] = max(cur["is_top"], it["is_top"])
            cur["is_good"] = max(cur["is_good"], it["is_good"])
    return list(merged.values())


def parse_thread_page(page: str) -> tuple[str, list[dict]]:
    """解析主题详情页 → (标题, [楼层])。

    楼层字段：pid/floor/author/content/post_time/is_lzl。
    楼层块为 <li class="list_item...">，data-info（&quot; 实体转义的 JSON）含
    pid/un/floor_num/name_show；主楼与部分楼层内容在 class="content"，
    翻页楼层在 class="floor_content"；楼中楼为块内 class="floor_footer_item"。
    """
    title_m = re.search(r"<title>([^<]+)</title>", page)
    title = html_mod.unescape(title_m.group(1)).strip() if title_m else ""
    title = re.sub(r"^第\d+/\d+页[,，]回贴列表-", "", title)
    title = re.sub(r"_百度贴吧.*$", "", title).strip()

    floors: list[dict] = []
    blocks = re.split(r'(?=<li\b[^>]*class="list_item)', page)
    for block in blocks[1:]:
        info: dict = {}
        m = re.search(r'data-info="([^"]*)"', block) or re.search(r"data-info='([^']*)'", block)
        if m:
            try:
                info = json.loads(html_mod.unescape(m.group(1)))
            except json.JSONDecodeError:
                pass
        pid = int(info.get("pid", 0) or 0)
        floor_num = int(info.get("floor_num", 0) or 0)
        author = info.get("name_show") or info.get("un") or ""
        if not author:
            un_m = re.search(r'class="user_name[^"]*"[^>]*>([^<]+)', block)
            if un_m:
                author = strip_tags(un_m.group(1)).rstrip(":：")
        # 正文：翻页楼层在 floor_content，主楼/首屏楼层在 content
        content_m = re.search(r'class="floor_content[^"]*">([\s\S]*?)</div>', block)
        if content_m is None:
            content_m = re.search(r'class="content"[^>]*>([\s\S]*?)</div>', block)
        content = strip_tags(content_m.group(1)) if content_m else ""
        time_m = re.search(r'class="list_item_time"[^>]*>([^<]+)<', block)
        post_time = strip_tags(time_m.group(1)) if time_m else ""
        parent_floor = floor_num if floor_num else (floors[-1]["floor"] + 1 if floors else 1)
        floors.append(
            {"pid": pid, "floor": float(parent_floor), "author": author,
             "content": content, "post_time": post_time, "is_lzl": 0}
        )
        # 楼中楼：floor 取 父楼层+k/1000 保持稳定排序
        lzl_seq = 0
        for lzl in re.finditer(
            r'<div class="floor_footer_item">([\s\S]*?)(?=<div class="floor_footer_item">|</div>\s*</div>\s*</li>|\Z)',
            block,
        ):
            frag = lzl.group(1)
            un_m = re.search(r'class="user_name[^"]*"[^>]*>([^<]+)', frag)
            lzl_author = strip_tags(un_m.group(1)).rstrip(":：") if un_m else ""
            lzl_content = strip_tags(frag)
            if lzl_author and lzl_content.startswith(lzl_author):
                lzl_content = lzl_content[len(lzl_author):].lstrip(":： ").strip()
            if not lzl_content:
                continue
            lzl_seq += 1
            floors.append(
                {"pid": 0, "floor": parent_floor + lzl_seq / 1000.0,
                 "author": lzl_author, "content": lzl_content,
                 "post_time": "", "is_lzl": 1}
            )
    return title, floors


def total_page_hint(page: str) -> int | None:
    """从详情页内嵌 JSON 里取总页数；末页之后贴吧会一直重复返回最后一页，必须靠它停页。"""
    m = re.search(r'"total_page":(\d+)', page)
    return int(m.group(1)) if m else None


# --------------------------------------------------------------------------- #
# SQLite 存储
# --------------------------------------------------------------------------- #

SCHEMA = """
CREATE TABLE IF NOT EXISTS threads (
    tid        INTEGER PRIMARY KEY,
    title      TEXT NOT NULL,
    author     TEXT DEFAULT '',
    reply_num  INTEGER DEFAULT 0,
    is_good    INTEGER DEFAULT 0,
    is_top     INTEGER DEFAULT 0,
    first_seen TEXT DEFAULT (datetime('now','localtime')),
    last_seen  TEXT DEFAULT (datetime('now','localtime')),
    fetched    INTEGER DEFAULT 0,
    fetched_at TEXT
);
CREATE TABLE IF NOT EXISTS posts (
    pid        INTEGER PRIMARY KEY,
    tid        INTEGER NOT NULL,
    floor      INTEGER NOT NULL,
    author     TEXT DEFAULT '',
    content    TEXT DEFAULT '',
    post_time  TEXT DEFAULT '',
    is_lzl     INTEGER DEFAULT 0,
    fetched_at TEXT DEFAULT (datetime('now','localtime'))
);
CREATE INDEX IF NOT EXISTS idx_posts_tid ON posts(tid);
CREATE INDEX IF NOT EXISTS idx_posts_lzl ON posts(pid) WHERE is_lzl=0;
CREATE TABLE IF NOT EXISTS meta (
    key   TEXT PRIMARY KEY,
    value TEXT
);
"""


class Store:
    def __init__(self, db_path: Path):
        db_path.parent.mkdir(parents=True, exist_ok=True)
        self.conn = sqlite3.connect(db_path)
        self.conn.executescript(SCHEMA)
        self.conn.commit()

    def upsert_threads(self, rows: list[dict]) -> int:
        now = time.strftime("%Y-%m-%d %H:%M:%S")
        cur = self.conn.cursor()
        new_cnt = 0
        for r in rows:
            cur.execute(
                """INSERT INTO threads(tid,title,author,reply_num,is_good,is_top,last_seen)
                   VALUES(?,?,?,?,?,?,?)
                   ON CONFLICT(tid) DO UPDATE SET
                     title=excluded.title,
                     author=COALESCE(NULLIF(excluded.author,''),threads.author),
                     reply_num=MAX(COALESCE(threads.reply_num,0),excluded.reply_num),
                     is_good=MAX(threads.is_good,excluded.is_good),
                     is_top=MAX(threads.is_top,excluded.is_top),
                     last_seen=excluded.last_seen""",
                (r["tid"], r["title"], r.get("author", ""), r.get("reply_num", 0),
                 r.get("is_good", 0), r.get("is_top", 0), now),
            )
            if cur.rowcount == 1:
                new_cnt += 1
        self.conn.commit()
        return new_cnt

    def new_unfetched_tids(self) -> list[int]:
        cur = self.conn.execute(
            "SELECT tid FROM threads WHERE fetched=0 AND is_top=0 ORDER BY tid DESC"
        )
        return [r[0] for r in cur.fetchall()]

    def mark_fetched(self, tid: int, title: str, reply_num: int) -> None:
        self.conn.execute(
            """UPDATE threads SET fetched=1, fetched_at=datetime('now','localtime'),
               title=?, reply_num=MAX(COALESCE(reply_num,0),?) WHERE tid=?""",
            (title, reply_num, tid),
        )
        self.conn.commit()

    def replace_posts(self, tid: int, floors: list[dict]) -> None:
        cur = self.conn.cursor()
        # 楼中楼无独立 pid，只保留主楼 pid 作主键；楼中楼以 (tid,floor) 虚拟主键
        cur.execute("DELETE FROM posts WHERE tid=?", (tid,))
        now = time.strftime("%Y-%m-%d %H:%M:%S")
        seq = 0
        for f in floors:
            seq += 1
            if f["is_lzl"]:
                cur.execute(
                    """INSERT INTO posts(pid,tid,floor,author,content,post_time,is_lzl,fetched_at)
                       VALUES(?,?,?,?,?,?,1,?)""",
                    (-seq, tid, f["floor"], f["author"], f["content"], f["post_time"], now),
                )
            else:
                cur.execute(
                    """INSERT OR REPLACE INTO posts(pid,tid,floor,author,content,post_time,is_lzl,fetched_at)
                       VALUES(?,?,?,?,?,?,0,?)""",
                    (f["pid"] or -seq, tid, f["floor"], f["author"], f["content"],
                     f["post_time"], now),
                )
        self.conn.commit()

    def search(self, kw: str, limit: int, scope: str) -> list[dict]:
        like = f"%{kw}%"
        out: list[dict] = []
        if scope in ("thread", "all"):
            for row in self.conn.execute(
                """SELECT tid,title,is_good,is_top,fetched FROM threads
                   WHERE title LIKE ? ORDER BY tid DESC LIMIT ?""",
                (like, limit),
            ):
                out.append({"type": "thread", "tid": row[0], "title": row[1],
                            "is_good": row[2], "is_top": row[3], "fetched": row[4]})
        if scope in ("post", "all"):
            for row in self.conn.execute(
                """SELECT p.tid,t.title,p.floor,p.author,p.content,p.is_lzl
                   FROM posts p JOIN threads t ON t.tid=p.tid
                   WHERE p.content LIKE ? ORDER BY p.tid DESC, p.floor LIMIT ?""",
                (like, limit),
            ):
                out.append({"type": "post", "tid": row[0], "title": row[1],
                            "floor": row[2], "author": row[3],
                            "content": row[4][:200], "is_lzl": row[5]})
        return out

    def stats(self) -> dict:
        one = lambda sql: self.conn.execute(sql).fetchone()[0]  # noqa: E731
        return {
            "threads": one("SELECT COUNT(*) FROM threads"),
            "threads_fetched": one("SELECT COUNT(*) FROM threads WHERE fetched=1"),
            "posts": one("SELECT COUNT(*) FROM posts"),
            "good_threads": one("SELECT COUNT(*) FROM threads WHERE is_good=1"),
            "db_last_sync": self.conn.execute(
                "SELECT value FROM meta WHERE key='last_sync'"
            ).fetchone(),
        }


# --------------------------------------------------------------------------- #
# 后端抽象：web = 移动端 SSR 抓 HTML（零依赖）；client = aiotieba 客户端 API（可选）
# --------------------------------------------------------------------------- #

def _uname(user: object) -> str:
    for attr in ("user_name", "nick_name", "show_name"):
        v = getattr(user, attr, "") or ""
        if v:
            return str(v)
    return ""


def _fmt_ts(ts: int) -> str:
    if not ts:
        return ""
    return time.strftime("%Y-%m-%d %H:%M", time.localtime(int(ts)))


class WebBackend:
    """移动端 SSR 通道。零依赖；缺点：桌面风控更敏感，末页会重复返回需自行去重。"""

    def __init__(self, proxy: str | None = None, delay: float = 1.5):
        self.sess = TiebaSession(proxy=proxy, delay=delay)

    def fetch_threads(self, forum: str, pages: int, good_pages: int) -> list[dict]:
        self.sess.warmup()
        kw = urllib.parse.quote(forum)
        rows: list[dict] = []
        for label, base, npages in (
            ("最新", f"https://tieba.baidu.com/mo/q/m?kw={kw}&pn=", pages),
            ("精华", f"https://tieba.baidu.com/mo/q/m?kw={kw}&lm=4&pn=", good_pages),
        ):
            for pn in range(npages):
                page = self.sess.get(base + str(pn))
                got = parse_forum_page(page)
                print(f"[{label}] 第 {pn} 页: {len(got)} 条")
                if not got:
                    break
                if label == "精华":
                    for r in got:
                        r["is_good"] = 1
                rows.extend(got)
        return rows

    def fetch_thread(self, tid: int, max_pages: int = 50,
                     with_lzl: bool = False) -> tuple[str, list[dict]]:
        """with_lzl 忽略：web SSR 页面自带楼中楼，无需额外请求。"""
        all_floors: list[dict] = []
        seen_sigs: set[str] = set()
        title, total_page = "", None
        for pn in range(1, max_pages + 1):
            page = self.sess.get(f"https://tieba.baidu.com/mo/q/m?kz={tid}&pn={pn}")
            if pn == 1:
                title, _ = parse_thread_page(page)
            total = total_page_hint(page)
            if total:
                total_page = total
            _, floors = parse_thread_page(page)
            main = [f for f in floors if not f["is_lzl"]]
            sig = "|".join(f"{f['pid']}:{f['content'][:40]}" for f in main)
            if sig and sig in seen_sigs:
                break  # 末页被服务器重复返回
            seen_sigs.add(sig)
            all_floors.extend(floors)
            if not floors:
                break
            if total_page and pn >= total_page:
                break
        return title, all_floors


class ClientBackend:
    """aiotieba 客户端 API 通道（c.tieba.baidu.com protobuf）。

    结构化数据、自带精华区/总页数/楼中楼，且与 Web 风控体系不同，推荐。
    需要：python3 -m venv .venv && .venv/bin/pip install aiotieba
    """

    def __init__(self, proxy: str | None = None):
        import aiotieba
        from aiotieba.enums import ThreadSortType

        self._aio = aiotieba
        # 最新列表按“发表时间”排序，保证新主题必然落在前几页
        self._sort = ThreadSortType.CREATE
        if proxy:
            os.environ.setdefault("HTTP_PROXY", proxy)
            os.environ.setdefault("HTTPS_PROXY", proxy)

    def _client(self):
        return self._aio.Client(proxy=bool(os.environ.get("HTTPS_PROXY")))

    def fetch_threads(self, forum: str, pages: int, good_pages: int) -> list[dict]:
        async def go():
            rows: list[dict] = []
            async with self._client() as c:
                for pn in range(1, pages + 1):
                    ts = await c.get_threads(forum, pn=pn, sort=self._sort)
                    if ts.err:
                        raise RuntimeError(f"get_threads 失败: {ts.err}")
                    got = self._thread_rows(ts, good=False)
                    print(f"[最新] 第 {pn - 1} 页: {len(got)} 条")
                    rows.extend(got)
                    if not ts.has_more:
                        break
                for pn in range(1, good_pages + 1):
                    ts = await c.get_threads(forum, pn=pn, sort=self._sort, is_good=True)
                    if ts.err:
                        raise RuntimeError(f"get_threads(精华) 失败: {ts.err}")
                    got = self._thread_rows(ts, good=True)
                    print(f"[精华] 第 {pn - 1} 页: {len(got)} 条")
                    rows.extend(got)
                    if not ts.has_more:
                        break
            return rows

        return asyncio.run(go())

    @staticmethod
    def _thread_rows(ts: object, good: bool) -> list[dict]:
        rows = []
        for t in ts.objs:
            rows.append(
                {
                    "tid": int(t.tid),
                    "title": (t.title or "").strip() or (t.text or "")[:60].strip(),
                    "is_good": 1 if (good or getattr(t, "is_good", False)) else 0,
                    "is_top": 1 if getattr(t, "is_top", False) else 0,
                    "author": _uname(t.user),
                    "reply_num": int(getattr(t, "reply_num", 0) or 0),
                }
            )
        return rows

    def fetch_thread(self, tid: int, max_pages: int = 50,
                     with_lzl: bool = False) -> tuple[str, list[dict]]:
        async def go():
            title, floors = "", []
            async with self._client() as c:
                for pn in range(1, max_pages + 1):
                    ps = await c.get_posts(tid, pn=pn)
                    if ps.err:
                        raise RuntimeError(f"get_posts 失败: {ps.err}")
                    if pn == 1 and getattr(ps, "thread", None):
                        title = (ps.thread.title or "").strip()
                    for p in ps.objs:
                        floors.append(
                            {"pid": int(p.pid), "floor": float(p.floor),
                             "author": _uname(p.user), "content": (p.text or "").strip(),
                             "post_time": _fmt_ts(p.create_time), "is_lzl": 0}
                        )
                        if with_lzl:
                            floors.extend(await self._fetch_lzl(c, tid, int(p.pid), float(p.floor)))
                    if not ps.has_more:
                        break
                    tp = getattr(ps.page, "total_page", 0) or 0
                    if tp and pn >= tp:
                        break
            if not floors:
                raise RuntimeError("未取到任何楼层")
            return title, floors

        return asyncio.run(go())

    async def _fetch_lzl(self, c, tid: int, pid: int, parent_floor: float) -> list[dict]:
        """楼中楼是独立接口，逐楼层拉取请求数放大明显，仅 --lzl 时启用。"""
        out: list[dict] = []
        for pn in range(1, 4):  # 楼中楼超 3 页按截断处理
            cs = await c.get_comments(tid, pid, pn=pn)
            if cs.err or not cs.objs:
                break
            for k, cm in enumerate(cs.objs, 1):
                out.append(
                    {"pid": 0, "floor": parent_floor + (k + (pn - 1) * 10) / 1000.0,
                     "author": _uname(cm.user), "content": (cm.text or "").strip(),
                     "post_time": _fmt_ts(getattr(cm, "create_time", 0) or 0),
                     "is_lzl": 1}
                )
            if not cs.has_more:
                break
        return out


def make_backend(args: argparse.Namespace):
    mode = args.backend
    if mode == "auto":
        try:
            import aiotieba  # noqa: F401
            mode = "client"
        except ImportError:
            # 本体无依赖但 .venv 装了 aiotieba → 自动换解释器重执行一次
            venv_py = Path(__file__).resolve().parent / ".venv" / "bin" / "python"
            if venv_py.exists() and os.environ.get("TIEBA_VENV_REEXEC") != "1":
                os.environ["TIEBA_VENV_REEXEC"] = "1"
                os.execv(str(venv_py), [str(venv_py), str(Path(__file__).resolve()), *sys.argv[1:]])
            mode = "web"
            print("[info] 未装 aiotieba（.venv），使用 web 通道（详见 README）", file=sys.stderr)
    if mode == "client":
        return ClientBackend(proxy=args.proxy)
    return WebBackend(proxy=args.proxy, delay=args.delay)


def sync_thread(store: Store, backend, tid: int, max_pages: int,
                with_lzl: bool = False) -> int:
    """抓一个主题并入库，返回楼层数。"""
    title, floors = backend.fetch_thread(tid, max_pages=max_pages, with_lzl=with_lzl)
    store.replace_posts(tid, floors)
    store.mark_fetched(tid, title, 0)
    return len(floors)


def cmd_update(args: argparse.Namespace) -> None:
    backend = make_backend(args)
    store = Store(Path(args.db))
    rows = backend.fetch_threads(args.forum, args.pages, args.good_pages)
    new_cnt = store.upsert_threads(rows)
    store.conn.execute(
        "INSERT INTO meta(key,value) VALUES('last_sync',?) "
        "ON CONFLICT(key) DO UPDATE SET value=excluded.value",
        (time.strftime("%Y-%m-%d %H:%M:%S"),),
    )
    store.conn.commit()
    print(f"列表合计 {len(rows)} 条，新入库 {new_cnt} 条")

    if args.no_fetch:
        return
    tids = store.new_unfetched_tids()
    if args.max_threads > 0:
        tids = tids[: args.max_threads]
    print(f"待抓详情 {len(tids)} 篇")
    for i, tid in enumerate(tids, 1):
        try:
            n = sync_thread(store, backend, tid, args.max_pages, with_lzl=args.lzl)
            print(f"  ({i}/{len(tids)}) tid={tid} 楼层数 {n}")
        except (RuntimeError, OSError) as e:
            print(f"  ({i}/{len(tids)}) tid={tid} 失败: {e}", file=sys.stderr)


def cmd_thread(args: argparse.Namespace) -> None:
    backend = make_backend(args)
    store = Store(Path(args.db))
    for tid in args.tid:
        n = sync_thread(store, backend, tid, args.max_pages, with_lzl=args.lzl)
        print(f"tid={tid} 已入库，楼层数 {n}")


def cmd_search(args: argparse.Namespace) -> None:
    store = Store(Path(args.db))
    hits = store.search(args.keyword, limit=args.limit, scope=args.scope)
    if args.json:
        print(json.dumps(hits, ensure_ascii=False, indent=2))
        return
    if not hits:
        print(f"无匹配：{args.keyword}")
        return
    for h in hits:
        if h["type"] == "thread":
            tag = ("[精]" if h["is_good"] else "") + ("[顶]" if h["is_top"] else "")
            state = "" if h["fetched"] else " (未抓详情, 可: thread %d)" % h["tid"]
            print(f"[主题 {h['tid']}]{tag} {h['title']}{state}")
        else:
            lzl = "楼中楼 " if h["is_lzl"] else ""
            snippet = h["content"]
            pos = snippet.find(args.keyword)
            start = max(0, pos - 30)
            ctx = ("…" if start else "") + snippet[start : pos + 60] + ("…" if pos + 60 < len(snippet) else "")
            print(f"[{h['tid']} {lzl}{h['floor']:g}F {h['author']}] {ctx}")


def cmd_show(args: argparse.Namespace) -> None:
    store = Store(Path(args.db))
    t = store.conn.execute(
        "SELECT title,reply_num,fetched FROM threads WHERE tid=?", (args.tid,)
    ).fetchone()
    if not t:
        print(f"tid={args.tid} 不在库中，先 update 或 thread 命令抓取", file=sys.stderr)
        sys.exit(1)
    print(f"# {t[0]}  (tid={args.tid}, 回复≈{t[1]})")
    rows = store.conn.execute(
        "SELECT floor,author,content,post_time,is_lzl FROM posts WHERE tid=? ORDER BY floor",
        (args.tid,),
    ).fetchall()
    for floor, author, content, ptime, is_lzl in rows:
        prefix = "    └ " if is_lzl else f"[{floor:g}F] "
        ts = f"  {ptime}" if ptime else ""
        print(f"{prefix}{author}{ts}: {content}")


def cmd_stats(args: argparse.Namespace) -> None:
    s = Store(Path(args.db)).stats()
    print(f"主题总数     : {s['threads']}（已抓详情 {s['threads_fetched']}，精华 {s['good_threads']}）")
    print(f"楼层/回复总数: {s['posts']}")
    print(f"最近同步     : {s['db_last_sync'][0] if s['db_last_sync'] else '从未'}")


def _add_common(p: argparse.ArgumentParser) -> None:
    """公共参数双位置可用（全局或子命令内），SUPPRESS 保证子命令未给时不覆盖全局值。"""
    p.add_argument("--db", default=argparse.SUPPRESS, help="SQLite 路径（默认 tools/tieba/data/tieba.db）")
    p.add_argument("--proxy", default=argparse.SUPPRESS,
                   help="HTTP 代理，如 http://127.0.0.1:7897（或环境变量 TIEBA_PROXY）")
    p.add_argument("--delay", type=float, default=argparse.SUPPRESS, help="请求间隔秒数（默认 1.5）")
    p.add_argument("--forum", default=argparse.SUPPRESS, help="吧名（默认 赛尔号）")
    p.add_argument("--backend", choices=["auto", "client", "web"], default=argparse.SUPPRESS,
                   help="client=aiotieba 客户端API（推荐，需 .venv）；web=移动端SSR抓HTML（零依赖）")


def main(argv: list[str] | None = None) -> None:
    ap = argparse.ArgumentParser(description="贴吧赛尔号吧抓取与检索")
    ap.add_argument("--db", default=str(DEFAULT_DB), help="SQLite 路径（默认 tools/tieba/data/tieba.db）")
    ap.add_argument("--proxy", default=None, help="HTTP 代理，如 http://127.0.0.1:7897（或环境变量 TIEBA_PROXY）")
    ap.add_argument("--delay", type=float, default=1.5, help="请求间隔秒数（默认 1.5）")
    ap.add_argument("--forum", default=DEFAULT_FORUM, help="吧名（默认 赛尔号）")
    ap.add_argument("--backend", choices=["auto", "client", "web"], default="auto",
                    help="client=aiotieba 客户端API（推荐，需 .venv）；web=移动端SSR抓HTML（零依赖）")
    sub = ap.add_subparsers(dest="cmd", required=True)

    p = sub.add_parser("update", help="抓最新列表+精华并补新帖详情")
    _add_common(p)
    p.add_argument("--pages", type=int, default=2, help="最新列表页数")
    p.add_argument("--good-pages", type=int, default=1, help="精华区页数")
    p.add_argument("--no-fetch", action="store_true", help="只更新列表不抓详情")
    p.add_argument("--max-threads", type=int, default=15, help="单次 update 最多补抓多少篇详情")
    p.add_argument("--max-pages", type=int, default=50, help="单篇详情最多翻页数")
    p.add_argument("--lzl", action="store_true", help="补抓楼中楼（仅 client 后端，请求数放大明显）")
    p.set_defaults(func=cmd_update)

    p = sub.add_parser("thread", help="抓指定主题（全部楼层）")
    _add_common(p)
    p.add_argument("tid", type=int, nargs="+")
    p.add_argument("--max-pages", type=int, default=50)
    p.add_argument("--lzl", action="store_true", help="补抓楼中楼（仅 client 后端）")
    p.set_defaults(func=cmd_thread)

    p = sub.add_parser("search", help="本地检索（标题+正文，LIKE）")
    p.add_argument("keyword")
    p.add_argument("--scope", choices=["all", "thread", "post"], default="all")
    p.add_argument("--limit", type=int, default=30)
    p.add_argument("--json", action="store_true", help="机器可读输出")
    p.set_defaults(func=cmd_search)

    p = sub.add_parser("show", help="终端阅读主题")
    _add_common(p)
    p.add_argument("tid", type=int)
    p.set_defaults(func=cmd_show)

    p = sub.add_parser("stats", help="库内概况")
    _add_common(p)
    p.set_defaults(func=cmd_stats)

    args = ap.parse_args(argv)
    try:
        args.func(args)
    except KeyboardInterrupt:
        sys.exit(130)
    except (RuntimeError, OSError) as e:
        print(f"[error] {e}", file=sys.stderr)
        sys.exit(2)


if __name__ == "__main__":
    main()
