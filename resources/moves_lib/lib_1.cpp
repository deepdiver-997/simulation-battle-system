// moves_lib 入口：仅保留 CoreApi 注入与插件注册。
// 效果实现按语义家族拆在同目录各 .cpp（分族见各文件头注释）。
// 拆分不变式：SKILL_EFFECT 注册总数恒等（拆分时 = 93 行），全量场景回归守护。

#include "common.h"


// 动态库导出函数 - 技能插件注册入口
// 该函数由 EffectFactory 在加载动态库时调用
extern "C" void skill_plugin_register(void* api) {
    PluginInitApi* init = static_cast<PluginInitApi*>(api);
    if (!init || !init->core || !init->registry) {
        return;  // core 注入异常：不判空直接解引用是理论崩溃点；静默放弃注册（日志由加载方负责）
    }
    verify_plugin_abi(init->core);  // 陈旧 dylib（与内核头不同源）当场 abort，防字段错位
    g_core = init->core;
    plugin_reg::flush(init->registry);
}
