#ifndef TRAIT_STATE_H
#define TRAIT_STATE_H

// ═══════════════════════════════════════════════════════════════════════════
// TraitState —— 通用特性（new_se stat=1）运行时层（header-only，插件可直调）
//
// 通用特性的静态数据在 `ElfPet::commonTrait`（PetFactory 从 new_se 装配，见
// entities/common_trait.h），本层负责**战斗内的生效视图**与**可查询/可变更**：
//   - 行为函数（红伤/粉伤管线里的常驻条目）只认这里解析出的 kind/参数，不直读 pet 数据；
//   - 检测特性的魂印（妙时天女复制、湮灭之主·扎克斯抑制、无极圣武读施加异常种类…）
//     走 BattleContext 上的内联查询/变更入口（effective_common_trait /
//     suppress_common_trait / copy_common_trait），**不解析描述文本**。
//
// 「取消触发条件」（天女式复制升级，"{n%}使对手害怕"→100%）不用解析子句：
// 概率触发类特性的行为函数掷点前统一查 `proc_forced`，真则必发 —— 复制时置位即可。
// 需要更细的"按 kind 覆写参数"时，给 EffectiveTrait 加字段，仍不碰文本。
//
// ⚠️ header-only 是为了插件能调（CLAUDE.md 3.9：插件 dylib 不链接 sim_core）——
//    仿 RuleCenter 先例，本头不建 .cpp；依赖 BattleContext 的入口内联在 battleContext.h。
// ═══════════════════════════════════════════════════════════════════════════

#include <cstdlib>
#include <optional>
#include <string>
#include <vector>

#include <entities/common_trait.h>

// 特性机制身份。按需扩充（一种机制一个值，与官方 effect_id 无关——同一机制不同星级
// 在 new_se 里是多行不同 idx/effect_id，行为一致只差参数）。
// 名字 → kind 的映射见 trait_kind_from_name；映射外的名字一律 None（数据可查、行为 no-op）。
enum class TraitKind {
    None = 0,
    InstantKill,  // 瞬杀（官方 effect_id 32/151——咤克斯式检测查的就是这两个 id）：
                  //      0-5星统一 = 红伤落地后 force_hp_to_zero 归零秒杀（**不是粉伤**、
                  //      无红伤修正，星级只差触发概率 args[0] 千分点）；
                  //      可被"秒杀转化/免疫"短路（原语查标记，见 battle_primitives.h）
    Hardness,     // 坚硬：受伤减 n%（红伤管线 REDUCE_TRAIT 阶段，非通用减伤、乘法、早于保底）
    Spirit,       // 精神：特殊攻击伤害 +n%（AMP_EXTRA 阶段，非通用增伤、乘法、按技能类别门控）
};

inline TraitKind trait_kind_from_name(const std::string& name) {
    if (name == "瞬杀") {
        return TraitKind::InstantKill;
    }
    if (name == "坚硬") {
        return TraitKind::Hardness;
    }
    if (name == "精神") {
        return TraitKind::Spirit;
    }
    return TraitKind::None;
}

// 一次查询拿到的"生效特性"（本体或复制条目的解析结果）。
// args 语义按 kind：
//   - InstantKill：args[0] = 触发概率**千分点**（3→0.3%、13→1.3%、15→1.5%、20→2%、25→2.5%、30→3%）。
//     ⚠️ 赛学必修6《精灵特性》：描述写的 3/3.5/4/5/6/7% 是**假的**，真实内置概率 = args/10%。
//   - Hardness：**不读 args**（官方读数错误）——用 trait_hardness_pct 的语料修正表。
//   - Spirit：args[0] = 官方 Category 代码（2=特殊攻击，与 map_skill_type 的 1物理/2特殊/4属性 一致）、
//     args[1] = 增伤百分比。
struct EffectiveTrait {
    TraitKind kind = TraitKind::None;
    int idx = 0;              // new_se.idx（本体行；复制条目带来源行 id）
    int star_level = 0;       // 0-5
    int args[2] = {0, 0};     // 官方 args 前两个参数（语义按 kind，见上）
    bool proc_forced = false; // 触发条件被取消（天女式复制升级）：概率触发 → 必发
    bool copied = false;      // true = 复制来的（生命周期锚来源方，见 TraitOverlay）
};

inline EffectiveTrait effective_trait_from_common(const CommonTrait& trait) {
    EffectiveTrait t;
    t.kind = trait_kind_from_name(trait.name);
    t.idx = trait.id;
    t.star_level = trait.star_level;
    if (trait.args.size() > 0) {
        t.args[0] = trait.args[0];
    }
    if (trait.args.size() > 1) {
        t.args[1] = trait.args[1];
    }
    return t;
}

// 坚硬的实际减伤%（按星级）。
// ⚠️ 语料修正表，**不读 DB args**：必修6 实测「实际数值 5/6/8/10/9/10%（跟描述的
// 5/6/8/10/12/14% 也不符）……坚硬是唯一一个存在读数错误的特性，升到 3 星就和 5 星相同」。
// DB args 是 5/6/7/8/9/10（2/3 星两档错）。用户 2026-09-15 拍板：按语料。
// 时点口径（L254）：「坚硬是靠前的减伤，**乘法**计算，时点**早于保底伤害**」；
// 「不属于通用减伤时点」（不进 REDUCE_PCT 的加/乘算槽）、「只能免减攻击伤害（红伤）」。
inline int trait_hardness_pct(int star_level) {
    static constexpr int kByStar[6] = {5, 6, 8, 10, 9, 10};
    if (star_level < 0 || star_level > 5) {
        return 0;
    }
    return kByStar[star_level];
}

// 概率触发类特性的触发概率（千分点；无概率触发机制的 kind 恒 0 = 只能靠 proc_forced）。
inline int trait_proc_permille(const EffectiveTrait& t) {
    if (t.kind == TraitKind::InstantKill) {
        return t.args[0];
    }
    return 0;
}

// 触发掷点：被取消触发条件（proc_forced）→ 必发；否则 rand()%1000 < 千分点。
// 引擎既有随机都走 std::rand（暴击掷点/连击掷点/伤害浮动），保持同源。
inline bool trait_proc_roll(const EffectiveTrait& t) {
    if (t.proc_forced) {
        return true;
    }
    const int permille = trait_proc_permille(t);
    return permille > 0 && (std::rand() % 1000) < permille;
}

// 复制来的特性条目（天女式）。生命周期锚**来源方**（仿 RuleCenter 的 source 锚）：
// 查询时惰性判活 —— 来源方换宠（登场槽不匹配）或来源效果被断回合作废（epoch 不匹配）即失效。
struct TraitOverlay {
    EffectiveTrait trait;
    int source_owner = -1;    // 复制来源方 (0/1)
    int source_slot = -1;     // 复制时来源方的登场槽（-1 = 不锚槽位）
    int source_valid_id = 0;  // 复制时的 round_effect_valid_id[source_owner]（0 = 不参与断回合作废）
};

// 对某方特性的抑制条目（扎克斯式"抑制对手的瞬杀"）。挂在**被抑制方**的 TraitState 上，
// source 维度描述"谁抑制的"（生命周期同 TraitOverlay）。
struct TraitSuppression {
    TraitKind kind = TraitKind::None;
    int source_owner = -1;
    int source_slot = -1;
    int source_valid_id = 0;
};

// 每方一个：战斗内特性运行时状态。
//   - `own`：**当前登场精灵的特性槽**（登场/换宠时由 sync_on_stage_trait 从 pet.commonTrait
//     拷入；行为函数与查询入口读它，不再惰性读 pet 数据）。kind == None = 无特性。
//   - copies / suppressions：战斗内产生的复制（天女式）与抑制（扎克斯式）条目，
//     查询时惰性判活（来源方登场槽 + epoch）。
// ⚠️ 全队"谁带瞬杀特性"的**扫描类**检测（咤克斯/琉梦战斗开始数层）不读这里——
//     那是逐宠读 `seerRobot[side].elfPets[i].commonTrait` + `trait_kind_from_name` 的 pet 数据。
// clearAllEffects 清空。死条目靠查询时惰性判活（与 TimedBucket/管线同款），不做主动清理。
struct TraitState {
    EffectiveTrait own;   // 登场精灵特性槽（kind == None = 无）
    std::vector<TraitOverlay> copies;
    std::vector<TraitSuppression> suppressions;

    void clear() {
        own = EffectiveTrait{};
        copies.clear();
        suppressions.clear();
    }
};

#endif // TRAIT_STATE_H
