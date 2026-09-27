#ifndef ELEMENTAL_ATTRIBUTES_H
#define ELEMENTAL_ATTRIBUTES_H

#include <iostream>
#include <memory>
#include <string>
#include <vector>

// #include <db/db_service.h>

//元素克制关系
class ElementalAttributes {
public:
    // static 
    ElementalAttributes();
    ~ElementalAttributes();
    void loadElementalAttributes();

// private:
    static std::vector<std::pair<std::string, int> > elementalAttributes;
    // 克制倍率矩阵：存官方原始倍率（0=免疫 / 0.5=减半 / 1.0=普通 / 2.0=克制）。
    // 用 double 而非 int 三档编码——0.5 减半语义必须保留（旧编码把 0.5 丢成 0=免疫）。
    static std::vector<std::vector<double> > elementalAttributesRestraints;
};

#endif // ELEMENTAL_ATTRIBUTES_H