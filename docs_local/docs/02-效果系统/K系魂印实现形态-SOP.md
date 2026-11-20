# K 系魂印实现形态 SOP（定版口径）

> **性质**：夜跑立单/动工前必读的实现形态约定。来源 = 2026-10-05 三份晨报的实测记档 +
> 合并决策清单（docs_local/docs/06-交接/合并决策清单-2026-10-05-K系夜跑合并.md）K1-K10 拍板。
> 新证据推翻本文条款时，改本文并在行内注明日期，不回头改历史单。
> 关联内核任务单 T1-T9 见决策清单第三部分。
> **效果形态 → 实现套路的选型目录**（递增/乘区/窗界/凭据/事件检测/资源计数）
> 见同目录《效果实现套路目录.md》——立单动工前先过其 §零决策树，本文管时点/注册/断言口径。

## 一、注册形态（zhr-night 晨报 §二.1，2026-10-05）

1. **每回合节点必须声明进 SOUL_PROGRAM**——SoulMarkManager 更新器每回合 ROUND_COMPLETION
   末尾重注册进魂印桶；arm 节点里经 register_skill_effect 手工挂的桶效果会在更新器重建后
   失效（K12 310 FMR 挂法实测 R2 起不触发）。
2. **对手行为反应不进声明桶**——桶按 (state, 持印者) 配对，对手行动时执行的是对手侧桶，
   持印者条目永不触发。正解 = EVENT 面 watcher（K2 262 同款）。
3. **手工 SoulMark 行参**——bind_soulmark_args 丢弃行参前两位、合并 [own,opp,slot]+行参[2:]。
   场景构造传 {0,0,参...}。**T4 已落地（2026-10-05）**：新代码行参一律用类型化
   `SoulNodeArgs::row_or(i, fallback)`（include/plugin/soul_node_args.h；注册用 SOUL_NODE /
   SOUL_MARK_TYPED，soul_lib/common.h 已统一引入）——错位编译期即报；本条对存量 raw
   EffectArgs 写法仍有效。

## 二、时点口径（K4 四条 + T2 字段，2026-10-05 定版）

1. **preemptive_right 在 FMR 桶执行前已重置**（handle_BattleFirstMoveRight 入口
   `pr = NONE`）→ "上回合谁先手"用内核字段 `ctx->prev_round_preemptive_right`
   （ROUND_COMPLETION 时点快照，T2）；不要再自记 plugin_storage（存量 K12 310 不强制迁移）。
2. **ws.preemptive_level 的 memset 在 MOVE_RIGHT 时点** → "下回合先制±N"必须写 FMR 桶
   （1123/636 同款）；技能效果带下回合先制改变的：**技能效果时点执行 → 再注册进 FMR 桶**，
   不是点技能时直挂先手权时点桶。
3. **威力改写必须在 SKILL_EFFECT 时点**——EVENT_SKILL_USED 时点写 skill_power_view 会被
   ON_SKILL_HIT 的物化覆盖；该时点 skill_use_seq 未自增 → executed_* 门不可读，攻击技门用
   roundChoice+技能表类型（K4 636 直系）。
4. **ROUND_START 桶内 roundCount 滞后一轮** → 窗口类效果用 plugin_storage 窗账本口径
   （K7 实测；T6 落地后窗界可交给注册面，届时本文更新）。

## 三、事件与时点语义（K1/K2/K6/K7 裁定，2026-10-05）

1. **【K1】TAKE_DAMAGE watcher 回调实际在 AFTER_ACTION 才跑**——"对手处于 X 状态"类
   条件查询届时已过期（睡眠在承担方行动处理时被清）。**条件查询类附加效果一律走管线
   DETECT 相位同步查询**（293 改造后落地）；读快照（ev.amount）的 watcher 不受影响。
2. **【K2】命中即醒**——受击方睡眠在命中时点被清（先于 ATTACK_DAMAGE）→ "若对手处于
   睡眠状态"的攻击系条件**恒假**，与官方"打醒"口径一致。"睡眠门"载体立单前先查此坑。
3. **【K6】击杀归因挂 BATTLE_AFTER_DEFEATED**（双侧桶，每杀必经）——
   AFTER_DEFEATING_OPPONENT 在击杀侧 on-stage dead 时被跳过（battleFsm:2213 直接转
   CHOOSE_AFTER_DEATH），K12 319 实测。
4. **【K7】必中语义（官方口径裁定）**：
   - 必中 = 不可能 Miss，**凭证存在时跳过命中率掷骰**（含精准/回避/命中等级/命中率乘区/
     必-miss 窗——H5 记档"必中绕过内核天然满足"即此，这是正确行为不是 bug）；
   - 必中**不保证命中效果**；面对技能无效（封属/盔/龙威）照样按未命中结算；
   - 只有**强制执行**（force_execute）无视命中效果失效——引擎 ②门判定与 ②.5 ③层票只被
     force_execute 绕过，必中不绕（skills.cpp ②.0/②.5），与裁定一致；
   - **失明**已实现（AbnormalStatusId::Blind）：非必中必 Miss；必中 50% 正常 / 50% 命中
     效果失效（skills.cpp ①(b)，2026-09-13 口径）；
   - **失神**已实现（Distraction）：属性技能 50% 无效（SEALED，skills.cpp ②.1）——
     是"技能无效"不是"命中效果失效"；
   - ⚠️ **断言防混**：命中率/封技类断言不得给攻击方置 must_hit（必中会跳过整条命中率链，
     J108 记档"accuracy 造 miss 需同清 must_hit"）；acc 100% ≠ 必中
     （accuracy_fix_100 不打必中标记，弱化仍可 Miss）。

## 四、桶语义（K3，2026-10-05 裁定：统一窗界；T6 已落地 2026-10-21）

- soulmark 桶 = moves 桶同款 TimedBucket，曾因**更新器每回合重注册**使 once/自然过期
  形同虚设 → "持续 N 回合"窗口用**账本显式写/清**模式（K7/K12 验证）。
- **T6 已落地（2026-10-21）**：注册面携带窗界（`SoulMarkNodeRef.duration_rounds`）、
  过期窗更新器跳过重注册、离场清锚再上场重开窗（探针 2994 锁行为）——
  **新代码一律用注册面窗界**，不再手写账本；存量账本载体不强制迁移。

## 五、断言方法论（night-author-4 §二.3/§二.6 + j-prod-3 §三）

1. **威力改写族的伤害比例断言不可对齐**（rewritten 重算重掷 85%~100% 浮动）——必须用
   SKILL_EFFECT 时点字段断言（debug_step 停泊读 view）；addPct 是公式内取整，外部 ×N
   差 ±1%，行为断言只能方向性。
2. **致死/锁血/回满类断言先压低血量**——1M 血 holders 上"重生回满"会空洞通过；
   revive 后断言清级读本体 ability_levels（ws.view_levels 是回合视图未必即时同步）；
   满血 heal 封顶要先压血量。
3. **ws 字段 post-drive 读取陷阱**——ws 每回合 reset，drive() 完再读 ws 字段（如
   preemptive_level）看到的是下一轮状态；挑时点（step_until/停泊）或改观测面。
4. **确定性手法**：must_hit 凭证短路 / 100% 概率短路 / 时长常量（极渊 DS-001 场景口径）；
   50% 掷点用运行期自校准（K12 2920 口径）。

## 六、立单素材甄别（k-prod-3 2026-10-07）

1. **effect_icon tips 为权威，行≠模板错位先核对再立**——同 effect_icon 组内大量行的
   tips 与 effect_info 官方模板**完全不同文**（619/628/1210/1383 等：复合行/错位行），
   不能按"1 宏 N 行"想当然立单。立单前必须 dump 对行、核 tips 与模板一致性；
   模板缺失时按 tips 落地（K194 实证：619 按 tips"50% 暴击+吸血"成功）。
2. **eid 撞号甄别**——soul_lib 宽 grep 命中要逐个看上下文再判"已实现"：
   400=护盾常量、1864=精灵 id、2087=已实现的空元镀。**registry 缺席 + 注释命中 ≠ 未实现**；
   反命题同样成立：registry 缺席 + soul_lib 命中也 ≠ 已实现，逐处看语义。
3. **记档壁垒不是终审判决**——K4 当年"390 五拍参数无法自洽解码"的记档壁垒，已被
   K196 按 monster_table 路线攻克。后续会话遇到旧记档的"不可解码/不可实现"结论，
   可带着新证据再攻，不必当作终审。
