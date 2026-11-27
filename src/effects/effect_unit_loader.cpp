#include <effects/effect_unit_loader.h>

#include <nlohmann/json.hpp>

namespace {

using nlohmann::json;

const char* kPrimitiveTagNames[] = {
    "Anomaly",       // 0
    "StatChange",    // 1
    "Heal",          // 2
    "FixedDamage",   // 3
    "PpReduce",      // 4
    "RemoveRoundEffects", // 5
    "DrainHp",       // 6
    "Kill",          // 7
    "PowerBoost",    // 8
};

const char* kUnitConditionNames[] = {
    "None", "SameElement", "FirstMove", "SecondMove",
    "TargetNoAnomaly", "TargetHasAnomaly", "TargetHpBelow",
};

// 依名查表；未命中返回 -1（调用方给默认值）。
int enum_by_name(const char* const* names, int count, const json& value) {
    if (value.is_string()) {
        const std::string s = value.get<std::string>();
        for (int i = 0; i < count; ++i) {
            if (s == names[i]) {
                return i;
            }
        }
        return -1;
    }
    if (value.is_number_integer()) {
        return value.get<int>();
    }
    return -1;
}

// 解析整数字段：缺省→def；数字→字面；{"arg": n}→skill_args[n]。
int resolve_int(const json& j, const char* key, const std::vector<int>& skill_args, int def) {
    const auto it = j.find(key);
    if (it == j.end()) {
        return def;
    }
    if (it->is_number_integer()) {
        return it->get<int>();
    }
    if (it->is_object()) {
        const auto arg = it->find("arg");
        if (arg != it->end() && arg->is_number_integer()) {
            const int idx = arg->get<int>();
            if (idx >= 0 && static_cast<std::size_t>(idx) < skill_args.size()) {
                return skill_args[static_cast<std::size_t>(idx)];
            }
        }
    }
    return def;
}

void fill_unit_fields(const json& j, EffectUnit& u, const std::vector<int>& skill_args) {
    int t = enum_by_name(kPrimitiveTagNames, 9, j.value("tag", json("Anomaly")));
    if (t >= 0) {
        u.primary_tag = static_cast<PrimitiveTag>(t);
    }
    // actor/target: 数字字面量仅（相对 0/1）；不参与 skill_args 占位。
    if (j.contains("actor") && j["actor"].is_number_integer()) {
        u.actor = j["actor"].get<int>();
    }
    if (j.contains("target") && j["target"].is_number_integer()) {
        u.target = j["target"].get<int>();
    }
    u.param0 = resolve_int(j, "param0", skill_args, 0);
    u.param1 = resolve_int(j, "param1", skill_args, 0);
    u.chance_arg = resolve_int(j, "chance_arg", skill_args, -1);
    u.chance_value = resolve_int(j, "chance_value", skill_args, -1);
    int c = enum_by_name(kUnitConditionNames, 7, j.value("condition", json("None")));
    if (c >= 0) {
        u.condition = static_cast<UnitCondition>(c);
    }
    u.condition_param = resolve_int(j, "condition_param", skill_args, 0);
}

// 递归构建：先追加父节点，再构建分支并记录子节点下标。
int build(const json& j, const std::vector<int>& skill_args, std::vector<EffectUnit>& storage) {
    const int idx = static_cast<int>(storage.size());
    EffectUnit u;
    fill_unit_fields(j, u, skill_args);
    storage.push_back(u);

    auto link = [&](const char* key, int EffectUnit::*member) {
        const auto it = j.find(key);
        if (it != j.end() && it->is_object()) {
            const int child = build(*it, skill_args, storage);
            storage[idx].*member = child;
        }
    };
    link("on_success", &EffectUnit::on_success);
    link("on_immune", &EffectUnit::on_immune);
    link("on_blocked", &EffectUnit::on_blocked);
    link("on_other", &EffectUnit::on_other);
    return idx;
}

}  // namespace

int load_effect_unit_from_json(const std::string& unit_json, const std::vector<int>& skill_args,
                               std::vector<EffectUnit>& storage) {
    json j;
    try {
        j = json::parse(unit_json);
    } catch (const std::exception&) {
        return -1;  // JSON 语法错误
    }
    if (!j.is_object()) {
        return -1;
    }
    return build(j, skill_args, storage);
}