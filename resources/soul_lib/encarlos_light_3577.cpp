// 光之惩戒·英卡洛斯 3577（光螳螂）—— 魂印 1164 完整版
//
// 官方魂印（DB effect_icon id=881 / effect_id=1164，pet_id=3577）：
//   ① 天生开启光之惩戒形态：自身所有技能先制+1，免疫所有能力下降状态，
//      攻击技能伤害计算时克制倍数至少为2；
//   ② 自身使用攻击技能则出手流程结束时，若本次攻击造成的伤害高于280则附加对手最大
//      体力1/3的百分比伤害且令对手下回合使用的属性技能无效，若本次攻击造成的伤害
//      低于280则{吸取}对手最大{体力}的1/3且令自身下回合攻击技能必定打出致命一击；
//   ③ 自身首次死亡时有100%的概率得到1次圣光祝福：当回合残留1点体力，同时战斗阶段
//      结束时解除自身异常状态且令自身下3次攻击技能造成的伤害提升150%（所有特效boss无效）
//
// 实现口径（2026-09-24 批 9，证据=语料 opus 866772495396503575《死或生》/ 947280935819149312
// 《免死检测》/ 1134971092549500952 + 用户 2026-09-24 口径）：
//   - ①先制+1 = MOVE_RIGHT 桶 ws.preemptive_level += 1（极战·阿尔斯兰同款，"所有技能"无条件）；
//     免降 = 登场 grant STAT_DROP 免疫票（ON_STAGE epoch，离场自动失效）；
//     锁 = SKILL_EFFECT 直写 restraint_view=max(自然,2)——免疫(0)/微弱(0.5)都抬 2，抬完
//     结算的是**正常红伤**（吃护盾/减伤，只是违背无效——用户 2026-09-24 更正，场景 121 A2 盾断言）。
//   - ②伤害读数 = ws.raw_attack_damage（"造成伤害"裸伤台账，422 族同源）；**严格**大于/小于：
//     恰好 ==280 两个分支都不触发（用户口径，"很多针对措施"的卡阈值打法）。miss/被盔
//     （无伤害公式）与属性技能不触发；raw==0 不算"低于280"。
//     >280 → 附加 1/3 百分比粉伤 + 记"下回合封属"标记；<280 → 吸取 1/3 + 记"下回合必暴"标记。
//   - ②封属**不是回合类效果**（用户口径）：出手流程结束时（ACTION_END）只记标记，真注册
//     发生在**下回合对手命中前桶**的镜像节点里——对手切走（槽位不符）或英卡洛斯自己
//     切走（在场守卫）都不生效。注册动作 = seal_skill(attribute, count=1) 即时插票，被
//     本次属性技能的 query_usage 消费。
//   - ②必暴同理：标记在，下回合自己命中前桶读 → ws.must_crit=1（回合内整回合有效，v1）。
//   - ③免死属**第二类（范围即时检测）**：引擎的死亡漏斗本来就是即时检测——走
//     DeathInterceptor，sees_hp_consume=false（残留体力档：消耗体力穿它，idx=149 晨曦套
//     穿螳螂实证）。首次限定（soulmark_storage 次数位）；拦下后 hp=1 + 记解控回合 +
//     下3次攻击增伤（3 层存 **on_stage_storage**——切走即清，用户口径"切走了下次就没有了"）。
//   - ③解控是**回合结束档**（非实时，105 C' 组口径）：被带控制的技能先手击杀 → 当回合
//     行动期控制还在身上，不能出手；BATTLE_ROUND_END 才解除。增伤 150%/次经
//     grant_attack_boost（次数型通用增伤通道，命中前桶注入、本次攻击消费）。
// 待做：boss 无效门（PVE 标记位）；必暴"整回合 vs 一次"的官方观察。
#include "common.h"

namespace {

constexpr int kMarkId = 1164;   // effect_icon.effect_id = 魂印注册键（现代链路）
constexpr double kMinRestraint = 2.0;
constexpr int kDamageThreshold = 280;
constexpr int kMirrorSealNodeId = kMarkId * 100 + 90;   // 对手桶镜像节点 id

// plugin_storage[+side] 键段 9901164xx（grep 无冲突，2026-09-24）。
constexpr int kArmGuardKey = 990116401;   // watcher/镜像接线守卫
constexpr int kSealRoundKey = 990116403;  // 封属生效回合（=出手回合+1）
constexpr int kSealSlotKey = 990116405;   // 封属目标槽（对手切走即失配）
constexpr int kCritRoundKey = 990116407;  // 必暴回合
constexpr int kCureRoundKey = 990116409;  // 圣光祝福解控回合
// soulmark_storage（下场保留）：首次死次数位。
constexpr int kBlessUsedKey = 990116402;
// on_stage_storage（下场即清）：增伤剩余次数——"切走了下次就没有了"。
constexpr int kBoostChargeKey = 990116404;
// plugin_storage[+side]：攻击前对手体力快照——②"造成的伤害"读**终伤红伤实际落地值**
// （用户 2026-09-24 口径：被护盾挡住的部分不算、Miss/被盔=0 也算低于280）。
constexpr int kHpSnapKey = 990116411;

EffectResult encarlos_1164_restraint_lock(BattleContext* ctx, const EffectArgs& args);

// ── 当前出手流程的 mover（FIRST/SECOND 通算；非命中前时点返回 -1）──
int flow_mover(BattleContext* ctx) {
    if (!ctx) {
        return -1;
    }
    if (ctx->currentState == State::BATTLE_FIRST_SKILL_EFFECT
        || ctx->currentState == State::BATTLE_FIRST_BEFORE_SKILL_HIT
        || ctx->currentState == State::BATTLE_FIRST_ACTION_END) {
        return ctx->is_second_mover(0) ? 1 : 0;
    }
    if (ctx->currentState == State::BATTLE_SECOND_SKILL_EFFECT
        || ctx->currentState == State::BATTLE_SECOND_BEFORE_SKILL_HIT
        || ctx->currentState == State::BATTLE_SECOND_ACTION_END) {
        if (ctx->is_second_mover(0) == ctx->is_second_mover(1)) {
            return -1;
        }
        return ctx->is_second_mover(0) ? 0 : 1;
    }
    return -1;
}

bool used_attack_skill(BattleContext* ctx, int mover) {
    const int idx = ctx->roundChoice[mover][1];
    if (idx < 0 || idx >= static_cast<int>(ctx->getPet(mover).skills.size())) {
        return false;
    }
    const Skills& used = ctx->getPet(mover).skills[static_cast<std::size_t>(idx)];
    return used.type != SkillType::Attribute;
}

// ── ① 克制锁（FIRST/SECOND_SKILL_EFFECT，STAGE）──────────────────
EffectResult encarlos_1164_restraint_lock(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    const int mover = flow_mover(ctx);
    if (mover < 0 || mover != owner) {
        return EffectResult::kOk;
    }
    if (!used_attack_skill(ctx, mover)) {
        return EffectResult::kOk;
    }
    if (!ctx->ws.skill_resolution_flags[mover].allowAttackDamagePipeline) {
        return EffectResult::kOk;   // miss/被盔：没有伤害公式
    }
    // 自然倍数按"技能系别视图 vs 防御方元素视图"现算（与公式同源，SKILL_EFFECT 已物化）。
    const double natural = g_core && g_core->restraint_multiplier
        ? g_core->restraint_multiplier(ctx->ws.skill_element_view[mover],
                                       ctx->ws.view_elementalAttributes[1 - mover])
        : 1.0;
    ctx->ws.restraint_view[mover] = natural < kMinRestraint ? kMinRestraint : natural;
    return EffectResult::kOk;
}

// ── ① 先制+1（MOVE_RIGHT，STAGE：所有技能无条件）─────────────────
EffectResult encarlos_1164_priority(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ctx->ws.preemptive_level[owner] += 1;
    return EffectResult::kOk;
}

// ── ① 免疫所有能力下降（ROUND_START 每回合刷新；ON_STAGE epoch 随离场失效。
//    不在登场 early 节点授——战斗初始化的 epoch 重锚会吃掉那时授的票，harness 桩同款教训）──
EffectResult encarlos_1164_stat_drop_immune(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ctx->grant_immunity(owner, ImmunityType::STAT_DROP, coverage_all(), /*anomaly_mask=*/0,
                        /*duration_rounds=*/99, /*source_id=*/kMarkId,
                        /*soul_immunity=*/false, EffectScope::ON_STAGE, /*counts=*/0,
                        /*source_effect_id=*/kMarkId);
    return EffectResult::kOk;
}

// ── ② 出手流程结束时（ACTION_END，STAGE）：280 严格阈值双分支 ──
EffectResult encarlos_1164_threshold_branch(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    const int mover = flow_mover(ctx);
    if (mover < 0 || mover != owner) {
        return EffectResult::kOk;
    }
    if (!used_attack_skill(ctx, mover)) {
        return EffectResult::kOk;
    }
    // "本次攻击造成的伤害" = **终伤红伤实际落地值**（用户 2026-09-24 口径）：攻击前
    // 快照 - 当前体力——被护盾挡住的部分不算；Miss/被盔 = 0，也算**低于280**（走吸取档）。
    const int foe = 1 - owner;
    int dealt = 0;
    auto snap = ctx->plugin_storage.find(kHpSnapKey + owner);
    if (snap != ctx->plugin_storage.end()) {
        dealt = any_int_or(snap->second) - ctx->getPet(foe).hp;
        if (dealt < 0) {
            dealt = 0;
        }
    }
    if (dealt > kDamageThreshold) {
        // >280：附加对手最大体力 1/3 的百分比伤害 + 记"下回合封属"标记。
        if (g_core && g_core->deal_pink_damage) {
            const int max_hp = resolve_pet_max_hp(ctx->getPet(foe));
            g_core->deal_pink_damage(ctx, foe, max_hp / 3, DamageKind::PERCENT_VALUE, owner);
        }
        ctx->plugin_storage[kSealRoundKey + owner] = ctx->roundCount + 1;
        ctx->plugin_storage[kSealSlotKey + owner] = ctx->on_stage[foe];
    } else if (dealt < kDamageThreshold) {
        // <280：吸取对手最大体力 1/3 + 记"下回合必暴"标记。恰好 ==280 两个分支都不触发。
        if (g_core && g_core->drain_hp) {
            g_core->drain_hp(ctx, owner, foe, 3);
        }
        ctx->plugin_storage[kCritRoundKey + owner] = ctx->roundCount + 1;
    }
    return EffectResult::kOk;
}

// ── ②下回合封属（镜像节点：对手命中前桶，TEAM + 双守卫）──────────
EffectResult encarlos_1164_seal_gate(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !g_core || !g_core->seal_skill) {
        return EffectResult::kOk;
    }
    int owner = -1;   // 英卡洛斯方
    int bucket_owner = -1;
    if (args.int_args && args.int_count >= 2) {
        owner = args.int_args[0];
        bucket_owner = args.int_args[1];
    }
    if (owner < 0 || owner > 1 || bucket_owner != (1 - owner)) {
        return EffectResult::kOk;
    }
    const int mover = flow_mover(ctx);
    if (mover != bucket_owner) {
        return EffectResult::kOk;
    }
    auto rd = ctx->plugin_storage.find(kSealRoundKey + owner);
    if (rd == ctx->plugin_storage.end()
        || any_int_or(rd->second) != ctx->roundCount) {
        return EffectResult::kOk;   // 不是"下回合"
    }
    auto sl = ctx->plugin_storage.find(kSealSlotKey + owner);
    if (sl == ctx->plugin_storage.end()
        || any_int_or(sl->second) != ctx->on_stage[mover]) {
        return EffectResult::kOk;   // 对手切走：登场者不受封属（"切走也不会生效"）
    }
    const int sealer = ctx->find_pet_with_soulmark(owner, kMarkId);
    if (sealer < 0 || ctx->on_stage[owner] != sealer) {
        return EffectResult::kOk;   // 英卡洛斯自己切走：同样不生效
    }
    // 只在 mover **确实选了技能**时判（CHOOSE_PET/嗑药行动 roundChoice[1] 是目标槽/
    // 药剂索引，不是技能槽——不设此门会把"切到槽1"误判成 1 号槽技能）。且对手既然没
    // 用属性技能，封属票也不该提前挂出（count=1 会滞留到之后任意一次属性技）。
    if (ctx->roundChoice[mover][0] != 1 /* SELECT_SKILL */) {
        return EffectResult::kOk;
    }
    const int idx = ctx->roundChoice[mover][1];
    if (idx < 0 || idx >= static_cast<int>(ctx->getPet(mover).skills.size())) {
        return EffectResult::kOk;
    }
    const Skills& used = ctx->getPet(mover).skills[static_cast<std::size_t>(idx)];
    if (used.type != SkillType::Attribute) {
        return EffectResult::kOk;   // 只封属性技能
    }
    // 即时插票：count=1，被本次属性技能的 query_usage 消费（"下回合使用的属性技能无效"）。
    g_core->seal_skill(ctx, /*source=*/owner, /*target=*/mover, /*effect_id=*/kMarkId,
                       /*attribute=*/true, /*attack=*/false, /*count=*/1,
                       /*duration_rounds=*/0, /*penetrable=*/true, /*source_slot=*/-1,
                       EffectScope::ON_STAGE, /*hit_invalid=*/false, /*chance_pct=*/100,
                       /*consumed_when_pierced=*/false);
    return EffectResult::kOk;
}

// ── ②下回合必暴（自己命中前桶，STAGE；切走后回合不再匹配）────────
EffectResult encarlos_1164_crit_gate(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    const int mover = flow_mover(ctx);
    if (mover < 0 || mover != owner) {
        return EffectResult::kOk;
    }
    auto rd = ctx->plugin_storage.find(kCritRoundKey + owner);
    if (rd == ctx->plugin_storage.end()
        || any_int_or(rd->second) != ctx->roundCount) {
        return EffectResult::kOk;
    }
    ctx->ws.must_crit[owner] = true;
    ctx->plugin_storage[kCritRoundKey + owner] = -1;
    return EffectResult::kOk;
}

// ── ③下3次攻击增伤（自己命中前桶，STAGE；次数存 on_stage_storage）──
EffectResult encarlos_1164_blessing_boost(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    const int mover = flow_mover(ctx);
    if (mover < 0 || mover != owner) {
        return EffectResult::kOk;
    }
    if (!used_attack_skill(ctx, mover)) {
        return EffectResult::kOk;   // "下3次攻击技能"
    }
    // ②的落地伤害快照（攻击前对手体力；miss/被盾后按差值=0/剩余计）。
    ctx->plugin_storage[kHpSnapKey + owner] = ctx->getPet(1 - owner).hp;
    ElfPet& self = ctx->seerRobot[owner].elfPets[ctx->on_stage[owner]];
    auto it = self.on_stage_storage.find(kBoostChargeKey);
    if (it == self.on_stage_storage.end()) {
        return EffectResult::kOk;
    }
    const int left = any_int_or(it->second);
    if (left <= 0) {
        return EffectResult::kOk;
    }
    ctx->grant_attack_boost(owner, kMarkId, /*count=*/1, /*pct=*/150);
    self.on_stage_storage[kBoostChargeKey] = left - 1;
    return EffectResult::kOk;
}

// ── ③回合结束解控（DEATH_TIMING，回合结束段、死亡漏斗之后；被击杀当回合
//    行动期控制还在身上 → 当回合不能出手 = 低级档，105 C' 组口径）。
//    ⚠️ 不挂 ROUND_END：第二手死亡（BATTLE_SECOND_MOVER_DEATH）的漏斗在 ROUND_END
//    **之后**才走（state 序：ROUND_END → SECOND_MOVER_DEATH → … → DEATH_TIMING），
//    挂 ROUND END 会在拦截器落标记之前跑空。DEATH_TIMING 对先/后手死亡都在漏斗后。──
EffectResult encarlos_1164_round_end_cure(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !g_core || !g_core->dispel_active_anomalies) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    auto rd = ctx->plugin_storage.find(kCureRoundKey + owner);
    if (rd == ctx->plugin_storage.end()
        || any_int_or(rd->second) != ctx->roundCount) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage[kCureRoundKey + owner] = -1;
    g_core->dispel_active_anomalies(ctx, owner);
    return EffectResult::kOk;
}

// ── 开局接线（ENTER_EXIT_STAGE，ROSTER）：拦截器 + 封属镜像 ───────
EffectResult encarlos_1164_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int guard = kArmGuardKey + owner;
    if (ctx->plugin_storage.count(guard)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage[guard] = true;

    // ③ 首次死亡圣光祝福：第二类范围即时检测（死亡漏斗即时询问），消耗体力穿它。
    if (g_core && g_core->register_death_interceptor) {
        DeathInterceptor ic;
        ic.source_effect_id = kMarkId;
        ic.owner = owner;
        ic.sees_hp_consume = false;   // 残留体力档：消耗体力类死亡穿它（idx=149 实证）
        ic.fn = [owner](BattleContext* c, int side, int slot, int, DefeatCause) -> bool {
            if (!c || side != owner) {
                return false;
            }
            // 只救英卡洛斯本人（此刻 hp 已 <=0，不能用 hp>0 过滤的查找）。
            if (c->seerRobot[side].elfPets[slot].soulMark.id != kMarkId) {
                return false;
            }
            auto& storage = c->seerRobot[side].elfPets[slot].soulmark_storage;
            auto used = storage.find(kBlessUsedKey);
            if (used != storage.end() && any_int_or(used->second) == 1) {
                return false;   // 仅首次死亡
            }
            storage[kBlessUsedKey] = 1;
            // 当回合残留 1 点体力（拦下契约：写回 >0）。
            c->seerRobot[side].elfPets[slot].hp = 1;
            // 战斗阶段结束时解除自身异常（回合结束档——当回合解不了控制）。
            c->plugin_storage[kCureRoundKey + owner] = c->roundCount;
            // 下3次攻击 +150%：次数存 on_stage_storage（切走即清）。
            c->seerRobot[side].elfPets[slot].on_stage_storage[kBoostChargeKey] = 3;
            return true;
        };
        g_core->register_death_interceptor(ctx, ic);
    }

    // ② 封属镜像节点：注册进对手命中前桶（TEAM + 载荷守卫；lingchao 镜像同款）。
    {
        const int foe = 1 - owner;
        auto make_mirror = [&](State st) {
            Effect wrapper(kMirrorSealNodeId, 0, foe, /*left_round=*/-1,
                           EffectArgs(std::vector<int>{owner, foe}),
                           &encarlos_1164_seal_gate);
            auto ce = std::make_unique<ContinuousEffect>(
                wrapper, st, foe, /*duration=*/-1, ctx->roundCount);
            ce->source_id_ = kMarkId;
            ce->scope_ = EffectScope::TEAM;
            ce->once_ = false;
            ctx->register_soulmark_effect(st, foe, std::move(ce));
        };
        make_mirror(State::BATTLE_FIRST_BEFORE_SKILL_HIT);
        make_mirror(State::BATTLE_SECOND_BEFORE_SKILL_HIT);
    }
    return EffectResult::kOk;
}

}  // namespace

SOUL_PROGRAM(kMarkId,
             {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &encarlos_1164_arm,
                              /*once=*/false, /*early=*/false, SoulScope::ROSTER),
              SoulMarkNodeRef(State::BATTLE_ROUND_START, &encarlos_1164_stat_drop_immune,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_FIRST_MOVE_RIGHT, &encarlos_1164_priority,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_FIRST_SKILL_EFFECT, &encarlos_1164_restraint_lock,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_SECOND_SKILL_EFFECT, &encarlos_1164_restraint_lock,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_FIRST_ACTION_END, &encarlos_1164_threshold_branch,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_SECOND_ACTION_END, &encarlos_1164_threshold_branch,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_FIRST_BEFORE_SKILL_HIT, &encarlos_1164_crit_gate,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_SECOND_BEFORE_SKILL_HIT, &encarlos_1164_crit_gate,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_FIRST_BEFORE_SKILL_HIT, &encarlos_1164_blessing_boost,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_SECOND_BEFORE_SKILL_HIT, &encarlos_1164_blessing_boost,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_DEATH_TIMING, &encarlos_1164_round_end_cure,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE)});
