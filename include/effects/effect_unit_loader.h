#ifndef EFFECT_UNIT_LOADER_H
#define EFFECT_UNIT_LOADER_H

#include <string>
#include <vector>

#include <effects/effect_unit.h>

// 认证数据层：从 JSON 加载离线编码的 EffectUnit 程序（替代运行时官方文本解析）。
//
// 与 parse_effect_unit 对等：先根后子追加到 storage，返回**根下标**（-1=加载失败）。
// 分支保存 storage 内的子单元下标，不受扩容影响。
// skill_args 用于把 JSON 里的 {"arg": n} 占位符解析成该效果记录的真实参数（与文本模板的 {n} 等价）。
//
// JSON schema（每行 custom_effect_programs.unit_json，一棵根节点树）：
// {
//   "tag": "Anomaly",            // PrimitiveTag 名(或数字下标)
//   "actor": 0, "target": 1,     // 相对发起方/目标(默认 0/1)
//   "param0": 0, "param1": 0,    // 原语参数；数字=字面；{"arg": n}=skill_args[n]
//   "chance_arg": -1, "chance_value": -1,
//   "condition": "SameElement",  // UnitCondition 名(或数字下标)
//   "condition_param": 0,
//   "on_success": {…}, "on_immune": {…}, "on_blocked": {…}, "on_other": {…}  // 可选嵌套子单元
// }
int load_effect_unit_from_json(const std::string& unit_json,
                               const std::vector<int>& skill_args,
                               std::vector<EffectUnit>& storage);

#endif // EFFECT_UNIT_LOADER_H