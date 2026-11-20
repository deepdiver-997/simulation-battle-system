# 数据管线迁移到 Unity 端 — 交接文档

整理时间：2026-08-09

> 本会话把数据源从已停更的 H5 迁移到了 Unity 端，并确认了工具链可用。
> 数据管线的"拉取 + 解码"已跑通；**SQLite 导入重构 + C++ repository 适配** 交给下一个会话。
> 本文是给下个会话的完整交接。

---

## 一、背景：H5 已停更，数据源迁移到 Unity

赛尔号已全面转 Unity 端，**H5 (seerh5.61.com) 的配置数据不再更新**（本地 version.json 停留在 1757008039424）。

新数据源：**newseer.61.com（Unity 客户端）**，配置在一个 Unity asset bundle 里：

- `https://newseer.61.com/Assets/StandaloneWindows64/ConfigPackage/pgame_configs_bytes.bundle`（12MB）
- 版本 `20260807153941`（2026-08-07，当前）
- 内含 664 个 `.bytes` 二进制配置（游戏自定义序列化，非 JSON/protobuf）

**社区镜像**（每 30 分钟自动从官方提取并更新）：

- 仓库：`https://github.com/SeerAPI/seer-unity-assets`（已提取的 `.bytes`，位于 `newseer/assets/game/configs/bytes/`）
- 提取工作流：`https://github.com/SeerAPI/data-update-workflows`

## 二、工具链（已确认可用）

| 工具 | 作用 | 安装 |
|---|---|---|
| `seerapi-solaris` | 解码 `.bytes` → JSON（312 个解析器，1.4s 全量） | `pip install seerapi-solaris` |
| `albi0` | 拉官方 bundle + 提取 `.bytes`（data-update-workflows 用的） | `pip install albi0` |

solaris 用法：
```python
from solaris import parse
parsers = parse.import_parser_classes()          # 312 个解析器
with parse.change_workdir(source_dir):            # load_source_config 按 CWD 读文件
    data = p.load_source_config()
    parsed = p.parse(data)
with parse.change_workdir(output_dir):
    cls().save_parsed_config(parsed)             # 输出 JSON
```
注意：`source_config_filename` 是**方法**要调用；`load_source_config` 依赖当前工作目录。

## 三、本会话已完成

- ✅ **H5 源归档**：`fetch_seer_official_json.py` / `import_seer_official_sqlite.py` / `update_seer_official_data.py` 头部加了 LEGACY 标记；旧快照 `scripts/data/raw/official/` + `seer_official.sqlite` 保留归档。
- ✅ **新拉取脚本** `scripts/fetch_seer_unity_bytes.py`：从 SeerAPI 镜像下载战斗相关 `.bytes` → `scripts/data/raw/unity_bytes/`（18 个文件已验证）。
- ✅ **新解码脚本** `scripts/decode_seer_bytes.py`：用 seerapi-solaris 解码 → `scripts/data/raw/unity_json/`（17 个 JSON 已验证）。
- ⚠️ `scripts/import_seer_unity_sqlite.py`：**草稿，方向错误**——硬套旧 H5 schema（side_effect 转空格串等）。**需推倒，按官方结构重写**（见下）。

数据位置：
- 原始 bytes：`scripts/data/raw/unity_bytes/*.bytes`
- 解码 JSON：`scripts/data/raw/unity_json/*.json`

## 四、import 重构 + C++ repo 适配（✅ 已完成 2026-08-09）

### 原则（本会话定的设计方向）

**新库以官方 Unity 结构为准，C++ 引擎去适配新结构**，而不是让新数据迁就旧 H5 schema。

已完成：
- ✅ `import_seer_unity_sqlite.py` **重写**：以官方结构建 18 表，数组字段（side_effect/param/en/att/pet_id/kind…）一律存 **JSON 数组文本**（不做空格拼接），learnable_moves 拆 **join 表 `monster_learnable_moves`**，types_relation 存 676 行克制表。行数：moves 27181 / monsters 6485 / monster_learnable_moves 81460 / skill_types 138 / side_effect 2023 / effect_info 2340 / types_relation 676 / new_se 400 / effect_icon 2125 / buff 3483 / hide_moves 2232 / boss_effect_icon 86。
- ✅ **C++ repository 适配**（`official_data_repository.cpp/.h`）：monster json_extract 全改小写（`$.def_name/$.type/$.hp/...`）；side_effect 用 `parse_json_int_array` 解析 JSON 数组；learnable_moves 改查 join 表；new_se 查询适配新 schema（去掉 can_reset）；`load_skill` 移除 cd 列。
- ✅ **克制表接入**：`ElementalAttributes::loadElementalAttributes()` 从 `types_relation` + `skill_types` 填充 256×256 矩阵（2.0→2 克制 / 1.0→1 / 0.5→0 微弱 / 0.0→0 免疫）；`PetFactory::initialize_runtime_data()` 中调用。
- ✅ **默认库已切**：`kDefaultOfficialDatabasePath = scripts/data/processed/seer_unity.sqlite`。smoke test + 探针断言全过（含 塞德 type=41 分解为 ground+fight）。
- ⚠️ **新发现**：新 Unity 双属性精灵 **type2 全 null**，`type` 用**合并 id**（21-220，最大 226；如 41=战斗地面）。C++ `load_monster` 已通过 skill_types.en 分解为两个单属性 id。克制矩阵下标需 ≥227（现取 256）。

### 官方 JSON 结构速查（solaris 解码产物，已核实）

| 文件 | 顶层结构 | 关键字段 |
|---|---|---|
| `moves.json` | `root.moves.move[]` | `id,name,type,category,power,accuracy,priority,max_pp,must_hit,atk_type,crit_rate,mon_id,side_effect[](数组!),side_effect_arg[](数组!),info,ordinary,friend_side_effect` |
| `monsters.json` | `monsters[]` | `id,def_name,type,type2,gender,hp,atk,def,sp_atk,sp_def,spd,learnable_moves.move[{id,learning_lv}],real_id`（字段小写；**无 add_se**） |
| `skillType.json` | list | `id,cn,en[](数组),att[](数组),is_dou` |
| `side_effect.json` | `side_effects[]` | `id,side_effect_arg,side_effect_argcount` |
| `effectInfo.json` | list | `id,args_num,info,param[](数组),analyze,type,key` |
| `battleEffects.json` | `battle_effects[]` | `type,name?,sub_effect[{id,name,efftype,ctrl,...}]` |
| `new_se.json` | `NewSe[]` | `Idx,Stat,Eid,Args,Des,Desc,Intro,StarLevel,ItemId`（描述在 Desc，Des 常空）。**`Stat=1` 即通用特性**（50 种 × 等级 0-5，`Desc`=特性名、`Intro`=描述、`Args`=参数、`Eid`=效果id）→ `CommonTrait`（`ElfPet::commonTrait`）；`Stat=2` 是属性加成，非魂印 |
| `typesRelation.json` | `root.relation[]` | `type(进攻方),opponent[{type,multiple}]`（克制表，H5 缺失的新数据！） |
| `effectIcon.json` | list | `id,pet_id(JSON数组!),icon_id,effect_id,kind,tips,des,...`（**魂印/被动的真实关联**：`pet_id`→精灵、`tips`=描述、`effect_id`+`kind`+`args`=效果，1857/2125 行 effect_id 映射到 effect_info） |
| `effectDes.json` | list | `id,kind,kinddes,desc` |
| `buff.json` | `data[]` | `id,desc,tag` |
| `hideMoves.json` / `spHideMoves.json` | — | 隐藏技能名映射 |

### 数据量对比（新 vs 旧）

| 表 | 旧 H5 | 新 Unity |
|---|---|---|
| moves | 26467 | **27181**（含更新精灵） |
| monsters | 6248 | **6485** |
| effect_info | 2115 | **2340** |
| types_relation | 无 | **新增**（克制表） |

### 需要 C++ 改的点（`include/db/official_data_repository.h` / `.cpp`）

1. **`load_monster` 的 json_extract 路径**要改小写：
   - `$.DefName`→`$.def_name`，`$.Type`→`$.type`，`$.Type2`→`$.type2`，`$.Gender`→`$.gender`
   - `$.HP`→`$.hp`，`$.Atk`→`$.atk`，`$.Def`→`$.def`，`$.SpAtk`→`$.sp_atk`，`$.SpDef`→`$.sp_def`，`$.Spd`→`$.spd`
   - `$.LearnableMoves.Move`→`$.learnable_moves.move`
   - **`$.AddSeParam`（魂印）在新结构没有**——新数据里魂印关联在别处（`add_se` / `new_se` / `effect_icon`），需重新查清关联方式
2. **`build_skill_effects` / `load_skill`**：如果 moves 的 `side_effect` 存 JSON 数组（而非空格串），解析方式要改。若新 schema 保留数组列，C++ 用 `json_extract` 或改存 join 表后 JOIN 查询
3. **克制表接入**：`Calculation::calculateDamage` 现在用硬编码的 `ElementalAttributes::elementalAttributesRestraints`（[include/entities/elemental-attributes.cpp](include/entities/elemental-attributes.cpp)）。`types_relation` 表是新数据源，建议把克制表数据化，C++ 从库查
4. **新库路径**：建议 `scripts/data/processed/seer_unity.sqlite`，`kDefaultOfficialDatabasePath` 指向它（或先并行验证再切换）

### 已定的设计决策（2026-08-09 落地）

1. **side_effect 表示** → moves 表存 **JSON 数组文本**（官方结构，SQLite JSON1 可直接操作），C++ 用 `parse_json_int_array` 轻量解析。
2. **learnable_moves** → 拆 **join 表 `monster_learnable_moves(monster_id, move_id, learning_lv, rec, tag)`**，C++ 直接 JOIN。
3. **schema 表名** → 同名概念沿用旧表名（moves/monsters/skill_types/side_effect/effect_info/battle_effects/new_se/effect_icon/effect_des/buff/hide_moves/boss_effect_icon/effect_buff/sp_hide_moves/sp_hide_moves_bisaifu），新增 `types_relation`/`monster_learnable_moves`/`item_type`。旧 `effect_buff` 表名沿用。
4. **solaris/albi0 依赖** → 解码脚本头部已注明 `pip install seerapi-solaris`；import 脚本仅用标准库，无需额外依赖。

### 已知 Gap（2026-08-09 已部分查清）

- **魂印关联 — 已查清（2026-08-09）**：真正的魂印表是 **`effect_icon`**（不是 `new_se`）。
  - `effect_icon.pet_id`（JSON 数组，2232/2233 命中真实怪物 id）＝精灵关联；`tips`＝完整人读魂印描述；`effect_id`+`kind`+`args`＝效果数据（1857/2125 行 effect_id 可映射到 `effect_info`）。
  - **`new_se` 是另一回事**：idx 1001+ 是"增加精灵防御20（赛尔间对战无效）"之类的属性加成/性格表（Stat/Eid/Args 结构），不是魂印。旧 H5 `AddSeParam` 只是 0/1 标志位（2167/6248），旧 `load_soul_mark(1)` 查 new_se.idx=1 本来就查不到 —— 旧引擎魂印关联也是断的。
  - 覆盖：1988/6485 精灵有 effect_icon（1764 只 1 行、207 只 2 行）。多行按 effect_id 区分（单行多为魂印，多行含特殊皮肤被动）。
  - **待做**：C++ `load_soul_mark`/`soul_mark_id` 改为从 `effect_icon` 走（或新增 pet→effect_icon join 查询），soul_lib 注册的 1001-1005 只是草案示例，与真实 effect_id 空间不对齐。
- **sp_hide_moves 双数组**：`config.show_moves[]` + `config.sp_moves[]` 合表存 `kind` 列（'show'/'sp'），旧代码不消费，仅入库。
- **effect_info.param_type — 已过时（2026-08-09）**：新 Unity 数据 `effectInfo.json` 根结构为 `root.effect[]`，字段是 `analyze/info/param/args_num/id/key/type`，**无 param_type**（旧 H5 才有）。此条从待做移除。

## 五、后续系统方向（数据就绪后）

1. **数据管线收尾**：✅ import 按官方结构 + C++ repo 适配 + 克制表接入已完；新精灵（薇尔诗等）验证可做。
2. **DamagePhase 管线继续**（已搭 `include/effects/damage_pipeline.h`）：✅ `applyDamageReduction` 已迁入 REDUCE 阶段（`install_default_damage_reduction()`，stage_simple_attack_damage 只算 base）；待办：阶段序按游戏知识校准（增伤/减伤/锁伤的精确相对位置）。
3. **萨瑞卡式**（受高伤检测归零 + 回血）：DETECT 阶段读 `resolvedDamage.final` 置 0，基础设施已有（damage_pipeline 测试覆盖）。
4. **沧岚式**（使对手挡伤失效）：`ElfPet::damage_suppress_mask |= BLOCK|DETECT`（BLOCK=挡伤归零族、DETECT=受高伤触发族；**减伤 MITIGATE 不在其列**——官方"减伤不可被无视"）。基础设施已通：`soul_lib` 的挡伤 helper 已收敛到读这个掩码，4泪（蚀砚之泪）达阈值由 `sync_eternal_tear_suppress_mask` 置位；场景 031 Q4/Q5 验证。
5. **薇尔诗式**（双方场下精灵无法影响场上）：需要**被动效果域**的全局抑制——被动常驻系统 + "效果来源在场下/场上"标签。当前引擎无被动系统，是下一块地基。
6. **被动/背包效果系统**：薇尔诗（场下抑制）、背包魂印（"自身在背包时XXX"）、常驻被动——这些需要一个统一的被动效果注册/抑制系统。

## 六、给下会话的一句话

> H5 已死，新源是 newseer.61.com 的 Unity 配置（经 SeerAPI 镜像 + seerapi-solaris 解码，已验证可用）。
> import 重构 + C++ repo 适配 + 克制表接入 **已全部完成**（默认库已切 seer_unity.sqlite，smoke test 通过）。
> **魂印关联已查清（2026-08-09）**：魂印表是 `effect_icon`（pet_id 关联精灵 + tips 描述 + effect_id/kind/args），`new_se` 是属性加成表、旧 AddSeParam 只是 0/1 标志 —— 旧引擎魂印关联本身就是断的。
> **通用特性已接入（2026-08-09）**：`new_se` 表 `stat=1` 即通用特性（50 种 × 等级 0-5 = 300 行）→ `CommonTrait`（`ElfPet::commonTrait`）+ `load_common_trait(idx)` + `BattlePetMessage.common_trait_id`。协议 pet 报文 12→13 int。
> 下会话：① 新精灵（薇尔诗等）验证 ✅ 数据就绪（4000 有 16 技能含必中，可加载）→ ② C++ `load_soul_mark` 改走 effect_icon（替换 new_se 查询 + soul_lib 1001-1005 草案 id）→ ③ 继续 DamagePhase（applyDamageReduction 迁入 REDUCE）+ 被动系统。
