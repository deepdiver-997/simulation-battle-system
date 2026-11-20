#!/usr/bin/env python3
"""工单/派发模板引用 lint（仅用 Python 3 标准库，不依赖构建或官方数据库）。

四类检查：
1. path：显式 repo 相对路径必须存在；Markdown 相对链接按文档目录解析。
2. basename：裸文件名在 SEARCH_DIRS 中递归归类；不唯一时列出候选供复核。
3. effect-id：同一短句内「文件名 … id」/「id … 文件名」的近邻效果号，先查
   前 40 行注释，再查全文；按完整数字匹配，全部未命中才 warning。不是语义验证。
4. assertions：「N 断言/检/检查」（也认 CHECK/checks）与邻近场景文件的静态断言位
   比较（CHECK*/audit*/assert(/REQUIRE( 行）；多场景求和，严格超 40 倍/低于 1/3
   才 warning（40 倍上界的校准依据见下方口径节）。

默认目标：docs_local/docs/05-任务清单/**/*.md + tools/agents/tasks/*.md。
用法：python3 tools/check_dispatch_refs.py [--root CHECKOUT] [文件或目录 ...]
      python3 tools/check_dispatch_refs.py --search-dir include 工单.md
白名单：tools/dispatch_refs_whitelist.txt（与脚本同目录；prefix/basename + 理由与依据，
      格式见文件头）。命中白名单按外部产物/语料/拟建报 warning。
--root 可指向历史快照，全部引用和场景均在该快照内解析；不调用 git、不改文件。
error 退出 1；只有 warning 或无问题退出 0；命令行参数错误退出 2。

口径：带 <占位符>、{占位符}、通配符的命名模板不视为具体文件；明确记为
原错引/退役/否定性（「没有 X」「X 缺失」）的局部引用，以及白名单收录的
外部产物/语料/拟建文件仅 warning——白名单引用不做存在性核验，结果与检出
无关（主检出常带未跟踪的 build/、reference/ 等目录，同一 commit 的两个检出
必须给出同一份 lint 结果）。不因整行含「勘误/已收官」而忽略其他引用。
拟建文件仍须存在（否则 error），除非白名单有据收录。已存在的 resources/、
include/ 等目录缩写按唯一后缀解析；完整根路径不做猜测或自动改名。脚本不能
判断「K396 模式切换」等没有文件名的语义错挂，也不把静态断言位（CHECK/
audit/assert/REQUIRE 行）当运行时断言精确数——运行时断言数可被循环/参数化
放大（实测样本 5740：1 个 CHECK 位跑出 30 断言、7520：69 位 538 断言，最大
约 30 倍），故数量级上界取 40 倍只拦幻觉级夸大，下界 1/3 拦过时低报。输出
保留文档行号和触发检查的引用原文。
"""

from __future__ import annotations

import argparse
from collections import Counter, defaultdict
from dataclasses import dataclass
from pathlib import Path
import re
import sys


SEARCH_DIRS = (
    "resources/moves_lib", "resources/soul_lib", "tools", "docs_local", "src", "test", "include", "resources",
)
EXTENSIONS = r"cpp|cxx|cc|c|hpp|hh|h|md|sql|py|sh|jsonl|json|txt|ya?ml|toml"
# Directories cannot contain an extension: a.h/b.cpp is a list, not a path.
FILE_RE = re.compile(
    rf"(?<![A-Za-z0-9_.-])(?P<ref>"
    rf"(?:(?:\.\.?|[A-Za-z_][A-Za-z0-9_-]*|\d{{2}}-[\w-]+)/"
    rf"(?:(?:\.\.?|[A-Za-z0-9_\u4e00-\u9fff<>*?{{}},-]+)/)*)?"
    rf"[A-Za-z0-9_\u4e00-\u9fff<>*?{{}},.-]+\.(?:{EXTENSIONS}))(?![A-Za-z0-9_])"
)
NUMBER_RE = re.compile(r"(?<![A-Za-z0-9_.:：-])\d{1,6}(?![A-Za-z0-9_.])")
ID_GROUP_RE = re.compile(NUMBER_RE.pattern + r"(?:\s*[/、,]\s*" + NUMBER_RE.pattern + r")*")
ASSERT_RE = re.compile(r"(?<![A-Za-z0-9_])(\d+)\s*(?:断言|检查|检(?!测)|CHECK\b|checks?\b)", re.I)
SCENE_RE = re.compile(
    r"(?:scenario[_ -]?|场景\s*)(\d+(?:\s*[-–~]\s*\d+)?"
    r"(?:\s*[/、,]\s*\d+(?:\s*[-–~]\s*\d+)?)*)", re.I,
)
CLAUSE_RE = re.compile(r"[;；|。\n]")
URL_RE = re.compile(r"(?:https?|file|ssh)://[^\s`<>]+")
ABSOLUTE_RE = re.compile(r"(?<![\w.])(?:/|~/)[^\s`<>]+")
# 静态断言位：CHECK/CHECK_EQ 子串、audit 槽断言（audit/audit_value/probe_audits…）、
# cassert 的 assert(、测试框架的 REQUIRE(。
SITE_RE = re.compile(r"CHECK|[Aa]udit\w*\(|\bassert\(|\bREQUIRE\(")
WHITELIST_NAME = "dispatch_refs_whitelist.txt"
# 运行时断言数可被循环放大（校准样本：场景 5740 一位 30 断言、7520 六十九位 538 断言，
# 实测合法比值最大 ~30×）；上界取 40× 只拦幻觉级夸大，下界 1/3 拦过时低报。
ASSERT_OVER_RATIO = 40


def load_whitelist(path: Path) -> tuple[dict[str, str], dict[str, str], list[str]]:
    """解析白名单文件，返回 (路径前缀表, 裸文件名表, 格式错误列表)。"""
    prefixes: dict[str, str] = {}
    basenames: dict[str, str] = {}
    errors: list[str] = []
    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except OSError:
        return prefixes, basenames, [f"白名单文件缺失：{path}"]
    for no, raw in enumerate(lines, 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split(None, 2)
        if len(parts) != 3 or parts[0] not in ("prefix", "basename") or not parts[1]:
            errors.append(f"第 {no} 行格式应为 `<prefix|basename> <token> <理由与依据>`：{line}")
            continue
        kind, token, reason = parts
        (prefixes if kind == "prefix" else basenames)[token] = reason.strip()
    # 长前缀优先，避免未来出现 build/ 与 build-asan/ 之类的包含关系时命中错条目。
    ordered = dict(sorted(prefixes.items(), key=lambda item: -len(item[0])))
    return ordered, basenames, errors


@dataclass
class Reference:
    text: str
    start: int
    end: int
    paths: tuple[Path, ...] = ()
    historical: bool = False


class Checker:
    def __init__(self, root: Path, search_dirs: list[str],
                 whitelist: tuple[dict[str, str], dict[str, str]] = ({}, {})) -> None:
        self.wl_prefixes, self.wl_basenames = whitelist
        self.root = root
        self.by_name: dict[str, list[Path]] = defaultdict(list)
        self.all_files: set[Path] = set()
        for directory in search_dirs:
            for path in sorted((root / directory).rglob("*")):
                if path.is_file() and path not in self.all_files:
                    self.by_name[path.name].append(path)
                    self.all_files.add(path)
        self.scenarios: dict[int, list[Path]] = defaultdict(list)
        for path in sorted((root / "test/scenario").glob("scenario_*.cpp")):
            match = re.match(r"scenario_(\d+)(?:_|\.)", path.name)
            if match:
                self.scenarios[int(match[1])].append(path)
        self.counts: Counter[str] = Counter()
        self.checks: Counter[str] = Counter()
        self.cache: dict[Path, str] = {}
        self.top_dirs = {p.name for p in root.iterdir() if p.is_dir()}

    def display(self, path: Path) -> str:
        try:
            return path.relative_to(self.root).as_posix()
        except ValueError:
            return str(path)

    def report(self, doc: Path, line: int, level: str, kind: str, quote: str, detail: str) -> None:
        self.counts[level] += 1
        self.counts[kind] += 1
        print(f"{self.display(doc)}:{line}: {level}[{kind}] 引用「{quote.strip()}」 — {detail}")

    def read(self, path: Path) -> str:
        if path not in self.cache:
            self.cache[path] = path.read_text(encoding="utf-8", errors="replace")
        return self.cache[path]

    @staticmethod
    def historical(line: str, ref: Reference, previous_end: int, next_start: int) -> bool:
        # Scope to this reference; never suppress another error elsewhere in a board row.
        # 没有/不存在/缺失 = 文档对文件缺席的否定性陈述（不是坏引用），一并降级。
        before = re.split(r"[;；|。]", line[previous_end:ref.start])[-1][-55:]
        after = line[ref.end:min(next_start, ref.end + 180)]
        return bool(
            re.search(r"(?:原引|原写|旧引|误引|退役文件|已删除文件|没有|不存在|缺失)[^;；|。]{0,40}$", before)
            or re.search(r"(?:该文件[^;；|。]{0,45})?(?:退役删除|已退役|已删除|历史不存在|缺失)", after)
        )

    def resolve(self, doc: Path, number: int, line: str, ref: Reference) -> None:
        name = ref.text
        if re.search(r"[<>*?{}]", name) or re.fullmatch(r"(?:xxx|XXX)\.[a-z]+", name):
            self.checks["templates"] += 1
            return
        kind = "path" if "/" in name else "basename"
        self.checks[kind] += 1
        # 白名单先行，且不做存在性核验：外部目录是否碰巧落盘不改变 lint 结果。
        whitelisted = (self.wl_basenames.get(name) if kind == "basename"
                       else next((why for prefix, why in self.wl_prefixes.items()
                                  if name.startswith(prefix)), None))
        if whitelisted:
            self.report(doc, number, "warning", kind, name,
                        "外部产物/语料/拟建（白名单，不核验存在性）：" + whitelisted)
            return
        if kind == "path":
            path = self.root / name
            if name.startswith(("./", "../")):
                # Markdown links are doc-relative; shell commands are repo-relative.
                path = (doc.parent if line[max(0, ref.start - 2):ref.start] == "](" else self.root) / name
            if path.is_file():
                ref.paths = (path,)
            elif name.split("/")[0] not in self.top_dirs and not name.startswith("."):
                ref.paths = tuple(p for p in self.by_name.get(Path(name).name, [])
                                  if p.as_posix().endswith("/" + name))
        else:
            ref.paths = tuple(self.by_name.get(name, []))
            if not ref.paths and (self.root / name).is_file():
                ref.paths = (self.root / name,)
        if not ref.paths:
            historical = ref.historical
            level = "warning" if historical else "error"
            reason = ("已标注的历史/否定性引用（文档自述该文件不存在或已退役）" if historical
                      else "引用文件不存在或不在裸文件名搜索目录内")
            self.report(doc, number, level, kind, name, reason)
        elif len(ref.paths) > 1:
            self.report(doc, number, "warning", kind, name,
                        "裸名/缩写有多个候选，请写完整路径：" + ", ".join(map(self.display, ref.paths)))

    @staticmethod
    def bounds(line: str, start: int, end: int) -> tuple[int, int]:
        left = max((m.end() for m in CLAUSE_RE.finditer(line, 0, start)), default=0)
        right = next((m.start() for m in CLAUSE_RE.finditer(line, end)), len(line))
        return left, right

    def check_id(self, doc: Path, number: int, line: str, ref: Reference,
                 previous_end: int, next_start: int) -> None:
        if not ref.paths or ref.historical or not ref.text.endswith((".cpp", ".cc", ".cxx", ".h", ".hpp")):
            return
        if Path(ref.text).name.startswith("scenario_"):
            return
        left, right = self.bounds(line, ref.start, ref.end)
        left, right = max(left, previous_end, ref.start - 55), min(right, next_start, ref.end + 55)
        candidates = []
        for start, end, direction in ((left, ref.start, "before"), (ref.end, right, "after")):
            part = line[start:end]
            for match in ID_GROUP_RE.finditer(part):
                a, b = start + match.start(), start + match.end()
                # Line references, board numbers, percentages, dimensions and counts aren't effect ids.
                if re.match(r"\s*(?:[%％×~–.-]|断言|检查|检|章|节|行|回合|点|次|槽|宏|载体|用例|场景|个|块|组|checks?\b)", line[b:], re.I):
                    continue
                if direction == "after" and re.match(r"\s*[:：]\s*\d", part):
                    continue
                gap = line[b:ref.start] if direction == "before" else line[ref.end:a]
                if re.search(r"[，,、+]|/(?!\s*\d)|(?:场景|commit|feat/|无\s*$)", gap) and gap.strip() != "/":
                    continue
                candidates.append((len(gap), a, b, match[0]))
        if not candidates:
            return
        _, a, b, id_text = min(candidates)
        effect_ids = sorted({int(n) for n in re.findall(r"\d+", id_text)})
        self.checks["effect-id"] += len(effect_ids)
        missing = []
        try:
            for effect_id in effect_ids:
                pattern = re.compile(rf"(?<!\d){effect_id}(?!\d)")
                for path in ref.paths:
                    text = self.read(path)
                    comments = "\n".join(s for s in text.splitlines()[:40]
                                         if s.lstrip().startswith(("//", "/*", "*", "#")))
                    if pattern.search(comments) or pattern.search(text):
                        break
                else:
                    missing.append(str(effect_id))
        except OSError as exc:
            self.report(doc, number, "error", "read", ref.text, str(exc))
            return
        if missing:
            quote = line[min(a, ref.start):max(b, ref.end)]
            self.report(doc, number, "warning", "effect-id", quote,
                        f"效果 id {'/'.join(missing)} 在前 40 行注释及全文均未命中："
                        + ", ".join(map(self.display, ref.paths)))

    @staticmethod
    def scene_ids(text: str) -> set[int]:
        result = set()
        for part in re.split(r"[/、,]", text):
            endpoints = [int(n) for n in re.findall(r"\d+", part)]
            if len(endpoints) == 1:
                result.add(endpoints[0])
            elif len(endpoints) == 2 and 0 <= endpoints[1] - endpoints[0] <= 1000:
                result.update(range(endpoints[0], endpoints[1] + 1))
        return result

    def check_assertions(self, doc: Path, number: int, line: str, refs: list[Reference]) -> None:
        scenes = []
        matches = list(SCENE_RE.finditer(line))
        # 「场景 7760(7 断言)/7761(45 断言)」retains the scenario prefix.
        for match in re.finditer(r"(?<=[)）])\s*/\s*(\d+)(?=\s*[(（])", line):
            left, _ = self.bounds(line, match.start(), match.end())
            if any(left <= m.start() < match.start() for m in matches):
                matches.append(match)
        for match in sorted(matches, key=lambda m: m.start()):
            # A full filename selects that file, not every scenario with the same numeric prefix.
            full = next((r for r in refs if r.start <= match.start() < r.end and r.paths), None)
            paths = set(full.paths) if full else {
                p for scene_id in self.scene_ids(match[1]) for p in self.scenarios.get(scene_id, [])
            }
            ids = self.scene_ids(match[1])
            complete = bool(paths) and (full is not None or all(len(self.scenarios.get(i, [])) == 1 for i in ids))
            scenes.append((match.start(), match.end(), paths, complete))
        # 「场景 A(说明,N 断言)/B(说明,M 断言)」：括号说明区里的断言数属于该场景，
        # 不参与跨「/」兄弟组的最近距离竞争（S15 行 7 断言误绑 /7761 教训）。
        zones = []
        for a, b, paths, complete in scenes:
            j = b
            while j < len(line) and line[j] == " ":
                j += 1
            if j >= len(line) or line[j] not in "(（":
                continue
            depth = 0
            for k in range(j, min(len(line), j + 160)):
                ch = line[k]
                if ch in "(（":
                    depth += 1
                elif ch in ")）":
                    depth -= 1
                    if depth == 0:
                        zones.append((j, k + 1, a, b, paths, complete))
                        break
                elif ch in ";；|。":
                    break
        for claim in ASSERT_RE.finditer(line):
            if any(a <= claim.start() < b for a, b, _, _ in scenes):
                continue  # 「场景 5010 断言」doesn't claim 5010 assertions.
            left, right = self.bounds(line, claim.start(), claim.end())
            zone = next((entry for entry in zones if entry[0] <= claim.start() < entry[1]), None)
            if zone:
                za, zb, a, b, paths, complete = zone
                # 括号内含更精确的场景匹配（「（scenario_7500 四块 100 断言）」的单文件
                # 内层匹配）时优先内层，免于号段缺文件导致的误报。
                inner = [entry for entry in scenes if za <= entry[0] and entry[1] <= zb]
                if inner:
                    a, b, paths, complete = min(inner, key=lambda e: abs(e[0] - claim.start()))
                candidates = [(0, a, b, paths, complete)]
            else:
                candidates = [(max(0, claim.start() - b, a - claim.end()), a, b, paths, complete)
                              for a, b, paths, complete in scenes if left <= a and b <= right]
                if not candidates:
                    continue
                candidates = [min(candidates, key=lambda item: item[0])]
            distance, a, b, paths, complete = candidates[0]
            if not zone and distance > 120:
                continue
            quote = line[min(a, claim.start()):max(b, claim.end())]
            if not complete:
                self.report(doc, number, "warning", "assertions", quote,
                            "场景文件缺失或同号多文件，未比较数量级；请写完整文件名")
                continue
            self.checks["assertions"] += 1
            try:
                actual = sum(sum(1 for s in self.read(p).splitlines() if SITE_RE.search(s))
                             for p in paths)
            except OSError as exc:
                self.report(doc, number, "error", "read", quote, str(exc))
                continue
            claimed = int(claim[1])
            if claimed > actual * ASSERT_OVER_RATIO or claimed * 3 < actual:
                self.report(doc, number, "warning", "assertions", quote,
                            f"声称 {claimed}，静态断言位（CHECK/audit/assert/REQUIRE 行）{actual}，"
                            f"比值超出 [1/3, {ASSERT_OVER_RATIO}×]（上界按实测循环放大 ≤30× 校准）；"
                            + ", ".join(map(self.display, sorted(paths))))

    def check_document(self, doc: Path) -> None:
        try:
            lines = self.read(doc).splitlines()
        except OSError as exc:
            self.report(doc, 1, "error", "read", str(doc), str(exc))
            return
        self.checks["documents"] += 1
        number = 0
        while number < len(lines):
            first = number + 1
            line = lines[number]
            number += 1
            # Markdown wraps a backticked path immediately after a slash.
            while line.rstrip().endswith("/") and line.count("`") % 2 and number < len(lines):
                line = line.rstrip() + lines[number].lstrip()
                number += 1
            searchable = URL_RE.sub(lambda m: " " * len(m[0]), line)
            searchable = ABSOLUTE_RE.sub(lambda m: " " * len(m[0]), searchable)
            refs = [Reference(m[0], m.start(), m.end()) for m in FILE_RE.finditer(searchable)]
            for index, ref in enumerate(refs):
                previous_end = refs[index - 1].end if index else 0
                next_start = refs[index + 1].start if index + 1 < len(refs) else len(line)
                ref.historical = self.historical(line, ref, previous_end, next_start)
                self.resolve(doc, first, line, ref)
            for index, ref in enumerate(refs):
                previous_end = refs[index - 1].end if index else 0
                next_start = refs[index + 1].start if index + 1 < len(refs) else len(line)
                self.check_id(doc, first, line, ref, previous_end, next_start)
            self.check_assertions(doc, first, line, refs)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("targets", nargs="*", help="Markdown 文件或目录（相对 --root；目录递归）")
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parent.parent)
    parser.add_argument("--search-dir", action="append", default=[], help="额外裸文件名搜索目录（相对 --root）")
    args = parser.parse_args(argv)
    root = args.root.resolve()
    if not root.is_dir():
        print(f"{root}:1: error[input] 仓库根目录不存在")
        return 1
    whitelist_path = Path(__file__).resolve().parent / WHITELIST_NAME
    wl_prefixes, wl_basenames, wl_errors = load_whitelist(whitelist_path)
    if wl_errors:
        for message in wl_errors:
            print(f"tools/{WHITELIST_NAME}:1: error[input] {message}")
        return 1
    checker = Checker(root, list(SEARCH_DIRS) + args.search_dir, (wl_prefixes, wl_basenames))
    targets = set()
    if args.targets:
        for name in args.targets:
            path = root / name
            if path.is_dir():
                targets.update(path.rglob("*.md"))
            else:
                targets.add(path)
    else:
        targets.update((root / "docs_local/docs/05-任务清单").rglob("*.md"))
        targets.update((root / "tools/agents/tasks").glob("*.md"))
    if not targets:
        checker.report(root, 1, "error", "input", "检查目标", "没有找到 Markdown 文件")
    for doc in sorted(targets):
        checker.check_document(doc)
    print("检查：" + ", ".join(f"{key}={checker.checks[key]}" for key in (
        "documents", "path", "basename", "effect-id", "assertions", "templates")))
    print(f"结果：error={checker.counts['error']} warning={checker.counts['warning']}")
    return 1 if checker.counts["error"] else 0


if __name__ == "__main__":
    sys.exit(main())
