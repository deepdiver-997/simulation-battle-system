-- R1 切片双表修复：effect 1654 side_effect.arg_count 0→1
-- 证据：模板 args_num=1（info 文案「首次使用的技能」不参数化故无{n}）；4399 冥府吞噬文案无数字；36338 单载体切片[1]，34812/37353 组合载体尾位完美对齐。
-- 回滚：UPDATE side_effect SET arg_count=0 WHERE id=1654;
UPDATE side_effect SET arg_count = 1 WHERE id = 1654 AND arg_count = 0;
SELECT '[verify] effect 1654 arg_count=' || arg_count FROM side_effect WHERE id = 1654;
