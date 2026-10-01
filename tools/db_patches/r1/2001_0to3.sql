-- R1 切片双表修复：effect 2001 side_effect.arg_count 0→3
-- 证据：模板「…{0}%令对手{1}{2}回合」3个{n}；载体 36622=[100,31,3]、38552=[100,15,3]（=100%令对手<异常码>N回合），2002/2003 下游随之对齐。
-- 回滚：UPDATE side_effect SET arg_count=0 WHERE id=2001;
UPDATE side_effect SET arg_count = 3 WHERE id = 2001 AND arg_count = 0;
SELECT '[verify] effect 2001 arg_count=' || arg_count FROM side_effect WHERE id = 2001;
