-- R1 切片双表修复：effect 816 side_effect.arg_count 0→1
-- 证据：T3 缺陷根因：模板「免疫对手下{0}次攻击…并附加免疫量真伤」1个{n}；4399 弦月锋华「免疫并反弹对手下次造成的伤害」→{0}=1 且同技 843=[2,2]（下2回合先制+2）；15 载体全 1 参（24730/25531/26336/26578/26919 跨效果完美对齐）。引擎 effect_816 需 int_count≥3，se=0 时恒 2 → 静默 no-op。
-- 回滚：UPDATE side_effect SET arg_count=0 WHERE id=816;
UPDATE side_effect SET arg_count = 1 WHERE id = 816 AND arg_count = 0;
SELECT '[verify] effect 816 arg_count=' || arg_count FROM side_effect WHERE id = 816;
