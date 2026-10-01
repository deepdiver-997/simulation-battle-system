-- X2 异常码错参修复：effect 1248 的 custom_effect_programs 程序行
-- 证据：模板「对手处于异常状态时{0}%令对手{1}」{0}=概率 {1}=异常码；原行 param0={"arg":0}
-- 把概率（无相谛=100）填进异常码槽且概率未接线，22 载体全中。修 = param0 取 {1}+chance_value
-- 接 {0}。⚠️ 概率必须用 chance_value={"arg":0}（装载期 record.args 下标），不要用
-- chance_arg（运行时 int_args 前缀下标，[0]=owner 恒 0 → 单元静默 Never）。
-- 回滚：UPDATE custom_effect_programs SET unit_json='{"tag":"Anomaly","target":1,"param0":{"arg":0},"param1":2,"condition":"TargetHasAnomaly"}' WHERE effect_id=1248;
UPDATE custom_effect_programs
SET unit_json = '{"tag":"Anomaly","target":1,"chance_value":{"arg":0},"param0":{"arg":1},"param1":2,"condition":"TargetHasAnomaly"}',
    memo = '无相谛1248迁移(带TargetHasAnomaly前置; 2026-10-01 X2修正: param0改arg:1异常码+chance_value:{arg:0}概率接线——原行把{0}概率当异常码，22载体全中)'
WHERE effect_id = 1248 AND skill_id = -1;
SELECT '[verify] effect 1248 unit_json=' || unit_json FROM custom_effect_programs WHERE effect_id = 1248;
