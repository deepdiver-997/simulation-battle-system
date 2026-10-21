# examples/public —— 公开示例包（免数据库、可直接运行）

> 目标：让你**不装任何官方数据**也能在本仓公开检出上跑通一个完整闭环：
> 手工装配精灵 → 挂一个自己写的魂印 → 驱动一个回合 → 断言结果。

## 快速开始

```bash
cmake -S . -B build
cmake --build build -j8 --target sim_demo_scenario
./build/sim_demo_scenario
# 期望输出：examples/public demo PASS（断言 4 条）
```

无需数据库、无需插件动态库：示例里的技能与面板全部手工装配（`bare_pet.h`），
魂印程序用公开 API 直接注册（`demo_mark.h`）。

## 目录

| 文件 | 作用 |
|---|---|
| `bare_pet.h` | 免数据库的技能/精灵装配（`skill_bare` / `pet_bare` / `party_of`） |
| `public_harness.h` | 最小对局驱动（`Battle` 类 + `check_eq` / `check_true` 断言） |
| `demo_mark.h` | **教学用示例魂印**（非官方机制）：登场时给自己加 10 点护盾 |
| `demo_scenario.cpp` | 三个断言组成的最小闭环（护盾生效 + 护盾打空 + 无印对照） |

## 怎么写你自己的第一个效果

1. **写效果函数**——签名固定：

   ```cpp
   EffectResult my_effect(BattleContext* ctx, const SoulNodeArgs& a) {
       // a.owner = 效果归属方（0/1）；a.opp = 对手；a.host_slot = 宿主槽
       // a.row = 官方行参（原序；缺失用 a.row_or(i, fallback)）
       return EffectResult::kOk;
   }
   ```

2. **声明节点**（时点 + 函数 + once + early + 作用域）：

   ```cpp
   SoulMarkManager::getInstance().registerSoulMarkProgram(
       90002,
       {SOUL_NODE(State::BATTLE_ROUND_START, &my_effect,
                  /*once=*/false, /*early=*/false, SoulScope::STAGE)});
   ```

   - `once=true`：本回合只跑一次（下回合重注册复位）；
   - `early=true`：**登场激活时立即执行一次**，且**同时**在该时点的桶里再执行一次
     → 同一个效果加两遍。`early` 只用于"必须在选择技能之前就位"的信号类效果
     （封技/必中旗标）。**"登场时做一次"用 `once=true` + `early=false`**；
   - `SoulScope::STAGE` = 只在宿主上场时生效；`ROSTER` = 宿主存活即生效（跨切换保留）。

3. **挂到精灵上**：`SoulMark(你的 id, "名字", "描述")` 塞进 `ElfPet` 的 `soul_mark` 位。

4. **断言**：用 `public_harness.h` 的 `check_eq` / `check_true`（失败即非零退出，可直接进 CI）。

## 内核 API 去哪找

- 效果与插件契约：`include/plugin/`（`plugin_interface.h` / `soul_node_args.h` / `core_api.h`）
- 原语（伤害/回血/护盾/异常/免疫…）：`include/primitives/battle_primitives.h`
- 事件面（可监听的事件清单）：`include/effects/event_center.h`
- 机制文档：`docs/02-效果系统/`（内核设计哲学、效果系统）、`docs/07-工具与测试/`（测试指南）
- 规则票（免疫/封技/穿透/无视…）：`include/effects/rule_center.h`

## 说明

- 本包**不含**任何官方魂印/技能实现，也不含内部机制文档与场景语料——
  官方效果实现层的开放路线见仓库 README「开放路线」。
- 示例魂印 id 用 9xxxx 段（官方 effect_id 空间在 1~3000 段），避免撞号。
- 官方效果文本可从公开数据库 dump（`tools/soul_effects_dump.py` 在 `tools/` 里），
  按文本自行实现即可。
