#ifndef ENTITIES_EFFECT_PARAM_H
#define ENTITIES_EFFECT_PARAM_H

#include <cstdint>
#include <vector>

// 词条参数覆盖（调试台"词条编辑"线，2026-09-26）。
//
// 官方效果的数值（概率/回合数/倍率…）不写在效果函数里，而是 DB 侧
// moves.side_effect_arg 扁平数组按 effect_info.args_num 切分后，经
// build_effect_args_for_skill 烤进 EffectArgs（效果函数读 args.int_args）。
// 本结构描述"把某技能某效果的某个参数改成某值"，装配期生效：
// Skills::loadSkills 在 rawEffectRecords 定型后先应用覆盖、再建效果，
// 因此所有读 args 的路径（注册分支/选择期/自定义程序）拿到的都是覆盖后数值。
//
// arg_index 与 effect_info.info 模板的 {n} 占位符同下标（0 起）；
// index 越界时尾部按 0 补齐（DB 省略的隐式尾参），效果不存在则装配报错。
struct EffectParamOverride {
    int skill_id = 0;   // 归属技能（0 = 不限，当前装配链按精灵逐槽过滤后必填）
    int effect_id = 0;  // 目标效果 id（side_effect 列表里的那条）
    int arg_index = 0;  // 模板 {n} 占位符下标
    int value = 0;      // 覆盖值（可为负，交给效果自己解释）
};

// 效果禁用（2026-09-26 效果禁用基建；"boss 有效"线第一步）。
//
// 装配期把指定效果从技能/魂印上整体摘除——被禁效果视为不存在（不走注册、
// 不进效果桶、不可触发）。用途两类：
//   · 手动实验：控制台效果明细弹窗按 id+描述勾选禁用（开局定死，重开生效）；
//   · boss 对局：开局自动预填 boss_invalid_effects.json 清单（官方 tips 尾注
//     "（boss无效）"解析，个别误报走 custom_effect_overrides 纠偏）。
// 技能侧按 effect_id 过滤 rawEffectRecords；魂印程序（SOUL_PROGRAM）节点无效果
// 号可寻址 → 粒度为整个魂印 id（官方"boss 无效"标注本就标注在魂印条款上，
// 与该粒度一致）；魂印单效果链路（旧 SOUL_MARK）按其 id（=效果号）过滤。
struct DisabledEffects {
    // 技能效果 id 集合（从 rawEffectRecords 摘除）
    std::vector<int> skill_effects;
    // 魂印 id 集合（整个魂印程序/单效果摘除 → 裸魂印）
    std::vector<int> soul_marks;
    bool empty() const { return skill_effects.empty() && soul_marks.empty(); }
};

#endif  // ENTITIES_EFFECT_PARAM_H
