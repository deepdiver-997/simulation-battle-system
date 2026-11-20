-- J335 · 选择期路由扩行三批：当回合条件先制族 1 行（1774）（2026-11-07，worktree
-- ../j335，分支 feat/j335）
-- 授权来源：数据刀清缴线（内核+DB 授权单 J335，唯一内核+DB 写者 a01-j335）；
--   设计基线 = J327 数据化（tools/db_patches/j4/selection_routes_29.sql）+
--   J331 扩行一（selection_routes_expansion_3.sql，32 行态）+ J332 扩行二
--   （selection_routes_expansion_4.sql，36 行态）；对接面记档 = K425 头注
--   （resources/moves_lib/k425_tiaojian_xianzhi_pool.cpp「留续单（施工清单）」段——
--   本单即其指名的续单：j4 补丁扩行 + SKILL_EFFECT(1774) 插件体（2273 同形门判据）
--   + 场景 8080 行为面）。
--
-- 本补丁是**纯增量**：不改写 36 行历史种子的任何列值；只 INSERT OR IGNORE 1 行。
--   前环补丁的目标态断言已按「种子集完整性」口径放宽（J332 对 expansion_3 的放宽
--   先例；expansion_4 的「精确 36 行显式集合」由本单同批放宽为 36 种子集）。
--   ⚠️ 本补丁原「精确 37 行显式集合」断言已按 K469 派发口径放宽为**37 种子集**
--   （总数下界 37 + 37 种子逐个在表 + 合法性全量量化 + plain 下界 35 + static 恰 2；
--   放宽先例 commit = c01a5bb2「expansion_3 精确 32 → 32 种子集口径」同款）：
--   K469 扩行补丁（selection_routes_expansion_6.sql，+1：616 血量先制族）使全补丁
--   重放时表里已是 38 行——精确总数十钳制会让 apply_all.sh 对已打补丁库重放不收敛
--   （重放序：本补丁在前、K469 扩行在后）。「精确 38 行显式集合」由 K469 扩行补丁
--   兜底（apply_all.sh 重放链最后一环是最严门）。
--
-- 语义（与 J327/J331/J332 同口径：表里是语义名，条件判定仍在插件运行时）：
--   args_num=0 的 plain 适配（复用既有枚举，不新造 adapter）；选择期只装节点，
--   on_selected 注册进 FIRST_MOVE_RIGHT 桶，FSM 侧顺序零改动。
--   1774 条件 = 对手本回合所选技能为攻击技（物理/特殊；roundChoice[opp][0]==SELECT_SKILL
--   且 skills[idx].type != Attribute——2273 同形判据，common.h selected_skill_is_attack
--   统一判据）；出 = ws.preemptive_level[owner] += 1（零参宏，先制量写死模板文案 +1，
--   无行参可消费——§七点五口径）。嗑药/切宠等非选技动作门闭（roundChoice[opp][0]!=1）。
--   行为落地 = resources/moves_lib/j335_cond_prio_vs_attack.cpp（SKILL_EFFECT 注册；
--   缺注册时 loadSkills fail-closed 报「clone 无实现」，部署绑定新插件 dylib）。
--
-- 官方现查（seer_unity.sqlite effect_info，2026-11-07）：
--   1774 | args_num=0 | 若对手当回合使用的技能为攻击技能则自身额外先制+1
--   side_effect 定义行 arg_count=0（双表一致，与 args_num 逐位同）。
-- 载体（moves 现查 8 条，切片逐位自洽——K425 头注行清单 2026-11-07 逐行核算，本单
--   抽验 28051/28366/35652/36277 四条复算一致；friend_side_effect 全空）：
--   28051 启明之辰     属性 p0   [1774,191,847,1342] args[4,400,400]
--                      （1774 零参切片）← 4408 派利尔 lv49
--   28366 饭后甜点     属性 p0   [1617,1774,870] args[3,70,2,1,400]
--                      ← 4526 华夫家族 lv57
--   28668 安魂引       属性 p0   [1774,191,426,1400] args[4,3,100,3,100,6]
--                      ← 3941 混沌暗王霍德尔（sp_hide_moves id=10042 隐藏技）
--   35652 几曾识干戈   物理 p150 [697,1568,1774,1395] args[]（全链零参）
--                      ← 4327 极战·阿尔斯兰 lv75
--   35786 兽灵震谷爪   物理 p150 [1774,664,1736] args[4] ← 4353 花寅 lv61
--   35907 御风无痕     物理 p85  [1774,1099,769] args[1,100] ← 4376 圣谕之风·岚 lv71
--   36277 雾停枝       特殊 p130 [1774] args[]（纯单载体）← 4448 流荧·雪 lv45
--   36411 灰刻截光破   物理 p150 [1774,925,937] args[50,100,30,2,1] ← 4472 夜罗·琉彩 lv61
--   链上 16 个兄弟效果已全部注册（registry moves 现查逐一比对全 True）；本族唯一
--   缺口 = 1774 本体（K425 四查 ③ 同结论）。boss_invalid_effects.json 的 1774 命中
--   = 魂印空间 effect_icon 同号行（namespace:"soul"，K425 头注已溯源——SKILL_EFFECT/
--   SOUL_MARK 两注册空间互不相干，非撞号）。
-- 引用面变化：461/457（J332 后基线）→ 469/465（+8 引用 / +8 载体——8 载体链上
--   均无既有路由 id，引用与载体同步 +8）——场景 7930 A/B 段期望值由本单同步。
--
-- 重放/回滚：
--   整脚本单事务 + .bail on；目标态断言 CHECK(ok=1)，第三态 → CLI 非零退出且回滚。
--   回滚（手动）：
--     DELETE FROM custom_skill_selection_routes WHERE effect_id = 1774;
--   （回滚后须同步回退插件 j335_cond_prio_vs_attack.cpp 与场景 8080 期望——与
--     J327/J331/J332 同款「内核/插件/数据三件套」部署绑定。）

.bail on
BEGIN TRANSACTION;

-- ═══ 扩 1 行（INSERT OR IGNORE 幂等；完整性靠目标态断言兜底）════════════════
INSERT OR IGNORE INTO custom_skill_selection_routes
    (effect_id, phase, state, arg_adapter, schema_version, evidence)
VALUES
    (1774, 'selection', 'FIRST_MOVE_RIGHT', 'plain', 1,
     '若对手当回合使用的技能为攻击技能则自身额外先制+1（effect_info 原文；8 载体 28051 启明之辰/28366 饭后甜点/28668 安魂引/35652 几曾识干戈/35786 兽灵震谷爪/35907 御风无痕/36277 雾停枝/36411 灰刻截光破，零参切片逐位自洽；门=对手选攻击技（roundChoice==SELECT_SKILL 且 type!=Attribute，2273 同形判据），K425 头注指名续单；插件 j335_cond_prio_vs_attack.cpp）');

-- ═══ 目标态断言：37 种子集（K469 放宽口径：总数下界 37 + 种子逐个在表 +
--     合法性全量量化 + plain 下界 35 + static 恰 2；「精确 38 行显式集合」
--     由 K469 扩行补丁兜底——重放链最后一环是最严门）══════════════
CREATE TEMP TABLE IF NOT EXISTS j4_selroute5_assert (ok INTEGER CHECK (ok = 1));
DELETE FROM j4_selroute5_assert;
INSERT INTO j4_selroute5_assert (ok)
SELECT CASE WHEN (
    -- 总数下界（K469 扩行后 38 行——精确钳制已放宽，见头部记档）
    (SELECT COUNT(*) FROM custom_skill_selection_routes) >= 37
    AND (SELECT COUNT(*) FROM custom_skill_selection_routes
          WHERE effect_id IN (2000,610,954,1848,881,1532,2298,779,1860,454,
                              1484,1487,1491,750,1508,747,1527,1465,1243,539,
                              958,1174,1081,1956,1292,95,785,2030,2490,
                              1940,1941,2133,
                              2436,2345,2346,2347,
                              1774)) = 37
    -- 全部路由行 phase/state/version 合法（种子与后续扩行全量化）
    AND (SELECT COUNT(*) FROM custom_skill_selection_routes
          WHERE phase = 'selection' AND state = 'FIRST_MOVE_RIGHT'
            AND schema_version = 1 AND length(evidence) > 0) >= 37
    -- plain 下界 35 ＋ static 恰 2（plain 只增不减；static 无第三态）
    AND (SELECT COUNT(*) FROM custom_skill_selection_routes
          WHERE arg_adapter = 'plain') >= 35
    AND (SELECT COUNT(*) FROM custom_skill_selection_routes
          WHERE arg_adapter = 'skill_static_elements') = 2
    -- 引用面守卫：37 id 全部有 effect_info 模板行（防拼写型错 id）
    AND (SELECT COUNT(*) FROM custom_skill_selection_routes r
          WHERE NOT EXISTS (SELECT 1 FROM effect_info e WHERE e.id = r.effect_id)) = 0
    -- static 适配严格仅用于零参效果（args_num=0）
    AND (SELECT COUNT(*) FROM custom_skill_selection_routes r
          JOIN effect_info e ON e.id = r.effect_id
          WHERE r.arg_adapter = 'skill_static_elements' AND e.args_num != 0) = 0
    -- 本单 1 行零参自洽（双表 arg_count/args_num 一致；8 载体切片逐位核对见头部）
    AND (SELECT COUNT(*) FROM custom_skill_selection_routes r
          JOIN effect_info e ON e.id = r.effect_id
          WHERE r.effect_id = 1774 AND e.args_num != 0) = 0
    AND (SELECT COUNT(*) FROM side_effect WHERE id = 1774 AND arg_count != 0) = 0
) THEN 1 ELSE 0 END;

-- ═══ verify 输出 ═════════════════════════════════════════════════════════
SELECT '[verify] 路由表: ' || COUNT(*) || ' 行（本环种子 37；K469 扩行后 38；plain=' ||
       SUM(arg_adapter = 'plain') || ' static=' ||
       SUM(arg_adapter = 'skill_static_elements') || '）'
FROM custom_skill_selection_routes;
SELECT '[verify] 引用面: ' || COUNT(*) || ' 处引用 / ' || COUNT(DISTINCT m.id) || ' 载体（R4 基线 454/451 + J331 3/2 + J332 4/4 + 本单 8/8）'
FROM moves m, json_each(m.side_effect) j
WHERE j.value IN (SELECT effect_id FROM custom_skill_selection_routes);

COMMIT;
