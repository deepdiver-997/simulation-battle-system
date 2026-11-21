-- z9_custom_tables.sql —— 固化 custom_* 引擎自有表内容为管线资产（2026-11-22）
--
-- 根因（2026-11-21/22 CI 实证链）：custom_* 五张表是 CREATE TABLE IF NOT EXISTS 的
-- 「引擎自有表」，内容只存在于本地 DB 的手工演化史——apply_all 旧注释称"重建后保留，
-- 无需重放"，但 CI/新环境**从零建库时表全空**：
--   CI ubuntu 7 场景红（此前误归"平台差异"）全部/大部分由此起：
--     009/010  custom_effect_programs 缺 179/456/700/1083/1248/1257（无相谛六类条件
--              模板的离线程序）→ 插件接管 → 本系判据 velem vs skill.elem 在场景构造下
--              恒 FALSE → powView 改写不发生（沙盒决定性实验：Mac 删行复现 CI 红、
--              重放恢复绿）
--     038      custom_effect_overrides 缺 843 window=next_rounds（用户 2026-09-13 定）
--              → meta->window 断言红
--     874      custom_equip_stats 缺 → suit 面板合成基值漂移（330 vs 363）
--   决定性证据：docs_local/docs/07-工具与测试/Windows-移植.md §9.2/§9.9 与本次沙盒实验
--
-- 内容 = 2026-11-22 本地 DB（13a802c7 系）五张表全量 dump，语义零变更（纯资产化）。
-- [verify] 每表行数应为：programs 6 / overrides 17 / equip_stats 16 / combo_rules 16 /
--          selection_routes 38（见文件尾注释的核对查询）
--
-- 回滚：DELETE FROM 各表（恢复 CI 旧空表态，相应场景回红）。

-- ── custom_effect_programs ──
DELETE FROM custom_effect_programs;
INSERT INTO custom_effect_programs VALUES(1,179,37460,'template','属性相同→威力提升','{"tag":"PowerBoost","target":1,"param0":{"arg":0},"condition":"SameElement"}','无相谛179迁移(通用模板, 替代运行时文本解析)——2026-11-02 J315 重锚: 仅 37460 保留单元形态(万相乖离 2260 六条件取消链依赖 parsed_units_ 六条目; 单元词汇无技能属性==使用者属性判据), 其余 17 载体走 moves_lib/j_cond_power_rewrite.cpp 插件新判据(技能属性==使用者属性, DB 实证 18/18 学习者属性=技能系别)');
INSERT INTO custom_effect_programs VALUES(2,700,-1,'template','先出手→降对手全PP','{"tag":"PpReduce","target":1,"param0":{"arg":0},"condition":"FirstMove"}','无相谛700迁移');
INSERT INTO custom_effect_programs VALUES(3,1083,-1,'template','后出手→消除对手回合类','{"tag":"RemoveRoundEffects","target":1,"condition":"SecondMove"}','无相谛1083迁移');
INSERT INTO custom_effect_programs VALUES(4,1257,-1,'template','对手无异常→吸取1/n体力','{"tag":"DrainHp","target":1,"param0":{"arg":0},"condition":"TargetNoAnomaly"}','无相谛1257迁移');
INSERT INTO custom_effect_programs VALUES(5,1248,-1,'template','对手异常→令其异常','{"tag":"Anomaly","target":1,"chance_value":{"arg":0},"param0":{"arg":1},"param1":2,"condition":"TargetHasAnomaly"}','无相谛1248迁移(带TargetHasAnomaly前置; 2026-10-01 X2修正: param0改arg:1异常码+chance_value:{arg:0}概率接线——原行把{0}概率当异常码，22载体全中)');
INSERT INTO custom_effect_programs VALUES(6,456,-1,'template','对手体力不足→秒杀','{"tag":"Kill","target":1,"condition":"TargetHpBelow","condition_param":{"arg":0}}','无相谛456迁移');

-- ── custom_effect_overrides ──
DELETE FROM custom_effect_overrides;
INSERT INTO custom_effect_overrides VALUES(1,58,'window','next_rounds','effect_info','原文「下{0}回合」= 本回合不算，先/后出手都从下一回合起算（用户 2026-09-13 定）');
INSERT INTO custom_effect_overrides VALUES(2,81,'window','next_rounds','effect_info','同上：「下{0}回合自身攻击技能必定命中」');
INSERT INTO custom_effect_overrides VALUES(3,83,'window','next_rounds','effect_info','同上：「下两回合必定先手/致命一击」');
INSERT INTO custom_effect_overrides VALUES(4,843,'window','next_rounds','effect_info','原文「下{0}回合令自身所有技能先制+{1}」= 本回合不算，窗口应为 R+1..R+N');
INSERT INTO custom_effect_overrides VALUES(5,10,'anomaly_channel','ancient','effect_info','古早"命中后{0}%令对方麻痹"族=主动毒：只被 Ancient 层免疫挡，无视弹控/抗性/转化（瞬雷天闪 10176；语料主动毒文章+用户实测）');
INSERT INTO custom_effect_overrides VALUES(6,11,'anomaly_channel','ancient','effect_info','古早"命中后{0}%令对方中毒"族，同 effect 10');
INSERT INTO custom_effect_overrides VALUES(7,12,'anomaly_channel','ancient','effect_info','古早"命中后{0}%令对方烧伤"族，同 effect 10');
INSERT INTO custom_effect_overrides VALUES(8,14,'anomaly_channel','ancient','effect_info','古早"命中后{0}%令对方冻伤"族，同 effect 10');
INSERT INTO custom_effect_overrides VALUES(9,15,'anomaly_channel','ancient','effect_info','古早"命中后{0}%令对方害怕"族，同 effect 10');
INSERT INTO custom_effect_overrides VALUES(10,114,'anomaly_channel','ancient','effect_info','古早"命中后有{0}%概率令对方易燃"，同 effect 10 族');
INSERT INTO custom_effect_overrides VALUES(11,2189,'anomaly_channel','modern','effect_info','现代参数化族"{0}%令对手{1}"（{1}=battle_effects 异常码；月下伏 37526=100%焚烬）：全检查链（次免/弹控/抗性/魂免/转化都响应）');
INSERT INTO custom_effect_overrides VALUES(12,48,'anomaly_immunity_tier','ancient','effect_info','"{N}回合内免疫所有受到的异常状态"=老技能免控（官方"带补丁"）：两种施加都挡；纯免疫不反弹（意志模组·怒 29025）');
INSERT INTO custom_effect_overrides VALUES(13,191,'anomaly_immunity_tier','modern','effect_info','"免疫并反弹"弹控主流件（1131 技能）：弹控不能免疫主动毒（含魂印弹控）→ 只响应现代施加');
INSERT INTO custom_effect_overrides VALUES(14,1687,'anomaly_immunity_tier','modern','effect_info','"免疫并反弹控制类"弹控变体，同 191');
INSERT INTO custom_effect_overrides VALUES(15,928,'anomaly_immunity_tier','modern','effect_info','官方次数型"免下N次"（19560+ 年代）：暂按 Modern（无挡主动毒的实测证据；用户 2026-09-16 拍板"先当作没有"，拿到实测翻回）');
INSERT INTO custom_effect_overrides VALUES(16,1159,'anomaly_immunity_tier','modern','effect_info','次数型"吸取成功免下N次"（月下伏自带），同 928 暂按 Modern');
INSERT INTO custom_effect_overrides VALUES(17,1440,'anomaly_immunity_tier','modern','effect_info','次数型"免疫下N次受到的异常状态"，同 928 暂按 Modern');

-- ── custom_equip_stats ──
DELETE FROM custom_equip_stats;
INSERT INTO custom_equip_stats VALUES(1301035,4,60,0,'per_suit','晨曦之星：背包内所有精灵速度+60',0,1,1);
INSERT INTO custom_equip_stats VALUES(1301035,5,60,0,'per_suit','晨曦之星：背包内所有精灵体力+60',0,1,1);
INSERT INTO custom_equip_stats VALUES(1300768,0,10,1,'per_suit','腐蚀者：背包内精灵攻击+10%',0,1,1);
INSERT INTO custom_equip_stats VALUES(1300768,1,10,1,'per_suit','腐蚀者：背包内精灵特攻+10%',0,1,1);
INSERT INTO custom_equip_stats VALUES(1300887,5,40,0,'per_suit','六界战甲：六界御神体力+40',4032,1,1);
INSERT INTO custom_equip_stats VALUES(1300887,0,20,0,'per_suit','六界战甲：六界御神攻击+20',4032,1,1);
INSERT INTO custom_equip_stats VALUES(1300887,2,20,0,'per_suit','六界战甲：六界御神防御+20',4032,1,1);
INSERT INTO custom_equip_stats VALUES(1300887,1,20,0,'per_suit','六界战甲：六界御神特攻+20',4032,1,1);
INSERT INTO custom_equip_stats VALUES(1300887,3,20,0,'per_suit','六界战甲：六界御神特防+20',4032,1,1);
INSERT INTO custom_equip_stats VALUES(1300887,4,20,0,'per_suit','六界战甲：六界御神速度+20',4032,1,1);
INSERT INTO custom_equip_stats VALUES(1301154,5,100,0,'per_suit','圣芒佑界：己方精灵体力+100',0,1,1);
INSERT INTO custom_equip_stats VALUES(1301154,0,80,0,'per_suit','圣芒佑界：己方精灵攻击+80',0,1,1);
INSERT INTO custom_equip_stats VALUES(1301154,1,80,0,'per_suit','圣芒佑界：己方精灵特攻+80',0,1,1);
INSERT INTO custom_equip_stats VALUES(1301154,2,80,0,'per_suit','圣芒佑界：己方精灵防御+80',0,1,1);
INSERT INTO custom_equip_stats VALUES(1301154,3,80,0,'per_suit','圣芒佑界：己方精灵特防+80',0,1,1);
INSERT INTO custom_equip_stats VALUES(1301154,4,80,0,'per_suit','圣芒佑界：己方精灵速度+80',0,1,1);

-- ── custom_skill_combo_rules ──
DELETE FROM custom_skill_combo_rules;
INSERT INTO custom_skill_combo_rules VALUES(31,0,1,1,'经典连击族 {0}~{1}：乱突/疯狂乱抓/花瓣舞，303 载体 2026-09-17 逐个核对（原 combo_arg_indices case 31）');
INSERT INTO custom_skill_combo_rules VALUES(1141,0,1,1,'1回合做{0}~{1}次攻击（原 case 1141）');
INSERT INTO custom_skill_combo_rules VALUES(1172,1,2,1,'{0}回合做{1}~{2}次攻击（原 case 1172）');
INSERT INTO custom_skill_combo_rules VALUES(1454,0,1,1,'1回合做{0}-{1}次攻击＋{2}%附加{3}点固定伤害（原 case 1454）');
INSERT INTO custom_skill_combo_rules VALUES(1455,0,1,1,'1回合做{0}-{1}次攻击，达上限秒杀（原 case 1455）');
INSERT INTO custom_skill_combo_rules VALUES(1500,0,1,1,'1回合做{0}-{1}次攻击＋护盾状态连击上限句（上限修正未实现，区间照取；原 case 1500）');
INSERT INTO custom_skill_combo_rules VALUES(1546,0,1,1,'1回合做{0}-{1}次攻击＋蓄力上限修正句（同上；原 case 1546）');
INSERT INTO custom_skill_combo_rules VALUES(1577,1,2,1,'{0}回合做{1}-{2}次攻击，当前技能PP值小于{3}时连击上限为{4}（上限修正句未实现，区间照取；原 case 1577）');
INSERT INTO custom_skill_combo_rules VALUES(1593,0,1,1,'1回合做{0}-{1}次攻击＋体力上限修正句（同上；原 case 1593）');
INSERT INTO custom_skill_combo_rules VALUES(1608,0,1,1,'布布鸟 5-10 次攻击：文案常量→显式行参（载体 34454 [100,1,5,10]；原 combo_fixed_range 常量旁路）');
INSERT INTO custom_skill_combo_rules VALUES(1609,0,1,1,'布布犬 5-10 次攻击：文案常量→显式行参（载体 34457；原 combo_fixed_range 常量旁路）');
INSERT INTO custom_skill_combo_rules VALUES(1610,0,1,1,'布布熊 5-10 次攻击：文案常量→显式行参（载体 34460；原 combo_fixed_range 常量旁路）');
INSERT INTO custom_skill_combo_rules VALUES(1627,1,2,1,'{0}回合做{1}-{2}次攻击，达上限必定秒杀（秒杀子句未实现，区间照取；原 case 1627）');
INSERT INTO custom_skill_combo_rules VALUES(1666,0,1,1,'1回合做{0}-{1}次攻击＋领域上限修正句（同上；原 case 1666）');
INSERT INTO custom_skill_combo_rules VALUES(1685,0,1,1,'1回合做{0}-{1}次攻击＋异常上限修正句（同上；原 case 1685）');
INSERT INTO custom_skill_combo_rules VALUES(1732,0,1,1,'激发古渊灵鱼全部的力量，1回合做{0}-{1}次攻击（原 case 1732）');

-- ── custom_skill_selection_routes ──
DELETE FROM custom_skill_selection_routes;
INSERT INTO custom_skill_selection_routes VALUES(95,'selection','FIRST_MOVE_RIGHT','plain',1,'睡眠门→暴击率加量 {0}/16，掷骰前直写 crit_rate_add（J254，1参；插件 j_sleep_crit_gate.cpp）');
INSERT INTO custom_skill_selection_routes VALUES(454,'selection','FIRST_MOVE_RIGHT','plain',1,'体力阈值 1/{0}→先制+{1}（J53 低体门，2参；插件 j_cond_priority.cpp）');
INSERT INTO custom_skill_selection_routes VALUES(539,'selection','FIRST_MOVE_RIGHT','plain',1,'先制即时＋威力后半句自延迟（J103：先制半在选择期判，威力半由 body 自挂当回合 SKILL_EFFECT；插件 j_foeboost_prio_power.cpp）');
INSERT INTO custom_skill_selection_routes VALUES(610,'selection','FIRST_MOVE_RIGHT','plain',1,'遇天敌→先制+{0}（璨灵圣光族，1参；插件 selection_seal.cpp）');
INSERT INTO custom_skill_selection_routes VALUES(616,'selection','FIRST_MOVE_RIGHT','plain',1,'当自身血量少于1/3时先制+2（effect_info 原文；6 载体 17660 破锐怒冲/19323 龙怒天火断/23413 海之契约/23836 轮回天生/24822 心怀苍生/38016 异怪波动，切片恒[3,2]逐位自洽；门=hp×3<max_hp 严格低于（454/1397 整数直乘口径，恰1/3门闭），出=ws.preemptive_level+=2 优先级修改面；J53 454 同形（j_cond_priority.cpp:36），a31-orch 2026-11-17 裁决扩行授权；soul 空间 616=K303 已实现（双空间同号甄别，K303 不受影响）；插件 k469_lowhp_prio.cpp）');
INSERT INTO custom_skill_selection_routes VALUES(747,'selection','FIRST_MOVE_RIGHT','plain',1,'自身下降→先制+{0}（J74，1参；插件 j_negslot_prio.cpp）');
INSERT INTO custom_skill_selection_routes VALUES(750,'selection','FIRST_MOVE_RIGHT','plain',1,'预武装 ACTION_START 穿伤害限制（J114 两级注册，MOVE_RIGHT 只装 tick；插件 j_first_move_gates2.cpp）');
INSERT INTO custom_skill_selection_routes VALUES(779,'selection','FIRST_MOVE_RIGHT','plain',1,'对手提升→先制+2（盖亚武道 J11；插件 j_gaia_stance.cpp）');
INSERT INTO custom_skill_selection_routes VALUES(785,'selection','FIRST_MOVE_RIGHT','plain',1,'判精灵系别视图微弱→先制+2（用户 2026-09-14 口径：读 ws.view_elementalAttributes；插件 selection_seal.cpp）');
INSERT INTO custom_skill_selection_routes VALUES(881,'selection','FIRST_MOVE_RIGHT','plain',1,'自身下降→先制+3（神火剑阵 37221，2026-09-20 烧伤三精灵线；插件 burn_trio_skills.cpp/artemis_skills.cpp 双注册为既有撞号审计项，非本单改动）');
INSERT INTO custom_skill_selection_routes VALUES(954,'selection','FIRST_MOVE_RIGHT','plain',1,'当前体力低于对手→先制+1（协议I·激 37354 等 36 技通用模板；插件 crawford_skills.cpp）');
INSERT INTO custom_skill_selection_routes VALUES(958,'selection','FIRST_MOVE_RIGHT','plain',1,'指定异常门→先制+{1}（J115，2参 has_active_anomaly(对手,码)；插件 j_cond_prio2.cpp）');
INSERT INTO custom_skill_selection_routes VALUES(1081,'selection','FIRST_MOVE_RIGHT','plain',1,'对手提升→先制+1（J115 779 孪生；插件 j_cond_prio2.cpp）');
INSERT INTO custom_skill_selection_routes VALUES(1174,'selection','FIRST_MOVE_RIGHT','plain',1,'自身提升→先制+2（J115 2298 孪生；插件 j_cond_prio2.cpp）');
INSERT INTO custom_skill_selection_routes VALUES(1243,'selection','FIRST_MOVE_RIGHT','plain',1,'对手下降→先制+1（J91，747 孪生；插件 j_foenslot_prio.cpp）');
INSERT INTO custom_skill_selection_routes VALUES(1292,'selection','FIRST_MOVE_RIGHT','plain',1,'自身体力低于对手→先制+{0}（J312 954 同款行参，1参；插件 j_cond_prio3.cpp）');
INSERT INTO custom_skill_selection_routes VALUES(1465,'selection','FIRST_MOVE_RIGHT','plain',1,'先手概率暴/失败降属性（J136 FIRST_ACTION_START，1参；插件 j_crit_gates.cpp）');
INSERT INTO custom_skill_selection_routes VALUES(1484,'selection','FIRST_MOVE_RIGHT','plain',1,'半血门→先制（J53；插件 j_cond_priority.cpp）');
INSERT INTO custom_skill_selection_routes VALUES(1487,'selection','FIRST_MOVE_RIGHT','plain',1,'护盾门→先制（J53；插件 j_cond_priority.cpp）');
INSERT INTO custom_skill_selection_routes VALUES(1491,'selection','FIRST_MOVE_RIGHT','plain',1,'遇天敌→先制+1（J66 首回合勘误代表：effect_register_state 路由赶不上首回合；插件 j_reversal_prio.cpp）');
INSERT INTO custom_skill_selection_routes VALUES(1508,'selection','FIRST_MOVE_RIGHT','plain',1,'预武装 ACTION_START 穿攻击免疫（J114；插件 j_first_move_gates2.cpp）');
INSERT INTO custom_skill_selection_routes VALUES(1527,'selection','FIRST_MOVE_RIGHT','plain',1,'后手必暴的 ACTION_START 路由（J136 SECOND_ACTION_START；插件 j_crit_gates.cpp）');
INSERT INTO custom_skill_selection_routes VALUES(1532,'selection','FIRST_MOVE_RIGHT','plain',1,'必暴（无神印）／神印 PP 后半句，非普通先制（神游·浩渺星河 2026-09-22 启灵线；插件 qiling_skills.cpp）');
INSERT INTO custom_skill_selection_routes VALUES(1774,'selection','FIRST_MOVE_RIGHT','plain',1,'若对手当回合使用的技能为攻击技能则自身额外先制+1（effect_info 原文；8 载体 28051 启明之辰/28366 饭后甜点/28668 安魂引/35652 几曾识干戈/35786 兽灵震谷爪/35907 御风无痕/36277 雾停枝/36411 灰刻截光破，零参切片逐位自洽；门=对手选攻击技（roundChoice==SELECT_SKILL 且 type!=Attribute，2273 同形判据），K425 头注指名续单；插件 j335_cond_prio_vs_attack.cpp）');
INSERT INTO custom_skill_selection_routes VALUES(1848,'selection','FIRST_MOVE_RIGHT','plain',1,'对手下降→先制+2（王·御影神罚族/神觉·米斯蒂克；插件 mistike_skills.cpp）');
INSERT INTO custom_skill_selection_routes VALUES(1860,'selection','FIRST_MOVE_RIGHT','plain',1,'自身无提升→先制+1且必中（天奔零式 J29；插件 j_boost_protect.cpp）');
INSERT INTO custom_skill_selection_routes VALUES(1940,'selection','FIRST_MOVE_RIGHT','plain',1,'自身持有回能标记时先制+2（effect_info 原文；载体 守一 36503[1940,1941,1922,1776] 4489 专属；条件读回能Ⅰ槽 4696000+side=K418 段契约；插件 j331_marker_cond_prio.cpp）');
INSERT INTO custom_skill_selection_routes VALUES(1941,'selection','FIRST_MOVE_RIGHT','plain',1,'对手持有失能标记时先制+2（effect_info 原文；载体 守一 36503（4489 专属）；条件读失能Ⅰ槽 4696010+side=K418 段契约；插件 j331_marker_cond_prio.cpp）');
INSERT INTO custom_skill_selection_routes VALUES(1956,'selection','FIRST_MOVE_RIGHT','plain',1,'对手无异常→先制+1（J312 扫异常全集 0..43；插件 j_cond_prio3.cpp）');
INSERT INTO custom_skill_selection_routes VALUES(2000,'selection','FIRST_MOVE_RIGHT','plain',1,'对手提升→先制+1且必中（大雪纷飞/烬灭神咒剑族；原内核 plain 表首位；插件 selection_seal.cpp）');
INSERT INTO custom_skill_selection_routes VALUES(2030,'selection','FIRST_MOVE_RIGHT','skill_static_elements',1,'判技能静态系别微弱→先制+1（args[2]/[3]=element[0]/[1] 防视图自污染；零参效果；插件 selection_seal.cpp）');
INSERT INTO custom_skill_selection_routes VALUES(2133,'selection','FIRST_MOVE_RIGHT','plain',1,'自身持有回体时先制+1，否则为自身附加3回合的回体并令对手疲惫（effect_info 原文全文=含否则支，K417 头注原引前半句已由本单勘误；载体 缘结·浮世诵 37021[699,1568,2132,2133,837] 4499 sp_extra_moves；回体Ⅰ槽 4695000+side=K417 段契约；插件 j331_marker_cond_prio.cpp）');
INSERT INTO custom_skill_selection_routes VALUES(2298,'selection','FIRST_MOVE_RIGHT','plain',1,'自身无提升→先制+2（盖亚武道 J11；官方无 side_effect 定义行、effect_info.args_num 兜底；插件 j_gaia_stance.cpp）');
INSERT INTO custom_skill_selection_routes VALUES(2345,'selection','FIRST_MOVE_RIGHT','plain',1,'英雄之耀达到3层时此技能不受PP值限制且额外先制+1（effect_info 原文；载体 鋭武铭戈 38086[2345,2195,2342,1208] 零参切片 4800 专属；条件读英雄之耀槽 kWujiYaoltKey=243601=魂印契约；PP 半句由 wuji_shengwu_4800.cpp ROUND_START 同步 per-skill IgnorePP（2260 ②先例）；插件 j332_cond_prio_expansion.cpp）');
INSERT INTO custom_skill_selection_routes VALUES(2346,'selection','FIRST_MOVE_RIGHT','plain',1,'英雄之耀达到3层时此技能不受PP值限制且使用时解除自身所处的异常状态（effect_info 原文；载体 乱武天傀 29149[2346,945,972,2348,2274] 零参切片 4800 专属；解除半句按 J331 2133 先例在节点体执行（先手比较前，与「使用时」的时点偏差已记档）；PP 半句同 2345 口径；插件 j332_cond_prio_expansion.cpp）');
INSERT INTO custom_skill_selection_routes VALUES(2347,'selection','FIRST_MOVE_RIGHT','plain',1,'英雄之耀达到3层时此技能不受PP值限制且使用时消除自身回合类效果（effect_info 原文；载体 烈武天征 29150[2347,1014,971,191,843] 零参切片 4800 专属；消除半句=break_round_effects 自身侧，J331 2133 先例同上；PP 半句同 2345 口径；插件 j332_cond_prio_expansion.cpp）');
INSERT INTO custom_skill_selection_routes VALUES(2436,'selection','FIRST_MOVE_RIGHT','plain',1,'自身上回合每有1级先制，先制+1，最高+3（effect_info 原文；载体 神诗终幕 38328[2436,9,429] 零参切片 4888 专属；条件=上回合先制级数快照 102436000x+side 段（宠 id 消歧），K420 头注指名续单；插件 j332_cond_prio_expansion.cpp）');
INSERT INTO custom_skill_selection_routes VALUES(2490,'selection','FIRST_MOVE_RIGHT','skill_static_elements',1,'微弱→先制+3/本系/神灵系奖励（args[2]/[3]=element[0]/[1]；官方无 side_effect 定义行、零参；插件 selection_seal.cpp）');

-- [verify] 应用后核对（应 6/17/16/16/38）：
--   SELECT 'programs',COUNT(*) FROM custom_effect_programs
--   UNION ALL SELECT 'overrides',COUNT(*) FROM custom_effect_overrides
--   UNION ALL SELECT 'equip',COUNT(*) FROM custom_equip_stats
--   UNION ALL SELECT 'combo',COUNT(*) FROM custom_skill_combo_rules
--   UNION ALL SELECT 'routes',COUNT(*) FROM custom_skill_selection_routes;
