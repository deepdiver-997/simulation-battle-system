#ifndef SOUL_LIB_COMMON_H
#define SOUL_LIB_COMMON_H

// soul_lib 各家族文件的共享助手（2026-09-17 从 lib_1.cpp 按家族拆分，行为不变）。
// g_core 是 **inline 变量**：同一 .so 内所有编译单元共享一份（插件不链接 sim_core，
// 原语一律经它的函数指针槽调用）。helpers 全部 inline（头内定义、链接器合并）。
//
// 分族见同目录各 .cpp 的文件头；`lib_1.cpp` 只剩 CoreApi 注入与插件注册入口。
// 拆分不变式：已实现的唯一魂印 ID 与行为覆盖不能丢失，全量场景回归守护。
// 2026-11-06 勘误：允许收敛重复注册，不能用“宏总数只增不改”保留同 ID 两份程序；
// 历史计数不代表唯一 ID 数，当前集合以 registry dump 为准。

#include <effects/effect.h>
#include <effects/event_center.h>
#include <effects/trait_state.h>
#include <fsm/battleContext.h>
#include <plugin/plugin_interface.h>
#include <plugin/core_api.h>
#include <plugin/soul_node_args.h>   // T4 类型化魂印行参（SoulNodeArgs/soul_node/SOUL_MARK_TYPED）
#include <abnormal-system/abnormal-types.h>

#include <algorithm>
#include <cstdlib>
#include <vector>
#include <any>

// ── storage 安全读取（2026-10-02 全库 any_cast 治理）─────────────────────────
// 抛式 std::any_cast<int>(any) 在类型不符时直接炸进程（fuzz any_cast_fault 家族，
// seed 446/2397 等）：plugin_storage / soulmark_storage 是共享键空间（守卫布尔、
// 别家窗口可能落进同前缀键位），类型不符 = "不是你的" → 必须按缺失语义回退，
// 绝不允许抛。取值形式全库禁用，读 int 一律走 any_int_or（指针形式 + fallback）。
inline int any_int_or(const std::any& a, int fallback = 0) {
    if (const int* p = std::any_cast<int>(&a)) {
        return *p;
    }
    return fallback;
}


// core 在插件初始化时传入的原语函数指针（依赖倒置：走真原语、不链接 core 符号）。
inline const CoreApi* g_core = nullptr;

inline const CoreApi& soul_chance_core() {
    if (!g_core) throw std::logic_error("soul chance requires the core API");
    return *g_core;
}

inline bool roll_soul_gate(BattleContext& ctx, int rule, int subsite, int actor, int target,
                           int pass, int total, ChanceSingleton singleton = ChanceSingleton::ConsumeDraw) {
    return soul_chance_core().roll_legacy_gate(&ctx, pass, total,
        chance_site(ChanceSiteKind::SoulMark, rule, subsite, actor, target), singleton);
}

inline int choose_soul_range(BattleContext& ctx, int rule, int subsite, int actor, int target,
                             int low, int high, ChanceSingleton singleton = ChanceSingleton::ConsumeDraw) {
    return soul_chance_core().choose_legacy_range(&ctx, low, high,
        chance_site(ChanceSiteKind::SoulMark, rule, subsite, actor, target), singleton);
}

inline int choose_soul_slot(BattleContext& ctx, int rule, int subsite, int actor, int target,
                            std::span<const ChanceOption> options,
                            ChanceSingleton singleton = ChanceSingleton::ConsumeDraw) {
    return soul_chance_core().choose_legacy_slot(&ctx, options,
        chance_site(ChanceSiteKind::SoulMark, rule, subsite, actor, target), singleton);
}

// 梅赫维特（含皮肤）精灵 id：本魂印专属的"切换后仍使用1号位技能"目标。

// ⚠️ 两个不同的官方名词，别混：
//   蚀砚之泪 = 沧岚攻击后挂在**对手**身上的泪（≥3滴废对手护盾、≥4滴废对手挡伤），本常量
//   永恒之水 = 沧岚自身的道数（每道增伤25%，8道下场保留）—— 尚未实现
constexpr const char* kMarkNameEternalTear = "蚀砚之泪";
constexpr int kEternalTearSealThreshold = 4;

inline bool roll_soul_percent(BattleContext& ctx, int mark_id, int subsite_id,
                              int actor, int target, int percent) {
    return soul_chance_core().roll_native_percent(&ctx, ChanceSiteKind::SoulMark, mark_id, subsite_id,
                                           actor, target, percent);
}

inline int resolve_owner_from_args(BattleContext* ctx, const EffectArgs& args) {
    if (args.int_args && args.int_count >= 1) {
        const int owner = args.int_args[0];
        if (owner == 0 || owner == 1) {
            return owner;
        }
    }
    if (ctx && ctx->resolvedDamage.defenderId >= 0 && ctx->resolvedDamage.defenderId <= 1) {
        return ctx->resolvedDamage.defenderId;
    }
    return 0;
}

inline int resolve_target_from_args(BattleContext* ctx, const EffectArgs& args, int owner) {
    if (args.int_args && args.int_count >= 2) {
        const int target = args.int_args[1];
        if (target == 0 || target == 1) {
            return target;
        }
    }
    if (ctx && ctx->resolvedDamage.attackerId >= 0 && ctx->resolvedDamage.attackerId <= 1) {
        return ctx->resolvedDamage.attackerId;
    }
    return 1 - owner;
}

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

inline Mark* find_mark(ElfPet& pet, const std::string& name) {
    auto it = std::find_if(
        pet.marks.begin(),
        pet.marks.end(),
        [&](const Mark& mark) { return mark.name == name; }
    );
    if (it == pet.marks.end()) {
        return nullptr;
    }
    return &(*it);
}

inline const Mark* find_mark(const ElfPet& pet, const std::string& name) {
    auto it = std::find_if(
        pet.marks.begin(),
        pet.marks.end(),
        [&](const Mark& mark) { return mark.name == name; }
    );
    if (it == pet.marks.end()) {
        return nullptr;
    }
    return &(*it);
}

inline int get_mark_count(const ElfPet& pet, const std::string& name) {
    const Mark* mark = find_mark(pet, name);
    return mark ? mark->count : 0;
}

inline void add_mark_stacks(ElfPet& pet, const std::string& name, int stacks) {
    if (stacks <= 0) {
        return;
    }

    Mark* mark = find_mark(pet, name);
    if (!mark) {
        pet.marks.emplace_back(0, name);
        mark = &pet.marks.back();
    }
    mark->count += stacks;
}

inline void remove_mark(ElfPet& pet, const std::string& name) {
    std::erase_if(
        pet.marks,
        [&](const Mark& mark) { return mark.name == name; }
    );
}

// "挡伤失效"（蚀砚之泪 ≥4 滴）统一由 core 的 `ElfPet::damage_suppress_mask` 表达
// （BLOCK|DETECT 类别被抑制）。soul_lib 的挡伤函数是从时点桶直接调的 EffectFn，
// **不经过 DamagePipeline 的 walk**，故此处自读同一个掩码——真相源是掩码本身，
// 插件不再维护自己的 allow-check 列表（旧 guard_not_sealed_by_eternal_tears 已删）。
inline int damage_guard_suppress_bits() {
    return (1 << static_cast<int>(DamageEffectCategory::BLOCK))
         | (1 << static_cast<int>(DamageEffectCategory::DETECT));
}

// 蚀砚之泪滴数 → 挡伤失效掩码（core 字段）。达阈值置位、未达清位。
// 由挂泪 / 清泪（沧岚阵亡）两处调用，保证掩码与印记同步。
inline void sync_eternal_tear_suppress_mask(ElfPet& pet) {
    const int bits = damage_guard_suppress_bits();
    if (get_mark_count(pet, kMarkNameEternalTear) >= kEternalTearSealThreshold) {
        pet.damage_suppress_mask |= bits;
    } else {
        pet.damage_suppress_mask &= ~bits;
    }
}

inline bool damage_guard_suppressed(BattleContext* ctx, int defender_id) {
    if (!ctx || defender_id < 0 || defender_id > 1) {
        return true;  // 参数异常按"已失效"处理（宁可不挡，不误挡）
    }
    const ElfPet& defender = ctx->seerRobot[defender_id].elfPets[ctx->on_stage[defender_id]];
    return (defender.damage_suppress_mask & damage_guard_suppress_bits()) != 0;
}

inline bool try_block_current_red_damage(BattleContext* ctx, int defender_id) {
    if (!ctx || defender_id < 0 || defender_id > 1) {
        return false;
    }

    DamageSnapshot& damage = ctx->resolvedDamage;
    if (damage.defenderId != defender_id) {
        return false;
    }
    if (!damage.isRed || damage.final <= 0) {
        return false;
    }
    if (damage_guard_suppressed(ctx, defender_id)) {
        return false;
    }

    damage.base = 0;
    damage.afterAdd = 0;
    damage.afterMul = 0;
    damage.final = 0;
    damage.addPct = 0;
    damage.mulCoef = 0.0;
    return true;
}

inline void reflect_damage_to_attacker(BattleContext* ctx, int attacker_id, int amount) {
    if (!ctx || attacker_id < 0 || attacker_id > 1 || amount <= 0) {
        return;
    }

    ElfPet& attacker = ctx->seerRobot[attacker_id].elfPets[ctx->on_stage[attacker_id]];
    attacker.hp -= amount;
    if (attacker.hp < 0) {
        attacker.hp = 0;
    }
}

inline void clear_mark_from_all_pets(SeerRobot& robot, const std::string& name) {
    for (ElfPet& pet : robot.elfPets) {
        remove_mark(pet, name);
        if (name == kMarkNameEternalTear) {
            sync_eternal_tear_suppress_mask(pet);  // 泪清 → 一并撤销挡伤失效掩码
        }
    }
}

// ── 原定义在家族区、但被多个家族用到的助手（拆分时上移到此处）──

inline bool onstage_has_soul_mark(BattleContext* ctx, int owner, int mark_id) {
    if (!ctx || owner < 0 || owner > 1) {
        return false;
    }
    const ElfPet& pet = ctx->seerRobot[owner].elfPets[ctx->on_stage[owner]];
    return pet.soulMark.id == mark_id;
}


// 攻击伤害的两个时点（"免疫下1次受到的技能伤害"覆盖它）。
inline uint64_t attack_damage_coverage() {
    return coverage_union({State::BATTLE_FIRST_ATTACK_DAMAGE,
                           State::BATTLE_SECOND_ATTACK_DAMAGE});
}

// ── 击杀效果族（2026-09-20，阿尔忒弥斯"击败类效果获取"所依赖的约定）──
//
// "击败类效果" = 注册在**击败时点族**上的魂印节点，当前两个时点（时点判定流程表）：
//   BATTLE_OPPONENT_DEFEAT_STATUS     「未击败对手 / 击败对手时」（灵巢之主·索杰德尔）
//   BATTLE_AFTER_DEFEATING_OPPONENT   「击败对手后」（莫塔里安 / 空元卪）
// 老式死亡时点（*_MOVER_DEATH / DEATH_TIMING）暂不入族——有具体精灵需求再扩。
// 可获取性**不做静态标注**（无排除清单）：副本执行时以阿尔忒弥斯为主体跑原载荷，
// 载荷自身的依赖读取（印记/计数器不在她身上）落空即自然不生效（霍光式）。
inline bool is_defeat_timing_state(State s) {
    return s == State::BATTLE_OPPONENT_DEFEAT_STATUS
        || s == State::BATTLE_AFTER_DEFEATING_OPPONENT;
}

// 主体槽：魂印效果 args 绑定约定（见 soul_mark_manager.cpp bind_soulmark_args 注释）
// args[2] = 当前效果归属 pet 的槽。击杀类载荷对"自身"的定位**一律读这里**，
// 禁止 find_pet_with_soulmark（那找到的是魂印宿主——副本执行时会写错对象）。
inline int resolve_subject_slot_from_args(const EffectArgs& args) {
    if (args.int_args && args.int_count >= 3) {
        const int slot = args.int_args[2];
        if (slot >= 0 && slot <= 5) {
            return slot;
        }
    }
    return -1;
}

// 击杀归因：本时点对手在场宠已倒地、且最后一击来自 owner 方 → 返回对手槽；否则 -1。
// 真相源 = BattleContext::last_damage_actor（死亡漏斗同款归因，秒杀在
// force_hp_to_zero 里也写入）。击败时点族的桶双侧都会执行，载荷**必须**先过
// 这道归因门，别把"对手击杀了我方"也认成自己的击败。
// 注：副本节点以阿尔忒弥斯为主体注册（ON_STAGE epoch 保证她在场），
// "owner 方的最后一击"即她的击杀——正常对局中出手方只有在场那只。
inline int find_defeated_foe_slot(BattleContext* ctx, int owner) {
    if (!ctx || owner < 0 || owner > 1) {
        return -1;
    }
    const int foe = 1 - owner;
    const int foe_slot = ctx->on_stage[foe];
    if (ctx->seerRobot[foe].elfPets[foe_slot].hp > 0) {
        return -1;  // 对手在场宠没倒地：本回合无击败
    }
    if (ctx->last_damage_actor[foe][foe_slot] != owner) {
        return -1;  // 最后一击不是 owner 方（反弹自尽 / 效果自扣等）
    }
    return foe_slot;
}

// 注：2026-09-18 起，"条件取消类"进度型改写（万相乖离）改走 **单元准入门**
// （`UnitAdmissionFn` + core 注册期询问，见 plugin_interface.h / effect_unit.h）：
// 进度是魂印侧的一个计数器、技能对象**只读**。原先那套"改 parsed_units_[i].condition
// + 台账记原值"的 helper 已删除（避免两份状态、避免持久改写 core 对象）。

#endif // SOUL_LIB_COMMON_H
