#ifndef EFFECT_UNIT_PARSER_H
#define EFFECT_UNIT_PARSER_H

#include <string>
#include <vector>

#include <effects/effect_unit.h>

// ⚠️【已废弃·保留在树】2026-11-05 用户裁决（S22 盘点后拍板）：运行时文本解析通道判定废弃，
// **不删代码**——留在树内作为「这条路存在」的活提醒（只靠 git 历史会遗忘），未来若使用条件
// 满足可直接翻新复用。盘点全文：docs_local/docs/02-效果系统/EffectUnit层现状盘点.md。
// 冻结纪律：禁止扩语法/新增 pattern——新效果一律走插件（SKILL_EFFECT）或
// custom_effect_programs 离线 JSON。现状：语法覆盖 0.7%（17/2380 模板）、运行时真实驱动
// 仅 1907 一条孤岛、危险面 83 模板/260 载体静默误执行（J 单「第四查」继续必查）。
// 真要删时：盘点 §6.2 有封存的迁移手册（1907 迁 JSON + 场景/文档改造清单）。

// 解析 effect_info 模板文本为条件效果单元（组合语法第二刀）。
//
// 第一刀范围：StatusInflict 主句（"{n}%令/使对手/自身{状态}"）+ 未触发兜底
// （递归异常 / 恢复 / 附加固定伤害）。其余模板返回 -1（不支持的跳过，维持现状）。
//
// skill_args 是该效果的参数列表（record.args，不含 build_effect_args_for_skill 加的
// [0,1] owner/target 前缀）；模板占位符 {n} → skill_args[n]，解析后烘成字面量（每技能实例化）。
//
// 所有单元先根后子追加到 storage，分支保存 storage 内的下标，不受扩容影响。
// 返回根单元在 storage 中的下标；失败返回 -1。
int parse_effect_unit(const std::string& info, const std::vector<int>& skill_args,
                      std::vector<EffectUnit>& storage);

#endif // EFFECT_UNIT_PARSER_H
