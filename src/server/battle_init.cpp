#include <server/battle_init.h>

#include <nlohmann/json.hpp>

namespace server {
namespace {

constexpr int kPartySize = 6;
constexpr int kSkillCount = 5;
constexpr int kNumericCount = 6;

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
    return true;
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
    if (node.size() != kPartySize) {
        err = "side must contain exactly " + std::to_string(kPartySize) + " pets, got " +
              std::to_string(node.size());
        return false;
    }
    for (int i = 0; i < kPartySize; ++i) {
        if (!parse_pet(node[static_cast<std::size_t>(i)], out[static_cast<std::size_t>(i)], err)) {
            err = "side[" + std::to_string(i) + "]: " + err;
            return false;
        }
    }
    return true;
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
    return parse_party(doc["party"], out, err);
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
    if (!parse_party(doc["side1"], out.side1, err)) {
        err = std::string("side1: ") + err;
        return false;
    }
    if (!parse_party(doc["side2"], out.side2, err)) {
        err = std::string("side2: ") + err;
        return false;
    }
    return true;
}

}  // namespace server
