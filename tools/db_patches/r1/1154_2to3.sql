-- R1 切片双表修复：effect 1154 side_effect.arg_count 2→3
-- 证据：模板「{0}回合内使用技能附加{1}{2}」3个{n}；4399 索比拉特「4回合内使用技能附加200点固定伤害」；载体 [3,200,0] 模式（25425/25764/25877/25969/26194/26344 六例跨效果完美）。旧值 2 吃掉 kind 码并错位下游。
-- 回滚：UPDATE side_effect SET arg_count=2 WHERE id=1154;
UPDATE side_effect SET arg_count = 3 WHERE id = 1154 AND arg_count = 2;
SELECT '[verify] effect 1154 arg_count=' || arg_count FROM side_effect WHERE id = 1154;
