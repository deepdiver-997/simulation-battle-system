-- J327 · 选择期路由数据化：custom_skill_selection_routes 29 行（2026-11-06，worktree ../j327）
-- 授权来源：数据刀清缴线（用户批方向）+ R4 计划 §3
--   docs_local/docs/05-任务清单/数据刀清缴线二批计划.md §3.1/§3.2（内核+DB 授权单 J327，
--   唯一内核+DB 写者 a01-j327；J326 同构先行单，j4 批次共用目录）。
--
-- 语义（迁「何时装槽、如何喂参」，不重写 29 个效果函数）：
--   自 skills.cpp loadSkills 两段 id 开关表迁移——27 个 plain（沿用本效果参数切片）
--   + 微弱族 3 条中 2 条 static（skill_static_elements：追加注入技能静态系别双槽
--   [element[0],element[1]]，2030/2490 判"此技能是否微弱"必须读静态槽防视图自污染；
--   785 判精灵系别视图，保持 plain）。条件判定仍在插件运行时执行：选择期只装节点，
--   on_selected 注册进 FIRST_MOVE_RIGHT 桶，FSM 侧顺序零改动。
--   表里是语义名（phase/state/adapter），禁止存 C++ enum 整数；首版只接受
--   phase='selection' × state='FIRST_MOVE_RIGHT'（on_selected 实际只建这一个桶，
--   R4 §3.2：不开放"表面可配置、实际无效"的多 state 接口）。
--
-- 纯增量：0 个 moves / side_effect / effect_info 行改写；只建一张内核自有表。
--   2298/2490 官方无 side_effect 定义行（args_num 兜底）是合法态——故**不设**
--   对 side_effect 的外键；29 id 全部有 effect_info 模板行（断言守卫拼写）。
--   static 适配行首版严格仅用于零参效果（args 固定 [owner,target,e0,e1]）。
--
-- 重放/重建：整脚本单事务（BEGIN…COMMIT）+ .bail on；断言表 CHECK(ok=1)。
--   纯增量补丁没有「官方表旧态/目标态」双态问题（一行官方数据都不改）：
--   缺表 → 建表+种子；已在目标态 → 幂等无操作；表存在但行集/列值被改（第三态）
--   → 目标态断言失败 → CLI 非零退出且事务回滚；表存在但 schema 缺列 → prepare
--   期报错同回滚。管线重建（fetch→import→apply_all.sh）后由本补丁恢复全部路由行。
--   （前置断言不引用本表：SQLite prepare 期解析全语句引用，旧态「表不存在」时
--     引用即报错——j4/1608_combo_rules.sql 同款注释教训。）
--
-- 回滚（手动；重放管线后需再跑本补丁恢复数据化态）：
--   DROP TABLE IF EXISTS custom_skill_selection_routes;
--   （内核自 J327 起对缺本表 fail-closed：回滚后新内核 Skills 构造会响亮报错，
--     需同步回退内核提交——部署绑定「新内核 + 本补丁」，见 R4 §5.1 原子发布门。）

.bail on
BEGIN TRANSACTION;

-- ═══ 建表 ＋ 29 行种子（INSERT OR IGNORE 幂等；完整性靠目标态断言兜底）══════
CREATE TABLE IF NOT EXISTS custom_skill_selection_routes (
    effect_id      INTEGER PRIMARY KEY,  -- 技能空间效果号（moves 命名空间；非精灵号/魂印号）
    phase          TEXT NOT NULL CHECK (phase = 'selection'),          -- 首版只开选择期
    state          TEXT NOT NULL CHECK (state = 'FIRST_MOVE_RIGHT'),   -- 语义名；on_selected 实际只建此桶
    arg_adapter    TEXT NOT NULL CHECK (arg_adapter IN ('plain', 'skill_static_elements')),
    schema_version INTEGER NOT NULL CHECK (schema_version >= 1),       -- loader 只认 1
    evidence       TEXT NOT NULL CHECK (length(evidence) > 0)          -- 来源/原内核行/插件文件（可审计）
);

INSERT OR IGNORE INTO custom_skill_selection_routes
    (effect_id, phase, state, arg_adapter, schema_version, evidence)
WITH seed(effect_id, adapter, ev) AS (
    VALUES
    -- ── 27 条 plain（沿用本效果参数切片；条件判定在插件运行时）──
    (2000, 'plain', '对手提升→先制+1且必中（大雪纷飞/烬灭神咒剑族；原内核 plain 表首位；插件 selection_seal.cpp）'),
    (610,  'plain', '遇天敌→先制+{0}（璨灵圣光族，1参；插件 selection_seal.cpp）'),
    (954,  'plain', '当前体力低于对手→先制+1（协议I·激 37354 等 36 技通用模板；插件 crawford_skills.cpp）'),
    (1848, 'plain', '对手下降→先制+2（王·御影神罚族/神觉·米斯蒂克；插件 mistike_skills.cpp）'),
    (881,  'plain', '自身下降→先制+3（神火剑阵 37221，2026-09-20 烧伤三精灵线；插件 burn_trio_skills.cpp/artemis_skills.cpp 双注册为既有撞号审计项，非本单改动）'),
    (1532, 'plain', '必暴（无神印）／神印 PP 后半句，非普通先制（神游·浩渺星河 2026-09-22 启灵线；插件 qiling_skills.cpp）'),
    (2298, 'plain', '自身无提升→先制+2（盖亚武道 J11；官方无 side_effect 定义行、effect_info.args_num 兜底；插件 j_gaia_stance.cpp）'),
    (779,  'plain', '对手提升→先制+2（盖亚武道 J11；插件 j_gaia_stance.cpp）'),
    (1860, 'plain', '自身无提升→先制+1且必中（天奔零式 J29；插件 j_boost_protect.cpp）'),
    (454,  'plain', '体力阈值 1/{0}→先制+{1}（J53 低体门，2参；插件 j_cond_priority.cpp）'),
    (1484, 'plain', '半血门→先制（J53；插件 j_cond_priority.cpp）'),
    (1487, 'plain', '护盾门→先制（J53；插件 j_cond_priority.cpp）'),
    (1491, 'plain', '遇天敌→先制+1（J66 首回合勘误代表：effect_register_state 路由赶不上首回合；插件 j_reversal_prio.cpp）'),
    (750,  'plain', '预武装 ACTION_START 穿伤害限制（J114 两级注册，MOVE_RIGHT 只装 tick；插件 j_first_move_gates2.cpp）'),
    (1508, 'plain', '预武装 ACTION_START 穿攻击免疫（J114；插件 j_first_move_gates2.cpp）'),
    (747,  'plain', '自身下降→先制+{0}（J74，1参；插件 j_negslot_prio.cpp）'),
    (1527, 'plain', '后手必暴的 ACTION_START 路由（J136 SECOND_ACTION_START；插件 j_crit_gates.cpp）'),
    (1465, 'plain', '先手概率暴/失败降属性（J136 FIRST_ACTION_START，1参；插件 j_crit_gates.cpp）'),
    (1243, 'plain', '对手下降→先制+1（J91，747 孪生；插件 j_foenslot_prio.cpp）'),
    (539,  'plain', '先制即时＋威力后半句自延迟（J103：先制半在选择期判，威力半由 body 自挂当回合 SKILL_EFFECT；插件 j_foeboost_prio_power.cpp）'),
    (958,  'plain', '指定异常门→先制+{1}（J115，2参 has_active_anomaly(对手,码)；插件 j_cond_prio2.cpp）'),
    (1174, 'plain', '自身提升→先制+2（J115 2298 孪生；插件 j_cond_prio2.cpp）'),
    (1081, 'plain', '对手提升→先制+1（J115 779 孪生；插件 j_cond_prio2.cpp）'),
    (1956, 'plain', '对手无异常→先制+1（J312 扫异常全集 0..43；插件 j_cond_prio3.cpp）'),
    (1292, 'plain', '自身体力低于对手→先制+{0}（J312 954 同款行参，1参；插件 j_cond_prio3.cpp）'),
    (95,   'plain', '睡眠门→暴击率加量 {0}/16，掷骰前直写 crit_rate_add（J254，1参；插件 j_sleep_crit_gate.cpp）'),
    (785,  'plain', '判精灵系别视图微弱→先制+2（用户 2026-09-14 口径：读 ws.view_elementalAttributes；插件 selection_seal.cpp）'),
    -- ── 2 条 static（skill_static_elements：追加注入技能静态系别双槽）──
    (2030, 'skill_static_elements', '判技能静态系别微弱→先制+1（args[2]/[3]=element[0]/[1] 防视图自污染；零参效果；插件 selection_seal.cpp）'),
    (2490, 'skill_static_elements', '微弱→先制+3/本系/神灵系奖励（args[2]/[3]=element[0]/[1]；官方无 side_effect 定义行、零参；插件 selection_seal.cpp）')
)
SELECT effect_id, 'selection', 'FIRST_MOVE_RIGHT', adapter, 1, ev FROM seed;

-- ═══ 目标态断言（J331 起为**种子集完整性**口径）＋ 引用面合法性（第三态在此拒绝）══
-- J327 原版断言「COUNT(*)=29 精确」；J331 扩行（tools/db_patches/j4/
-- selection_routes_expansion_3.sql，+3 条件先制族）后重放本补丁时表里已是 32 行
-- ——总数十钳制会让全补丁重放不收敛（apply_all.sh 重放序：本补丁在前、扩行在后）。
-- 故 J331 起本补丁只守**29 种子集**（下界 + 种子 id 逐个在表 + 合法性全量量化），
-- 「精确 32 行显式集合」由扩行补丁的目标态断言兜底（重放链最后一环是最严门）。
CREATE TEMP TABLE IF NOT EXISTS j4_selroute_assert (ok INTEGER CHECK (ok = 1));
DELETE FROM j4_selroute_assert;
INSERT INTO j4_selroute_assert (ok)
SELECT CASE WHEN (
    -- 总数下界（种子 29 全在；扩行只增不减——精确总数归扩行补丁断言）
    (SELECT COUNT(*) FROM custom_skill_selection_routes) >= 29
    -- 29 种子逐个在表（缺一即拒；多一不归本补丁管）
    AND (SELECT COUNT(*) FROM custom_skill_selection_routes
          WHERE effect_id IN (2000,610,954,1848,881,1532,2298,779,1860,454,
                              1484,1487,1491,750,1508,747,1527,1465,1243,539,
                              958,1174,1081,1956,1292,95,785,2030,2490)) = 29
    -- 全部路由行 phase/state/version 合法（按全量量化，扩行不破本条）
    AND (SELECT COUNT(*) FROM custom_skill_selection_routes
          WHERE phase = 'selection' AND state = 'FIRST_MOVE_RIGHT'
            AND schema_version = 1 AND length(evidence) > 0)
        = (SELECT COUNT(*) FROM custom_skill_selection_routes)
    -- 27 plain 种子全在 ＋ static 仍恰 2（扩行只准加 plain——新 adapter 须另立补丁）
    AND (SELECT COUNT(*) FROM custom_skill_selection_routes
          WHERE arg_adapter = 'plain') >= 27
    AND (SELECT COUNT(*) FROM custom_skill_selection_routes
          WHERE arg_adapter = 'skill_static_elements') = 2
    -- 引用面守卫：29 id 全部有 effect_info 模板行（防拼写型错 id）
    AND (SELECT COUNT(*) FROM custom_skill_selection_routes r
          WHERE NOT EXISTS (SELECT 1 FROM effect_info e WHERE e.id = r.effect_id)) = 0
    -- static 适配首版严格仅用于零参效果（args_num=0）
    AND (SELECT COUNT(*) FROM custom_skill_selection_routes r
          JOIN effect_info e ON e.id = r.effect_id
          WHERE r.arg_adapter = 'skill_static_elements' AND e.args_num != 0) = 0
) THEN 1 ELSE 0 END;

-- ═══ verify 输出 ═════════════════════════════════════════════════════════
SELECT '[verify] 路由表: ' || COUNT(*) || ' 行（种子下界 29；J331 扩行后 32；plain=' ||
       SUM(arg_adapter = 'plain') || ' static=' ||
       SUM(arg_adapter = 'skill_static_elements') || '）'
FROM custom_skill_selection_routes;
SELECT '[verify] 路由行: ' || group_concat(route, ' ')
FROM (SELECT effect_id || ':' || arg_adapter AS route
      FROM custom_skill_selection_routes ORDER BY effect_id);
SELECT '[verify] 引用面: ' || COUNT(*) || ' 处引用 / ' || COUNT(DISTINCT m.id) || ' 载体（R4 附录 A 基线 454/451）'
FROM moves m, json_each(m.side_effect) j
WHERE j.value IN (SELECT effect_id FROM custom_skill_selection_routes);

COMMIT;
