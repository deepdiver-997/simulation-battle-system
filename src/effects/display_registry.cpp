// 显示面注册表实现（include/effects/display_registry.h）：
// display 段 JSON 组装 + 内核种子（kernel 自写入方与 Mark 0 的显示身份）。

#include <effects/display_registry.h>

#include <cstdio>
#include <string>

#include <server/output_json.h>   // server::json_escape

namespace display_reg {

std::string DisplayRegistry::display_section_json() const {
    auto escape = [](const std::string& s) { return server::json_escape(s); };
    std::string out = "{\"storageKeys\":[";
    bool first = true;
    for (const auto& [key, spec] : storage_) {
        if (!first) {
            out += ',';
        }
        first = false;
        out += "{\"key\":" + std::to_string(key)
             + ",\"name\":\"" + escape(spec.name_zh)
             + "\",\"kind\":\"" + escape(spec.kind) + "\"}";
    }
    out += "],\"markTypes\":[";
    first = true;
    for (const auto& [id, spec] : mark_types_) {
        if (!first) {
            out += ',';
        }
        first = false;
        out += "{\"id\":" + std::to_string(id)
             + ",\"name\":\"" + escape(spec.name_zh)
             + "\",\"des\":\"" + escape(spec.des) + "\"}";
    }
    out += "],\"duplicateKeys\":[";
    first = true;
    for (const int key : duplicate_keys_) {
        if (!first) {
            out += ',';
        }
        first = false;
        out += std::to_string(key);
    }
    out += "]}";
    return out;
}

}  // namespace display_reg

// ── 内核种子（2026-11-23）────────────────────────────────────────────────
// 内核自己是写入方的 key 无法经插件登记（STATE_DISPLAY 是插件侧宏）——静态初始化补：
//   · 暴击充能 990238000+owner（src/entities/skills.cpp:992 写、暴击落地消费）
//   · Mark 0 异常免疫（include/entities/mark.h 文档语义——免控/魂免标记；
//     蚩庸之锁同用 id=0 但走 name 键控，markTypes 只收内核固定语义）
namespace {
const bool kKernelDisplaySeed = [] {
    auto& reg = display_reg::DisplayRegistry::instance();
    reg.register_storage({990238000, "暴击充能(方0)", "int", nullptr,
                          "内核种子 display_registry.cpp（写入方 skills.cpp:992）"});
    reg.register_storage({990238001, "暴击充能(方1)", "int", nullptr,
                          "内核种子 display_registry.cpp（写入方 skills.cpp:992）"});
    reg.register_mark_type({0, "异常免疫", "免控/魂免标记（Mark 0，条件魂免按 count 倒计时）",
                            "内核种子 display_registry.cpp（include/entities/mark.h）"});
    return true;
}();
}  // namespace
