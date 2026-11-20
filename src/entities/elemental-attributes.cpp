#include <entities/elemental-attributes.h>

#include <db/official_data_repository.h>
#include <db/type_audit.h>

std::vector<std::pair<std::string, int>> ElementalAttributes::elementalAttributes;
std::vector<std::vector<double>> ElementalAttributes::elementalAttributesRestraints(256, std::vector<double>(256, 1.0));
// J322 复合属性映射：type_id → 组件单属性 id 对。启动时全量拷入（见 loadElementalAttributes）。
std::unordered_map<int, std::pair<int, int>> ElementalAttributes::typeComponents;

ElementalAttributes::ElementalAttributes() = default;

ElementalAttributes::~ElementalAttributes() = default;

void ElementalAttributes::loadElementalAttributes() {
    // 克制表来自官方 types_relation（新 Unity 数据，H5 缺失）。
    // 单属性 id 最大 226（混沌/虚空等），矩阵取 256 保证下标不越界。
    elementalAttributesRestraints.assign(256, std::vector<double>(256, 1.0));

    auto& store = official_data::OfficialDataStore::instance();
    if (!store.ready()) {
        return;
    }
    store.repository().load_elemental_restraints(elementalAttributesRestraints);
    // J322：全量拷入复合属性映射（skill_types 138 行 = 26 单属性 + 112 复合，
    // 仓库侧 ensure_type_components_cache 已做未知引用记账）。拷贝而非每次查仓储
    // ——克制聚合/本系判定在伤害热路径上，map 拷贝一次、之后只读。
    typeComponents = store.repository().type_components();
}

std::pair<int, int> ElementalAttributes::componentsOf(int type_id) {
    if (type_id == 0) {
        return {0, 0};   // 空槽哨兵（make_pet / 空位精灵），合法值
    }
    const auto it = typeComponents.find(type_id);
    if (it != typeComponents.end()) {
        return it->second;
    }
    // 未知 id：退化单属性（= J322 前对未知 id 的旧行为）+ 记账（账本按
    // (origin,id) 去重，不会随伤害调用刷屏；启动统一响亮报出）。
    type_audit::instance().record(
        "runtime.componentsOf", type_id, "",
        "运行时查映射表 miss：type_id=" + std::to_string(type_id) +
            " 不在 skill_types 映射里（数据未被启动审计覆盖，或映射尚未加载）",
        type_audit::source_of(__FILE__, __LINE__));
    return {type_id, 0};
}

double ElementalAttributes::restraintCell(int attacker_id, int defender_id) {
    const int rows = static_cast<int>(elementalAttributesRestraints.size());
    const bool in_bounds =
        attacker_id >= 0 && defender_id >= 0 && attacker_id < rows && defender_id < rows;
    if (in_bounds) {
        return elementalAttributesRestraints[static_cast<std::size_t>(attacker_id)]
                                            [static_cast<std::size_t>(defender_id)];
    }
    // 越界：缺省 1.0（J322 前的矩阵语义）+ 记账响亮报出。
    type_audit::instance().record(
        "runtime.restraintCell", attacker_id, "",
        "克制矩阵查表越界：attacker_id=" + std::to_string(attacker_id) +
            " defender_id=" + std::to_string(defender_id) + "（矩阵维度 " +
            std::to_string(rows) + "×" + std::to_string(rows) + "）按缺省 1.0 处理",
        type_audit::source_of(__FILE__, __LINE__));
    return 1.0;
}
