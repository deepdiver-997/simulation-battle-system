// 湮灭之主·咤克斯 4762 技能组（2026-09-26）
//
// 技能与效果分解（moves 表 mon_id=4762，本文件只写**尚未注册**的效果；
// 889/636/478、1440/693、911/1304/843、881/1232/2105 均有既有注册，直接复用）：
//   无限·虚数湮灭 37912（先3物理）: 889 消回合成功→异常 / 636 消状态成功→异常 / 478 封属窗口
//   沉寂·永夜悼亡 29047（属性）  : 1440 次数免异常 / 803 ★本文件 / 693 额外增伤窗口 /
//                                  584 ★本文件（攻击技能附带秒杀概率）
//   崩解·万念劫灰 29048（属性）  : 911 全属性强化 / 1647 ★本文件（逐回合吸取）/
//                                  1304 忽略双防窗口 / 843 先制窗口
//   劫数·限界归无 37913（特攻）  : 881 条件先制 / 1232 反转弱化 / 2296 ★本文件 /
//                                  2105 未击败→消逝 / 2017 ★本文件
//
// ★ 584 与特性瞬杀的叠加口径（本次设计核心，详见交接文档 2026-09-26 段）：
//   584 = "{0}回合内自己的所有攻击技能都附有{1}%的秒杀概率"。概率**写进
//   `ws.instant_kill_permille_bonus`**（回合作用域，ws reset→窗口效果每回合重写），
//   消费点在 `trait_instant_kill_zero_hook`（红伤落地后）与特性瞬杀**各自独立掷点**
//   ——它是技能效果不是特性，不进 trait_state_（那个容器管的是"特性身份"，生命周期
//   锚在登场槽/来源方；584 的生命周期锚在回合窗口，两套生命周期不该共享一个容器）。
//
// 语料差异记档（reference/scrape/out/all_posts_final.jsonl idx=225《机制解析—湮灭之主·咤克斯》）：
//   帖子描述第五技"按剩余 PP 获得等量魔王咒怨 / 必定秒杀"——现行 DB（2026-09-19 更新）
//   37913 的效果清单（881/1232/2296/2105/2017）无此子句，按"最终以数据库为准"实现。
#include "common.h"

namespace {

// ── 584 {0}回合内自己的所有攻击技能都附有{1}%的秒杀概率 ──────────────────
// 每回合把概率（千分点）写进 ws 槽；消费在 trait_instant_kill_zero_hook 的第二掷点。
// 注册点选 MOVE_RIGHT：该时点 execute(0)+execute(1) **双侧都跑**（battleFsm
// handle_BattleFirstMoveRight），一个注册点即覆盖先/后手；且先于本回合全部伤害结算。
EffectResult effect_584_write_bonus(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int permille = args.int_args[2];
    if (self < 0 || self > 1 || permille <= 0) {
        return EffectResult::kOk;
    }
    // 赋值（非累加）：同源去重由桶的 key 保证，这里幂等重写。
    ctx->ws.instant_kill_permille_bonus[self] = permille;
    return EffectResult::kOk;
}

EffectResult effect_skill_584(BattleContext* ctx, const EffectArgs& args) {
    // args = [self, target, rounds, pct]（模板占位 {0}=回合数、{1}=百分点）
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    const int pct = args.int_args[3];
    if (self < 0 || self > 1 || rounds <= 0 || pct <= 0) {
        return EffectResult::kOk;
    }
    // 先出手当回合立即生效（InRounds：起点=本回合）；后出手顺延 → 不提前写，
    // 等下一回合 MOVE_RIGHT 桶重写。
    const EffectWindowKind window = (g_core && g_core->effect_window_kind)
        ? g_core->effect_window_kind(584)
        : EffectWindowKind::InRounds;
    if (ctx->round_effect_start_round(self, rounds, window) == ctx->roundCount) {
        ctx->ws.instant_kill_permille_bonus[self] = pct * 10;   // 百分点 → 千分点
    }
    register_round_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, self, 584, rounds,
                          {self, 1 - self, pct * 10}, &effect_584_write_bonus);
    return EffectResult::kOk;
}

// ── 803 命中后{0}%令对手{1}，未触发则恢复自身最大体力的1/{2}体力
//        且{3}回合内自身受到的伤害不超过{4} ─────────────────────────────────
// args = [self, target, chance, anomaly, heal_denom, cap_rounds, cap_value]
// 上限落点 = 管线 **CAP** 阶段（锁伤位；官方减伤区"伤害锁定"，MITIGATE 类别——
// 不被"挡伤失效"压制，但可被 697「无视伤害限制」凭证穿透）。窗口判法同 525
// （roundCount 半开区间；条目驻留到 clearAllEffects，窗口外自身失效）。
// ⚠️ 已知边界：CAP 只在红伤/属性直伤管线，粉伤无锁伤位 →"受到的伤害"暂不含粉伤。
EffectResult effect_skill_803(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 7) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int target = args.int_args[1];
    const int chance = args.int_args[2];
    const int anomaly = args.int_args[3];
    const int heal_denom = args.int_args[4];
    const int cap_rounds = args.int_args[5];
    const int cap_value = args.int_args[6];
    if (self < 0 || self > 1 || target < 0 || target > 1 || cap_value <= 0) {
        return EffectResult::kOk;
    }
    if (roll_percent(ctx->rng(), chance)) {
        if (g_core && g_core->apply_anomaly) {
            g_core->apply_anomaly(ctx, target, anomaly, random_anomaly_duration(), self);
        }
        return EffectResult::kOk;
    }
    heal_max_hp_fraction(ctx, self, heal_denom);
    const int start = ctx->round_effect_start_round(self, cap_rounds, EffectWindowKind::InRounds);
    const int expire = start + cap_rounds;
    ctx->register_damage_effect(
        DamagePhase::CAP, self, DamageEffectCategory::MITIGATE,
        [self, start, expire, cap_value](BattleContext* c, int owner) {
            if (!c || owner != self) {
                return;
            }
            DamageSnapshot& d = c->resolvedDamage;
            if (d.defenderId != self || d.final <= 0) {
                return;
            }
            if (c->roundCount < start || c->roundCount >= expire) {
                return;   // 窗口外
            }
            const int attacker = d.attackerId;
            if (attacker >= 0 && attacker <= 1) {
                const auto& cred = c->ws.attack_credential[attacker];
                if (cred.valid && cred.ignore_damage_limit) {
                    return;   // 697 凭证可穿锁伤
                }
            }
            if (d.final > cap_value) {
                d.final = cap_value;
            }
        });
    return EffectResult::kOk;
}

// ── 1647 {0}回合内每回合使用技能吸取对手最大体力的1/{1}，吸取体力时若自身体力
//        低于最大体力的1/{2}则吸取效果翻倍，对手免疫百分比伤害时额外附加{3}点真实伤害 ──
// args = [self, target, rounds, denom, double_denom, true_dmg]
// 吸取 = 1303 同款（"每回合**使用技能**"→ AFTER_ACTION 对桶 + used_skill 门；
// 翻倍 = 两次 drain_hp 两段独立结算，1303 先例）。真伤附加条件查 PINK_DAMAGE 免疫票
//（⚠️ 引擎免粉票不分固定/百分比，"免疫粉"即视为命中本条件——细分待官方口径）。
EffectResult effect_1647_drain_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core || !g_core->drain_hp) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int target = args.int_args[1];
    const int denom = args.int_args[2];
    const int true_dmg = args.int_args[3];
    if (!used_skill_this_round(ctx, self) || denom <= 0) {
        return EffectResult::kOk;
    }
    const bool low = ctx->getPet(self).hp < max_hp_of(ctx, self) / 2;
    g_core->drain_hp(ctx, /*actor=*/self, /*target=*/target, denom);
    if (low) {
        g_core->drain_hp(ctx, self, target, denom);   // "吸取效果翻倍"
    }
    if (true_dmg > 0
        && ctx->is_immune(target, ImmunityType::PINK_DAMAGE, ctx->currentState)) {
        if (g_core->deal_true_damage) {
            g_core->deal_true_damage(ctx, target, true_dmg, self);
        }
    }
    return EffectResult::kOk;
}

EffectResult effect_skill_1647(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 6) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int target = args.int_args[1];
    const int rounds = args.int_args[2];
    const int denom = args.int_args[3];
    const int true_dmg = args.int_args[5];
    if (self < 0 || self > 1 || target < 0 || target > 1 || rounds <= 0 || denom <= 0) {
        return EffectResult::kOk;
    }
    for (State st : {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_round_effect(ctx, st, self, 1647, rounds, {self, target, denom, true_dmg},
                              &effect_1647_drain_tick);
    }
    return EffectResult::kOk;
}

// ── 2296 令对方体力上限最低的存活精灵体力上限提升至与自身相等并恢复等量体力，
//        然后此精灵所有非体力上限的能力值减少提升的数值，最多减少50点且最多减少至1 ──
// 语料口径（idx=225）："如果对手所有精灵当前体力高于咤克斯就无视发生，必须是对手任意
// 一个精灵体力低于咤才能触发"→ 本实现按**体力上限**判（模板原文），无低于者即 no-op。
// 数值层直写 pet.numericalBase（suit.cpp 数值层先例）；若目标在场，同步 ws.battle_attrs
// 让本回合立刻生效（sync 每回合从 numericalBase 重基 → 下一回合自动延续）。
EffectResult effect_skill_2296(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core
        || !g_core->raise_max_hp_flat) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int target = args.int_args[1];
    if (self < 0 || self > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    const int self_max = max_hp_of(ctx, self);
    int best_slot = -1;
    int best_max = 0;
    for (int slot = 0; slot < 6; ++slot) {
        const ElfPet& pet = ctx->seerRobot[target].elfPets[slot];
        if (ctx->is_vanished(target, slot) || pet.hp <= 0) {
            continue;   // "存活精灵"（消逝/阵亡都不算）
        }
        const int cap = pet.numericalBase[NumericalPropertyIndex::HP];
        if (best_slot < 0 || cap < best_max) {
            best_slot = slot;
            best_max = cap;
        }
    }
    if (best_slot < 0 || best_max >= self_max) {
        return EffectResult::kOk;   // 没有体力上限低于自身的存活精灵 → 不触发
    }
    const int raised = self_max - best_max;
    g_core->raise_max_hp_flat(ctx, target, best_slot, raised);
    ElfPet& pet = ctx->seerRobot[target].elfPets[best_slot];
    const int new_max = pet.numericalBase[NumericalPropertyIndex::HP];
    pet.hp = std::min(new_max, pet.hp + raised);   // 恢复等量体力
    // 面板削减：非体力上限 5 项，每项减 min(raised, 50)、下限钳 1。
    const int cut = std::min(raised, 50);
    const NumericalPropertyIndex kStats[5] = {
        NumericalPropertyIndex::PHYSICAL_ATTACK, NumericalPropertyIndex::SPECIAL_ATTACK,
        NumericalPropertyIndex::DEFENSE,         NumericalPropertyIndex::SPECIAL_DEFENSE,
        NumericalPropertyIndex::SPEED,
    };
    for (const NumericalPropertyIndex idx : kStats) {
        pet.numericalBase[idx] = std::max(1, pet.numericalBase[idx] - cut);
    }
    if (best_slot == ctx->on_stage[target]) {
        // 在场目标：本回合视图立即生效（下一回合 sync 重基自动延续本体值）。
        for (const NumericalPropertyIndex idx : kStats) {
            ctx->ws.battle_attrs[target][idx] = pet.numericalBase[idx];
        }
    }
    return EffectResult::kOk;
}

// ── 2017 击败对手则对方下只出战精灵首回合先制效果失效 ─────────────────────
// 两段式：① 击败检测节点挂 BATTLE_OPPONENT_DEFEAT_STATUS（"击败对手时"官方时点，
// execute(-1) 双侧都跑；find_defeated_foe_slot 归因）→ 置 pending；
// ② ROUND_START 常驻小节点消费 pending：首个回合开始 → ws.priority_nullified[对方]=true
//（该 ws 槽由 handle_BattleFirstMoveRight 统一清先制——与束缚(28)同语义）。
// 为什么不直接在登场事件里写 ws：换宠/替换登场多发生在 CHOOSE~行动期，写入会被
// 随后的 ROUND_START reset 冲掉 → 必须"回合开始时再写"（进入者的首回合 = 置位后
// 首个 ROUND_START，两种登场路径都成立）。
constexpr int k2017ArmKey = 20170001;     // plugin_storage：击败检测置位（插件自约定）
constexpr int k2017EnteredKey = 20170101; // plugin_storage：对手已有精灵登场过

EffectResult effect_2017_round_start_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int target = args.int_args[1];
    if (self < 0 || self > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    auto armed_it = ctx->plugin_storage.find(k2017ArmKey + self);
    auto entered_it = ctx->plugin_storage.find(k2017EnteredKey + self);
    if (armed_it == ctx->plugin_storage.end()
        || entered_it == ctx->plugin_storage.end()) {
        return EffectResult::kOk;
    }
    const bool* armed = std::any_cast<bool>(&armed_it->second);
    const bool* entered = std::any_cast<bool>(&entered_it->second);
    if (!armed || !entered || !*armed || !*entered) {
        return EffectResult::kOk;
    }
    // "下只出战精灵"= 置位后**首个登场的对方精灵**，其首回合 = 该登场后的首个
    // ROUND_START（换宠执行点无论在 CHOOSE 还是行动期，都先于下一个回合开始）。
    ctx->ws.priority_nullified[target] = true;
    ctx->plugin_storage[k2017ArmKey + self] = false;      // 消费（"下只"一次）
    ctx->plugin_storage[k2017EnteredKey + self] = false;
    return EffectResult::kOk;
}

EffectResult effect_2017_defeat_probe(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    if (self < 0 || self > 1) {
        return EffectResult::kOk;
    }
    // 击杀归因（与 soul_lib find_defeated_foe_slot 同款：对手在场宠倒地 + 最后一击是本方
    // ——last_damage_actor 是死亡漏斗同款真相源）。此处内联：moves_lib 不含 soul_lib 头。
    const int foe = 1 - self;
    const int foe_slot = ctx->on_stage[foe];
    if (ctx->seerRobot[foe].elfPets[foe_slot].hp > 0
        || ctx->last_damage_actor[foe][foe_slot] != self) {
        return EffectResult::kOk;   // 本回合没有"本方击败对手"
    }
    ctx->plugin_storage[k2017ArmKey + self] = true;
    // 击败瞬间清登场标记：对手**当前**这只已倒地，此前的登场记录不算"下只"——
    // 只认置位之后的首个 EVENT_ENTER_STAGE。
    ctx->plugin_storage[k2017EnteredKey + self] = false;
    return EffectResult::kOk;
}

EffectResult effect_skill_2017(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int target = args.int_args[1];
    if (self < 0 || self > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    // ① 击败检测：本回合一次性（duration=1、起点=本回合；击败时点在 SKILL_EFFECT 之后
    //    同回合到达，执行未过期；下一回合起 expired 清理）。
    {
        Effect e(2017, 0, self, 1, EffectArgs(std::vector<int>{self, target}),
                 &effect_2017_defeat_probe);
        auto ce = std::make_unique<ContinuousEffect>(e, State::BATTLE_OPPONENT_DEFEAT_STATUS,
                                                     self, 1, ctx->roundCount);
        ce->source_id_ = 2017;
        ctx->register_skill_effect(State::BATTLE_OPPONENT_DEFEAT_STATUS, self, std::move(ce));
    }
    // ② 登场标记 watcher：对方任一精灵登场即置 entered（回调自筛 actor，投递无过滤）。
    ctx->event_center_.register_watcher(
        EventType::EVENT_ENTER_STAGE, self, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [self, target](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || ev.actor != target) {
                return;
            }
            wctx->plugin_storage[k2017EnteredKey + self] = true;
        },
        WatcherScope::TEAM);
    // ③ 消费节点：挂到战斗结束（切换即随 ON_STAGE epoch 作废——官方对"施放者退场后
    //    击败收益是否还在"无实证，v1 绑定施放者在场）。
    register_round_effect(ctx, State::BATTLE_ROUND_START, self, 2017, 999,
                          {self, target}, &effect_2017_round_start_tick);
    return EffectResult::kOk;
}

}  // namespace

// ── 注册 ──
SKILL_EFFECT(584, effect_skill_584)
SKILL_EFFECT(803, effect_skill_803)
SKILL_EFFECT(1647, effect_skill_1647)
SKILL_EFFECT(2296, effect_skill_2296)
SKILL_EFFECT(2017, effect_skill_2017)
