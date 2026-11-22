#ifndef UTILS_REGISTRATION_ARBITER_H
#define UTILS_REGISTRATION_ARBITER_H

#include <string>

namespace registration_arbiter {

// 注册表撞号仲裁（2026-11-22，方案 B——内核一劳永逸）。
//
// 背景：moves/soul/suit 三张函数注册表此前是 last-writer-wins（map[id] 直接覆盖），
// 而重复注册是备案存量模式（旧实验实现与新实现共存）。生效者取决于**静态初始化
// 顺序**——MinGW 的 .ctors 按链接序逆序执行、Mach-O/ELF 的 .init_array 按正序：
// 同一份代码，Windows 生效旧实现、Mac/Linux 生效新实现，行为静默分叉
// （2026-11-21/22 wuying 6 红实证，见 docs_local §9）。
//
// 规则：撞号时按「basename + 全路径 tie-break」的**稳定字典序**决定保留者——
// basename 与平台无关（绝对路径前缀不参与比较），任何平台/任何构建下
// 同一组注册来源必然仲裁出同一实现。方向固定为「key 较大者胜」。
// 无来源（source 空，手动/测试注册）永不覆盖已注册实现；已有无来源实现时，
// 带来源注册接管（对齐原 last-wins 语义）。

inline std::string arbitration_key(const std::string& source) {
    const auto pos = source.find_last_of("/\\");
    const std::string base =
        (pos == std::string::npos) ? source : source.substr(pos + 1);
    return base + '\x1f' + source;   // 主序 basename，同 basename 以全路径定序
}

// ⚠️ fn=nullptr 的注册是「显式运行期注销」语义（测试用它模拟"未注册"态，如 7921 的
// MissingMoves1470；也可能是插件动态卸载）——注销/恢复的覆盖豁免**由注册表调用方
// 处理**（写入方 fn==nullptr 无条件写；条目处于注销态时接受任何非空接管），
// 本函数只仲裁「两个非空实现」的归属。
inline bool should_replace(const std::string& existing_source,
                           const std::string& new_source) {
    if (new_source.empty()) {
        return false;   // 无来源注册不覆盖任何已有实现
    }
    if (existing_source.empty()) {
        return true;    // 已有实现无来源：带来源的接管
    }
    return arbitration_key(new_source) > arbitration_key(existing_source);
}

// 注册表条目：函数 + 来源（供撞号仲裁与审计）。
struct RegisteredFn {
    void* fn = nullptr;
    std::string source;
};

}  // namespace registration_arbiter

#endif  // UTILS_REGISTRATION_ARBITER_H
