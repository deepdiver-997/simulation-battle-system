-- J326 · 连击静态规则数据化：1608/1609/1610 显式行参 + 16 行索引表（2026-11-05，worktree ../agents/j326）
-- 授权来源：数据刀清缴线（用户批方向）+ R4 计划
--   docs_local/docs/05-任务清单/数据刀清缴线二批计划.md §2（J326 首选项＝规范化行参＋通用索引规则表；
--   备选 fixed/args 双分支线未采纳——不引入两套真相，调试台参数覆盖得以直通）。
--
-- 语义（三件事）：
--   ① 1608/1609/1610（「召唤自己的伙伴进行5-10次攻击…」）零参定义行规范化为显式行参：
--      side_effect.arg_count 0→2，三载体（34454/34457/34460，链 [451,160x]）平铺参数
--      [100,1] → [100,1,5,10]。**[5,10] 是本地规范化参数，不声称官方曾提供**；
--      effect_info.args_num=0 与官方文本原文保留——两表 0/2 差异是本补丁白名单验收项，
--      后续清洗不得"纠正"（见下方目标态断言）。
--   ② 新表 custom_skill_combo_rules：16 行静态连击索引规则（自 skills.cpp
--      combo_arg_indices 13 case + combo_fixed_range 3 case 常量旁路迁移）。内核只留
--      一条通用规则（查行→校验下标→max(1,参数)→排序写 combo_min/max），不含任何效果
--      id 或 {5,10} 常量；链上顺序与「后一个连击节点覆盖前一个」行为保持不变。
--   ③ 直通红利：行参化后调试台 effectParams 覆盖能真正改写 160x 连击区间
--      （常量旁路时代 160x 无参数可覆盖——选此形态的核心理由）。
--
-- 切片安全性：160x arg_count 0→2，参数追加在平铺数组**尾部**（[100,1,5,10]），
--   build_skill_effects 按链序逐 id 用 arg_count 切片——链兄弟 451 的前序切片 [100,1]
--   不受影响；160x 切走尾部两参 [5,10]。13 既有规则的 arg_count 与切片一字未动。
--
-- 重放/重建：整脚本单事务（BEGIN…COMMIT）+ .bail on；断言表 CHECK(ok=1)——
--   「完整旧态」与「完整目标态」皆可重放，第三态 CHECK 违反 → CLI 非零退出且事务
--   不提交回滚（R4 §2.3：只接受完整旧态或完整目标态）。管线重建（fetch→import→
--   apply_all.sh）后由本补丁恢复全部改动。
--
-- 回滚（手动；重放管线后需再跑本补丁恢复数据化态）：
--   UPDATE side_effect SET arg_count=0, raw_json='{"id": 1608, "side_effect_arg": "没有参数", "side_effect_argcount": 0}'
--     WHERE id IN (1608,1609,1610) AND arg_count=2;
--   UPDATE moves SET side_effect_arg='[100, 1]' WHERE id IN (34454,34457,34460)
--     AND (SELECT group_concat(j.value,',') FROM json_each(moves.side_effect_arg) j)='100,1,5,10';
--   DROP TABLE IF EXISTS custom_skill_combo_rules;

.bail on
BEGIN TRANSACTION;

-- ═══ 前置断言：只接受「完整旧态」或「完整目标态」，第三态报错 ═════════════
CREATE TEMP TABLE IF NOT EXISTS j4_combo_assert (ok INTEGER CHECK (ok = 1));
DELETE FROM j4_combo_assert;
INSERT INTO j4_combo_assert (ok)
SELECT CASE WHEN (
    -- 定义行三兄弟：arg_count 只能是 0（旧）或 2（目标）
    (SELECT COUNT(*) FROM side_effect WHERE id IN (1608,1609,1610)
       AND (arg_count IS NULL OR arg_count NOT IN (0,2))) = 0
    -- 三载体链必须精确 [451,160x]（json_each 成员精确匹配，不做子串猜 id）
    AND (SELECT COUNT(*) FROM moves m WHERE m.id IN (34454,34457,34460)
       AND (SELECT COUNT(*) FROM json_each(m.side_effect)) = 2
       AND EXISTS(SELECT 1 FROM json_each(m.side_effect) j WHERE j.value = 451)
       AND EXISTS(SELECT 1 FROM json_each(m.side_effect) j WHERE j.value IN (1608,1609,1610))) = 3
    -- 三载体平铺参数只能是 [100,1]（旧）或 [100,1,5,10]（目标）
    AND (SELECT COUNT(*) FROM moves m WHERE m.id IN (34454,34457,34460)
       AND (SELECT group_concat(j.value, ',') FROM json_each(m.side_effect_arg) j)
           NOT IN ('100,1', '100,1,5,10')) = 0
) THEN 1 ELSE 0 END;
-- （规则表完整性不在前置断言：新表在本补丁事务内创建，SQLite prepare 期就会解析
--   全语句引用，旧态「表不存在」时引用即报错。规则表「16 行精确」由下方目标态
--   断言在 COMMIT 前兜底——缺行/坏行/被手改的第三态同样过不了事务。）

-- ═══ ① 定义行规范化：arg_count 0→2 ＋ raw_json 本地来源说明（幂等）═══════
UPDATE side_effect SET
    arg_count = 2,
    raw_json = json_set(raw_json, '$.side_effect_argcount', 2,
                        '$.note', 'J326 本地规范化：连击区间 5-10 写入行参（官方原文「没有参数」；custom_skill_combo_rules 同步声明下标 (0,1)；effect_info.args_num=0 原文保留——两表 0/2 差异是补丁白名单验收项，勿纠正）')
WHERE id IN (1608,1609,1610) AND arg_count = 0;

-- ═══ ① 三载体平铺参数：[100,1] → [100,1,5,10]（只认旧值，重放安全）═══════
UPDATE moves SET side_effect_arg = json_set(side_effect_arg, '$[#]', 5, '$[#]', 10)
WHERE id IN (34454,34457,34460)
  AND (SELECT group_concat(j.value, ',') FROM json_each(moves.side_effect_arg) j) = '100,1';

-- ═══ ② 规则表 ＋ 16 行种子（INSERT OR IGNORE 幂等；完整性靠断言兜底）═════
CREATE TABLE IF NOT EXISTS custom_skill_combo_rules (
    effect_id      INTEGER PRIMARY KEY,                            -- 技能空间效果号（非精灵号/魂印号）
    min_arg_index  INTEGER NOT NULL CHECK (min_arg_index >= 0),    -- SkillEffectRecord.args 零基下标（不含 owner/target +2 前缀）
    max_arg_index  INTEGER NOT NULL CHECK (max_arg_index >= min_arg_index),
    schema_version INTEGER NOT NULL CHECK (schema_version >= 1),   -- loader 拒绝未知版本
    evidence       TEXT NOT NULL CHECK (length(evidence) > 0)      -- 来源/原内核 case/本地规范化理由
);

INSERT OR IGNORE INTO custom_skill_combo_rules
    (effect_id, min_arg_index, max_arg_index, schema_version, evidence)
WITH seed(effect_id, lo, hi, ev) AS (
    VALUES
    -- ── 13 条既有参数下标规则（J326 自 skills.cpp combo_arg_indices 迁移，行为等价）──
    (31,   0, 1, '经典连击族 {0}~{1}：乱突/疯狂乱抓/花瓣舞，303 载体 2026-09-17 逐个核对（原 combo_arg_indices case 31）'),
    (1172, 1, 2, '{0}回合做{1}~{2}次攻击（原 case 1172）'),
    (1577, 1, 2, '{0}回合做{1}-{2}次攻击，当前技能PP值小于{3}时连击上限为{4}（上限修正句未实现，区间照取；原 case 1577）'),
    (1627, 1, 2, '{0}回合做{1}-{2}次攻击，达上限必定秒杀（秒杀子句未实现，区间照取；原 case 1627）'),
    (1141, 0, 1, '1回合做{0}~{1}次攻击（原 case 1141）'),
    (1454, 0, 1, '1回合做{0}-{1}次攻击＋{2}%附加{3}点固定伤害（原 case 1454）'),
    (1455, 0, 1, '1回合做{0}-{1}次攻击，达上限秒杀（原 case 1455）'),
    (1500, 0, 1, '1回合做{0}-{1}次攻击＋护盾状态连击上限句（上限修正未实现，区间照取；原 case 1500）'),
    (1546, 0, 1, '1回合做{0}-{1}次攻击＋蓄力上限修正句（同上；原 case 1546）'),
    (1593, 0, 1, '1回合做{0}-{1}次攻击＋体力上限修正句（同上；原 case 1593）'),
    (1666, 0, 1, '1回合做{0}-{1}次攻击＋领域上限修正句（同上；原 case 1666）'),
    (1685, 0, 1, '1回合做{0}-{1}次攻击＋异常上限修正句（同上；原 case 1685）'),
    (1732, 0, 1, '激发古渊灵鱼全部的力量，1回合做{0}-{1}次攻击（原 case 1732）'),
    -- ── 3 条本地规范化行参（区间来自官方文案 5-10，行参为本地写入）──
    (1608, 0, 1, '布布鸟 5-10 次攻击：文案常量→显式行参（载体 34454 [100,1,5,10]；原 combo_fixed_range 常量旁路）'),
    (1609, 0, 1, '布布犬 5-10 次攻击：文案常量→显式行参（载体 34457；原 combo_fixed_range 常量旁路）'),
    (1610, 0, 1, '布布熊 5-10 次攻击：文案常量→显式行参（载体 34460；原 combo_fixed_range 常量旁路）')
)
SELECT effect_id, lo, hi, 1, ev FROM seed;

-- ═══ 目标态断言：16 行精确 ＋ 白名单验收项（双表 0/2 差异）═══════════════
DELETE FROM j4_combo_assert;
INSERT INTO j4_combo_assert (ok)
SELECT CASE WHEN (
    (SELECT COUNT(*) FROM custom_skill_combo_rules) = 16
    AND (SELECT COUNT(*) FROM custom_skill_combo_rules
         WHERE effect_id IN (1172,1577,1627)
           AND min_arg_index = 1 AND max_arg_index = 2 AND schema_version = 1
           AND length(evidence) > 0) = 3
    AND (SELECT COUNT(*) FROM custom_skill_combo_rules
         WHERE effect_id IN (31,1141,1454,1455,1500,1546,1593,1666,1685,1732)
           AND min_arg_index = 0 AND max_arg_index = 1 AND schema_version = 1
           AND length(evidence) > 0) = 10
    AND (SELECT COUNT(*) FROM custom_skill_combo_rules
         WHERE effect_id IN (1608,1609,1610)
           AND min_arg_index = 0 AND max_arg_index = 1 AND schema_version = 1
           AND length(evidence) > 0) = 3
    AND (SELECT COUNT(*) FROM side_effect WHERE id IN (1608,1609,1610)
           AND arg_count = 2) = 3
    -- 白名单验收项：side_effect.arg_count=2 与 effect_info.args_num=0 的既定差异
    AND (SELECT COUNT(*) FROM effect_info WHERE id IN (1608,1609,1610)
           AND args_num = 0) = 3
    AND (SELECT COUNT(*) FROM moves WHERE id IN (34454,34457,34460)
         AND (SELECT group_concat(j.value, ',') FROM json_each(moves.side_effect_arg) j)
             = '100,1,5,10') = 3
    -- raw_json 本地来源说明已写入（note 字段存在）
    AND (SELECT COUNT(*) FROM side_effect WHERE id IN (1608,1609,1610)
         AND json_extract(raw_json, '$.side_effect_argcount') = 2
         AND json_extract(raw_json, '$.note') LIKE 'J326%') = 3
) THEN 1 ELSE 0 END;

-- ═══ verify 输出 ════════════════════════════════════════════════════════
SELECT '[verify] side_effect 160x: id=' || id || ' arg_count=' || arg_count
FROM side_effect WHERE id IN (1608,1609,1610) ORDER BY id;
SELECT '[verify] 载体链+参数: ' || id || ' ' || side_effect || ' | ' || side_effect_arg
FROM moves WHERE id IN (34454,34457,34460) ORDER BY id;
SELECT '[verify] 规则表: ' || COUNT(*) || ' 行（预期 16）' FROM custom_skill_combo_rules;
SELECT '[verify] 规则行: ' || group_concat(rule, ' ')
FROM (SELECT effect_id || ':' || min_arg_index || '-' || max_arg_index AS rule
      FROM custom_skill_combo_rules ORDER BY effect_id);

COMMIT;
