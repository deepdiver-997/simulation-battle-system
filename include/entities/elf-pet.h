#ifndef ELF_PET_H
#define ELF_PET_H

#include <any>
#include <array>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include <entities/common_trait.h>
#include <entities/elemental-attributes.h>
#include <entities/mark.h>
#include <abnormal-system/resistance-system.h>
#include <entities/numerical-properties.h>
#include <entities/shield_bank.h>
#include <entities/skills.h>
#include <entities/soul_mark.h>

enum class Gender {
    MALE = 0,
    FEMALE = 1,
    NONE = 2
};

// 精灵**自身**的伤害抗性（官方三种：暴击 / 固定 / 百分比）。
// 这是精灵的属性（抗性训练刷出来的），属于本体 → **跨切换保留**，随 pet 存活。
// ⚠️ 与"临时 buff 修改抗性"区分：临时修改（如混元天尊死亡 buff 让己方精灵抗性
//    被视为 100%，3 回合后恢复）写 BattleWorkspace 的**有效抗性视图**，不写本体。
//    伤害计算一律读 ws 视图；ws 视图在回合开始 / 换宠时从本体重基。
// 依据：docs/02-效果系统/官方机制理解与引擎缺口对照.md §一 粉伤抗性系统。
struct DamageResist {
    int crit_pct = 0;      // 暴击伤害抗性%（**乘整段**：100 → 暴击×2 → 200 → ×(1−抗性)）
    int fixed_pct = 0;     // 固定伤害抗性%
    int percent_pct = 0;   // 百分比伤害抗性%
};

class ElfPet {
public:
    ElfPet() = delete;

    ElfPet(int id,
           std::string name,
           std::array<int, 2> elemental_attributes,
           int soul_seal,
           Gender gender,
           SoulMark soul_mark,
           CommonTrait common_trait,
           numerical_properties numerical_base,
           int initial_hp,
           int shield,
           int cover,
           bool is_locked,
           std::array<Skills, 5> skills)
        : elementalAttributes(std::move(elemental_attributes))
        , soulSeal(soul_seal)
        , gender(gender)
        , soulMark(std::move(soul_mark))
        , commonTrait(std::move(common_trait))
        , numericalBase(numerical_base)
        , numericalProperties(numerical_base)
        , hp(numericalProperties[NumericalPropertyIndex::HP])
        , speed_priority(0)
        , shield(shield)
        , cover(cover)
        , is_locked(is_locked)
        , skills(std::move(skills))
        , id(id)
        , name(std::move(name)) {
        hp = initial_hp;
    }

    ElfPet(const ElfPet& other)
        : elementalAttributes(other.elementalAttributes)
        , soulSeal(other.soulSeal)
        , gender(other.gender)
        , soulMark(other.soulMark)
        , commonTrait(other.commonTrait)
        , numericalBase(other.numericalBase)
        , numericalProperties(other.numericalProperties)
        , hp(numericalProperties[NumericalPropertyIndex::HP])
        , speed_priority(other.speed_priority)
        , shield(other.shield)
        , cover(other.cover)
        , is_locked(other.is_locked)
        , skills(other.skills)
        , marks(other.marks)
        , soulmark_storage(other.soulmark_storage)
        , resistance(other.resistance)
        , damage_resist(other.damage_resist)
        , id(other.id)
        , name(other.name) {}

    ElfPet(ElfPet&& other) noexcept
        : elementalAttributes(std::move(other.elementalAttributes))
        , soulSeal(other.soulSeal)
        , gender(other.gender)
        , soulMark(std::move(other.soulMark))
        , commonTrait(std::move(other.commonTrait))
        , numericalBase(other.numericalBase)
        , numericalProperties(other.numericalProperties)
        , hp(numericalProperties[NumericalPropertyIndex::HP])
        , speed_priority(other.speed_priority)
        , shield(other.shield)
        , cover(other.cover)
        , is_locked(other.is_locked)
        , skills(std::move(other.skills))
        , marks(std::move(other.marks))
        , soulmark_storage(std::move(other.soulmark_storage))
        , resistance(std::move(other.resistance))
        , damage_resist(std::move(other.damage_resist))
        , id(other.id)
        , name(std::move(other.name)) {}

    ElfPet& operator=(const ElfPet& other) {
        if (this == &other) {
            return *this;
        }
        elementalAttributes = other.elementalAttributes;
        soulSeal = other.soulSeal;
        gender = other.gender;
        soulMark = other.soulMark;
        commonTrait = other.commonTrait;
        numericalBase = other.numericalBase;
        numericalProperties = other.numericalProperties;
        speed_priority = other.speed_priority;
        shield = other.shield;
        cover = other.cover;
        is_locked = other.is_locked;
        skills = other.skills;
        marks = other.marks;
        soulmark_storage = other.soulmark_storage;
        resistance = other.resistance;
        damage_resist = other.damage_resist;
        id = other.id;
        name = other.name;
        return *this;
    }

    ElfPet& operator=(ElfPet&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        elementalAttributes = std::move(other.elementalAttributes);
        soulSeal = other.soulSeal;
        gender = other.gender;
        soulMark = std::move(other.soulMark);
        commonTrait = std::move(other.commonTrait);
        numericalBase = other.numericalBase;
        numericalProperties = other.numericalProperties;
        speed_priority = other.speed_priority;
        shield = other.shield;
        cover = other.cover;
        is_locked = other.is_locked;
        skills = std::move(other.skills);
        marks = std::move(other.marks);
        soulmark_storage = std::move(other.soulmark_storage);
        resistance = std::move(other.resistance);
        damage_resist = std::move(other.damage_resist);
        id = other.id;
        name = std::move(other.name);
        return *this;
    }

    ~ElfPet() = default;

    std::array<int, 2> elementalAttributes{};
    int soulSeal = 0;
    Gender gender = Gender::NONE;
    SoulMark soulMark;
    CommonTrait commonTrait;
    ResistanceSystem resistance;  // 异常抗性（训练刷出的概率抵抗；apply_anomaly 在魂免前 roll）
    DamageResist damage_resist;   // 本体伤害抗性（暴击/固定/百分比；跨切换保留，计算走 ws 视图）
    numerical_properties numericalBase, numericalProperties;
    int& hp = numericalProperties[NumericalPropertyIndex::HP];
    // ⚠️ 能力等级**不在 pet 上**（2026-09-16 改造）：等级只在"在场期间"有意义、换宠即清，
    //    权威状态在 `BattleContext::ability_levels`（on-stage 作用域），回合内视图是
    //    `ws.view_levels`。见 battleContext.h 的字段注释与 perform_switch 的清除点。
    int speed_priority = 0;
    int shield = 0;      // 旧字段，暂留（未参与伤害计算）
    int cover = 0;
    ShieldBank shield_bank_;  // 护盾槽（只响应红伤 NORMAL；多来源 + 优先级消耗 + 每回合刷新）
    ShieldBank hood_bank_;    // 护罩槽（只响应粉伤 FIXED/PERCENT；结构同护盾，独立实体）
    int damage_suppress_mask = 0;  // 伤害效果抑制掩码（bit = DamageEffectCategory，被抑制类别在管线 walk 时跳过）
    bool is_locked = false;
    std::array<Skills, 5> skills;
    std::vector<Mark> marks;

    // ── 效果私有存储：两个槽为**一对**，选哪个只看生命周期 ──────────────
    // ① soulmark_storage —— **下场保留**（跨回合 + 跨切换，随 pet 对象存活；
    //    精灵阵亡/战斗结束随 pet 销毁）。按来源 id 分槽，内容由效果自解释（引擎不管布局）。
    //    典型：无相谛蓄力（万相乖离已取消条件数 + 威力提升）——**故意**要跨切换累计。
    // ② on_stage_storage —— **本次上场**（换宠由引擎清空，结构同上）。
    //    大多数"本次上场累计/每次使用递增"计数走这里（如落芳天华 497 固伤递增）；
    //    官方实测：这类计数**下场不保留**（用户 2026-09-13 游戏内确认）。
    // ⚠️ 选错槽的后果是静默的（该清的没清 / 该留的丢了），改动时先想清楚生命周期。
    std::map<int, std::any> soulmark_storage;
    std::map<int, std::any> on_stage_storage;

    int id = -1;
    std::string name;
};

#endif // ELF_PET_H
