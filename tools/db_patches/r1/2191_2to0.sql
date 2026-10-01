-- R1 切片双表修复：effect 2191 side_effect.arg_count 2→0
-- 证据：4399 旷海回音「为对手附加浪潮聚焦」无数字（28752 moves.info 同文）；28752 切片 7 参在 0 参时下游完美：191=[4]↔「4回合内免疫并反弹异常」、1819=[100,15,1]↔「100%冰封未触发免疫1次」、2040=[3,100,5]↔「3回合切换冻伤」。旧值 2 劫持 191 的回合参数（[4,100] 被吞，191 只剩 [15]）。effect_info 行存在 args_num=0 同证。
-- 回滚：UPDATE side_effect SET arg_count=2 WHERE id=2191;
UPDATE side_effect SET arg_count = 0 WHERE id = 2191 AND arg_count = 2;
SELECT '[verify] effect 2191 arg_count=' || arg_count FROM side_effect WHERE id = 2191;
