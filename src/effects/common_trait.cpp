#include <effects/common_trait_effects.h>

#include <effects/trait_state.h>
#include <entities/skills.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>

// ═══════════════════════════════════════════════════════════════════════════
// 通用特性（new_se stat=1）行为层。
//
// 安装形态与 install_default_* 一致：TEAM 绑定的常驻管线条目，**每方一个**，
// 回调里经 `effective_common_trait` 实时读**当前登场精灵**的生效特性——
// 切换/死亡天然正确（trait 没有就不做事），不需要 per-switch 重装。
// 查询/抑制/复制的可编程入口在 battleContext.h（TraitState 运行时层）。
//
// 机制口径（赛学必修6《精灵特性》+ 用户 2026-09-15/16 两轮实测拍板）：
//   - 瞬杀 **0-5 星统一 = 归零秒杀**（游戏更新后没有红伤修正了，星级只差触发概率）：
//     红伤结算完之后，经 `force_hp_to_zero` 原语强制对手体力归零。原语是秒杀族统一口子：
//     每次调用 emit EVENT_HP_TO_ZERO（blocked=是否被短路）；目标方"秒杀转化"标记或
//     来源方瞬杀特性被抑制 → 短路（咤克斯咒怨 +1 仍收得到事件）。犀牛回血挂
//     EVENT_TAKE_DAMAGE（drain 在状态桶之后）→ 回血晚于归零 → 高伤+瞬杀同时触发犀牛不死。
//   - 坚硬：靠前的乘法减伤，非通用减伤区、早于保底（L254），只免红伤；数值用语料修正表
//     （trait_hardness_pct，不读 DB args——官方读数错误）。
//   - 精神：特殊攻击伤害 +n%，非通用增伤 → 乘法（AMP_EXTRA 阶段），按 ws.skill_type_view 门控。
// ═══════════════════════════════════════════════════════════════════════════

namespace {

// 本次管线是否对应"攻击技能命中"的结算（瞬杀两变体的公共门）：
// 命中（HIT）才触发；miss/被封/命中失效都到不了管线（SKILL_INVALID 早退）或在此拦下。
bool instant_kill_gate(const BattleContext* ctx, int attacker_id) {
    if (ctx->ws.skill_exec_result[attacker_id] != SkillExecResult::HIT) {
        return false;   // 必修6：瞬杀需要攻击技能命中才能触发（0 星口径同，用户拍板）
    }
    if (ctx->ws.skill_type_view[attacker_id] == static_cast<int>(SkillType::Attribute)) {
        return false;   // 「进攻类技能」：物理/特殊可以，属性技能不行
    }
    return true;
}

//---- 瞬杀 0-5 星：红伤落地后的归零秒杀（钩位见 trait_instant_kill_zero_hook）----
// （旧"0 星红伤拉高"口径已废——游戏更新后 0-5 星统一为秒杀效果；TRAIT_REPLACE 阶段
//   已从管线移除，kOrder 常量恢复 11 阶段。）

//---- 坚硬：靠前的乘法减伤（REDUCE_TRAIT 阶段）----
void install_hardness(BattleContext* ctx, int owner) {
    ctx->register_default_damage_effect(
        DamagePhase::REDUCE_TRAIT,
        owner,
        DamageEffectCategory::MITIGATE,   // 减伤：不被"挡伤失效"无视（官方口径同减伤区）
        [](BattleContext* c, int bucket_owner) {
            DamageSnapshot& damage = c->resolvedDamage;
            if (damage.defenderId != bucket_owner) {
                return;   // 减伤从"承受方"的桶读
            }
            if (!damage.isRed || damage.final <= 0) {
                return;   // 只免红伤（必修6：坚硬只能免减攻击伤害）
            }
            const std::optional<EffectiveTrait> trait =
                c->effective_common_trait(bucket_owner, TraitKind::Hardness);
            if (!trait) {
                return;
            }
            const int pct = trait_hardness_pct(trait->star_level);
            if (pct <= 0) {
                return;
            }
            damage.final = damage.final * (100 - pct) / 100;   // 乘法（非通用减伤区）
        }
    );
}

//---- 精神：特殊攻击伤害 +n%（AMP_EXTRA 阶段，非通用增伤·乘法）----
void install_spirit(BattleContext* ctx, int owner) {
    ctx->register_default_damage_effect(
        DamagePhase::AMP_EXTRA,
        owner,
        DamageEffectCategory::AMP,
        [](BattleContext* c, int bucket_owner) {
            DamageSnapshot& damage = c->resolvedDamage;
            if (damage.attackerId != bucket_owner) {
                return;   // 增伤从"攻击方"的桶读
            }
            if (damage.final <= 0) {
                return;
            }
            const std::optional<EffectiveTrait> trait =
                c->effective_common_trait(bucket_owner, TraitKind::Spirit);
            if (!trait) {
                return;   // 先查特性（早退），再查技能类别
            }
            // 类别门控**读 args[0]**（官方类别码 1=物理=强袭 / 2=特殊=精神），
            // 经 trait_skill_type_from_official_code 换算成引擎 SkillType 再比
            //（⚠️ 两套码不同：官方 1/2/4 vs 引擎 0/1/2；早年硬编码"只认特殊"会让强袭失效）。
            if (c->ws.skill_type_view[bucket_owner]
                != trait_skill_type_from_official_code(trait->args[0])) {
                return;   // 「物理攻击」吃强袭、「特殊攻击」吃精神，互不串门
            }
            if (trait->args[1] == 0) {
                return;
            }
            damage.final = damage.final * (100 + trait->args[1]) / 100;
        }
    );
}

//---- 单属性增伤 15 个（叶绿/流水/…/威严）：AMP_EXTRA 阶段 · 非通用增伤·乘法 ----
// 必修6："某单属性技能造成的伤害额外提升 n%"、"对指定单属性有效，**双属性无效**"。
// 参数：args[0]=官方属性 id（同 skill_types.id 与 Skills::element），args[1]=增伤百分点。
// ⚠️ 未建模的官方怪癖：该增伤"时点过于靠前"，**会被变威力效果覆盖重写**（必修6 ①）——
//    变威力二次结算会丢弃它，待有真实需求时再按"重算不施加"补。
void install_single_element_amp(BattleContext* ctx, int owner) {
    ctx->register_default_damage_effect(
        DamagePhase::AMP_EXTRA,
        owner,
        DamageEffectCategory::AMP,
        [](BattleContext* c, int bucket_owner) {
            DamageSnapshot& damage = c->resolvedDamage;
            if (damage.attackerId != bucket_owner) {
                return;   // 增伤从"攻击方"的桶读
            }
            if (damage.final <= 0) {
                return;
            }
            const std::optional<EffectiveTrait> trait =
                c->effective_common_trait(bucket_owner, TraitKind::SingleElementAmp);
            if (!trait || trait->args[1] <= 0) {
                return;   // 先查特性（早退，不给无毒特性者增加行为）
            }
            // 单属性门：技能必须是**单一属性**，且首属性 = 特性指定属性
            if (c->ws.skill_element_view[bucket_owner][0] != trait->args[0]) {
                return;
            }
            if (c->ws.skill_element_view[bucket_owner][1] != 0) {
                return;   // 双属性技能无效（必修6）
            }
            damage.final = damage.final * (100 + trait->args[1]) / 100;   // 乘法（非通用增伤）
        }
    );
}

//---- 免爆（Eid 64/150）：BLOCK 阶段 · **对暴击的挡伤** ----
// ⚠️ 必修6 修正了望文生义："免爆实际为**当回合免疫受到的暴击伤害**（对暴击进行一个挡伤效果），
//    而不是降低暴击概率" → 不是改暴击率，是在伤害落地的挡伤位把这次**暴击伤害**归零。
// 落点 BLOCK：与"免疫下1次攻击伤害"同阶段（唯一的归零位）。
void install_crit_immunity(BattleContext* ctx, int owner) {
    ctx->register_default_damage_effect(
        DamagePhase::BLOCK,
        owner,
        DamageEffectCategory::MITIGATE,
        [](BattleContext* c, int bucket_owner) {
            DamageSnapshot& damage = c->resolvedDamage;
            if (damage.defenderId != bucket_owner) {
                return;   // 挡伤从"承受方"的桶读
            }
            if (!damage.isCrit || damage.final <= 0) {
                return;   // 只对**暴击**伤害挡（非暴击照常）
            }
            const std::optional<EffectiveTrait> trait =
                c->effective_common_trait(bucket_owner, TraitKind::CritImmunity);
            if (!trait) {
                return;   // 先查特性（早退）——⚠️ rand 只在此后消耗
            }
            if (!trait_proc_roll(*trait)) {
                return;
            }
            damage.final = 0;   // 挡下本次暴击伤害
        }
    );
}

//---- 强攻(62)/强念(63)：追加伤害（AMP_EXTRA 点数加成）----
// DB desc："物理攻击有 n% 几率使伤害提高 m 点"（强攻）/ "特殊攻击有…"（强念）；
// 必修6 ①："属于**追加伤害，与红伤一并结算**" → 落在红伤上、受后续减伤与保底约束。
// ②miss 也扣血的那一半见 `trait_extra_damage_on_miss_hook`。
// ⚠️ 只有**确带该特性**才消耗 rand（惰性：先查特性、再掷点）。
void install_extra_damage(BattleContext* ctx, int owner) {
    ctx->register_default_damage_effect(
        DamagePhase::AMP_EXTRA,
        owner,
        DamageEffectCategory::AMP,
        [](BattleContext* c, int bucket_owner) {
            DamageSnapshot& damage = c->resolvedDamage;
            if (damage.attackerId != bucket_owner || !damage.isRed || damage.final <= 0) {
                return;   // 只对红伤、且是攻击方自己的桶
            }
            const int skill_type = c->ws.skill_type_view[bucket_owner];
            TraitKind want = TraitKind::None;
            if (skill_type == static_cast<int>(SkillType::Physical)) {
                want = TraitKind::ExtraDamagePhysical;   // 强攻
            } else if (skill_type == static_cast<int>(SkillType::Special)) {
                want = TraitKind::ExtraDamageSpecial;    // 强念
            } else {
                return;
            }
            const std::optional<EffectiveTrait> trait = c->effective_common_trait(bucket_owner, want);
            if (!trait || trait->args[1] <= 0) {
                return;
            }
            if (!trait_proc_roll(*trait)) {
                return;
            }
            damage.final += trait->args[1];   // 追加伤害（点数）
        }
    );
}

//---- 吸收（Eid 60）：REDUCE_FLAT 阶段 · 点数减伤 ----
// 必修6："受到攻击时 m% 几率使受到的伤害降低 n 点"、"减伤属于**减少裸伤**效果"；
// 点数减伤是官方减伤区第一位（先扣点数、再算百分比）。
void install_absorb(BattleContext* ctx, int owner) {
    ctx->register_default_damage_effect(
        DamagePhase::REDUCE_FLAT,
        owner,
        DamageEffectCategory::MITIGATE,
        [](BattleContext* c, int bucket_owner) {
            DamageSnapshot& damage = c->resolvedDamage;
            if (damage.defenderId != bucket_owner) {
                return;   // 减伤从"承受方"的桶读
            }
            if (!damage.isRed || damage.final <= 0) {
                return;   // 只免红伤（"使受到的伤害降低"=攻击伤害）
            }
            const std::optional<EffectiveTrait> trait =
                c->effective_common_trait(bucket_owner, TraitKind::Absorb);
            if (!trait || trait->args[1] <= 0) {
                return;   // 先查特性（早退）——⚠️ rand 只在此后消耗
            }
            if (!trait_proc_roll(*trait)) {
                return;
            }
            damage.final = std::max(0, damage.final - trait->args[1]);
        }
    );
}

}  // namespace

//---- 被动属性降低 5 个（反抗/反驳/忽略/草率/慌张，Eid 34）----
// "受到**特殊攻击**时有 n% 使对方 m 降低 1 个等级"。钩位：ON_SKILL_HIT（同上，需命中）。
// ⚠️ args[0] 是**官方能力码**，与引擎 stat 索引不同序 → 必过 trait_stat_index_from_code
//    （依据 DB intro：`34|1 5|反驳|…使对方**防御**降低1个等级` —— 透传会把防御/特攻互换）。
// ⚠️ 走 `stat_drop`：这是"对手施予的弱化" → 查免弱 STAT_DROP（必修6 ① 也提到面对免弱精灵
//    无法正常赋予）。
void trait_passive_stat_drop_hook(BattleContext* ctx, int attacker_id) {
    if (!ctx || attacker_id < 0 || attacker_id > 1) {
        return;
    }
    if (ctx->ws.skill_exec_result[attacker_id] != SkillExecResult::HIT) {
        return;   // 需要这次攻击命中
    }
    if (ctx->ws.skill_type_view[attacker_id] != static_cast<int>(SkillType::Special)) {
        return;   // "受到**特殊攻击**时"——只认特攻
    }
    const int defender_id = 1 - attacker_id;
    const EffectiveTrait& def_own = ctx->trait_state_[defender_id].own;
    if (def_own.kind != TraitKind::PassiveStatDrop
        || ctx->is_trait_suppressed(defender_id, def_own.kind)) {
        return;
    }
    const int stat = trait_stat_index_from_code(def_own.args[0]);
    if (stat < 0 || !trait_proc_roll(def_own)) {
        return;   // ⚠️ rand 只在特性确在时消耗
    }
    (void)stat_drop(ctx, attacker_id, stat, 1);   // 使**对方**降 1 级
}

//---- 被动属性提升 5 个（反击/抵抗/反攻/坚韧/借风，Eid 35）----
// "受到**任何攻击**时有 n% 使**自身** m 提升 1 个等级"。钩位：BEFORE_SKILL_HIT——
// 必修6 ①："在技能**命中时之前**赋予能力提升，因此会被对方一些技能带有消强/吸强/反强补偿
// 影响"（先赋予、后挨打，对方才能消掉它）。
// 必修6 ②："**属性技能可以触发**该特性" → **不按技能类别门控**（也不要求命中：赋予发生在
// 命中判定之前）。⚠️ 属**自身增益** → 走 stat_change（不查免弱，与 PassiveStatDrop 相对）。
void trait_pre_hit_stat_boost_hook(BattleContext* ctx, int actor_id) {
    if (!ctx || actor_id < 0 || actor_id > 1) {
        return;
    }
    // "受到攻击"= 对方出手，故读**被攻击方**（非出手方）的登场特性槽
    const int defender_id = 1 - actor_id;
    const EffectiveTrait& def_own = ctx->trait_state_[defender_id].own;
    if (def_own.kind != TraitKind::PassiveStatBoost
        || ctx->is_trait_suppressed(defender_id, def_own.kind)) {
        return;
    }
    const int stat = trait_stat_index_from_code(def_own.args[0]);
    if (stat < 0 || !trait_proc_roll(def_own)) {
        return;   // ⚠️ rand 只在特性确在时消耗
    }
    (void)stat_change(ctx, defender_id, stat, 1);   // 使**自身**升 1 级
}


//---- 顽强（Eid 31/147）/ 回神（Eid 33/148）：致死存活 ----
// 钩位：finish_attack_damage 里**瞬杀归零之后**（用户 2026-09-16："看先后顺序定实际效果，
// 不要动不动就短路"）→ 先归零、再判定留存/回满。
// 必修6 口径：①仅**战斗阶段**触发（回合结束后的致死伤害直接击杀）——本钩只挂在攻击伤害
// 出口，回合结束的粉/真伤路径不经过，天然满足；②"强制残留体力，**不受削续航影响**"
// → 直写 hp（不过 heal，不被封回血挡）；③两者"0 体力也可以触发"。
void trait_survive_lethal_hook(BattleContext* ctx, int defender_id) {
    if (!ctx || defender_id < 0 || defender_id > 1) {
        return;
    }
    // 本次攻击确实结算了伤害（防"对已倒地的目标再次进场"把尸体救活）
    if (ctx->resolvedDamage.defenderId != defender_id || ctx->resolvedDamage.final <= 0) {
        return;
    }
    ElfPet& defender = ctx->seerRobot[defender_id].elfPets[ctx->on_stage[defender_id]];
    const EffectiveTrait& own = ctx->trait_state_[defender_id].own;
    if (own.kind != TraitKind::Tenacious && own.kind != TraitKind::Revival) {
        return;
    }
    if (ctx->is_trait_suppressed(defender_id, own.kind)) {
        return;
    }
    if (own.kind == TraitKind::Tenacious) {
        // "受到**致死**攻击时"：本次伤害后体力已 ≤0 才判定
        if (defender.hp > 0 || !trait_proc_roll(own)) {
            return;   // ⚠️ rand 只在致死时消耗
        }
        defender.hp = own.args[1] > 0 ? own.args[1] : 1;   // 余下 m 点（1/1/1/1/2/2）
        return;
    }
    // 回神："体力降低到 1/{args[0]} 时回满"（DB 分母恒为 8）
    const int denom = own.args[0] > 0 ? own.args[0] : 8;
    const int max_hp = defender.numericalBase[NumericalPropertyIndex::HP];
    if (max_hp <= 0 || defender.hp > max_hp / denom) {
        return;
    }
    if (!trait_proc_roll(own)) {
        return;
    }
    defender.hp = max_hp;
}

void install_common_trait_effects(BattleContext* ctx) {
    if (!ctx) {
        return;
    }
    for (int owner = 0; owner < 2; ++owner) {
        install_hardness(ctx, owner);
        install_spirit(ctx, owner);
        install_single_element_amp(ctx, owner);
        install_absorb(ctx, owner);
        install_crit_immunity(ctx, owner);
        install_extra_damage(ctx, owner);
    }
}

void trait_instant_kill_zero_hook(BattleContext* ctx, int attacker_id) {
    if (!ctx || attacker_id < 0 || attacker_id > 1) {
        return;
    }
    if (!instant_kill_gate(ctx, attacker_id)) {
        return;
    }
    // ⚠️ include_suppressed=true：瞬杀被抑制 ≠ 不触发——照常掷点、照常调原语，
    //    抑制表现为原语短路（不归零、事件 blocked=true，咤克斯咒怨照样 +1）。
    const std::optional<EffectiveTrait> trait =
        ctx->effective_common_trait(attacker_id, TraitKind::InstantKill,
                                    /*include_suppressed=*/true);
    if (!trait) {
        return;
    }
    if (!trait_proc_roll(*trait)) {
        return;
    }
    const int defender_id = 1 - attacker_id;
    ElfPet& defender = ctx->seerRobot[defender_id].elfPets[ctx->on_stage[defender_id]];
    if (defender.hp <= 0) {
        return;
    }
    // 红伤已结算完（本钩位在 apply_resolved_damage 之后）→ 强制体力归零。
    // 犀牛式回血挂 EVENT_TAKE_DAMAGE、事件 drain 在状态桶之后 → 回血晚于归零，
    // 高伤+瞬杀同时触发时犀牛最后仍满血（用户 2026-09-15/16 实测口径）。
    (void)force_hp_to_zero(ctx, defender_id, attacker_id);
}

//---- 接触毒特性（静电/颤栗/火热/极寒 主动毒 + 带电/高热/冰冷/阴森 被动毒）----
// 钩位：BATTLE_FIRST/SECOND_ON_SKILL_HIT 的注册桶之后、技能效果结算（SKILL_EFFECT）之前
// ——必修6："需要（自身）技能命中才可以触发"；被盔技能 SKILL_INVALID（不计为命中，
// 必修3）时 handler 连注册桶一起跳过，钩子自然不触发。
//
// ⚠️ 只认**本体槽**（天女式复制毒不经特性节点——见 trait_state.h 的架构拍板：
// 复制毒走现代异常效果，由天女魂印自己注册效果到本时点、调 apply_anomaly）。
//---- 强攻/强念的 miss 分支：攻击技能 miss 也扣血（必修6 ②）----
// 时点：`Skills::execute` 的 miss 出口（**只对 MISS，不对被盔/封技的 SEALED**——后者技能
// 根本没打出去，与"miss 也算使出过这一击"不同）。
// 伤害形态取 `deal_damage(NORMAL)`：走红伤出口（吃护盾/事件），与"与红伤一并结算"同源。
void trait_extra_damage_on_miss_hook(BattleContext* ctx, int attacker_id) {
    if (!ctx || attacker_id < 0 || attacker_id > 1) {
        return;
    }
    const int skill_type = ctx->ws.skill_type_view[attacker_id];
    TraitKind want = TraitKind::None;
    if (skill_type == static_cast<int>(SkillType::Physical)) {
        want = TraitKind::ExtraDamagePhysical;
    } else if (skill_type == static_cast<int>(SkillType::Special)) {
        want = TraitKind::ExtraDamageSpecial;
    } else {
        return;   // 属性技能不参与（"攻击技能 miss 了也…"）
    }
    const std::optional<EffectiveTrait> trait = ctx->effective_common_trait(attacker_id, want);
    if (!trait || trait->args[1] <= 0) {
        return;
    }
    if (!trait_proc_roll(*trait)) {
        return;
    }
    const int defender_id = 1 - attacker_id;
    if (ctx->getPet(defender_id).hp <= 0) {
        return;
    }
    (void)deal_damage(ctx, defender_id, trait->args[1], DamageKind::NORMAL, attacker_id);
}

void trait_contact_poison_hook(BattleContext* ctx, int attacker_id) {
    if (!ctx || attacker_id < 0 || attacker_id > 1) {
        return;
    }
    if (ctx->ws.skill_exec_result[attacker_id] != SkillExecResult::HIT) {
        return;   // 必修6 ②③：需要技能命中（miss 不触发）
    }
    const int skill_type = ctx->ws.skill_type_view[attacker_id];
    if (skill_type != static_cast<int>(SkillType::Physical)
        && skill_type != static_cast<int>(SkillType::Special)) {
        return;   // 属性技能不触发（"物理攻击或者特殊攻击"/"受到普通攻击"）
    }
    const bool is_physical = (skill_type == static_cast<int>(SkillType::Physical));
    const int defender_id = 1 - attacker_id;

    // ── 主动毒：攻方本体槽，物/特类别匹配后令**守方**中招（主动毒通道）──
    // ⚠️ `std::rand()` 只在**确实要施加**时才消耗（duration 的计算放在分支内）——
    //    本仓库的伤害/命中断言依赖既有 rand 序列（场景 013/031/053/054 都因此抓过），
    //    无条件掷点会给所有场景移位。
    const EffectiveTrait& atk_own = ctx->trait_state_[attacker_id].own;
    const TraitKind active_want = is_physical ? TraitKind::ActivePoisonPhysical
                                              : TraitKind::ActivePoisonSpecial;
    if (atk_own.kind == active_want && !ctx->is_trait_suppressed(attacker_id, atk_own.kind)
        && is_valid_abnormal_status_id(atk_own.args[1]) && trait_proc_roll(atk_own)) {
        const ElfPet& defender = ctx->seerRobot[defender_id].elfPets[ctx->on_stage[defender_id]];
        if (defender.hp > 0) {
            const int duration = 2 + std::rand() % 2;   // 必修6：赋予回合随机 2~3
            (void)apply_anomaly_ancient(ctx, defender_id, atk_own.args[1], duration,
                                        attacker_id);
        }
    }

    // ── 被动毒：守方本体槽，"受到普通攻击（=物攻，用户 2026-09-16 确认）"时令**攻方**中招。
    //    遗留裸施加：apply_anomaly_raw 什么都不检测（免疫/弹控/抗性/转化全穿）。
    if (is_physical) {
        const EffectiveTrait& def_own = ctx->trait_state_[defender_id].own;
        if (def_own.kind == TraitKind::PassivePoison
            && !ctx->is_trait_suppressed(defender_id, def_own.kind)
            && is_valid_abnormal_status_id(def_own.args[1]) && trait_proc_roll(def_own)) {
            const ElfPet& attacker = ctx->seerRobot[attacker_id].elfPets[ctx->on_stage[attacker_id]];
            if (attacker.hp > 0) {
                const int duration = 2 + std::rand() % 2;   // 必修6：赋予回合随机 2~3
                (void)apply_anomaly_raw(ctx, attacker_id, def_own.args[1], duration,
                                        defender_id);
            }
        }
    }
}
