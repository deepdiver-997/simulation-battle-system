// suit_lib 入口：仅保留 CoreApi 注入与套装注册 flush。
// 套装实现按套装分文件（yingyi_462.cpp = 影翼裁决）。
// 入口符号独立于 moves/soul 的 plugin_register（suit_plugin_register，见 suit_reg.h）——
// 装错目录的 dylib 不会被误注册，SuitManager 也只会装载带本符号的库。

#include "common.h"

// 动态库导出函数 - 套装插件注册入口（SuitManager 在加载动态库时 dlsym 本符号）
extern "C" void suit_plugin_register(void* api) {
    // core 传入 SuitPluginApi = core 原语指针 + 套装注册口；复制出 core（勿留栈指针）。
    suit_reg::SuitPluginApi* init = static_cast<suit_reg::SuitPluginApi*>(api);
    if (!init || !init->core) {
        return;  // core 注入异常：不判空直接解引用是理论崩溃点；静默放弃注册（日志由加载方负责）
    }
    verify_plugin_abi(init->core);  // 陈旧 dylib（与内核头不同源）当场 abort，防字段错位
    g_core = init->core;
    suit_reg::flush(api);
}
