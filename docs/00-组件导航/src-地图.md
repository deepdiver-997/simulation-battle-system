# src/ — 实现层 + 可运行目标

`src/` 与 `include/` **镜像**：多数 `.cpp` 是头文件声明的实现（一一对应），
编译进 `sim_core` 静态库（见 `CMakeLists.txt` 的 `SIM_CORE_SOURCES`）。
这里只标注**非 1:1** 的部分和**可运行目标**，1:1 的查对应头文件即可。

## 与头文件映射

| `src/` 文件 | 实现谁 | 备注 |
|------------|--------|------|
| `effects/*.cpp` | `include/effects/*.h` | 各 effect 内核实现（effect/effect_meta/effect_unit/effect_unit_parser/timed_bucket/damage_pipeline/pink_damage_pipeline；**RuleCenter 为 header-only，无 .cpp**） |
| `entities/pet_factory.cpp` | `include/entities/pet_factory.h` | **DB/插件初始化**（相对路径 sqlite，见 CLAUDE.md §1） |
| `entities/skills.cpp` | `include/entities/skills.h` | 技能加载（`loadSkills` 读 DB + 插件 + parser 接线） |
| `entities/soul_mark_manager.cpp` | `include/entities/soul_mark_manager.h` | 魂印激活/钩子 |
| `entities/numerical-properties.cpp` / `elemental-attributes.cpp` | 对应头 | 数值/系别克制实现 |
| `fsm/battleContext.cpp` | `include/fsm/battleContext.h` | **状态收纳盒** + `execute_registered_actions`（魂印桶先于技能桶） |
| `fsm/battleFsm.cpp` | `include/fsm/battleFsm.h` | **状态机核心**：`stateHandlerMap` + 各时点 handler + `perform_switch` + `handle_BattleAfterDefeated`(EVENT_DEATH 收敛点) |
| `fsm/iControlBlock.cpp` | `include/fsm/iControlBlock.h` | 控制块实现 |
| `primitives/battle_primitives.cpp` | `include/primitives/battle_primitives.h` | 原语实现（断回合/能力/恢复/固伤/施加异常…） |
| `db/official_data_repository.cpp` | `include/db/*` | 官方 SQLite 数据访问 |
| `utils/dynamic_library.cpp` | `include/utils/dynamic_library.h` | 插件动态库加载 |

## 可运行目标（CMake 生成）+ 其它

| 目标 | 来源 | 作用 |
|------|------|------|
| `sim_core` | 上面全部源码（`SIM_CORE_SOURCES`） | 静态库，引擎本体 |
| `sim_server` | `server/server.cpp` + `main.cpp` | 服务端 |
| `sim_training_cli` | `tools/training_cli.cpp` | 训练 CLI |
| `sim_smoke_test` | `test/test.cpp`（gitignored） | 段式冒烟 |
| `sim_scenario_0NN_*` | `test/scenario/scenario_0NN_*.cpp`（**GLOB 自动发现，见 docs/07/场景测试说明.md**） | 场景化对战测试 |
| `moves_plugin` / `soul_plugin` | `resources/*_lib`（本地维护，不入库） | 效果插件动态库 |

**想改"效果在这个时点到底执行谁" → 进 `battleFsm.cpp` 的 handler；
想改"这个效果执行后写哪" → 进效果内核 + `battleContext` 收纳盒。**