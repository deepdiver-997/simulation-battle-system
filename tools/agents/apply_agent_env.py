#!/usr/bin/env python3
"""sbs-local 的 agent 资源改写器（agent-workspace-kit 策略层）。

被 new-worktree.sh 第 4 步调用：把 .agent-env 里的独占资源写进 worktree 自己的
配置副本，并输出 TOUCHED=<被改写的 tracked 文件列表>（脚本会对它们标 skip-worktree）。

本项目（回合制对战引擎）的资源现状与策略：
  · 数据库   sqlite 只读共享（scripts/data/processed/seer_unity.sqlite）——不做 per-worktree 库；
  · 端口     sim_server 由 battle_replay/探针自动挑空闲口，场景二进制无端口——预留段仅备用；
  · 构建目录 build/ 在 worktree 内天然隔离（CMake 缓存绑定各自源码树）；
  · 运行时   soul 插件 dylib 由 *_deploy 目标落在 worktree 自己的 resources/ 下。
⇒ 无任何需要改写的配置文件。本脚本只做记录性输出；将来出现 per-worktree 配置
 （如独立回放数据目录、固定端口服务）时在此扩展，输出 TOUCHED= 列表即可挂钩
 skip-worktree 机制。
"""
import os
import sys

worktree = sys.argv[1] if len(sys.argv) > 1 else "."
env_path = os.path.join(worktree, ".agent-env")
index = "?"
if os.path.exists(env_path):
    with open(env_path, encoding="utf-8") as f:
        for line in f:
            if line.startswith("AGENT_INDEX="):
                index = line.split("=", 1)[1].strip()
print(f"sbs-local 策略层：AGENT_INDEX={index}；无需改写配置（sqlite 只读共享 / 端口自动 / build 天然隔离）")
print("TOUCHED=")
sys.exit(0)
