-- R1 切片双表修复：effect 1515 side_effect.arg_count 4→5
-- 证据：模板 5个{n}；4399 艾欧丽娅「高于1700→+170%，低于1700→附加自身速度值17%」↔38419 切片[1700,170,1700,4,17]逐位吻合（{3}=4=速度槽码）；刑天[750,50,750,0,50]。37645 单载体 5 参完整。
-- 回滚：UPDATE side_effect SET arg_count=4 WHERE id=1515;
UPDATE side_effect SET arg_count = 5 WHERE id = 1515 AND arg_count = 4;
SELECT '[verify] effect 1515 arg_count=' || arg_count FROM side_effect WHERE id = 1515;
