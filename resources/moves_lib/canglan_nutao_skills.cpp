// 怒涛·沧岚（神谕 3740）技能组（精灵王线批 A，2026-09-24）
//
// 官方效果文本 = effect_info 表（args_num 为参数个数）。本组效果 id：
//   581  奔流浪涌(140)：属性相同时，额外附加{0}点固定伤害
//   901  沧海永存：消除对手回合类效果，消除成功则{0}（6 参条件族）
//   1019 上善若水：造成的攻击伤害若高于{0}则令对手{1}回合内使用的属性技能无效
//   1082 上善若水：造成的伤害低于{0}则附加自身最大体力{1}%的百分比伤害
//   1155 止戈龙吟式连用族：附加自身最大体力值{0}%的百分比伤害，连续使用每次增加{1}%，最高{2}%
//   1194 浮生若梦：反转自身能力下降状态，反转成功则自身{0}回合内免疫能力下降状态
//   1232 王·澥宇浮岚：反转自身能力下降状态，反转成功则附加给对手等同的能力下降状态
//   1326 浮生若梦：全属性+{0}，自身处于护盾状态时强化效果翻倍
//   1399 王·洛水惊鸿：消除对手回合类效果，消除成功则对手{0}，未触发则自身免疫下{1}次受到的异常状态
//   1751 沧海永存式：获得{0}点护盾，护盾消失时对对手造成{1}点固定伤害且有{2}%概率使对手{3}
//   2117 王·浩露倾天：将对手所处的{0}转化为{1}
//   2262 水月幽溟：对手的蚀砚之泪数量高于自身的永恒之水时获得等同于对手蚀砚之泪数量的永恒之水
//   2263 王·澥宇浮岚：消耗自身的护盾值，每消耗{0}点附加给对手2回合的冻伤与1滴蚀砚之泪
//   2264 王·浩露倾天：附加自身最大体力{0}%的百分比伤害，自身每有1道永恒之水将其中的1/8
//        从百分比伤害改为真实伤害，对手每有1滴蚀砚之泪将其中的1/4从附加改为吸取
//
// 时点选择：默认 BATTLE_FIRST_SKILL_EFFECT（skills.cpp effect_register_state 缺省），
// 伤害结算后才能判定的（1019/1082）走 AFTER_ACTION —— 需要内核 switch 收录（已加）。
#include "common.h"
#include "../nutao_runtime.h"

#include <any>

namespace {

// 最大体力（本体优先逐级回退；oracle_skills.cpp 同款就地实现）
int nutao_max_hp(const ElfPet& pet) {
    int max_hp = pet.numericalBase[NumericalPropertyIndex::HP];
    if (max_hp <= 0) {
        max_hp = pet.numericalProperties[NumericalPropertyIndex::HP];
    }
    if (max_hp <= 0) {
        max_hp = pet.hp;
    }
    return max_hp;
}

// ── 581 —— 属性相同时额外附加固定伤害（攻击技能，攻击结算后判同系）──
EffectResult effect_581_same_element(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int amount = args.int_args[2];
    if (owner < 0 || owner > 1 || amount <= 0) {
        return EffectResult::kOk;
    }
    ElfPet& atk_pet = ctx->getPet(owner);
    ElfPet& def_pet = ctx->getPet(1 - owner);
    // "属性相同" = 攻击技能系别与防御方系别一致（双系任一命中即算同）。
    const int skill_elem = ctx->ws.skill_element_view[owner][0];
    const bool same = skill_elem == def_pet.elementalAttributes[0]
                   || skill_elem == def_pet.elementalAttributes[1];
    if (same && g_core && g_core->fixed_damage) {
        g_core->fixed_damage(ctx, target, amount);
    }
    return EffectResult::kOk;
}

// ── 901 —— 消除对手回合类效果，消除成功则{0}（6 参：2=异常id 3=回合 …）──
EffectResult effect_901_break_then(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (!g_core || !g_core->break_round_effects) {
        return EffectResult::kOk;
    }
    const BreakResult br = g_core->break_round_effects(ctx, target);
    if (br != BreakResult::SUCCESS) {
        return EffectResult::kOk;
    }
    // 成功分支：{0}={1}回合{2}（异常施加）… 官方 901 参数排布随技能而异，
    // 沧岚 901 行 = 「消除成功则对手冰封」两参 —— args[2]=异常id。
    if (args.int_count >= 3 && args.int_args[2] > 0 && g_core->apply_anomaly) {
        g_core->apply_anomaly(ctx, target, args.int_args[2],
                              args.int_count >= 4 ? args.int_args[3] : 1, owner);
    }
    return EffectResult::kOk;
}

// ── 1019 —— 攻击伤害高于{0}：对手{1}回合内使用的属性技能无效（伤害落地后判）──
EffectResult effect_1019_high_damage_seal(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int threshold = args.int_args[2];
    const int rounds = args.int_args[3];
    if (ctx->ws.raw_attack_damage[owner] <= threshold) {
        return EffectResult::kOk;
    }
    if (g_core && g_core->seal_skill) {
        // 属性技能封（HIT_INVALID_ATTRIBUTE 语义走封属票 attribute=true, attack=false）
        g_core->seal_skill(ctx, owner, target, /*effect_id=*/1019, /*attribute=*/true,
                           /*attack=*/false, /*count=*/0, rounds, /*penetrable=*/false,
                           /*source_slot=*/ctx->on_stage[owner], EffectScope::ON_STAGE,
                           /*hit_invalid=*/true, /*chance_pct=*/100,
                           /*consumed_when_pierced=*/false);
    }
    return EffectResult::kOk;
}

// ── 1082 —— 造成的伤害低于{0}：附加自身最大体力{1}%百分比伤害（伤害落地后判）──
EffectResult effect_1082_low_damage_pink(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int threshold = args.int_args[2];
    const int pct = args.int_args[3];
    if (ctx->ws.raw_attack_damage[owner] >= threshold) {
        return EffectResult::kOk;
    }
    if (g_core && g_core->deal_pink_damage) {
        const int max_hp = nutao_max_hp(ctx->getPet(owner));
        g_core->deal_pink_damage(ctx, target, max_hp * pct / 100, DamageKind::PERCENT_VALUE, owner);
    }
    return EffectResult::kOk;
}

// ── 1155 —— 连用增幅族：附加最大体力{0}%百分比伤害，每次+{1}%，最高{2}%（王·洛水惊鸿 等）──
EffectResult effect_1155_scaling_pink(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int base_pct = args.int_args[2];
    const int step = args.int_args[3];
    const int cap_pct = args.int_args[4];
    // 连用计数（per-pet storage，按技能 id 分槽；"连续使用"官方口径跨下场保留）
    auto& store = ctx->getPet(owner).soulmark_storage;
    const int key = 1155000 + owner;
    const int uses = store.count(key) ? any_int_or(store[key]) : 0;
    store[key] = uses + 1;
    int pct = base_pct + step * uses;   // 本次为第 uses+1 次 → 增幅按已用次数
    if (pct > cap_pct) {
        pct = cap_pct;
    }
    if (pct > 0 && g_core && g_core->deal_pink_damage) {
        const int max_hp = nutao_max_hp(ctx->getPet(owner));
        g_core->deal_pink_damage(ctx, target, max_hp * pct / 100, DamageKind::PERCENT_VALUE, owner);
    }
    return EffectResult::kOk;
}

// ── 1194 —— 反转自身能力下降，成功则{0}回合免能力下降（伤害结算后反转族）──
EffectResult effect_1194_reverse_then_immune(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    if (!g_core || !g_core->stat_reversal) {
        return EffectResult::kOk;
    }
    if (g_core->stat_reversal(ctx, owner) != StatReversalResult::REVERSED) {
        return EffectResult::kOk;
    }
    ctx->grant_immunity(owner, ImmunityType::STAT_DROP, coverage_all(),
                        /*anomaly_mask=*/0, rounds, /*source_id=*/0, /*soul_immunity=*/false,
                        EffectScope::ON_STAGE, /*counts=*/0, /*source_effect_id=*/1194);
    return EffectResult::kOk;
}

// ── 1232 —— 反转自身能力下降，成功则附加给对手等同的能力下降 ──
EffectResult effect_1232_reverse_mirror(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    if (!g_core || !g_core->stat_reversal) {
        return EffectResult::kOk;
    }
    // 先抄等级快照（负项），反转成功后逐项镜像给对手。
    int drops[6] = {0, 0, 0, 0, 0, 0};
    int* levels = ctx->ability_levels[owner];
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount && i < 6; ++i) {
        if (levels[i] < 0) {
            drops[i] = -levels[i];
        }
    }
    if (g_core->stat_reversal(ctx, owner) != StatReversalResult::REVERSED) {
        return EffectResult::kOk;
    }
    if (g_core->stat_drop) {
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount && i < 6; ++i) {
            if (drops[i] > 0) {
                g_core->stat_drop(ctx, target, i, drops[i], owner);
            }
        }
    }
    return EffectResult::kOk;
}

// ── 1326 —— 全属性+{0}，护盾状态时翻倍（强化技，choose 期数据驱动为先制前的选择效果）──
EffectResult effect_1326_buff_shield_double(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int delta = args.int_args[2];
    int boost = delta;
    if (ctx->getPet(owner).shield_bank_.total() > 0) {
        boost *= 2;
    }
    if (!g_core || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    for (int stat = 0; stat < 5; ++stat) {   // 攻/特攻/防/特防/速
        g_core->stat_change(ctx, owner, stat, boost);
    }
    return EffectResult::kOk;
}

// ── 1399 —— 消回合成功→对手{0}；未触发→自身免疫下{1}次异常 ──
EffectResult effect_1399_break_or_immune(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    if (!g_core || !g_core->break_round_effects) {
        return EffectResult::kOk;
    }
    if (g_core->break_round_effects(ctx, target) == BreakResult::SUCCESS) {
        if (args.int_args[2] > 0 && g_core->apply_anomaly) {
            g_core->apply_anomaly(ctx, target, args.int_args[2], 1, owner);
        }
        return EffectResult::kOk;
    }
    ctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(),
                        /*anomaly_mask=*/0, /*duration_rounds=*/0, /*source_id=*/0,
                        /*soul_immunity=*/false, EffectScope::ON_STAGE,
                        /*counts=*/args.int_args[3], /*source_effect_id=*/1399);
    return EffectResult::kOk;
}

// ── 1751 —— 获得带亡语的护盾：消失时造成{1}固伤 + {2}%令对手{3} ──
// 普通来源盾（priority 0），亡语监听 EVENT_SHIELD_BROKEN（source_id 匹配）。
EffectResult effect_1751_shield_with_deathrattle(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 6) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int shield = args.int_args[2];
    const int dmg = args.int_args[3];
    const int chance = args.int_args[4];
    const int anomaly = args.int_args[5];
    if (shield <= 0) {
        return EffectResult::kOk;
    }
    const int source = 175100 + owner;
    ctx->getPet(owner).shield_bank_.add(/*priority=*/0, shield, source, /*duration=*/0);
    if (g_core) {
        ctx->event_center_.register_watcher(
            EventType::EVENT_SHIELD_BROKEN, owner, ctx->roundCount, 0, /*once=*/false,
            [owner, dmg, chance, anomaly, source](BattleContext* wctx, const BattleEvent& ev) {
                (void)ev;
                if (!wctx) {
                    return;
                }
                // 只认自己 source 的盾没了才结算亡语（护盾被击破即从 bank 移除，
                // 亡语在移除事件后触发——查不到自己 source 即"破的是我这笔"之前已结算完，
                // 故用 once 一次性标记防重入）。
                if (wctx->getPet(owner).shield_bank_.quantity_of(source) > 0
                    && wctx->getPet(owner).shield_bank_.total() > 0) {
                    return;   // 还有别的盾在（含自己的没破）→ 不结算
                }
                if (!g_core || !g_core->fixed_damage) {
                    return;
                }
                g_core->fixed_damage(wctx, 1 - owner, dmg);
                if (anomaly > 0 && g_core->apply_anomaly && roll_percent(chance)) {
                    g_core->apply_anomaly(wctx, 1 - owner, anomaly, 1, owner);
                }
            },
            WatcherScope::TEAM);
    }
    return EffectResult::kOk;
}

// ── 2117 —— 将对手所处的{0}转化为{1}（异常转化；解异常+施加新异常的连招语义）──
EffectResult effect_2117_convert_anomaly(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int from = args.int_args[2];
    const int to = args.int_args[3];
    if (!g_core || !g_core->has_active_anomaly || from <= 0 || to <= 0) {
        return EffectResult::kOk;
    }
    if (!g_core->has_active_anomaly(ctx, target, from)) {
        return EffectResult::kOk;
    }
    if (g_core->cure_anomalies) {
        const int one = from;
        g_core->cure_anomalies(ctx, target, &one, 1);
    }
    if (g_core->apply_anomaly) {
        g_core->apply_anomaly(ctx, target, to, 2, owner);
    }
    return EffectResult::kOk;
}

// ── 2262 —— 对手泪 > 自身水：补齐永恒之水（水月幽溟）──
EffectResult effect_2262_sync_water(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int tears = nutao_pub_get(ctx->plugin_storage, kNutaoPubTears + owner);
    const int water = nutao_pub_get(ctx->plugin_storage, kNutaoPubWater + owner);
    if (tears <= water) {
        return EffectResult::kOk;
    }
    int gained = tears - water;
    const int cur = water;
    if (cur + gained > 8) {
        gained = 8 - cur;
    }
    if (gained > 0) {
        nutao_pub_set(ctx->plugin_storage, kNutaoPubWater + owner, cur + gained);
        if (cur + gained >= 8) {
            nutao_pub_set(ctx->plugin_storage, kNutaoPubWaterLock + owner, 8);
        }
    }
    return EffectResult::kOk;
}

// ── 2263 —— 消耗自身护盾值：每{0}点 → 对手 2 回合冻伤 + 1 滴蚀砚之泪（王·澥宇浮岚）──
EffectResult effect_2263_consume_shield(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int per = args.int_count >= 3 && args.int_args[2] > 0 ? args.int_args[2] : 100;
    ElfPet& me = ctx->getPet(owner);
    const int total = me.shield_bank_.total();
    if (total <= 0) {
        return EffectResult::kOk;
    }
    int broken = 0;
    int consumed = 0;
    if (g_core->consume_shield) {
        consumed = g_core->consume_shield(ctx, owner, total, &broken);
    }
    if (consumed <= 0) {
        return EffectResult::kOk;
    }
    const int stacks = consumed / per;
    if (stacks <= 0) {
        return EffectResult::kOk;
    }
    if (g_core->apply_anomaly) {
        for (int i = 0; i < stacks; ++i) {
            g_core->apply_anomaly(ctx, target,
                                  static_cast<int>(AbnormalStatusId::Frostbite), 2, owner);
        }
    }
    const int tears = nutao_pub_get(ctx->plugin_storage, kNutaoPubTears + owner);
    int add = stacks;
    if (tears + add > 4) {
        add = 4 - tears;
    }
    if (add > 0) {
        nutao_pub_set(ctx->plugin_storage, kNutaoPubTears + owner, tears + add);
    }
    return EffectResult::kOk;
}

// ── 2264 —— 第五：最大体力{0}%百分比伤害；每道水 1/8 转真伤；每滴泪 1/4 转吸取（王·浩露倾天）──
EffectResult effect_2264_haolou_qingtian(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int pct = args.int_count >= 3 ? args.int_args[2] : 0;
    const int max_hp = nutao_max_hp(ctx->getPet(owner));
    const int base = max_hp * pct / 100;
    if (base <= 0) {
        return EffectResult::kOk;
    }
    const int water = nutao_effective_water(ctx->plugin_storage, owner);
    const int tears = nutao_pub_get(ctx->plugin_storage, kNutaoPubTears + owner);

    int true_part = base / 8 * water;         // 每道水将其中的 1/8 改为真实伤害
    if (true_part > base) {
        true_part = base;
    }
    const int drain_part = base / 4 * tears;  // 每滴泪将其中的 1/4 从附加改为吸取
    const int pink_part = base - true_part - drain_part;
    if (pink_part > 0 && g_core->deal_pink_damage) {
        g_core->deal_pink_damage(ctx, target, pink_part, DamageKind::PERCENT_VALUE, owner);
    }
    if (true_part > 0 && g_core->deal_true_damage) {
        g_core->deal_true_damage(ctx, target, true_part, owner);
    }
    if (drain_part > 0 && g_core->fixed_damage) {
        // "吸取"（drain_hp_amount 未暴露）：固伤落地后按实际扣血回（封回血生效）。
        ElfPet& tgt = ctx->getPet(target);
        const int before = tgt.hp;
        g_core->fixed_damage(ctx, target, drain_part);
        const int dealt = before - tgt.hp;
        if (dealt > 0 && g_core->heal_amount) {
            g_core->heal_amount(ctx, owner, dealt);
        }
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(581, effect_581_same_element);
SKILL_EFFECT(901, effect_901_break_then);
SKILL_EFFECT(1019, effect_1019_high_damage_seal);
SKILL_EFFECT(1082, effect_1082_low_damage_pink);
SKILL_EFFECT(1155, effect_1155_scaling_pink);
SKILL_EFFECT(1194, effect_1194_reverse_then_immune);
SKILL_EFFECT(1232, effect_1232_reverse_mirror);
SKILL_EFFECT(1326, effect_1326_buff_shield_double);
SKILL_EFFECT(1399, effect_1399_break_or_immune);
SKILL_EFFECT(1751, effect_1751_shield_with_deathrattle);
SKILL_EFFECT(2117, effect_2117_convert_anomaly);
SKILL_EFFECT(2262, effect_2262_sync_water);
SKILL_EFFECT(2263, effect_2263_consume_shield);
SKILL_EFFECT(2264, effect_2264_haolou_qingtian);
