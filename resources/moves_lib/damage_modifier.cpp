// 经典伤害杂项（反弹 34 / 惩罚占位 35）/ 概率固伤(93) / 不微弱(760) / 威力视图族
// （先手40 / 后手30 / 连用递增9 / 连击占位31——31 的本体动作在 loadSkills 的 combo 表）。
//
// （2026-09-17 从 lib_1.cpp 按家族拆分；移动零改动，行为由全量场景回归守护。）
#include "common.h"

namespace {

EffectResult effect_skill_4(BattleContext* context, const EffectArgs& args) {
    if (!context || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }

    int attacker_id = args.int_args[0];
    if (attacker_id < 0 || attacker_id > 1) {
        return EffectResult::kOk;
    }

    int stat_index = args.int_args[2];
    int probability = args.int_args[3];
    int level_change = args.int_args[4];

    if (stat_index < 0 || stat_index >= 6) {
        return EffectResult::kOk;
    }

    if (!roll_percent(probability)) {
        return EffectResult::kOk;
    }

    if (g_core && g_core->stat_change) {
        g_core->stat_change(context, attacker_id, stat_index, level_change);
    }

    return EffectResult::kOk;
}

/**
 * Effect 6 - Recoil damage (疲惫)
 * Info: "对方所受伤害的1/{0}会反弹给自己"
 * Args: [attackerId(0), targetId(1), divisor]
 *
 * Used by: 突进 (10038), 猛禽 (10039)
 * Note: This effect is applied after damage is dealt, reflecting 1/divisor of damage taken back to attacker
 */

EffectResult effect_skill_6(BattleContext* context, const EffectArgs& args) {
    if (!context || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }

    int attacker_id = args.int_args[0];  // The one who deals damage (but takes recoil)
    int divisor = args.int_args[2];

    if (attacker_id < 0 || attacker_id > 1 || divisor <= 0) {
        return EffectResult::kOk;
    }

    const DamageSnapshot& damage = context->resolvedDamage;
    if (damage.final <= 0) {
        return EffectResult::kOk;
    }

    const int recoil = std::max(1, damage.final / divisor);
    // ⚠️ 走**粉伤原语**，不要直写 hp：直写会绕过 护罩/抗性/减粉、不发 EVENT_TAKE_DAMAGE、
    //    也不会走死亡结算（hp 打到 0 却没有死亡通知）。反伤 = "对攻击方造成 X 点固定伤害"。
    if (g_core && g_core->deal_pink_damage) {
        g_core->deal_pink_damage(context, attacker_id, recoil, DamageKind::FIXED, -1);
    }
    return EffectResult::kOk;
}

/**
 * Effect 7 - Fear (害怕)
 * Info: "对方体力高于自己时才能命中，将对方体力减到和自己相同"
 * Args: none
 *
 * Used by: 同生共死 (10036)
 * Note: This is a special move that only works when opponent HP > self HP,
 * and sets opponent HP to equal self HP (deals fixed HP-equalizing damage)
 */

EffectResult effect_skill_7(BattleContext* context, const EffectArgs& args) {
    if (!context || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }

    int attacker_id = args.int_args[0];
    int target_id = args.int_args[1];

    if (attacker_id < 0 || attacker_id > 1 || target_id < 0 || target_id > 1) {
        return EffectResult::kOk;
    }

    ElfPet& attacker = context->seerRobot[attacker_id].elfPets[context->on_stage[attacker_id]];
    ElfPet& target = context->seerRobot[target_id].elfPets[context->on_stage[target_id]];

    // Can only hit if target HP > self HP
    if (target.hp <= attacker.hp) {
        return EffectResult::kOk;
    }

    // 将对方体力减到与自己相同。★ 口径（用户 2026-09-15）：这是**早期老代码，直接改 hp，
    // 相当于真实伤害**——不是固定伤害（我先前按固定伤害接是错的：那会被护罩/固定抗性/
    // 减粉削掉，而老代码的语义是"体力直接拉到相等"，任何减免都不参与）。
    // 用 `deal_true_damage` 表达：真伤**直通护盾/护罩、穿抗性/免疫**，数值上等价于直写 hp，
    // 但额外带来 事件 + 死亡结算（直写 hp 的旧做法是静默的，掉到 0 也不通知）。
    const int diff = target.hp - attacker.hp;
    if (g_core && g_core->deal_true_damage) {
        g_core->deal_true_damage(context, target_id, diff, attacker_id);
    }
    return EffectResult::kOk;
}

/**
 * Effect 8 - Sleep (睡眠)
 * Info: "对方体力高于自己时才能命中，将对方体力减到和自己相同" (same as effect 7 description, but different effect)
 * Args: none
 *
 * Used by: 手下留情 (10057)
 * Note: Leaves opponent at 1 HP when damage would exceed opponent HP
 * Actually: Looking at effect_info id=8, it says "伤害大于对方体力时，对方会余下1体力"
 * This is a "leave 1 HP" effect
 */

EffectResult effect_skill_8(BattleContext* context, const EffectArgs& args) {
    if (!context || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }

    int attacker_id = args.int_args[0];
    int target_id = args.int_args[1];

    if (attacker_id < 0 || attacker_id > 1 || target_id < 0 || target_id > 1) {
        return EffectResult::kOk;
    }

    const ElfPet& target = context->seerRobot[target_id].elfPets[context->on_stage[target_id]];

    DamageSnapshot& damage = context->resolvedDamage;
    if (damage.final >= target.hp) {
        // Keep final damage aligned with the FSM apply_resolved_damage() pipeline.
        damage.final = std::max(0, target.hp - 1);
    }

    return EffectResult::kOk;
}

// effect 760 - 攻击时造成的伤害不会出现微弱（克制为微弱时都变普通，克制保持克制）。
// 置位 ws.no_weakness → 伤害公式 restraint<1 钳 1（王系通用，王·焚天君焱等）。
EffectResult effect_760_no_weakness(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ctx->ws.no_weakness[owner] = true;
    return EffectResult::kOk;
}

// ── effect 22 「{0}%令对手疲惫{1}回合」—— 灵魂干涉 20499（Args=[100, 2]）──
// Args = [owner, target, 概率%, 回合数]。疲惫 = AbnormalStatusId::Fatigue(7)。
// 概率在**施放时**掷一次（与概率封属的"每次响应掷"不同——那是 condition，这里是一次性动作）。
EffectResult effect_22_fatigue(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int chance = args.int_args[2];
    const int rounds = args.int_args[3] > 0 ? args.int_args[3] : 1;
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    if (!roll_percent(chance)) {
        return EffectResult::kOk;
    }
    if (g_core && g_core->apply_anomaly) {
        g_core->apply_anomaly(ctx, target, static_cast<int>(AbnormalStatusId::Fatigue),
                              rounds, owner);
    }
    return EffectResult::kOk;
}

// ── helper：挡伤/弹伤是否已被抑制（蚀砚之泪 ≥4 滴）──
// 真相源是 `ElfPet::damage_suppress_mask`（bit = DamageEffectCategory）——管线 walk 会按类别跳过，
// 但**不在管线里**的时点桶效果必须自己查（soul_lib 有一份同语义的 `damage_guard_suppressed`；
// 插件之间不互相链接，故各持一份、语义以掩码为准）。
bool damage_guard_suppressed(BattleContext* ctx, int owner) {
    if (!ctx || owner < 0 || owner > 1) {
        return true;   // 参数异常按"已失效"处理（宁可不弹，不误弹）
    }
    constexpr int kBits = (1 << static_cast<int>(DamageEffectCategory::BLOCK))
                        | (1 << static_cast<int>(DamageEffectCategory::DETECT));
    return (ctx->getPet(owner).damage_suppress_mask & kBits) != 0;
}

// ── effect 34「将所受的伤害{0}倍反馈给对手」—— 能量 11274 / 鲁斯王「克制」10020（Args=[2]）──
// Args = [owner, target, 倍率]。
// ★ 口径（用户 2026-09-15）：读**当前回合的红伤管线结果** `ctx->resolvedDamage`
//   ——"如果自己先出手只会弹 0 伤害出去，因为对手本回合在管线里还没有造成伤害"
//   （`ws.resolvedDamage` 每回合 reset，先手时它还是空的）。
// ★ **弹伤直接改 hp，不管对方的减伤**（与鲁斯王「克制」同族）：不是粉伤、不是真伤，
//   就是"受到多少 × 倍率"原样打回去——对方的点数/百分比减伤、护盾护罩一概不参与。
//   故这里不调任何伤害原语，直接扣 hp（唯一的"直写"合法场景：它本就不走伤害系统）。
// ★ **必须自检 `damage_suppress_mask`**（沧岚蚀砚之泪≥4滴 → 挡伤/弹伤失效）：本效果是
//   **时点桶**里的 EffectFn，**不经过 DamagePipeline 的 walk**，所以管线那两行类别跳过够不着它
//   ——真相源是掩码本身，谁不在管线里谁自己查（soul_lib 的挡伤函数是同一个套路，
//   见 `damage_guard_suppressed`）。漏了这一步的症状：带 4 泪的精灵照样能弹伤。
EffectResult effect_34_reflect_taken_red(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int mult = args.int_args[2];
    if (owner < 0 || owner > 1 || mult <= 0) {
        return EffectResult::kOk;
    }
    // 挡伤/弹伤已被抑制（蚀砚之泪≥4滴）→ 不弹。DETECT = 弹伤/受击检测那一类。
    if (damage_guard_suppressed(ctx, owner)) {
        return EffectResult::kOk;
    }
    const DamageSnapshot& damage = ctx->resolvedDamage;
    // 本回合我得先"挨过打"：管线结果里 defender 是我，才有可弹的量。
    if (damage.defenderId != owner || damage.attackerId < 0 || damage.attackerId > 1
        || !damage.isRed || damage.final <= 0) {
        return EffectResult::kOk;   // 先出手 / 本回合没被打 → 弹 0
    }
    const int reflect = damage.final * mult;
    ElfPet& attacker_pet = ctx->getPet(damage.attackerId);
    attacker_pet.hp -= reflect;
    if (attacker_pet.hp < 0) {
        attacker_pet.hp = 0;
    }
    return EffectResult::kOk;
}

// ── effect 816「免疫对手下{0}次攻击技能造成的伤害并附加**免疫量**的真实伤害 ──
//      —— 冥王·苍月影「弦月锋华」25848（Args=[1]）──
// Args = [owner, target, 次数, 回合数?]。这是**另一种典型反弹**：反弹出去的是
// **真实伤害**（不是固定伤害、也不是 34 那种直写 hp）。
//
// 落在 `DamagePhase::BLOCK`：这一格天然是"归零类"，而且**能拿到被归零前的 final**
// （= 免疫量）。⚠️ 不走"次数型免伤票据"：那条路由 BLOCK 的**默认回调**先消费并归零，
// 我方的回调再跑时 final 已经是 0、拿不到免疫量了 —— 顺序上取不到值，
// 所以这里把"免疫 N 次"的计数**放在自己的闭包状态里**（一次注册一个计数，
// 技能用两次就是两条各自计数，正好是"用两次挡两次"的语义）。
// ★ **共享实现**（不是共用 effect id）：这条机制被多个技能以**不同 id** 引用——
//   · 816 弦月锋华：独立模板，整条都是它；
//   · 2360 王·龙予狂宴：复合子句里的最后一段"…且免疫下次对手攻击的伤害并附加免疫量的真实伤害"。
//   措辞逐字相同 → 机制一个 → 抽成 helper，各自的包装函数只负责掰参数。
//   新技能再遇到这条，直接调它，别再抄一份闭包。
void arm_immunity_reflect_true(BattleContext* ctx, int self, int count) {
    if (!ctx || self < 0 || self > 1 || count <= 0) {
        return;
    }
    // ON_STAGE 绑定：换宠即作废（条目由 pipeline_valid_id epoch 惰性失效）。
    ctx->register_damage_effect(
        DamagePhase::BLOCK, self, DamageEffectCategory::BLOCK,
        [self, remaining = count](BattleContext* c, int bucket_owner) mutable {
            if (!c || bucket_owner != self || remaining <= 0) {
                return;
            }
            DamageSnapshot& d = c->resolvedDamage;
            if (d.defenderId != self || !d.isRed || d.final <= 0) {
                return;   // 只免疫"对手的攻击技能伤害"
            }
            const int attacker = d.attackerId;
            const int immune_amount = d.final;   // ★ **免疫量** = 被归零前的伤害值
            if (attacker < 0 || attacker > 1) {
                return;
            }
            --remaining;
            // 归零（整段快照，不只 final）
            d.base = 0;
            d.afterAdd = 0;
            d.afterMul = 0;
            d.final = 0;
            d.addPct = 0;
            d.mulCoef = 0.0;
            // 附加**免疫量的真实伤害**（直通护盾/护罩、穿抗性/免疫）
            if (g_core && g_core->deal_true_damage) {
                g_core->deal_true_damage(c, attacker, immune_amount, self);
            }
        });
}

EffectResult effect_816_immunity_reflect_true(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int count = args.int_args[2] > 0 ? args.int_args[2] : 1;
    arm_immunity_reflect_true(ctx, self, count);
    return EffectResult::kOk;
}

// ── 2360 —— 王·龙予狂宴（29187）复合子句末段，措辞与 816 逐字相同 → 调同一 helper。
//（2026-09-29 接线：helper 注释邀请"新技能再遇到这条直接调它"；29187 目前无精灵
//  携带（boss 技），先注册接账，载体发放即生效。）
EffectResult effect_2360_immunity_reflect_true(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int count = args.int_args[2] > 0 ? args.int_args[2] : 1;
    arm_immunity_reflect_true(ctx, self, count);
    return EffectResult::kOk;
}

// ── effect 35「惩罚，对方能力等级越高，此技能威力越大」—— 灵光之怒 10997 ──
// ⚠️ **空实现、占位**（用户 2026-09-15：先跳过，大概率不会用到）。
// 它是**变威力**（要写 `ws.skill_power_view[owner]`），但官方**没给加成公式**
//（每正等级加多少威力？只算正等级还是净等级？）——凭空造一个公式等于替官方定语义。
// 拿到公式后在这里补：读对手 `levels[]` → 算加成 → `ws.skill_power_view[owner] = power + bonus`。
EffectResult effect_35_punishment_placeholder(BattleContext* ctx, const EffectArgs& args) {
    (void)ctx;
    (void)args;
    return EffectResult::kOk;
}

// ════════════════════════════════════════════════════════════════════
// 批 1：原语直通族 + 古早族补员（2026-09-17，按技能覆盖数排序的头部家族）
//   31  连击（303 技）——次数由 loadSkills 的 combo_arg_indices 消化，本体 no-op
//   47  免弱窗口（671 技）——grant_immunity(STAT_DROP) 窗口
//   180 消回合（288 技）——break_round_effects 直通
//   521 反转自身下降（290 技）——stat_reversal 直通（AFTER_ACTION，同 1221 口径）
//   447 保底伤害（131 技）——ws.damage_floor + 管线 FLOOR 阶段默认条目（core）
//   16  命中后睡眠（119 技）——古早施加族补员
//   （456 条件秒杀 109 技：运行时解析器 TargetHpBelow+Kill 路径已覆盖，
//     Kill 标签 2026-09-17 起走 force_hp_to_zero 秒杀体系——无需注册处理器）
// ════════════════════════════════════════════════════════════════════

// effect 31 —— 经典连击族（乱突/疯狂乱抓[2,5]、花瓣舞[2,3]）。无 effect_info 行，
// 模板本体没有"动作"：连击次数在 loadSkills 里由 combo_arg_indices(31) 取走
// （args[0]=下限/args[1]=上限 → Skills::roll_combo_count 每次使用掷一次）。
// 注册 no-op 仅为启动审计消音（数据引用了 31，不注册会报"未实现"）。
EffectResult effect_31_combo_noop(BattleContext* /*ctx*/, const EffectArgs& /*args*/) {
    return EffectResult::kOk;
}

// ── effect 40 —— 先出手的话威力为2倍（时光飞石 37804 等，137 技）──
// SKILL_EFFECT 时点威力视图已被引擎物化为基线威力（ON_SKILL_HIT materialize），
// ×2 写回（operator= 置 rewritten）→ 走标准变威力重算管线。后手时无事发生。
EffectResult effect_40_first_power_double(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    if (self < 0 || self > 1 || !acting_side_is_first(ctx)) {
        return EffectResult::kOk;
    }
    auto& view = ctx->ws.skill_power_view[self];
    if (view.value > 0) {
        view = view.value * 2;
    }
    return EffectResult::kOk;
}

// ── effect 30 —— 后出手的话威力为2倍（霸天屠龙斩 16513 等，93 技）──
EffectResult effect_30_second_power_double(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    if (self < 0 || self > 1 || acting_side_is_first(ctx)) {
        return EffectResult::kOk;
    }
    auto& view = ctx->ws.skill_power_view[self];
    if (view.value > 0) {
        view = view.value * 2;
    }
    return EffectResult::kOk;
}

// ── effect 9 —— 连续使用每次威力增加{0}，最高威力{1}（水晶烈冲 37585 等，103 技）──
// Args=[owner,target,inc,max_power,skill_id]（skill_id 由 loadSkills 追加）。
// 连用判定用 ctx->skill_use_seq（每方技能使用序号，核心字段）：存上次使用时的序号 + 连用
// 次数——本次序号 == 上次+1 且同一技能 → 连用继续；序号跳变（中间用过别的技能）→ 断连；
// 嗑药/被控回合不出手、序号不动 → 天然不断连（嗑药口径的自然推论）。
// 首次使用无加成；威力封顶 {1}。计数存 plugin_storage（战斗级，clearAllEffects 清）。
EffectResult effect_9_growing_power(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int inc = args.int_args[2];
    const int max_power = args.int_args[3];
    const int skill_id = args.int_args[4];
    if (self < 0 || self > 1 || inc <= 0 || skill_id <= 0) {
        return EffectResult::kOk;
    }
    const int kLastKey = 900000 + self * 3;    // [kLastKey]=上次序号 [kLastKey+1]=技能id [kLastKey+2]=连用次数
    const int kCntKey = kLastKey + 2;
    const int seq_now = ctx->skill_use_seq[self];
    int last_seq = -1;
    int last_skill = -1;
    int count = 0;
    if (ctx->plugin_storage.count(kLastKey)) {
        last_seq = any_int_or(ctx->plugin_storage[kLastKey]);
        last_skill = any_int_or(ctx->plugin_storage[kLastKey + 1]);
        count = any_int_or(ctx->plugin_storage[kCntKey]);
    }
    const bool consecutive = (last_seq >= 0 && seq_now == last_seq + 1
                              && last_skill == skill_id);
    count = consecutive ? count + 1 : 1;
    ctx->plugin_storage[kLastKey] = seq_now;
    ctx->plugin_storage[kLastKey + 1] = skill_id;
    ctx->plugin_storage[kCntKey] = count;
    const int bonus = inc * (count - 1);   // 首用无加成
    auto& view = ctx->ws.skill_power_view[self];
    if (bonus > 0 && view.value >= 0) {
        view = std::min(view.value + bonus, max_power);
    }
    return EffectResult::kOk;
}


// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表）──
    SKILL_EFFECT(4, effect_skill_4);   // 自身属性变化
    // 注：effect 5（对手属性变化）注册在本函数**末尾**的 effect_5_change_opp_stat——
    // 同一 effect id 后注册覆盖先注册。此处原有一个 effect_skill_5 手抄版（只改本体、
    // 不同步视图、且不查免弱），被末尾那条覆盖后就是死代码，已删。
    SKILL_EFFECT(6, effect_skill_6);   // 反弹伤害
    SKILL_EFFECT(7, effect_skill_7);   // 同生共死
    SKILL_EFFECT(8, effect_skill_8);   // 手下留情
    SKILL_EFFECT(760, effect_760_no_weakness);               // 攻击不微弱(克制<1→钳1,通用王系)
    SKILL_EFFECT(22, effect_22_fatigue);                // 灵魂干涉: 概率疲惫
    // ── 批 4 收尾 ──
    SKILL_EFFECT(34, effect_34_reflect_taken_red);      // 能量/鲁斯王克制: 按受到的红伤×倍率直写hp
    SKILL_EFFECT(35, effect_35_punishment_placeholder); // 灵光之怒: 惩罚（**空实现占位**，公式待官方）
    SKILL_EFFECT(816, effect_816_immunity_reflect_true); // 弦月锋华: 免疫N次攻击伤害 + 附免疫量真伤
    SKILL_EFFECT(2360, effect_2360_immunity_reflect_true); // 王·龙予狂宴: 免疫下次攻击伤害+附免疫量真伤（816 同 helper）

    // ── 批 1：原语直通族 + 古早族补员（2026-09-17）──
    SKILL_EFFECT(31, effect_31_combo_noop);            // 经典连击: 次数由 loadSkills combo_arg_indices 消化
    SKILL_EFFECT(40, effect_40_first_power_double);    // 先出手威力2倍(变威力)
    SKILL_EFFECT(30, effect_30_second_power_double);   // 后出手威力2倍(变威力)
    SKILL_EFFECT(9, effect_9_growing_power);           // 连续使用威力递增,最高威力封顶

}  // namespace
