-- R1 切片双表修复：effect 2190 side_effect.arg_count 0→1
-- 证据：模板字面「{0}」1个{n}；单效果载体 [0]/[2]，28780/28781/29133/29245 组合载体跨效果完美对齐。修复后 4 个无为觉者引用技的数据侧解锁。
-- 回滚：UPDATE side_effect SET arg_count=0 WHERE id=2190;
UPDATE side_effect SET arg_count = 1 WHERE id = 2190 AND arg_count = 0;
SELECT '[verify] effect 2190 arg_count=' || arg_count FROM side_effect WHERE id = 2190;
