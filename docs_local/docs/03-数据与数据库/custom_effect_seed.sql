PRAGMA foreign_keys=OFF;
BEGIN TRANSACTION;
CREATE TABLE custom_effect_programs (
    id INTEGER PRIMARY KEY, effect_id INTEGER, skill_id INTEGER,
    kind TEXT NOT NULL DEFAULT 'program', name_zh TEXT, unit_json TEXT NOT NULL, memo TEXT,
    UNIQUE(effect_id, skill_id, kind));
INSERT INTO custom_effect_programs VALUES(1,179,37460,'template','属性相同→威力提升','{"tag":"PowerBoost","target":1,"param0":{"arg":0},"condition":"SameElement"}','无相谛179迁移(通用模板, 替代运行时文本解析)——2026-11-02 J315 重锚: 仅 37460 保留单元形态(万相乖离 2260 六条件取消链依赖 parsed_units_ 六条目; 单元词汇无技能属性==使用者属性判据), 其余 17 载体走 moves_lib/j_cond_power_rewrite.cpp 插件新判据(技能属性==使用者属性, DB 实证 18/18 学习者属性=技能系别)');
INSERT INTO custom_effect_programs VALUES(2,700,-1,'template','先出手→降对手全PP','{"tag":"PpReduce","target":1,"param0":{"arg":0},"condition":"FirstMove"}','无相谛700迁移');
INSERT INTO custom_effect_programs VALUES(3,1083,-1,'template','后出手→消除对手回合类','{"tag":"RemoveRoundEffects","target":1,"condition":"SecondMove"}','无相谛1083迁移');
INSERT INTO custom_effect_programs VALUES(4,1257,-1,'template','对手无异常→吸取1/n体力','{"tag":"DrainHp","target":1,"param0":{"arg":0},"condition":"TargetNoAnomaly"}','无相谛1257迁移');
INSERT INTO custom_effect_programs VALUES(5,1248,-1,'template','对手异常→令其异常','{"tag":"Anomaly","target":1,"chance_value":{"arg":0},"param0":{"arg":1},"param1":2,"condition":"TargetHasAnomaly"}','无相谛1248迁移(带TargetHasAnomaly前置; 2026-10-01 X2修正: param0改arg:1异常码+chance_value:{arg:0}概率接线——原行把{0}概率当异常码(22载体全中)，首修chance_arg:0又踩运行时前缀下标坑(int_args[0]=owner)改字面接线');
INSERT INTO custom_effect_programs VALUES(6,456,-1,'template','对手体力不足→秒杀','{"tag":"Kill","target":1,"condition":"TargetHpBelow","condition_param":{"arg":0}}','无相谛456迁移');
CREATE TABLE custom_effect_overrides (
    id INTEGER PRIMARY KEY, effect_id INTEGER NOT NULL, override_type TEXT NOT NULL,
    map_value TEXT, source_scope TEXT, rationale TEXT,
    UNIQUE(effect_id, override_type));
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
COMMIT;
