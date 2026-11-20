#ifndef ELEMENTAL_ATTRIBUTES_H
#define ELEMENTAL_ATTRIBUTES_H

#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// #include <db/db_service.h>

//元素克制关系
class ElementalAttributes {
public:
    // static
    ElementalAttributes();
    ~ElementalAttributes();
    void loadElementalAttributes();

    // ── 复合属性映射层（J322，2026-11-05）─────────────────────────────
    // type_id → 组件单属性 id 对（主, 次；单属性行次槽 0）。启动时从官方库
    // skill_types 全量拷入（138 行 = 26 单 + 112 复合）。
    static std::unordered_map<int, std::pair<int, int> > typeComponents;

    // 查一个 type_id 的组件对。语义：
    //   · id == 0 → {0,0}（空槽哨兵，合法值不记账）
    //   · 映射命中 → 表值（单属性 {id,0} / 复合分解对）
    //   · miss（未知 id）→ {id,0}（退化单属性 = J322 前的旧行为）+
    //     type_audit 记账（去重，启动统一响亮报出）
    static std::pair<int, int> componentsOf(int type_id);

    // 克制矩阵查表封装（越界防护）：id 越界（<0 或 ≥矩阵维度）→ 1.0 + 记账。
    static double restraintCell(int attacker_id, int defender_id);

// private:
    static std::vector<std::pair<std::string, int> > elementalAttributes;
    // 克制倍率矩阵：存官方原始倍率（0=免疫 / 0.5=减半 / 1.0=普通 / 2.0=克制）。
    // 用 double 而非 int 三档编码——0.5 减半语义必须保留（旧编码把 0.5 丢成 0=免疫）。
    static std::vector<std::vector<double> > elementalAttributesRestraints;
};

#endif // ELEMENTAL_ATTRIBUTES_H