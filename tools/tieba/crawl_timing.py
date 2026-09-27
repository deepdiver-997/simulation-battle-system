#!/usr/bin/env python3
"""低频爬取赛尔号吧：登场先后/压秒/房主挑战方/启灵免控 相关讨论 → data/tieba.db

search_exact 返回帖子级命中（正文片段），直接落库即可；必要时再对少数 tid 抓整帖。
节奏控制（防封 IP）：请求间 sleep 8~12s 随机抖动；总请求数硬上限 12。
"""
import asyncio
import random
import sqlite3
import sys
import time

import aiotieba

DB = "data/tieba.db"
FORUM = "赛尔号"
QUERIES = ["压秒", "登场顺序", "启灵元神 免控", "房主 挑战方", "登场先后"]
MAX_REQUESTS = 12

req = {"n": 0}


def store_post(conn, tid, pid, title, author, text):
    conn.execute(
        "INSERT OR IGNORE INTO posts(pid,tid,floor,author,content) VALUES(?,?,?,?,?)",
        (pid, tid, 0, author, f"《{title}》{text}"),
    )


async def main():
    conn = sqlite3.connect(DB)
    total = 0
    async with aiotieba.Client() as client:
        for query in QUERIES:
            if req["n"] >= MAX_REQUESTS:
                break
            req["n"] += 1
            print(f"== 搜索: {query}", flush=True)
            try:
                result = await client.search_exact(FORUM, query)
            except Exception as exc:  # noqa: BLE001
                print(f"  搜索失败: {exc!r}", flush=True)
                time.sleep(random.uniform(8, 12))
                continue
            n = 0
            for hit in result.objs:
                if not hit.text.strip():
                    continue
                store_post(conn, hit.tid, hit.pid, hit.title, hit.show_name, hit.text)
                n += 1
            conn.commit()
            total += n
            print(f"  命中 {n} 条", flush=True)
            time.sleep(random.uniform(8, 12))
    conn.close()
    print(f"完成：{req['n']} 个请求，{total} 条帖子")


if __name__ == "__main__":
    sys.exit(asyncio.run(main()))
