-- J331 · 选择期路由扩行：条件先制族 3 行（1940/1941/2133）（2026-11-06，worktree ../j331）
-- 授权来源：数据刀清缴线（内核+DB 授权单 J331，唯一内核+DB 写者 a01-j331）；
--   设计基线 = J327 数据化（tools/db_patches/j4/selection_routes_29.sql），
--   对接面记档 = K418 头注（resources/moves_lib/k418_puluxiutuo_pool.cpp
--   「落地范围裁决 1940/1941 不产码」段 + 遗留记档）与 K417 头注
--   （resources/moves_lib/k417_debubite_pool.cpp 遗留记档 2132/2133 段）。
--
-- 本补丁是**纯增量**：不改写 29 行历史种子的任何列值；只 INSERT OR IGNORE 3 行。
--   老补丁的目标态断言已按「种子集完整性」口径放宽（J331 同批单独提交）：
--   总数下界 29 + 种子逐个在表 + 合法性全量量化。
--   ⚠️ 本补丁原「精确 32 行显式集合」断言已按 J332 派发口径放宽为**32 种子集**
--   （总数下界 32 + 32 种子逐个在表 + 合法性全量量化 + plain 只增不减下界）：
--   J332 扩行补丁（selection_routes_expansion_4.sql，+4：2436/2345/2346/2347）
--   使全补丁重放时表里已是 36 行——精确总数十钳制会让 apply_all.sh 对已打补丁库
--   重放不收敛（重放序：本补丁在前、J332 扩行在后）。「精确 36 行显式集合」由
--   J332 扩行补丁兜底（apply_all.sh 重放链最后一环是最严门）。
--
-- 语义（与 J327 同口径：表里是语义名，条件判定仍在插件运行时）：
--   三条都是 args_num=0 的 plain 适配（复用既有枚举，不新造 adapter）；
--   选择期只装节点，on_selected 注册进 FIRST_MOVE_RIGHT 桶，FSM 侧顺序零改动。
--   条件判定读标记功能槽（K417/K418 建的 side 级契约，0/缺=未持有）：
--     回能Ⅰ 4696000+side、失能Ⅰ 4696010+side（K418 段）；
--     回体Ⅰ 4695000+side（K417 段）。
--   行为落地 = resources/moves_lib/j331_marker_cond_prio.cpp（SKILL_EFFECT 注册；
--   缺注册时 loadSkills fail-closed 报「clone 无实现」，部署绑定新插件 dylib）。
--
-- ⚠️ 勘误记档（对 K417 头注）：2133 官方模板全文 =
--   「自身持有回体时先制+1，**否则为自身附加3回合的回体并令对手疲惫**」——
--   K417 头注遗留记档只引了前半句（"2133'自身持有回体时先制+1'"）。本单按
--   effect_info 原文全量落地：持回体 → 先制+1（选择期）；未持有 → 否则支
--   （附加 3 回合回体 + 令对手疲惫），两支都在 FIRST_MOVE_RIGHT 节点体里执行。
--   1940/1941 模板无后缀句，纯条件先制（K418 头注与官方原文一致）。
--
-- 官方现查（seer_unity.sqlite effect_info，2026-11-06）：
--   1940 | args_num=0 | 自身持有回能标记时先制+2
--   1941 | args_num=0 | 对手持有失能标记时先制+2
--   2133 | args_num=0 | 自身持有回体时先制+1，否则为自身附加3回合的回体并令对手疲惫
-- 载体（moves 现查，side_effect.arg_count 与 effect_info.args_num 逐位一致）：
--   守一 36503 [1940,1941,1922,1776]（flat[2,2]：1940/1941 零参切片、1922/1776 各 [2]）；
--   monster_learnable_moves：36503 ← 4489 普鲁修托（K418 选载体依据同源）。
--   缘结·浮世诵 37021 [699,1568,2132,2133,837]（flat[200,100,400] 全归 837）；
--   monsters.raw_json sp_extra_moves：37021 ← 4499 德布比特（learning_lv 77，K417 记档同源）。
-- 引用面变化：454/451（R4 附录 A 基线）→ 457/453（+3 引用 / +2 载体）——
--   场景 7930 B 段期望值由本单同步。
--
-- 重放/回滚：
--   整脚本单事务 + .bail on；目标态断言 CHECK(ok=1)，第三态 → CLI 非零退出且回滚。
--   回滚（手动）：
--     DELETE FROM custom_skill_selection_routes WHERE effect_id IN (1940,1941,2133);
--   （回滚后须同步回退插件 j331_marker_cond_prio.cpp 与场景期望——与 J327 同款
--     「内核/插件/数据三件套」部署绑定。）

.bail on
BEGIN TRANSACTION;

-- ═══ 扩 3 行（INSERT OR IGNORE 幂等；完整性靠目标态断言兜底）════════════════
INSERT OR IGNORE INTO custom_skill_selection_routes
    (effect_id, phase, state, arg_adapter, schema_version, evidence)
VALUES
    (1940, 'selection', 'FIRST_MOVE_RIGHT', 'plain', 1,
     '自身持有回能标记时先制+2（effect_info 原文；载体 守一 36503[1940,1941,1922,1776] 4489 专属；条件读回能Ⅰ槽 4696000+side=K418 段契约；插件 j331_marker_cond_prio.cpp）'),
    (1941, 'selection', 'FIRST_MOVE_RIGHT', 'plain', 1,
     '对手持有失能标记时先制+2（effect_info 原文；载体 守一 36503（4489 专属）；条件读失能Ⅰ槽 4696010+side=K418 段契约；插件 j331_marker_cond_prio.cpp）'),
    (2133, 'selection', 'FIRST_MOVE_RIGHT', 'plain', 1,
     '自身持有回体时先制+1，否则为自身附加3回合的回体并令对手疲惫（effect_info 原文全文=含否则支，K417 头注原引前半句已由本单勘误；载体 缘结·浮世诵 37021[699,1568,2132,2133,837] 4499 sp_extra_moves；回体Ⅰ槽 4695000+side=K417 段契约；插件 j331_marker_cond_prio.cpp）');

-- ═══ 目标态断言：32 种子集（29 历史 + 本单 3；缺一即拒，多一归后继扩行补丁）══
CREATE TEMP TABLE IF NOT EXISTS j4_selroute3_assert (ok INTEGER CHECK (ok = 1));
DELETE FROM j4_selroute3_assert;
INSERT INTO j4_selroute3_assert (ok)
SELECT CASE WHEN (
    -- 总数下界（32 种子全在；扩行只增不减——精确总数归最后一环扩行补丁断言）
    (SELECT COUNT(*) FROM custom_skill_selection_routes) >= 32
    AND (SELECT COUNT(*) FROM custom_skill_selection_routes
          WHERE effect_id IN (2000,610,954,1848,881,1532,2298,779,1860,454,
                              1484,1487,1491,750,1508,747,1527,1465,1243,539,
                              958,1174,1081,1956,1292,95,785,2030,2490,
                              1940,1941,2133)) = 32
    -- 全部路由行 phase/state/version 合法（按全量量化，扩行不破本条）
    AND (SELECT COUNT(*) FROM custom_skill_selection_routes
          WHERE phase = 'selection' AND state = 'FIRST_MOVE_RIGHT'
            AND schema_version = 1 AND length(evidence) > 0)
        = (SELECT COUNT(*) FROM custom_skill_selection_routes)
    -- 30 plain 种子全在 ＋ static 仍恰 2（扩行只准加 plain——新 adapter 须另立补丁）
    AND (SELECT COUNT(*) FROM custom_skill_selection_routes
          WHERE arg_adapter = 'plain') >= 30
    AND (SELECT COUNT(*) FROM custom_skill_selection_routes
          WHERE arg_adapter = 'skill_static_elements') = 2
    -- 引用面守卫：32 id 全部有 effect_info 模板行（防拼写型错 id）
    AND (SELECT COUNT(*) FROM custom_skill_selection_routes r
          WHERE NOT EXISTS (SELECT 1 FROM effect_info e WHERE e.id = r.effect_id)) = 0
    -- static 适配严格仅用于零参效果（args_num=0）
    AND (SELECT COUNT(*) FROM custom_skill_selection_routes r
          JOIN effect_info e ON e.id = r.effect_id
          WHERE r.arg_adapter = 'skill_static_elements' AND e.args_num != 0) = 0
    -- 本单 3 行零参自洽（载体切片与 effect_info.args_num 逐位核对，见头部）
    AND (SELECT COUNT(*) FROM custom_skill_selection_routes r
          JOIN effect_info e ON e.id = r.effect_id
          WHERE r.effect_id IN (1940,1941,2133) AND e.args_num != 0) = 0
) THEN 1 ELSE 0 END;

-- ═══ verify 输出 ═════════════════════════════════════════════════════════
SELECT '[verify] 路由表: ' || COUNT(*) || ' 行（本单种子下界 32；J332 扩行后 36；plain=' ||
       SUM(arg_adapter = 'plain') || ' static=' ||
       SUM(arg_adapter = 'skill_static_elements') || '）'
FROM custom_skill_selection_routes;
SELECT '[verify] 引用面: ' || COUNT(*) || ' 处引用 / ' || COUNT(DISTINCT m.id) || ' 载体（R4 基线 454/451 + 本单 3/2 + J332 4/4）'
FROM moves m, json_each(m.side_effect) j
WHERE j.value IN (SELECT effect_id FROM custom_skill_selection_routes);

COMMIT;
