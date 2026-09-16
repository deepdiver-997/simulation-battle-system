#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <numerical-calculation/calculation.h>
#include <iostream>
#include <sstream>

namespace {

constexpr std::array<State, 40> kLinearStateOrder = {
    State::GAME_START,
    State::OPERATION_ENTER_EXIT_STAGE,
    State::OPERATION_CHOOSE_SKILL_MEDICAMENT,
    State::OPERATION_PROTECTION_MECHANISM_1,
    State::OPERATION_ENTER_STAGE,
    State::BATTLE_ROUND_START,
    State::BATTLE_FIRST_MOVE_RIGHT,
    State::BATTLE_FIRST_ACTION_START,
    State::BATTLE_FIRST_BEFORE_SKILL_HIT,
    State::BATTLE_FIRST_ON_SKILL_HIT,
    State::BATTLE_FIRST_SKILL_EFFECT,
    State::BATTLE_FIRST_ATTACK_DAMAGE,
    State::BATTLE_FIRST_AFTER_ACTION,
    State::BATTLE_FIRST_ACTION_END,
    State::BATTLE_FIRST_AFTER_ACTION_END,
    State::BATTLE_FIRST_EXTRA_ACTION,
    State::BATTLE_FIRST_MOVER_DEATH,
    State::BATTLE_SECOND_ACTION_START,
    State::BATTLE_SECOND_BEFORE_SKILL_HIT,
    State::BATTLE_SECOND_ON_SKILL_HIT,
    State::BATTLE_SECOND_SKILL_EFFECT,
    State::BATTLE_SECOND_ATTACK_DAMAGE,
    State::BATTLE_SECOND_AFTER_ACTION,
    State::BATTLE_SECOND_ACTION_END,
    State::BATTLE_SECOND_AFTER_ACTION_END,
    State::BATTLE_SECOND_EXTRA_ACTION,
    State::BATTLE_ROUND_END,
    State::BATTLE_SECOND_MOVER_DEATH,
    State::BATTLE_OLD_ROUND_END_1,
    State::BATTLE_ROUND_REDUCTION_ALL_ROUND_MINUS,
    State::BATTLE_ROUND_REDUCTION_NEW_ROUND_END,
    State::BATTLE_OLD_ROUND_END_2,
    State::BATTLE_DEATH_TIMING,
    State::BATTLE_DEFEAT_STATUS,
    State::BATTLE_OPPONENT_DEFEAT_STATUS,
    State::BATTLE_NEW_DEFEAT_MECHANISM,
    State::OPERATION_PROTECTION_MECHANISM_2,
    State::BATTLE_AFTER_DEFEATED,
    State::CHOOSE_AFTER_DEATH,
    State::BATTLE_AFTER_DEFEATING_OPPONENT,
};

std::size_t find_state_index(State state) {
    const auto it = std::find(kLinearStateOrder.begin(), kLinearStateOrder.end(), state);
    if (it == kLinearStateOrder.end()) {
        return kLinearStateOrder.size();
    }
    return static_cast<std::size_t>(std::distance(kLinearStateOrder.begin(), it));
}

TimedBucket& bucket_for(BattleContext* ctx, EffectContainer container) {
    return (container == EffectContainer::SoulMark) ? ctx->soul_mark_effects : ctx->skills_effects;
}

const TimedBucket& bucket_for(const BattleContext* ctx, EffectContainer container) {
    return (container == EffectContainer::SoulMark) ? ctx->soul_mark_effects : ctx->skills_effects;
}

// 时点桶的执行逻辑已移入 TimedBucket::execute_at（src/effects/timed_bucket.cpp）。

} // namespace

BattleContext::BattleContext(IControlBlock* control_block, const SeerRobot robots[])
    : m_fsm(nullptr)
    , seerRobot{robots[0], robots[1]}
    , roundCount(0)
    , uuid(std::chrono::system_clock::now().time_since_epoch().count())
    , currentState(State::GAME_START)
    , failed_attempts(0)
    , control_block_(control_block)
    , current_player_id_(0)
    , m_buffer(1024)
    , roundChoice(ws.roundChoice)
    , lastActionType(ws.lastActionType)
    , lastActionIndex(ws.lastActionIndex)
    , preemptive_right(ws.preemptive_right)
    , damage_reduce_flat(ws.damage_reduce_flat)
    , damage_reduce_add(ws.damage_reduce_add)
    , damage_reduce_mul(ws.damage_reduce_mul)
    , damage_add_extra_mul(ws.damage_add_extra_mul)
    , pendingDamage(ws.pendingDamage)
    , resolvedDamage(ws.resolvedDamage)
    , resolvedPink(ws.resolvedPink)
{
    init_battle();
}

BattleContext::~BattleContext() {
}

void BattleContext::init_battle() {
    // 初始化逻辑
    on_stage[0] = 0;
    on_stage[1] = 0;
    // 登场特性槽：首发两只的特性从 pet 数据拷入 context（查询/行为函数读槽）
    sync_on_stage_trait(0);
    sync_on_stage_trait(1);
    elf_element_view_bound_slot[0] = -1;  // 首回合 sync 时从 pet.elementalAttributes 基线系别
    elf_element_view_bound_slot[1] = -1;
    clear_all_on_stage_abnormal_statuses();
    reset_operation_collection();
    roundChoice[0][0] = -1;
    roundChoice[0][1] = -1;
    roundChoice[1][0] = -1;
    roundChoice[1][1] = -1;
    install_default_damage_reduction();
    install_default_damage_block();
    install_default_damage_amp();
    install_default_damage_amp_extra();
    install_default_damage_guard_detect();
    pink_damage_pipeline_.clear();
    install_default_pink_mitigation();
    install_common_trait_effects(this);
}

void BattleContext::install_default_pink_mitigation() {
    for (int owner = 0; owner < 2; ++owner) {
        // 粉伤免减只从"承受方"桶读。run 每阶段按 {actor, target} 各走一趟，
        // 因此回调里用 resolvedPink.target 判断当前桶 owner 是否为承受方。
        //
        // ① RESIST：**抗性免减粉伤** —— 链条第一位（流程图最左）。
        // ⚠️ 取整：官方算法是 `伤害量 − 伤害量×抗性`（乘法**向下**取整，L84），
        //    即 `final -= final * resist / 100`——写成"乘 (100-resist)/100"会因两次取整对不上。
        register_default_pink_effect(
            PinkDamagePhase::RESIST,
            owner,
            [](BattleContext* ctx, int bucket_owner) {
                if (!ctx) {
                    return;
                }
                PinkDamageResolved& r = ctx->resolvedPink;
                if (r.target < 0 || r.target > 1 || bucket_owner != r.target) {
                    return;
                }
                if (r.final <= 0) {
                    return;
                }
                // 抗性按来源分型：FIXED 走固定抗性、百分比走百分比抗性（官方：暴击/固定/百分比）。
                // ⚠️ 读的是 ws **有效视图**而非 pet 本体——临时 buff 可修改抗性，
                //    视图基线由 sync_damage_resist_view 在回合开始/换宠重基。
                const int resist_pct = (r.tier == PinkDamageTier::PERCENT)
                    ? ctx->ws.eff_percent_resist_pct[r.target]
                    : ctx->ws.eff_fixed_resist_pct[r.target];
                r.final -= r.final * resist_pct / 100;
                if (r.final < 0) {
                    r.final = 0;
                }
            }
        );
        // ② IMMUNE：**免疫粉伤 / 免疫并反弹粉伤** —— 排在抗性**之后**（流程图第二个框）。
        //    ★ **免粉与"挡伤"走同一条路**（用户 2026-09-15 口径）：同一个 RuleCenter 免疫票据
        //      （`ImmunityType::PINK_DAMAGE`）、同一个 `is_immune` / `consume_immune` 消费点，
        //      与红伤 BLOCK 阶段的"次数型免伤"完全同构——不再另设 `pink_immune` 裸 bool。
        //    ★ **次数型免粉只挡一段**：多段粉 = N 次独立结算 → 只消费掉票据的 1 次，
        //      其余各段照常落到本体。窗口型/永久型（counts=0）则每段都查、每段都免。
        //    ⚠️ 免疫与"抗性 100% / 效果减粉"在下游检测里**不做区分**（都只是把 final 削到 0）。
        //    ⚠️ 「免疫并**反弹**粉伤」**未做**——反弹量应取抗性后的值（即此刻的 final）。
        register_default_pink_effect(
            PinkDamagePhase::IMMUNE,
            owner,
            [](BattleContext* ctx, int bucket_owner) {
                if (!ctx) {
                    return;
                }
                PinkDamageResolved& r = ctx->resolvedPink;
                if (r.target < 0 || r.target > 1 || bucket_owner != r.target) {
                    return;
                }
                if (r.final <= 0) {
                    return;
                }
                if (!ctx->is_immune(r.target, ImmunityType::PINK_DAMAGE, ctx->currentState)) {
                    return;
                }
                ctx->consume_immune(r.target, ImmunityType::PINK_DAMAGE, ctx->currentState);
                r.final = 0;
            }
        );
        // ③ REDUCE_EXTRA：**百分比免减粉伤**（技能特效 + 魂印特效共用本阶段，乘法连乘）。
        //    排在抗性之后——L463 的乘算链 `×(1−抗性免减)×(1−技能特效免减)×(1−魂印特效免减)`。
        register_default_pink_effect(
            PinkDamagePhase::REDUCE_EXTRA,
            owner,
            [](BattleContext* ctx, int bucket_owner) {
                if (!ctx) {
                    return;
                }
                PinkDamageResolved& r = ctx->resolvedPink;
                if (r.target < 0 || r.target > 1 || bucket_owner != r.target) {
                    return;
                }
                if (r.final <= 0) {
                    return;
                }
                r.final -= r.final * ctx->pink_reduce_pct[r.target] / 100;
                if (r.final < 0) {
                    r.final = 0;
                }
            }
        );
        // 从这里开始接下来应该是：粉伤提升n% -> 粉转护罩检测点（目前没有这个效果要实现但是最好留着这个阶段） -> 粉转真（未受到粉伤）检测点 -> 消费护罩 -> 粉转真（免疫粉伤）检测点 -> 最终粉伤
        // 然后在抗性过后，后面的区间一直到最终粉伤都是星皇之怒二段无视的区间，也就是只用经过抗性减免就可以直接得到最终粉伤
        // ⑤ PINK_TO_HOOD：**粉转护罩 检测点**（挂在"粉伤提升 n%"之后、护罩之前）。
        //    ⚠️ **暂无生产者**——阶段先留着（用户 2026-09-15：目前没有这个效果要实现，但阶段要占位）。
        //       要接的时候：把本段转成护罩（`to_hood`）+ 终止后续（护罩不再吃这段）。
        //
        // ⑥ PINK_TO_TRUE_IMMUNE：**粉转真（免疫粉伤）检测点** —— 挂在**护罩之前**。
        //    ⚠️ **引擎不注册默认回调**（用户 2026-09-15 口径）：粉伤是**即时结算**的，
        //       谁的效果谁在 `deal_pink_damage` 返回后直接读 `ctx->resolvedPink` 就能判
        //       （`final <= 0 && absorbed == 0` = 被免疫/免减挡下），或往本阶段注册一条
        //       带自己条件的条目。曾经这里有一个 `ctx->pink_to_true[2]` 全局 bool——
        //       那会把一个效果附带的粉转真串给下一个无关的粉伤效果，已删。
        //    ⚠️ 因为检测点在护罩之前，这里转出的真伤**不消耗护罩**（真伤直通护盾/护罩）。
        //
        // ⑦ CAP：上限/锁伤（沧岚 2343「不超过此护盾的数值」）——**非流程图节点**，
        //    是本引擎为"受到的粉伤不超过X"一族留的槽；位置（护罩之前）待实测确认。
        //
        // ⑧ HOOD：**护罩免减粉伤**——**消费护罩**（流程图：算完抗性/免减/增粉/转护罩才扣护罩）。
        //    记 absorbed 供"护罩算不算受到伤害"判定；破罩发 EVENT_SHIELD_BROKEN。
        //    ⚠️ **扣体力不在这里**：管线只把值定形，扣血由 deal_damage 在管线之后统一做。
        register_default_pink_effect(
            PinkDamagePhase::HOOD,
            owner,
            [](BattleContext* ctx, int bucket_owner) {
                if (!ctx) {
                    return;
                }
                PinkDamageResolved& r = ctx->resolvedPink;
                if (r.target < 0 || r.target > 1 || bucket_owner != r.target) {
                    return;
                }
                if (r.final <= 0) {
                    return;
                }
                // 攻击方可设 ws.ignore_shield[actor] 使本次伤害无视护盾/护罩响应（如无极圣武魂印）。
                const int actor = r.actor;
                if (actor >= 0 && actor <= 1 && ctx->ws.ignore_shield[actor]) {
                    return;
                }
                int broken = 0;
                const int remaining = ctx->getPet(r.target).hood_bank_.absorb(r.final, &broken);
                r.absorbed += r.final - remaining;
                r.final = remaining;
                if (broken > 0) {
                    ctx->event_center_.emit(
                        BattleEvent{EventType::EVENT_SHIELD_BROKEN, actor, r.target});
                }
            }
        );
        // ⑨ PINK_TO_TRUE_UNHARMED：**粉转真（未受到粉伤）检测点** —— 挂在**护罩之后**。
        //    ★ 位置不能提前：要判"有没有受到粉伤"，必须等护罩消费完（护罩吃光 → 没落到本体）。
        //    ★ 这是个**给效果挂**的检测点（免粉补偿一族，L338 三类）：
        //      · 只看体力有没有降（类1）→ 本段此刻 `final <= 0` 即"没受到"，效果自行决定补什么；
        //      · 连护罩一起看（类2）→ 读 `r.absorbed`；
        //      · "体力变化 < n 点则触发"（类3）→ 读 `r.final`。
        //    ⚠️ 引擎自身**不在这里做任何事**——这里只是给效果留的挂钩；"免疫粉伤型"（见 ⑥）
        //       与"未受到粉伤型"是两条不同的判据，别混成一个开关。
        //    ⚠️ 此刻**体力还没扣**（扣血在管线之后）——要按"实际掉血"判的效果请挂
        //       `EVENT_TAKE_PINK_DAMAGE`（它只在真的扣到本体时才发）。
        //
        // ⑩ FINAL：**本次最终受到的粉伤**（终点读点）。
    }
}

void BattleContext::install_default_damage_reduction() {
    for (int owner = 0; owner < 2; ++owner) {
        // 减伤只从"防御方"的槽位读取。管线会先后走攻击方/防御方两个桶，
        // 因此回调里用 resolvedDamage.defenderId 判断当前桶 owner 是否为防御方，是才施加。
        // MITIGATE 类别 → 可被 damage_suppress_mask 抑制（如沧岚"挡伤失效"）。
        //
        // ① REDUCE_FLAT：**点数减伤**——官方减伤区顺序的第一位（L402「点数减伤——百分比减伤」）。
        //    先在裸伤上扣点数，再算百分比，与"反序"结果不同（(base-30)×0.5 vs base×0.5-30）。
        register_default_damage_effect(
            DamagePhase::REDUCE_FLAT,
            owner,
            DamageEffectCategory::MITIGATE,
            [](BattleContext* ctx, int bucket_owner) {
                if (!ctx) {
                    return;
                }
                const int defender = ctx->resolvedDamage.defenderId;
                if (defender < 0 || defender > 1 || bucket_owner != defender) {
                    return;
                }
                int flat_sum = 0;
                for (int i = 0; i < 4; ++i) {
                    flat_sum += ctx->damage_reduce_flat[defender][i];
                }
                if (flat_sum == 0) {
                    return;
                }
                ctx->resolvedDamage.final = std::max(0, ctx->resolvedDamage.final - flat_sum);
            }
        );
        // ② REDUCE_PCT：**百分比减伤**——加算槽求和（钳 ±100，官方"通用减伤叠加超 100% 即失效"）
        //    + 乘算槽逐条连乘。实现在 Calculation::applyDamageReduction。
        register_default_damage_effect(
            DamagePhase::REDUCE_PCT,
            owner,
            DamageEffectCategory::MITIGATE,
            [](BattleContext* ctx, int bucket_owner) {
                if (!ctx) {
                    return;
                }
                const int defender = ctx->resolvedDamage.defenderId;
                if (defender < 0 || defender > 1 || bucket_owner != defender) {
                    return;
                }
                ctx->resolvedDamage.final = Calculation::applyDamageReduction(
                    ctx->resolvedDamage.final,
                    ctx->damage_reduce_add[defender],
                    ctx->damage_reduce_mul[defender]
                );
            }
        );
    }
}

// 链首受击快照（GUARD_DETECT 阶段）——把"增伤前"的伤害值记进 ws，供「受高伤/受低伤」一族魂印读。
//
// 为什么需要它（而不是让插件自己在攻击时点的桶里读 `resolvedDamage`）：
//   攻击伤害时点（`BATTLE_*_ATTACK_DAMAGE`）的桶**只跑当回合 mover 那一侧** ——
//   犀牛作为防守方、自己不是 mover 时根本不执行（谱尼虚无的注释早就点过这个坑）。
//   而伤害管线是双方都走的（每阶段先攻击方桶、再防守方桶），所以 GUARD_DETECT 挂在管线上
//   对"防守方"天然生效，且位置就在**增伤之前**（链首）。
//
// 为什么必须是"增伤前"：官方时点链是「犀牛魂印—通用增伤—693增伤—保底伤害—护盾」。
//   693 增伤（AMP_EXTRA）落在犀牛检测**之后** → 它把实际伤害顶过 350 时犀牛看不到
//   —— 这就是「693 增伤乱穿犀牛」（检测早于增伤）。
// DETECT 类别 → 吃 `damage_suppress_mask`（"挡伤失效"也该废掉受高伤检测）。
void BattleContext::install_default_damage_guard_detect() {
    for (int owner = 0; owner < 2; ++owner) {
        register_default_damage_effect(
            DamagePhase::GUARD_DETECT,
            owner,
            DamageEffectCategory::DETECT,
            [](BattleContext* ctx, int bucket_owner) {
                if (!ctx) {
                    return;
                }
                const DamageSnapshot& damage = ctx->resolvedDamage;
                const int defender = damage.defenderId;
                if (defender < 0 || defender > 1 || bucket_owner != defender) {
                    return;   // 只记"我是本次攻击的防守方"的那一趟
                }
                const bool red = damage.isRed && damage.final > 0;
                ctx->ws.guard_detect_damage[defender] = red ? damage.final : 0;
                ctx->ws.guard_detect_happened[defender] = red;
            }
        );
    }
}

// 非通用增伤（乘法通道）——官方 L352：「通用增伤……**所有的通用增伤加法计算，
// 而非通用增伤全部乘法计算**」。措辞判据是"**额外**提升X%"（如 693 圣光吟诵）。
// 单独一个阶段（AMP_EXTRA）且排在通用增伤（AMP）之后——这正是"693 增伤乱穿犀牛"的结构：
// 犀牛的受高伤检测在链首（GUARD_DETECT），之后才被 693 抬起来的伤害它看不到。
// AMP 类别 → **不被** damage_suppress_mask 抑制（增伤不是"挡伤"）。
void BattleContext::install_default_damage_amp_extra() {
    for (int owner = 0; owner < 2; ++owner) {
        register_default_damage_effect(
            DamagePhase::AMP_EXTRA,
            owner,
            DamageEffectCategory::AMP,
            [](BattleContext* ctx, int bucket_owner) {
                if (!ctx) {
                    return;
                }
                DamageSnapshot& damage = ctx->resolvedDamage;
                if (damage.attackerId < 0 || damage.attackerId > 1
                    || bucket_owner != damage.attackerId) {
                    return;
                }
                if (damage.final <= 0) {
                    return;
                }
                for (int i = 0; i < 4; ++i) {
                    const int v = ctx->damage_add_extra_mul[bucket_owner][i];
                    if (v == 0) {
                        continue;
                    }
                    damage.final = damage.final * (100 + v) / 100;
                }
                if (damage.final < 0) {
                    damage.final = 0;
                }
            }
        );
    }
}

void BattleContext::install_default_damage_block() {
    for (int owner = 0; owner < 2; ++owner) {
        // 挡伤归零只对"防御方"生效。管线在 BLOCK 阶段先后走攻击方/防御方两个桶，
        // 因此回调里用 resolvedDamage.defenderId 判断当前桶 owner 是否为防御方。
        // BLOCK 类别 → 可被 damage_suppress_mask 抑制（蚀砚之泪≥4滴"挡伤失效"）。
        register_default_damage_effect(
            DamagePhase::BLOCK,
            owner,
            DamageEffectCategory::BLOCK,
            [](BattleContext* ctx, int bucket_owner) {
                if (!ctx) {
                    return;
                }
                DamageSnapshot& damage = ctx->resolvedDamage;
                const int defender = damage.defenderId;
                if (defender < 0 || defender > 1 || bucket_owner != defender) {
                    return;
                }
                if (!damage.isRed || damage.final <= 0) {
                    return;  // 只挡红伤（技能攻击伤害），已被挡下的不重复处理
                }
                // 次数型免伤（"免疫下N次攻击伤害"）：纯查询命中才消费——非次数型（窗口/永久）
                // 命中也不扣，故两个调用都要走。
                if (!ctx->is_immune(defender, ImmunityType::DAMAGE, ctx->currentState)) {
                    return;
                }
                ctx->consume_immune(defender, ImmunityType::DAMAGE, ctx->currentState);
                // 完全挡下：整段快照归零（不只 final——下游若读 base 不应看到幻影伤害）。
                damage.base = 0;
                damage.afterAdd = 0;
                damage.afterMul = 0;
                damage.final = 0;
                damage.addPct = 0;
                damage.mulCoef = 0.0;
            }
        );
    }
}

void BattleContext::install_default_damage_amp() {
    for (int owner = 0; owner < 2; ++owner) {
        // 增伤只从"攻击方"的槽位读取。管线在 AMP 阶段会先后走攻击方/防御方两个桶，
        // 因此回调里用 resolvedDamage.attackerId 判断当前桶 owner 是否为攻击方。
        // AMP 类别 → **不被** damage_suppress_mask 抑制（官方口径：增伤/减伤都不是"挡伤"，
        // "使对手挡伤失效"只废归零类，见 DamageEffectCategory）。
        register_default_damage_effect(
            DamagePhase::AMP,
            owner,
            DamageEffectCategory::AMP,
            [](BattleContext* ctx, int bucket_owner) {
                if (!ctx) {
                    return;
                }
                DamageSnapshot& damage = ctx->resolvedDamage;
                if (damage.attackerId < 0 || damage.attackerId > 1
                    || bucket_owner != damage.attackerId) {
                    return;
                }
                if (damage.final <= 0) {
                    return;
                }
                const int pct = ctx->ws.damage_add_pct[bucket_owner];
                const int flat = ctx->ws.damage_add_flat[bucket_owner];
                if (pct == 0 && flat == 0) {
                    return;
                }
                damage.addPct += pct;   // 记进快照，供调试/下游读取
                damage.final = damage.final * (100 + pct) / 100 + flat;
                if (damage.final < 0) {
                    damage.final = 0;
                }
            }
        );
    }
}

bool BattleContext::need_input() const {
    if (!is_empty) return false;
    switch (currentState) {
        case State::CHOOSE_AFTER_DEATH:
        if (seerRobot[0].elfPets[on_stage[0]].hp == 0
            || seerRobot[1].elfPets[on_stage[1]].hp == 0
        ) return true;
        else return false;
        case State::OPERATION_CHOOSE_SKILL_MEDICAMENT:
            return true;
        default:
            return false;
    }
}

void BattleContext::generateState() {
    this->failed_attempts = 0;
    if (currentState == State::FINISHED) {
        return;
    }

    const std::size_t index = find_state_index(currentState);
    if (index >= kLinearStateOrder.size()) {
        currentState = State::FINISHED;
        return;
    }

    if (index + 1 >= kLinearStateOrder.size()) {
        currentState = State::BATTLE_ROUND_COMPLETION;
        return;
    }

    currentState = kLinearStateOrder[index + 1];
}

void BattleContext::back_to_last_state() {
    ++failed_attempts;
    if (MAX_ATTEMPTS && failed_attempts > MAX_ATTEMPTS) {
        currentState = State::FINISHED;
        return;
    }

    if (currentState == State::FINISHED) {
        currentState = State::BATTLE_ROUND_COMPLETION;
        return;
    }

    const std::size_t index = find_state_index(currentState);
    if (index == 0 || index >= kLinearStateOrder.size()) {
        currentState = State::GAME_START;
        return;
    }

    currentState = kLinearStateOrder[index - 1];
}

void BattleContext::registerEffect(State trigger, int owner, std::unique_ptr<ContinuousEffect> effect,
                                   EffectContainer container) {
    // valid_id 绑定 / 同源去重 / 回合计数回滚都在 TimedBucket::register_effect 内完成。
    bucket_for(this, container).register_effect(trigger, owner, std::move(effect),
                                                round_effect_valid_id[owner]);
}

void BattleContext::clear_on_stage_abnormal_statuses(int robotId) {
    if (robotId < 0 || robotId > 1) {
        return;
    }
    abnormal_status_end_round[robotId].fill(0);
}

void BattleContext::clear_all_on_stage_abnormal_statuses() {
    clear_on_stage_abnormal_statuses(0);
    clear_on_stage_abnormal_statuses(1);
}

void BattleContext::cleanup_expired_effects() {
    // 清理每个时点桶中：
    //   1. 自然过期的效果（isExpired）
    //   2. 被断回合/切换作废的效果（valid_id 不匹配）
    // 同时扣减各自的回合计数。判定谓词只在 TimedBucket::cleanup 内写一次。
    skills_effects.cleanup(roundCount, round_effect_valid_id);
    soul_mark_effects.cleanup(roundCount, round_effect_valid_id);

    // 清理过期的断回合补偿 watcher（事件中心统一管理生命周期）
    // 传 watcher_valid_id 供 cleanup 检查 ON_STAGE 监听器是否被切换作废
    event_center_.cleanup(roundCount, watcher_valid_id);

    // 清理窗口已过的免疫 Provider
    rule_center_.cleanup(roundCount);
}

// invalidate_all_round_effects 已内联至头文件（插件需可见）。

int BattleContext::register_break_callback(int owner, int duration_rounds,
                                           std::function<void(BattleContext*)> fn) {
    if (owner < 0 || owner > 1) return -1;
    // ON_STAGE 监听器：绑定当前 watcher_valid_id[owner]，切换精灵时作废（补偿不继承给新精灵）
    return event_center_.register_watcher(
        EventType::EVENT_BREAK,
        owner,
        roundCount,        // register_round
        duration_rounds,   // 0 = 永久
        /*once=*/true,     // 被断补偿只触发一次，触发后自动移除
        [fn = std::move(fn), owner](BattleContext* ctx, const BattleEvent& event) {
            if (event.target != owner) {
                return;  // 只有自己被断才触发
            }
            fn(ctx);
        },
        WatcherScope::ON_STAGE,
        watcher_valid_id[owner]);
}

void BattleContext::remove_break_callback(int owner, int callback_id) {
    (void)owner;  // 兼容层：watcher 自含 owner，注销只需 id
    event_center_.remove_watcher(callback_id);
}


void BattleContext::set_abnormal_status_end_round(int robotId, int statusId, int endRound) {
    if (robotId < 0 || robotId > 1 || !is_valid_abnormal_status_id(statusId)) {
        return;
    }
    abnormal_status_end_round[robotId][statusId] = endRound;
}

void BattleContext::apply_abnormal_status_for_rounds(int robotId, int statusId, int durationRounds) {
    if (durationRounds <= 0) {
        return;
    }
    set_abnormal_status_end_round(robotId, statusId, roundCount + durationRounds);
}

int BattleContext::get_abnormal_status_end_round(int robotId, int statusId) const {
    if (robotId < 0 || robotId > 1 || !is_valid_abnormal_status_id(statusId)) {
        return 0;
    }
    return abnormal_status_end_round[robotId][statusId];
}

bool BattleContext::has_active_abnormal_status(int robotId, int statusId) const {
    if (robotId < 0 || robotId > 1 || !is_valid_abnormal_status_id(statusId)) {
        return false;
    }
    return roundCount < abnormal_status_end_round[robotId][statusId];
}

void BattleContext::execute_registered_actions(int robotId, State state) {
    if (robotId == -1) {
        execute_registered_actions(0, state);
        execute_registered_actions(1, state);
        return;
    }

    // 魂印容器优先于技能容器执行。
    soul_mark_effects.execute_at(state, robotId, this);
    skills_effects.execute_at(state, robotId, this);
}

template<int EffectId>
bool BattleContext::hasEffect(State trigger, int owner) const {
    const auto has_in_bucket = [&](EffectContainer container) {
        for (const auto& [key, effect] : bucket_for(this, container).at(trigger, owner)) {
            (void)key;
            if (effect->getEffectId() == EffectId && !effect->isExpired(roundCount)) {
                return true;
            }
        }
        return false;
    };

    if (has_in_bucket(EffectContainer::SoulMark)) {
        return true;
    }
    if (has_in_bucket(EffectContainer::Skill)) {
        return true;
    }
    return false;
}

template<int EffectId>
bool BattleContext::consumeEffect(State trigger, int opponent) {
    // 默认仅消费技能容器，避免技能侧误删魂印侧效果。
    auto& oppEffects = bucket_for(this, EffectContainer::Skill).at(trigger, opponent);
    for (auto& [key, effect] : oppEffects) {
        (void)key;
        if (effect->getEffectId() == EffectId && !effect->isExpired(roundCount)) {
            bool consumed = effect->consume(this);
            // 清理所有已过期条目（map 惰性删除）
            for (auto mit = oppEffects.begin(); mit != oppEffects.end();) {
                if (mit->second->isExpired(roundCount)) {
                    mit = oppEffects.erase(mit);
                } else {
                    ++mit;
                }
            }
            return consumed;
        }
    }
    return false;
}

// 显式实例化模板
template bool BattleContext::hasEffect<0>(State trigger, int owner) const;
template bool BattleContext::hasEffect<1>(State trigger, int owner) const;
template bool BattleContext::hasEffect<2>(State trigger, int owner) const;
template bool BattleContext::hasEffect<3>(State trigger, int owner) const;
template bool BattleContext::hasEffect<4>(State trigger, int owner) const;
template bool BattleContext::hasEffect<10>(State trigger, int owner) const;
template bool BattleContext::hasEffect<11>(State trigger, int owner) const;
template bool BattleContext::hasEffect<12>(State trigger, int owner) const;

template bool BattleContext::consumeEffect<0>(State trigger, int opponent);
template bool BattleContext::consumeEffect<1>(State trigger, int opponent);
template bool BattleContext::consumeEffect<2>(State trigger, int opponent);
template bool BattleContext::consumeEffect<3>(State trigger, int opponent);
template bool BattleContext::consumeEffect<4>(State trigger, int opponent);
template bool BattleContext::consumeEffect<10>(State trigger, int opponent);
template bool BattleContext::consumeEffect<11>(State trigger, int opponent);
template bool BattleContext::consumeEffect<12>(State trigger, int opponent);

std::string BattleContext::getStateJson() const {
    // ... (existing implementation kept below)
    auto append_skills = [](std::ostringstream& oss, const ElfPet& pet) {
        oss << "\"skills\":[";
        for (int i = 0; i < 5; ++i) {
            const auto& skill = pet.skills[i];
            oss << "{\"id\":" << skill.id
                << ",\"name\":\"" << skill.name
                << "\",\"pp\":" << skill.pp
                << ",\"maxPp\":" << skill.maxPP
                << "}";
            if (i < 4) {
                oss << ",";
            }
        }
        oss << "]";
    };

    auto append_party = [this, &append_skills](std::ostringstream& oss, int robot_id) {
        oss << "\"party\":[";
        for (int slot = 0; slot < 6; ++slot) {
            const ElfPet& pet = seerRobot[robot_id].elfPets[slot];
            const int max_hp = pet.numericalBase[NumericalPropertyIndex::HP];
            oss << "{"
                << "\"slot\":" << slot << ","
                << "\"id\":" << pet.id << ","
                << "\"name\":\"" << pet.name << "\","
                << "\"hp\":" << pet.hp << ","
                << "\"maxHp\":" << max_hp << ","
                << "\"alive\":" << (pet.hp > 0 ? "true" : "false") << ","
                << "\"onStage\":" << (on_stage[robot_id] == slot ? "true" : "false") << ",";
            append_skills(oss, pet);
            oss << "}";
            if (slot < 5) {
                oss << ",";
            }
        }
        oss << "]";
    };

    const bool wait_input_state = currentState == State::OPERATION_CHOOSE_SKILL_MEDICAMENT
        || currentState == State::CHOOSE_AFTER_DEATH;
    const bool waiting_input = wait_input_state && is_empty;
    const bool need_death_switch0 = waiting_input && currentState == State::CHOOSE_AFTER_DEATH
        && seerRobot[0].elfPets[on_stage[0]].hp <= 0;
    const bool need_death_switch1 = waiting_input && currentState == State::CHOOSE_AFTER_DEATH
        && seerRobot[1].elfPets[on_stage[1]].hp <= 0;

    std::ostringstream oss;
    oss << "{";
    oss << "\"uuid\":" << uuid << ",";
    oss << "\"round\":" << roundCount << ",";
    oss << "\"state\":" << static_cast<int>(currentState) << ",";
    oss << "\"stateName\":\"" << state_name_cn(currentState) << "\",";
    oss << "\"needInput\":" << (waiting_input ? "true" : "false") << ",";
    oss << "\"inputPlayer\":" << current_player_id_ << ",";
    oss << "\"needDeathSwitch\":{";
    oss << "\"player0\":" << (need_death_switch0 ? "true" : "false") << ",";
    oss << "\"player1\":" << (need_death_switch1 ? "true" : "false");
    oss << "},";

    // Player 0 info
    oss << "\"player0\":{";
    oss << "\"onStage\":" << on_stage[0] << ",";
    const ElfPet& pet0 = seerRobot[0].elfPets[on_stage[0]];
    oss << "\"pet\":{";
    oss << "\"id\":" << pet0.id << ",";
    oss << "\"name\":\"" << pet0.name << "\",";
    oss << "\"hp\":" << pet0.hp << ",";
    oss << "\"maxHp\":" << pet0.numericalBase[NumericalPropertyIndex::HP] << ",";
    oss << "\"levels\":[";
    for (int i = 0; i < kAbilityLevelSlotCount; ++i) {
        oss << ability_levels[0][i];   // 等级本体在 context（不再读 pet）
        if (i < 5) oss << ",";
    }
    oss << "],";
    append_skills(oss, pet0);
    oss << "}";
    oss << ",";
    append_party(oss, 0);
    oss << "},";

    // Player 1 info
    oss << "\"player1\":{";
    oss << "\"onStage\":" << on_stage[1] << ",";
    const ElfPet& pet1 = seerRobot[1].elfPets[on_stage[1]];
    oss << "\"pet\":{";
    oss << "\"id\":" << pet1.id << ",";
    oss << "\"name\":\"" << pet1.name << "\",";
    oss << "\"hp\":" << pet1.hp << ",";
    oss << "\"maxHp\":" << pet1.numericalBase[NumericalPropertyIndex::HP] << ",";
    oss << "\"levels\":[";
    for (int i = 0; i < kAbilityLevelSlotCount; ++i) {
        oss << ability_levels[1][i];   // 同上
        if (i < 5) oss << ",";
    }
    oss << "],";
    append_skills(oss, pet1);
    oss << "}";
    oss << ",";
    append_party(oss, 1);
    oss << "},";

    // Last actions
    oss << "\"lastActions\":{";
    oss << "\"player0\":{\"type\":" << ws.lastActionType[0] << ",\"index\":" << ws.lastActionIndex[0] << "},";
    oss << "\"player1\":{\"type\":" << ws.lastActionType[1] << ",\"index\":" << ws.lastActionIndex[1] << "}";
    oss << "},";

    // Damage info
    oss << "\"damage\":{";
    oss << "\"pending\":{";
    oss << "\"attacker\":" << ws.pendingDamage.attackerId << ",";
    oss << "\"defender\":" << ws.pendingDamage.defenderId << ",";
    oss << "\"final\":" << ws.pendingDamage.final;
    oss << "},";
    oss << "\"resolved\":{";
    oss << "\"attacker\":" << ws.resolvedDamage.attackerId << ",";
    oss << "\"defender\":" << ws.resolvedDamage.defenderId << ",";
    oss << "\"final\":" << ws.resolvedDamage.final;
    oss << "}";
    oss << "}";

    oss << "}";
    return oss.str();
}

std::string BattleContext::getFullStateJson() const {
    // Shared helpers
    auto json_escape = [](const std::string& s) -> std::string {
        std::string out;
        out.reserve(s.size() + 8);
        for (char c : s) {
            switch (c) {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default: out += c;
            }
        }
        return out;
    };

    auto append_skills_full = [&](std::ostringstream& oss, const ElfPet& pet) {
        oss << "\"skills\":[";
        for (int i = 0; i < 5; ++i) {
            const auto& skill = pet.skills[i];
            oss << "{"
                << "\"id\":" << skill.id << ","
                << "\"name\":\"" << json_escape(skill.name) << "\","
                << "\"pp\":" << skill.pp << ","
                << "\"maxPp\":" << skill.maxPP << ","
                << "\"power\":" << skill.power << ","
                << "\"accuracy\":" << skill.accuracy << ","
                << "\"type\":" << static_cast<int>(skill.type) << ","
                << "\"priority\":" << skill.priority << ","
                << "\"element\":[" << skill.element[0] << "," << skill.element[1] << "]"
                << "}";
            if (i < 4) oss << ",";
        }
        oss << "]";
    };

    auto append_pet_full = [&](std::ostringstream& oss, const ElfPet& pet, int slot, int robot_id) {
        const int max_hp = pet.numericalBase[NumericalPropertyIndex::HP];
        oss << "{"
            << "\"slot\":" << slot << ","
            << "\"id\":" << pet.id << ","
            << "\"name\":\"" << json_escape(pet.name) << "\","
            << "\"hp\":" << pet.hp << ","
            << "\"maxHp\":" << max_hp << ","
            << "\"alive\":" << (pet.hp > 0 ? "true" : "false") << ","
            << "\"onStage\":" << (on_stage[robot_id] == slot ? "true" : "false") << ","
            << "\"shield\":" << pet.shield << ","
            << "\"cover\":" << pet.cover << ","
            << "\"isLocked\":" << (pet.is_locked ? "true" : "false") << ","
            << "\"speedPriority\":" << pet.speed_priority << ",";
        // Numerical properties
        oss << "\"stats\":{"
            << "\"attack\":" << pet.numericalProperties[NumericalPropertyIndex::PHYSICAL_ATTACK] << ","
            << "\"specialAttack\":" << pet.numericalProperties[NumericalPropertyIndex::SPECIAL_ATTACK] << ","
            << "\"defense\":" << pet.numericalProperties[NumericalPropertyIndex::DEFENSE] << ","
            << "\"specialDefense\":" << pet.numericalProperties[NumericalPropertyIndex::SPECIAL_DEFENSE] << ","
            << "\"speed\":" << pet.numericalProperties[NumericalPropertyIndex::SPEED] << ","
            << "\"hp\":" << pet.numericalProperties[NumericalPropertyIndex::HP]
            << "},";
        oss << "\"baseStats\":{"
            << "\"attack\":" << pet.numericalBase[NumericalPropertyIndex::PHYSICAL_ATTACK] << ","
            << "\"specialAttack\":" << pet.numericalBase[NumericalPropertyIndex::SPECIAL_ATTACK] << ","
            << "\"defense\":" << pet.numericalBase[NumericalPropertyIndex::DEFENSE] << ","
            << "\"specialDefense\":" << pet.numericalBase[NumericalPropertyIndex::SPECIAL_DEFENSE] << ","
            << "\"speed\":" << pet.numericalBase[NumericalPropertyIndex::SPEED] << ","
            << "\"hp\":" << pet.numericalBase[NumericalPropertyIndex::HP]
            << "},";
        oss << "\"levels\":[";
        for (int i = 0; i < kAbilityLevelSlotCount; ++i) {
            oss << ability_levels[robot_id][i];   // 等级本体在 context（见字段注释）
            if (i < 5) oss << ",";
        }
        oss << "],";
        oss << "\"elementalAttributes\":[" << pet.elementalAttributes[0] << "," << pet.elementalAttributes[1] << "],";
        oss << "\"soulSeal\":" << pet.soulSeal << ",";
        oss << "\"gender\":" << static_cast<int>(pet.gender) << ",";
        oss << "\"soulMark\":{"
            << "\"id\":" << pet.soulMark.id << ","
            << "\"name\":\"" << json_escape(pet.soulMark.name) << "\""
            << "},";
        append_skills_full(oss, pet);
        // Marks
        oss << ",\"marks\":[";
        for (size_t m = 0; m < pet.marks.size(); ++m) {
            oss << "{\"name\":\"" << json_escape(pet.marks[m].name) << "\",\"count\":" << pet.marks[m].count << "}";
            if (m + 1 < pet.marks.size()) oss << ",";
        }
        oss << "]";
        // Abnormal states — 从 context 的权威数组读取
        oss << ",\"abnormalStates\":[";
        bool first_ab = true;
        for (int ab_id = 0; ab_id <= kOfficialAbnormalStatusMaxId; ++ab_id) {
            if (abnormal_status_end_round[robot_id][ab_id] > roundCount) {
                int remaining = abnormal_status_end_round[robot_id][ab_id] - roundCount;
                if (!first_ab) oss << ",";
                first_ab = false;
                oss << "{\"id\":" << ab_id
                    << ",\"name\":\"" << json_escape(abnormal_status_name_cn(ab_id))
                    << "\",\"remaining\":" << remaining << "}";
            }
        }
        oss << "]";
        oss << "}";
    };

    // --- Build JSON ---
    std::ostringstream oss;
    oss << "{";

    // Basic info
    oss << "\"uuid\":" << uuid << ",";
    oss << "\"round\":" << roundCount << ",";
    oss << "\"state\":" << static_cast<int>(currentState) << ",";
    oss << "\"stateName\":\"" << state_name_cn(currentState) << "\",";
    oss << "\"needInput\":" << (need_input() ? "true" : "false") << ",";
    oss << "\"inputPlayer\":" << current_player_id_ << ",";

    // Debug settings
    oss << "\"debug\":{"
        << "\"stepMode\":" << (debug_step_mode ? "true" : "false") << ","
        << "\"breakpoints\":[";
    {
        bool first = true;
        for (int bp : breakpoints) {
            if (!first) oss << ",";
            first = false;
            oss << bp;
        }
    }
    oss << "]},";

    // Need death switch
    const bool need_ds0 = currentState == State::CHOOSE_AFTER_DEATH && is_empty &&
        seerRobot[0].elfPets[on_stage[0]].hp <= 0;
    const bool need_ds1 = currentState == State::CHOOSE_AFTER_DEATH && is_empty &&
        seerRobot[1].elfPets[on_stage[1]].hp <= 0;
    oss << "\"needDeathSwitch\":{\"player0\":" << (need_ds0 ? "true" : "false")
        << ",\"player1\":" << (need_ds1 ? "true" : "false") << "},";

    // Player 0 full party
    oss << "\"player0\":{";
    oss << "\"onStage\":" << on_stage[0] << ",";
    oss << "\"medicines\":[";
    for (int m = 0; m < MEDICINES_SIZE; ++m) {
        oss << seerRobot[0].medicines[m];
        if (m + 1 < MEDICINES_SIZE) oss << ",";
    }
    oss << "],";
    oss << "\"aliveCount\":" << seerRobot[0].allive() << ",";
    oss << "\"party\":[";
    for (int slot = 0; slot < 6; ++slot) {
        append_pet_full(oss, seerRobot[0].elfPets[slot], slot, 0);
        if (slot < 5) oss << ",";
    }
    oss << "],";
    // Abnormal status end rounds for current on-stage pet
    oss << "\"abnormalStatusEndRounds\":{";
    {
        bool first = true;
        for (int s = 0; s < kOfficialAbnormalStatusSlotCount; ++s) {
            int end = abnormal_status_end_round[0][s];
            if (end > 0) {
                if (!first) oss << ",";
                first = false;
                oss << "\"" << s << "\":" << end;
            }
        }
    }
    oss << "}";
    oss << "},";

    // Player 1 full party
    oss << "\"player1\":{";
    oss << "\"onStage\":" << on_stage[1] << ",";
    oss << "\"medicines\":[";
    for (int m = 0; m < MEDICINES_SIZE; ++m) {
        oss << seerRobot[1].medicines[m];
        if (m + 1 < MEDICINES_SIZE) oss << ",";
    }
    oss << "],";
    oss << "\"aliveCount\":" << seerRobot[1].allive() << ",";
    oss << "\"party\":[";
    for (int slot = 0; slot < 6; ++slot) {
        append_pet_full(oss, seerRobot[1].elfPets[slot], slot, 1);
        if (slot < 5) oss << ",";
    }
    oss << "],";
    oss << "\"abnormalStatusEndRounds\":{";
    {
        bool first = true;
        for (int s = 0; s < kOfficialAbnormalStatusSlotCount; ++s) {
            int end = abnormal_status_end_round[1][s];
            if (end > 0) {
                if (!first) oss << ",";
                first = false;
                oss << "\"" << s << "\":" << end;
            }
        }
    }
    oss << "}";
    oss << "},";

    // Workspace details
    oss << "\"workspace\":{";
    oss << "\"preemptiveRight\":\"" << (preemptive_right == PreemptiveRight::SEER_ROBOT_1 ? "player0"
            : preemptive_right == PreemptiveRight::SEER_ROBOT_2 ? "player1" : "none") << "\",";
    oss << "\"preemptiveLevel\":[" << ws.preemptive_level[0] << "," << ws.preemptive_level[1] << "],";
    oss << "\"roundChoice\":[";
    oss << "{\"player0\":{\"type\":" << ws.roundChoice[0][0] << ",\"index\":" << ws.roundChoice[0][1] << "}},";
    oss << "{\"player1\":{\"type\":" << ws.roundChoice[1][0] << ",\"index\":" << ws.roundChoice[1][1] << "}}";
    oss << "],";
    oss << "\"lastActions\":[";
    oss << "{\"player0\":{\"type\":" << ws.lastActionType[0] << ",\"index\":" << ws.lastActionIndex[0] << "}},";
    oss << "{\"player1\":{\"type\":" << ws.lastActionType[1] << ",\"index\":" << ws.lastActionIndex[1] << "}}";
    oss << "],";
    oss << "\"damage\":{";
    oss << "\"pending\":{\"attacker\":" << ws.pendingDamage.attackerId
        << ",\"defender\":" << ws.pendingDamage.defenderId
        << ",\"final\":" << ws.pendingDamage.final
        << ",\"isCrit\":" << (ws.pendingDamage.isCrit ? "true" : "false")
        << ",\"isTrueDamage\":" << (ws.pendingDamage.isTrueDamage ? "true" : "false") << "},";
    oss << "\"resolved\":{\"attacker\":" << ws.resolvedDamage.attackerId
        << ",\"defender\":" << ws.resolvedDamage.defenderId
        << ",\"final\":" << ws.resolvedDamage.final
        << ",\"isCrit\":" << (ws.resolvedDamage.isCrit ? "true" : "false")
        << ",\"isTrueDamage\":" << (ws.resolvedDamage.isTrueDamage ? "true" : "false") << "}";
    oss << "},";
    oss << "\"damageReduce\":{";
    oss << "\"player0Add\":[" << ws.damage_reduce_add[0][0] << "," << ws.damage_reduce_add[0][1]
        << "," << ws.damage_reduce_add[0][2] << "," << ws.damage_reduce_add[0][3] << "],";
    oss << "\"player0Mul\":[" << ws.damage_reduce_mul[0][0] << "," << ws.damage_reduce_mul[0][1]
        << "," << ws.damage_reduce_mul[0][2] << "," << ws.damage_reduce_mul[0][3] << "],";
    oss << "\"player1Add\":[" << ws.damage_reduce_add[1][0] << "," << ws.damage_reduce_add[1][1]
        << "," << ws.damage_reduce_add[1][2] << "," << ws.damage_reduce_add[1][3] << "],";
    oss << "\"player1Mul\":[" << ws.damage_reduce_mul[1][0] << "," << ws.damage_reduce_mul[1][1]
        << "," << ws.damage_reduce_mul[1][2] << "," << ws.damage_reduce_mul[1][3] << "]";
    oss << "},";
    oss << "\"viewLevels\":{"
        << "\"player0\":[" << ws.view_levels[0][0] << "," << ws.view_levels[0][1] << "," << ws.view_levels[0][2]
        << "," << ws.view_levels[0][3] << "," << ws.view_levels[0][4] << "," << ws.view_levels[0][5] << "],"
        << "\"player1\":[" << ws.view_levels[1][0] << "," << ws.view_levels[1][1] << "," << ws.view_levels[1][2]
        << "," << ws.view_levels[1][3] << "," << ws.view_levels[1][4] << "," << ws.view_levels[1][5] << "]"
        << "},";
    oss << "\"battleAttrs\":{"
        << "\"player0\":[" << ws.battle_attrs[0][NumericalPropertyIndex::PHYSICAL_ATTACK]
        << "," << ws.battle_attrs[0][NumericalPropertyIndex::SPECIAL_ATTACK]
        << "," << ws.battle_attrs[0][NumericalPropertyIndex::DEFENSE]
        << "," << ws.battle_attrs[0][NumericalPropertyIndex::SPECIAL_DEFENSE]
        << "," << ws.battle_attrs[0][NumericalPropertyIndex::SPEED]
        << "," << ws.battle_attrs[0][NumericalPropertyIndex::HP] << "],"
        << "\"player1\":[" << ws.battle_attrs[1][NumericalPropertyIndex::PHYSICAL_ATTACK]
        << "," << ws.battle_attrs[1][NumericalPropertyIndex::SPECIAL_ATTACK]
        << "," << ws.battle_attrs[1][NumericalPropertyIndex::DEFENSE]
        << "," << ws.battle_attrs[1][NumericalPropertyIndex::SPECIAL_DEFENSE]
        << "," << ws.battle_attrs[1][NumericalPropertyIndex::SPEED]
        << "," << ws.battle_attrs[1][NumericalPropertyIndex::HP] << "]"
        << "},";
    // Skill resolution status
    oss << "\"skillResolution\":{";
    oss << "\"player0Ready\":" << (ws.skill_resolution_ready[0] ? "true" : "false") << ",";
    oss << "\"player1Ready\":" << (ws.skill_resolution_ready[1] ? "true" : "false") << ",";
    oss << "\"player0Used\":" << (ws.skill_used[0] ? "true" : "false") << ",";
    oss << "\"player1Used\":" << (ws.skill_used[1] ? "true" : "false") << ",";
    oss << "\"player0Attacked\":" << (ws.has_attacked[0] ? "true" : "false") << ",";
    oss << "\"player1Attacked\":" << (ws.has_attacked[1] ? "true" : "false");
    oss << "}";
    oss << "},";

    // Effects tables
    auto append_effect_table = [&](std::ostringstream& oss, const TimedBucket& bucket) {
        oss << "{";
        bool first_state = true;
        for (const auto& [state, per_player] : bucket.all()) {
            for (int p = 0; p < 2; ++p) {
                if (per_player[p].empty()) continue;
                if (!first_state) oss << ",";
                first_state = false;
                oss << "\"" << static_cast<int>(state) << "_p" << p << "\":{";
                oss << "\"state\":" << static_cast<int>(state) << ",";
                oss << "\"stateName\":\"" << state_name_cn(state) << "\",";
                oss << "\"player\":" << p << ",";
                oss << "\"count\":" << per_player[p].size() << ",";
                oss << "\"effects\":[";
                size_t ei = 0;
                for (const auto& [key, eff] : per_player[p]) {
                    (void)key;
                    oss << "{"
                        << "\"effectId\":" << eff->getEffectId() << ","
                        << "\"owner\":" << eff->owner() << ","
                        << "\"isExpired\":" << (eff->isExpired(roundCount) ? "true" : "false") << ","
                        << "\"isRoundEffect\":" << (eff->isRoundEffect() ? "true" : "false") << ","
                        << "\"registeredRound\":" << eff->getRegisteredRound();
                    oss << "}";
                    if (ei + 1 < per_player[p].size()) oss << ",";
                    ++ei;
                }
                oss << "]}";
            }
        }
        oss << "}";
    };

    oss << "\"skillEffects\":";
    append_effect_table(oss, skills_effects);
    oss << ",\"soulMarkEffects\":";
    append_effect_table(oss, soul_mark_effects);

    // Operation log
    oss << "\"operationLog\":\"" << json_escape(operation_log_) << "\",";

    // Raw buffer info
    oss << "\"bufferInfo\":{"
        << "\"isEmpty\":" << (is_empty ? "true" : "false") << ","
        << "\"bufferSize\":" << m_buffer.size()
        << "},";

    // Operation collection status
    oss << "\"operationCollected\":{"
        << "\"player0\":" << (operation_collected[0] ? "true" : "false") << ","
        << "\"player1\":" << (operation_collected[1] ? "true" : "false")
        << "}";

    oss << "}";
    return oss.str();
}
