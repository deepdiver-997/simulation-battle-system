# 架构评审与设计哲学 —— 原作游戏对战模拟框架

> 本文记录了一次关于此项目架构的深度讨论。后续任何 AI 阅读此项目时，建议先读本文以理解设计意图和取舍。

---

## 项目状态

一个原作游戏 PvP 对战模拟系统的 C++ 框架。核心架构已完成，包括：
- 41 状态 FSM（完整时点系统）
- 效果注册/执行系统（ContinuousEffect / PendingEffect）
- 异常状态系统（统一施加入口 `apply_anomaly`）
- 断回合系统（epoch 版本号 O(1) 批量失效 + 断回合补偿）
- IControlBlock 接口解耦（FSM 与 IO 分离）
- 动态库插件系统（技能效果和魂印通过 dylib 加载）

**项目当前处于暂停状态**。原因见下文。

---

## 核心设计理念

### 1. 契约式底层效果 > 各自操作 ctx

**原则：让业务逻辑复用定义好的底层效果契约，而不是各自直接操作 BattleContext。**

最成功的例子是 `apply_anomaly()` —— 这是"施加异常状态"的**唯一入口**：

```
技能效果 ──┐
魂印效果 ──┼──→ apply_anomaly() ──→ 7步检查链 ──→ 写入 abnormal_status_end_round
场地效果 ──┘       ↑
              唯一修改点
```

当需要新增异常相关特性（如"异常施加时触发某魂印"），只需在 `apply_anomaly` 加一个回调点，所有调用方自动覆盖。如果每个效果各自写 `ctx->abnormal_status_end_round[target][id] = rounds`，新增特性就需要改 N 处。

**这不是耦合，这是收敛**。调用方表达的是意图（"我要施加麻痹"），而非实现（"修改某数组某位置"）。底层修改实现时，意图不变量保持不变。

项目中已有此模式的：
- 异常施加：`apply_anomaly()`
- 伤害计算：`Calculation::calculateDamage()`
- 断回合：`break_round_effects()`（原语层 `include/primitives/battle_primitives.h`）

尚未统一、值得抽取的：
- 体力回复（各效果各自操作 `pet.hp`，无统一入口）
- 能力等级变化（无统一入口）
- 护盾施加/消耗（无统一入口）
- 效果来源标记（无统一入口）

### 2. 时点是语义锚点，不应无限膨胀

FSM 的 41 个 `State` 枚举值对应官方时点判定流程表中的全部时点。时点是固定的语义节点，效果只决定在**哪个时点注册**，不决定时点本身的存在。

官方游戏 bug 频出的一个重要原因：早期架构简单（没几个时点），后期新特性无法在既有时点内通过优先级和来源区分来交互，于是要么硬编码，要么不断新增时点（"新版回合结束后"等）。结果是时点越来越多且语义相近，交互行为难以预测。

正确做法：**时点稳定，效果在时点内通过优先级 + 来源标记竞争**。

### 3. 效果来源标记是防止"特性盲区"的关键

官方最近出现的一个案例：新魂印"自身存活于背包时，对手场下精灵无法对场上精灵造成影响"。这导致大量已有背包辅助效果失效，但因为一开始没有统一的**效果来源标记**（区分场上/背包/外部），无法系统性地判断哪些效果应被拦截，只能逐个特殊处理。

如果框架层面有：
```cpp
enum class EffectSource { Field, Backpack, External };
```
并在 `ContinuousEffect` 注册时记录来源，新魂印只需在统一的 `can_effect_apply()` 中检查来源标签，所有已有效果自动响应新规则。这是**契约式设计的又一个例证**。

---

## 为什么项目暂停

1. **官方真实逻辑不可靠**：游戏内文字描述不可全信，真实判定逻辑只能靠多方测试反推，且官方频繁修改机制（新精灵的加入会影响旧精灵的判定）。
2. **17 年历史包袱**：遗留内容巨大，做个"和官服一致"的模拟器工作量不切实际。
3. **性价比判断**：框架层面的学习价值已经获取完毕（FSM 设计、效果系统、契约式 API、断回合机制），继续填充技能数据对游戏方向求职帮助有限。

**结论**：框架到此为止是明智的。后续如有兴趣可随意扩展，但不追求和官服一致。

---

## 框架层面的技术亮点（供后续 AI 参考）

### 断回合 epoch 机制
```cpp
// 效果注册时拷贝当前 epoch
effect.valid_id_ = ctx->round_effect_valid_id[owner];

// 断回合：epoch 递增，所有旧效果自动失效
++ctx->round_effect_valid_id[target];  // O(1)

// 执行前检查
if (effect.valid_id_ != ctx->round_effect_valid_id[owner]) return;  // O(1)
```
断回合补偿通过独立的 `BreakCallback` map 管理，不受断回合本身影响。

### Mark vs 回合效果的区分
- **Mark**（`ElfPet::marks`）：不受断回合影响的永久/计数型标记（如异常免疫标记 ID 0）
- **回合类效果**（`ContinuousEffect`）：受断回合影响，有回合数，走 epoch 机制

### FSM 与 IO 解耦
```
Server → IControlBlock (纯虚接口)
              ├── TrainingControlBlock  (单 socket, 无超时)
              └── BattleControlBlock    (双 socket, 30s 超时)
```
BattleFsm 只通过 `control_block_->wait_for_input()` / `async_write()` 与外部交互，不碰 socket。ControlBlock 是 BattleContext 的唯一所有者。

### 三类效果注册表
```
skills_effects[State][side]     → 技能产生的效果，技能结算时注册
soul_mark_effects[State][side]  → 魂印效果，初始化时注册
updater_effects[State][side]    → 回合边界的魂印刷新器（保证进入选择期时本回合效果已就位）
```
> ⚠️ 曾有的 `pending_effects`（延迟/观察型"未来触发器"）**已于 2026-09-15 删除**：
> 它就是高级监控、不响应断回合，能力被**事件中心**（`EventCenter::register_watcher` + `emit`）
> 完全覆盖，且资源端零命中。
同一状态内，魂印效果先于技能效果结算。

---

## 文件索引（关键文件快速定位）

| 用途 | 文件 |
|------|------|
| 中央战斗状态 | `include/fsm/battleContext.h` |
| FSM 状态机 | `include/fsm/battleFsm.h`, `src/fsm/battleFsm.cpp` |
| IO 抽象接口 | `include/fsm/iControlBlock.h` |
| 回合临时数据 | `include/fsm/battleWorkspace.h` |
| 异常类型定义 | `include/abnormal-system/abnormal-types.h` |
| 原语层（异常施加/断回合等原子动作） | `include/primitives/battle_primitives.h` |
| 效果基类 | `include/effects/effect.h` |
| 持续效果 | `include/effects/continuousEffect.h` |
| 伤害修正管线 | `include/effects/damage_pipeline.h`, `pink_damage_pipeline.h` |
| 精灵实体 | `include/entities/elf-pet.h` |
| Mark 系统 | `include/entities/mark.h` |
| 魂印定义 | `include/entities/soul_mark.h` |
| 伤害计算 | `include/numerical-calculation/calculation.h` |

| 架构演进记录 | `docs/01-架构与设计/architecture-evolution.md` |
