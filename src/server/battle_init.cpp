#include <server/battle_init.h>

#include <nlohmann/json.hpp>

namespace server {
namespace {

constexpr int kPartySize = 6;
constexpr int kSkillCount = 5;
constexpr int kNumericCount = 6;

// per-pet 抗性配置（培养线，2026-09-26；可选）：
//   {"enabled": true, "damage": [暴击,固定,百分比],
//    "control": [[异常id, 概率], ...≤3], "uncontrol": [...≤3], "completeImmunity": 0}
// 上限钳制（伤害 35 / 异常槽 50）在组装层；此处只做结构/非负校验。
bool parse_resist(const nlohmann::json& node, ResistConfig& out, std::string& err) {
    if (!node.is_object()) {
        err = "cultivate.resist must be an object";
        return false;
    }
    ResistConfig cfg;
    cfg.enabled = true;   // 显式携带 resist 段 = 开启（可写 "enabled": false 显式关）
    if (node.contains("enabled")) {
        if (!node["enabled"].is_boolean()) {
            err = "cultivate.resist.enabled must be a boolean";
            return false;
        }
        cfg.enabled = node["enabled"].get<bool>();
    }
    if (node.contains("damage")) {
        const auto& arr = node["damage"];
        if (!arr.is_array() || arr.size() != 3) {
            err = "cultivate.resist.damage must be an array of 3";
            return false;
        }
        for (int i = 0; i < 3; ++i) {
            if (!arr[static_cast<std::size_t>(i)].is_number_integer()) {
                err = "cultivate.resist.damage entries must be integers";
                return false;
            }
            cfg.damage[static_cast<std::size_t>(i)] = arr[static_cast<std::size_t>(i)].get<int>();
        }
    }
    const auto read_slots = [&](const char* key,
                                std::array<std::pair<int, int>, 3>& slots) -> bool {
        if (!node.contains(key)) {
            return true;
        }
        const auto& arr = node[key];
        if (!arr.is_array() || arr.size() > 3) {
            err = std::string("cultivate.resist.") + key + " must be an array of at most 3 pairs";
            return false;
        }
        int slot = 0;
        for (const auto& pair : arr) {
            if (!pair.is_array() || pair.size() != 2 || !pair[0].is_number_integer() ||
                !pair[1].is_number_integer()) {
                err = std::string("cultivate.resist.") + key + " entries must be [anomalyId, pct]";
                return false;
            }
            slots[static_cast<std::size_t>(slot)] = {pair[0].get<int>(), pair[1].get<int>()};
            ++slot;
        }
        return true;
    };
    if (!read_slots("control", cfg.control) || !read_slots("uncontrol", cfg.uncontrol)) {
        return false;
    }
    if (node.contains("completeImmunity")) {
        if (!node["completeImmunity"].is_number_integer()) {
            err = "cultivate.resist.completeImmunity must be an integer";
            return false;
        }
        cfg.complete_immunity = node["completeImmunity"].get<int>();
    }
    out = cfg;
    return true;
}

// 一只精灵。字段全可缺省（缺省 = 0），但 petId 必须 > 0 —— 否则建队时查库必然失败，
// 早报错比让 FSM 建到一半抛出来好定位。
bool parse_pet(const nlohmann::json& node, BattlePetMessage& out, std::string& err) {
    if (!node.is_object()) {
        err = "pet entry must be an object";
        return false;
    }
    out = BattlePetMessage{};

    if (node.contains("petId")) {
        if (!node["petId"].is_number_integer()) {
            err = "petId must be an integer";
            return false;
        }
        out.pet_id = node["petId"].get<int>();
    }
    if (out.pet_id <= 0) {
        err = "petId is required and must be > 0";
        return false;
    }

    if (node.contains("skills")) {
        const auto& arr = node["skills"];
        if (!arr.is_array() || arr.size() != kSkillCount) {
            err = "skills must be an array of " + std::to_string(kSkillCount);
            return false;
        }
        for (int i = 0; i < kSkillCount; ++i) {
            if (!arr[static_cast<std::size_t>(i)].is_number_integer()) {
                err = "skills entries must be integers";
                return false;
            }
            out.chosen_skills_id[static_cast<std::size_t>(i)] =
                arr[static_cast<std::size_t>(i)].get<int>();
        }
    }

    if (node.contains("base")) {
        const auto& arr = node["base"];
        if (!arr.is_array() || arr.size() != kNumericCount) {
            err = "base must be an array of " + std::to_string(kNumericCount);
            return false;
        }
        for (int i = 0; i < kNumericCount; ++i) {
            if (!arr[static_cast<std::size_t>(i)].is_number_integer()) {
                err = "base entries must be integers";
                return false;
            }
            out.numerical_base[static_cast<std::size_t>(i)] =
                arr[static_cast<std::size_t>(i)].get<int>();
        }
    }

    if (node.contains("traitId")) {
        if (!node["traitId"].is_number_integer()) {
            err = "traitId must be an integer";
            return false;
        }
        out.common_trait_id = node["traitId"].get<int>();
    }

    // 神谕开关（精灵王线 K1）：可选布尔，缺省 false = 未神谕。
    if (node.contains("oracle")) {
        if (!node["oracle"].is_boolean()) {
            err = "oracle must be a boolean";
            return false;
        }
        out.oracle_on = node["oracle"].get<bool>();
    }

    // 词条参数覆盖（调试台"词条编辑"线，2026-09-26；可选）：
    // "effectParams": [{"skill": 技能id, "effect": 效果id, "index": 模板{n}下标, "value": 新值}]
    // 装配期生效（Skills::loadSkills 先改记录后建效果），index 越界补 0、效果不存在报错。
    if (node.contains("effectParams")) {
        const auto& arr = node["effectParams"];
        if (!arr.is_array()) {
            err = "effectParams must be an array";
            return false;
        }
        for (const auto& item : arr) {
            if (!item.is_object()) {
                err = "effectParams entries must be objects";
                return false;
            }
            const auto need_int = [&](const char* key, int& dst, bool allow_zero) -> bool {
                if (!item.contains(key) || !item[key].is_number_integer()) {
                    err = std::string("effectParams.") + key + " must be an integer";
                    return false;
                }
                dst = item[key].get<int>();
                if (!allow_zero && dst <= 0) {
                    err = std::string("effectParams.") + key + " must be > 0";
                    return false;
                }
                return true;
            };
            EffectParamOverride ov;
            if (!need_int("skill", ov.skill_id, false) || !need_int("effect", ov.effect_id, false)) {
                return false;
            }
            if (!need_int("index", ov.arg_index, true)) {
                return false;
            }
            if (!item.contains("value") || !item["value"].is_number_integer()) {
                err = "effectParams.value must be an integer";
                return false;
            }
            ov.value = item["value"].get<int>();
            out.effect_params.push_back(ov);
        }
    }

    // 效果禁用（2026-09-26 禁用基建；可选）：
    // "disabledEffects": {"skillEffects": [id...], "soulMarks": [id...]}
    // 兼容旧式扁平数组 ["id"...] = 只禁技能效果（soulMarks 省）。
    if (node.contains("disabledEffects")) {
        const auto& d = node["disabledEffects"];
        const auto read_ids = [](const nlohmann::json& arr, std::vector<int>& dst,
                                 const char* what, std::string& err) -> bool {
            for (const auto& item : arr) {
                if (!item.is_number_integer() || item.get<int>() <= 0) {
                    err = std::string("disabledEffects.") + what + " must be positive integers";
                    return false;
                }
                dst.push_back(item.get<int>());
            }
            return true;
        };
        if (d.is_array()) {
            if (!read_ids(d, out.disabled_effects.skill_effects, "skillEffects", err)) {
                return false;
            }
        } else if (d.is_object()) {
            if (d.contains("skillEffects")) {
                if (!d["skillEffects"].is_array()
                    || !read_ids(d["skillEffects"], out.disabled_effects.skill_effects,
                                 "skillEffects", err)) {
                    return false;
                }
            }
            if (d.contains("soulMarks")) {
                if (!d["soulMarks"].is_array()
                    || !read_ids(d["soulMarks"], out.disabled_effects.soul_marks,
                                 "soulMarks", err)) {
                    return false;
                }
            }
        } else {
            err = "disabledEffects must be an array or object";
            return false;
        }
    }

    // 培养段（培养线，2026-09-26；可选）。present 即 cultivate=true（拟真合成模式，
    // "base" 不得同传——互斥校验在 PetFactory::create_numerical_base）。
    if (node.contains("cultivate")) {
        const auto& c = node["cultivate"];
        if (!c.is_object()) {
            err = "cultivate must be an object";
            return false;
        }
        CultivateConfig cfg;
        cfg.cultivate = true;
        if (c.contains("ev")) {
            const auto& ev = c["ev"];
            if (!ev.is_array() || ev.size() != kNumericCount) {
                err = "cultivate.ev must be an array of " + std::to_string(kNumericCount);
                return false;
            }
            for (int i = 0; i < kNumericCount; ++i) {
                if (!ev[static_cast<std::size_t>(i)].is_number_integer()) {
                    err = "cultivate.ev entries must be integers";
                    return false;
                }
                cfg.ev[static_cast<std::size_t>(i)] = ev[static_cast<std::size_t>(i)].get<int>();
            }
        }
        if (c.contains("natureId")) {
            if (!c["natureId"].is_number_integer()) {
                err = "cultivate.natureId must be an integer";
                return false;
            }
            cfg.nature_id = c["natureId"].get<int>();
        }
        if (c.contains("titleId")) {
            if (!c["titleId"].is_number_integer()) {
                err = "cultivate.titleId must be an integer";
                return false;
            }
            cfg.title_id = c["titleId"].get<int>();
        }
        if (c.contains("inscriptions")) {
            const auto& arr = c["inscriptions"];
            if (!arr.is_array() || arr.size() > 3) {
                err = "cultivate.inscriptions must be an array of at most 3 item ids";
                return false;
            }
            int slot = 0;
            for (const auto& it : arr) {
                if (!it.is_number_integer()) {
                    err = "cultivate.inscriptions entries must be integers";
                    return false;
                }
                cfg.inscriptions[static_cast<std::size_t>(slot++)] = it.get<int>();
            }
        }
        if (c.contains("hpCap20")) {
            if (!c["hpCap20"].is_boolean()) {
                err = "cultivate.hpCap20 must be a boolean";
                return false;
            }
            cfg.hp_cap20 = c["hpCap20"].get<bool>();
        }
        if (c.contains("guild")) {
            if (!c["guild"].is_boolean()) {
                err = "cultivate.guild must be a boolean";
                return false;
            }
            cfg.guild_boost = c["guild"].get<bool>();
        }
        if (c.contains("title")) {
            if (!c["title"].is_number_integer()) {
                err = "cultivate.title must be an integer (title_stats.id, 0=none)";
                return false;
            }
            cfg.title_id = c["title"].get<int>();
        }
        if (c.contains("annual")) {
            if (!c["annual"].is_boolean()) {
                err = "cultivate.annual must be a boolean";
                return false;
            }
            cfg.annual_bonus = c["annual"].get<bool>();
        }
        if (c.contains("resist")) {
            if (!parse_resist(c["resist"], cfg.resist, err)) {
                return false;
            }
        }
        out.cultivate_cfg = cfg;
    }
    return true;
}

// 阵容级 "resistDefault"："off"（缺省）/ "opened"（开启，伤害抗性 5/5/5）/
// "maxed_damage"（伤害抗性 35×3）。异常抗性槽需要指定抗哪种异常，无阵容级默认形态，
// 一律 per-pet 显式配置。返回 false = 文档带非法值。
bool parse_resist_default(const nlohmann::json& doc, ResistConfig& out, bool& has_default) {
    has_default = false;
    if (!doc.contains("resistDefault")) {
        return true;
    }
    const auto& v = doc["resistDefault"];
    if (!v.is_string()) {
        return false;
    }
    const std::string mode = v.get<std::string>();
    has_default = true;
    out = ResistConfig{};
    if (mode == "off") {
        return true;
    }
    out.enabled = true;
    if (mode == "opened") {
        out.damage = {5, 5, 5};
        return true;
    }
    if (mode == "maxed_damage") {
        out.damage = {35, 35, 35};
        return true;
    }
    has_default = false;
    return false;
}

// 一侧阵容。数组 = 逐只；对象 = 同一只重复 6 遍（调机制时的便捷写法）。
bool parse_party(const nlohmann::json& node, std::array<BattlePetMessage, kPartySize>& out,
                 std::string& err) {
    if (node.is_object()) {
        BattlePetMessage one;
        if (!parse_pet(node, one, err)) {
            return false;
        }
        for (int i = 0; i < kPartySize; ++i) {
            out[static_cast<std::size_t>(i)] = one;
        }
        return true;
    }
    if (!node.is_array()) {
        err = "side must be an array of pets or a single pet object";
        return false;
    }
    // 上场少于 6（2026-09-26）：接受 1~6 只，低位开始填充、空位垫底——
    // 官方允许少于 6 上场；空位恒 hp=0（fainted 不可选），0 号位必须为真实精灵。
    // 空位标记 pet_id=-1（0 是合法精灵 id，测试宠常用；负数才是非精灵）。
    if (node.size() > static_cast<std::size_t>(kPartySize)) {
        err = "side must contain at most " + std::to_string(kPartySize) + " pets, got " +
              std::to_string(node.size());
        return false;
    }
    if (node.empty()) {
        err = "side must contain at least 1 pet";
        return false;
    }
    for (std::size_t i = 0; i < node.size(); ++i) {
        if (!parse_pet(node[i], out[i], err)) {
            err = "side[" + std::to_string(i) + "]: " + err;
            return false;
        }
    }
    if (out[0].pet_id < 0) {
        err = "side[0] must be a real pet (empty slots go last)";
        return false;
    }
    // 低位填充校验：真实精灵（pet_id>=0）必须连续排在前面（官方口径：
    // 不会出现 0、3、5 这种排序）；空位槽统一标记 pet_id=-1。
    for (int i = 0; i < kPartySize; ++i) {
        if (out[static_cast<std::size_t>(i)].pet_id == 0) {
            out[static_cast<std::size_t>(i)].pet_id = -1;   // 0 = JSON 未提供，归一化为空位
        }
    }
    bool seen_empty = false;
    for (int i = 0; i < kPartySize; ++i) {
        if (out[static_cast<std::size_t>(i)].pet_id < 0) {
            seen_empty = true;
        } else if (seen_empty) {
            err = "side[" + std::to_string(i) +
                  "]: real pet after an empty slot (fill from low slots)";
            return false;
        }
    }
    return true;
}

// 阵容级 resistDefault 只填未显式带 resist 的精灵（party 节点为数组或单对象两种形态）。
void apply_resist_default(const nlohmann::json& doc, const nlohmann::json& party,
                          std::array<BattlePetMessage, kPartySize>& pets) {
    ResistConfig def;
    bool has_def = false;
    if (!parse_resist_default(doc, def, has_def) || !has_def) {
        return;   // 无/非法默认——非法值由调用方报错路径处理，这里不静默吞
    }
    const auto pet_node = [&](std::size_t i) -> const nlohmann::json* {
        if (party.is_array()) {
            return &party[i];
        }
        return &party;   // 单对象 = 同一只重复 6 遍
    };
    for (std::size_t i = 0; i < pets.size(); ++i) {
        const nlohmann::json* node = pet_node(i);
        const bool explicit_resist = node->contains("cultivate") &&
                                     (*node)["cultivate"].is_object() &&
                                     (*node)["cultivate"].contains("resist");
        if (!explicit_resist) {
            pets[i].cultivate_cfg.resist = def;
        }
    }
}

}  // namespace

bool decode_party_json(const std::string& text, std::array<BattlePetMessage, 6>& out,
                       std::string& err) {
    err.clear();
    nlohmann::json doc;
    try {
        doc = nlohmann::json::parse(text);
    } catch (const std::exception& ex) {
        err = std::string("invalid JSON: ") + ex.what();
        return false;
    }
    if (!doc.is_object() || !doc.contains("party")) {
        err = "expected an object with a \"party\" field";
        return false;
    }
    ResistConfig def;
    bool has_def = false;
    if (!parse_resist_default(doc, def, has_def)) {
        err = "resistDefault must be \"off\" / \"opened\" / \"maxed_damage\"";
        return false;
    }
    if (!parse_party(doc["party"], out, err)) {
        return false;
    }
    if (has_def) {
        apply_resist_default(doc, doc["party"], out);
    }
    return true;
}

bool decode_lineup_json(const std::string& text, BattleCreateRequest& out, std::string& err) {
    err.clear();
    nlohmann::json doc;
    try {
        doc = nlohmann::json::parse(text);
    } catch (const std::exception& ex) {
        err = std::string("invalid JSON: ") + ex.what();
        return false;
    }
    if (!doc.is_object()) {
        err = "expected a JSON object";
        return false;
    }
    if (!doc.contains("side1") || !doc.contains("side2")) {
        err = "expected \"side1\" and \"side2\"";
        return false;
    }
    ResistConfig def;
    bool has_def = false;
    if (!parse_resist_default(doc, def, has_def)) {
        err = "resistDefault must be \"off\" / \"opened\" / \"maxed_damage\"";
        return false;
    }
    if (!parse_party(doc["side1"], out.side1, err)) {
        err = std::string("side1: ") + err;
        return false;
    }
    if (!parse_party(doc["side2"], out.side2, err)) {
        err = std::string("side2: ") + err;
        return false;
    }
    // boss 挑战开关（2026-09-26 "boss 有效"线；可选）：顶层 "bossChallenge": true。
    if (doc.contains("bossChallenge")) {
        if (!doc["bossChallenge"].is_boolean()) {
            err = "\"bossChallenge\" must be a boolean";
            return false;
        }
        out.boss_challenge = doc["bossChallenge"].get<bool>();
    }

    if (doc.contains("pvp")) {
        if (!doc["pvp"].is_boolean()) {
            err = "\"pvp\" must be a boolean";
            return false;
        }
        out.pvp_battle = doc["pvp"].get<bool>();
    }

    if (has_def) {
        apply_resist_default(doc, doc["side1"], out.side1);
        apply_resist_default(doc, doc["side2"], out.side2);
    }
    // 可选药剂段（嗑药线，2026-09-24）："medicines": {"side1": {itemId: 数量, ...},
    // "side2": {...}}。item_id 必须在 battle_items 表（SeerRobot::use_medicine 查表结算），
    // 数量 ≥1。缺省 = 无药剂（legal.medicines 为空、前端嗑药区显示"无"）。
    if (doc.contains("medicines")) {
        const auto& meds = doc["medicines"];
        if (!meds.is_object()) {
            err = "\"medicines\" must be an object with \"side1\"/\"side2\" objects";
            return false;
        }
        const auto read_stock = [&](const char* key, MedicineStock& out) -> bool {
            if (!meds.contains(key)) {
                return true;
            }
            const auto& obj = meds[key];
            if (!obj.is_object()) {
                err = std::string("medicines.") + key + " must be an object of itemId: count";
                return false;
            }
            for (auto it = obj.begin(); it != obj.end(); ++it) {
                const int item_id = std::atoi(it.key().c_str());
                if (item_id <= 0 || !it.value().is_number_integer() ||
                    it.value().get<int>() <= 0) {
                    err = std::string("medicines.") + key +
                          " entries must be \"<itemId>\": <count>=1";
                    return false;
                }
                out[item_id] = it.value().get<int>();
            }
            return true;
        };
        if (!read_stock("side1", out.medicines[0]) || !read_stock("side2", out.medicines[1])) {
            return false;
        }
    }
    // 可选装备段（套装线，2026-09-19）："equip": {"side1": [itemId...], "side2": [...]}。
    // 缺省 = 不带装备；给出的数组元素必须是非负整数（部件 item_id）。
    // 待命背包（精灵王线）：可选 "standby": {"side1": [petId...], "side2": [...]}，各 ≤6。
    if (doc.contains("standby")) {
        const auto& standby = doc["standby"];
        if (!standby.is_object()) {
            err = "\"standby\" must be an object with \"side1\"/\"side2\" arrays";
            return false;
        }
        const auto read_ids = [&](const char* key, std::vector<int>& out) -> bool {
            if (!standby.contains(key)) {
                return true;
            }
            const auto& arr = standby[key];
            if (!arr.is_array() || arr.size() > 6) {
                err = std::string("standby.") + key + " must be an array of at most 6";
                return false;
            }
            for (const auto& v : arr) {
                if (!v.is_number_integer() || v.get<int>() <= 0) {
                    err = std::string("standby.") + key + " entries must be positive integers";
                    return false;
                }
                out.push_back(v.get<int>());
            }
            return true;
        };
        if (!read_ids("side1", out.standby_pet_ids[0])
            || !read_ids("side2", out.standby_pet_ids[1])) {
            return false;
        }
    }
    if (doc.contains("equip")) {
        const auto& equip = doc["equip"];
        if (!equip.is_object()) {
            err = "\"equip\" must be an object with \"side1\"/\"side2\" arrays";
            return false;
        }
        const auto parse_side = [&](const char* key, std::vector<int>& out) -> bool {
            if (!equip.contains(key)) {
                return true;
            }
            const auto& arr = equip[key];
            if (!arr.is_array()) {
                err = std::string("equip.") + key + " must be an array";
                return false;
            }
            for (const auto& v : arr) {
                if (!v.is_number_integer() || v.get<int>() <= 0) {
                    err = std::string("equip.") + key + " entries must be positive integers";
                    return false;
                }
                out.push_back(v.get<int>());
            }
            return true;
        };
        if (!parse_side("side1", out.equip_item_ids[0])) {
            return false;
        }
        if (!parse_side("side2", out.equip_item_ids[1])) {
            return false;
        }
    }
    return true;
}

}  // namespace server
