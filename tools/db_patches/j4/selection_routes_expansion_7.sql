-- K487 · 选择期路由扩行五批：回合窗回血+血量先制族 1 行（953）（2026-11-19，
-- worktree ~/.agent-workspaces/workspaces/sbs-local/k487，分支 feat/k487）
-- 授权来源：E 板 K487 行明写「血量门先制+{3}（616 先例直系）」——条件先制族唯一
--   正位=选择期路由（J66 勘误系结论：用技期注册赶不上首回合先手判定；K469 616
--   同判）。先例链：J327 数据化（selection_routes_29.sql）+ J331 扩一（32 行态）
--   + J332 扩二（36 行态）+ J335 扩三（37 行态）+ K469 扩四（38 行态）；
--   **本次扩行后总行数 39**。
--
-- 本补丁是**纯增量**：不改写 38 行历史种子的任何列值；只 INSERT OR IGNORE 1 行。
--   前环补丁的目标态断言已按「种子集完整性」口径放宽（J332 对 expansion_3、
--   K469 对 expansion_5 的放宽先例 commit c01a5bb2 同款）：本补丁兜底
--   「精确 39 行显式集合」（apply_all.sh 重放链最后一环是最严门）。
--
-- 语义（与 J327/J331/J332/J335/K469 同口径：表里是语义名，条件判定仍在插件运行时）：
--   args_num=4 的 plain 适配（复用既有枚举，不新造 adapter）；选择期只装节点，
--   on_selected 注册进 FIRST_MOVE_RIGHT 桶，FSM 侧顺序零改动。
--   953 先制半条件 = 自身 hp×100 < max_hp×{2}（严格低于整数直乘——454/1397/K469 616
--     同款口径，恰 {2}% 门闭）；出 = ws.preemptive_level[owner] += {3}（优先级修改面：
--     不触技能基础优先级，先手权比较前生效——K457"必先→先制→速度"真实序的"先制"位）。
--   953 回血半 = 路由 continue 吞普通注册 → 插件体自挂当回合 SKILL_EFFECT 桶武装节点
--     （539 双半直系 j_foeboost_prio_power.cpp）→ 真实用技趟开
--     ROUND_REDUCTION_ALL_ROUND_MINUS InRounds {0} 回合窗（762 tick 直系）。
--   行为落地 = resources/moves_lib/k487_regen_prio_and_foeboost_cap.cpp（SKILL_EFFECT
--     注册；缺注册时 loadSkills fail-closed 报「clone 无实现」，部署绑定新插件 dylib）。
--   行参消费 = 五载体切片 [4,3,40,2]×4 + [3,3,60,1]×1（行参在位即消费，不写死文案数字）。
--
-- 官方现查（seer_unity.sqlite effect_info，2026-11-19）：
--   953 | args_num=4 | {0}回合恢复最大体力的1/{1}，体力低于{2}%时先制+{3}
--   side_effect 定义行 arg_count=4（双表一致）；同文模板全库唯一（三查②）；
--   effect_des 953 无行（模板全文即权威）；boss_invalid_effects.json 含 953 =
--   effect_icon.tips 尾注官方名单（R7"消费层未实现"语义，非退役记录）。
--   ⚠️ 双空间同号甄别：effect_icon 953 行（宠 3347，"首次登场后第1回合所有技能
--     先制+2…"复合魂印模板）= **soul 空间模板占用**，SOUL_PROGRAM(953) 全库零注册
--     （无运行时实现无键段）→ 审计基址免子块拆分（K481 728 判据：soul 无实现=
--     无键段）；本行只路由 **moves 技能空间** 953，与魂印空间模板行互不相干。
--
-- 载体（moves 现查 5 条，side_effect.arg_count 依序切分全链零剩余，2026-11-19
--   逐链手工核算 + 场景 T8742-pre load_skill_effects 实载锁定；friend_side_effect
--   全空；五载体链上均无既有路由 id——引用面纯 +5/+5）：
--   24921 坠天断魂曲 属性 p0 [952,953,776] args[1,4,3,40,2,2]（953=[4,3,40,2]）← 3121
--   25269 地底核心   属性 p0 [585,953,843] args[1×6,4,3,40,2,2,1]（953=[4,3,40,2]）← 3280
--   26577 冰霸霸之魂 属性 p0 [4,4,4,953,780] args[…,4,3,40,2,…]（953=[4,3,40,2]）← 3801
--   28592 福龙临世   属性 p0 [48,953,557] args[4,4,3,40,2,4,100,20]（953=[4,3,40,2]，
--     tips"开始恢复自己的体力了"与回血子句互证——主载体）← 4602
--   28714 风翼护体   属性 p0 [138,48,953] args[1,3,3,3,3,60,1]（953=[3,3,60,1]，
--     唯一 {0}=3/{2}=60/{3}=1 差分载体）← 4639/4640
--   （25269 同链 843=下2回合全技能先制+1 已注册 round_window.cpp——行为面先制断言
--     避开本载体，T8742-pre 切片核验不受影响）
--
-- 引用面历史基线：479/475（K469）+ 五个 953 载体 = 484/480，作为下限。
--   滚动镜像可能新增已有路由的载体；953 的五个已知载体仍各须精确含一条 953。
--
-- 重放/回滚：
--   整脚本单事务 + .bail on；目标态断言 CHECK(ok=1)，第三态 → CLI 非零退出且回滚。
--   回滚（手动）：
--     DELETE FROM custom_skill_selection_routes WHERE effect_id = 953;
--   （回滚后须同步回退插件 k487_regen_prio_and_foeboost_cap.cpp 与场景
--     8742/7930 期望——与 J327/K469 同款「内核/插件/数据三件套」部署绑定。）

.bail on
BEGIN TRANSACTION;

-- ═══ 扩 1 行（INSERT OR IGNORE 幂等；完整性靠目标态断言兜底）════════════════
INSERT OR IGNORE INTO custom_skill_selection_routes
    (effect_id, phase, state, arg_adapter, schema_version, evidence)
VALUES
    (953, 'selection', 'FIRST_MOVE_RIGHT', 'plain', 1,
     '{0}回合恢复最大体力的1/{1}，体力低于{2}%时先制+{3}（effect_info 原文；5 载体
     24921 坠天断魂曲/25269 地底核心/26577 冰霸霸之魂/28592 福龙临世/28714 风翼护体，
     切片[4,3,40,2]×4+[3,3,60,1]逐位自洽；先制半门=hp×100<max_hp×{2} 严格低于整数直乘
     （454/1397/K469 616 口径，恰{2}%门闭），出=ws.preemptive_level+={3} 优先级修改面；
     回血半=路由 continue 吞普通注册→插件体自挂当回合 SKILL_EFFECT 武装节点（539 双半
     直系）→ROUND_REDUCTION InRounds {0} 回合窗（762 tick 直系）；K469 616 同形
     （k469_lowhp_prio.cpp:36 先例 + j_cond_priority.cpp:454 同形），E 板 K487 行
     616 先例直系授权；soul 空间 953 仅 effect_icon 模板行无运行时注册（审计基址免
     子块拆分）；插件 k487_regen_prio_and_foeboost_cap.cpp）');

-- ═══ 目标态断言：39 行显式集合（38 历史 + 本单 1；缺一多一皆拒）══════════════
CREATE TEMP TABLE IF NOT EXISTS j4_selroute7_assert (ok INTEGER CHECK (ok = 1));
DELETE FROM j4_selroute7_assert;
INSERT INTO j4_selroute7_assert (ok)
SELECT CASE WHEN (
    -- 总数与集合精确（39 行、缺一多一皆拒——重放链最后一环）
    (SELECT COUNT(*) FROM custom_skill_selection_routes) = 39
    AND (SELECT COUNT(*) FROM custom_skill_selection_routes
          WHERE effect_id IN (2000,610,954,1848,881,1532,2298,779,1860,454,
                              1484,1487,1491,750,1508,747,1527,1465,1243,539,
                              958,1174,1081,1956,1292,95,785,2030,2490,
                              1940,1941,2133,
                              2436,2345,2346,2347,
                              1774,
                              616,
                              953)) = 39
    -- 全部路由行 phase/state/version 合法
    AND (SELECT COUNT(*) FROM custom_skill_selection_routes
          WHERE phase = 'selection' AND state = 'FIRST_MOVE_RIGHT'
            AND schema_version = 1 AND length(evidence) > 0) = 39
    -- 37 plain（J335 35 + K469 1 + 本单 1）＋ 2 static，无第三种
    AND (SELECT COUNT(*) FROM custom_skill_selection_routes
          WHERE arg_adapter = 'plain') = 37
    AND (SELECT COUNT(*) FROM custom_skill_selection_routes
          WHERE arg_adapter = 'skill_static_elements') = 2
    -- 引用面守卫：39 id 全部有 effect_info 模板行（防拼写型错 id）
    AND (SELECT COUNT(*) FROM custom_skill_selection_routes r
          WHERE NOT EXISTS (SELECT 1 FROM effect_info e WHERE e.id = r.effect_id)) = 0
    -- static 适配严格仅用于零参效果（args_num=0）
    AND (SELECT COUNT(*) FROM custom_skill_selection_routes r
          JOIN effect_info e ON e.id = r.effect_id
          WHERE r.arg_adapter = 'skill_static_elements' AND e.args_num != 0) = 0
    -- 本单路由及双表参数定义必须在位（953 plain，args_num=4 / arg_count=4）。
    AND (SELECT COUNT(*) FROM custom_skill_selection_routes r
          JOIN effect_info e ON e.id = r.effect_id
          WHERE r.effect_id = 953 AND r.arg_adapter = 'plain' AND e.args_num = 4) = 1
    AND (SELECT COUNT(*) FROM side_effect WHERE id = 953 AND arg_count = 4) = 1
    -- 固定载体缺失或重复不能被镜像新增引用掩盖。
    AND (SELECT COUNT(*) FROM (
          SELECT m.id FROM moves m, json_each(m.side_effect) j
          WHERE m.id IN (24921,25269,26577,28592,28714) AND j.value = 953
          GROUP BY m.id HAVING COUNT(*) = 1
    )) = 5
    -- 全库引用/载体是滚动镜像统计，484/480 为历史下限（同场景 7930/8742）。
    AND (SELECT COUNT(*) FROM moves m, json_each(m.side_effect) j
          WHERE j.value IN (SELECT effect_id FROM custom_skill_selection_routes)) >= 484
    AND (SELECT COUNT(DISTINCT m.id) FROM moves m, json_each(m.side_effect) j
          WHERE j.value IN (SELECT effect_id FROM custom_skill_selection_routes)) >= 480
) THEN 1 ELSE 0 END;

-- ═══ verify 输出 ═════════════════════════════════════════════════════════
SELECT '[verify] 路由表: ' || COUNT(*) || ' 行（预期 39；plain=' ||
       SUM(arg_adapter = 'plain') || ' static=' ||
       SUM(arg_adapter = 'skill_static_elements') || '）'
FROM custom_skill_selection_routes;
SELECT '[verify] 引用面: ' || COUNT(*) || ' 处引用 / ' || COUNT(DISTINCT m.id) || ' 载体（历史下限 484/480；五个固定 953 载体各一处）'
FROM moves m, json_each(m.side_effect) j
WHERE j.value IN (SELECT effect_id FROM custom_skill_selection_routes);

COMMIT;
