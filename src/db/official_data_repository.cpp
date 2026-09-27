#include <db/official_data_repository.h>

#include <sqlite3.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <sstream>
#include <utility>
#include <iostream>

namespace official_data {

namespace {

class Statement {
public:
    Statement(sqlite3* db, const char* sql) {
        if (db) {
            sqlite3_prepare_v2(db, sql, -1, &stmt_, nullptr);
        }
    }

    ~Statement() {
        if (stmt_) {
            sqlite3_finalize(stmt_);
        }
    }

    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    sqlite3_stmt* get() const { return stmt_; }
    explicit operator bool() const { return stmt_ != nullptr; }

private:
    sqlite3_stmt* stmt_ = nullptr;
};

std::string trim(std::string value) {
    const auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

std::vector<int> parse_int_list(const std::string& raw) {
    std::vector<int> values;
    std::istringstream iss(raw);
    int value = 0;
    while (iss >> value) {
        values.push_back(value);
    }
    return values;
}

// 解析官方 JSON 数组文本（如 "[5, 15, -1]"、"[]"）为 int 列表。
// 官方 moves.side_effect / side_effect_arg 是数组，C++ 侧做轻量解析。
std::vector<int> parse_json_int_array(const std::string& raw) {
    std::vector<int> values;
    std::size_t i = 0;
    const std::size_t n = raw.size();
    while (i < n) {
        const char ch = raw[i];
        if (ch == '-' || std::isdigit(static_cast<unsigned char>(ch))) {
            const bool negative = (ch == '-');
            if (negative) {
                ++i;
            }
            long value = 0;
            bool any = false;
            while (i < n && std::isdigit(static_cast<unsigned char>(raw[i]))) {
                value = value * 10 + (raw[i] - '0');
                ++i;
                any = true;
            }
            if (any) {
                values.push_back(negative ? -static_cast<int>(value) : static_cast<int>(value));
            }
        } else {
            ++i;
        }
    }
    return values;
}

// 解析官方 JSON 字符串数组（如 "[\"grass\", \"fight\"]"）为字符串列表。
std::vector<std::string> parse_json_string_array(const std::string& raw) {
    std::vector<std::string> values;
    std::size_t i = 0;
    const std::size_t n = raw.size();
    while (i < n) {
        if (raw[i] == '"') {
            std::string token;
            ++i;
            while (i < n && raw[i] != '"') {
                token += raw[i];
                ++i;
            }
            values.push_back(std::move(token));
            if (i < n) {
                ++i;  // 跳过结尾引号
            }
        } else {
            ++i;
        }
    }
    return values;
}

std::string column_text(sqlite3_stmt* stmt, int column) {
    const unsigned char* text = sqlite3_column_text(stmt, column);
    return text ? reinterpret_cast<const char*>(text) : std::string{};
}

bool bind_int(sqlite3_stmt* stmt, int index, int value) {
    return sqlite3_bind_int(stmt, index, value) == SQLITE_OK;
}

bool bind_text(sqlite3_stmt* stmt, int index, const std::string& value) {
    return sqlite3_bind_text(stmt, index, value.c_str(), -1, SQLITE_TRANSIENT) == SQLITE_OK;
}

std::optional<int> query_side_effect_arg_count(sqlite3* db, int effect_id) {
    Statement stmt(db, "SELECT arg_count FROM side_effect WHERE id = ?1");
    if (!stmt || !bind_int(stmt.get(), 1, effect_id)) {
        return std::nullopt;
    }
    if (sqlite3_step(stmt.get()) != SQLITE_ROW) {
        return std::nullopt;
    }
    return sqlite3_column_int(stmt.get(), 0);
}

void fill_effect_metadata(sqlite3* db, SkillEffectRecord& effect) {
    // 注意：新 Unity 库 effect_info 无 sp 列（旧 H5 遗留），查询只取实际存在的列，
    // 否则整个语句失败 → info 永远为空（2026-08-22 组合语法解析时发现）。
    Statement stmt(
        db,
        "SELECT args_num, info, param FROM effect_info WHERE id = ?1"
    );
    if (!stmt || !bind_int(stmt.get(), 1, effect.effect_id)) {
        return;
    }
    if (sqlite3_step(stmt.get()) != SQLITE_ROW) {
        return;
    }
    effect.effect_args_num = sqlite3_column_int(stmt.get(), 0);
    effect.info = column_text(stmt.get(), 1);
    effect.param = column_text(stmt.get(), 2);
}

std::vector<SkillEffectRecord> build_skill_effects(
    sqlite3* db,
    const std::string& side_effect_raw,
    const std::string& side_effect_arg_raw
) {
    const std::vector<int> effect_ids = parse_json_int_array(side_effect_raw);
    const std::vector<int> flat_args = parse_json_int_array(side_effect_arg_raw);
    std::vector<SkillEffectRecord> effects;
    effects.reserve(effect_ids.size());

    std::size_t arg_cursor = 0;
    for (int effect_id : effect_ids) {
        SkillEffectRecord effect;
        effect.effect_id = effect_id;
        fill_effect_metadata(db, effect);
        // 切片数 = side_effect.arg_count；**表缺行时**（神谕新效果段常见：2380/2428/2429、
        // 2425~2432 整段都没有行，2026-09-25 圣莫技能组发现）用 effect_info.args_num 兜底
        // ——否则切片长度算 0，尾部效果的参数整段饿死（带参技能效果静默 no-op）。
        // 两表都有行时以 side_effect 为准（口径存在既存不一致，如 118/596/1154，不可互换）。
        effect.arg_count = query_side_effect_arg_count(db, effect_id).value_or(effect.effect_args_num);

        if (effect.arg_count > 0) {
            const std::size_t end = std::min(flat_args.size(), arg_cursor + static_cast<std::size_t>(effect.arg_count));
            effect.args.assign(flat_args.begin() + static_cast<std::ptrdiff_t>(arg_cursor),
                               flat_args.begin() + static_cast<std::ptrdiff_t>(end));
            arg_cursor = end;
        }

        effects.push_back(std::move(effect));
    }

    return effects;
}

} // namespace

OfficialDataRepository::~OfficialDataRepository() {
    close();
}

bool OfficialDataRepository::open_read_only(const std::string& db_path) {
    std::cout << "db status: " << (db_ == nullptr) << std::endl;
    close();
    last_error_.clear();

    if (!std::filesystem::exists(db_path)) {
        last_error_ = "database file does not exist: " + db_path;
        return false;
    }

    sqlite3* connection = nullptr;
    const int rc = sqlite3_open_v2(db_path.c_str(), &connection, SQLITE_OPEN_READONLY, nullptr);
    if (rc != SQLITE_OK || !connection) {
        last_error_ = connection ? sqlite3_errmsg(connection) : "sqlite3_open_v2 failed";
        if (connection) {
            sqlite3_close(connection);
        }
        return false;
    }

    sqlite3_busy_timeout(connection, 1000);
    sqlite3_exec(connection, "PRAGMA query_only = ON", nullptr, nullptr, nullptr);

    db_ = connection;
    db_path_ = db_path;
    return true;
}

void OfficialDataRepository::close() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
    db_path_.clear();
    type_components_cache_loaded_ = false;
    type_components_cache_.clear();
}

std::optional<SkillRecord> OfficialDataRepository::load_skill(int move_id) const {
    if (!db_) {
        last_error_ = "database is not open";
        return std::nullopt;
    }

    Statement stmt(
        db_,
        "SELECT id, name, type_id, category, power, accuracy, COALESCE(priority, 0), "
        "COALESCE(max_pp, 0), COALESCE(must_hit, 0), COALESCE(crit_rate, 0), "
        "COALESCE(side_effect, ''), COALESCE(side_effect_arg, '') "
        "FROM moves WHERE id = ?1"
    );
    if (!stmt || !bind_int(stmt.get(), 1, move_id)) {
        last_error_ = sqlite3_errmsg(db_);
        return std::nullopt;
    }

    if (sqlite3_step(stmt.get()) != SQLITE_ROW) {
        return std::nullopt;
    }

    SkillRecord skill;
    skill.id = sqlite3_column_int(stmt.get(), 0);
    skill.name = column_text(stmt.get(), 1);
    skill.type_id = sqlite3_column_int(stmt.get(), 2);
    skill.category = sqlite3_column_int(stmt.get(), 3);
    skill.power = sqlite3_column_int(stmt.get(), 4);
    skill.accuracy = sqlite3_column_int(stmt.get(), 5);
    skill.priority = sqlite3_column_int(stmt.get(), 6);
    skill.max_pp = sqlite3_column_int(stmt.get(), 7);
    skill.must_hit = sqlite3_column_int(stmt.get(), 8);
    skill.crit_rate = sqlite3_column_int(stmt.get(), 9);
    skill.effects = build_skill_effects(db_, column_text(stmt.get(), 10), column_text(stmt.get(), 11));
    return skill;
}

std::vector<SkillEffectRecord> OfficialDataRepository::load_skill_effects(int move_id) const {
    const std::optional<SkillRecord> skill = load_skill(move_id);
    return skill ? skill->effects : std::vector<SkillEffectRecord>{};
}

std::optional<CustomProgramRecord> OfficialDataRepository::load_custom_program(int effect_id, int skill_id) const {
    // 认证数据层：查离线编码效果程序。effect_id/skill_id 传 -1 表示"不按该维过滤"；
    // 表空时返回 nullopt → 调用方走既有注册函数/parser 路径，行为不变。
    if (!db_) {
        last_error_ = "database is not open";
        return std::nullopt;
    }
    Statement stmt(
        db_,
        "SELECT effect_id, skill_id, kind, name_zh, unit_json, memo "
        "FROM custom_effect_programs "
        // （skill_id=-1 = 通用模板, 适用于任意技能；否则=仅该技能）
        "WHERE (?1 = -1 OR effect_id = ?1) AND (?2 = -1 OR skill_id = -1 OR skill_id = ?2) "
        "LIMIT 1"
    );
    if (!stmt || !bind_int(stmt.get(), 1, effect_id) || !bind_int(stmt.get(), 2, skill_id)) {
        last_error_ = sqlite3_errmsg(db_);
        return std::nullopt;
    }
    if (sqlite3_step(stmt.get()) != SQLITE_ROW) {
        return std::nullopt;
    }
    CustomProgramRecord r;
    r.effect_id = sqlite3_column_int(stmt.get(), 0);
    r.skill_id = sqlite3_column_int(stmt.get(), 1);
    r.kind = column_text(stmt.get(), 2);
    r.name_zh = column_text(stmt.get(), 3);
    r.unit_json = column_text(stmt.get(), 4);
    r.memo = column_text(stmt.get(), 5);
    return r;
}

std::optional<CustomOverrideRecord> OfficialDataRepository::load_custom_override(int effect_id) const {
    // 认证数据层：查官方差异纠偏(此 effect 是否采用与官方不同的解释/处置)。
    if (!db_) {
        last_error_ = "database is not open";
        return std::nullopt;
    }
    Statement stmt(
        db_,
        "SELECT effect_id, override_type, map_value, source_scope, rationale "
        "FROM custom_effect_overrides WHERE effect_id = ?1 LIMIT 1"
    );
    if (!stmt || !bind_int(stmt.get(), 1, effect_id)) {
        last_error_ = sqlite3_errmsg(db_);
        return std::nullopt;
    }
    if (sqlite3_step(stmt.get()) != SQLITE_ROW) {
        return std::nullopt;
    }
    CustomOverrideRecord r;
    r.effect_id = sqlite3_column_int(stmt.get(), 0);
    r.override_type = column_text(stmt.get(), 1);
    r.map_value = column_text(stmt.get(), 2);
    r.source_scope = column_text(stmt.get(), 3);
    r.rationale = column_text(stmt.get(), 4);
    return r;
}

std::vector<std::pair<int, std::string>> OfficialDataRepository::load_effect_window_overrides() const {
    std::vector<std::pair<int, std::string>> out;
    if (!db_) {
        return out;
    }
    Statement stmt(
        db_,
        "SELECT effect_id, map_value FROM custom_effect_overrides "
        "WHERE override_type = 'window' AND map_value IS NOT NULL"
    );
    if (!stmt) {
        last_error_ = sqlite3_errmsg(db_);
        return out;
    }
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        out.emplace_back(sqlite3_column_int(stmt.get(), 0), column_text(stmt.get(), 1));
    }
    return out;
}

std::optional<int> OfficialDataRepository::find_monster_id_by_exact_name(const std::string& monster_name) const {
    if (!db_) {
        last_error_ = "database is not open";
        return std::nullopt;
    }

    Statement stmt(
        db_,
        "SELECT id FROM monsters "
        "WHERE json_extract(raw_json, '$.def_name') = ?1 "
        "LIMIT 1"
    );
    if (!stmt || !bind_text(stmt.get(), 1, trim(monster_name))) {
        last_error_ = sqlite3_errmsg(db_);
        return std::nullopt;
    }

    if (sqlite3_step(stmt.get()) != SQLITE_ROW) {
        return std::nullopt;
    }
    return sqlite3_column_int(stmt.get(), 0);
}

std::vector<LearnableMoveRecord> OfficialDataRepository::load_monster_learnable_moves(int monster_id) const {
    std::vector<LearnableMoveRecord> moves;
    if (!db_) {
        last_error_ = "database is not open";
        return moves;
    }

    Statement stmt(
        db_,
        "SELECT move_id, learning_lv "
        "FROM monster_learnable_moves "
        "WHERE monster_id = ?1 "
        "ORDER BY learning_lv ASC, move_id ASC"
    );
    if (!stmt || !bind_int(stmt.get(), 1, monster_id)) {
        last_error_ = sqlite3_errmsg(db_);
        return moves;
    }

    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        LearnableMoveRecord item;
        item.move_id = sqlite3_column_int(stmt.get(), 0);
        item.learning_level = sqlite3_column_int(stmt.get(), 1);
        moves.push_back(std::move(item));
    }

    return moves;
}

std::vector<int> OfficialDataRepository::load_monster_hidden_moves(int monster_id) const {
    std::vector<int> moves;
    if (!db_) {
        last_error_ = "database is not open";
        return moves;
    }

    Statement stmt(
        db_,
        "SELECT move_id FROM hide_moves "
        "WHERE pet_id = ?1 "
        "ORDER BY move_id ASC"
    );
    if (!stmt || !bind_int(stmt.get(), 1, monster_id)) {
        last_error_ = sqlite3_errmsg(db_);
        return moves;
    }

    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        moves.push_back(sqlite3_column_int(stmt.get(), 0));
    }

    return moves;
}

OracleMoves OfficialDataRepository::load_monster_oracle_moves(int monster_id) const {
    OracleMoves oracle;
    if (!db_) {
        last_error_ = "database is not open";
        return oracle;
    }

    // 分类判据用 moves 表的威力列（sp_hide_moves 自身无威力）：≥155 = 第五，
    // 其余（属性位 0 / 先三 85 / 150 位）= 1~4 号槽候选。kind 列不参与分类。
    Statement stmt(
        db_,
        "SELECT s.moves, COALESCE(m.power, 0) "
        "FROM sp_hide_moves s LEFT JOIN moves m ON m.id = s.moves "
        "WHERE s.monster = ?1 "
        "ORDER BY s.moves ASC"
    );
    if (!stmt || !bind_int(stmt.get(), 1, monster_id)) {
        last_error_ = sqlite3_errmsg(db_);
        return oracle;
    }

    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        const int move_id = sqlite3_column_int(stmt.get(), 0);
        const int power = sqlite3_column_int(stmt.get(), 1);
        if (move_id <= 0) {
            continue;
        }
        if (power >= 155) {
            oracle.fifth_moves.push_back(move_id);
        } else {
            oracle.normal_moves.push_back(move_id);
        }
    }
    return oracle;
}

bool OfficialDataRepository::pet_has_move_effect(int monster_id, int effect_id) const {
    if (!db_) {
        last_error_ = "database is not open";
        return false;
    }

    Statement stmt(
        db_,
        "SELECT DISTINCT m.side_effect FROM moves m WHERE m.id IN ("
        "  SELECT move_id FROM monster_learnable_moves WHERE monster_id = ?1"
        "  UNION SELECT move_id FROM hide_moves WHERE pet_id = ?1"
        "  UNION SELECT moves FROM sp_hide_moves WHERE monster = ?1"
        ")"
    );
    if (!stmt || !bind_int(stmt.get(), 1, monster_id)) {
        last_error_ = sqlite3_errmsg(db_);
        return false;
    }

    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        const std::vector<int> effects =
            parse_json_int_array(column_text(stmt.get(), 0));
        if (std::find(effects.begin(), effects.end(), effect_id) != effects.end()) {
            return true;
        }
    }
    return false;
}

std::optional<MonsterRecord> OfficialDataRepository::load_monster(int monster_id) const {
    if (!db_) {
        last_error_ = "database is not open";
        return std::nullopt;
    }

    Statement stmt(
        db_,
        "SELECT "
        "id, "
        "COALESCE(json_extract(raw_json, '$.def_name'), ''), "
        "COALESCE(json_extract(raw_json, '$.type'), 0), "
        "COALESCE(soul_mark_id, 0), "
        "COALESCE(json_extract(raw_json, '$.gender'), 2), "
        "COALESCE(json_extract(raw_json, '$.hp'), 0), "
        "COALESCE(json_extract(raw_json, '$.atk'), 0), "
        "COALESCE(json_extract(raw_json, '$.def'), 0), "
        "COALESCE(json_extract(raw_json, '$.sp_atk'), 0), "
        "COALESCE(json_extract(raw_json, '$.sp_def'), 0), "
        "COALESCE(json_extract(raw_json, '$.spd'), 0) "
        "FROM monsters WHERE id = ?1"
    );
    if (!stmt || !bind_int(stmt.get(), 1, monster_id)) {
        last_error_ = sqlite3_errmsg(db_);
        return std::nullopt;
    }

    if (sqlite3_step(stmt.get()) != SQLITE_ROW) {
        return std::nullopt;
    }

    MonsterRecord monster;
    monster.id = sqlite3_column_int(stmt.get(), 0);
    monster.name = column_text(stmt.get(), 1);
    const int raw_type = sqlite3_column_int(stmt.get(), 2);
    monster.soul_mark_id = sqlite3_column_int(stmt.get(), 3);
    monster.gender = sqlite3_column_int(stmt.get(), 4);
    monster.hp = sqlite3_column_int(stmt.get(), 5);
    monster.atk = sqlite3_column_int(stmt.get(), 6);
    monster.def = sqlite3_column_int(stmt.get(), 7);
    monster.sp_atk = sqlite3_column_int(stmt.get(), 8);
    monster.sp_def = sqlite3_column_int(stmt.get(), 9);
    monster.spd = sqlite3_column_int(stmt.get(), 10);
    // 新 Unity 结构：双属性精灵 type 用合并 id，分解为两个单属性 id。
    const std::pair<int, int> components = decompose_type(raw_type);
    monster.type = components.first;
    monster.secondary_type = components.second;
    monster.learnable_moves = load_monster_learnable_moves(monster.id);
    monster.hidden_moves = load_monster_hidden_moves(monster.id);
    monster.oracle_moves = load_monster_oracle_moves(monster.id);
    return monster;
}

std::optional<MonsterRecord> OfficialDataRepository::load_monster_by_exact_name(const std::string& monster_name) const {
    const std::optional<int> monster_id = find_monster_id_by_exact_name(monster_name);
    return monster_id ? load_monster(*monster_id) : std::nullopt;
}

std::optional<SoulMarkRecord> OfficialDataRepository::load_soul_mark(int soul_mark_id) const {
    if (!db_) {
        last_error_ = "database is not open";
        return std::nullopt;
    }
    if (soul_mark_id <= 0) {
        return std::nullopt;
    }

    Statement stmt(
        db_,
        "SELECT idx, stat, effect_id, COALESCE(args, ''), "
        "COALESCE(desc, ''), COALESCE(intro, ''), COALESCE(star_level, 0) "
        "FROM new_se WHERE idx = ?1"
    );
    if (!stmt || !bind_int(stmt.get(), 1, soul_mark_id)) {
        last_error_ = sqlite3_errmsg(db_);
        return std::nullopt;
    }
    if (sqlite3_step(stmt.get()) != SQLITE_ROW) {
        return std::nullopt;
    }

    SoulMarkRecord record;
    record.id = sqlite3_column_int(stmt.get(), 0);
    record.stat = sqlite3_column_int(stmt.get(), 1);
    record.effect_id = sqlite3_column_int(stmt.get(), 2);
    record.args = parse_int_list(column_text(stmt.get(), 3));
    record.can_reset = 0;  // 新 Unity 数据无 CanReset 字段
    record.description = column_text(stmt.get(), 4);
    record.intro = column_text(stmt.get(), 5);
    record.star_level = sqlite3_column_int(stmt.get(), 6);
    return record;
}

std::optional<CommonTraitRecord> OfficialDataRepository::load_common_trait(int idx) const {
    if (!db_) {
        last_error_ = "database is not open";
        return std::nullopt;
    }
    if (idx <= 0) {
        return std::nullopt;
    }

    Statement stmt(
        db_,
        "SELECT idx, stat, effect_id, COALESCE(args, ''), "
        "COALESCE(desc, ''), COALESCE(intro, ''), COALESCE(star_level, 0) "
        "FROM new_se WHERE idx = ?1 AND stat = 1"
    );
    if (!stmt || !bind_int(stmt.get(), 1, idx)) {
        last_error_ = sqlite3_errmsg(db_);
        return std::nullopt;
    }
    if (sqlite3_step(stmt.get()) != SQLITE_ROW) {
        return std::nullopt;
    }

    CommonTraitRecord record;
    record.id = sqlite3_column_int(stmt.get(), 0);
    record.stat = sqlite3_column_int(stmt.get(), 1);
    record.effect_id = sqlite3_column_int(stmt.get(), 2);
    record.args = parse_int_list(column_text(stmt.get(), 3));
    record.description = column_text(stmt.get(), 4);
    record.intro = column_text(stmt.get(), 5);
    record.star_level = sqlite3_column_int(stmt.get(), 6);
    return record;
}

std::optional<EquipRecord> OfficialDataRepository::load_equip(int item_id) const {
    if (!db_) {
        last_error_ = "database is not open";
        return std::nullopt;
    }
    if (item_id <= 0) {
        return std::nullopt;
    }

    Statement stmt(
        db_,
        "SELECT item_id, COALESCE(name, ''), COALESCE(quality, 0), "
        "COALESCE(suit_id, 0), COALESCE(desc, '') "
        "FROM equip WHERE item_id = ?1"
    );
    if (!stmt || !bind_int(stmt.get(), 1, item_id)) {
        last_error_ = sqlite3_errmsg(db_);
        return std::nullopt;
    }
    if (sqlite3_step(stmt.get()) != SQLITE_ROW) {
        return std::nullopt;
    }

    EquipRecord record;
    record.item_id = sqlite3_column_int(stmt.get(), 0);
    record.name = column_text(stmt.get(), 1);
    record.quality = sqlite3_column_int(stmt.get(), 2);
    record.suit_id = sqlite3_column_int(stmt.get(), 3);
    record.desc = column_text(stmt.get(), 4);
    return record;
}

std::optional<SuitRecord> OfficialDataRepository::load_suit(int suit_id) const {
    if (!db_) {
        last_error_ = "database is not open";
        return std::nullopt;
    }
    if (suit_id <= 0) {
        return std::nullopt;
    }

    Statement stmt(
        db_,
        "SELECT id, COALESCE(name, ''), COALESCE(cloths, '[]'), COALESCE(suitdes, '') "
        "FROM suit WHERE id = ?1"
    );
    if (!stmt || !bind_int(stmt.get(), 1, suit_id)) {
        last_error_ = sqlite3_errmsg(db_);
        return std::nullopt;
    }
    if (sqlite3_step(stmt.get()) != SQLITE_ROW) {
        return std::nullopt;
    }

    SuitRecord record;
    record.id = sqlite3_column_int(stmt.get(), 0);
    record.name = column_text(stmt.get(), 1);
    record.cloths = parse_json_int_array(column_text(stmt.get(), 2));
    record.suitdes = column_text(stmt.get(), 3);
    return record;
}

std::vector<EquipStatRecord> OfficialDataRepository::load_equip_stats(int item_id) const {
    std::vector<EquipStatRecord> rows;
    if (!db_ || item_id <= 0) {
        return rows;
    }

    Statement stmt(
        db_,
        "SELECT item_id, stat_index, amount, COALESCE(add_way, 0), COALESCE(scope, 'per_piece'), "
        "COALESCE(target_monster, 0) "
        "FROM custom_equip_stats WHERE item_id = ?1"
    );
    if (!stmt || !bind_int(stmt.get(), 1, item_id)) {
        return rows;   // custom 表可能尚未建（旧库）——视为无数值，不报错
    }
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        EquipStatRecord row;
        row.item_id = sqlite3_column_int(stmt.get(), 0);
        row.stat_index = sqlite3_column_int(stmt.get(), 1);
        row.amount = sqlite3_column_int(stmt.get(), 2);
        row.add_way = sqlite3_column_int(stmt.get(), 3);
        row.scope = column_text(stmt.get(), 4);
        row.target_monster = sqlite3_column_int(stmt.get(), 5);
        rows.push_back(std::move(row));
    }
    return rows;
}

std::optional<NatureRecord> OfficialDataRepository::load_nature(int nature_id) const {
    if (!db_ || nature_id < 0) {
        return std::nullopt;
    }

    Statement stmt(
        db_,
        "SELECT id, COALESCE(name, ''), COALESCE(atk, 1.0), COALESCE(sp_atk, 1.0), "
        "COALESCE(def, 1.0), COALESCE(sp_def, 1.0), COALESCE(spd, 1.0) "
        "FROM nature WHERE id = ?1"
    );
    if (!stmt || !bind_int(stmt.get(), 1, nature_id)) {
        return std::nullopt;   // nature 表可能尚未建（旧库）——视为无性格
    }
    if (sqlite3_step(stmt.get()) != SQLITE_ROW) {
        return std::nullopt;
    }

    NatureRecord record;
    record.id = sqlite3_column_int(stmt.get(), 0);
    record.name = column_text(stmt.get(), 1);
    record.mult[0] = sqlite3_column_double(stmt.get(), 2);  // 攻
    record.mult[1] = sqlite3_column_double(stmt.get(), 3);  // 特攻
    record.mult[2] = sqlite3_column_double(stmt.get(), 4);  // 防
    record.mult[3] = sqlite3_column_double(stmt.get(), 5);  // 特防
    record.mult[4] = sqlite3_column_double(stmt.get(), 6);  // 速
    return record;
}

std::optional<MintmarkRecord> OfficialDataRepository::load_mintmark(int item_id) const {
    if (!db_ || item_id <= 0) {
        return std::nullopt;
    }

    Statement stmt(
        db_,
        "SELECT id, COALESCE(name, ''), COALESCE(effect_des, ''), COALESCE(type, 0), "
        "COALESCE(grade, 0), COALESCE(quality, 0), COALESCE(class_id, 0), "
        "COALESCE(arg_json, '[]'), COALESCE(base_json, '[]'), COALESCE(max_json, '[]'), "
        "COALESCE(monster_ids, '[]'), COALESCE(hide, 0) "
        "FROM mintmark WHERE id = ?1"
    );
    if (!stmt || !bind_int(stmt.get(), 1, item_id)) {
        return std::nullopt;   // mintmark 表可能尚未建（旧库）
    }
    if (sqlite3_step(stmt.get()) != SQLITE_ROW) {
        return std::nullopt;
    }

    MintmarkRecord record;
    record.id = sqlite3_column_int(stmt.get(), 0);
    record.name = column_text(stmt.get(), 1);
    record.effect_des = column_text(stmt.get(), 2);
    record.type = sqlite3_column_int(stmt.get(), 3);
    record.grade = sqlite3_column_int(stmt.get(), 4);
    record.quality = sqlite3_column_int(stmt.get(), 5);
    record.class_id = sqlite3_column_int(stmt.get(), 6);
    const auto read6 = [&](int col, std::array<int, 6>& out) {
        const std::vector<int> v = parse_json_int_array(column_text(stmt.get(), col));
        for (std::size_t i = 0; i < out.size() && i < v.size(); ++i) {
            out[i] = v[i];
        }
    };
    // 官方 mintmark 六维序 = [攻, 防, 特攻, 特防, 速, 体]（圣·虚无 effect_des 实测钉死：
    // "攻击/防御/特防/速度/体力"五项 ↔ max=[55,25,0,25,35,90]——防在 index1、特攻恒 0
    // 在 index2；圣·元素 max=[55,30,0,30,30,85] 同构印证）。⚠️ 与引擎 NumericalPropertyIndex
    // 序 [攻,特攻,防,特防,速,体] 不同，此处重排，调用方拿到的统一是引擎序。
    std::array<int, 6> raw_arg{}, raw_base{}, raw_max{};
    read6(7, raw_arg);
    read6(8, raw_base);
    read6(9, raw_max);
    const auto to_engine_order = [](const std::array<int, 6>& raw) {
        return std::array<int, 6>{raw[0], raw[2], raw[1], raw[3], raw[4], raw[5]};
    };
    record.stat_arg = to_engine_order(raw_arg);
    record.stat_base = to_engine_order(raw_base);
    record.stat_max = to_engine_order(raw_max);
    record.monster_ids = parse_json_int_array(column_text(stmt.get(), 10));
    record.hide = sqlite3_column_int(stmt.get(), 11) != 0;
    return record;
}

std::optional<BattleItemRecord> OfficialDataRepository::load_battle_item(int item_id) const {
    if (!db_ || item_id <= 0) {
        return std::nullopt;
    }

    // hp / pp / remove_mon_stat 保留 NULL 语义（官方 JSON 无该字段 = 无此效果），
    // 不能 COALESCE 成 0——0 会和"回复 0 点"混淆，也让"带 HP 效果"判定失真。
    Statement stmt(
        db_,
        "SELECT item_id, COALESCE(name, ''), COALESCE(item_type, 0), hp, pp, remove_mon_stat, "
        "COALESCE(remove_all_mon_stat, 0), COALESCE(remove_bt_lv_down, 0), "
        "COALESCE(bonus, 0), COALESCE(max_count, 0) "
        "FROM battle_items WHERE item_id = ?1"
    );
    if (!stmt || !bind_int(stmt.get(), 1, item_id)) {
        return std::nullopt;   // battle_items 表可能尚未建（旧库）——视为无此物品
    }
    if (sqlite3_step(stmt.get()) != SQLITE_ROW) {
        return std::nullopt;
    }

    const auto column_nullable_int = [stmt = stmt.get()](int column) -> std::optional<int> {
        if (sqlite3_column_type(stmt, column) == SQLITE_NULL) {
            return std::nullopt;
        }
        return sqlite3_column_int(stmt, column);
    };

    BattleItemRecord record;
    record.item_id = sqlite3_column_int(stmt.get(), 0);
    record.name = column_text(stmt.get(), 1);
    record.item_type = sqlite3_column_int(stmt.get(), 2);
    record.hp = column_nullable_int(3);
    record.pp = column_nullable_int(4);
    record.remove_mon_stat = column_nullable_int(5);
    record.remove_all_mon_stat = sqlite3_column_int(stmt.get(), 6);
    record.remove_bt_lv_down = sqlite3_column_int(stmt.get(), 7);
    record.bonus = sqlite3_column_double(stmt.get(), 8);
    record.max_count = sqlite3_column_int(stmt.get(), 9);
    return record;
}

// 去除官方富文本标记（[color=#xxx]...[/color]、[sprite name=xxx]、\n 转义）。
std::string strip_rich_text(const std::string& raw) {
    std::string out;
    out.reserve(raw.size());
    std::size_t i = 0;
    const std::size_t n = raw.size();
    while (i < n) {
        if (raw[i] == '[') {
            const std::size_t close = raw.find(']', i);
            if (close != std::string::npos) {
                const std::string tag = raw.substr(i + 1, close - i - 1);
                const bool is_rich = tag.rfind("color", 0) == 0 || tag.rfind("/color", 0) == 0
                    || tag.rfind("size", 0) == 0 || tag.rfind("/size", 0) == 0
                    || tag.rfind("sprite", 0) == 0;
                if (is_rich) {
                    i = close + 1;
                    continue;
                }
            }
        }
        if (raw[i] == '\\' && i + 1 < n && raw[i + 1] == 'n') {
            out += '\n';
            i += 2;
            continue;
        }
        out += raw[i];
        ++i;
    }
    return out;
}

std::optional<SoulMarkDisplayRecord> OfficialDataRepository::load_soul_mark_display_by_monster(
    int monster_id
) const {
    const std::vector<SoulMarkDisplayRecord> all = load_soul_mark_displays_by_monster(monster_id);
    if (all.empty()) {
        return std::nullopt;
    }
    return all.back();   // 多版本取 icon_id 最大（= 当前版本，含神谕版）
}

std::vector<SoulMarkDisplayRecord> OfficialDataRepository::load_soul_mark_displays_by_monster(
    int monster_id
) const {
    std::vector<SoulMarkDisplayRecord> rows;
    if (!db_) {
        last_error_ = "database is not open";
        return rows;
    }

    Statement stmt(
        db_,
        "SELECT id, pet_id, effect_id, COALESCE(kind, '[]'), COALESCE(args, ''), "
        "COALESCE(tips, ''), COALESCE(come, '') FROM effect_icon"
    );
    if (!stmt) {
        last_error_ = sqlite3_errmsg(db_);
        return rows;
    }

    // pet_id 是 JSON 数组字符串（如 "[4911]"），必须解析后精确比对，
    // LIKE 匹配会把 14911 误认成 4911。多行（基础/强化/神谕版本）按 icon_id 升序返回。
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        const std::vector<int> pet_ids = parse_json_int_array(column_text(stmt.get(), 1));
        if (std::find(pet_ids.begin(), pet_ids.end(), monster_id) == pet_ids.end()) {
            continue;
        }
        SoulMarkDisplayRecord record;
        record.monster_id = monster_id;
        record.icon_id = sqlite3_column_int(stmt.get(), 0);
        record.effect_id = sqlite3_column_int(stmt.get(), 2);
        record.kind_tags = parse_json_int_array(column_text(stmt.get(), 3));
        record.args = parse_int_list(column_text(stmt.get(), 4));
        record.tips = column_text(stmt.get(), 5);
        record.tips_plain = strip_rich_text(record.tips);
        record.come = column_text(stmt.get(), 6);
        auto it = rows.begin();
        while (it != rows.end() && it->icon_id < record.icon_id) {
            ++it;
        }
        rows.insert(it, std::move(record));
    }
    return rows;
}

std::optional<TermRecord> OfficialDataRepository::load_term(const std::string& term_name) const {
    if (!db_) {
        last_error_ = "database is not open";
        return std::nullopt;
    }

    auto run = [&](const char* sql) -> std::optional<TermRecord> {
        Statement stmt(db_, sql);
        if (!stmt || !bind_text(stmt.get(), 1, term_name)) {
            last_error_ = sqlite3_errmsg(db_);
            return std::nullopt;
        }
        if (sqlite3_step(stmt.get()) != SQLITE_ROW) {
            return std::nullopt;
        }
        TermRecord record;
        record.id = sqlite3_column_int(stmt.get(), 0);
        record.kind = sqlite3_column_int(stmt.get(), 1);
        record.name = column_text(stmt.get(), 2);
        record.description = strip_rich_text(column_text(stmt.get(), 3));
        return record;
    };

    if (auto record = run(
            "SELECT id, kind, kinddes, COALESCE(desc, '') FROM effect_des WHERE kinddes = ?1")) {
        return record;
    }
    return run(
        "SELECT id, kind, kinddes, COALESCE(desc, '') FROM effect_des WHERE kinddes LIKE ?1 LIMIT 1");
}

std::vector<TermRecord> OfficialDataRepository::load_terms_by_kind(int kind) const {
    std::vector<TermRecord> records;
    if (!db_) {
        last_error_ = "database is not open";
        return records;
    }

    Statement stmt(
        db_,
        "SELECT id, kind, kinddes, COALESCE(desc, '') FROM effect_des WHERE kind = ?1"
    );
    if (!stmt || !bind_int(stmt.get(), 1, kind)) {
        last_error_ = sqlite3_errmsg(db_);
        return records;
    }
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        TermRecord record;
        record.id = sqlite3_column_int(stmt.get(), 0);
        record.kind = sqlite3_column_int(stmt.get(), 1);
        record.name = column_text(stmt.get(), 2);
        record.description = strip_rich_text(column_text(stmt.get(), 3));
        records.push_back(std::move(record));
    }
    return records;
}

std::vector<EffectTemplateRecord> OfficialDataRepository::load_all_effect_templates() const {
    std::vector<EffectTemplateRecord> records;
    if (!db_) {
        last_error_ = "database is not open";
        return records;
    }

    Statement stmt(
        db_,
        "SELECT id, COALESCE(args_num, 0), COALESCE(info, ''), COALESCE(param, '') "
        "FROM effect_info"
    );
    if (!stmt) {
        last_error_ = sqlite3_errmsg(db_);
        return records;
    }
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        EffectTemplateRecord record;
        record.id = sqlite3_column_int(stmt.get(), 0);
        record.args_num = sqlite3_column_int(stmt.get(), 1);
        record.info = column_text(stmt.get(), 2);
        record.param = column_text(stmt.get(), 3);
        records.push_back(std::move(record));
    }
    return records;
}

void OfficialDataRepository::ensure_type_components_cache() const {
    if (type_components_cache_loaded_ || !db_) {
        return;
    }
    type_components_cache_loaded_ = true;

    // en 名 -> 单属性 id（如 "grass" -> 1）。仅单元素数组为单属性。
    std::unordered_map<std::string, int> name_to_id;
    std::vector<std::pair<int, std::pair<std::string, std::string>>> pending_combined;

    Statement stmt(db_, "SELECT id, en FROM skill_types");
    if (!stmt) {
        return;
    }
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        const int id = sqlite3_column_int(stmt.get(), 0);
        const std::vector<std::string> names = parse_json_string_array(column_text(stmt.get(), 1));
        if (names.size() == 1) {
            type_components_cache_[id] = {id, 0};
            name_to_id[names[0]] = id;
        } else if (names.size() == 2) {
            pending_combined.emplace_back(id, std::make_pair(names[0], names[1]));
        }
    }

    for (const auto& [id, names] : pending_combined) {
        const int a = name_to_id.count(names.first) ? name_to_id.at(names.first) : 0;
        const int b = name_to_id.count(names.second) ? name_to_id.at(names.second) : 0;
        type_components_cache_[id] = {a, b};
    }
}

std::pair<int, int> OfficialDataRepository::decompose_type(int type_id) const {
    ensure_type_components_cache();
    const auto it = type_components_cache_.find(type_id);
    if (it == type_components_cache_.end()) {
        return {type_id, 0};
    }
    return it->second;
}

bool OfficialDataRepository::load_elemental_restraints(
    std::vector<std::vector<double>>& matrix
) const {
    if (!db_) {
        last_error_ = "database is not open";
        return false;
    }

    // en 名 -> 单属性 id（克制表用 en 名作外键，需映射回 skill_types.id）。
    std::unordered_map<std::string, int> name_to_id;
    {
        Statement stmt(
            db_,
            "SELECT id, en FROM skill_types WHERE json_array_length(en) = 1"
        );
        if (!stmt) {
            last_error_ = sqlite3_errmsg(db_);
            return false;
        }
        while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
            const int id = sqlite3_column_int(stmt.get(), 0);
            const std::vector<std::string> names =
                parse_json_string_array(column_text(stmt.get(), 1));
            if (names.size() == 1) {
                name_to_id[names[0]] = id;
            }
        }
    }

    Statement stmt(
        db_,
        "SELECT attacker_type, defender_type, multiple FROM types_relation"
    );
    if (!stmt) {
        last_error_ = sqlite3_errmsg(db_);
        return false;
    }
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        const std::string attacker = column_text(stmt.get(), 0);
        const std::string defender = column_text(stmt.get(), 1);
        const double multiple = sqlite3_column_double(stmt.get(), 2);

        const auto ait = name_to_id.find(attacker);
        const auto dit = name_to_id.find(defender);
        if (ait == name_to_id.end() || dit == name_to_id.end()) {
            continue;
        }
        const int attacker_id = ait->second;
        const int defender_id = dit->second;
        if (attacker_id < 0 || defender_id < 0 ||
            attacker_id >= static_cast<int>(matrix.size()) ||
            defender_id >= static_cast<int>(matrix[static_cast<std::size_t>(attacker_id)].size())) {
            continue;
        }
        // 直接存官方原始倍率 {0.0 免疫, 0.5 减半, 1.0 普通, 2.0 克制}。
        // 旧实现压缩成 int 三档把 0.5 减半丢成 0（免疫）→ 半克制伤害归 0 的 bug，已修复。
        matrix[static_cast<std::size_t>(attacker_id)][static_cast<std::size_t>(defender_id)] = multiple;
    }
    return true;
}

OfficialDataStore& OfficialDataStore::instance() {
    static OfficialDataStore store;
    return store;
}

bool OfficialDataStore::initialize(const std::string& db_path) {
    return repository_.open_read_only(db_path);
}

void OfficialDataStore::shutdown() {
    repository_.close();
}

} // namespace official_data
