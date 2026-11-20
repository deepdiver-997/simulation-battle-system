# EffectUnit 层现状盘点（S22，2026-11-05）

> **一句话结论**：EffectUnit 三件套里，**执行器 + JSON 加载器**是活的且承重（万相乖离 2260 依赖它），
> **运行时文本 parser** 只剩 0.5% 语法覆盖面外加 83 个模板的静默误执行风险——
> **建议：废弃 parser（运行时文本解析通道），保留 loader/executor 作为离线程序数据层**。
> **↑ 裁决改道（2026-11-05 用户拍板）：不删**——parser 保留在树内、代码标注废弃、扩展冻结，
> §6.2 方案封存为翻案手册（详见 §八）。
> 「往复杂语法解析发力」的架构方向被数据否定：语法完整可表达的官方模板只有 **17/2380（0.7%）**，
> 插件 args 驱动模式已用 1/20 的成本达到 92.31% 链级可用率。

## 0. 盘点方法与口径

- 语料：三件套全文（818 行）+ 全部调用点（grep 全查）+ git log 逐提交考古 + effect_info 全量
  2380 行 SQL/脚本句式分类 + registry_night.json（1396 effect id，2026-11-05 快照）交叉比对。
- 分类脚本按 `parse_effect_unit` 的**真实匹配语义**逐条对齐（含主句前文静默丢弃、`{N}回合`
  拒绝保护、未触发兜底三类），不是按文档描述归类；脚本只读 DB，未入库未提交。
- 覆盖率快照由 S17 交付的 `tools/coverage_recompute.py` 现查重放
  〔源: tools/coverage_recompute.py:4-18〕。

## 一、三件套是什么

### 1.1 数据模型（effect_unit.h）

一条 EffectUnit = 「前置条件 → 概率 roll → 原语调用 → 返回值归一化 → if-else 分支递归」：

- **9 个原语标签** `PrimitiveTag`：Anomaly / StatChange / Heal / FixedDamage / PpReduce /
  RemoveRoundEffects / DrainHp / Kill / PowerBoost，各带 param0/param1
  〔源: include/effects/effect_unit.h:17-28〕。
- **7 个前置条件** `UnitCondition`：None / SameElement / FirstMove / SecondMove /
  TargetNoAnomaly / TargetHasAnomaly / TargetHpBelow，先于概率 roll 求值
  〔源: include/effects/effect_unit.h:31-39〕。
- **7 个分支键** `BranchKey`：Success / Immune / Blocked / TargetDefeated / AtCap / Invalid /
  Never——原语细码归一化后选 on_success/on_immune/on_blocked/on_other 四条递归分支
  （分支子单元指针指向同一 storage）〔源: include/effects/effect_unit.h:42-72〕。
- 设计意图写在头文件注释里：**Type B 补偿（被免疫/被阻 → 补偿）落在 on_immune 分支**；
  "数据驱动方向：EffectUnit 后续从 effect_info 模板 + effect_meta 生成"——即官方文本 → 可执行单元
  〔源: include/effects/effect_unit.h:7-15〕。

### 1.2 执行器（effect_unit.cpp，304 行）

`execute_effect_unit` 主流程：条件求值（不满足走 on_other）→ 概率 roll（chance_value 字面优先、
chance_arg 指向 args 下标；**只有 Anomaly 类概率过亮节闸门**，2026-09-22 口径）→ run_primitive
调原语 → 细码归一化 → 递归分支〔源: src/effects/effect_unit.cpp:227-268〕〔源: src/effects/effect_unit.cpp:17-39〕。
另有三个配套件：

- `execute_effect_unit_unconditional`：跳过本单元顶层条件的变体
  〔源: src/effects/effect_unit.cpp:274-277〕。
- **单元准入门**（进程全局钩子表）：`add_unit_admission` / `unit_admission_grants`，
  注册方为 EffectFactory 与 SoulMarkManager 两处（moves_lib / soul_lib 经
  `IEffectRegistry::registerUnitAdmission` 写入）〔源: src/effects/effect_unit.cpp:279-304〕
  〔源: src/effects/effect.cpp:319-321〕〔源: src/entities/soul_mark_manager.cpp:297-299〕。
- Kill 标签走 `force_hp_to_zero` 秒杀族统一入口（免疫票/瞬杀抑制/EVENT_HP_TO_ZERO），
  旧直写 hp=0 版本因绕过体系被删〔源: src/effects/effect_unit.cpp:179-190〕〔源: e79f5aa2 提交注〕。

### 1.3 文本 parser（effect_unit_parser.cpp，230 行，2026-11-05 废弃标注后）——"两刀"

`parse_effect_unit(info, skill_args, storage)` 把官方模板文本解析为单元树，**能接的句式**：

- **第一刀·StatusInflict 主句**：`{n}%令/使对手/自身{状态}` 四种模式；概率取主句前最后一个
  `{n}%`；时长**写死 param1=2 回合**；前缀含"对手处于异常状态" → 补 TargetHasAnomaly 条件
  （1248 类）〔源: src/effects/effect_unit_parser.cpp:73-107〕〔源: src/effects/effect_unit_parser.cpp:203-208〕。
  **占位符后紧跟"回合"的模板拒绝**（堕煞误挂烧伤事故后的保护，见 §四）
  〔源: src/effects/effect_unit_parser.cpp:88-95〕。
- **未触发兜底**：主句后切"，未触发则"子句，只认三类——递归异常 / 恢复自身（1/{n} 或全部）/
  附加{n}点固定伤害，挂到 on_other〔源: src/effects/effect_unit_parser.cpp:110-141〕〔源: src/effects/effect_unit_parser.cpp:211-221〕。
- **第二刀·无相谛 5 类条件模板**：措辞硬编码匹配——179 若属性相同→威力提升 / 700 先出手→
  降全 PP / 1083 后出手→消回合 / 1257 无异常→吸取 / 456 低体→秒杀
  〔源: src/effects/effect_unit_parser.cpp:145-195〕。

**没接的**：StatChange（原语存在但无文本 pattern，强化/弱化模板一律不解析）、一切回合窗口
（`{0}回合内…`）、一切 watcher 条件（被消除/被攻击/使用技能…）、护盾、概率递增、双序参数、
damage 管线类、seal/无效类、切换类。解析失败返回 -1，调用方维持跳过
〔源: include/effects/effect_unit_parser.h:16-19〕。

### 1.4 JSON 加载器（effect_unit_loader.cpp，136 行）

`load_effect_unit_from_json` 从 `custom_effect_programs.unit_json` 加载一棵单元树，
schema 支持全部 9 原语 × 7 条件 × 4 分支 + `{"arg": n}` 参数占位符（对齐文本模板的 `{n}`）；
预先 count_nodes reserve 保证分支指针零 realloc 悬垂
〔源: include/effects/effect_unit_loader.h:9-27〕〔源: src/effects/effect_unit_loader.cpp:88-137〕。

### 1.5 执行路径（谁加载、谁执行、什么时点）

全部在 `Skills::loadSkills()`（**每个 Skills 对象构造时一次**），逐 effect_record 三级瀑布：

1. 选择期硬编码路由表（~40 个条件先制/必中/必暴 id）→ continue
   〔源: src/entities/skills.cpp:554-658〕；
2. **认证数据层**：`load_custom_program(effect_id, skill_id)` 命中 → JSON loader 构造单元 →
   注册 `effect_run_parsed_unit` → **continue（旁路下方一切路径）**；custom_override 目前仅
   查询接缝（dispatch 待 seeds）〔源: src/entities/skills.cpp:660-687〕
   〔源: src/db/official_data_repository.cpp:275-304〕；
3. `clone_effect` 查插件注册表：命中 → 正常注册；
   **未命中（logic==nullptr）→ `parse_effect_unit` 兜底**，解析成功同样注册通用执行器
   （args.extra 指向 `parsed_units_` 内单元）〔源: src/entities/skills.cpp:715-732〕。

执行期：技能释放 → `Skills::execute` → `register_branch` 把 HIT/INHERENT 分支桶里的节点
注册到对应时点桶（ContinuousEffect）；注册时**逐单元询问准入门**，放行则换成
unconditional 执行器（万相乖离"取消进度"语义，不改技能对象）
〔源: src/entities/skills.cpp:1327-1388〕〔源: src/entities/skills.cpp:1350-1359〕。

**parser 与 skills.cpp 的兜底是同一个东西**——skills.cpp 里没有第二个 parser，"兜底 parser"就是
`parse_effect_unit` 本体，二者不是分工关系而是同一函数的调用关系。百科 05 章 §六"三通道"
表述中的通道③即此〔源: docs_local/docs/10-游戏百科/05-技能效果宏族.md:297-300〕。

## 二、git 考古：谁写的、什么时候、为什么停

| 日期 | 提交 | 内容 |
|---|---|---|
| 2026-08-22 | `98c766a6` | 第一刀：执行器 + stat_change 原语（"Type B 补偿"）〔源: 98c766a6〕 |
| 2026-08-22 | `00908a7b` | parser 第一刀：StatusInflict+未触发兜底。"解析器让'未注册函数'的效果（此前 skills.cpp 直接跳过）数据驱动地进引擎"〔源: 00908a7b〕 |
| 2026-08-23 | `42700a6f` | 第二刀：无相谛 5 类条件模板 + PowerBoost 视图层〔源: 42700a6f〕 |
| 2026-09-12 | `2f145b0b` | JSON 加载器。"离线编码、不碰脆弱官方文本解析"〔源: 2f145b0b〕 |
| 2026-09-17 | `e79f5aa2` | **批 1（转折点）**：头部 7 家族（≈2100 技能引用：47 免弱窗 671 技 / 31 连击 303 技 / 521 反转 290 技 / 180 消回合 288 技 / 447 保底 131 技 / 16 古早睡眠 119 技 / 456 秒杀 109 技）——只有 456 用了 parser 已覆盖的路径，其余全部走**插件/内核直通**，parser 未被选为批产载体〔源: e79f5aa2 提交注〕 |
| 2026-09-18 | `870ff81a` | 万相乖离 2260 改造为单元准入门（EffectUnit 基建的正向用户）〔源: 870ff81a〕 |
| 2026-09-22 | `ad38aba8` | 概率闸门管道接入 roll_chance〔源: ad38aba8〕 |
| 2026-09-27 | `3df86c56` | **parser 唯一一次线上事故修复**：`{N}回合`后缀误解析——堕煞（2321 族）"令对手{1}回合内属性技能无效"的 {1}=2 被当异常 id 施加烧伤〔源: 3df86c56〕〔源: src/effects/effect_unit_parser.cpp:88-95〕 |
| 2026-09-30 | — | 之后 effect_unit* 三件再无功能性提交；最后一次触碰是 2026-10-25 RNG sed 机械迁移（`5e7f2a2a`）〔源: git log〕 |
| 2026-10-01 起 | — | J 组插件批产立项（10 族 ≈129 宏/1107 载体），parser 正式让位〔源: docs_local/docs/05-任务清单/已收官/J组批量效果工单.md:1-3〕 |

**活跃窗口 = 2026-08-22 ~ 2026-09-27，五周；单人（zhr）作品。**

## 三、覆盖面实测：EffectUnit 语法能表达官方模板的百分之几

### 3.1 分类结果（2380 模板全量，逐条对齐 parser 匹配语义）

| 类别 | 模板数 | 占比 | 引用加权* | 占比 | 语义 |
|---|---|---|---|---|---|
| **FULL 完整可表达** | 12 | 0.5% | 207 | 0.4% | 主句+兜底全覆盖，前后文均安全 |
| **SECOND 第二刀** | 5 | 0.2% | 221 | 0.5% | 179/700/1083/1257/456 |
| PARTIAL·前文条件/窗口被丢弃 | 190 | 8.0% | 2164 | 4.6% | **危险**：`{0}回合内若X则{1}%令对手{2}` 被解析成无条件施加 |
| PARTIAL·主句外子句被丢弃 | 20 | 0.8% | 211 | 0.4% | **危险**：概率递增/翻倍/刷新等附加语义静默丢失 |
| PARTIAL·兜底不可表达 | 29 | 1.2% | 244 | 0.5% | **危险**：主句执行、未触发兜底丢失（917 即此类） |
| REJECT·回合窗口保护拒绝 | 14 | 0.6% | 164 | 0.3% | 安全跳过（堕煞事故补丁） |
| NONE·无主句不接 | 1805 | 75.8% | 37164 | 79.1% | 安全跳过（parser 存在前后行为一致） |
| NONE·有兜底句式但无主句 | 305 | 12.8% | 6243 | 13.3% | 安全跳过（兜底只挂主句后） |

\* 引用加权 = moves.side_effect JSON 展开去重后的载体技引用数（46997 refs）；
另有 379 refs（0.8%）指向无 effect_info 行的 id（如裸连击 31），记"未分类"。

**核心数字：语法完整可表达 = 17 模板（0.7%）／428 引用（0.9%）。**
危险匹配面（会**执行但语义被静默改写**）= 239 模板（10.0%）／2619 引用（5.6%）。

### 3.2 FULL 类清单（全部 12 条）

1605 `{0}%令对手{1}`、756 `命中后{0}%令对手{1}`、2196 `{0}%令自身{1}`、1907 `{0}%令对手{1}`（无百分号变体）、
1248 `对手处于异常状态时{0}%令对手{1}`、1035/936/803/1600/1823/2533/2471（主句+未触发兜底组合）。
头部两模板 1605/756 合计 87 引用——**这是 parser 全部真实价值所在**。

### 3.3 运行时"活着的 parser"只有孤岛

把 FULL/SECOND 集与 registry_night.json（插件已实现集，1396 id）求差：

- FULL 类只剩 **1907**（1 模板 1 引用）未被插件覆盖；
- SECOND 类只剩 **179**——而它已被 J315 处置锚定到 `skill_id=37460` 单行 + 其余 17 载体走
  `j_cond_power_rewrite.cpp` 插件（registry 快照早于该合并，树内已有实现）
  〔源: docs_local/docs/06-交接/夜跑晨报-2026-11-02-a04-orch.md:43-44〕
  〔源: resources/moves_lib/j_cond_power_rewrite.cpp:50〕；
- 6 条无相谛链（37460 = [179,700,1083,1257,1248,456]）走 custom_effect_programs JSON 加载，
  **parser 对它们已死**（custom_prog 命中即 continue，先于 parser）
  〔源: docs_local/docs/03-数据与数据库/custom_effect_seed.sql:6-11〕
  〔源: sqlite 实测 moves.id=37460 side_effect="[179,700,1083,1257,1248,456]"〕。

**即：今天在真实战斗里由 parser 实际驱动的官方模板 ≈ 1 个（1907）。**

### 3.4 危险面：83 个模板的静默误执行（爆炸半径）

PARTIAL 类未被插件覆盖的部分（registry_night 口径）= **83 模板 / 260 载体技**。这些效果一旦
被模拟池里的精灵使用，parser 会注册并执行**语义子集**：

- 646 `体力高于对手时，此技能命中后100%使对手{0}` → 体力条件被丢，100% 无条件施加；
- 889 `消除对手回合类效果，消除成功则令对手{0}`（92 引用，已在 registry，安全）；
  未覆盖同族则是"消除门槛丢失"；
- 917 `命中后{0}%使对手{1}，未触发则附加自身最大体力值1/{2}的百分比伤害` → 兜底丢失，
  100% 档直接上异常——J129 场景实测踩中（37377 链上 917 被解析活、100% 上寄生）
  〔源: resources/moves_lib/j_hit_roll_fallback.cpp:17-21〕〔源: docs_local/docs/00-组件导航/常识与工具速查.md:144〕。

这就是派发模板里"第四查（parser 覆盖面）"的由来：每个新插件单都要先查链上会不会有
未注册兄弟被 parser 寄生——**parser 的存在本身成为全体 J 单的固定税**。

### 3.5 与插件模式覆盖率的对照

插件 args 驱动模式现状（S17 工具现查，2026-11-05 20:55）：
链级可用率 23891/25880 = **92.31%**（非空链口径）／23891/27307 = 92.72%（含空链）／
87.49%（混搭口径）；id 级注册率 64.82%（定义面）；registry moves=1396
〔源: tools/coverage_recompute.py 运行输出〕。派发消息所称"95%+"为早期口径，
**引用时必须带分母**（百科 05 章 §3.2 步骤 3 纪律）〔源: docs_local/docs/10-游戏百科/05-技能效果宏族.md:185-190〕。

## 四、沉睡原因判定：三重证据链

**结论先行：不是能力天花板撞死，而是三件事叠加——①子串匹配官方文本被实测证明脆弱；
②插件模式在批产竞赛中全面胜出；③parser 自身变成批产的风险源。**

1. **脆弱性实证**：parser 活跃期内唯一的功能修复就是事故修复——堕煞（2321 族）
   "令对手{1}回合内…"的 {1} 被当异常码误挂烧伤，修复方式是给 parser 加拒绝保护而非扩语法
   （`3df86c56`，2026-09-27）〔源: 3df86c56〕〔源: src/effects/effect_unit_parser.cpp:88-95〕。
   同月自定义程序侧又出 X2 修正（1248 行把概率当异常码、chance_arg 又踩 owner 前缀坑，
   seed memo 记档）〔源: docs_local/docs/03-数据与数据库/custom_effect_seed.sql:10〕。
   "运行时官方文本解析"这条路被 loader 提交语明确定性为"**脆弱**"
   （"离线编码、不碰脆弱官方文本解析"，`2f145b0b`）〔源: 2f145b0b〕。
2. **批产竞赛败给插件**：2026-09-17 批 1 是显式转折——头部 7 家族 ≈2100 技能引用里只有
   456 走 parser 路径，其余直通插件/内核〔源: e79f5aa2〕。10-01 起 J 组用
   "每单一家族 + 双节点模板 + 审计槽"范式 5 周打了 268 单、registry moves 775→1396（+621）
   〔源: docs_local/docs/05-任务清单/已收官/J组批量效果工单.md:1-3〕〔源: tools/coverage_recompute.py 输出〕。
   parser 的"未触发兜底/Type B 补偿"概念被插件侧以双节点 + `!anomaly_landed` 申报制吸收
   （J3 明言："on_other 未触发分支（effect_unit 模板'未触发则'**或** SKILL_EFFECT 内 if !landed"））
   〔源: docs_local/docs/05-任务清单/已收官/J组批量效果工单.md J3 行〕——**概念活了，载体死了**。
3. **从兜底变风险源**：J129 实测确立"载体须选纯链"纪律（917 寄生事故），速查§五把
   "查 parser 覆盖面"写进选载体流程〔源: docs_local/docs/00-组件导航/常识与工具速查.md:144〕。
   一个本该扩大覆盖的组件，最终以"让所有批产单多查一项"的方式留在工程里。

**不是纯实验遗留**：执行器+加载器有真实承重用户（2260 万相乖离六条件取消链 +
custom_effect_programs 6 行），准入门是魂印侧取消进度的正式机制（`870ff81a`，场景 010 承载）。
**也不是没有数据**：effect_info 2380 行模板 + moves 参数全在库里，数据一直有——缺的是
语法对得上号的句子。

## 五、与两个邻居的关系

### 5.1 与文本 parser 兜底：同一物

见 §1.5 末段——skills.cpp 兜底 = `parse_effect_unit` 本体，无第二实现。

### 5.2 与 custom_effect_programs / custom_effect_overrides：同源（EffectUnit 数据通道的另一半）

- `custom_effect_programs.unit_json` 的消费方就是 `load_effect_unit_from_json`，执行器共用
  `effect_run_parsed_unit`/`execute_effect_unit`——**两表与三件套是同一体系的代码面与数据面**，
  非两套机制〔源: src/entities/skills.cpp:669-687〕。
- **J315 旁路事故的根源正在这条数据通道**：loadSkills 对 custom_prog **命中即 continue**，
  短路其后的插件注册与文本 parser。2026-10-01 迁移遗留的 179 通用行把后来落地的
  J51/J88 插件整体遮蔽（"插件被解析路径遮蔽"悬案），2026-11-02 破案：行重锚 skill_id=37460，
  其余 17 载体走插件〔源: docs_local/docs/06-交接/夜跑晨报-2026-11-02-a04-orch.md:43-44〕。
- **旁路风险是否同样存在？是，且是结构性的**：任何一行 custom_effect_programs 都会在未来
  某个插件落地时变成静默遮蔽源。现行缓解 = 行带 memo + scope 锚定（6 行全带 rationale）+
  J 单三查里查 custom 表（J318 明查 "custom_effect_programs 0 行"）〔源: docs_local/docs/05-任务清单/已收官/J组批量效果工单.md J318 行〕。
  风险受控但**不会因废弃 parser 而消失**——它的载体是数据行，不是 parser 代码。
- `custom_effect_overrides`（17 行）与 EffectUnit 执行器**无代码耦合**：window 族由
  effect_meta 消费、anomaly_channel/immunity_tier 由异常系统消费——是独立于 EffectUnit 的
  元数据纠偏层，不在本次废弃/保留讨论范围内〔源: src/effects/effect_meta.cpp:221〕〔源: include/fsm/battleContext.h:923〕。

## 六、三选一论证

### 6.1 补全（达到"可表达 90% 官方模板"）——**不成立**

- **缺口清单**：从 0.7% 到 90% = **≈2125 条新 pattern 规则**。分类显示长尾语法高度异构：
  消除成功则/吸取成功则（门槛复合）、`{0}回合内`（窗口）、watcher 条件（被消除/被攻击/用技/
  切换）、概率递增/翻倍、双序参数（速查§三属性排列坑）、护盾/护罩、seal、damage 条件、
  暴击票、pp 窗……每一类都要新原语或新条件词表，且原语层要补
  窗口票/watcher 订阅/审计槽/管线钩子——**EffectUnit"一单元一时点一动作"的模型装不下**。
  现有 92.31% 可用链里，选择期路由（先制/必中/必暴 40+ id）、SKILL_USED watcher 条件窗（J5）、
  消除门槛族（J1/J2）、姿态机（J11）等全部依赖 watcher/多时点/审计槽，均超出单元模型表达域
  〔源: src/entities/skills.cpp:554-658〕〔源: docs_local/docs/05-任务清单/已收官/J组批量效果工单.md J5/J11 行〕。
- **工作量**：以 J 组实测单产折算——每条 pattern rule 含语法、误执行审计、场景回归约
  0.5-1 人日（J 单一家族 5-24 宏 = 1 agent/夜），2125 条 ≈ **1000-2000 人日**；
  对照插件模式 5 周 268 单达到 92.31%，补全路线成本高 **一个数量级**、天花板反而更低。
- **收益核算**：收益 = "长尾批产不再逐个写插件"。但长尾本来就是家族化的（J 组立项语：
  10 族 ≈129 宏/1107 载体）〔源: docs_local/docs/05-任务清单/已收官/J组批量效果工单.md:3-4〕，
  插件家族单的单产已把逐个成本摊薄；parser 唯一真实优势场景（无参数简单施加）只有
  1605/756 两个模板 87 引用，收益面 ≈ 0.2%。
- **风险**：子串匹配的 PARTIAL 误执行面随语法扩面同步增长——每加一类 pattern 就多一类
  "长得像但语义不同"的邻居（堕煞事故即此型）。10% 的现有 PARTIAL 率外推到 90% 覆盖面，
  静默错效果量级不可接受。

### 6.2 废弃——**建议采纳，但范围要切准：废 parser，留 loader/executor**

> **裁决改道（2026-11-05）**：用户拍板**不删**——保留代码 + 标注废弃 + 冻结扩展。本节方案
> 封存为「翻案手册」：若未来危险面失控或训练数据生产前需要清理第三真相源，按本节迁移
> 路径执行即可（工作量估算仍有效）。

EffectUnit 三件套不是一个可整体废弃的东西，拆开看：

**废弃：运行时文本 parser 通道**（effect_unit_parser.cpp/.h + skills.cpp:716-732 兜底调用）

- 迁移路径：
  1. 删 parser 两文件与 loadSkills 兜底段（≈250 行）；
  2. 1907（唯一活模板，1 引用）迁 `custom_effect_programs` 一行 JSON
     （`{"tag":"Anomaly","chance_arg":{"arg":0},"param0":{"arg":1},"param1":2}`）或并进
     1605 的插件——半日内的事；
  3. 场景改造：scenario_010（2260 准入门链走 JSON 装载不受影响，仅删 parser 相关断言）、
     scenario_2230（J129 场景本就在验证"parser 寄生不存在"的纯链语义，改后断言更强）、
     test.cpp 的 parser 单测段删除〔源: test/scenario/scenario_010_combo_condition_cancel.cpp:1-37〕；
  4. 备案：百科 05 章 §六通道③删除、"三通道"改"两通道"；速查§五三条通道行更新
     （J129 的"第四查"永久消失）；派发模板删 parser 覆盖面检查项；
  5. 回归：868 场景全量 + registry moves 只增不减（1907 迁移后 +0 或经插件 +1）。
- 收益：**消灭第三真相源**（官方文本的第三种解释权）；消灭 83 模板/260 载体的静默误执行面；
  全体后续批产单免掉第四查固定税；coverage_recompute 的"registry 未引用/长尾"口径
  少一层"其实被 parser 兜着"的解释负担。
- 工作量：**0.5-1 人日**（一次授权轮可完成，非内核深水区——parser 是叶子模块，
  无 ABI 面、无插件依赖）。
- 数据清理：无（custom_effect_programs 6 行全保留，与 parser 无关）。

**保留：effect_unit.cpp（执行器）+ effect_unit_loader.cpp（JSON 加载器）+ effect_unit.h
（数据模型）+ 单元准入门**

- 承重证据：万相乖离 2260 六条件取消链 = custom_effect_programs 六行经 loader 装入
  `parsed_units_`，准入门逐单元取消条件（进度计数器）——这是魂印侧"改技能行为不改技能对象"
  的唯一通道〔源: docs_local/docs/03-数据与数据库/custom_effect_seed.sql:6〕
  〔源: resources/soul_lib/wuxiang_2260.cpp:93,253〕〔源: src/entities/skills.cpp:1350-1359〕。
- 定位转为**离线编码效果程序的数据层**（认证数据层原文即此定位："替代运行时官方文本解析"）
  〔源: include/effects/effect_unit_loader.h:9〕。它天然是「复杂语法解析」的反面：
  程序离线写好、运行时只装载不解释——这恰是 J315 事故后该体系的安全形态。

### 6.3 维持现状——什么条件下成立

同时满足才成立：①训练/AI 线永不扩大模拟池（83 模板/260 载体永不激活）；
②J 组接受第四查永久税；③接受"官方文本第三解释权"与插件/JSON 并存的口径成本。
①已被训练线推进证伪（模拟池随批产扩大），②③是无期限负债。**不成立。**

### 6.4 与台账字段解法的边界

台账字段（ws 槽 + 审计槽，如 raw_attack_damage/审计键）解决的是**观测与断言**（场景无随机
精确断言、跨时点记账）；EffectUnit 分支解决的是**控制流**（细码归一化后选路）。两者正交且
已合流：J317 概率补偿族用"双节点模板 + 零槽 SUCCESS 判未触发 → 次数票 + 审计槽"实现了
parser on_other/Type B 想做的一切，且可断言〔源: docs_local/docs/10-游戏百科/05-技能效果宏族.md:80〕。
未来官方效果若真出现"子句引用前序执行结果"类需求，正确载体是**插件读审计槽**（现成机制），
而不是复活 parser——组合语义在代码侧以插件表达已被 268 单实证可控，在文本侧则被
堕煞/J129 两次事故实证不可控。

## 七、总判定

**废弃运行时文本 parser，保留 EffectUnit 执行器 + JSON 加载器作为离线程序数据层。**
一句话理由：语法覆盖 0.7% 的解析器不值得养，83 模板/260 载体的误执行面与全体 J 单的
第四查税却在逐夜计息；而体系里真正承重的部分（万相乖离 + 六行离线程序）恰好不需要 parser。
「往复杂语法解析发力」应当否决：组合表达的正确形态是**插件代码 + 离线 JSON 程序 +
台账/审计槽**（已有 92.31% 实证），不是运行时解析官方文本。

**2026-11-05 用户裁决（终局口径）**：上文「废弃 parser」建议被改道执行——「不往复杂语法
解析发力」照单全收；「删 parser」改为「**保留 + 标注废弃 + 冻结扩展**」：代码留在树内才能
随时提醒这条路的存在，未来使用条件满足时可直接翻新，只靠 git 历史会遗忘。落地见 §八。

---

### 附：分类数字复核口径

- 分类脚本逐条对齐 parser 匹配语义（主句四模式 × UTF-8 定位 × `{N}回合` 拒绝 × 前文
  安全词表〔命中后/技能使用成功时/对手处于异常状态时等〕× 未触发兜底三型），2380 行全跑；
  引用面由 moves.side_effect JSON 展开（46997 refs，含 friend 侧未计）。
- registry 快照：build/registry_night.json（1396 moves id，2026-11-05）；179 的树内插件
  （j_cond_power_rewrite.cpp）晚于该快照合并，差集判断已人工校正。
- effect_info 2380 行、custom_effect_programs 6 行、custom_effect_overrides 17 行均为
  2026-11-05 sqlite 实测。

## 八、裁决记录（2026-11-05，S22 收口）

**裁决：运行时文本 parser 不删——保留在树内、代码标注废弃、扩展冻结。**

- 用户要义：git 历史虽在，但删了「到时候不一定想得起来」；留在树里既是一条随时可拿出来
  的路，也是对「这条路存在」的持续提醒。
- 落地动作（本日完成）：
  1. `include/effects/effect_unit_parser.h` 头注废弃块（含现状数据与冻结纪律）；
  2. `src/effects/effect_unit_parser.cpp` 文件头指针注；
  3. `src/entities/skills.cpp` 兜底调用点注解（唯一调用方，原两行注释内折叠改写，行号零漂移）；
  4. 速查 §五「三条通道」行补裁决指针；
  5. 本文 §6.2 封存为翻案手册。
- **保留 ≠ 风险消失**：83 模板/260 载体静默误执行面继续在，**J 单「第四查 parser 覆盖面」
  纪律不解除**；1907 孤岛继续被 parser 驱动（§3.3 事实不变）。
- 冻结纪律：本通道禁止扩语法/新增 pattern；新效果一律走插件（SKILL_EFFECT）或
  custom_effect_programs 离线 JSON（§6.4 边界结论不变）。
- 翻案触发条件（满足其一即可重启 §6.2 手册）：训练数据生产启动前的大清理、parser 寄生类
  事故复发、官方模板面变化使覆盖成本收益逆转。
- 未采用 `[[deprecated]]` 编译属性：唯一调用点 + 测试调用会常驻编译告警，干扰夜跑净构建
  纪律；注释级标注足够醒目（本节为最终权威口径）。
