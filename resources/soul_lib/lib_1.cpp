// soul_lib 入口：仅保留 CoreApi 注入与插件注册。
// 魂印实现按机制家族拆在同目录各 .cpp（分族见各文件头注释）。
// 拆分不变式：SOUL_MARK + SOUL_PROGRAM 注册总数**只增不改**（2026-09-20 晚烧伤三精灵线：
//             5 + 27 = 32，新增 希拓 2217 / 绮影罗心·织焰 2300 / 永烬劫炎·上古炎兽 2481
//             三条 SOUL_PROGRAM），全量场景回归守护。

#include "common.h"

// 动态库导出函数 - 魂印插件注册入口
// 该函数由 SoulMarkManager 在加载动态库时调用
extern "C" void soulmark_plugin_register(void* api) {
    // core 传入 PluginInitApi = 注册表 + core 原语函数指针；复制出 core（勿留栈指针）。
    PluginInitApi* init = static_cast<PluginInitApi*>(api);
    if (!init || !init->core || !init->registry) {
        return;  // core 注入异常：不判空直接解引用是理论崩溃点；静默放弃注册（日志由加载方负责）
    }
    verify_plugin_abi(init->core);  // 陈旧 dylib（与内核头不同源）当场 abort，防字段错位
    g_core = init->core;
    plugin_reg::flush(init->registry);
}
