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
    // ── 接触毒特性（2026-09-16 接线；必修6《精灵特性》+ 用户拍板）──
    // 主动毒：静电/颤栗（Eid 66 家族）= **物理攻击**命中时 args[0]% 令对手 args[1] 号异常
    // （0=麻痹/6=害怕）；火热/极寒（Eid 67 家族）= **特殊攻击**命中（2=烧伤/5=冻伤）。
    // 触发门：技能命中（HIT）+ 物/特类别匹配；赋予 2~3 回合；本体走 apply_anomaly_ancient
    // （主动毒通道——无视 Modern 层免疫/弹控/抗性/转化，Ancient 层免疫/老魂免能挡）。
    // 被动毒：带电/高热/冰冷/阴森（Eid 6 家族）= **受到普通攻击（=物攻，用户 2026-09-16
    // 确认）命中时** args[0]% 令**攻方**中 args[1]。遗留机制的遗留机制：走 apply_anomaly_raw
    // **裸施加**（什么都不检测——免疫/弹控/抗性/转化全穿，用户 2026-09-16 拍板）。
    // ⚠️ 天女式复制条目**不经本特性节点触发**（用户 2026-09-16 架构拍板）：游戏实测——
    //    复制的主动/被动毒虽然"取消触发条件（物/特攻都行）"，但施加走**现代异常效果**
    //    （会被现代弹控、异常抗性响应），不再是主动毒 → 语义上已不是"特性施加"。
    //    落地方案：天女魂印把**行为保持**的特性（瞬杀等）拷到 context 槽（copy_common_trait，
    //    特性时点自动检测）；把**行为改变**的毒拷到 pet 容器 + 注册**不可清除**的效果到
    //    ON_SKILL_HIT 时点、直接调 apply_anomaly（现代原语）。故本族钩子只认**本体槽**。
    //    ⚠️ 别给 Eid 6/66/67 写 custom_effect_overrides 行——这些 effect_info id 与
    //    moves 用的同 id 不同义（effect_info 6=反弹伤害文本）。
    ActivePoisonPhysical,  // 静电 / 颤栗（主动毒·物理）
    ActivePoisonSpecial,   // 火热 / 极寒（主动毒·特殊）
    PassivePoison,         // 带电 / 高热 / 冰冷 / 阴森（被动毒·受物攻弹给攻方，裸施加）
    // ── 批次 A（2026-09-16，必修6《精灵特性》口径）──
    // ⚠️ 全族的**官方描述数值都是错的**（必修6 逐族给了实际内置值）；好在本项目用的
    //    `new_se.args` 里存的**就是修正后的实际值**（逐星级一行），故本层直读 args，
    //    只有"坚硬"一处 DB 也错（用 trait_hardness_pct 语料修正表）。
    SingleElementAmp,      // 单属性增伤 15 个（叶绿/流水/炎火/飞空/蓄电/机能/碎裂/平衡/冰霜/
                           //      魔幻/战意/光环/黑夜/奇异/威严）：
                           //      args[0]=官方属性 id（1 草=叶绿 … 15 龙=威严，同 skill_types.id
                           //      与 `Skills::element` 命名空间）；args[1]=增伤百分点（DB 5~10）。
                           //      ★ 落地在 AMP_EXTRA（"**额外**提升"=非通用增伤·乘法，同 693 与精神）；
                           //      ★ **双属性技能无效**（必修6："对指定单属性有效，双属性无效"）→
                           //        skill_element_view[1] 必须为 0；
                           //      ⚠️ 未建模的官方怪癖：该增伤"时点过于靠前"，
                           //        **会被变威力效果覆盖重写**（必修6 ①，经典威严王之哈莫）——
                           //        变威力重算会丢弃本增伤，待有需求时按"重算时不再施加"实现。
    Absorb,                // 吸收（Eid 60）：args[0]=触发概率**百分点**（5~14）、args[1]=**点数**（35~70）；
                           //      落地 REDUCE_FLAT（"减伤属于减少裸伤效果"=点数减伤，官方减伤区第一位），
                           //      受击时掷点 → final -= 点数。
    PassiveStatDrop,       // 被动属性降低 5 个（反抗/反驳/忽略/草率/慌张，Eid 34）：
                           //      "受到**特殊攻击**时有 n% 使对方 m 降低 1 个等级"。
                           //      args[0]=能力码、args[1]=概率百分点（5~14）。
                           //      ⚠️ **args[0] 与引擎 stat 索引不同序**（见 trait_stat_index_from_code）；
                           //      ⚠️ 走 `stat_drop`（对手施予的弱化 → 查免弱 STAT_DROP）。
    PassiveStatBoost,      // 被动属性提升 5 个（反击/抵抗/反攻/坚韧/借风，Eid 35）：
                           //      "受到**任何攻击**时有 n% 使**自身** m 提升 1 个等级"。
                           //      args[0]=能力码、args[1]=概率百分点。
                           //      ★ 时点（必修6 ①）："在技能**命中时之前**赋予能力提升，因此会被
                           //        对方一些技能带有消强/吸强/反强补偿影响" → 挂 BEFORE_SKILL_HIT；
                           //      ★ 必修6 ②："**属性技能可以触发**该特性" → 不按技能类别门控；
                           //      ⚠️ 走 `stat_change`（**自身增益**，不查免弱——与 PassiveStatDrop 相对）。
    // 已被官方废除、按"存在但无行为"处理（必修6：乱舞"依旧被废除"、0 星与白板无异；
    // 增伤 args 0/6/7/8/9/10 与"增加攻击目标数"args 全 0）→ 只登记 kind，行为 no-op。
    Unimplemented,
    // ── 批次 C（2026-09-16）：致死/存活族 ──
    // ★ 与 `force_hp_to_zero` 的关系（用户 2026-09-16 拍板）：**不加短路条件**——
    //   归零与"强制残留/回满"并不冲突，**由先后顺序决定实际效果**：本族的钩位在
    //   `trait_instant_kill_zero_hook`（瞬杀归零）**之后**，即"先归零、后判定存活"，
    //   于是顽强把 0 体力抬回 m 点、回神抬回满血。将来若发现口径要改，只需挪这一个调用点。
    //   （刻意不为它往归零原语里塞第三个短路分支——原语已有的两条短路是"转化/免疫"这类
    //     语义性豁免，不是"后手补救"。）
    Tenacious,  // 顽强（Eid 31/147）："受到致死攻击时有 n% 几率余下 m 点体力（0 体力也可以触发）"；
                //   args[0]=概率（见 trait_survive_prob_permille 的混编说明）、args[1]=残留点数（1/1/1/1/2/2）。
                //   ★ 仅**战斗阶段**触发（必修6 ①：回合结束后的致死伤害直接击杀，不触发）；
                //   ★ "强制残留体力，**不受削续航影响**"→ 直写 hp、不过 heal（不被封回血挡）。
    Revival,    // 回神（Eid 33/148）："体力降低到 1/{args[0]} 时有 n% 几率体力回满（0 体力也可以触发）"；
                //   args[0]=阈值分母（DB 全为 8 → 1/8）、args[1]=概率。同样仅战斗阶段、同样直写 hp。
    // ── 批次 B（2026-09-16）：命中/暴击族（必修6 的机制修正已并入注释）──
    // ⚠️ 这一族**修正了三处望文生义**：免爆不是降暴击率而是**对暴击挡伤**；
    //    虚无不是挡伤而是**概率闪避**；会心 0 星是**与初始暴击率加法**、1-5 星才是独立二次结算。
    Precision,     // 精准（Eid 29）："所有技能命中率提升 n%"；args[0]=百分点（DB 5~10）。
                   //   ★ 与技能初始命中率**乘法**（必修6：90×1.1=99）；**可作用于属性技能**。
                   //   无概率掷点（是命中率修正，不是概率效果）。
    Evasion,       // 回避（Eid 7）："被技能命中的几率减少 n%"；args[0]=百分点（DB 5~10）。
                   //   ★ 与精准同款乘法（90×0.9=81）；★ 本质是**降低对手命中率**、不是概率效果
                   //   （故不会被"概率提升"类效果推到 100%，必修6 ③）；也作用于属性技能。
    CritBoost,     // 会心（Eid 30/146）："所有技能的致命一击率增加"；args[0]：**0 星=1（=1/16=6.25%）**、
                   //   1-5 星 = 75/88/100/120/140（= 7.5/8.8/10/12/14%，×10 即万分数）。
                   //   ★ 0 星与技能**初始暴击率加法**；1-5 星**独立二次结算**（必修6 ①②）。
    CritImmunity,  // 免爆（Eid 64/150）："受到致命一击的概率降低"——⚠️ **实际是对暴击的挡伤**：
                   //   "当回合免疫受到的暴击伤害"，**不是**降低暴击率（必修6）。args[0] 同会心编码。
                   //   落点：BLOCK 阶段读 `DamageSnapshot::isCrit` → 掷点 → 本次伤害归 0。
    VoidDodge,     // 虚无（Eid 61/149）：描述"n%几率完全抵挡一次伤害"，⚠️ **实际是有概率闪避
                   //   对手攻击技能**（必修6：本质是闪避、不是挡伤）。args[0] 概率（见下混编说明）；
                   //   只对**攻击技能**生效（"闪避对手攻击技能"）。
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
    if (name == "静电" || name == "颤栗" || name == "战栗") {
        return TraitKind::ActivePoisonPhysical;
    }
    if (name == "火热" || name == "极寒") {
        return TraitKind::ActivePoisonSpecial;
    }
    if (name == "带电" || name == "高热" || name == "冰冷" || name == "阴森") {
        return TraitKind::PassivePoison;
    }
    // 批次 A：单属性增伤 15 个（属性 id 从 args[0] 读，名字只用于定型）
    if (name == "叶绿" || name == "流水" || name == "炎火" || name == "飞空"
        || name == "蓄电" || name == "机能" || name == "碎裂" || name == "平衡"
        || name == "冰霜" || name == "魔幻" || name == "战意" || name == "光环"
        || name == "黑夜" || name == "奇异" || name == "威严") {
        return TraitKind::SingleElementAmp;
    }
    if (name == "吸收") {
        return TraitKind::Absorb;
    }
    if (name == "反抗" || name == "反驳" || name == "忽略" || name == "草率"
        || name == "慌张") {
        return TraitKind::PassiveStatDrop;
    }
    if (name == "反击" || name == "抵抗" || name == "反攻" || name == "坚韧"
        || name == "借风") {
        return TraitKind::PassiveStatBoost;
    }
    if (name == "顽强") {
        return TraitKind::Tenacious;
    }
    if (name == "回神") {
        return TraitKind::Revival;
    }
    if (name == "精准") {
        return TraitKind::Precision;
    }
    if (name == "回避") {
        return TraitKind::Evasion;
    }
    if (name == "会心") {
        return TraitKind::CritBoost;
    }
    if (name == "免爆") {
        return TraitKind::CritImmunity;
    }
    if (name == "虚无") {
        return TraitKind::VoidDodge;
    }
    return TraitKind::None;
}

// ★ 顽强/回神族的概率解码（**混编编码**，务必按族取值）：
//   必修6 给出的实际值 = 3/3.5/4/5/6/7%；而 DB args = 3/35/40/50/60/70。
//   → args < 10 按**整百分点**读（3 → 3%）；args ≥ 10 按**千分点**读（35 → 3.5%、70 → 7%）。
//   ⚠️ 与瞬杀族**同数字不同义**（瞬杀 args=3 → 0.3%，本族 args=3 → 3%）——别共用一套换算。
//   ⚠️ 0 星档是唯一有歧义处（若按瞬杀口径读则 0.3%）：本文按必修6 明写的"3%"取值，
//      将来实测若推翻只需改本函数一处。
inline int trait_survive_prob_permille(int raw) {
    if (raw <= 0) {
        return 0;
    }
    return raw < 10 ? raw * 10 : raw;
}

// ★ 被动属性族（Eid 34/35）的 args[0] → 引擎 stat 索引（`stat_change`/`stat_drop` 的能力下标：
//   0=攻击 1=特攻 2=防御 3=特防 4=速度）。
// ⚠️ **两者不同序，必须重映射**——依据是数据库 `new_se.intro`（本项目"最终以数据库为准"）：
//   `34|1 5|反驳|受到特殊攻击时有5%几率使对方**防御**降低1个等级`
//   `34|2 5|忽略|受到特殊攻击时有5%几率使对方**特攻**降低1个等级`
//   即官方码序 = 0攻击 / 1防御 / 2特攻 / 3特防 / 4速度；引擎码序 = 0攻击 / 1特攻 / 2防御 / …
//   → 直接透传会把"防御"和"特攻"互换（这类静默错位最难查，故单列成函数 + 证词）。
// 注：Eid 26（数值提升，stat=2）又是第三套码序（1防御/2特防/3攻击/4特攻/5速度），
//     本函数**只管 Eid 34/35**，别的族用前先各自核对 intro。
inline int trait_stat_index_from_code(int code) {
    switch (code) {
        case 0: return 0;   // 攻击
        case 1: return 2;   // 防御
        case 2: return 1;   // 特攻
        case 3: return 3;   // 特防
        case 4: return 4;   // 速度
        default: return -1;
    }
}

// 一次查询拿到的"生效特性"（本体或复制条目的解析结果）。
// args 语义按 kind：
//   - InstantKill：args[0] = 触发概率**千分点**（3→0.3%、13→1.3%、15→1.5%、20→2%、25→2.5%、30→3%）。
//     ⚠️ 赛学必修6《精灵特性》：描述写的 3/3.5/4/5/6/7% 是**假的**，真实内置概率 = args/10%。
//   - Hardness：**不读 args**（官方读数错误）——用 trait_hardness_pct 的语料修正表。
//   - Spirit：args[0] = 官方 Category 代码（2=特殊攻击，与 map_skill_type 的 1物理/2特殊/4属性 一致）、
//     args[1] = 增伤百分比。
//   - ActivePoisonPhysical/Special、PassivePoison：args[0] = 触发概率**百分点**（必修6：
//     0-5 星 = 3/4/5/6/7/8%，与瞬杀的千分点口径不同！）、args[1] = 施加的异常码
//     （battle_effects 命名空间=引擎 AbnormalStatusId：带电/静电 0 麻痹 / 阴森/颤栗 6 害怕 /
//     高热/火热 2 烧伤 / 冰冷/极寒 5 冻伤）。
//   - SingleElementAmp：args[0] = 官方属性 id、args[1] = 增伤百分点（无概率掷点）。
//   - Absorb：args[0] = 概率百分点、args[1] = 减伤点数。
//   - PassiveStatDrop / PassiveStatBoost：args[0] = **官方能力码**（须过
//     trait_stat_index_from_code 重映射）、args[1] = 概率百分点。
//   ⚠️ 概率的**存放位置逐族不同**（毒在 args[0]、被动属性在 args[1]），故 trait_proc_permille
//      按 kind 分支取——不要写统一的 "args[0]" 假设。
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
    if (t.kind == TraitKind::ActivePoisonPhysical
        || t.kind == TraitKind::ActivePoisonSpecial
        || t.kind == TraitKind::PassivePoison) {
        return t.args[0] * 10;   // 必修6：args[0] 是百分点（3~8）→ 千分点
    }
    if (t.kind == TraitKind::Absorb) {
        return t.args[0] * 10;   // 概率在 args[0]
    }
    if (t.kind == TraitKind::PassiveStatDrop || t.kind == TraitKind::PassiveStatBoost) {
        return t.args[1] * 10;   // ⚠️ 概率在 args[1]（args[0] 是能力码）
    }
    if (t.kind == TraitKind::Tenacious) {
        return trait_survive_prob_permille(t.args[0]);   // 概率在 args[0]、残留点数是 args[1]
    }
    if (t.kind == TraitKind::Revival) {
        return trait_survive_prob_permille(t.args[1]);   // ⚠️ 概率在 args[1]（args[0] 是 1/N 的分母）
    }
    if (t.kind == TraitKind::VoidDodge) {
        // ⚠️ 同族混编编码（见 trait_survive_prob_permille 的说明）：必修6 给的实际值
        //    1/1.3/1.5/2/2.5/3% 对应 DB args 1/13/15/20/25/30 → raw<10 按整百分点读。
        return trait_survive_prob_permille(t.args[0]);
    }
    if (t.kind == TraitKind::CritImmunity) {
        // 免爆：0 星 args=1 → 1/16 = 6.25% = 62.5‰；1-5 星 args=75..140 → ×10 即千分点（75→7.5%）。
        if (t.args[0] <= 1) {
            return 62;   // 6.25% 取整到千分点
        }
        return t.args[0] * 10;
    }
    return 0;
}

// 触发掷点：被取消触发条件（proc_forced）→ 必发；概率 ≥1000‰（=100%）→ 必发**且不消耗
// rand()**；否则 rand()%1000 < 千分点。
// 引擎既有随机都走 std::rand（暴击掷点/连击掷点/伤害浮动），保持同源。
// ⚠️ 100% 档**短路不消耗**是刻意为之，与本仓库既有的"不调用=不消耗"契约一致
//    （参见 skills.h 连击区间退化 / battleFsm.cpp 无连击模板 / 暴击率 0 短路）——
//    否则一次"必发"掷点会推动全局随机序列，使无关的伤害浮动/闪避判定整体错位
//    （2026-09-16 已因此发生过 4 个场景的回归）。
inline bool trait_proc_roll(const EffectiveTrait& t) {
    if (t.proc_forced) {
        return true;
    }
    const int permille = trait_proc_permille(t);
    if (permille <= 0) {
        return false;
    }
    if (permille >= 1000) {
        return true;   // 100%：不消耗 rand()
    }
    return (std::rand() % 1000) < permille;
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
