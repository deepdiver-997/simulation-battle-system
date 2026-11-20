-- J325 · 1526 数据刀拆分：后缀子句迁私有技能效果新 id 900001（2026-11-05）
-- 来源：J325 看板单（用户拍板「数据刀拆分」——数据自描述优于内核兜底，与 J322 哲学同源；
--       内核+db_patch 双授权 = a24-orch 担保延续，本单为内核+db 唯一授权 agent）。
--
-- 语义：1526「随机附加{0}-{1}点固定伤害，技能无效时减少对手所有技能PP1点」是**混合模板**
--   （主句命中趟固伤 + 后缀无效趟减 PP）。J321 期靠内核 case 1526 → INHERENT 双趟桶 +
--   插件读 ws.skill_exec_result 判趟别（可用但绕）。本单把后缀迁到私有 id **900001**
--   （技能空间私有段约定 900000+ 起始，本地自造不经官方管线），1526 回归纯主句——
--   内核 default_branch_for_effect 的 case 1526 随之回退默认 HIT，趟别判断整体摘除。
--
-- arg 分配安全性：900001 arg_count=0 不消耗参数。build_skill_effects
--   （official_data_repository.cpp）按 side_effect 链序逐 id 用 arg_count 切
--   moves.side_effect_arg 平铺数组——链尾追加 arg_count=0 的 id 不改变前序切分，
--   1526 的 [lo,hi] 两参定位不变（8 载体逐一核对见下表）。
--
-- 8 载体清单（side_effect LIKE '%1526%' 现查 2026-11-05，8/8）：
--   26986 菌丝体期  [1526]           → [1526, 900001]
--   35732 智眼解析  [1526]           → [1526, 900001]
--   35902 花心向阳  [1670, 1526]     → [1670, 1526, 900001]
--   36288 乐善好施  [1526, 1861]     → [1526, 1861, 900001]
--   36306 当当当    [494, 1526]      → [494, 1526, 900001]
--   37802 圣爪在上  [785, 39, 1526]  → [785, 39, 1526, 900001]
--   37851 螺旋脉冲  [1526]           → [1526, 900001]
--   38057 碎叶击打  [1526]           → [1526, 900001]
--
-- 私有段约定（首例记档，后续「本地语义效果」照此办理）：id 段 900000+ 仅供
--   side_effect 表本地 INSERT + moves 链本地引用 + moves_lib SKILL_EFFECT 注册；
--   effect_info 表**不建行**（官方权威不动，fill_effect_metadata 查无行安全返回）。
--   撞号审计由 SKILL_EFFECT 宏自带 source（include/plugin/id_collision.h）。
--
-- 回滚：
--   UPDATE moves SET side_effect = replace(side_effect, ', 900001', '') WHERE id IN
--     (26986,35732,35902,36288,36306,37802,37851,38057);
--   DELETE FROM side_effect WHERE id = 900001;

INSERT INTO side_effect (id, arg_count, raw_json)
SELECT 900001, 0,
       '{"id": 900001, "side_effect_arg": "", "side_effect_argcount": 0, "note": "J325 私有拆分：技能无效时减少对手所有技能PP1点（原1526后缀子句）"}'
WHERE NOT EXISTS (SELECT 1 FROM side_effect WHERE id = 900001);

-- 8 载体 moves.side_effect JSON 数组链尾追加（幂等：NOT LIKE 防重放二次 append；
-- json_set '$[#]' = 数组尾部追加，重放管线 import 重建后由 apply_all.sh 恢复）。
UPDATE moves SET side_effect = json_set(side_effect, '$[#]', 900001)
WHERE id IN (26986, 35732, 35902, 36288, 36306, 37802, 37851, 38057)
  AND side_effect LIKE '%1526%'
  AND side_effect NOT LIKE '%900001%';

SELECT '[verify] side_effect 900001: id=' || id || ' arg_count=' || arg_count
FROM side_effect WHERE id = 900001;
SELECT '[verify] 载体链: ' || id || ' ' || side_effect
FROM moves
WHERE id IN (26986, 35732, 35902, 36288, 36306, 37802, 37851, 38057)
ORDER BY id;
