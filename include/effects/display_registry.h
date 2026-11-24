#ifndef EFFECTS_DISPLAY_REGISTRY_H
#define EFFECTS_DISPLAY_REGISTRY_H

// 显示面注册表（效果显示gate规范 §4 G4，2026-11-23）。
//
// 管两类"显示身份"登记：
//   ① storage key：plugin_storage / soulmark_storage / on_stage_storage 里的 int 键
//      本来是插件自约定魔数（暴击充能 990238000+owner、蚩庸之锁 4676000+side…），
//      显示面上是裸键值黑盒。STATE_DISPLAY 宏（插件侧静态登记 → flush → 本表）给它们
//      名字与解码器；未登记 key 在 fullstate 以 registered:false 裸键导出（优雅降级）。
//   ② Mark 类型：mark id → 中文名/描述固定表（债务 #4）。Mark 的 name 字段是施加方
//      自传字符串（过渡期保留），表齐后退役。
//
// 撞号策略：同 key 二次登记 = **首注保留**，重复键记入 duplicates（dump 可见）。
// 引擎种子（内核侧 known keys，如暴击充能 990238000+owner——kernel 写入方在
// src/entities/skills.cpp，插件侧无从登记）在本 .cpp 静态初始化。

#include <map>
#include <set>
#include <string>

#include <plugin/plugin_interface.h>   // StorageDisplaySpec / MarkTypeSpec / DecodedValue

namespace display_reg {

class DisplayRegistry {
public:
    static DisplayRegistry& instance() {
        static DisplayRegistry reg;
        return reg;
    }

    // 注册（首注保留）。返回 false = 该 key/id 已有登记（首注保留，本次被忽略）。
    bool register_storage(const StorageDisplaySpec& spec) {
        const auto it = storage_.find(spec.key);
        if (it != storage_.end()) {
            duplicate_keys_.insert(spec.key);
            return false;
        }
        storage_.emplace(spec.key, spec);
        return true;
    }
    bool register_mark_type(const MarkTypeSpec& spec) {
        const auto it = mark_types_.find(spec.id);
        if (it != mark_types_.end()) {
            duplicate_mark_ids_.insert(spec.id);
            return false;
        }
        mark_types_.emplace(spec.id, spec);
        return true;
    }

    const StorageDisplaySpec* find_storage(int key) const {
        const auto it = storage_.find(key);
        return it == storage_.end() ? nullptr : &it->second;
    }
    const MarkTypeSpec* find_mark_type(int id) const {
        const auto it = mark_types_.find(id);
        return it == mark_types_.end() ? nullptr : &it->second;
    }

    // 通用解码（spec.decode == nullptr 时按 kind）：int/bool/str。
    // ⚠️ 只对**内核 TU 写入**的值安全（如暴击充能 990238000+owner）：std::any 的
    // type_info/manager 各 TU 独立实例，跨 dylib any_cast 必抛 bad_any_cast——
    // 插件写入的 key 一律用 STATE_DISPLAY_FN（插件侧解码器，同 dylib 读写才统一）。
    // 解码失败返回 false，调用方显示 "?"。
    static bool decode_generic(const StorageDisplaySpec& spec, const std::any& stored,
                               DecodedValue& out) {
        try {
            if (spec.kind == "int") {
                std::snprintf(out.text, sizeof(out.text), "%d", std::any_cast<int>(stored));
                return true;
            }
            if (spec.kind == "bool") {
                std::snprintf(out.text, sizeof(out.text), "%s",
                              std::any_cast<bool>(stored) ? "true" : "false");
                return true;
            }
            if (spec.kind == "str") {
                const auto s = std::any_cast<std::string>(stored);
                std::snprintf(out.text, sizeof(out.text), "%.*s",
                              static_cast<int>(s.size()), s.c_str());
                return true;
            }
        } catch (const std::bad_any_cast&) {
            return false;
        }
        return false;
    }

    // registry dump 的 display 段（build_registry_dump 消费；规范 §3 registry 出口）。
    // 只出原始登记维度（key/name/kind/id/name/des），解码器不进 dump（函数不可序列化）。
    std::string display_section_json() const;

private:
    DisplayRegistry() = default;
    std::map<int, StorageDisplaySpec> storage_;      // key → 显示身份（首注保留）
    std::map<int, MarkTypeSpec> mark_types_;         // mark id → 描述固定表
    std::set<int> duplicate_keys_;                   // 重复登记的 key（dump 可见，审计用）
    std::set<int> duplicate_mark_ids_;
};

}  // namespace display_reg

#endif  // EFFECTS_DISPLAY_REGISTRY_H
