#ifndef COMMON_TRAIT_H
#define COMMON_TRAIT_H

#include <string>
#include <vector>

// 通用特性：任何精灵都可配置的通用能力（new_se 表 stat=1，50 种 × 等级 0-5）。
// 非怪物天生绑定，创建宠物时通过 BattlePetMessage.common_trait_id 指定。
// 数据层；效果层见 effects/trait_state.h（运行时槽）+ effects/common_trait_effects.h（行为）。
struct CommonTrait {
    int id = 0;               // new_se.idx 主键
    int effect_id = 0;        // new_se.effect_id（瞬杀=32/151——咤克斯/琉梦扫描"谁带瞬杀"的官方判据）
    std::string name;         // desc 列（特性名，如 瞬杀/精准/强袭/阴森）
    int star_level = 0;       // 等级 0-5
    std::string description;  // intro 列（人读描述）
    std::vector<int> args;    // args 列（效果参数）
};

#endif // COMMON_TRAIT_H
