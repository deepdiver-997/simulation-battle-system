# tieba-intel — 贴吧「赛尔号吧」抓取与检索

为 agent 工作流提供游戏情报输入：新版本公告、策划动向、玩家对精灵/魂印机制的讨论、
卡关攻略等。双后端设计：

- **client 后端（推荐）**：[aiotieba](https://github.com/Starry-OvO/aiotieba) 客户端 API
  （`c.tieba.baidu.com` protobuf）。结构化数据、自带精华区/总页数/楼中楼接口，与 Web
  风控体系不同（Web 被封时通常仍可用）。需装依赖（Python 3.14 实测 4.7.1 可用）。
- **web 后端（零依赖）**：移动端 SSR 页面（`/mo/q/m`）正则解析，仅标准库。

`--backend auto`（默认）：本体尝试 import aiotieba，失败则自动换 `.venv/bin/python`
重执行一次，仍不可用则回退 web。

## 数据通道

client（aiotieba）：`get_threads(forum, pn, sort, is_good)` / `get_posts(tid, pn)` /
`get_comments(tid, pid)`。

web（已实测可用，2026-09）：

| 用途 | URL |
|------|-----|
| 吧主题列表 | `https://tieba.baidu.com/mo/q/m?kw=<吧名>&pn=<页从0起>` |
| 精华区 | `https://tieba.baidu.com/mo/q/m?kw=<吧名>&lm=4&pn=<页>` |
| 主题详情 | `https://tieba.baidu.com/mo/q/m?kz=<tid>&pn=<页从1起>` |

web 通道前置：先访问 `https://www.baidu.com/` 预热拿 `BAIDUID` cookie（工具自动做；
手机 UA 直访贴吧首页会 403）。桌面版 `/p/` 页面是 JS 壳不可用。
停页依据：详情页内嵌 JSON `"total_page":N` + 主楼签名去重（末页会被服务器重复返回）。

## 安装（可选，只为 client 后端）

```bash
cd tools/tieba
python3 -m venv .venv
./.venv/bin/pip install aiotieba
```

## 用法

```bash
cd tools/tieba

# 日常增量：抓最新 2 页 + 精华 1 页，新帖自动补详情（最多 15 篇）
python3 tieba_intel.py update

# 只更新列表，不抓详情 / 补抓楼中楼（仅 client 后端，请求数放大明显）
python3 tieba_intel.py update --no-fetch
python3 tieba_intel.py thread <tid> --lzl

# 手动抓指定主题全部楼层（可多个）
python3 tieba_intel.py thread 11037204969

# 本地检索（标题+正文，LIKE；agent 建议加 --json）
python3 tieba_intel.py search 天启 --json
python3 tieba_intel.py search 魂印 --scope thread --limit 10

# 终端阅读 / 库内概况
python3 tieba_intel.py show 11037204969
python3 tieba_intel.py stats
```

全局参数（放子命令前后均可）：`--db`（默认 `data/tieba.db`）、`--backend`
（`auto|client|web`）、`--proxy`（如 `http://127.0.0.1:7897`，或环境变量
`TIEBA_PROXY`）、`--delay`（web 后端请求间隔秒，默认 1.5）、`--forum`（默认
`赛尔号`，可抓其他吧）。

注意排序口径：client 后端「最新」按**发表时间**排序（保证新主题必落前几页），
web 后端是贴吧默认的回复序。

## 风控须知（重要）

- 百度对高频访问做临时封锁（403/安全验证页），**可能持续 1 小时以上**。直连与代理
  出口会分别被记账——一边被封时换另一边（`--proxy http://127.0.0.1:7897`）。
- web 后端触发封锁后等自愈即可，**不要加密探测频率**（可能刷新风控标记）；client
  后端不受 Web 风控影响。
- web 后端检测到风控页会立刻报错退出而不是静默存脏数据。client 后端默认参数
  （2+1 页 + ≤15 篇详情）实测稳定。

## 数据模型（SQLite）

- `threads(tid, title, author, reply_num, is_good, is_top, fetched, first_seen, last_seen…)`
- `posts(pid, tid, floor, author, content, post_time, is_lzl)` — 楼层；`is_lzl=1` 为楼中楼
  （floor 用 `父楼层+k/1000` 保持排序）
- `meta(key, value)` — 最近同步时间

`data/`、`.venv/` 已 gitignore，不进版本库。
