-- R1 切片双表修复：effect 118 side_effect.arg_count 0→2
-- 证据：模板「随机改为{0}~{1}点威力」2个{n}；4399「随机改为160~180点威力」2数字；8载体首位切片[150,220]（33494 下游 781/1040 随之对齐）。side_effect=0 使威力区间参数整段丢失。
-- 回滚：UPDATE side_effect SET arg_count=0 WHERE id=118;
UPDATE side_effect SET arg_count = 2 WHERE id = 118 AND arg_count = 0;
SELECT '[verify] effect 118 arg_count=' || arg_count FROM side_effect WHERE id = 118;
