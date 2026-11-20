#!/usr/bin/env python3
"""R7 evidence replay. Standard library only; no network, builds or DB writes.

Default/--check reconstructs and byte-compares the JSONL appendices and index.
--write-evidence writes only this directory's generated audit inputs.
--check-resource also requires the additive namespace/sidecar resource contract.
The resource itself is NEVER written here. Run with python3 -I.
"""
from __future__ import annotations

import argparse
import collections
import contextlib
import hashlib
import io
import json
from pathlib import Path
import re
import runpy
import sqlite3
import subprocess
import sys
from types import SimpleNamespace
import unicodedata

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
REL = HERE.relative_to(ROOT).as_posix()
LEX = re.compile(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'')
CONST = re.compile(
    r'\b(?:constexpr|const)\s+(?:(?:unsigned|signed|static)\s+)?'
    r'(?:int|unsigned|long|std::uint32_t|uint32_t|int32_t|auto)\s+'
    r'(\w+)\s*=\s*([\w:]+)\s*;')
MACRO = re.compile(
    r'\b(SKILL_EFFECT|CARRY_EFFECT|SOUL_PROGRAM|SOUL_MARK_TYPED|'
    r'SOUL_MARK|SOUL_PRE_BATTLE|SOUL_MARK_HOOKS)\s*\(\s*([\w:]+)\s*,')
CITATION = re.compile(r'〔源:\s*([^:]+):(\d+)(?:-(\d+))?〕')
QUERIES = {
    'effect_info': 'SELECT id,args_num,info,param,analyze,type,key FROM effect_info ORDER BY id',
    'side_effect': 'SELECT id,arg_count FROM side_effect ORDER BY id',
    'effect_icon': 'SELECT id,effect_id,pet_id,tips,args,kind,come FROM effect_icon ORDER BY effect_id,id',
    'monsters': 'SELECT id,name FROM monsters ORDER BY id',
    'moves': 'SELECT id,name,side_effect,friend_side_effect FROM moves ORDER BY id',
    'programs': 'SELECT * FROM custom_effect_programs ORDER BY id',
    'overrides': 'SELECT * FROM custom_effect_overrides ORDER BY id',
}


def require(ok, message):
    if not ok:
        raise ValueError(message)


def norm(text):
    return re.sub(r'\s+', '', unicodedata.normalize('NFKC', text or '').lower())


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def json_text(obj):
    return json.dumps(obj, ensure_ascii=False, indent=2) + '\n'


def json_lines(rows):
    return ''.join(json.dumps(row, ensure_ascii=False, separators=(',', ':')) + '\n' for row in rows)


def cite(path, start, end=None):
    return f'〔源: {path}:{start}' + (f'-{end}' if end and end != start else '') + '〕'


def readonly(path):
    con = sqlite3.connect(path.resolve().as_uri() + '?mode=ro', uri=True)
    con.execute('PRAGMA query_only=ON')
    require(con.execute('PRAGMA query_only').fetchone()[0] == 1, 'query_only not enabled')
    return con


def soul_dump(db):
    """Run the existing tool with only its connection factory replaced by RO.

    tools/soul_effects_dump.py uses sqlite3.connect(DB) without URI flags.
    Replacing its *local globals facade*, not sqlite3.connect process-wide,
    makes the actual tool run safely without editing it or opening the DB RW.
    """
    ns = runpy.run_path(str(ROOT / 'tools/soul_effects_dump.py'))
    fn = ns['main']
    connections = []

    def connect(_path):
        require(Path(_path).resolve() == db.resolve(), 'dump requested another DB')
        con = readonly(db)
        connections.append(con)
        return con

    fn.__globals__['DB'] = str(db)
    fn.__globals__['sqlite3'] = SimpleNamespace(connect=connect)
    old_argv = sys.argv
    buf = io.StringIO()
    try:
        sys.argv = [str(ROOT / 'tools/soul_effects_dump.py'), '--all']
        with contextlib.redirect_stdout(buf):
            require(fn() == 0, 'soul_effects_dump failed')
    finally:
        sys.argv = old_argv
        for con in connections:
            con.close()
    return {int(k): sorted(v, key=lambda x: x['row']) for k, v in json.loads(buf.getvalue()).items()}


def scan_registration_sources():
    text_cache = {}
    read_hashes = {}
    macro_rows = {'moves': collections.defaultdict(list), 'soul': collections.defaultdict(list)}

    def texts(path):
        if path not in text_cache:
            original = path.read_text(encoding='utf-8')
            commentless = LEX.sub(lambda m: re.sub(r'[^\n]', ' ', m[0])
                                 if m[0].startswith('/') else m[0], original)
            masked = LEX.sub(lambda m: re.sub(r'[^\n]', ' ', m[0]), original)
            text_cache[path] = (commentless, masked)
            read_hashes[path.relative_to(ROOT).as_posix()] = sha(path)
        return text_cache[path]

    def constants(path, seen):
        if path in seen:
            return {}
        seen.add(path)
        commentless, masked = texts(path)
        result = {}
        for inc in re.findall(r'^\s*#include\s+"([^"]+)"', commentless, re.M):
            target = (path.parent / inc).resolve()
            if target.is_relative_to(ROOT) and target.is_file():
                result.update(constants(target, seen))
        for m in CONST.finditer(masked):
            result[m[1]] = (m[2], cite(path.relative_to(ROOT).as_posix(), masked.count('\n', 0, m.start()) + 1))
        return result

    def resolve(arg, defs, visited=None):
        if arg.isdigit():
            return int(arg), []
        visited = set() if visited is None else visited
        require(arg not in visited and arg in defs, f'unresolved registration constant {arg}')
        visited.add(arg)
        expr, source = defs[arg]
        value, sources = resolve(expr, defs, visited)
        return value, [source] + sources

    for path in sorted((ROOT / 'resources').rglob('*.cpp')):
        _, masked = texts(path)
        matches = list(MACRO.finditer(masked))
        if not matches:
            continue
        defs = constants(path, set())
        for m in matches:
            eid, definition_sources = resolve(m[2], defs)
            space = 'moves' if m[1] in ('SKILL_EFFECT', 'CARRY_EFFECT') else 'soul'
            macro_rows[space][eid].append({
                'macro': m[1], 'argument': m[2],
                'source': cite(path.relative_to(ROOT).as_posix(), masked.count('\n', 0, m.start()) + 1),
                'constant_sources': definition_sources,
            })
    return macro_rows, dict(sorted(read_hashes.items()))


def verify_citations_before_write(objects):
    """Verify every actual cited line with sed, then retain its exact excerpt.

    Includes .vue and deployment files, unlike the wiki checker's MD default.
    No grep/tail pipelines and no invented line numbers for SQLite.
    """
    matches = set()
    for obj in objects:
        text = json.dumps(obj, ensure_ascii=False)
        for m in CITATION.finditer(text):
            matches.add((m[1], int(m[2]), int(m[3] or m[2])))
    selected = collections.defaultdict(set)
    cache = {}
    for path, start, end in sorted(matches):
        file = ROOT / path
        require(file.is_file(), f'citation path missing: {path}')
        if path not in cache:
            cache[path] = file.read_text(encoding='utf-8').splitlines()
        require(1 <= start <= end <= len(cache[path]), f'citation out of bounds: {path}:{start}-{end}')
        selected[path].update(range(start, end + 1))
    for path, numbers in sorted(selected.items()):
        indices = sorted(numbers)
        result = subprocess.run(['sed', '-n', ';'.join(f'{i}p' for i in indices), str(ROOT / path)],
                                check=True, text=True, capture_output=True)
        require(result.stdout.splitlines() == [cache[path][i - 1] for i in indices],
                f'sed content mismatch: {path}')
    return [
        {'citation': cite(path, start, end), 'text': '\n'.join(cache[path][start - 1:end])}
        for path, start, end in sorted(matches)
    ]


def set_record(values):
    ids = sorted(set(values))
    return {'count': len(ids), 'ids': ids}


def build_evidence(db, registry_path, policy, baseline):
    before = sha(db)
    registry = json.loads(registry_path.read_text())
    reg = {k: set(registry[k]) for k in ('moves', 'soul')}
    ids = baseline['ids']
    require(ids == sorted(set(ids)) and baseline['count'] == len(ids), 'bad baseline ids/count')
    require(all(type(i) is int for i in ids), 'noninteger baseline id')
    original_ids = set(ids)
    dump = soul_dump(db)
    with contextlib.closing(readonly(db)) as con:
        con.row_factory = sqlite3.Row
        con.execute('BEGIN')
        tables = {key: [dict(row) for row in con.execute(sql)] for key, sql in QUERIES.items()}
    require(sha(db) == before, 'DB changed during read-only audit')
    names = {x['id']: x['name'] for x in tables['monsters']}
    independent_dump = collections.defaultdict(list)
    for x in tables['effect_icon']:
        raw = x['pet_id']
        pets = json.loads(raw) if raw and raw.startswith('[') else ([int(raw)] if str(raw).isdigit() else [])
        independent_dump[x['effect_id']].append({
            'row': x['id'], 'pet_id': pets, 'pet_name': [names.get(p, str(p)) for p in pets],
            'tips': x['tips'] or '', 'args': x['args'] or '', 'kind': x['kind'], 'come': x['come'],
        })
    require(dump == dict(independent_dump), 'soul_effects_dump/independent SELECT mismatch')
    info = {x['id']: x for x in tables['effect_info']}
    side = {x['id']: x for x in tables['side_effect']}
    refs = collections.defaultdict(list)
    for move in tables['moves']:
        for chain in ('side_effect', 'friend_side_effect'):
            array = json.loads(move[chain] or '[]')
            require(isinstance(array, list) and all(type(i) is int for i in array), 'noninteger move array')
            for pos, eid in enumerate(array):
                refs[eid].append({'move_id': move['id'], 'name': move['name'], 'chain': chain, 'index': pos})
    registrations, source_hashes = scan_registration_sources()
    moves_universe = set(info) | set(side) | set(refs) | reg['moves'] | set(registrations['moves'])
    criterion = policy['classification']
    invalid_term = criterion['soul_invalid_evidence_term']

    def soul_rows(eid):
        rows = []
        for raw in dump.get(eid, []):
            text = norm(raw['tips'])
            rows.append({**raw, 'evidence': {
                'boss_invalid_text': invalid_term in text,
                'boss_valid_text': 'boss有效' in text,
                'boss_immune_text': 'boss免疫' in text,
                'reverse_marker': '仅boss有效' in text or 'boss对战中' in text,
            }})
        return rows

    entries = []
    for eid in ids:
        template = info.get(eid)
        analyzed = norm((template or {}).get('analyze'))
        explicit_valid = bool(re.search(criterion['explicit_valid_marker_regex'], analyzed)) and not any(
            term in analyzed for term in criterion['conflict_terms'])
        collision = eid in moves_universe
        rows = soul_rows(eid)
        require(any(x['evidence']['boss_invalid_text'] for x in rows), f'missing soul invalid evidence: {eid}')
        verdict = ('confirmed_wrong_move_prefill' if explicit_valid else
                   'unknown_moves_semantics' if collision else 'soul_only_in_audited_move_sources')
        entries.append({
            'id': eid,
            'evidence_namespace': 'soul',
            'soul_retention': 'keep_row_clause_evidence_not_whole_mark_disable',
            'verdict': verdict,
            'collision': collision,
            'moves_valid_marker_scope': ('terminal_marker_not_expanded_to_unmarked_clauses'
                                        if explicit_valid and analyzed.endswith('(boss有效)') else
                                        'nonterminal_valid_clause_only' if explicit_valid else 'none'),
            'ui_reachable_via_move_detail': bool(refs.get(eid)),
            'effect_info': template,
            'side_effect': side.get(eid),
            'soul_rows': rows,
            'move_references': refs.get(eid, []),
            'registry_membership': {s: eid in reg[s] for s in reg},
            'resource_registrations': {s: registrations[s].get(eid, []) for s in reg},
            'custom_programs': [x for x in tables['programs'] if x['effect_id'] == eid],
            'custom_overrides': [x for x in tables['overrides'] if x['effect_id'] == eid],
            'db_locator': {'effect_info.id': eid, 'effect_icon.effect_id': eid,
                           'moves': 'json_each(side_effect|friend_side_effect).value = id'},
        })
    valid = {x['id'] for x in entries if x['verdict'] == 'confirmed_wrong_move_prefill'}
    unknown = {x['id'] for x in entries if x['verdict'] == 'unknown_moves_semantics'}
    soul_only = original_ids - valid - unknown
    collision_ids = original_ids & moves_universe
    mixed = {x['id'] for x in entries if any(
        r['evidence']['boss_invalid_text'] and r['evidence']['boss_valid_text'] for r in x['soul_rows'])}
    variant = {x['id'] for x in entries if any(not r['evidence']['boss_invalid_text'] for r in x['soul_rows'])}
    invalid_in_db = {eid for eid in dump if any(invalid_term in norm(row['tips']) for row in dump[eid])}
    counts = {
        'prefill': len(ids), 'soul_retained': len(entries), 'collision': len(collision_ids),
        'explicit_moves_valid': len(valid), 'unknown_moves_semantics': len(unknown),
        'soul_only_in_audited_move_sources': len(soul_only),
        'collisions_with_carriers': len(collision_ids & set(refs)),
        'explicit_moves_valid_with_carriers': len(valid & set(refs)),
        'explicit_moves_valid_with_carriers_and_registry': len(valid & set(refs) & reg['moves']),
        'affected_distinct_moves': len({x['move_id'] for i in valid for x in refs.get(i, [])}),
        'affected_references': sum(len(refs.get(i, [])) for i in valid),
    }
    require(counts == policy['expected_counts'], f'count drift: {counts}')
    require({1296, 1470} <= valid, 'S25/S26 regression')
    require(criterion['unknown_is_valid'] is False and criterion['whole_soul_disable_from_evidence_id'] is False,
            'unsafe classification policy')
    require(criterion['manual_disable_allowed'] is True, 'manual disable must remain allowed')
    require(valid.isdisjoint(unknown) and valid | unknown | soul_only == original_ids, 'incomplete partition')
    pending = []
    for eid in criterion['s4_pending_ids']:
        require(eid not in original_ids, f'S4 original membership changed: {eid}')
        require(any(x['evidence']['boss_immune_text'] for x in soul_rows(eid)), f'S4 missing immune evidence: {eid}')
        pending.append({
            'id': eid, 'decision_status': 'pending_user_decision', 'user_decision': None,
            'legacy_membership': False, 'action': 'unchanged', 'effect_info': info.get(eid),
            'soul_rows': soul_rows(eid), 'registry_soul': eid in reg['soul'],
            'resource_soul_registrations': registrations['soul'].get(eid, []),
            'current_tags_not_approval': policy['s4']['observed_current_tags_not_approval'][str(eid)],
        })
    partitions = {
        'collision_ids': set_record(collision_ids),
        'confirmed_wrong_move_prefill_ids': set_record(valid),
        'valid_marker_at_template_end_ids': set_record(x['id'] for x in entries
            if x['moves_valid_marker_scope'] == 'terminal_marker_not_expanded_to_unmarked_clauses'),
        'nonterminal_valid_clause_only_ids': set_record(x['id'] for x in entries
            if x['moves_valid_marker_scope'] == 'nonterminal_valid_clause_only'),
        'reachable_wrong_move_prefill_ids': set_record(valid & set(refs)),
        'known_valid_without_move_carriers': set_record(valid - set(refs)),
        'registered_reachable_wrong_move_prefill_ids': set_record(valid & set(refs) & reg['moves']),
        'unknown_moves_semantics_ids': set_record(unknown),
        'soul_only_ids': set_record(soul_only),
        'legal_soul_retained_ids': set_record(original_ids),
        'soul_mixed_valid_invalid_same_row_ids': set_record(mixed),
        'soul_some_variant_rows_without_invalid_tag_ids': set_record(variant),
        'naive_soul_invalid_not_in_legacy_ids_no_automatic_addition': set_record(invalid_in_db - original_ids),
    }
    comparison = {}
    for s in reg:
        source_ids = set(registrations[s])
        comparison[s] = {
            'registry_count': len(reg[s]), 'source_macro_id_count': len(source_ids),
            'registry_without_scanned_macro': sorted(reg[s] - source_ids),
            'source_macro_without_registry': sorted(source_ids - reg[s]),
            'prefill_registry_count': len(original_ids & reg[s]),
            'prefill_source_macro_count': len(original_ids & source_ids),
            'prefill_disagreement_ids': sorted(original_ids & (reg[s] ^ source_ids)),
        }
    index = {
        'task': 'R7', 'baseline_commit': policy['baseline_commit'],
        'consumer_fix_implemented': False,
        'database_access': 'mode=ro; query_only=ON; soul_effects_dump --all through RO facade',
        'inputs': {
            'database': {'path': 'scripts/data/processed/seer_unity.sqlite', 'sha256': before},
            'baseline_prefill': {'path': f'{REL}/baseline_prefill.json', 'sha256': sha(HERE / 'baseline_prefill.json')},
            'registry_snapshot': {'path': f'{REL}/registry_snapshot.json', 'sha256': sha(registry_path),
                                  'provenance': 'hub build/registry.json; existing dump, not rebuilt'},
            'policy_sha256': sha(HERE / 'policy.json'),
            'soul_dump_tool_sha256': sha(ROOT / 'tools/soul_effects_dump.py'),
            'registration_source_sha256': source_hashes,
        },
        'queries': QUERIES,
        'table_counts': {key: len(rows) for key, rows in tables.items()},
        'soul_dump_verified_rows': sum(len(rows) for rows in dump.values()),
        'counts': counts,
        'extra_counts': {
            'prefill_with_effect_info': len(original_ids & set(info)),
            'prefill_with_side_effect': len(original_ids & set(side)),
            'valid_move_regular_references': sum(x['chain'] == 'side_effect' for i in valid for x in refs.get(i, [])),
            'valid_move_friend_references': sum(x['chain'] == 'friend_side_effect' for i in valid for x in refs.get(i, [])),
            'soul_invalid_db_ids': len(invalid_in_db),
            'soul_invalid_db_rows': sum(invalid_term in norm(x['tips']) for x in tables['effect_icon']),
            'prefill_soul_rows': sum(len(x['soul_rows']) for x in entries),
            'prefill_soul_invalid_rows': sum(r['evidence']['boss_invalid_text'] for x in entries for r in x['soul_rows']),
        },
        'partitions': partitions,
        'entry_order': 'entries.jsonl has exactly one row per baseline id, in ascending id order; line = baseline ids index + 1',
        'registration_comparison': comparison,
        'registration_limit': 'macro scan != compiler/runtime coverage; PRE_BATTLE/HOOKS declarations are recorded separately by macro; absent registry is not proof of absent behavior',
        's4_pending': pending,
    }
    citations = verify_citations_before_write([policy, entries, pending, {
        'registry': cite(f'{REL}/registry_snapshot.json', 1),
        'baseline': cite(f'{REL}/baseline_prefill.json', 1, 5),
    }])
    index['verified_citations'] = len(citations)
    require(sha(db) == before, 'DB changed before final evidence emission')
    return entries, index, citations


def resource_contract(baseline, index):
    """Additive data-only proposal; no legacy ids removed or reordered."""
    return {
        **baseline,
        'schema_version': 2,
        'namespace': 'soul',
        'granularity': 'effect_id_evidence_index_not_whole_soul_disable',
        'consumer_migration': {
            'status': 'not_implemented',
            'warning': '现有前端仍读取 ids 并误用于 skillEffects；新增空间标注/旁路尚未接入，不能宣称运行行为已修复。',
            'contract': f'{REL}/policy.json',
            'evidence': f'{REL}/entries.jsonl',
        },
        'moves_prefill_exclusions': {
            'namespace': 'moves',
            **index['partitions']['confirmed_wrong_move_prefill_ids'],
            'criterion': '仅原 ids 同号 moves 模板 effect_info.analyze 至少一个子句明确标（boss有效）且无冲突；不推定其余子句有效，不是整库有效白名单。',
            'application': '只排除整效果自动预填候选，避免丢失有效子句；不得撤销手动禁用或删除 soul ids，未标子句及其余同号项仍待同空间取证。',
            'manual_disable_allowed': True,
        },
        'pending_user_decision': {
            'namespace': 'soul', 'count': 4, 'ids': [357, 360, 644, 738],
            'status': 'pending',
            'note': 'S4 证据包完成不等于裁决；原 ids membership 和现有插件标签均不改。',
        },
    }


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    mode = ap.add_mutually_exclusive_group()
    mode.add_argument('--check', action='store_true')
    mode.add_argument('--write-evidence', action='store_true')
    ap.add_argument('--check-resource', action='store_true')
    ap.add_argument('--db', type=Path, default=ROOT / 'scripts/data/processed/seer_unity.sqlite')
    ap.add_argument('--registry', type=Path, default=HERE / 'registry_snapshot.json')
    opt = ap.parse_args()
    policy = json.loads((HERE / 'policy.json').read_text())
    baseline = json.loads((HERE / 'baseline_prefill.json').read_text())
    entries, index, citations = build_evidence(opt.db, opt.registry, policy, baseline)
    outputs = {
        'entries.jsonl': json_lines(entries),
        'source_citations.jsonl': json_lines(citations),
        'index.json': json_text(index),
    }
    for name, text in outputs.items():
        path = HERE / name
        if opt.write_evidence:
            path.write_text(text, encoding='utf-8')
        else:
            require(path.is_file() and path.read_text(encoding='utf-8') == text, f'evidence drift: {name}')
    if opt.check_resource:
        actual = json.loads((ROOT / 'resources/boss_invalid_effects.json').read_text())
        require(actual == resource_contract(baseline, index), 'resource must match additive namespace/sidecar contract')
        require(actual['count'] == len(actual['ids']) == len(set(actual['ids'])), 'resource ids/count mismatch')
        require(actual['ids'] == baseline['ids'], 'legal soul ids deleted/reordered')
        excluded = actual['moves_prefill_exclusions']
        require(excluded['count'] == len(excluded['ids']) == len(set(excluded['ids'])), 'sidecar ids/count mismatch')
        require(set(actual['pending_user_decision']['ids']).isdisjoint(actual['ids']), 'S4 membership changed')
    result = {
        'task': 'R7', 'status': 'PASS', 'mode': 'write-evidence' if opt.write_evidence else 'check',
        'counts': index['counts'], 'verified_citations': len(citations),
        'soul_dump_rows_verified': index['soul_dump_verified_rows'],
        'resource_checked': opt.check_resource, 'consumer_fix_implemented': False,
    }
    print(json.dumps(result, ensure_ascii=False, sort_keys=True))
    return 0


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (ValueError, OSError, sqlite3.Error, subprocess.CalledProcessError) as exc:
        print(f'R7 FAIL: {exc}', file=sys.stderr)
        raise SystemExit(1)
