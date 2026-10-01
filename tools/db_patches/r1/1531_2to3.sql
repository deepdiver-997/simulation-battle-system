-- R1 切片双表修复：effect 1531 side_effect.arg_count 2→3
-- 证据：模板「{0}回合内每回合使用技能附加{1}{2}」3个{n}；4399 卡尔玛「3回合内每回合…附加300点固定伤害」；载体 [3,300,0] 模式（27020/27497/27549/27752/28360/29188 六例跨效果完美）。引擎 effect_1531 需 3 业务参。
-- 回滚：UPDATE side_effect SET arg_count=2 WHERE id=1531;
UPDATE side_effect SET arg_count = 3 WHERE id = 1531 AND arg_count = 2;
SELECT '[verify] effect 1531 arg_count=' || arg_count FROM side_effect WHERE id = 1531;
