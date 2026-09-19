#ifndef ABNORMAL_TYPES_H
#define ABNORMAL_TYPES_H

// 引擎侧主口径 = `effect_des` 里 `kind=2`（异常状态定义）的 `icon` 字段，逐条对齐。
//
// 2026-09-18（用户口径「36 槽是之前的数量，新增了就继续加呗」）把 **icon 37~43** 一并收进来：
//   37 砥砺 / 38 星赎 / 39 神游 / 40 空定 / 41 狂信 / 42 雷解 / 43 渐冻
// ⇒ 槽数 36 → **44**。⚠️ `icon = 36` 在官方表里**没有条目**（空号），故槽 36 留空
//   （`abnormal_status_name_cn` 会回"未知异常"）；不要为了"填满"给它编一条。
constexpr int kOfficialAbnormalStatusMaxId = 43;
constexpr int kOfficialAbnormalStatusSlotCount = kOfficialAbnormalStatusMaxId + 1;

enum class AbnormalStatusId : int {
    Paralysis = 0,              // 麻痹
    Poison = 1,                 // 中毒
    Burn = 2,                   // 烧伤
    ParasitizeOpponent = 3,     // 寄生对手
    Parasite = 4,               // 寄生
    Frostbite = 5,              // 冻伤
    Fear = 6,                   // 害怕
    Fatigue = 7,                // 疲惫
    Sleep = 8,                  // 睡眠
    Petrify = 9,                // 石化
    Confusion = 10,             // 混乱
    Weakness = 11,              // 衰弱
    MountainGuardian = 12,      // 山神守护
    Flammable = 13,             // 易燃
    Berserk = 14,               // 狂暴
    Icebound = 15,              // 冰封
    Bleed = 16,                 // 流血
    Immunity = 17,              // 免疫
    ImmunityII = 18,            // 免疫（第二槽）
    Crippled = 19,              // 瘫痪
    Blind = 20,                 // 失明
    AbnormalImmunity = 21,      // 异常免疫
    Incinerate = 22,            // 焚烬
    Curse = 23,                 // 诅咒
    FlameCurse = 24,            // 烈焰诅咒
    DeathCurse = 25,            // 致命诅咒
    WeaknessCurse = 26,         // 虚弱诅咒
    Infection = 27,             // 感染
    Bind = 28,                  // 束缚
    Distraction = 29,           // 失神
    Silence = 30,               // 沉默
    Submission = 31,            // 臣服
    Stasis = 32,                // 凝滞
    StarBlessing = 33,          // 星赐
    StarWisdom = 34,            // 星哲
    Overclock = 35,             // 超频
    // ── 2026-09-18 扩表：icon 37~43（36 是官方空号，故意跳过）──
    Resolution = 37,            // 砥砺（附属类）
    StarRedemption = 38,        // 星赎（附属类）
    Wandering = 39,             // 神游（控制类）
    VoidFixation = 40,          // 空定（控制类）
    Fanaticism = 41,            // 狂信（控制类）
    ThunderRelease = 42,        // 雷解（附属类）
    SlowFreeze = 43,            // 渐冻（附属类）
};

// battle_effects.Efftype:
// 0 = 控制类异常, 1 = 弱化/属性变化类, 2 = 特殊 buff/保护类
enum class AbnormalStatusKind : int {
    Control = 0,
    Debuff = 1,
    Special = 2,
    Unknown = -1,
};

inline bool is_valid_abnormal_status_id(int status_id) {
    return status_id >= 0 && status_id <= kOfficialAbnormalStatusMaxId;
}

inline int abnormal_status_id(AbnormalStatusId status_id) {
    return static_cast<int>(status_id);
}

inline const char* abnormal_status_name_cn(AbnormalStatusId status_id) {
    switch (status_id) {
        case AbnormalStatusId::Paralysis: return "麻痹";
        case AbnormalStatusId::Poison: return "中毒";
        case AbnormalStatusId::Burn: return "烧伤";
        case AbnormalStatusId::ParasitizeOpponent: return "寄生对手";
        case AbnormalStatusId::Parasite: return "寄生";
        case AbnormalStatusId::Frostbite: return "冻伤";
        case AbnormalStatusId::Fear: return "害怕";
        case AbnormalStatusId::Fatigue: return "疲惫";
        case AbnormalStatusId::Sleep: return "睡眠";
        case AbnormalStatusId::Petrify: return "石化";
        case AbnormalStatusId::Confusion: return "混乱";
        case AbnormalStatusId::Weakness: return "衰弱";
        case AbnormalStatusId::MountainGuardian: return "山神守护";
        case AbnormalStatusId::Flammable: return "易燃";
        case AbnormalStatusId::Berserk: return "狂暴";
        case AbnormalStatusId::Icebound: return "冰封";
        case AbnormalStatusId::Bleed: return "流血";
        case AbnormalStatusId::Immunity: return "免疫";
        case AbnormalStatusId::ImmunityII: return "免疫（第二槽）";
        case AbnormalStatusId::Crippled: return "瘫痪";
        case AbnormalStatusId::Blind: return "失明";
        case AbnormalStatusId::AbnormalImmunity: return "异常免疫";
        case AbnormalStatusId::Incinerate: return "焚烬";
        case AbnormalStatusId::Curse: return "诅咒";
        case AbnormalStatusId::FlameCurse: return "烈焰诅咒";
        case AbnormalStatusId::DeathCurse: return "致命诅咒";
        case AbnormalStatusId::WeaknessCurse: return "虚弱诅咒";
        case AbnormalStatusId::Infection: return "感染";
        case AbnormalStatusId::Bind: return "束缚";
        case AbnormalStatusId::Distraction: return "失神";
        case AbnormalStatusId::Silence: return "沉默";
        case AbnormalStatusId::Submission: return "臣服";
        case AbnormalStatusId::Stasis: return "凝滞";
        case AbnormalStatusId::StarBlessing: return "星赐";
        case AbnormalStatusId::StarWisdom: return "星哲";
        case AbnormalStatusId::Overclock: return "超频";
        case AbnormalStatusId::Resolution: return "砥砺";
        case AbnormalStatusId::StarRedemption: return "星赎";
        case AbnormalStatusId::Wandering: return "神游";
        case AbnormalStatusId::VoidFixation: return "空定";
        case AbnormalStatusId::Fanaticism: return "狂信";
        case AbnormalStatusId::ThunderRelease: return "雷解";
        case AbnormalStatusId::SlowFreeze: return "渐冻";
        default: return "未知异常";
    }
}

inline const char* abnormal_status_name_cn(int status_id) {
    if (!is_valid_abnormal_status_id(status_id)) {
        return "未知异常";
    }
    return abnormal_status_name_cn(static_cast<AbnormalStatusId>(status_id));
}

inline AbnormalStatusKind abnormal_status_kind(AbnormalStatusId status_id) {
    switch (status_id) {
        case AbnormalStatusId::Paralysis:
        case AbnormalStatusId::ParasitizeOpponent:
        case AbnormalStatusId::Fear:
        case AbnormalStatusId::Fatigue:
        case AbnormalStatusId::Sleep:
        case AbnormalStatusId::Petrify:
        case AbnormalStatusId::Icebound:
        case AbnormalStatusId::Crippled:
        case AbnormalStatusId::Incinerate:
        case AbnormalStatusId::Curse:
        case AbnormalStatusId::Infection:
            return AbnormalStatusKind::Control;
        case AbnormalStatusId::Poison:
        case AbnormalStatusId::Burn:
        case AbnormalStatusId::Parasite:
        case AbnormalStatusId::Frostbite:
        case AbnormalStatusId::Confusion:
        case AbnormalStatusId::Weakness:
        case AbnormalStatusId::Flammable:
        case AbnormalStatusId::Bleed:
        case AbnormalStatusId::Blind:
        case AbnormalStatusId::Bind:
        case AbnormalStatusId::Distraction:
        case AbnormalStatusId::Silence:
        case AbnormalStatusId::Submission:
        case AbnormalStatusId::Stasis:
            return AbnormalStatusKind::Debuff;
        case AbnormalStatusId::MountainGuardian:
        case AbnormalStatusId::Berserk:
        case AbnormalStatusId::Immunity:
        case AbnormalStatusId::ImmunityII:
        case AbnormalStatusId::AbnormalImmunity:
        case AbnormalStatusId::FlameCurse:
        case AbnormalStatusId::DeathCurse:
        case AbnormalStatusId::WeaknessCurse:
        case AbnormalStatusId::StarBlessing:
        case AbnormalStatusId::StarWisdom:
        case AbnormalStatusId::Overclock:
        // 官方 effect_des 文本自述"附属类异常状态"（2026-09-18 扩表）
        case AbnormalStatusId::Resolution:
        case AbnormalStatusId::StarRedemption:
        case AbnormalStatusId::ThunderRelease:
        case AbnormalStatusId::SlowFreeze:
            return AbnormalStatusKind::Special;
        // 官方自述"控制类异常状态，该状态下精灵无法行动"（2026-09-18 扩表）
        case AbnormalStatusId::Wandering:
        case AbnormalStatusId::VoidFixation:
        case AbnormalStatusId::Fanaticism:
            return AbnormalStatusKind::Control;
        default:
            return AbnormalStatusKind::Unknown;
    }
}

inline AbnormalStatusKind abnormal_status_kind(int status_id) {
    if (!is_valid_abnormal_status_id(status_id)) {
        return AbnormalStatusKind::Unknown;
    }
    return abnormal_status_kind(static_cast<AbnormalStatusId>(status_id));
}

inline bool is_control_abnormal_status(AbnormalStatusId status_id) {
    switch (status_id) {
        case AbnormalStatusId::Paralysis:
        case AbnormalStatusId::ParasitizeOpponent:
        case AbnormalStatusId::Fatigue:
        case AbnormalStatusId::Fear:
        case AbnormalStatusId::Sleep:
        case AbnormalStatusId::Petrify:
        case AbnormalStatusId::Infection:
        case AbnormalStatusId::Icebound:
        case AbnormalStatusId::Crippled:
        case AbnormalStatusId::Incinerate:
        case AbnormalStatusId::Curse:
        // 2026-09-18 扩表：官方 effect_des 自述控制类 + "无法行动"
        case AbnormalStatusId::Wandering:
        case AbnormalStatusId::VoidFixation:
        case AbnormalStatusId::Fanaticism:
            return true;
        default:
            return false;
    }
}

// ----------------------------------------------------------------
// 限制类异常（官方 effect_des 19「瘫痪：控制类异常状态，限制类异常状态，该状态下精灵
// 无法行动、主动切换」/ 32「凝滞：弱化类异常状态，限制类异常状态，该状态下精灵无法切换，
// 同时免疫受到的控制类异常状态」；语料 idx=355「限制类异常状态，顾名思义就是限制对方切换，
// 嗑药等等，目前只有凝滞和瘫痪」）。
//
// 与 is_control_abnormal_status 是**正交**的两张表：
//   - 控制类 → 禁**行动**（has_blocking_control_status）；
//   - 限制类 → 禁**主动切换**（can_switch_out）。
// 瘫痪同时属于两张表（既不能行动也不能切换）；凝滞只属于限制类（能行动，不能切换）。
// ----------------------------------------------------------------
inline bool is_restriction_abnormal_status(AbnormalStatusId status_id) {
    return status_id == AbnormalStatusId::Crippled   // 瘫痪 19
        || status_id == AbnormalStatusId::Stasis;    // 凝滞 32
}

// 第五技能槽位（`ElfPet` 的 `std::array<Skills, 5>`，0..4；万相乖离 2260 的 kFifthSkillSlot 同值）。
// 用途：沉默(30)「第五技能**无效**」要判"本次打的是不是第五技能"。
constexpr int kFifthSkillSlot = 4;

// ----------------------------------------------------------------
// 信仰对象（狂信 41 的配套状态）
//
// 官方定义（数据库 `effect_des` **id=517、`kind=6`** 规则/概念词条 —— 注意它**不是**异常状态本身，
// 而是狂信引用的一个独立规则属性）：
//   > 信仰对象：**战斗中每只精灵恒常存在的规则属性，初始为无对象**
// 生命期依据（三条独立互证，见开工文档 §十）：
//   · 上引 517「战斗中**恒常存在**」；同族 536「临时体力上限」明写"未注明消失时点时**默认为精灵下场时**"
//     —— 官方要"下场清"时会写，517 没写 → **不是下场清**；
//   · 语料「哪怕是**阵亡精灵**也得信仰教皇，除非该精灵**消逝**」；
//   · `effect_info` 2544「**自身位于场下时**，为自身的信仰对象提供支援」——背包里仍在读。
// ⇒ 存 **`ElfPet::soulmark_storage`**（下场保留、跨阵亡保留，只有消逝/清场才没）。
//   ⚠️ **不能**放 `on_stage_storage`（下场清）或 `abnormal_status_end_round`（per-side + 下场清）。
// key 用官方词条 id 517（`soulmark_storage` 里其它键是"魂印 mark_id*100+n"系，裸 id 不冲突）。
constexpr int kFaithTargetStorageKey = 517;

// 信仰对象 = 一个指向**某只具体精灵**的引用。用 `(side, slot)` 二元组表达 —— 这是全引擎
// 既定的"引用另一只 pet"惯用法（同 `ExtraSpirit::linked_slot` / `last_damage_actor[2][6]` /
// `RuleCenter::source_slot`）。⚠️ 语义上**不是**"当前在场的那只"：
// 语料「如果此时我方切换为非信仰对象的精灵，那么对手挂着狂信异常依然可以正常出招」
// → **存的是对象引用、判的是"当前在场的对手是否就是它"**（两段式）。
struct FaithTarget {
    int side = -1;   // -1 = 无对象
    int slot = -1;
};

// ----------------------------------------------------------------
// 异常扣血档案
//
// 官方 effect_des 的 kind=2 逐条给出时点与档位，语料 idx=472《机制讲解—挡伤/锁伤，miss/异常》
// 把整族归纳为三档（真实百分比 / 百分比 / 固定），两者互相印证，故这里做**单一真相源**：
//
//   档位                     结算语义                          官方/语料依据
//   TruePercent  1/8 最大体力、**真实伤害**（护盾护罩不响应、  idx=266「真实伤害：无视对手
//                不吃伤害抗性）                              护盾、护罩与抗性的伤害」
//   Percent      1/8 最大体力、吃百分比抗性/护罩/免粉          idx=472「会被 35 免减，辛也能分担」
//   Fixed        点数额定伤害                                流血 idx=472「固定 80 伤害」
//
// 时点：ActionStart = "每回合对手行动开始时"（官方 6 条）；RoundReduction = "每回合结束后"
// （官方 24/30，语料 idx=429 明确"异常沉默，束缚，烈焰诅咒均在回合扣减点结算"）；
// OnExpire = "…结束时"（官方 28）。
// ----------------------------------------------------------------
enum class AbnormalDamageTiming : int {
    None = 0,
    ActionStart = 1,      // 每回合（对手）行动开始时
    RoundReduction = 2,   // 回合扣减点（官方"每回合结束后"）
    OnExpire = 3,         // 该异常自然结束时
};

enum class AbnormalDamageTier : int {
    None = 0,
    TruePercent = 1,   // 真实百分比（1/8 最大体力，真伤）
    Percent = 2,       // 百分比伤害（1/8 最大体力，吃抗性/护罩/免粉）
    Fixed = 3,         // 固定点数伤害
};

struct AbnormalDamageProfile {
    AbnormalDamageTiming timing = AbnormalDamageTiming::None;
    AbnormalDamageTier tier = AbnormalDamageTier::None;
    int numerator = 0;    // 分子；tier == Fixed 时即点数
    int denominator = 0;  // 分母（>0 = 按目标最大体力的百分比；0 = 用 numerator 当点数）
    int chance_pct = 100; // 触发概率（官方 10 混乱只有 5%）
    bool heal_opponent_equal = false;  // 官方 3/4 寄生："同时对手会恢复等量的体力"
};

inline AbnormalDamageProfile abnormal_damage_profile(AbnormalStatusId status_id) {
    switch (status_id) {
        // ── 真实百分比档 ──
        case AbnormalStatusId::Poison:      // 官方 1「每回合对手行动开始时扣除最大体力的 1/8」
        case AbnormalStatusId::Burn:        // 官方 2（另：攻击技能威力 -50%）
        case AbnormalStatusId::Frostbite:   // 官方 5
            return {AbnormalDamageTiming::ActionStart, AbnormalDamageTier::TruePercent, 1, 8, 100, false};
        case AbnormalStatusId::Parasite:    // 官方 4「扣除最大体力的 1/8 恢复至对手」
            return {AbnormalDamageTiming::ActionStart, AbnormalDamageTier::TruePercent, 1, 8, 100, true};
        // ⚠️ 引擎口径：只有 Parasite(4) 进扣血表。ParasitizeOpponent(3) 在 effect_des 里
        //    名字同样是"寄生"，引擎把 3/4 拆成两条是历史约定 —— 不在此静默改写它的行为。
        // ── 固定档 ──
        case AbnormalStatusId::Bleed:       // 官方 16「每回合对手行动开始时扣除 80 点体力」
            return {AbnormalDamageTiming::ActionStart, AbnormalDamageTier::Fixed, 80, 0, 100, false};
        case AbnormalStatusId::Confusion:   // 官方 10「每回合对手行动开始时 5% 扣除 50 点体力」
            // 语料 idx=472 把混乱与流血并列为"固定伤害异常"，但作者自述概率太低未实测；
            // 官方文本只说"扣除 50 点体力"→ 取保守档（走 FIXED，吃护罩/固定抗性）。待实测。
            return {AbnormalDamageTiming::ActionStart, AbnormalDamageTier::Fixed, 50, 0, 5, false};
        // ── 百分比档（吃减免、走粉伤管线）──
        case AbnormalStatusId::FlameCurse:  // 官方 24「每回合结束后受到最大体力 1/8 的百分比伤害」
        case AbnormalStatusId::Silence:     // 官方 30（另：第五技能无效）
            return {AbnormalDamageTiming::RoundReduction, AbnormalDamageTier::Percent, 1, 8, 100, false};
        case AbnormalStatusId::Bind:        // 官方 28「束缚结束时受到最大体力 1/8 的百分比伤害」
            return {AbnormalDamageTiming::OnExpire, AbnormalDamageTier::Percent, 1, 8, 100, false};
        default:
            return {};
    }
}

// 行动开始时会扣血的异常（旧谓词保留：现由档案派生，语义 = timing 为 ActionStart 者）。
inline bool deals_damage_at_action_start(AbnormalStatusId status_id) {
    return abnormal_damage_profile(status_id).timing == AbnormalDamageTiming::ActionStart;
}

// ----------------------------------------------------------------
// 衍化（官方"a 异常自然结束后转化为 b 异常"）
//
// 完整清单（官方 effect_des 文本）：
//   冰封 15 → 冻伤 5 + 速度等级 -1
//   焚烬 22 → 烧伤 2 + **命中等级 -1**
//   诅咒 23 → 随机（烈焰诅咒 24 / 致命诅咒 25 / 虚弱诅咒 26）
//   感染 27 → 中毒 1 + 攻击等级 -1、特攻等级 -1
//   超频 35 → 瘫痪 19（1~2 回合）
//   神游 39 → 失神 29 / 空定 40 → 2 回合沉默 30 / 渐冻 43 → 3 回合冰封 15（**表外**，36 槽不含，待扩表）
//
// 附带弱化**无视任何免疫弱化**（语料 idx=37「异常转化为弱化会无视免弱」；
// idx=466「不能免疫异常带来的弱化（焚烬，冰封，感染）」）→ 落地时用穿透版弱化。
// ----------------------------------------------------------------
enum class AbnormalDerivationGroup : int {
    None = 0,
    Single = 1,     // 转出单个指定异常
    CurseRoll = 2,  // 诅咒：随机转出 烈焰诅咒/致命诅咒/虚弱诅咒 之一
};

// 能力等级槽位（与 BattleContext::ability_levels / BattleWorkspace::view_levels 同索引）。
// **6 槽**：0=攻击 1=特攻 2=防御 3=特防 4=速度 **5=命中**（2026-09-18 口径更正：
// 槽 5 原注释误标"体力"——官方能力提升状态没有体力，第 6 项是命中；见 battleContext.h 的长注释）。
// ⚠️ 别与"基础数值"混：`numerical_properties` 的第 6 项**是**体力（那是属性值，不是等级）。
constexpr int kAbilityLevelIndexAttack = 0;
constexpr int kAbilityLevelIndexSpecialAttack = 1;
constexpr int kAbilityLevelIndexSpeed = 4;
constexpr int kAbilityLevelIndexHit = 5;

// 命中等级 → 命中率倍率（**百分数**，分母 100）。负档沿用引擎既有档位表
// （原先误置在 `BattleWorkspace::getTempAbilityValue` 的 `index == 5 && level < 0` 分支里，
//   注释即"命中等级为负时的特殊处理"，2026-09-18 正名后搬到这里给精度公式用）。
// 正档暂无官方档位表 → **按用户 2026-09-18 口径**暂用与攻击等其余 5 项等级**相同的数值修正**
// `(lvl+2)/2`（+1 → ×1.5、+2 → ×2、+6 → ×4）。找到官方描述后再校正。
inline int hit_level_accuracy_pct(int level) {
    if (level <= -6) return 25;
    if (level < 0) {
        static constexpr int kNegativeHitPct[7] = {100, 85, 70, 55, 45, 35, 25};
        return kNegativeHitPct[-level];
    }
    return 100 * (level + 2) / 2;
}

struct AbnormalDerivation {
    AbnormalDerivationGroup group = AbnormalDerivationGroup::None;
    int derived_id = -1;        // group == Single 时的转出异常 id
    int derived_rounds_min = 0; // 转出异常的回合数区间；min <= 0 → 用 random_anomaly_duration()
    int derived_rounds_max = 0;
    // 附带的能力等级下降（最多两项，如感染 = 攻击 + 特攻）。索引见 kAbilityLevelIndex*。
    int stat_indices[2] = {-1, -1};
    int stat_deltas[2] = {0, 0};   // 下降量（负数）
};

inline AbnormalDerivation abnormal_derivation(AbnormalStatusId status_id) {
    AbnormalDerivation d;
    switch (status_id) {
        case AbnormalStatusId::Icebound:
            d.group = AbnormalDerivationGroup::Single;
            d.derived_id = static_cast<int>(AbnormalStatusId::Frostbite);
            d.stat_indices[0] = kAbilityLevelIndexSpeed;
            d.stat_deltas[0] = -1;
            return d;
        case AbnormalStatusId::Incinerate:
            d.group = AbnormalDerivationGroup::Single;
            d.derived_id = static_cast<int>(AbnormalStatusId::Burn);
            d.stat_indices[0] = kAbilityLevelIndexHit;   // 命中等级 -1
            d.stat_deltas[0] = -1;
            return d;
        case AbnormalStatusId::Curse:
            d.group = AbnormalDerivationGroup::CurseRoll;
            return d;
        case AbnormalStatusId::Infection:
            d.group = AbnormalDerivationGroup::Single;
            d.derived_id = static_cast<int>(AbnormalStatusId::Poison);
            d.stat_indices[0] = kAbilityLevelIndexAttack;
            d.stat_deltas[0] = -1;
            d.stat_indices[1] = kAbilityLevelIndexSpecialAttack;
            d.stat_deltas[1] = -1;
            return d;
        case AbnormalStatusId::Overclock:
            d.group = AbnormalDerivationGroup::Single;
            d.derived_id = static_cast<int>(AbnormalStatusId::Crippled);
            d.derived_rounds_min = 1;   // 官方 35「超频结束后会转化为 1~2 回合的瘫痪」
            d.derived_rounds_max = 2;
            return d;
        // ── 2026-09-18 扩表后的衍化族（官方 effect_des 文学文本）──
        case AbnormalStatusId::Wandering:
            // 官方 39「神游结束后会转化为失神」（未写回合数 → 用默认）
            d.group = AbnormalDerivationGroup::Single;
            d.derived_id = static_cast<int>(AbnormalStatusId::Distraction);
            return d;
        case AbnormalStatusId::VoidFixation:
            // 官方 40「空定结束后会转化为**2回合**沉默」
            d.group = AbnormalDerivationGroup::Single;
            d.derived_id = static_cast<int>(AbnormalStatusId::Silence);
            d.derived_rounds_min = 2;
            d.derived_rounds_max = 2;
            return d;
        case AbnormalStatusId::SlowFreeze:
            // 官方 43「渐冻结束后会转化为**3回合**冰封」
            d.group = AbnormalDerivationGroup::Single;
            d.derived_id = static_cast<int>(AbnormalStatusId::Icebound);
            d.derived_rounds_min = 3;
            d.derived_rounds_max = 3;
            return d;
        default:
            return d;   // None = 不衍化
    }
}

#endif // ABNORMAL_TYPES_H
