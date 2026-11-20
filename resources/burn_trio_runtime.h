#ifndef BURN_TRIO_RUNTIME_H
#define BURN_TRIO_RUNTIME_H

// 烧伤三精灵（希拓 4622 / 绮影罗心·织焰 4696 / 永烬劫炎·上古炎兽 4875）运行时 ——
// 魂印侧（soul_lib/xitao_4622.cpp、zhiyan_4696.cpp、yanshou_4875.cpp）与技能侧
// （moves_lib/burn_trio_skills.cpp）共享的键约定 + 状态存取。
// 放独立头：场景测试 097 也要按同一类型读盘面。
//
// ⚠️ 跨 dylib 纪律（crawford_runtime.h 教训）：**跨 soul_lib / moves_lib 读写的一律
//   fundamental type（int）**；自定义 struct 只能单侧 any_cast。本头全部用 int 槽。
//
// 官方魂印（4399 图鉴问答 / effect_icon 2217 / 2300 / 2481）：
//   希拓 2217：回合开始时100%令对手烧伤；对手处于烧伤状态时无法恢复体力且每回合结束后
//     受到战意层数*10的真实伤害；回合结束后获得2层战意，己方场下每有1只系别中带有火系
//     的精灵层数+1，战意达到10层时获得命运庇护效果(层数下场保留，最多持有20层)；
//     战意：每持有一层受到的固定伤害、百分比伤害减少4%，造成的攻击伤害提升5%；
//     命运庇护：所有异常抗性必定触发，失去全免抗性(boss无效)。
//   织焰 2300：对手使用技能后，若双方任意一方处于烧伤状态则与对手交换体力值百分比；
//     回合结束后，若双方任意一方不处于烧伤状态，则自身恢复已损失体力值的30%(boss无效)。
//   上古炎兽 2481：战斗阶段结束时，若对手处于烧伤状态则获得3回合的神惧劫炎，自身已处于
//     神惧劫炎时改为令其回合数+1，若对手不处于烧伤状态则恢复自身最大体力的1/3并附加
//     恢复量的百分比伤害，然后若此伤害未减少对手体力则自身额外获得最大体力1/3的护盾；
//     对手使用攻击技能时令对手烧伤，未触发则消除对手回合类效果，然后若自身不处于烧伤
//     则自身下1次进入的异常状态转化为2回合的烧伤；自身存活于出战背包时，对手免疫能力
//     下降状态时视为处于烧伤状态（→ include/effects/burn_perception.h 虚拟烧伤）；
//     神惧劫炎：回合结束后，若自身存在技能已损失PP高于对手对应位置的技能则将其已损失PP
//     调整至与自身相同;自身先出手时效果回合数-1且对手的烧伤回合数+1;自身所有技能先制+2
//     且造成的攻击伤害提升100%;最高8回合。
//
// 口径备注（2026-09-20）：
//   ★ 魂印里的"处于烧伤状态"一律走 burn_perception.h 的 is_burn_effective（虚拟也算）；
//     上古炎兽魂印自己的"若对手处于烧伤状态→神惧劫炎"同为检测类 → 虚拟也算（免弱对手
//     反过来喂神惧劫炎，正是这条魂印的反制设计）。只有**烧伤→焚烬转化**（effect 572 等）
//     查 is_real_burn。
//   ★ 神惧劫炎是**在场期**增益：存 on_stage_storage（换宠清空）。回合数模型 = 剩余回合数
//     int：回合结束桶内按"先结算（PP调整/先手衰减）→ 自然-1 → 本桶获得（+3/+1，封顶8）"
//     定序，保证"获得3回合"字面成立。
//   ★ "每次使用额外附加100点"（秘纹护体 837）的累计档存 soulmark_storage（下场保留）。
#include <any>
#include <map>

#include <abnormal-system/abnormal-types.h>
#include <abnormal-system/resistance-system.h>
#include <entities/elf-pet.h>
#include <fsm/battleContext.h>

// ── 精灵 / 魂印 id ──
constexpr int kXitaoPetId = 4622;
constexpr int kXitaoMarkId = 2217;
constexpr int kZhiyanPetId = 4696;
constexpr int kZhiyanMarkId = 2300;
constexpr int kYanshouPetId = 4875;
constexpr int kYanshouMarkId = 2481;

// ── ElfPet::soulmark_storage（下场保留；int 槽）──
constexpr int kXitaoZhanYiStacksKey = 2217;   // 战意层数（cap 20；官方"层数下场保留"）
constexpr int kXitaoDestinyGuardKey = 2218;   // 命运庇护已获得旗（0/1；阈值一次性）

// ── ElfPet::on_stage_storage（本次上场；int 槽）──
constexpr int kYanshouShenjuRoundsKey = 2481; // 神惧劫炎剩余回合数（0 = 未持有）
constexpr int kYanshouFirstMoverRoundKey = 2482; // 本回合先出手过的回合号（-1 = 无）
// 秘纹护体吸取递增档（837 "每次使用额外附加100点，最高500点"）：**只在场上保留**
// ——换宠/下场由引擎清 on_stage_storage 即归零，重新上场从头（100 起步）累计。
constexpr int kMiwenDrainBonusKey = 8370;

// ── BattleContext::plugin_storage（int 槽）──
// 管线接线守卫（每场一次，键互不相同）：
constexpr int kXitaoPipelineWiredKey = 462201;
constexpr int kYanshouPipelineWiredKey = 487501;
// 希拓锁疗票句柄（HEAL_BLOCK 条件票，TEAM 域 + on_exit 撤销）：+owner → source_id
constexpr int kXitaoHealBlockTicketBase = 221700;
// 织焰护罩破碎监听只挂一次旗（+owner → 1）
constexpr int kZhiyanHoodWatcherKey = 469601;
// 乱云烟锁烬 1853 破罩真伤"已发过"旗（+owner → 1）
constexpr int kZhiyanHoodBreakFiredKey = 469602;

// ── 数值常量 ──
constexpr int kZhanYiCap = 20;              // 战意上限
constexpr int kZhanYiDestinyThreshold = 10; // 命运庇护阈值
constexpr int kShenjuMaxRounds = 8;         // 神惧劫炎回合上限
constexpr int kElementFire = 3;             // skill_types：火（单属性 id）

// ── 战意（希拓，仅 soul 侧读写）────────────────────────────
inline int xitao_zhanyi_stacks(const ElfPet& pet) {
    auto it = pet.soulmark_storage.find(kXitaoZhanYiStacksKey);
    return it == pet.soulmark_storage.end() ? 0 : std::any_cast<int>(it->second);
}

inline void xitao_set_zhanyi_stacks(ElfPet& pet, int stacks) {
    if (stacks < 0) {
        stacks = 0;
    }
    if (stacks > kZhanYiCap) {
        stacks = kZhanYiCap;
    }
    pet.soulmark_storage[kXitaoZhanYiStacksKey] = stacks;
}

inline bool xitao_has_destiny_guard(const ElfPet& pet) {
    auto it = pet.soulmark_storage.find(kXitaoDestinyGuardKey);
    return it != pet.soulmark_storage.end() && std::any_cast<int>(it->second) != 0;
}

inline void xitao_set_destiny_guard(ElfPet& pet) {
    pet.soulmark_storage[kXitaoDestinyGuardKey] = 1;
}

// 命运庇护落笔：所有**已配置**的抗性槽 → 必定触发（100%），全免抗性清零。
// 一次性（达阈值时调用）；抗性数据长在 pet 上，改写即持续。
// （"boss无效"：引擎暂无 boss 概念，不设例外——待有 boss 标记后再补。）
inline void xitao_apply_destiny_guard(ElfPet& pet) {
    ResistanceSystem& res = pet.resistance;
    for (int i = 0; i < kResistanceSlotCount; ++i) {
        const auto ctrl = res.getControlSlot(i);
        if (ctrl.second > 0 && ctrl.second < 100) {
            res.setControlSlot(i, ctrl.first, 100);
        }
        const auto unctrl = res.getUncontrolSlot(i);
        if (unctrl.second > 0 && unctrl.second < 100) {
            res.setUncontrolSlot(i, unctrl.first, 100);
        }
    }
    res.setCompleteImmunity(0);
}

// ── 秘纹护体吸取递增档（moves 侧 837 独占；on_stage_storage = 下场清）──────
inline int miwen_drain_bonus(const ElfPet& pet) {
    auto it = pet.on_stage_storage.find(kMiwenDrainBonusKey);
    return it == pet.on_stage_storage.end() ? 0 : std::any_cast<int>(it->second);
}

inline void miwen_set_drain_bonus(ElfPet& pet, int bonus) {
    pet.on_stage_storage[kMiwenDrainBonusKey] = bonus;
}

// ── 秘纹护体次免记账（2059 剩余次数；on_stage_storage = 与票同生命周期）──────
// ⚠️ 必须与票同生命周期（用户 2026-09-20 口径"次免不可传承"）：票是 ON_STAGE 作用域、
//    换宠即被 `RuleCenter::clear_on_stage` 清掉；记账若放 battle 级容器（plugin_storage），
//    票清掉后残留计数会继续吃**别人**免疫票的事件。放 pet 的 on_stage_storage 则一起清。
constexpr int kMiwenImmuneRemainKey = 205902;

inline int miwen_immune_remain(const ElfPet& pet) {
    auto it = pet.on_stage_storage.find(kMiwenImmuneRemainKey);
    return it == pet.on_stage_storage.end() ? 0 : std::any_cast<int>(it->second);
}

inline void miwen_set_immune_remain(ElfPet& pet, int remain) {
    pet.on_stage_storage[kMiwenImmuneRemainKey] = remain;
}

// ── 神惧劫炎（soul 写、moves 2419 也写 → int 槽跨 dylib）────
inline int yanshou_shenju_rounds(const ElfPet& pet) {
    auto it = pet.on_stage_storage.find(kYanshouShenjuRoundsKey);
    return it == pet.on_stage_storage.end() ? 0 : std::any_cast<int>(it->second);
}

inline void yanshou_set_shenju_rounds(ElfPet& pet, int rounds) {
    if (rounds < 0) {
        rounds = 0;
    }
    if (rounds > kShenjuMaxRounds) {
        rounds = kShenjuMaxRounds;
    }
    pet.on_stage_storage[kYanshouShenjuRoundsKey] = rounds;
}

// 获得神惧劫炎：未持有 → 基础回合数；已持有 → 在现有剩余上叠加，封顶 8。
inline void yanshou_gain_shenju(ElfPet& pet, int base_rounds) {
    const int cur = yanshou_shenju_rounds(pet);
    yanshou_set_shenju_rounds(pet, cur > 0 ? cur + base_rounds : base_rounds);
}

inline int yanshou_first_mover_round(const ElfPet& pet) {
    auto it = pet.on_stage_storage.find(kYanshouFirstMoverRoundKey);
    return it == pet.on_stage_storage.end() ? -1 : std::any_cast<int>(it->second);
}

inline void yanshou_set_first_mover_round(ElfPet& pet, int round) {
    pet.on_stage_storage[kYanshouFirstMoverRoundKey] = round;
}

#endif  // BURN_TRIO_RUNTIME_H
