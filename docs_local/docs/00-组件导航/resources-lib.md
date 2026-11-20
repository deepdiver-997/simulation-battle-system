# resources/moves_lib + soul_lib — 效果插件

**⚠️ 进去看插件前先读这一条：插件 `moves_lib` / `soul_lib` 不链接 sim_core，只能调内联方法与公开字段。**
完整约束见 `CLAUDE.md` §3.9 + §5 坑1 —— 违反会**链接失败**。

| 文件 | 承载 | 入口 | 关联文档 |
|------|------|------|---------|
| `resources/moves_lib/lib_1.cpp` | **技能效果**插件（编译为 `plugins/moves_lib/lib_1.dylib`） | 技能已注册函数：`注册` 效果模板效果函数，`Skills::register_branch` 用。inline 入口 `register_skill_effect`、`grant_penetration`、`grant_skill_invalid` | CLAUDE.md §3.9 |
| `resources/soul_lib/lib_1.cpp` | **魂印效果**插件（编译为 `plugins/soul_lib/lib_1.dylib`） | `registerSoulMarkProgram(id, nodes)` / `registerSoulMarkHooks(id, hooks)` / `registerSoulMark(id, fn)`；场下源→场上目标类效果经统一门 `offstage_source_suppressed(ctx, owner, mark_id)`；源空间 `find_pet_with_soulmark` | CLAUDE.md §3.5、`docs/02/魂印机制设计与精灵表现档案.md` |

## 插件能 / 不能

- ✅ **能**：调头文件内联方法（`grant_immunity`、`register_skill_effect`、`find_pet_with_soulmark`、`grant_skill_invalid`、`grant_penetration`…）；访问公开字段（`ctx->ws`、`ctx->roundChoice`、`ctx->seerRobot`）。
- ✅ **能**：读写 `ElfPet::soulmark_storage[id]`（持久槽，`std::any`）。
- ❌ **不能**：调非内联方法（`clear_on_stage_abnormal_statuses`、`seal_skill` 等）——会链接失败；对策是改用 inline 入口或直写底层字段（如 `ctx->abnormal_status_end_round[owner].fill(0)`）。

**新增精灵机制时：先在引擎找 inline 入口 / 公开字段，没有就补 inline 入口进 `battleContext.h`，别在插件里硬凑。**

> ⚠️ 命名注：实际产物是 `liblib_1.dylib`（CMake `OUTPUT_NAME "lib_1"` 在 mac 上被自动加 `lib` 前缀 → `liblib_1`）。加载器 `SoulMarkManager::initialize` 用 `opendir` 按**扩展名扫描整目录**，因此 `liblib_1.dylib` 正是被加载的插件——**不可删**。双重 `lib` 只是命名不美观，功能正常；若要改名，需同步 CMake `set_target_properties(OUTPUT_NAME)` 与加载约定。