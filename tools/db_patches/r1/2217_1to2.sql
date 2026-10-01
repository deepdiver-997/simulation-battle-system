-- R1 切片双表修复：effect 2217 side_effect.arg_count 1→2
-- 证据：模板 info 截断（「延」）{n} 不可用；moves 零载体（side_effect/friend_side_effect 均无，raw_json 命中皆 move id 子串）；唯一可用证据=上游 raw_json args_num=2 → 按 args_num 修正。无载体故对运行时零影响，仅为两表一致。
-- 回滚：UPDATE side_effect SET arg_count=1 WHERE id=2217;
UPDATE side_effect SET arg_count = 2 WHERE id = 2217 AND arg_count = 1;
SELECT '[verify] effect 2217 arg_count=' || arg_count FROM side_effect WHERE id = 2217;
