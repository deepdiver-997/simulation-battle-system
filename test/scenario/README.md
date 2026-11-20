# test/scenario — 场景索引速览

每个 `scenario_0NN_*.cpp` 是**一个独立的可运行对战测试**(走真实 `BattleFsm`,`main()` 动 `assert`)。
CMake 用 `file(GLOB CONFIGURE_DEPENDS)` 自动发现,新增场景**无需改 CMake**。

构建运行(从项目根):
```bash
cmake --build build -j8
./build/sim_scenario_023_selfdestruct_legacy      # 单场景;退出码 0 = 通过
```

> 完整索引表(被测特性×关键效果 id + 横向能力域归并)见 `docs/07-工具与测试/场景测试说明.md`。
> 新增场景清单/命名约定也在那里。

## 速览表

| # | 文件 | 测什么(一句话) | 例子
|---|------|--------------|
| 001 | `scenario_001_conditional_first_move` | 条件先制 → 决定先手权(低速翻盘) | 大雪纷飞
| 002 | `scenario_002_skill_invalid_compensation` | 技能无效补偿:断回合+消除提升+焚烬 | 希拓先手无效补偿
| 003 | `scenario_003_absorb_stat_mitigate` | 吸取能力提升 + 下N次受击减伤 | 希拓·烬灭神咒剑 吸对手速度→下1次受击减伤 |
| 004 | `scenario_004_penetrate_attack_immunity` | 「无视攻击免疫」穿可穿盔 / 不可穿盔被挡 | 法咒共鸣斩699 无视攻击免疫穿盔 |
| 005 | `scenario_005_force_execute_pp0` | 强制执行凭证:PP=0 必中(绕过 miss) | 无为觉者2260信号→PP0必中 |
| 006 | `scenario_006_soul_event_callback` | 魂印事件回调:对方切换→挂层+按层真伤 | 启灵元神1581神印 对方切换挂层真伤 |
| 007 | `scenario_007_2260_full_program` | 无为觉者 2260 完整子句(程序多时点) | 无为觉者2260完整程序 |
| 008 | `scenario_008_pet_storage_slot` | pet 持久槽 `soulmark_storage` 跨回合/下场 | 无相谛存储信息在pet的any容器内 |
| 009 | `scenario_009_combo_syntax_parser` | 无相谛组合语法第二刀:parser 解析条件模板 | 无相谛179条件威力+170等5模板 |
| 010 | `scenario_010_combo_condition_cancel` | 万相乖离:逐条取消触发条件 + 威力提升 | 无相谛逐条取消条件,威力0→150
| 011 | `scenario_011_pet_switch_armor` | 换宠 + 盔/威/封属(技能无效中心) | 换宠 高速移动被龙威SEALED→换宠洗拦截 |
| 012 | `scenario_012_restraint_multiplier` | 克制倍率修复:官方原始倍率 | 草系打水2x/打草0.5/电打地面0 |
| 013 | `scenario_013_skill_element_view` | 技能元素/克制倍率视图层 | 技能系别克制:草系技能打水2x(非精灵系别) |
| 014 | `scenario_014_elf_element_pink_resist` | 精灵系别半持久视图 + 粉伤抗性 | 改系别跨回合保留 + 暴击抗性削加成 |
| 015 | `scenario_015_anomaly_resist_reflect` | 异常抗性纠正优先级 + 反弹/转化 | 异常抗性击穿魂免 + 反弹/转化异常 |
| 016 | `scenario_016_shield_hood_true` | 护盾/护罩分离 + 无视护盾 + 真伤直通 | 护盾挡红/护罩挡粉/真伤直通 无极圣武无视护盾 |
| 017 | `scenario_017_heal_drain_healblock` | 恢复/吸血/固伤 + 封回血 | 吃月亮:封回血→恢复归零 |
| 018 | `scenario_018_soul_mark_source_query` | 魂印源空间查询原语 | find_pet_with_soulmark 判源场下(薇尔诗2513) |
| 019 | `scenario_019_soul_mark_scope` | 魂印作用域 STAGE/ROSTER + 更新器桶 + on_exit | ROSTER场下触发 vs STAGE场下不触发 |
| **020** | `scenario_020_skill_replace_exec_only` | **技能替换·执行期重定向**(艾欧丽娅 kExecOnly,固有效果保留) | 艾欧丽娅骑士决斗替换技能但保留原固有效果
| 021 | `scenario_021_offstage_suppression` | 场下源抑制场域 + 星皇之赐/之佑 | 薇尔诗抑制星皇之赐回血
| **022** | `scenario_022_skill_replace_pre_selected` | **技能替换·前置替换**(米修莉 kFull,失去天生先制) | 米修莉·入魅印记 把对手技能转成「摸摸」 |
| 023 | `scenario_023_selfdestruct_legacy` | 自爆传承:牺牲自己→下只登场精灵获一次不可穿盔 | 尤纳斯 虚空附体
| 024 | `scenario_024_undying_aigens` | **不死锁血**:受致命伤锁1血+清双方提升/回合类, **全场仅一次** | 不灭·艾恩斯1067 免死 |
| 025 | `scenario_025_lowhp_lock_medicine` | **锁1血精确**(对手体力不高于我→③不回血) + 嗑药即时生效 | 艾恩斯1067 + 恢复PP药 |
| 026 | `scenario_026_guaranteed_first` | **必先(guaranteed-first)**:先手权双参照(必先→先制→速度),可被断回合移除 | 10630 光闪击 对照 / grant_guaranteed_first |
| 027 | `scenario_027_xiaoqiang_guaranteed_first` | **消强成功→下一回合必先**(条件触发); 未成功不触发 | moves_lib effect_xiaoqiang_bixian / effect 999900 |
| 028 | `scenario_028_fengxianzhihuo_stat_change` | **通用 effect 5** `{n}%改变对手属性等级`(走 stat_change 真原语) | 艾恩斯奉献之火31269 (攻击/特攻-2) |
| 029 | `scenario_029_zhixu_zhizhu_hit_invalid` | **封属·命中失效** `SEAL_ATTRIBUTE_HIT`:大雪纷飞被命中失效, **正常效果不触发 且 2006无效补偿不触发**(有补偿却不触发) | 艾恩斯秩序之助31268 消回合→2回合属性命中失效 |
| 030 | `scenario_030_aigens_finish` | **艾恩斯收尾**:反转原语(`stat_reversal` 下降→提升)+ 王酷低伤焚烬(1256) + 随机固伤(683) + 697穿透 | 31272王酷/31271诸邪/31270不灭 |
| 031 | `scenario_031_count_immune_damage_block` | **次数型免伤接入伤害管线**(`BLOCK` 阶段/类别):counts=1/2 逐次挡下并消费; **挡伤失效掩码置位→不触发也不扣次数**; 同掩码下减伤(MITIGATE)仍生效; BLOCK 归零后 DETECT 受高伤不触发 | 谱尼虚无 / 沧岚"抵挡下1次攻击" + 蚀砚之泪≥4滴 |
| 032 | `scenario_032_puni_lunhui_rollback` | **圣灵谱尼 1038「轮回」状态回档**:致死残留1体力(不死) → 回合结束后 体力/PP/能力等级**回撤到"当回合选择操作之后"**; 每场仅1次(第2次致死正常阵亡且回档不复活) | 谱尼七刻印1038 轮回 |
| 033 | `scenario_033_transfer_stat_boosts_protect` | **转换强化原语**(`transfer_stat_boosts`)+ 免消除强化 `STAT_CLEAR`:效果85转化/1287吸取同一通道, **消除/转化/吸取三入口都查**, 只护提升不护弱化, 窗口过期解除; 含封顶+6与视图同步 | 落芳天华85 / 烬灭神咒剑1287 / 神煌炎舞斩1960 |
| 034 | `scenario_034_stat_drop_immunity` | **弱化原语** `stat_drop` + 免弱 `STAT_DROP`:先查免弱(无条件, 有 +N 也不降)、可穿强化保护 STAT_CLEAR、钳 -6、视图同步; 含 effect5 端到端与自身增益视图回归 | 奉献之火31269(effect5) / 高速移动20017(effect4) |
| 035 | `scenario_035_boost_reversal_immunity` | **反转对手能力提升**(`stat_boost_reversal`): 弱化类动作 → **先查免弱**(BLOCKED)、可穿强化保护、+2→-2 等量反转; 含 effect143 端到端有/无免弱对照 | 雨燕之影17587 / 903之佑免弱 |
| 036 | `scenario_036_puni_seven_seals` | **圣灵谱尼七刻印(六条)**:圣洁免异常+免弱 / 元素登场断回合→**成功才**给龙威 / 虚无受击后免下1次技能伤害 / 能量已损失体力50%百分比伤害(`PERCENT_VALUE`) / 生命回1/4+随机2PP / 永恒体力高则必先 | 谱尼 1038 |
| 037 | `scenario_037_pet_storage_scopes` | **pet 两私有槽生命周期**(下场保留 vs 本次上场) + 落芳天华497: 固伤递增/封顶400/**下场重置**/**天敌翻倍** | 落芳天华17912 / restraint_multiplier |
| 038 | `scenario_038_round_window_first_second` | **回合数窗口 先/后出手**:N回合内(先R..R+N-1/后R+1..R+N, N=1不顺延) / 下N回合(都R+1..R+N); 家族走认证数据层声明; 修 843 少一回合 | effect 843 / window 声明 |
| 039 | `scenario_039_puni_signature_skills` | **圣灵谱尼 8 专属技能**(14 组): 525 免伤窗口 / 776·693 增伤写`ws.damage_add_pct`(AMP 阶段读) / 1238 全属性翻倍 / 1237 双方净化 / 1005 等量百分比 / 700·1222 PP压制 / 906 递增百分比 / 694必先·499后手先制 / 610 天敌先制 / 936 概率异常·未触发封属 / 1236 受击惩罚(DETECT) / 1217 消强封攻击 / 57·1239 回血 / 695 概率封属 | 谱尼专属 7 技能 |
| 040 | `scenario_040_critical_hit` | **暴击**: 暴击率接入(crit_rate=分子/16, **0=0/16永不必暴**) / 判定位=miss后·门判定前(只有miss阻止暴击) / **暴击破防**(物理→防御, 特殊→特防, 只清正等级, 不查免疫, 打盔照样破) / 属性技能不掷 / 先结算伤害再重置 | `crit_defense_break` |
| 042 | `scenario_042_armor_pierce_and_force_execute` | **盔穿透语义**: 被穿消不消耗看模板有无子句(`consumed_when_pierced`); **强制执行**盔照常消耗但不拦截+视图威力置0; `EVENT_SKILL_ARMOR_TRIGGERED` 只对真正生效的盔发→被穿子句天然不触发 | 2270/570/2006/2269 |
| 041 | `scenario_041_must_hit_blind_hit_invalid` | **必中凭证+失明+③层命中失效分两类**: 条件必中(effect 2000)写 `ws.must_hit_grant`→`AttackCredential::must_hit`(不再永久改技能对象) / 失明: 非必中必miss、必中50%效果失效、强制执行最先短路 / `HIT_INVALID` 拆 ATTACK/ATTRIBUTE 两类分别消费 | 失明(异常20) |

## 能力域归并(找"同一套机制"用这个)

- **先手权**: 001
- **技能无效(盔/威/封属)**: 002、011、023
- **技能无效·命中失效分支(封属非补偿语义)**: 029
- **伤害管线·挡伤/免伤(BLOCK 类别·挡伤失效掩码)**: 031
- **穿透/强制/命中凭证**: 004、005
- **吸取/减伤/恢复/封回血**: 003、017
- **能力等级变更(改/弱化/消除/反转/转换吸取 + 免弱/免消除强化)**: 028、030、033、034、035
- **魂印多子句程序(七刻印)**: 007、019、032、036
- **pet 私有槽/次数累计/天敌**: 008、037
- **回合数窗口(先/后出手)**: 038
- **PP 操作/封属概率/增伤窗口/受击惩罚**: 039
- **暴击/暴击破防**: 040
- **必中/失明/③层命中失效分类型**: 041
- **盔穿透/强制执行/盔生效事件**: 042
- **魂印程序模型**: 006、007、008、009、018、019
- **组合语法/条件取消**: 009、010
- **伤害公式/克制/系别/抗性**: 012、013、014、016
- **异常体系**: 015
- **技能替换(同一套机制)**: **020(kExecOnly) + 022(kFull)**
- **场域/事件监控**: 021、023
- **不死/锁血(EventCenter watcher)**: 024、025
- **状态快照/回档(pet soulmark_storage)**: 032
- **药品系统(嗑药7种)**: 025

> ⚠️ 020 与 022 刻意**分成两个文件**:虽是同一 "技能替换" 机制,但**载体、时点、语义**都不同
> (020: `ws.skill_effect_source`,执行期,只换效果、**固有效果保留**;022: `ctx->pending_skill_replacement`,
> 前置,换整个技能、**固有效果(先制)消失**)。各自单测更好定位回归;归并见速度用本表家族分组即可。
> 各有入口 `main()` 独立运行,互不干扰。