-- R1 切片双表修复：effect 596 side_effect.arg_count 5→1
-- 证据：模板「{0}%…冻伤、中毒、烧伤中任意一种」1个{n}；4399 角突「100%给予…一种」1数字、37228 切片[100]；16168 五参[100,38,40,20,40]为异例旧数据（4399 万象星魔斩无数字），按模板+干净载体定 1。旧值 5 会在未来多效果载体上劫持下游参数。
-- 回滚：UPDATE side_effect SET arg_count=5 WHERE id=596;
UPDATE side_effect SET arg_count = 1 WHERE id = 596 AND arg_count = 5;
SELECT '[verify] effect 596 arg_count=' || arg_count FROM side_effect WHERE id = 596;
