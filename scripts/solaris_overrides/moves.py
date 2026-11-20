#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""K450：moves.bytes 双布局兼容解析器（2026-11-06 布局变更修复）。

背景（docs_local/docs/05-任务清单/圣光灵神SP形态与新精灵内核影响调研-2026-11-09.md §1.4）：
2026-11-06 游戏更新（镜像 ConfigPackage 20261106181039 起）改了 moves.bytes 的
item 布局——在 side_effect_arg 之后、type 之前插入一个 int 字段。该字段已由
上游 seerapi-solaris 106.12.2（2026-11-07 发布）修复并命名为 `soercemove`
（字节级逆向 + 上游实现双确认）：

  - 语义：变体/联动源技能 id（绝大多数 move 为 0；帝皇之戎"谋死"系 13739-13748
    等变体技指向其母本技能，如 13739 痴愚 → 37460 无相谛）。
  - 旧布局（< 2026-11-06，seerapi-solaris ≤ 106.11）：无此字段。
  - 新布局（≥ 2026-11-06，seerapi-solaris ≥ 106.12.2）：每 item 多 4 字节。

本解析器做**逐文件布局探测**：先用新布局全量解析并校验"恰好消费到 EOF"，
不通过再回退旧布局同样校验——两代数据都能正确解码（K450 纪律：不改坏旧数据
兼容）。字段名沿用上游 `soercemove`，保证未来升级官方包后语义一致。

落点说明：本文件是仓库内 vendored 副本（scripts/solaris_overrides/），由
decode_seer_bytes.py 优先加载替换官方 MovesParser，不依赖外部包版本。
外部安装包的等效修复见 scripts/patches/README.md（≥106.12.2 已自带，无需补）。

权利声明（2026-10-01）：仅用于解析公开社区镜像的配置文件，供个人学习/研究
搭建本地模拟环境使用；本仓库不分发任何游戏数据（拉取产物不入库）。
"""
from __future__ import annotations

from struct import error as struct_error
from typing import Any, TypedDict

from solaris.parse.base import BaseParser
from solaris.parse.bytes_reader import BytesReader


class UnityMoveItem(TypedDict, total=False):
    """字段集与上游 106.12.2 对齐（soercemove 为 11-06 新增）。"""

    accuracy: int
    atk_num: int
    atk_type: int
    category: int
    crit_rate: int
    friend_side_effect: list[int]
    friend_side_effect_arg: list[int]
    id: int
    max_pp: int
    mon_id: int
    must_hit: int
    name: str
    power: int
    priority: int
    side_effect: list[int]
    side_effect_arg: list[int]
    soercemove: int
    type: int
    info: str
    ordinary: int


class MovesConfig(TypedDict):
    root: dict[str, Any] | None


class MovesParserV1106(BaseParser[MovesConfig]):
    """moves.bytes 解析器：自动探测 11-06 前后两种 item 布局。"""

    @classmethod
    def source_config_filename(cls) -> str:
        return 'moves.bytes'

    @classmethod
    def parsed_config_filename(cls) -> str:
        return 'moves.json'

    @staticmethod
    def _read_optional_int_array(r: BytesReader) -> list[int]:
        if not r.ReadBoolean():
            return []
        count = r.ReadSignedInt()
        return [r.ReadSignedInt() for _ in range(count)]

    @classmethod
    def _parse_with_layout(cls, data: bytes, with_soercemove: bool):
        """按指定布局解析；成功时要求恰好消费到 EOF，否则抛异常。"""
        r = BytesReader(data)
        result: MovesConfig = {'root': None}
        if not r.ReadBoolean():
            if not r.eof():
                raise ValueError('layout probe: trailing bytes after empty tbl')
            return result
        if not r.ReadBoolean():
            if not r.eof():
                raise ValueError('layout probe: trailing bytes after empty moves')
            result['root'] = {'moves': None}
            return result
        if not r.ReadBoolean():
            if not r.eof():
                raise ValueError('layout probe: trailing bytes after empty move[]')
            result['root'] = {'moves': {'move': [], 'text': ''}}
            return result

        move_count = r.ReadSignedInt()
        moves: list[UnityMoveItem] = []
        for _ in range(move_count):
            item: UnityMoveItem = {
                'accuracy': r.ReadSignedInt(),
                'atk_num': r.ReadSignedInt(),
                'atk_type': r.ReadSignedInt(),
                'category': r.ReadSignedInt(),
                'crit_rate': r.ReadSignedInt(),
                'friend_side_effect': cls._read_optional_int_array(r),
                'friend_side_effect_arg': cls._read_optional_int_array(r),
                'id': 0,
                'max_pp': 0,
                'mon_id': 0,
                'must_hit': 0,
                'name': '',
                'power': 0,
                'priority': 0,
                'side_effect': [],
                'side_effect_arg': [],
                'type': 0,
                'info': '',
                'ordinary': 0,
            }
            item['id'] = r.ReadSignedInt()
            item['max_pp'] = r.ReadSignedInt()
            item['mon_id'] = r.ReadSignedInt()
            item['must_hit'] = r.ReadSignedInt()
            item['name'] = r.ReadUTFBytesWithLength()
            item['power'] = r.ReadSignedInt()
            item['priority'] = r.ReadSignedInt()
            item['side_effect'] = cls._read_optional_int_array(r)
            item['side_effect_arg'] = cls._read_optional_int_array(r)
            if with_soercemove:
                # 11-06 布局：side_effect_arg 之后、type 之前多一个 int。
                item['soercemove'] = r.ReadSignedInt()
            item['type'] = r.ReadSignedInt()
            item['info'] = r.ReadUTFBytesWithLength()
            item['ordinary'] = r.ReadSignedInt()
            moves.append(item)

        text = r.ReadUTFBytesWithLength()
        if not r.eof():
            raise ValueError(
                f'layout probe: {r.remaining()} bytes unconsumed after {move_count} items')
        result['root'] = {'moves': {'move': moves, 'text': text}}
        return result

    def parse(self, data: bytes) -> MovesConfig:
        # 先探 11-06 新布局（当前线上数据），失败回退旧布局；两者都失败才报错。
        try:
            return self._parse_with_layout(data, with_soercemove=True)
        except (ValueError, IndexError, UnicodeDecodeError, struct_error):
            pass
        try:
            return self._parse_with_layout(data, with_soercemove=False)
        except (ValueError, IndexError, UnicodeDecodeError, struct_error) as e:
            raise ValueError(
                f'moves.bytes: neither 11-06 layout nor legacy layout parses: {e}'
            ) from e
