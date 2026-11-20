#ifndef OFFICIAL_DATA_REPOSITORY_H
#define OFFICIAL_DATA_REPOSITORY_H

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

struct sqlite3;

namespace official_data {

inline constexpr const char* kDefaultOfficialDatabasePath = "scripts/data/processed/seer_unity.sqlite";

struct SkillEffectRecord {
    int effect_id = -1;
    int arg_count = 0;
    std::vector<int> args;
    int effect_args_num = 0;
    std::string info;
    std::string param;
    std::string sp;
};

struct SkillRecord {
    int id = -1;
    std::string name;
    int type_id = 0;
    int category = 0;
    int power = 0;
    int accuracy = 0;
    int priority = 0;
    int max_pp = 0;
    int cd = 0;
    int must_hit = 0;   // 官方 MustHit：1 = 必中（无视命中率）
    // 官方 CritRate：**分母 16 的分子**（用户 2026-09-13 定口径）——crit_rate=8 → 8/16 = 50%，
    // =16 → 100%；**0 = 0/16 = 永不必暴**（绝大多数技能是 0：没有别的引爆效果就永远不暴击）。
    int crit_rate = 0;
    std::vector<SkillEffectRecord> effects;
};

// 认证数据层（custom_effect_programs / custom_effect_overrides 表）——非官方字段。
// 见 scripts/import_seer_unity_sqlite.py SCHEMA 里 custom_* 表的注释；随官方 import 重刷存活。
struct CustomProgramRecord {
    int effect_id = -1;
    int skill_id = -1;
    std::string kind;
    std::string name_zh;
    std::string unit_json;   // 有序 EffectUnit 序列(JSON)，模型离线编码，引擎直接加载
    std::string memo;
};

struct CustomOverrideRecord {
    int effect_id = -1;
    std::string override_type;  // interpret_as_program | map_to | ignore | dead_column
    std::string map_value;      // override_type 的载荷(目标effect_id / 模板 / 空)
    std::string source_scope;   // moves|effect_icon|hide_moves|...
    std::string rationale;      // 与官方差异的原因(存决策, 防下个会话当 bug 重查)
};

// 静态连击索引规则（custom_skill_combo_rules 表，J326 数据刀清缴线二批，2026-11-05）。
// 自 skills.cpp combo_arg_indices（13 case）+ combo_fixed_range（1608/1609/1610 常量
// 旁路 {5,10}）数据化迁移——内核不再含任何效果 id 开关表或 {5,10} 常量；种子见
// tools/db_patches/j4/1608_combo_rules.sql（160x 三行配套载体行参 [100,1,5,10]）。
struct ComboRuleRecord {
    int effect_id = -1;        // 技能空间效果号（非精灵号/魂印号）
    int min_arg_index = 0;     // SkillEffectRecord.args 的零基下标（不含 owner/target +2 前缀）
    int max_arg_index = 0;
    int schema_version = 0;    // loader 只接受 kComboRulesSchemaVersion
    std::string evidence;      // 来源/原内核 case/本地规范化理由（补丁 j4 种子）
};

// 连击规则表当前 schema 版本（J326 首版）。loader 对其它值显式失败（fail-closed，
// 不静默退单击）——未来加版本须同步 loader 支持列表与补丁 CHECK 约束。
inline constexpr int kComboRulesSchemaVersion = 1;

// 选择期路由（custom_skill_selection_routes 表，J327 数据刀清缴线二批，2026-11-06）。
// 自 skills.cpp loadSkills 两段 id 开关表（27 plain 条件先制/必暴/穿透预武装 +
// 微弱族 3 条）数据化迁移——迁「何时装槽、如何喂参」，不重写效果函数：条件判定
// 仍在插件运行时执行，on_selected 只装节点，FSM 侧 FIRST_MOVE_RIGHT 桶顺序零改动。
// 内核不再含任何选择期效果 id 开关；种子见 tools/db_patches/j4/selection_routes_29.sql。
struct SelectionRouteRecord {
    int effect_id = -1;        // 技能空间效果号（moves 命名空间；非精灵号/魂印号）
    std::string phase;         // 首版只认 "selection"
    std::string state;         // 语义名，首版只认 "FIRST_MOVE_RIGHT"（禁 C++ enum 整数）
    std::string arg_adapter;   // "plain"（沿用本效果参数切片）| "skill_static_elements"（追加静态系别双槽）
    int schema_version = 0;    // loader 只接受 kSelectionRoutesSchemaVersion
    std::string evidence;      // 来源/原内核行/插件文件（补丁 j4 种子）
};

// 选择期路由表当前 schema 版本（J327 首版）。loader 对其它值显式失败（fail-closed）。
// 首版故意只开 phase=selection × state=FIRST_MOVE_RIGHT：on_selected 实际只建这一个
// 桶（R4 §3.2）——DB 写别的 state 会形成"表面可配置、实际无效"的接口，直接拒绝。
inline constexpr int kSelectionRoutesSchemaVersion = 1;
inline constexpr const char* kSelectionRoutePhaseSelection = "selection";
inline constexpr const char* kSelectionRouteStateFirstMoveRight = "FIRST_MOVE_RIGHT";
inline constexpr const char* kSelectionRouteAdapterPlain = "plain";
inline constexpr const char* kSelectionRouteAdapterStaticElements = "skill_static_elements";

struct LearnableMoveRecord {
    int move_id = -1;
    int learning_level = 0;
};

// 神谕新技（sp_hide_moves 表，2026-09-24 精灵王线 K1）。
// 神谕波次精灵的专属新技不进学习表，按官方口径分两组：
//   fifth   = 第五技能候选（威力 ≥155 的 王· 系新技 / kind='sp' 行）
//   normal  = 1~4 号槽候选（其余：属性强化位 / 先制位 / 150 位）
// 空表 = 该精灵没有神谕形态（开关无意义）。
struct OracleMoves {
    std::vector<int> normal_moves;
    std::vector<int> fifth_moves;

    bool empty() const { return normal_moves.empty() && fifth_moves.empty(); }
    bool contains(int move_id) const {
        return std::find(normal_moves.begin(), normal_moves.end(), move_id) != normal_moves.end()
            || std::find(fifth_moves.begin(), fifth_moves.end(), move_id) != fifth_moves.end();
    }
};

struct MonsterRecord {
    int id = -1;
    std::string name;
    int type = 0;
    int secondary_type = 0;
    int soul_mark_id = 0;
    int gender = 2;
    int hp = 0;
    int atk = 0;
    int def = 0;
    int sp_atk = 0;
    int sp_def = 0;
    int spd = 0;
    std::vector<LearnableMoveRecord> learnable_moves;
    // 第五技能（hide_moves 表）。官方第五技能不走学习表，编队校验与 Skills
    // 构造的"属于这只精灵"判定都要算上它（槽位规则由 PetFactory 把守：仅第 5 槽）。
    // ⚠️ 神谕波次精灵（sp_hide_moves 有行）的新第五也混在本表里——神谕 OFF 时
    // 由 PetFactory 用 oracle_moves 过滤掉，本表保持 DB 原样。
    std::vector<int> hidden_moves;
    // 神谕新技（sp_hide_moves 表）。空 = 无神谕形态。
    OracleMoves oracle_moves;
};

struct SoulMarkRecord {
    int id = 0;
    int stat = 0;
    int effect_id = -1;
    std::vector<int> args;
    int can_reset = 0;
    std::string description;
    std::string intro;
    int star_level = 0;
};

// 通用特性（new_se 表 stat=1，50 种 × 等级 0-5）。每种特性是任何精灵都可配置的通用能力，
// 非怪物天生绑定，创建宠物时通过 BattlePetMessage.common_trait_id 指定。
struct CommonTraitRecord {
    int id = 0;           // idx 主键
    int stat = 1;
    int effect_id = -1;
    std::vector<int> args;
    std::string description;  // desc 列（特性名，如 瞬杀/精准/强袭）
    std::string intro;        // intro 列（人读描述）
    int star_level = 0;       // 等级 0-5
};

// 现代精灵魂印展示记录（effect_icon 表）。
// monsters.soul_mark_id 在 unity 库中全为 0（死列），魂印权威数据在 effect_icon：
// pet_id 是 JSON 数组字符串（须精确匹配，LIKE 会误中 14911 之类）；
// tips 是完整描述（含富文本标记），kind 是官方分类标签；
// effect_id 是引擎内部效果号，本库快照不可解析，原样保留。
// 一只精灵可能有多行（基础/强化版本），取 icon_id 最大者为当前版本。
struct SoulMarkDisplayRecord {
    int icon_id = 0;             // effect_icon.id 行主键
    int effect_id = 0;           // 引擎内部效果号（插件函数注册键）
    std::vector<int> kind_tags;  // kind 列（JSON 数组解析）
    std::vector<int> args;       // args 列（空格分隔）
    std::string tips;            // 原始富文本描述
    std::string tips_plain;      // 去富文本纯文本
    std::string come;            // 来源描述
    int monster_id = 0;
};

// effect_des 词典词条。kind=1 专属名词/状态词条（412 条）；kind=2 异常状态定义（46 条）；
// kind=3~5 机制/时点术语（"执行""回合开始时""免疫对手的攻击"等官方正式定义）。
struct TermRecord {
    int id = 0;
    int kind = 0;
    std::string name;         // kinddes 列
    std::string description;  // desc 列
};

// effect_info 效果模板（元数据分类器的输入）。
struct EffectTemplateRecord {
    int id = 0;
    int args_num = 0;
    std::string info;   // 模板文本（{0}{1} 占位符）
    std::string param;  // 参数类型规格（类型 id 数组，unity 库无图例表，原样保留）
};

// 装备部件（equip 表，equip.bytes 导入）。suit_id=0 = 无套装归属的散件。
// desc 逐件携带完整成套效果文本（官方每件重复同一段），人读校验用。
struct EquipRecord {
    int item_id = 0;
    std::string name;
    int quality = 0;
    int suit_id = 0;
    std::string desc;
};

// 套装（suit 表，suit.bytes 导入）。cloths = 部件 item_id 清单；
// 成套激活判定 = 穿戴件数 ≥ cloths 长度（官方无独立"需求件数"字段）。
struct SuitRecord {
    int id = 0;
    std::string name;
    std::vector<int> cloths;
    std::string suitdes;
};

// 单件装备数值加成行（custom_equip_stats，离线编码——Unity equip.bytes 无结构化数值）。
// stat_index：0=攻击 1=特攻 2=防御 3=特防 4=速度 5=体力；add_way：0=点数 1=百分比。
// scope：per_piece（每穿一件算一次）/ per_suit（成套后算一次，每件上重复编码）。
// target_monster：0 = 背包内所有精灵；>0 = 只有该精灵 id 受益（六界战甲定向条款）。
struct EquipStatRecord {
    int item_id = 0;
    int stat_index = 0;
    int amount = 0;
    int add_way = 0;
    std::string scope;
    int target_monster = 0;
    // 生效维度（2026-09-28 三视角线）：官方套装存在 pvp/pve 限定词（"仅限赛尔与赛尔间
    // 对战"/"赛尔与赛尔间对战无效"等）。1=该维度生效；三视角：base=两维都 1 的行、
    // pvp=base+pvp 行、pve=base+pve 行。
    int pvp = 1;
    int pve = 1;
};

// 性格（nature 表，nature.bytes 导入，培养线 2026-09-26）。官方 25 种（id 0-24），
// 五维 ±10% 乘数、无体力项（性格不影响体力）；id 20-24 中性（害羞/实干/坦率/浮躁/认真）。
// mult 下标 = NumericalPropertyIndex 序：0=攻 1=特攻 2=防 3=特防 4=速（无 HP 槽）。
struct NatureRecord {
    int id = 0;
    std::string name;
    double mult[5] = {1.0, 1.0, 1.0, 1.0, 1.0};
};

// 称号加成（title_stats 手工表，培养线 2026-09-28）。官方 Unity/H5 配置均无称号
// 属性表（configs 全目录仅 TitleBg 背景图）→ 手工表 + 工作值，游戏内核准后改表。
// 六维列即引擎序（nature 同款约定，无重排问题）；target_monster>0 = 专属称号。
struct TitleRecord {
    int id = 0;
    std::string name;
    int stats[6] = {0, 0, 0, 0, 0, 0};   // 引擎序：攻 特攻 防 特防 速 体
    int target_monster = 0;              // 0 = 通用；>0 = 仅该精灵可佩戴
    std::string memo;
};

// 刻印（mintmark 表，mintmark.bytes 导入，培养线 2026-09-26）。
// ⚠️ 官方六维序 = [攻,防,特攻,特防,速,体]（圣·虚无 effect_des 实测钉死），与引擎
// NumericalPropertyIndex 序 [攻,特攻,防,特防,速,体] 不同——load_mintmark 已重排，
// 本结构体的三个六维数组统一是**引擎序**（0=攻 1=特攻 2=防 3=特防 4=速 5=体）。
// type：0=属性刻印（stat_arg 固定加成）/ 1=技能刻印（绑定 move_ids，绝版）/
//       3=系列成长刻印（stat_base → stat_max）/ 4=碎片素材。
// 面板合成取 stat_max（强化满口径——竞技环境默认满强化，与天赋恒 31 同理）。
// ⚠️ stat_max 已并入官方"附加属性"extra_json（2026-09-28 对账修正）：强化满的实际
//    面板 = max + extra（圣战之锋α [55,25,0,25,30,80]+[5,3,0,3,2,2]=[60,28,0,28,32,82]，
//    与游戏内刻印加成及 seerinfo 刻印库逐一吻合）；全库 342 件 type3 有 extra。
struct MintmarkRecord {
    int id = 0;
    std::string name;
    std::string effect_des;
    int type = 0;
    int grade = 0;
    int quality = 0;
    int class_id = 0;
    std::array<int, 6> stat_arg{};
    std::array<int, 6> stat_base{};
    std::array<int, 6> stat_max{};
    std::vector<int> monster_ids;   // 非空 = 专属绑定（仅列出的精灵可装）
    bool hide = false;              // 官方未放出（合成按无此件处理）
};

// 战斗内物品（battle_items 表，2026-09-22 药剂线）。效果即官方 Battleitem 字段，
// 人读说明在 raw 抓取的 itemsTip（不入库）。
//   hp / pp                 = 回复体力量 / 回复技能使用次数（nullopt = 无此效果）
//   remove_mon_stat         = 解除指定异常状态（状态码口径 = AbnormalStatusId）
//   remove_all_mon_stat / remove_bt_lv_down = 1 解全部异常 / 解能力下降
//   bonus                   = 捕捉加成（胶囊族，战斗模拟不用，原样保留）
// ⚠️ 官方 ItemType 有错标（巅峰/极限活力药剂标 1 但效果是 PP）——**以效果字段为准**，
//    item_type 只透出不参与判定。精灵王 4 药剂效果字段全空（场景限定）→ 引擎不做。
struct BattleItemRecord {
    int item_id = 0;
    std::string name;
    int item_type = 0;
    std::optional<int> hp;
    std::optional<int> pp;
    std::optional<int> remove_mon_stat;
    int remove_all_mon_stat = 0;
    int remove_bt_lv_down = 0;
    double bonus = 0.0;
    int max_count = 0;
};

class OfficialDataRepository {
public:
    OfficialDataRepository() = default;
    OfficialDataRepository(const OfficialDataRepository&) = delete;
    OfficialDataRepository& operator=(const OfficialDataRepository&) = delete;
    ~OfficialDataRepository();

    bool open_read_only(const std::string& db_path = kDefaultOfficialDatabasePath);
    void close();

    bool is_open() const { return db_ != nullptr; }
    const std::string& db_path() const { return db_path_; }
    const std::string& last_error() const { return last_error_; }

    std::optional<SkillRecord> load_skill(int move_id) const;
    std::vector<SkillEffectRecord> load_skill_effects(int move_id) const;

    // ── 认证数据层（custom_* 表）────────────────────────────
    // 表空时返回 nullopt；skills.cpp loadSkills 在"注册函数→运行时 parser"之前查这两处以作离线纠偏。
    std::optional<CustomProgramRecord> load_custom_program(int effect_id, int skill_id) const;
    std::optional<CustomOverrideRecord> load_custom_override(int effect_id) const;

    // 认证数据层：全部 `override_type='window'` 声明（effect_id → "next_rounds"/"in_rounds"）。
    // 供 EffectMetaCatalog 一次性加载（判"N回合内" vs "下N回合"的窗口家族，见 EffectWindowKind）。
    std::vector<std::pair<int, std::string>> load_effect_window_overrides() const;

    // ── 静态连击索引规则（custom_skill_combo_rules，J326 2026-11-05）────
    // 授权来源：数据刀清缴线（用户批方向）+ R4 计划 §2（docs_local/docs/05-任务清单/
    // 数据刀清缴线二批计划.md）。发布绑定「新内核 + j4 补丁」：
    //   load_combo_rules() 全量装载并缓存（按 DB 连接生命周期；close()/open_read_only()
    //   清缓存）。返回 false = 数据错误（表缺失＝未打 j4 迁移 / schema_version 未知 /
    //   负下标或 lo>hi / evidence 空），last_error() 带因——loadSkills 收到 false 必须
    //   throw（fail-closed，不静默退单击）。
    //   find_combo_rule() 查缓存；未装载成功时恒返回 nullptr（调用方须先 load_combo_rules）。
    bool load_combo_rules() const;
    const ComboRuleRecord* find_combo_rule(int effect_id) const;

    // ── 选择期路由（custom_skill_selection_routes，J327 2026-11-06）────
    // 授权来源：数据刀清缴线（用户批方向）+ R4 计划 §3（docs_local/docs/05-任务清单/
    // 数据刀清缴线二批计划.md）。发布绑定「新内核 + j4 补丁」：
    //   load_selection_routes() 全量装载并缓存（按 DB 连接生命周期；close()/
    //   open_read_only() 清缓存）。返回 false = 数据错误（表缺失＝未打 j4 迁移 /
    //   空表＝种子被清或迁移半途 / schema_version 未知 / phase-state-adapter 越出
    //   首版白名单 / evidence 空），last_error() 带因——loadSkills 收到 false 必须
    //   throw（fail-closed，不静默回硬编码或把效果当普通 HIT 分支）。
    //   find_selection_route() 查缓存；未装载成功时恒返回 nullptr（调用方须先
    //   load_selection_routes）。无路由行的效果保持既有执行分支（缺行不是错误）。
    bool load_selection_routes() const;
    const SelectionRouteRecord* find_selection_route(int effect_id) const;

    std::optional<int> find_monster_id_by_exact_name(const std::string& monster_name) const;
    std::optional<MonsterRecord> load_monster(int monster_id) const;
    std::optional<MonsterRecord> load_monster_by_exact_name(const std::string& monster_name) const;
    std::vector<LearnableMoveRecord> load_monster_learnable_moves(int monster_id) const;
    // 第五技能（hide_moves 表）：官方第五技能不走普通学习表（实测 2240/2240 都不在
    // monster_learnable_moves），编队校验需单独放行。行序即库序（= 官方解锁序）。
    std::vector<int> load_monster_hidden_moves(int monster_id) const;
    // 神谕新技（sp_hide_moves，精灵王线 K1，2026-09-24）：按威力分第五/1~4 两组，
    // kind 忽略（'show'/'sp' 都能是第五——4186 的 王·荒合归心 是 'sp'，其余王是 'show'）。
    OracleMoves load_monster_oracle_moves(int monster_id) const;
    // 精灵王谓词（K5）：该精灵技能全集（学习表 ∪ hide_moves ∪ sp_hide_moves）里
    // 是否存在带 effect_id 的技能（官方判据 effect 760）。side_effect 是 JSON 数组
    // 文本，精确解析成员判定（LIKE 会误中 1760 之类）。
    bool pet_has_move_effect(int monster_id, int effect_id) const;
    std::optional<SoulMarkRecord> load_soul_mark(int soul_mark_id) const;
    std::optional<CommonTraitRecord> load_common_trait(int idx) const;

    // ── 装备/套装（equip / suit / custom_equip_stats 表）────────────────
    std::optional<EquipRecord> load_equip(int item_id) const;
    std::optional<SuitRecord> load_suit(int suit_id) const;
    // 某部件的数值加成行（custom_equip_stats 空表时返回空向量）。
    std::vector<EquipStatRecord> load_equip_stats(int item_id) const;

    // ── 培养（nature / mintmark 表，培养线 feat/peiyang）───────────────
    // 表未建（旧库）或 id 不存在 → nullopt。load_mintmark 对 hide=1 的行仍返回
    // （数据完整透出），"未放出不可装"由组装层判定。
    std::optional<NatureRecord> load_nature(int nature_id) const;
    std::optional<MintmarkRecord> load_mintmark(int item_id) const;
    std::optional<TitleRecord> load_title(int title_id) const;

    // ── 战斗内物品（battle_items 表，药剂线）──────────────────────────
    // 嗑药结算（SeerRobot::use_medicine）按 item_id 查效果；不在表中的 id（下架/
    // 非战斗物品）返回 nullopt，嗑药拒绝。
    std::optional<BattleItemRecord> load_battle_item(int item_id) const;

    // 现代精灵魂印：按精灵 id 从 effect_icon 精确匹配（pet_id JSON 数组），
    // 多行取 icon_id 最大。无记录返回 nullopt。
    std::optional<SoulMarkDisplayRecord> load_soul_mark_display_by_monster(int monster_id) const;
    // 该精灵的**全部**魂印版本行（effect_icon，按 icon_id 升序 = 官方版本序）。
    // 神谕波次精灵恰有两行：低 icon = 神谕前、高 icon = 神谕后（官方文本均完整），
    // 神谕开关据此选行（K1，2026-09-24）。
    std::vector<SoulMarkDisplayRecord> load_soul_mark_displays_by_monster(int monster_id) const;

    // effect_des 词典：名字精确匹配（失败退 LIKE），kind 过滤查询（2=异常状态词表等）。
    std::optional<TermRecord> load_term(const std::string& term_name) const;
    std::vector<TermRecord> load_terms_by_kind(int kind) const;

    // 全量 effect_info 模板（约 2340 条），供 EffectMetaCatalog 构建用。
    std::vector<EffectTemplateRecord> load_all_effect_templates() const;

    // 从 types_relation（官方克制表）填充克制矩阵。
    // matrix[attacker_type_id][defender_type_id] ∈ {0, 1, 2}（0=微弱/免疫, 1=普通, 2=克制）。
    bool load_elemental_restraints(std::vector<std::vector<double>>& matrix) const;

    // ── 复合属性映射层（J322，2026-11-05）──────────────────────────────
    // type_id → 组件单属性 id 对（单属性行 → {id,0}，复合行 → 分解结果）。
    // 全量载入：skill_types 138 行（26 单 + 112 复合）都在表里——克制聚合与本系
    // 判定消费这份映射（ ElementalAttributes::typeComponents 拷贝本表）。
    // 引用完整性未知项记 type_audit 账本（孤儿名/非法数组长度在 ensure 时记账）。
    const std::unordered_map<int, std::pair<int, int>>& type_components() const;

    // 启动审计：全库属性引用完整性（J322）。校验
    //   ① moves.type_id 全部 ∈ skill_types id 集（排除 0）
    //   ② monsters 属性（raw_json $.type）全部 ∈ id 集（排除 0）
    // 未知 id 记 type_audit 账本（skill_types 孤儿名 / types_relation 孤儿名在
    // ensure_type_components_cache / load_elemental_restraints 时已记账）。
    // 返回报告文本（空 = 干净）；判定（响亮报错/STRICT fail）由 type_audit::audit_now
    // 统一做，本函数只记账。
    std::string audit_type_references() const;

private:
    // 新 Unity 结构：双属性精灵 type 用合并 id（如 41=战斗地面），需分解为两个单属性 id。
    void ensure_type_components_cache() const;
    std::pair<int, int> decompose_type(int type_id) const;

    sqlite3* db_ = nullptr;
    std::string db_path_;
    mutable std::string last_error_;
    mutable bool type_components_cache_loaded_ = false;
    mutable std::unordered_map<int, std::pair<int, int>> type_components_cache_;
    // 连击规则缓存（J326）：装载过（含失败——失败也缓存，fail-closed 不反复重查）
    // 标记 + 结果表；close()/open_read_only() 双清。
    mutable bool combo_rules_loaded_ = false;
    mutable bool combo_rules_ok_ = false;
    mutable std::unordered_map<int, ComboRuleRecord> combo_rules_cache_;
    // 选择期路由缓存（J327）：语义同上（含失败也缓存）；close()/open_read_only() 双清。
    mutable bool selection_routes_loaded_ = false;
    mutable bool selection_routes_ok_ = false;
    mutable std::unordered_map<int, SelectionRouteRecord> selection_routes_cache_;
};

class OfficialDataStore {
public:
    static OfficialDataStore& instance();

    bool initialize(const std::string& db_path = kDefaultOfficialDatabasePath);
    void shutdown();

    bool ready() const { return repository_.is_open(); }
    const OfficialDataRepository& repository() const { return repository_; }
    OfficialDataRepository& repository() { return repository_; }

private:
    OfficialDataStore() = default;

    OfficialDataRepository repository_;
};

} // namespace official_data

#endif // OFFICIAL_DATA_REPOSITORY_H
