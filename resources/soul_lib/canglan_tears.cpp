// 沧岚泪族（1004 挂泪 / 1005 死亡清泪 / 1006 朝曦操作改写）
//
// （2026-09-17 从 lib_1.cpp 按家族拆分；代码零改动，行为由全量场景回归守护。）
//
// ⚠️ 2026-10-21 方案 B 迁移**豁免**（本文件不迁 a.row_or）：
//    这里的 1004/1005/1006 是**手写魂印 id**，不对应 effect_icon.effect_id：
//    DB 里 effect_id=1004 是「帝皇之钺 3404」（args `1 50 300 300 1 1`），与
//    「蚀砚之泪」毫无关系；"泪"的真身是 effect 2343（怒涛·沧岚 3740, args=`1 1 1`，
//    tips「为对手附加1滴蚀砚之泪」）——**官方是"每次 1 滴"**，而本行的 stacks=4 是
//    旧链路的**内部封挡阈值**（kEternalTearSealThreshold=4，≥4 滴才置
//    BLOCK|DETECT 掩码）。迁到 db_at 会把 4 滴读成 DB args[0]（=1）→ 每次只挂 1 滴
//    → 封挡阈值永不达成 = **行为回归**。故保留 a.row_or(0, 4) 并在此记档。
//    （若日后要数据驱动化"泪滴数"，需重新建模为"挂 N 滴 = 多次挂 1 滴"。）
//    （S19 复核 2026-11-05：全库 row_or 现存仅本文件与 legacy_guards.cpp 两处，
//    均属手写魂印 id 豁免范围，维持不迁——迁 db_at 会触发头注所述行为回归。）
#include "common.h"

namespace {


// ── 朝曦 1006 专用常量（拆分时从 prelude 移到本族）──
// 演示用常量；后续可改为 effect_icon 数据驱动。
constexpr int kZhaoxiPets[] = {4853, 1400786};
bool is_valid_slot(int slot) {
    return slot >= 0 && slot < 4;
}

EffectResult soulmark_canglan_apply_eternal_tears(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }

    const int owner = a.owner;
    const int target = a.opp;
    const int stacks = a.row_or(0, 4);
    ElfPet& defender = ctx->seerRobot[target].elfPets[ctx->on_stage[target]];

    add_mark_stacks(defender, kMarkNameEternalTear, stacks);
    sync_eternal_tear_suppress_mask(defender);  // ≥4 滴 → 置 BLOCK|DETECT 掩码（挡伤失效）
    return EffectResult::kOk;
}

EffectResult soulmark_canglan_on_death_cleanup_tears(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }

    const int owner = resolve_owner_from_args(ctx, args);
    const int target = 1 - owner;
    clear_mark_from_all_pets(ctx->seerRobot[target], kMarkNameEternalTear);
    return EffectResult::kOk;
}

/**
 * 魂印 9901006（原 1006）- 朝曦（梅赫维特）：主动切换时当回合选择并使用自身1号位技能。
 *
 * 常规操作中 CHOOSE_PET = 放弃出手权（should_skip_action_flow 直接跳过）。
 * 本魂印在 OPERATION_ENTER_EXIT_STAGE 时点检查操作记录：
 *   - roundChoice[self][0] == CHOOSE_PET（0）
 *   - 目标精灵是梅赫维特（kZhaoxiPets）
 * 满足则把 roundChoice 改写为 (SELECT_SKILL, 0)，使先手权比较和出手流程正常继续。
 *
 * 切换动作本身由 OPERATION_ENTER_EXIT_STAGE 的换宠流程完成；这里只改写操作记录。
 * Args: [owner(0), target(1)]
 */
EffectResult soulmark_zhaoxi_operation_rewrite(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }

    // ActionType 枚举值：CHOOSE_PET = 0, SELECT_SKILL = 1
    constexpr int kActionChoosePet = 0;
    constexpr int kActionSelectSkill = 1;

    // 只有选择了"切换精灵"才可能被朝曦接管
    if (ctx->roundChoice[owner][0] != kActionChoosePet) {
        return EffectResult::kOk;
    }

    const int target_slot = ctx->roundChoice[owner][1];
    if (target_slot < 0 || target_slot >= 6) {
        return EffectResult::kOk;
    }
    const ElfPet& target_pet = ctx->seerRobot[owner].elfPets[target_slot];
    if (target_pet.hp <= 0) {
        return EffectResult::kOk;
    }

    bool is_zhaoxi_pet = false;
    for (int id : kZhaoxiPets) {
        if (target_pet.id == id) {
            is_zhaoxi_pet = true;
            break;
        }
    }
    if (!is_zhaoxi_pet) {
        return EffectResult::kOk;
    }

    // 改写操作记录：从"切换"变为"使用1号位技能"（slot 0）
    ctx->roundChoice[owner][0] = kActionSelectSkill;
    ctx->roundChoice[owner][1] = 0;
    return EffectResult::kOk;
}

// 无为觉者魂印（effect_icon 1862 / effect_id 2260）真实文本（2026-08-22 从 effect_icon 读取）：
//   "战斗开始时，自身所有能力值变为所有能力值之和的一半；自身使用技能不受PP值限制且
//    使用之后的PP值消耗改为反转当前PP值与已损失PP值 |
//    自身使用PP值不为0的技能时解除所处的异常状态且免疫下次受到的异常状态，
//    使用的PP值为0的技能时必定命中、强制执行命中效果且使用后触发万相乖离，
//    自身未使用技能的回合结束，根据满足的条件触发技能中的无为效果 |
//    万相乖离：永久取消第五技能首条未取消过的效果中的触发条件并提升50点威力，
//    自身击败对手后，重置万相乖离并令对方全部阵亡精灵消逝 |（boss无效）"
// 程序模型节点拆分（子句 → 时点）：
//   ① 能力减半      → OPERATION_ENTER_EXIT_STAGE（战斗开始）
//   ② 不受PP+PP反转  → BATTLE_ROUND_START 信号（early 置位早于首轮技能选择）
//   ③ PP≠0 解异常+免疫 / PP=0 强制 → BATTLE_FIRST/SECOND_ON_SKILL_HIT
//   ④ 万相乖离/无为/击败重置 → 依赖 effect-unit 条件取消机制，留 TODO

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表）──
    // ⚠️ 私有号段（2026-10-21 撞号治理）：原 1004/1005/1006 手写 id 与官方行
    //    （1004=帝皇之钺3404、1005=3403、1006=3408）撞号——pet_factory 按官方行造宠
    //    会误触发本文件程序。已迁 9901xxx；泪系统现役走 nutao_runtime 的
    //    plugin_storage 计数（canglan_pink 2343），本组注册为旧链路保留。
    //    官方 1004-1006 语义未实现（启动审计呈现）。
    SOUL_MARK_TYPED(9901004, soulmark_canglan_apply_eternal_tears);
    SOUL_MARK(9901005, soulmark_canglan_on_death_cleanup_tears);
    SOUL_MARK(9901006, soulmark_zhaoxi_operation_rewrite);  // 朝曦：切换仍用1号位

}  // namespace
