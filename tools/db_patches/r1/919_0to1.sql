-- R1 切片双表修复：effect 919 side_effect.arg_count 0→1
-- 证据：模板「后出手时令对手下{0}回合属性技能无效」1个{n}；4399 洛比特/赫尔卡「N回合内…属性技能无效」；全载体切片[1]，27357/30567/28416 跨效果完美对齐。引擎 effect_919 需 1 业务参，se=0 时恒 no-op。
-- 回滚：UPDATE side_effect SET arg_count=0 WHERE id=919;
UPDATE side_effect SET arg_count = 1 WHERE id = 919 AND arg_count = 0;
SELECT '[verify] effect 919 arg_count=' || arg_count FROM side_effect WHERE id = 919;
