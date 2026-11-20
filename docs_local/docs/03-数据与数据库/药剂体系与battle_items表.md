# 药剂体系与 battle_items 表（2026-09-22 药剂线）

## 一句话

战斗内嗑药从"七槽占位枚举 + 硬编码数值"改为**官方 battle_items 表驱动**（item_id → 效果字段），
回血不走恢复原语、查 RuleCenter 药剂反噬票（预留位），解异常/解能力下降直改槽位不发 event。

## 数据来源与入库

- 官方 `Battleitem_*.json`（`data/raw/official/` 快照，hash 文件名 → glob）= **战斗内可用物品表**
  （91 条：胶囊抓捕族 + 药剂族）。效果即官方字段，无独立效果表。
- 入库管线：`scripts/import_seer_unity_sqlite.py`（SCHEMA + `import_battle_items` importer，
  已进 IMPORTERS，随官方 import 重刷）。
- **不入库**（2026-09-22 拍板）：蛋糕块×3 / 完全净化药剂（在 itemType 分类但不在 Battleitem 表，
  疑似下架）；精灵王 4 药剂（300758~761，效果字段全空、纯文字、场景限定）→ 引擎不做；
  持续恢复类（持续恢复/长效体力，需回合性 HoT）→ 暂不做。
- 官方数据瑕疵：巅峰/极限活力药剂 `ItemType=1`（体力类）但效果字段是 PP —— **以效果字段为准**，
  `item_type` 列只透出不参与判定。

## 表结构（battle_items）

| 列 | 含义 |
|----|------|
| item_id / name | 主键 / 物品名 |
| hp / pp | 回复体力量 / 回复技能使用次数（NULL = 无此效果，**不能 COALESCE 成 0**） |
| remove_mon_stat | 解除指定异常状态（状态码 = `AbnormalStatusId`，如 2=烧伤） |
| remove_all_mon_stat | 1 = 解除自身全部异常 |
| remove_bt_lv_down | 1 = 解除自身能力下降 |
| bonus / max_count / item_type | 捕捉加成 / 背包上限 / 官方分类（引擎不用，原样保留） |

常用 id：300011 初级体力药剂(+20) 300016 初级活力药剂(+5PP) 300614 状态解除药剂-烧伤
300701 全能恢复(+150HP+3PP) 300749 修的神奇药水(+170HP+解全异常) 300750 龙的振奋药水(+5PP+解能力下降)。

## 引擎结算口径（SeerRobot::use_medicine，2026-09-22 用户拍板）

1. **效果查表**：`OfficialDataStore::load_battle_item(item_id)`；库存 0 / 表无此 id → 拒绝（不扣数）。
2. **回血不走恢复原语**：
   - 不受封回血（HEAL_BLOCK）影响——嗑药是玩家操作不是效果；
   - 但要查 **RuleCenter 药剂反噬票（`RuleCategory::POTION_BACKLASH`）**：命中则
     "回复 x"反转为"扣 x"（钳 0 可致死；嗑药不是攻击，不触发击败时点效果）。
   - ⚠️ 反噬票是**预留位，当前无任何注册者**（授予/撤销入口
     `BattleContext::grant_potion_backlash / revoke_potion_backlash / has_potion_backlash`）。
3. **钳上限**：回血钳 `numericalBase[HP]`，回 PP 钳各技能 `maxPP`，不能超。
4. **解异常 / 解能力下降直改槽位、不发 event**（不是正常结束异常流程）：
   全解 = `abnormal_status_end_round[side].fill(0)`；单解 = 对应槽清 0；
   解能力下降 = 负等级清零 + `ws.view_levels` 视图同步。

## 库存模型与协议

- `SeerRobot::medicines` = `MedicineStock`（`std::map<item_id, count>`，item_id 升序稳定序）。
  旧 `MedicineType` 七槽枚举 / `MEDICINES_SIZE` 已删。
- `USE_MEDICINE` 的 `index` = 库存升序位次（`legal_actions` 原样透出，FSM 按位次解析 item_id）。
  ⚠️ 兼容旧口径：**结算失败（空库存/非法位次）也照记 roundChoice**——"嗑药跳过出手"
  是既有测法（090 crawford/tianqi、096 gaia 等），嗑不成同样算交了动作。
- `legal_actions` 药剂档字段：`index / itemId / name / count / usable`（旧 `type` 字段已废）；
  状态 JSON 里 `medicines` = `[{"id":..,"count":..}, ...]`（旧 10 槽数组已废）。

## 测试

- `scenario_115_medicine_system`：钳上限 / 封回血不拦 / 反噬反转 / 单品解异常 / 组合药剂 /
  解能力下降视图同步 / 库存与数据门 / 双回药剂。
- 回归注意：用"嗑药跳过出手"技巧的场景（068/069/074/076/090/091/096 等）库存写法已改为
  `MedicineStock{{300011, N}}`；满血嗑药钳 0 的断言语义不受数值占位变化影响。
