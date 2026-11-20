#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""仓库内 seerapi-solaris 解析器覆盖层。

官方 PyPI 包对部分新布局滞后（详见 moves.py 头注）。decode_seer_bytes.py 用
`get_parser_overrides()` 拿到 {source_config_filename: 解析器类}，对同名源文件
优先用本层解析器；未覆盖的配置照走官方包。
"""
from __future__ import annotations

from .moves import MovesParserV1106


def get_parser_overrides() -> dict[str, type]:
    """返回覆盖表：源文件名 → 替代解析器类。"""
    return {
        MovesParserV1106.source_config_filename(): MovesParserV1106,
    }


__all__ = ['MovesParserV1106', 'get_parser_overrides']
