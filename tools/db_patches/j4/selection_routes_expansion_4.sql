-- J332 · 选择期路由扩行二批：条件/状态先制族 4 行（2436/2345/2346/2347）
-- （2026-11-06，worktree ../j332，分支 feat/j332）
-- 授权来源：数据刀清缴线（内核+DB 授权单 J332，唯一内核+DB 写者 a01-j332）；
--   设计基线 = J327 数据化（selection_routes_29.sql）+ J331 扩行一（
--   selection_routes_expansion_3.sql，32 行态）；对接面记档 = K420 头注
--   （resources/moves_lib/k420_ruiya_pool.cpp「落地范围裁决（2436 不产码）」段——
--   本单即其指名的续单：j4 补丁扩行 + 目标态断言同步 + 读「上回合先制级数」的
--   插件体，快照走 side 级槽式）。
--
-- 本补丁是**纯增量**：不改写 32 行历史种子的任何列值；只 INSERT OR IGNORE 4 行。
--   前环补丁的目标态断言已按「种子集完整性」口径放宽（J332 同批单独提交）：
--   expansion_3 精确 32 → 32 种子集；「精确 36 行显式集合」原由本补丁兜底
--   （apply_all.sh 重放链最后一环是最严门——重放收敛见隔离五验记档）。
--   ⚠️ 本补丁原「精确 36 行显式集合」断言已按 J335 派发口径放宽为**36 种子集**
--   （总数下界 36 + 36 种子逐个在表 + 合法性全量量化 + plain 只增不减下界）：
--   J335 扩行补丁（selection_routes_expansion_5.sql，+1：1774）使全补丁重放时
--   表里已是 37 行——精确总数十钳制会让 apply_all.sh 对已打补丁库重放不收敛
--   （重放序：本补丁在前、J335 扩行在后）。J332 对 expansion_3 的同款放宽先例。
--   「精确 37 行显式集合」由 J335 扩行补丁兜底（apply_all.sh 重放链最后一环是
--   最严门）。
--
-- 语义（与 J327/J331 同口径：表里是语义名，条件判定仍在插件运行时）：
--   四条都是 args_num=0 的 plain 适配（复用既有枚举，不新造 adapter）；
--   选择期只装节点，on_selected 注册进 FIRST_MOVE_RIGHT 桶，FSM 侧顺序零改动。
--   2436 条件 = 上回合先制级数快照（side 级槽 + 宠 id 消歧，自建键段 102436000x，
--   tools/check_storage_keys.py 备案）；2345/2346/2347 条件 = 英雄之耀 ≥3 层
--   （既有发布槽 kWujiYaoltKey=243601，soulmark_storage，resources/
--   wuji_shengwu_runtime.h——魂印侧契约，本单是消费端）。
--   2345-2347 的「此技能不受PP值限制」半句 = 选技期可用性修饰（PP=0 仍可选），
--   **不在路由节点体**（可选性检查先于 on_selected——R4 §3.3 步 1，路由体救不了
--   本回合 PP 门），由英雄之耀层数属主 wuji_shengwu_4800.cpp 的 ROUND_START
--   early 节点按层同步 per-skill IgnorePP（无为觉者 2260 ②「每回合重断言」先例）。
--   行为落地 = resources/moves_lib/j332_cond_prio_expansion.cpp（SKILL_EFFECT 注册；
--   缺注册时 loadSkills fail-closed 报「clone 无实现」，部署绑定新插件 dylib）。
--
-- 官方现查（seer_unity.sqlite effect_info，2026-11-06）：
--   2436 | args_num=0 | 自身上回合每有1级先制，先制+1，最高+3
--   2345 | args_num=0 | 英雄之耀达到3层时此技能不受PP值限制且额外先制+1
--   2346 | args_num=0 | 英雄之耀达到3层时此技能不受PP值限制且使用时解除自身所处的异常状态
--   2347 | args_num=0 | 英雄之耀达到3层时此技能不受PP值限制且使用时消除自身回合类效果
--   side_effect 定义表四 id 全无行（同 2298/2490 合法态：args_num 兜底，无外键）。
-- 载体（moves 现查，切片与 args_num 逐位自洽；monster_learnable_moves 全表单宠）：
--   神诗终幕 38328 [2436,9,429]（2436 零参切片）← 4888 慈辉永颂·瑞亚（K420 行清单同源）；
--   鋭武铭戈 38086 [2345,2195,2342,1208]（2345 零参切片）← 4800 无极圣武；
--   乱武天傀 29149 [2346,945,972,2348,2274]（2346 零参切片）← 4800；
--   烈武天征 29150 [2347,1014,971,191,843]（2347 零参切片）← 4800。
-- 候选裁决记档（其余 6 id 不落，如实记档）：
--   2534/2535（莫缇 4923 序曲 29395/源数 29396）：2534=纯「不受PP值限制」选技约束、
--     2535=先制+1，语义属选技期——但门条件「蜃都之序/之源的转化顺序位于…之后」
--     依赖蜃都四印记（effect_des 538-541，kind=1 场地族：序/源/逐/理）的产生端与
--     "转化顺序"状态机，该状态机不在技能数据内（K420 头注 ② 同判：需私造状态机）
--     → 条件不可真值化，语义不明不硬做，留印记体系专项。
--   2062（圣谕邪魇 墟埪 36851）：命中期多分支模板（消除对手回合类效果/反转提升/
--     双印记/湮灭触发）＝命中时结算，非选技期语义。
--   2063/2064（净世新生 28441/神明送葬 28442）：「切换为圣谕/邪魇形态」＝用技即时
--     形态切换，非选技期先制/选技约束。
--   2065（此身即无间 36856）：形态条件用技 rider（圣谕形态令对手随机 2 技 PP 归零 /
--     邪魇形态令对手属性技无效）＝命中/用技期，非选技期。
-- 引用面变化：457/453（J331 后基线）→ 461/457（+4 引用 / +4 载体）——
--   场景 7930 A/B 段期望值由本单同步。
--
-- 重放/回滚：
--   整脚本单事务 + .bail on；目标态断言 CHECK(ok=1)，第三态 → CLI 非零退出且回滚。
--   回滚（手动）：
--     DELETE FROM custom_skill_selection_routes WHERE effect_id IN (2436,2345,2346,2347);
--   （回滚后须同步回退插件 j332_cond_prio_expansion.cpp 与 wuji_shengwu_4800.cpp
--     的 ROUND_START 同步节点——与 J327/J331 同款「内核/插件/数据三件套」部署绑定。）

.bail on
BEGIN TRANSACTION;

-- ═══ 扩 4 行（INSERT OR IGNORE 幂等；完整性靠目标态断言兜底）════════════════
INSERT OR IGNORE INTO custom_skill_selection_routes
    (effect_id, phase, state, arg_adapter, schema_version, evidence)
VALUES
    (2436, 'selection', 'FIRST_MOVE_RIGHT', 'plain', 1,
     '自身上回合每有1级先制，先制+1，最高+3（effect_info 原文；载体 神诗终幕 38328[2436,9,429] 零参切片 4888 专属；条件=上回合先制级数快照 102436000x+side 段（宠 id 消歧），K420 头注指名续单；插件 j332_cond_prio_expansion.cpp）'),
    (2345, 'selection', 'FIRST_MOVE_RIGHT', 'plain', 1,
     '英雄之耀达到3层时此技能不受PP值限制且额外先制+1（effect_info 原文；载体 鋭武铭戈 38086[2345,2195,2342,1208] 零参切片 4800 专属；条件读英雄之耀槽 kWujiYaoltKey=243601=魂印契约；PP 半句由 wuji_shengwu_4800.cpp ROUND_START 同步 per-skill IgnorePP（2260 ②先例）；插件 j332_cond_prio_expansion.cpp）'),
    (2346, 'selection', 'FIRST_MOVE_RIGHT', 'plain', 1,
     '英雄之耀达到3层时此技能不受PP值限制且使用时解除自身所处的异常状态（effect_info 原文；载体 乱武天傀 29149[2346,945,972,2348,2274] 零参切片 4800 专属；解除半句按 J331 2133 先例在节点体执行（先手比较前，与「使用时」的时点偏差已记档）；PP 半句同 2345 口径；插件 j332_cond_prio_expansion.cpp）'),
    (2347, 'selection', 'FIRST_MOVE_RIGHT', 'plain', 1,
     '英雄之耀达到3层时此技能不受PP值限制且使用时消除自身回合类效果（effect_info 原文；载体 烈武天征 29150[2347,1014,971,191,843] 零参切片 4800 专属；消除半句=break_round_effects 自身侧，J331 2133 先例同上；PP 半句同 2345 口径；插件 j332_cond_prio_expansion.cpp）');

-- ═══ 目标态断言：36 种子集（32 历史 + 本单 4；缺一即拒，多一归后继扩行补丁）══
CREATE TEMP TABLE IF NOT EXISTS j4_selroute4_assert (ok INTEGER CHECK (ok = 1));
DELETE FROM j4_selroute4_assert;
INSERT INTO j4_selroute4_assert (ok)
SELECT CASE WHEN (
    -- 总数下界（36 种子全在；扩行只增不减——精确总数归最后一环扩行补丁断言）
    (SELECT COUNT(*) FROM custom_skill_selection_routes) >= 36
    AND (SELECT COUNT(*) FROM custom_skill_selection_routes
          WHERE effect_id IN (2000,610,954,1848,881,1532,2298,779,1860,454,
                              1484,1487,1491,750,1508,747,1527,1465,1243,539,
                              958,1174,1081,1956,1292,95,785,2030,2490,
                              1940,1941,2133,
                              2436,2345,2346,2347)) = 36
    -- 全部路由行 phase/state/version 合法（按全量量化，扩行不破本条）
    AND (SELECT COUNT(*) FROM custom_skill_selection_routes
          WHERE phase = 'selection' AND state = 'FIRST_MOVE_RIGHT'
            AND schema_version = 1 AND length(evidence) > 0)
        = (SELECT COUNT(*) FROM custom_skill_selection_routes)
    -- 34 plain 种子全在 ＋ static 仍恰 2（扩行只准加 plain——新 adapter 须另立补丁）
    AND (SELECT COUNT(*) FROM custom_skill_selection_routes
          WHERE arg_adapter = 'plain') >= 34
    AND (SELECT COUNT(*) FROM custom_skill_selection_routes
          WHERE arg_adapter = 'skill_static_elements') = 2
    -- 引用面守卫：36 id 全部有 effect_info 模板行（防拼写型错 id）
    AND (SELECT COUNT(*) FROM custom_skill_selection_routes r
          WHERE NOT EXISTS (SELECT 1 FROM effect_info e WHERE e.id = r.effect_id)) = 0
    -- static 适配严格仅用于零参效果（args_num=0）
    AND (SELECT COUNT(*) FROM custom_skill_selection_routes r
          JOIN effect_info e ON e.id = r.effect_id
          WHERE r.arg_adapter = 'skill_static_elements' AND e.args_num != 0) = 0
    -- 本单 4 行零参自洽（side_effect 定义行全缺 = args_num 兜底合法态；载体切片逐位核对见头部）
    AND (SELECT COUNT(*) FROM custom_skill_selection_routes r
          JOIN effect_info e ON e.id = r.effect_id
          WHERE r.effect_id IN (2436,2345,2346,2347) AND e.args_num != 0) = 0
) THEN 1 ELSE 0 END;

-- ═══ verify 输出 ═════════════════════════════════════════════════════════
SELECT '[verify] 路由表: ' || COUNT(*) || ' 行（本单种子下界 36；J335 扩行后 37；plain=' ||
       SUM(arg_adapter = 'plain') || ' static=' ||
       SUM(arg_adapter = 'skill_static_elements') || '）'
FROM custom_skill_selection_routes;
SELECT '[verify] 引用面: ' || COUNT(*) || ' 处引用 / ' || COUNT(DISTINCT m.id) || ' 载体（R4 基线 454/451 + J331 3/2 + 本单 4/4 + J335 8/8）'
FROM moves m, json_each(m.side_effect) j
WHERE j.value IN (SELECT effect_id FROM custom_skill_selection_routes);

COMMIT;
