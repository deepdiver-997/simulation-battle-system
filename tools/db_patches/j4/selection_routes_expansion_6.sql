-- K469 · 选择期路由扩行四批：血量先制族 1 行（616）（2026-11-17，worktree
-- ~/.agent-workspaces/workspaces/sbs-local/k469，分支 feat/k469）
-- 授权来源：a31-orch 2026-11-17 裁决（K469 派发词"仅插件与独立场景"未预见条件
--   先制族路由行需求；616 与 J53 454 同形、条件先制族唯一正位=选择期路由为
--   J66 勘误系结论——按 J327/J331/J332/J335 四次扩行先例修正授权）；设计基线 =
--   J327 数据化（selection_routes_29.sql）+ J331 扩一（expansion_3，32 行态）+
--   J332 扩二（expansion_4，36 行态）+ J335 扩三（expansion_5，37 行态）；
--   **本次扩行后总行数 38**。
--
-- 本补丁是**纯增量**：不改写 37 行历史种子的任何列值；只 INSERT OR IGNORE 1 行。
--   前环补丁的目标态断言已按「种子集完整性」口径放宽（J332 对 expansion_3、
--   K469 对 expansion_5 的放宽先例 commit c01a5bb2 同款）：本补丁兜底
--   「精确 38 行显式集合」（apply_all.sh 重放链最后一环是最严门）。
--
-- 语义（与 J327/J331/J332/J335 同口径：表里是语义名，条件判定仍在插件运行时）：
--   args_num=2 的 plain 适配（复用既有枚举，不新造 adapter）；选择期只装节点，
--   on_selected 注册进 FIRST_MOVE_RIGHT 桶，FSM 侧顺序零改动。
--   616 条件 = 自身 hp×{0} < max_hp（454/1397"严格低于"整数直乘口径，恰 1/3 门闭）；
--   出 = ws.preemptive_level[owner] += {1}（优先级修改面：不触技能基础优先级，
--   先手权比较前生效——K457"必先→先制→速度"真实序的"先制"位）。
--   行为落地 = resources/moves_lib/k469_lowhp_prio.cpp（SKILL_EFFECT 注册；
--   缺注册时 loadSkills fail-closed 报「clone 无实现」，部署绑定新插件 dylib）。
--   行参消费 = 六载体切片恒 [3,2]（除数 3/先制量 2，与 effect_info 模板字面
--   "1/3"、"+2"逐位互证——行参在位即消费，不写死文案数字）。
--
-- 官方现查（seer_unity.sqlite effect_info，2026-11-17）：
--   616 | args_num=2 | 当自身血量少于1/3时先制+2
--   side_effect 定义行 arg_count=2（双表一致）；同文模板全库唯一（三查②）。
--   ⚠️ 双空间同号甄别：soul 空间 616 = 焰魂·金斯特 2863（row197）已由 K303 实现
--   （resources/soul_lib/k303_attack_burn_or_amp.cpp，SOUL_PROGRAM(616)，"每次使用
--   攻击技能都会使对手烧伤,若对手未烧伤则自身攻击+2"）——SKILL_EFFECT/SOUL_MARK
--   两注册空间互不相干，本行只路由 **moves 技能空间** 616（moves_lib/k469_
--   lowhp_prio.cpp），K303 不受影响（无交叠键段：K303 审计 990803000 /
--   本单审计 1016160100 子块）。
-- 载体（moves 现查 6 条，side_effect.arg_count 依序切分全链零剩余，2026-11-17
--   逐链手工核算 + 场景 T8565-pre load_skill_effects 实载锁定；friend_side_effect
--   全空；六载体链上均无既有路由 id——引用面纯 +6/+6）：
--   17660 破锐怒冲   物理 p140 [128,616] args[1,3,2]（616=[3,2]）← 3170 王·卡修斯
--   19323 龙怒天火断 物理 p150 [180,680,616] args[40,6,3,3,2] ← 3046 战龙哈莫
--   23413 海之契约   属性 p0   [616,43,57,5] args[3,2,2,3,4,5,100,-1] ← 2485 神·鱼龙王
--   23836 轮回天生   属性 p0   [705,616] args[3,2] ← 2667 不动明王
--   24822 心怀苍生   属性 p2   [616,902,749] args[3,2,1,100,1] ← 3074 仁天之君·刘备
--   38016 异怪波动   特殊 p10  [616,1627] args[3,2,1,7,15] ← 4796 基亚
--   （38016 同链 1627 已注册 = K465 连击+秒杀族，T8565-pre 切片核验与 K465
--     scenario_8520 T8520-pre 相容——616 路由化后其 616 节点满血门闭 no-op，
--     K465 行为面断言不受影响，2026-11-17 全量门复核）
-- 引用面变化：473/469（K450 刷新后基线）→ 479/475（+6 引用 / +6 载体）——
--   场景 7930 A/B 段期望值由本单同步（K469 同批）。
--
-- 重放/回滚：
--   整脚本单事务 + .bail on；目标态断言 CHECK(ok=1)，第三态 → CLI 非零退出且回滚。
--   回滚（手动）：
--     DELETE FROM custom_skill_selection_routes WHERE effect_id = 616;
--   （回滚后须同步回退插件 k469_lowhp_prio.cpp 与场景 8565/7930 期望——与
--     J327/J331/J332/J335 同款「内核/插件/数据三件套」部署绑定。）

.bail on
BEGIN TRANSACTION;

-- ═══ 扩 1 行（INSERT OR IGNORE 幂等；完整性靠目标态断言兜底）════════════════
INSERT OR IGNORE INTO custom_skill_selection_routes
    (effect_id, phase, state, arg_adapter, schema_version, evidence)
VALUES
    (616, 'selection', 'FIRST_MOVE_RIGHT', 'plain', 1,
     '当自身血量少于1/3时先制+2（effect_info 原文；6 载体 17660 破锐怒冲/19323 龙怒天火断/23413 海之契约/23836 轮回天生/24822 心怀苍生/38016 异怪波动，切片恒[3,2]逐位自洽；门=hp×3<max_hp 严格低于（454/1397 整数直乘口径，恰1/3门闭），出=ws.preemptive_level+=2 优先级修改面；J53 454 同形（j_cond_priority.cpp:36），a31-orch 2026-11-17 裁决扩行授权；soul 空间 616=K303 已实现（双空间同号甄别，K303 不受影响）；插件 k469_lowhp_prio.cpp）');

-- ═══ 目标态断言：38 种子集（2026-11-23 K487 放宽——后环 expansion_7 落地后本断言须容忍 39+ 行态；c01a5bb2/K469 放宽先例同款）══════════════
CREATE TEMP TABLE IF NOT EXISTS j4_selroute6_assert (ok INTEGER CHECK (ok = 1));
DELETE FROM j4_selroute6_assert;
INSERT INTO j4_selroute6_assert (ok)
SELECT CASE WHEN (
    -- 总数下界 38（种子集口径：后环扩行只增不减，本环不精确封顶）
    (SELECT COUNT(*) FROM custom_skill_selection_routes) >= 38
    AND (SELECT COUNT(*) FROM custom_skill_selection_routes
          WHERE effect_id IN (2000,610,954,1848,881,1532,2298,779,1860,454,
                              1484,1487,1491,750,1508,747,1527,1465,1243,539,
                              958,1174,1081,1956,1292,95,785,2030,2490,
                              1940,1941,2133,
                              2436,2345,2346,2347,
                              1774,
                              616)) = 38
    -- 全部路由行 phase/state/version 合法
    AND (SELECT COUNT(*) FROM custom_skill_selection_routes
          WHERE phase = 'selection' AND state = 'FIRST_MOVE_RIGHT'
            AND schema_version = 1 AND length(evidence) > 0) >= 38
    -- plain 下界 36（J335 35 + 本单 1；后环 plain 扩行容忍）
    AND (SELECT COUNT(*) FROM custom_skill_selection_routes
          WHERE arg_adapter = 'plain') >= 36
    AND (SELECT COUNT(*) FROM custom_skill_selection_routes
          WHERE arg_adapter = 'skill_static_elements') >= 2
    -- 引用面守卫：38 id 全部有 effect_info 模板行（防拼写型错 id）
    AND (SELECT COUNT(*) FROM custom_skill_selection_routes r
          WHERE NOT EXISTS (SELECT 1 FROM effect_info e WHERE e.id = r.effect_id)) = 0
    -- static 适配严格仅用于零参效果（args_num=0）
    AND (SELECT COUNT(*) FROM custom_skill_selection_routes r
          JOIN effect_info e ON e.id = r.effect_id
          WHERE r.arg_adapter = 'skill_static_elements' AND e.args_num != 0) = 0
    -- 本单 1 行双表一致（args_num=2 / side_effect.arg_count=2）
    AND (SELECT COUNT(*) FROM custom_skill_selection_routes r
          JOIN effect_info e ON e.id = r.effect_id
          WHERE r.effect_id = 616 AND e.args_num != 2) = 0
    AND (SELECT COUNT(*) FROM side_effect WHERE id = 616 AND arg_count != 2) = 0
    -- 引用面锚定（K450 基线 473/469 + 本单 +6/+6；SQL 复刻同 7930 B 段）
    AND (SELECT COUNT(*) FROM moves m, json_each(m.side_effect) j
          WHERE j.value IN (SELECT effect_id FROM custom_skill_selection_routes)) >= 479
    AND (SELECT COUNT(DISTINCT m.id) FROM moves m, json_each(m.side_effect) j
          WHERE j.value IN (SELECT effect_id FROM custom_skill_selection_routes)) >= 475
) THEN 1 ELSE 0 END;

-- ═══ verify 输出 ═════════════════════════════════════════════════════════
SELECT '[verify] 路由表: ' || COUNT(*) || ' 行（预期 38；plain=' ||
       SUM(arg_adapter = 'plain') || ' static=' ||
       SUM(arg_adapter = 'skill_static_elements') || '）'
FROM custom_skill_selection_routes;
SELECT '[verify] 引用面: ' || COUNT(*) || ' 处引用 / ' || COUNT(DISTINCT m.id) || ' 载体（K450 基线 473/469 + 本单 +6/+6）'
FROM moves m, json_each(m.side_effect) j
WHERE j.value IN (SELECT effect_id FROM custom_skill_selection_routes);

COMMIT;
