#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_wiki_citations.py — 百科机校脚本第一版（看板单 S7，2026-11-03）。

对 docs_local/docs/10-游戏百科/*.md 做四项自动校验（S6 试点结论的脚本化，
规范源：docs_local/docs/10-游戏百科/00-编写规范与导航.md §三「机校清单」）：

  1.〔源: …〕引注校验
     - 「路径:行」类（.h/.cpp/.md/…）：文件存在 + 行号不越界（支持单行 / a-b 区间 /
       逗号列表 / 「+」并列多源 / 〔源: 场景NNN 头注 + :31-33〕式裸行号续接）。
     - 「场景NNN …」：test/scenario/scenario_NNN_*.cpp 存在性（硬校验）；
       带行号（场景045:368-371、头注：10-15 行）时同样查越界。
     - 「官方原文 Lxxx」：reference/scrape/out/all_posts_final.jsonl 行存在（硬）/非空（软）。
     - 「协议文档§N.N」「速查§X」「夜跑晨报-…§X」：对应文件存在（硬）、节标题定位（软，
       定位不到只告警）；「K 系文件头」「…行注」「路径:行」（规范章元描述）仅格式认可。
  2. 效果 id 存在性：正文的 effect/Eid/效果 id/K 系/魂印 id 现查
     sqlite side_effect（moves 空间）与 effect_icon（soul 空间），registry.json
     moves/soul 码表兜底；四处皆无 → 错误；仅 registry 有、sqlite 两表皆无 → 警告（漂移）。
     effect_des id（「effect_des NNN」）单独对 effect_des 表核。
  3. anomalies 码表对齐：正文「| id | 名称 |…」码表 vs build/registry.json anomalies
     全量 diff（正文多出 / 缺漏 / 名称不一致均报）。
  4. 待验证清单汇总：抽各行「⚠待验证」行内标记 + 章末「待验证清单」表行，
     汇总输出（供人类只扫清单、不通读全文）。

用法：
  python3 tools/check_wiki_citations.py            # 校验 <root>/docs_local/docs/10-游戏百科/*.md
  python3 tools/check_wiki_citations.py --quiet    # gate 接入：只出错误/警告/汇总行
  python3 tools/check_wiki_citations.py --root /path/to/checkout
      # 在 worktree 里跑、而 build/registry.json、sqlite、reference/ 等数据在另一 checkout 时
      # （worktree 常缺 reference/（不入库）与 build/registry.json（需构建 dump））

参数（默认均相对 --root 解析，--root 默认 = 本脚本所在仓库根）：
  --wiki-dir    百科目录      docs_local/docs/10-游戏百科
  --registry    registry dump build/registry.json
  --db          官方库        scripts/data/processed/seer_unity.sqlite
  --jsonl       官方原文抓取  reference/scrape/out/all_posts_final.jsonl
  --scenario-dir 场景目录     test/scenario
  --fails       回归失败清单  build/night-regression-fails.txt（缺文件则跳过软校验）

退出码：0 = 无错误（警告不算）；1 = 有错引；2 = 输入数据缺失（目录/registry/db/jsonl）。

接入说明（是否挂 gate.sh 尾部由编排白班裁决，本次交付为独立可跑）：
    # gate.sh 尾部可选：
    if ls docs_local/docs/10-游戏百科/*.md >/dev/null 2>&1; then
        python3 tools/check_wiki_citations.py --quiet || exit 1
    fi
  「仅百科目录有改动时才校验」的细化判断由接入方负责；night-regression-fails.txt
  为空/缺省时场景真绿软校验自动跳过，不重跑 gate。
"""

from __future__ import annotations

import argparse
import json
import re
import sqlite3
import sys
from pathlib import Path

# ---------------------------------------------------------------- 标记正则

MARK_RE = re.compile(r"〔源:\s*(.*?)〕")
PART_SPLIT_RE = re.compile(r"\s*[+＋]\s*")

# 规范章里的元描述（描述标记格式本身，不是真引注）
META_VOCAB = {"路径:行"}

PATH_RE = re.compile(
    r"^(?P<path>[^\s:：]+?\.(?:h|hpp|hh|cpp|cc|cxx|md|txt|json|jsonl|py|sql))(?P<rest>.*)$"
)
LEAD_LINES_RE = re.compile(r"^\s*[:：]\s*(?P<lines>\d[\d,\-–—]*)")
ANY_LINES_RE = re.compile(r"[:：]\s*(?P<lines>\d[\d,\-–—]*)\s*行?")
BARE_LINES_RE = re.compile(r"^[:：]\s*(?P<lines>\d[\d,\-–—]*)$")

SCEN_RE = re.compile(r"^场景\s*(?P<no>\d+)(?P<rest>.*)$")
OFFICIAL_RE = re.compile(r"^官方原文\s*(?P<ls>L\d+(?:\s*[/、]\s*L\d+)*)(?P<rest>.*)$")
EDES_RE = re.compile(r"^(?:官方\s*)?effect_des\s*(?P<ids>\d+(?:\s*[/、]\s*\d+)*)(?P<rest>.*)$")
PROTO_RE = re.compile(r"^协议文档\s*§(?P<sec>[^\s〔〕]+)")
SPEED_RE = re.compile(r"^速查\s*§(?P<sec>[^\s〔〕]+)")
MORNING_RE = re.compile(
    r"^夜跑晨报-(?P<name>\S+?)(?:\s*§(?P<sec>[^\s〔〕]+))?(\s(?P<rest>\S.*))?$")
KHEAD_RE = re.compile(r"^K\d+\s*文件头$")
LINENOTE_RE = re.compile(r"^\d+\s*行注$")
# 「技能判定流程§8.8」式——库内文档短名 + §节（在 速查/协议文档/晨报 专名分支之后兜底）
SHORT_DOC_RE = re.compile(r"^(?P<name>[^\s:：〔〕+＋]{2,20})§(?P<sec>[^\s〔〕]+)(?P<rest>.*)$")

# ---------------------------------------------------------------- 效果 id 正则
# 先宽后紧（看板单要求）：只认带前缀词的 id，裸数字一律不抓（防把普通数值当效果 id）。

EFF_RE = re.compile(r"\beffect(?:\s+id)?\s*[:：]?\s*(\d+(?:\s*[/、]\s*\d+)*)")
XIAOGUO_ID_RE = re.compile(r"效果\s*id\s*[:：]?\s*(\d+(?:\s*[/、]\s*\d+)*)")
EID_RE = re.compile(r"\bEid\s*[:：]?\s*(\d+(?:\s*[/、]\s*\d+)*)")
EDES_ID_RE = re.compile(r"\beffect_des\s*[:：]?\s*(\d+(?:\s*[/、]\s*\d+)*)")
K_ID_RE = re.compile(r"\bK\d+(?:\s*的|\s*族\s*|\s+)(\d+(?:\s*[/、]\s*\d+)*)")
HUNYIN_RE = re.compile(r"魂印\s*(\d+)\b")

SCEN_FILE_RE = re.compile(r"scenario[_\- ]?(\d+)")
HEADING_RE = re.compile(r"^#{1,6}\s*(?P<title>\S.*)$")
TABLE_ROW_SPLIT_RE = re.compile(r"\|")
ZHUZHANG_RE = re.compile(r"^\s*\|\s*\d+\s*\|")  # 章末待验证清单表行


def split_lines_token(tok: str) -> list[tuple[int, int]]:
    """'109' / '159-171' / '19,52' / '126-137,162' → [(a,b), …]（1 起）。"""
    out = []
    for piece in re.split(r"[,，]", tok):
        piece = piece.strip().replace("–", "-").replace("—", "-")
        if not piece:
            continue
        if "-" in piece:
            a, b = piece.split("-", 1)
            out.append((int(a), int(b)))
        else:
            out.append((int(piece), int(piece)))
    return out


class Report:
    """错误/警告收集 + 输出。quiet 模式下即时错误仍输出（gate 要看得见）。"""

    def __init__(self, quiet: bool) -> None:
        self.quiet = quiet
        self.errors = 0
        self.warns = 0
        self._pending: list[str] = []
        self._seen: set[tuple[str, str, str]] = set()

    def _emit(self, level: str, where: str, msg: str) -> None:
        key = (level, where, msg)
        if key in self._seen:  # 同一条问题可能被「引注解析 + 全行 id 扫描」双路径命中
            return
        self._seen.add(key)
        line = f"  [{level}] {where} {msg}"
        if self.quiet:
            print(line)
        else:
            self._pending.append(line)

    def err(self, where: str, msg: str) -> None:
        self.errors += 1
        self._emit("错误", where, msg)

    def warn(self, where: str, msg: str) -> None:
        self.warns += 1
        self._emit("警告", where, msg)

    def flush_section(self) -> None:
        for line in self._pending:
            print(line)
        self._pending = []


def check_lines(rep: Report, where: str, target: str, nlines: int, tok: str) -> None:
    """行号不越界校验：区间 a-b 要求 1<=a<=b<=nlines。"""
    for a, b in split_lines_token(tok):
        if a < 1 or b < a:
            rep.err(where, f"行号区间非法 {a}-{b}（{target}，共 {nlines} 行）")
        elif b > nlines:
            rep.err(
                where,
                f"行号越界：{target} 引 L{a}-{b} 但文件只有 {nlines} 行"
                if a != b
                else f"行号越界：{target} 引 L{a} 但文件只有 {nlines} 行",
            )


class WikiChecker:
    def __init__(self, opt: argparse.Namespace) -> None:
        self.opt = opt
        self.rep = Report(opt.quiet)
        self.root = Path(opt.root).resolve()
        self.wiki_dir = Path(opt.wiki_dir)
        self.scenario_dir = Path(opt.scenario_dir)
        self.registry_path = Path(opt.registry)
        self.db_path = Path(opt.db)
        self.jsonl_path = Path(opt.jsonl)
        self.fails_path = Path(opt.fails)

        # 数据（延迟加载）
        self.registry: dict | None = None
        self.space_ids: dict[str, set[int]] = {}
        self.jsonl_lines: list[str] | None = None
        self.fail_scens: set[str] = set()

        # 统计
        self.mark_tally = {"路径行": 0, "场景": 0, "官方原文": 0, "库内文档": 0, "元描述": 0}
        self.cited_scens: set[str] = set()
        self.id_hits: list[tuple[str, int, str, str]] = []  # (file, line, pattern, ids)
        self.anomaly_table: dict[int, str] = {}
        self.anomaly_table_src: dict[int, str] = {}
        self.pending_items: list[tuple[str, int, str]] = []  # 行内 ⚠待验证
        self.pending_chapter: list[tuple[str, str, str]] = []  # 章末清单行
        self.missing_jsonl = False  # jsonl 缺失但正文有 L 引 → 属数据缺失，退出码 2 而非 1
        self.missing_jsonl_count = 0

    # ---------------------------------------------------------- 数据加载

    def load_data(self) -> None:
        if not self.wiki_dir.is_dir():
            print(f"[数据缺失] 百科目录不存在：{self.wiki_dir}", file=sys.stderr)
            print("提示：用 --wiki-dir 或 --root 指定。", file=sys.stderr)
            sys.exit(2)
        if not self.registry_path.is_file():
            print(f"[数据缺失] registry.json 不存在：{self.registry_path}", file=sys.stderr)
            print("提示：build/registry.json 由构建 dump 生成；worktree 里跑请加 "
                  "--root /path/to/主checkout 或 --registry 指向已有 dump。", file=sys.stderr)
            sys.exit(2)
        if not self.db_path.is_file():
            print(f"[数据缺失] sqlite 库不存在：{self.db_path}", file=sys.stderr)
            sys.exit(2)
        try:
            self.registry = json.loads(self.registry_path.read_text(encoding="utf-8"))
        except (OSError, ValueError) as e:
            print(f"[数据损坏] registry.json 解析失败：{e}", file=sys.stderr)
            sys.exit(2)
        con = sqlite3.connect(f"file:{self.db_path}?mode=ro", uri=True)
        try:
            self.space_ids = {
                "moves(side_effect)": {r[0] for r in con.execute("select id from side_effect")},
                "soul(effect_icon)": {r[0] for r in con.execute("select id from effect_icon")},
                "effect_des": {r[0] for r in con.execute("select id from effect_des")},
                "effect_info": {r[0] for r in con.execute("select id from effect_info")},
            }
        finally:
            con.close()
        reg = self.registry
        self.space_ids["moves(registry.moves)"] = {int(x) for x in reg.get("moves", [])}
        self.space_ids["soul(registry.soul)"] = {int(x) for x in reg.get("soul", [])}
        if self.jsonl_path.is_file():
            self.jsonl_lines = self.jsonl_path.read_text(
                encoding="utf-8", errors="replace").splitlines()
        if self.fails_path.is_file():
            text = self.fails_path.read_text(encoding="utf-8", errors="replace")
            self.fail_scens = set(SCEN_FILE_RE.findall(text))

    # ---------------------------------------------------------- 文件定位

    def find_wiki_file(self, rel: str) -> Path | None:
        p = (self.root / rel) if not rel.startswith("/") else Path(rel)
        if p.is_file():
            return p
        # build/* 引用在 worktree 里可能不存在 → 退回 registry 所在目录找
        if rel.startswith("build/"):
            alt = self.registry_path.parent / rel[len("build/"):]
            if alt.is_file():
                return alt
        return None

    def find_scenario(self, no: str) -> Path | None:
        hits = sorted(self.scenario_dir.glob(f"scenario_{no}_*.cpp"))
        if not hits:
            hits = sorted(self.scenario_dir.glob(f"scenario_{no}.cpp"))
        return hits[0] if hits else None

    def find_doc(self, pattern: str) -> list[Path]:
        return sorted(self.root.glob(f"docs_local/docs/**/{pattern}"))

    def section_exists(self, path: Path, sec: str) -> bool:
        """节标题软定位：标题文本（去 # 后）以 sec 开头即算命中（§22.1/§五/§四点五）。"""
        try:
            for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
                m = HEADING_RE.match(line)
                if m and m.group("title").startswith(sec):
                    return True
        except OSError:
            pass
        return False

    # ---------------------------------------------------------- 校验 1：引注

    def check_line_count(self, path: Path) -> int:
        try:
            return len(path.read_text(encoding="utf-8", errors="replace").splitlines())
        except OSError:
            return -1

    def parse_mark(self, src: str, ln: int, idx: int, body: str) -> None:
        """解析一个〔源: …〕标记（内部按 + 拆分多源）。"""
        where = f"{src}:{ln} 源#{idx}"
        parts = [p.strip() for p in PART_SPLIT_RE.split(body) if p.strip()]
        prev_file: Path | None = None
        for part in parts:
            if part in META_VOCAB:
                self.mark_tally["元描述"] += 1
                continue
            m = SCEN_RE.match(part)
            if m:
                self.mark_tally["场景"] += 1
                no = m.group("no")
                self.cited_scens.add(no)
                sc = self.find_scenario(no)
                if sc is None:
                    self.rep.err(where, f"场景{no} 对应 test/scenario/scenario_{no}_*.cpp 不存在")
                    continue
                prev_file = sc
                rest = m.group("rest")
                lm = ANY_LINES_RE.search(rest)
                if lm:
                    n = self.check_line_count(sc)
                    if n >= 0:
                        check_lines(self.rep, where, sc.name, n, lm.group("lines"))
                continue
            m = OFFICIAL_RE.match(part)
            if m:
                self.mark_tally["官方原文"] += 1
                if self.jsonl_lines is None:
                    self.missing_jsonl = True
                    self.missing_jsonl_count += 1
                    if self.missing_jsonl_count == 1:  # 只报一次，收尾按数据缺失退出 2
                        self.rep.err(where, "all_posts_final.jsonl 缺失，官方原文 L 引"
                                            "（如 " + m.group("ls") + "）无法校验——其余同类不重复报")
                    continue
                total = len(self.jsonl_lines)
                for lnum in re.findall(r"L(\d+)", m.group("ls")):
                    i = int(lnum)
                    if i < 1 or i > total:
                        self.rep.err(where, f"官方原文 L{i} 超出 jsonl 行数（共 {total} 行）")
                    elif not self.jsonl_lines[i - 1].strip():
                        self.rep.warn(where, f"官方原文 L{i} 在 jsonl 中为空行")
                continue
            m = EDES_RE.match(part)
            if m:
                self.mark_tally["库内文档"] += 1
                for ids in re.findall(r"\d+", m.group("ids")):
                    i = int(ids)
                    if i not in self.space_ids["effect_des"]:
                        alt = self._spaces_of(i, skip="effect_des")
                        if alt:
                            self.rep.warn(
                                where,
                                f"effect_des 表无 id {i}（见于 {', '.join(sorted(alt))}）——"
                                "确认引用的是官方文本定义还是效果实现",
                            )
                        else:
                            self.rep.err(where, f"effect_des id {i} 不存在于任何已知空间")
                continue
            m = PROTO_RE.match(part)
            if m:
                self.mark_tally["库内文档"] += 1
                self._check_named_doc(where, ["*协议*.md"], ["协议采样"],
                                      m.group("sec"), "协议文档")
                continue
            m = SPEED_RE.match(part)
            if m:
                self.mark_tally["库内文档"] += 1
                self._check_named_doc(where, ["*速查*.md"], [], m.group("sec"), "速查")
                continue
            m = MORNING_RE.match(part)
            if m:
                self.mark_tally["库内文档"] += 1
                name = m.group("name")
                hits = self.find_doc(f"夜跑晨报-{name}*.md")
                if not hits:
                    self.rep.err(where, f"晨报文件 夜跑晨报-{name}*.md 在 docs_local/docs 下不存在")
                elif m.group("sec") and not self.section_exists(hits[0], m.group("sec")):
                    self.rep.warn(where, f"晨报 {hits[0].name} 中未定位到节「{m.group('sec')}」"
                                         "（软校验：标题写法可能不同）")
                continue
            if KHEAD_RE.match(part) or LINENOTE_RE.match(part):
                self.mark_tally["库内文档"] += 1  # K 系文件头/行注：格式认可（K 号≠文件名，无法落文件）
                continue
            m = SHORT_DOC_RE.match(part)
            if m:
                self.mark_tally["库内文档"] += 1
                name, sec = m.group("name"), m.group("sec")
                hits = self.find_doc(f"*{name}*.md")
                if not hits:
                    self.rep.err(where, f"库内文档短名「{name}」未匹配到 docs_local/docs 下的任何 .md")
                else:
                    if len(hits) > 1:
                        self.rep.warn(where, f"短名「{name}」匹配到 {len(hits)} 个文件，按第一个"
                                             f"（{hits[0].name}）定位节「{sec}」")
                    if not self.section_exists(hits[0], sec):
                        self.rep.warn(where, f"{hits[0].name} 中未定位到节「{sec}」"
                                             "（软校验：标题写法可能不同）")
                continue
            m = PATH_RE.match(part)
            if m:
                self.mark_tally["路径行"] += 1
                rel = m.group("path")
                f = self.find_wiki_file(rel)
                if f is None:
                    self.rep.err(where, f"被引文件不存在：{rel}")
                    continue
                prev_file = f
                rest = m.group("rest")
                lm = LEAD_LINES_RE.match(rest)
                if lm:
                    n = self.check_line_count(f)
                    if n >= 0:
                        check_lines(self.rep, where, rel, n, lm.group("lines"))
                continue
            m = BARE_LINES_RE.match(part)
            if m:
                self.mark_tally["路径行"] += 1
                if prev_file is None:
                    self.rep.warn(where, f"裸行号「{part}」前无同一标记内可续接的文件源")
                else:
                    n = self.check_line_count(prev_file)
                    if n >= 0:
                        check_lines(self.rep, where, prev_file.name, n, m.group("lines"))
                continue
            self.mark_tally["库内文档"] += 1
            self.rep.warn(where, f"未识别的源标记形态：「{part}」"
                                 "（软告警——若是新形态请扩 check_wiki_citations.py 的解析分支）")

    def _check_named_doc(self, where: str, globs: list[str], prefer: list[str],
                         sec: str, label: str) -> None:
        hits: list[Path] = []
        for g in globs:
            hits.extend(self.find_doc(g))
        if not hits:
            self.rep.err(where, f"{label} 对应文件（{'/'.join(globs)}）在 docs_local/docs 下不存在")
            return
        if prefer and len(hits) > 1:
            pref = [h for h in hits if any(k in h.name for k in prefer)]
            if pref:
                hits = pref
        if not self.section_exists(hits[0], sec):
            self.rep.warn(where, f"{label}（{hits[0].name}）中未定位到节「{sec}」"
                                 "（软校验：标题写法可能不同）")

    def _spaces_of(self, i: int, skip: str | None = None) -> set[str]:
        return {name for name, ids in self.space_ids.items()
                if name != skip and i in ids}

    # ---------------------------------------------------------- 校验 2：效果 id

    def scan_effect_ids(self, src: str, ln: int, line: str) -> None:
        for pat_name, rex in (
            ("effect", EFF_RE), ("效果id", XIAOGUO_ID_RE), ("Eid", EID_RE),
            ("K系", K_ID_RE), ("魂印", HUNYIN_RE),
        ):
            for m in rex.finditer(line):
                ids = re.findall(r"\d+", m.group(1))
                self.id_hits.append((src, ln, pat_name, ",".join(ids)))
                for i in ids:
                    self._check_one_id(src, ln, pat_name, int(i), False)
        for m in EDES_ID_RE.finditer(line):
            ids = re.findall(r"\d+", m.group(1))
            self.id_hits.append((src, ln, "effect_des", ",".join(ids)))
            for i in ids:
                self._check_one_id(src, ln, "effect_des", int(i), True)

    def _check_one_id(self, src: str, ln: int, pat: str, i: int, des_only: bool) -> None:
        where = f"{src}:{ln}"
        if des_only:
            if i not in self.space_ids["effect_des"]:
                alt = self._spaces_of(i, skip="effect_des")
                if alt:
                    self.rep.warn(where, f"effect_des id {i}（{pat} 引）不在 effect_des 表"
                                         f"（见于 {', '.join(sorted(alt))}）")
                else:
                    self.rep.err(where, f"effect_des id {i}（{pat} 引）不存在于任何已知空间")
            return
        spaces = self._spaces_of(i)
        if not spaces:
            self.rep.err(where, f"效果 id {i}（{pat} 引）不存在——"
                                "moves/soul 两空间（sqlite side_effect/effect_icon +"
                                " registry moves/soul）均无此 id")
        else:
            sqlite_spaces = {s for s in spaces if "registry" not in s}
            if not sqlite_spaces:
                self.rep.warn(where, f"效果 id {i}（{pat} 引）仅见于 registry 码表，"
                                     f"sqlite 两表皆无行（见于 {', '.join(sorted(spaces))}）"
                                     "——DB/registry 漂移？")

    # ---------------------------------------------------------- 校验 3/4：码表 + 待验证

    def scan_table_row(self, src: str, ln: int, line: str) -> None:
        cells = [c.strip() for c in TABLE_ROW_SPLIT_RE.split(line.strip())]
        if cells and cells[0] == "":
            cells = cells[1:]
        if cells and cells[-1] == "":
            cells = cells[:-1]
        if len(cells) < 2 or len(cells) % 2 != 0:
            return
        ids, names = cells[0::2], cells[1::2]
        if not all(re.fullmatch(r"\d+", c) for c in ids):
            return
        if not all(names):
            return
        for i_s, name in zip(ids, names):
            i = int(i_s)
            if i in self.anomaly_table and self.anomaly_table[i] != name:
                self.rep.err(f"{src}:{ln}",
                             f"正文码表内部冲突：id {i} 同时写作"
                             f"「{self.anomaly_table[i]}」与「{name}」")
            self.anomaly_table[i] = name
            self.anomaly_table_src.setdefault(i, f"{src}:{ln}")

    def scan_pending(self, src: str, ln: int, line: str, in_list_section: bool) -> None:
        pos = line.find("⚠待验证")
        if pos >= 0:
            snippet = line[pos:].strip()
            snippet = re.split(r"[。]|〔源", snippet)[0].strip(" 　")
            self.pending_items.append((src, ln, snippet[:80]))
        if in_list_section and ZHUZHANG_RE.match(line):
            cells = [c.strip() for c in TABLE_ROW_SPLIT_RE.split(line.strip())]
            cells = [c for c in cells if c != ""]
            if len(cells) >= 2 and re.fullmatch(r"\d+", cells[0]):
                self.pending_chapter.append((src, cells[0], cells[1][:60]))

    # ---------------------------------------------------------- 主流程

    def run(self) -> int:
        self.load_data()
        files = sorted(self.wiki_dir.glob("*.md"))
        print("== 百科机校 check_wiki_citations（S7）==")
        print(f"wiki 目录: {self.wiki_dir}  （{len(files)} 个 .md）")

        # ---- 校验 1 + 顺带收集 2/3/4 的素材
        print("[1] 〔源:…〕引注校验")
        for f in files:
            src = f.name
            in_fence = False
            list_section = False
            try:
                lines = f.read_text(encoding="utf-8", errors="replace").splitlines()
            except OSError as e:
                self.rep.err(src, f"读取失败：{e}")
                continue
            for ln, line in enumerate(lines, 1):
                if line.lstrip().startswith("```"):
                    in_fence = not in_fence
                    continue
                if in_fence:
                    continue
                hm = re.match(r"^#{1,6}\s*(.*)$", line)
                if hm:
                    list_section = "待验证清单" in hm.group(1)
                for idx, m in enumerate(MARK_RE.finditer(line), 1):
                    self.parse_mark(src, ln, idx, m.group(1).strip())
                self.scan_effect_ids(src, ln, line)
                self.scan_table_row(src, ln, line)
                self.scan_pending(src, ln, line, list_section)
        self.rep.flush_section()
        tally = self.mark_tally
        print(f"  引注 {sum(tally.values())} 处：路径行 {tally['路径行']}｜场景 {tally['场景']}"
              f"｜官方原文 {tally['官方原文']}｜库内文档 {tally['库内文档']}"
              f"｜元描述 {tally['元描述']}")

        # 场景真绿软校验（night-regression-fails.txt 对账，不重跑 gate）
        if self.fail_scens and self.cited_scens:
            bad = sorted(self.cited_scens & self.fail_scens)
            for no in bad:
                self.rep.warn("[场景真绿]", f"场景{no} 出现在 build/night-regression-fails.txt"
                                            "（近期回归失败——百科断言口径可能过期）")
        self.rep.flush_section()

        # ---- 校验 2
        print("[2] 效果 id 存在性")
        uniq = {}
        for src, ln, pat, ids in self.id_hits:
            uniq.setdefault((pat, ids), (src, ln))
        print(f"  扫到 id 引用 {len(self.id_hits)} 处（去重后 {len(uniq)} 组："
              f"{', '.join(sorted({p for p, _ in uniq})) or '无'}）")
        self.rep.flush_section()

        # ---- 校验 3
        print("[3] anomalies 码表对齐")
        anomalies = {int(a["id"]): str(a.get("name", "")) for a in
                     (self.registry or {}).get("anomalies", [])}
        if not self.anomaly_table:
            self.rep.warn("[码表]", "正文未找到 anomalies 码表（| id | 名称 | 形态）——"
                                    "若本章应含码表请检查表格写法")
        else:
            print(f"  正文码表 {len(self.anomaly_table)} 条 vs registry anomalies {len(anomalies)} 条")
            for i in sorted(set(self.anomaly_table) - set(anomalies)):
                self.rep.err(self.anomaly_table_src[i],
                             f"码表多出 id {i}（「{self.anomaly_table[i]}」）——registry anomalies 无此 id")
            for i in sorted(set(anomalies) - set(self.anomaly_table)):
                self.rep.err("[码表]", f"registry anomalies id {i}（「{anomalies[i]}」）"
                                       "不在正文码表中——码表缺漏")
            for i in sorted(set(anomalies) & set(self.anomaly_table)):
                if self.anomaly_table[i] != anomalies[i]:
                    self.rep.err(self.anomaly_table_src[i],
                                 f"id {i} 名称不一致：正文「{self.anomaly_table[i]}」"
                                 f" vs registry「{anomalies[i]}」")
        self.rep.flush_section()

        # ---- 校验 4
        print("[4] 待验证清单汇总（供人类只扫清单）")
        by_file_items: dict[str, list[tuple[int, str]]] = {}
        for src, ln, snip in self.pending_items:
            by_file_items.setdefault(src, []).append((ln, snip))
        by_file_chap: dict[str, list[tuple[str, str]]] = {}
        for src, no, claim in self.pending_chapter:
            by_file_chap.setdefault(src, []).append((no, claim))
        total = len(self.pending_items) + len(self.pending_chapter)
        for src in sorted(set(by_file_items) | set(by_file_chap)):
            items = by_file_items.get(src, [])
            chap = by_file_chap.get(src, [])
            print(f"  {src}：行内 ⚠待验证 {len(items)} 处，章末清单 {len(chap)} 条")
            if not self.rep.quiet:
                for ln, snip in items:
                    print(f"    L{ln}: {snip}")
                for no, claim in chap:
                    print(f"    清单#{no}: {claim}")
        print(f"  合计待验证条目 {total} 条")

        # ---- 收尾
        print(f"—— 结果：错误 {self.rep.errors}｜警告 {self.rep.warns}｜待验证 {total} 条")
        if self.missing_jsonl:
            print(f"[数据缺失] all_posts_final.jsonl 不存在而正文含 {self.missing_jsonl_count} "
                  "处官方原文 L 引（均未校验）——请用 --root/--jsonl 指向有抓取数据的 checkout"
                  f"（本次默认路径 {self.jsonl_path}）。按输入缺失处理，退出码 2。", file=sys.stderr)
            return 2
        return 1 if self.rep.errors else 0


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(
        description="百科引注机校（四项：引注/效果 id/anomalies 码表/待验证汇总）",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__.split("用法：", 1)[-1].split("退出码：", 1)[0],
    )
    ap.add_argument("--root", default=str(Path(__file__).resolve().parent.parent),
                    help="仓库根（默认 = 本脚本所在仓库）")
    ap.add_argument("--wiki-dir", default=None, help="百科目录（默认 <root>/docs_local/docs/10-游戏百科）")
    ap.add_argument("--registry", default=None, help="registry.json（默认 <root>/build/registry.json）")
    ap.add_argument("--db", default=None,
                    help="官方 sqlite（默认 <root>/scripts/data/processed/seer_unity.sqlite）")
    ap.add_argument("--jsonl", default=None,
                    help="官方原文 jsonl（默认 <root>/reference/scrape/out/all_posts_final.jsonl）")
    ap.add_argument("--scenario-dir", default=None, help="场景目录（默认 <root>/test/scenario）")
    ap.add_argument("--fails", default=None,
                    help="回归失败清单（默认 <root>/build/night-regression-fails.txt，缺则跳过）")
    ap.add_argument("--quiet", action="store_true", help="gate 模式：只出错误/警告/汇总行")
    opt = ap.parse_args(argv)

    root = Path(opt.root).resolve()
    opt.wiki_dir = Path(opt.wiki_dir) if opt.wiki_dir else root / "docs_local/docs/10-游戏百科"
    opt.registry = Path(opt.registry) if opt.registry else root / "build/registry.json"
    opt.db = (Path(opt.db) if opt.db else
              root / "scripts/data/processed/seer_unity.sqlite")
    opt.jsonl = (Path(opt.jsonl) if opt.jsonl else
                 root / "reference/scrape/out/all_posts_final.jsonl")
    opt.scenario_dir = Path(opt.scenario_dir) if opt.scenario_dir else root / "test/scenario"
    opt.fails = Path(opt.fails) if opt.fails else root / "build/night-regression-fails.txt"
    return WikiChecker(opt).run()


if __name__ == "__main__":
    sys.exit(main())
