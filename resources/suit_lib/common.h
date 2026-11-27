#ifndef SUIT_LIB_COMMON_H
#define SUIT_LIB_COMMON_H

// suit_lib 共享助手（套装线，2026-09-19）。结构仿 soul_lib/common.h：
// g_core 是 **inline 变量**（同一 .so 内所有编译单元共享一份）；helpers 全部 inline。
//
// 套装效果的统一纪律（用户 2026-09-19 口径）：
//   · 生效范围**全队**——per-side 查询，不锚具体精灵槽；
//   · **无法被任何效果抹除或者屏蔽**——条目由激活路径展开为 TEAM 常驻
//     （suit.h::Suit::register_suit_effect），插件侧不要再注册 ON_STAGE/
//     回合类条目，也不要把可被抑制的类别（BLOCK/DETECT 等）当常驻规则用；
//   · 套装不是回合类效果：断回合（break_round_effects）对它天然无效。

#include <effects/effect.h>
#include <effects/event_center.h>
#include <effects/damage_pipeline.h>
#include <effects/pink_damage_pipeline.h>
#include <fsm/battleContext.h>
#include <plugin/plugin_interface.h>
#include <plugin/suit_reg.h>
#include <plugin/core_api.h>
#include <abnormal-system/abnormal-types.h>

#include <cstdlib>
#include <vector>
#include <any>
// ── storage 安全读取（2026-10-02 全库 any_cast 治理，与 moves/soul common.h 同款）──
// 抛式 std::any_cast<int>(any) 类型不符时炸进程（fuzz any_cast_fault 家族）；
// storage 是共享键空间，类型不符 = "不是你的" → 按缺失语义回退，取值形式全库禁用。
inline int any_int_or(const std::any& a, int fallback = 0) {
    if (const int* p = std::any_cast<int>(&a)) {
        return *p;
    }
    return fallback;
}


// core 在插件初始化时传入的原语函数指针（依赖倒置：走真原语、不链接 core 符号）。
inline const CoreApi* g_core = nullptr;

inline int resolve_owner_from_args(BattleContext* ctx, const EffectArgs& args) {
    if (args.int_args && args.int_count >= 1) {
        const int owner = args.int_args[0];
        if (owner == 0 || owner == 1) {
            return owner;
        }
    }
    return 0;
}

// 当前在场的"持有方"精灵（套装全队效果大多以在场精灵为载体）。
inline ElfPet& on_stage_pet(BattleContext* ctx, int side) {
    return ctx->seerRobot[side].elfPets[ctx->on_stage[side]];
}

// 精灵体力上限（同 soul_lib 语义：本体优先，退化到当前数值槽）。
inline int resolve_pet_max_hp(const ElfPet& pet) {
    int max_hp = pet.numericalBase[NumericalPropertyIndex::HP];
    if (max_hp <= 0) {
        max_hp = pet.numericalProperties[NumericalPropertyIndex::HP];
    }
    if (max_hp <= 0) {
        max_hp = pet.hp;
    }
    return max_hp;
}

#endif  // SUIT_LIB_COMMON_H
