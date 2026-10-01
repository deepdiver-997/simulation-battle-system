#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Fetch Seer Unity config bytes from the community mirror.

背景：赛尔号已全面转 Unity 端，H5 (seerh5.61.com) 的配置数据不再更新。
Unity 端配置在 newseer.61.com 的 ConfigPackage bundle 中，社区镜像仓库
每 30 分钟自动提取一次并托管：
  https://github.com/SeerAPI/seer-unity-assets
本脚本从该镜像下载战斗相关 .bytes 配置到 data/raw/unity_bytes/。

权利声明（2026-10-01）：本脚本仅从公开社区镜像拉取配置文件，供个人学习/研究
搭建本地模拟环境使用；本仓库不分发任何游戏数据（拉取产物不入库）。「赛尔号」及
相关游戏内容的权利归属上海淘米网络科技有限公司。如权利方提出异议，请通过
仓库 README 中的反馈渠道联系。


后续两步：
  decode_seer_bytes.py       （用 seerapi-solaris 把 .bytes 解码成 JSON）
  import_seer_unity_sqlite.py（把 JSON 导入 SQLite）

用法：
    python fetch_seer_unity_bytes.py
"""
from __future__ import annotations

import os
import urllib.request

ROOT = os.path.dirname(os.path.abspath(__file__))
BASE_URL = (
    "https://raw.githubusercontent.com/SeerAPI/seer-unity-assets/"
    "master/newseer/assets/game/configs/bytes"
)
DEST = os.path.join(ROOT, "data", "raw", "unity_bytes")

# 战斗相关配置（.bytes 文件名与 H5 一致，多为 snake_case）
BATTLE_CONFIGS = [
    "moves",           # 技能
    "monsters",        # 精灵
    "skillTypes",      # 属性类型
    "side_effect",     # 技能副作用参数表
    "effectInfo",      # 效果模板
    "effectDes",       # 效果/异常描述
    "effectIcon",      # 魂印/效果图标与描述
    "battle_effects",  # 异常状态分类
    "effectbuff",      # 场地效果
    "new_se",          # 魂印/特性
    "addmoves",        # 额外行动（仓库文件名为小写 addmoves）
    "partnerEffectUpgrade",  # 魂印升级描述
    "bossEffectIcon",  # Boss 被动特性
    "buff",            # 战斗 buff
    "typesRelation",   # 属性克制表（H5 缺失的新数据）
    "hide_moves",      # 隐藏技能名映射
    "sp_hide_moves",   # 特殊隐藏技能
    "sp_hide_moves_bisaifu",  # 比赛服特殊隐藏技能
    "itemType",        # 道具类型
    "equip",           # 装备（套装部件：单件描述/品质/所属套装；成套效果全文逐件重复在 desc）
    "suit",            # 套装（id → 部件清单 cloths；成套激活判定依据）
    "mintmark",        # 刻印（培养线 2026-09-26：刻印目录；游戏内系统名 mintmark）
    "mintmarkElevare", # 刻印强化档位（等级 → 数值/描述）
    "nature",          # 性格表（培养线：25 种，五维 ±10% 修正，无体力项；官方 id 0-24）
]


def main() -> int:
    os.makedirs(DEST, exist_ok=True)
    ok = 0
    for name in BATTLE_CONFIGS:
        url = f"{BASE_URL}/{name}.bytes"
        dest = os.path.join(DEST, f"{name}.bytes")
        try:
            with urllib.request.urlopen(url) as resp:
                data = resp.read()
            with open(dest, "wb") as f:
                f.write(data)
            ok += 1
            print(f"  {name}.bytes  {len(data):>9} bytes")
        except Exception as e:
            print(f"  FAIL {name}: {e}")
    print(f"Downloaded {ok}/{len(BATTLE_CONFIGS)} files to {DEST}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
