# 效果显示 gate 规范

显示面是引擎状态的**第三消费者**（前端控制台 / AI 观察空间 / 官方对拍），与机制结算共用同一份
BattleContext。本规范定义两类**机器可验证**义务：

1. **数据面**：双方身上参与结算的效果与印记，必须能从引擎状态导出（快照 / 全量状态 / 注册表）；
2. **描述链**：每个导出的 id 必须能经固定渠道解析出描述文案（协议只传 ID，名字客户端解析）。

官方客户端大量效果并不展示——本引擎做**权威超集**：凡参与结算的效果状态一律可导出可显示；
显不显示、怎么分组折叠是展示策略（开工文档 §2.2），不是数据面问题。**显示分类必须是数据声明，
不允许前端硬编码**。

规范前身：`docs_local/docs/07-工具与测试/控制台状态显示-开工.md`（2026-09-27，Stage 1/2 已落地，
Stage 3 即本文 G4）。

## 1. 效果状态分类学

每类运行时状态必须明确三件事：权威存储在哪、从哪个出口导出、描述从哪查。**不允许出现
"参与结算却三处都查不到"的状态**。

| 类 | 例子 | 权威存储 | 显示出口（现状） | 描述源（现状） |
|---|---|---|---|---|
| 回合类效果 | 「下2回合必定先手」 | 时点桶（skills/soulmark/suit/updater_effects 四桶） | fullstate 桶条目 ✅ | 技能空间：effect_info 模板 + `{n}` 代入 args ✅ |
| 次数/回合票 | 免疫票、盔/威/封属 | RuleCenter | fullstate `ruleTickets` ✅ | sourceEffectId 走技能空间模板；类别名走 registry 固定表 ✅ |
| 印记 Mark | 免控标记(0)、蚩庸之锁 | `pet.marks` | fullstate `marks`（id/name/count）✅ | **无固定表**：name 是施加方自传字符串 ❌ |
| storage 计数 | 神谕沧岚泪层数、暴击充能 | `plugin_storage` / `ElfPet::soulmark_storage` / `on_stage_storage` | fullstate `storage` 段（全键+registered 标志）✅、快照 `storage`（仅已登记）✅——G4 2026-11-23 落地 | STATE_DISPLAY/STATE_DISPLAY_FN（`DisplayRegistry`）✅，首批 14 键已登记 |
| 异常状态 | 麻痹、烧伤 | `abnormal_status_end_round` | 快照 + fullstate ✅ | 引擎固定表 `abnormal_status_name_cn` ✅ |
| 事件 watcher 窗口 | j5_arm 族「N回合窗口」（1151/1311 等） | `EventCenter::watchers_`（G3 考古发现，2026-11-23） | 快照 `effects.watchers` ✅（G3 新收编）；fullstate 未导出 | **watcher 无效果 id**（fn 是闭包，注册不携带 source）——窗口维度可导出、归属待 G4（债务 #8） |

次级结构（同样参与结算，现状未导出，归入 G4 范围）：`anomaly_view_layers`（K6 虚拟异常层数）、
`trait_state_`（特性复制/抑制条目）、`hp_consume_marks`（斩杀印记）、`attack_boost_grants`。
这些同时也是 golden 状态链 v1 自认盲区（`include/fsm/state_snapshot.h` 头注）。

### 1.1 显示义务的挂点：驻留显示态，不是注册 id（2026-11-22 用户定稿）

**不是所有效果都需要可见**。义务挂在**驻留显示态**（跨时点存在、影响后续结算的状态）上，
不挂在"注册了效果 id"上：

- **一次性效果**（无 duration 的强化/回复、SKILL_EXEC 在途节点）：引擎里不留驻留状态，
  可见性由事件带承载（`EVENT_STAT_CHANGED`、TAPE 采样）——界面看到的是"等级变了"这个
  结果。这类 id **没有描述义务**。
- **驻留显示态**：回合桶条目（duration>0 未过期）、规则票、Mark、storage 计数、异常状态。
  "身上挂着什么"的显示问题只对这类存在。
- **禁止空描述凑数**：为过 gate 写空串/占位文本 = 污染描述链（消费端无法区分"一次性"
  与"忘了翻译"）。"无描述"是白名单显式登记的合法终态，仅此一条路。

**桶条目 effectId 的语义按桶不同**（消费端选表依据，必须写死在规范里）：

- `skillEffects` 桶：effectId = **effect_info 主键**（技能效果空间），模板代入用 `args`；
- `soulMarkEffects` 桶：effectId = **合成节点 id**（`魂印id × 100 + 节点序号`，见
  `src/entities/soul_mark_manager.cpp` `register_soul_effect`），**不是任何空间的查表键**；
  描述必须按 `sourceId`（魂印 id）查 effect_icon tips；
- `suitEffects` 桶：按 `sourceId`（套装 id）解析——注意 DB `suit.suitdes` 是**故事文本**，
  不是效果描述，套装效果描述**无现成表**（实测 462/476，2026-11-22）；
- `ruleTickets.sourceEffectId`：技能空间效果 id；
- 三个 id 空间数字重叠（effect 1004 ≠ 魂印 1004 ≠ 套装 1004），**线协议里必须能消歧**（见 C2）；
- ⚠️ **"回合类效果"的权威存储有两处**（G3 考古更正）：桶回合条目（`register_round_effect`
  族：2076/843/1814）与 event watcher 窗口（`j5_arm` 族：1151/1311）。桶里的 SKILL_EXEC
  一次性在途节点 duration=1/isRoundEffect=true，**真窗口判别 = `actionOneShot:false`**
  （fullstate）或快照摘要（一次性节点被滤除）——G2 场景初版只查 isRoundEffect 被误匹配，
  已更正。

## 2. 数据契约（不变式）

- **C1 只传 ID + 数值**：名字/描述解析全在客户端（开工文档 §2.1 定稿）。例外：快照的
  `abnormalStates` 带中文名（44 个固定值，历史决定，保留）。
- **C2 空间可消歧**：任何 id 字段（桶条目 effectId/sourceId、票面 sourceEffectId、storage key）
  最终消费端都能确定它属于哪个注册空间（move/soul/suit/trait）。过渡期靠"桶归属 + sourceId
  兜底"隐式契约（effectText.js 现状）；终态（G4）线协议显式带 `space`/`sourceKind` 字段。
- **C3 剩余量口径**：桶条目只出原始两维 `duration`/`registeredRound`，剩余回合客户端自算；
  票面出 `counts`/`rounds` 剩余值；异常出 `remaining`；Mark 出 `count`（0=永久）。
  消费端按字段名区分，**引擎不在同一字段里混两种口径**。
- **C4 显示分类是数据**：registry dump 的 `display` 段声明每个 id 的显示属性
  （displayability / 分组 / 图标类）。默认=可见；未注册 id 优雅降级显示裸键值，**永不因缺注册崩**。
- **C5 序列化纯只读**：显示出口不得影响战斗结果（tape 同原则）。加字段只许加在
  `getStateJson` / `getFullStateJson` / registry dump 三处只读序列化里。

## 3. 三层出口与最小内容集

| 出口 | 现状 | 本规范要求的最小内容集 | 缺口 |
|---|---|---|---|
| 快照 `getStateJson`（SYNC_STATE/INPUT_REQUIRED 内嵌，"立刻可渲染"） | 血量/等级/技能/全队/异常 | 上述 + **效果摘要段**（`effects.round`/`effects.tickets`/`effects.watchers` + `marks`，按 owner 分组，过滤 expired/invalidated/actionOneShot）——✅ G3 2026-11-23 落地 | 已补齐（storage 类条目待 G4） |
| 全量 `getFullStateJson`（DEBUG_FULLSTATE，按需拉） | 四桶+票+marks+异常+魂印名+结算视图 | 上述 + **storage 段**（三容器全键，registered 标志+解码值）——✅ G4 2026-11-23 落地 | 次级结构（anomaly_view_layers/trait_state 等）仍 ❌ |
| 注册表 dump `build_registry_dump`（--dump-registry-exit / HELLO 指纹） | id 清单+固定对照表 | 上述 + **display 段**（`storageKeys` 14 键 + `markTypes` + `duplicateKeys` 审计）——✅ G4 2026-11-23 落地 | displayability 分级（show/fold/none）待后续 |

快照加段会自动触发 HELLO 的 `stateSchemaHash` 变化报警（`include/server/protocol.h`、
`src/server/output_json.cpp` `version_fingerprint_json`）——该机制就是为"内核加字段显示面没跟上
响亮报警"设计的，G3 落地时不需要额外通知手段。

## 4. 四条门（G1–G4）

每门给：检查内容 / 执行器 / 验收标志 / 分层归属。前两条效果层可独立完成，后两条是内核层。

### G1 描述链静态对账（工具层）—— ✅ 2026-11-22 落地

- **内容**：registry dump 的每个 id（moves/soul/suit）必须能解析出描述：moves → effect_info
  模板；soul → effect_icon tips（**不回退 effect_info**——C2 空间隔离，effect 106 ≠ 魂印 106）；
  suit → display 段（G4 前先人工口径清单过渡）。解析不出 = 红。
  输出按空间的覆盖率报告（与既有覆盖率对账 `build/registry.json` 同构）。
- **执行器**：`tools/ci/display_gate.py`（判定口径与网关 catalog.py 同构，改两边要同步），
  白名单 `tools/ci/display_gate_whitelist.json`（键=id，值=原因，须指认实现文件/口径依据；
  stale/僵尸条目警告）。registry 缺失时自动跑 `--dump-registry-exit` 重导。
- **挂点**：`tools/night/build.sh` registry dump 之后（exit 5），nightly 必跑。
- **验收**：解析失败清单为空（或只剩白名单）。MUT：摘白名单条目转红（已验）。
- **落地基线（2026-11-22）**：moves 1601 可解析 1595 + 白名单 6（官方无模板行的 G8 考古
  效果 21/31/42 + 本地扩展/承接 id 270210/900001/999900）；soul 416 可解析 405 + 白名单 11
  （尾族 106/110/133、击败族 187/201/218 官方无 tips，私有号段 9901001-9901006）；
  suit 4 全白名单（G4 前无效果描述源）。共 21 条债务登记，G4 display 段落地时收编。

### G2 动态显示锁场景（效果层）

- **内容**：场景**手写"应显示清单"**，断言 fullstate/快照里逐条出现。扩
  `test/scenario/scenario_179_fullstate_stage2.cpp` 的模式：那个场景锁的是 Stage 2 数据面契约
  （票面字段、桶条目增强字段、JSON 合法性），显示锁场景把"五类状态各至少一条"都锁进去——
  回合桶条目 / 规则票 / Mark / storage 计数（G4 落地后）/ 异常。
- **执行器**：`test/scenario/scenario_<序号>_显示锁.cpp`，构建 GLOB 自动发现，全量回归覆盖。
- **验收**：场景绿 = 显示面契约在；**MUT 反向锁**——摘除序列化字段或效果实现，场景必须转红。
- **分层归属**：纯新增场景文件 = 效果层，走自动合并通道。
- > 📌 **2026-11-23 G3 更正**：真窗口判别必须加 `actionOneShot:false`（场景 8770 已修）——
  只查 `isRoundEffect:true` 会命中 SKILL_EXEC 一次性在途节点（弱锁）。

### G3 快照契约升级（内核层）—— ✅ 2026-11-23 落地

- **内容**：`getStateJson` 每玩家增 `marks`（场上宠，形状与 fullstate 一致，name 过渡字段）+
  `effects` 摘要段：`round`（四桶真回合窗口）/ `tickets`（活票）/ `watchers`（事件窗口），
  过滤 expired / ON_STAGE epoch 不符 / actionOneShot / 常驻条目（duration<0：魂印本体已在
  soulMark 字段显示，更新器条目是内核机械），remaining 现算——渲染面口径，与 fullstate
  原始两维按字段名区分（C3 不混口径）；按 (effectId, sourceId) 排序保证输出稳定（桶是无序容器）。
- **配套**：`stateSchemaHash` 探针从"顶层键集"改为**全深度键集**
  （`top_level_keys`→`all_json_keys`——原实现只看顶层，player 内嵌段不触发报警，G3 前置修复）；
  `event_center.h` 增 `watchers()` 只读访问器。
- **验收**：场景 8770 扩 D6 断言（快照 round/tickets/watchers/marks 逐类）+ 内置 JSON 结构
  自检（串感知括号深度校验——实测抓过两次序列化结构错，子串断言抓不住）；MUT 摘 effects
  段转红已验；新旧引擎 HELLO 对拍：仅 `stateSchemaHash` 变化、`registryHash`/`protoVersion`
  不动（嵌套加字段已能触发报警）。
- **分层归属**：内核层（2026-11-22 用户定稿：自研不提 issue，分支+本地 merge）。

### G4 storage 导出 + display 段（内核层 + 插件层，= 开工文档 Stage 3）

**实现形状（2026-11-22 用户定稿）：按需遍历（Option B），不建生命周期注册桶。**

显示态不做"注册时提交 + 切换/断回合成攻时重置"的二次记账（Option A 否决理由：派生视图
两份账必然漂移——双注册撞号/Windows 初始化序/golden 链污染都是前车之鉴；且注册埋点要
覆盖每条桶写入路径，漏一处就漏显示）。生命周期规则（过期/断回合/切换作废）已经在桶+
`valid_id` epoch 里实现过，遍历时现查即可（`isExpired`/`invalidated` 位现成）。量级支持：
常规桶条目 10-50 条，fullstate 按需拉取非每帧，TAPE 高频出口本来每时点过一遍 ctx。

- **内容**：
  1. **只读显示态枚举器**（如 `collect_display_state(ctx)`）：四桶（过滤
     expired/invalidated/actionOneShot）+ 票（过滤 expired）+ `pet.marks` + 异常 +
     storage 注册键 + 次级结构，一处聚合；快照/fullstate/tape 三出口都从它取数
     （单一事实来源，C5 只读）；
  2. `STATE_DISPLAY` 自注册宏（仿 SOUL_PROGRAM 先例，静态初始化注册；**跨 dylib 纪律**：
     解码器 header-only 或经 CoreApi 回调表注册，插件不链接 sim_core）；
  3. fullstate 增 storage 段：三个 storage 的注册 key 逐条导出（解码器给
     `{label, value}` 形状；未注册 key 导出裸键值并计白名单）；
  4. registry dump 增 `display` 段（displayability + storageKeys + markTypes + sourceKind），
     网关 `/api/meta` 分发，客户端每注册表版本拉一次；
  5. Mark 收编：mark id → 描述固定表进 `markTypes`，`marks.name` 字段过渡期保留、表齐后退役；
  6. **次数类消耗事件**：把 `EVENT_SKILL_ARMOR_RESOLVED` 模式（被穿也发、带 grant_id）
     推广到免伤票消耗、穿透凭证消耗、管线计数变动——"消耗了不通知"意味着事件带讲不出
     "为什么少了 1 次"；
  7. 首批解码器：DS-001 算力/限制/乱流、圣武英耀/武诛、妙时两态、启灵放逐
     （开工文档 Stage 3 原清单）+ 神谕沧岚泪计数（Stage 2 实测缺口）。
- **验收**：控制台能看到"算力 50 / 乱流激活"类活状态；G2 场景的 storage 计数条目转绿。
- **分层归属**：枚举器/宏/fullstate/dump 改动 = 内核层；解码器登记 = 插件层。
- > ✅ **2026-11-23 落地（核心机制 + 首批收编）**：DisplayRegistry（include/effects/display_registry.h，
>   撞 key 首注保留 + duplicateKeys 审计）+ STATE_DISPLAY/STATE_DISPLAY_FN/STATE_MARK_TYPE 宏
>   （plugin_interface.h 静态表 → flush → IEffectRegistry 尾部两虚槽 → 内核）；**ABI v2→v3**；
>   fullstate `storage`（三容器全键）/ 快照 `storage`（仅已登记）/ registry dump `display` 段；
>   首批 14 键：蚩庸之锁 ×4、逆涛 ×6、空妄诗章 ×2、内核种子暴击充能 ×2 + markTypes[0 异常免疫]。
>   场景 8770 D7/D8 随锁；MUT 摘段红（经 D5 结构自检触发）。
> - ⚠️ **重要新知（std::any 跨 dylib 坑）**：std::any 的 type_info/manager 各 TU 独立实例，
>   内核 generic 解码 any_cast 对**插件写入**的值必抛 bad_any_cast——插件 key 一律
>   STATE_DISPLAY_FN（插件侧解码器，同 dylib 读写才统一），generic 解码只许内核种子用。
> - ⏸ **剩余项**：① 次数类消耗事件（consume_immune 拿 const ctx，发事件需非 const 通道
>   或签名改造，单独立项）；② watcher 归属的描述源（sourceEffectId 已导出，effect_info
>   无模板族描述仍缺）；③ displayability 分级段；④ 前端渲染接线（网关 /api/meta 分发
>   display 段待接）。

### 4.1 次数类台账（散落点对账，防遗漏工具）

次数类的权威存储**本来就按机制分家**——显示面不发明第四个存储，只要求每类持有者
暴露统一的读出口形状 `{sourceEffectId, space, label, remaining}`（+消耗事件）。
逐类对账（G4 施工清单，动一类勾一类）：

| 计数类 | 权威存储 | 现有出口 | 缺口 |
|---|---|---|---|
| 免疫票（含次免传承/不传承） | RuleCenter IMMUNE（coverage/anomalyMask/soul/tier/scope） | ruleTickets ✅ | 消耗无事件（G4-6）；传承性与 scope/soul 字段的映射待对齐 |
| 盔/威/封属 | RuleCenter SEAL 等 | ruleTickets ✅ | 事件已有（EVENT_SKILL_ARMOR_RESOLVED，被穿也发）✅ |
| 穿透凭证 | RuleCenter PEN（pen_level 等） | ruleTickets ✅ | 消耗事件待补（G4-6） |
| 管线增伤/减伤 | `ws.damage_reduce_add/mul`（回合内标量） | fullstate damageReduce（裸值） | **无来源归属**（哪个效果给的，G4-1 枚举器带）；回合清零无事件 |
| 护盾/护罩 | ShieldBank / pet.cover | fullstate shield/cover ✅ | 分项来源待查（bank 分桶对齐 G4-1） |
| PP 消耗 | `skills[i].pp` | 快照/fullstate skills.pp ✅ | 效果侧事件已有（EVENT_PP_REDUCED）✅ |
| storage 计数 | plugin_storage / soulmark_storage / on_stage_storage | fullstate storage 段 + 快照（G4）✅ | 已收编（14 键已登记）；未登记 key 裸键降级 |
| 虚拟异常层数 | anomaly_view_layers | ❌ | G4-1（枚举器收编） |
| Mark 计数 | pet.marks | fullstate marks ✅ | id→描述表（G4-5） |

## 5. 效果贡献者工作流衔接

《效果实现指南》主循环（查库 → 插件 → 场景 → 变异 → 全量门 → PR）新增三条义务
（G4 落地后生效，落地前"先登记后实现"）：

1. **跨时点计数状态必须走有显示身份的载体**：能用桶/票/Mark 表达的不自建 storage key；
   确需 `plugin_storage` 时，key 用 STATE_DISPLAY 注册（含 `{label, decode}`），禁止裸魔数 key
   无显示登记地散落（存量反例：暴击充能 `990238000+owner`，`src/entities/skills.cpp:992`）；
2. **场景带显示断言**：场景若含"挂驻留显示态"路径（上票/加印记/回合类窗口），补一条
   fullstate 含本效果条目的断言（来源字段对得上自己），让显示面与机制同锁。
   **一次性效果不在此列**（§1.1）——它没有驻留态可断言；
3. **描述义务只对驻留显示态生效**：实现的 id 若承载驻留显示态，effect_info 无模板（soul
   空间无 tips）时 PR 里注明缺口——G1 对账把它记入白名单待补清单，不是静默漏。
   **禁止写空描述/占位文本过门**；一次性效果无描述是默认合法状态，无需登记。

## 6. 已知债务清单（2026-11-22 考古基线）

| # | 债务 | 归属门 |
|---|---|---|
| 1 | ~~快照无 marks / 效果摘要~~ ✅ G3 2026-11-23 收编（`marks` + `effects.round/tickets/watchers`） | ~~G3~~ |
| 2 | ~~storage 三表三处出口均不可见~~ ✅ G4 2026-11-23 收编（fullstate 全键 + 快照已登记 + display 段 14 键） | ~~G4~~ |
| 3 | `anomaly_view_layers` / `trait_state_` / `hp_consume_marks` / `attack_boost_grants` 未导出 | G4 |
| 4 | Mark 无 id→描述固定表，name 为施加方自传字符串 | G4 |
| 5 | 套装空间无效果描述源（`suit.suitdes` 是故事文本） | G1（白名单过渡）/ G4 |
| 6 | id 空间消歧靠桶归属隐式契约，线协议无 space 字段 | G4（C2 终态） |
| 7 | 桶条目与 golden 链对 storage 三表同为盲区（`state_snapshot.h` v1 自认）——显示面补齐时**顺手补 golden 链**，一份数据两处收益 | G4 |
| 8 | ~~watcher 窗口无归属字段~~ ✅ G4 2026-11-23 收编（register_watcher 尾参 source_effect_id，快照 watchers 带 sourceEffectId；j5_arm/1311 直调族已传，存量 0=未归属逐步补齐）——watcher 描述源仍缺（见 G4 剩余项） | ~~G4~~ |

> 已销账：~~"disabledEffects.soulMarks 误伤技能效果（46 票消失）"~~ —— R2 复核关单
> （commit `6218a548`、场景 873）：46 张免伤票正常授予且当回合正确挡下+消耗，r0 末
> 快照见不到票是**观测时点伪影**，引擎无缺陷。开工文档 2026-09-30 注记同步作废。

## 7. 实施顺序

```
G1 工具脚本（效果层可先行，白名单过渡套装/无描述 id）—— ✅ 2026-11-22 落地
  → G2 显示锁场景（锁驻留显示态契约，防退化）—— ✅ 2026-11-22 落地（场景 8770）
    → G3 快照契约（内核）—— ✅ 2026-11-23 落地（含指纹探针全深度修复 + watcher 收编）
      → G4 storage + display 段 —— ✅ 2026-11-23 核心落地（机制+14 键+消耗事件/watcher 描述源 defer）
```

G1/G2 不碰 `src/`/`include/`；G3/G4 动内核（2026-11-22 用户定稿：自研分支+本地 merge，
不走 issue 通道）。
每条门落地即在本文勾掉对应债务行，债务清单清零 = 显示面与结算面同源达成。

**G1 收编说明**：债务清单 #5（套装无描述源）与 #4（Mark 无描述表）中「描述源」部分由
G1 白名单承接记账（21 条，见 §4 G1 落地基线），载体缺口本身仍待 G4 display 段解决。
