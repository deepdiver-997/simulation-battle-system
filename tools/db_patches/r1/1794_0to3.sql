-- R1 切片双表修复：effect 1794 side_effect.arg_count 0→3
-- 证据：模板 args_num=3（文案 1/2·下1次·50% 三数烤死）；全 7 载体尾位 [2,1,50] 一致（36005 首位、28933/29025/29213/29346/29398/35813 组合跨效果完美对齐）。
-- 回滚：UPDATE side_effect SET arg_count=0 WHERE id=1794;
UPDATE side_effect SET arg_count = 3 WHERE id = 1794 AND arg_count = 0;
SELECT '[verify] effect 1794 arg_count=' || arg_count FROM side_effect WHERE id = 1794;
