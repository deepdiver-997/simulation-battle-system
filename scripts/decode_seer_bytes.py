#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Decode Seer Unity .bytes configs into JSON using seerapi-solaris.

依赖：pip install seerapi-solaris
输入：data/raw/unity_bytes/*.bytes（由 fetch_seer_unity_bytes.py 拉取）
输出：data/raw/unity_json/*.json

只解析源文件存在的配置（fetch 可能只拉了战斗相关子集）。

权利声明（2026-10-01）：本脚本仅从公开社区镜像拉取配置文件，供个人学习/研究
搭建本地模拟环境使用；本仓库不分发任何游戏数据（拉取产物不入库）。「赛尔号」及
相关游戏内容的权利归属上海淘米网络科技有限公司。如权利方提出异议，请通过
仓库 README 中的反馈渠道联系。


用法：
    python decode_seer_bytes.py
"""
from __future__ import annotations

import os
from pathlib import Path

from solaris import parse

ROOT = Path(os.path.dirname(os.path.abspath(__file__)))
SRC = ROOT / "data" / "raw" / "unity_bytes"
OUT = ROOT / "data" / "raw" / "unity_json"


def main() -> int:
    if not SRC.is_dir():
        print(f"源目录不存在: {SRC}，先运行 fetch_seer_unity_bytes.py")
        return 1
    OUT.mkdir(parents=True, exist_ok=True)

    parsers = parse.import_parser_classes()
    parsed_data = {}
    ran, failed = 0, 0
    for cls in parsers:
        try:
            p = cls()
            fname = p.source_config_filename()
        except Exception:
            continue
        if not (SRC / fname).is_file():
            continue
        try:
            # load_source_config 按当前工作目录解析文件名，需先切到源目录
            with parse.change_workdir(SRC):
                data = p.load_source_config()
                parsed_data[cls] = p.parse(data)
            ran += 1
            print(f"  {fname} -> {p.parsed_config_filename()}")
        except Exception as e:
            failed += 1
            print(f"  ERR {fname}: {str(e)[:80]}")

    with parse.change_workdir(OUT):
        for cls in parsed_data:
            cls().save_parsed_config(parsed_data[cls])

    print(f"Decoded {ran} configs, {failed} failed -> {OUT}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
