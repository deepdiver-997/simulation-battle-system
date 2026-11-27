// 圣光格劳瑞（神谕 4004）技能组（精灵王线，2026-09-24）
//
//   760  两代第五共有：攻击时造成的伤害不会出现微弱（ws.no_weakness 现成通道）
//   1551 王·万雷禁法（自爆，PP3）：{0}%消耗自身全部体力，{1}%消耗敌方全部体力，
//        自身触发→消除敌方所有PP；敌方触发→消除自身所有PP；均未触发→恢复自身全部体力。
//        idx=19 口径：消耗体力精灵数（含自己；快照有待命背包时计入）>5 → 改为消耗至 1 点
//        不挂斩杀印记；=5 → 己方所有精灵王尽可能获得其专属特性首个标记（标记发放待各王
//        落地后接入，本实现仅计数与恢复分支）。
//   967  王·风雷啸世：攻击结束时附加对手双防之和{0}%的百分比伤害
//   968  王·风雷啸世：命中对手后有{0}%使对手{1}，先出手时概率翻倍
//   2548 王·圣鸣净界：解除双方所处的异常状态，解除成功则附加对手双防值{0}%的真实伤害，
//        若对手不处于异常状态则吸取对手双防值{1}%的体力
//   2551 王·圣鸣净界：击败对手时，自身获得1层神圣之霆（且到霆减少为止对方下只精灵
//        属性技命中失效——后半句待做，见交接文档）
//   2552 王·圣鸣净界：未击败对手时，自身下次属性技能消耗对手的1层荣光之裁以使当次技能
//        附加自身速度值{0}%的电·圣灵系伤害（"下次属性技"以旗存储，MOVE_RIGHT 消费）
#include "common.h"
#include "../gelaorui_runtime.h"

#include <any>

namespace {

constexpr int kMark = 2532;

int gla_max_hp(const ElfPet& pet) {
    int max_hp = pet.numericalBase[NumericalPropertyIndex::HP];
    if (max_hp <= 0) {
        max_hp = pet.numericalProperties[NumericalPropertyIndex::HP];
    }
    if (max_hp <= 0) {
        max_hp = pet.hp;
    }
    return max_hp;
}

// ── 760 —— 伤害不出现微弱（MOVE_RIGHT 置位，伤害管线钳制）──
EffectResult effect_760_no_weak(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner >= 0 && owner <= 1) {
        ctx->ws.no_weakness[owner] = true;
    }
    return EffectResult::kOk;
}

// ── 1551 —— 自爆（直开对局=6 槽语义；BP 快照存在时计入待命背包）──
EffectResult effect_1551_self_destruct(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    // 官方参数 {0}=30 {1}=30；moves.side_effect_arg 为空时按默认（DB effect_info args_num=2）
    const int self_pct = args.int_count >= 3 && args.int_args[2] > 0 ? args.int_args[2] : 30;
    const int foe_pct = args.int_count >= 4 && args.int_args[3] > 0 ? args.int_args[3] : 30;

    // 消耗体力精灵数量（含自己）：出战 6 + 待命背包 6——两背包同构且**战斗创建时定死**
    //（2026-09-25 用户口径），待命位旗标由 Room 装配期按 id 精确评估，不再"每只各计 1"。
    int consumers = 0;
    for (int side = 0; side < 2; ++side) {
        for (int slot = 0; slot < 6; ++slot) {
            if (ctx->seerRobot[side].elfPets[slot].has_hp_consume_kit) {
                ++consumers;
            }
        }
        if (ctx->standby_initialized) {
            for (int i = 0; i < 6; ++i) {
                if (ctx->standby_pet_ids[side][i] > 0 && ctx->standby_hp_consume_kit[side][i]) {
                    ++consumers;   // 待命位按精灵判（快照旗标；官方口径同族）
                }
            }
        }
    }
    const bool over_five = consumers > 5;
    const bool exactly_five = consumers == 5;

    auto consume_all = [&](int side) -> bool {
        ElfPet& pet = ctx->getPet(side);
        if (pet.hp <= 0) {
            return false;
        }
        if (over_five) {
            pet.hp = 1;   // 官方：>5 只时自爆改为消耗至 1 点体力（不挂斩杀印记）
        } else {
            pet.hp = 0;
        }
        return true;
    };

    const bool self_hit = roll_percent(ctx->rng(), self_pct);
    const bool foe_hit = !self_hit && roll_percent(ctx->rng(), foe_pct);
    if (self_hit) {
        consume_all(owner);
        if (g_core->pp_reduce) {
            g_core->pp_reduce(ctx, target, 99);   // 消除敌方所有 PP
        }
    } else if (foe_hit) {
        consume_all(target);
        if (g_core->pp_reduce) {
            g_core->pp_reduce(ctx, owner, 99);
        }
    } else if (g_core->heal_amount) {
        g_core->heal_amount(ctx, owner, gla_max_hp(ctx->getPet(owner)));
    }

    // =5 只：恢复己方**不在场**精灵王全体 + 己方所有精灵王获"首个标记"
    //（语料 idx=19/20 口径；恢复量官方文本未给，按满体力、仅 off-stage 落地——待实测）。
    // 不在场恢复：不占在场位、无恢复管线语义（封回血/减疗管不到下场精灵），直写 hp。
    if (exactly_five) {
        for (int slot = 0; slot < 6; ++slot) {
            ElfPet& pet = ctx->seerRobot[owner].elfPets[slot];
            if (pet.is_spirit_king && slot != ctx->on_stage[owner] && pet.hp > 0) {
                pet.hp = gla_max_hp(pet);
            }
        }
        for (int slot = 0; slot < 6; ++slot) {
            const ElfPet& pet = ctx->seerRobot[owner].elfPets[slot];
            if (pet.is_spirit_king && pet.id > 0) {
                gla_pub_set(ctx->plugin_storage, kGlaPubMarkBase + pet.id, 1);
            }
        }
        if (ctx->standby_initialized) {
            for (int i = 0; i < 6; ++i) {
                if (ctx->standby_pet_ids[owner][i] > 0 && ctx->standby_is_spirit_king[owner][i]) {
                    gla_pub_set(ctx->plugin_storage,
                                kGlaPubMarkBase + ctx->standby_pet_ids[owner][i], 1);
                }
            }
        }
    }
    return EffectResult::kOk;
}

// ── 967 —— 攻击结束时附加双防和{0}%百分比伤害（伤害落地后）──
EffectResult effect_967_def_pink(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int pct = args.int_args[2];
    ElfPet& opp = ctx->getPet(target);
    const int def_sum = opp.numericalBase[NumericalPropertyIndex::DEFENSE]
                      + opp.numericalBase[NumericalPropertyIndex::SPECIAL_DEFENSE];
    g_core->deal_pink_damage(ctx, target, def_sum * pct / 100, DamageKind::PERCENT_VALUE, owner);
    return EffectResult::kOk;
}

// ── 968 —— 命中后{0}%令对手{1}，先出手翻倍 ──
EffectResult effect_968_chance_anomaly(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core || !g_core->apply_anomaly) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int pct = args.int_args[2];
    const int anomaly = args.int_args[3];
    int chance = ctx->is_second_mover(owner) ? pct : pct * 2;
    if (roll_percent(ctx->rng(), chance)) {
        g_core->apply_anomaly(ctx, target, anomaly, 1, owner);
    }
    return EffectResult::kOk;
}

// ── 2548 —— 解除双方异常；成功→双防和{0}%真伤；对手无异常→吸取双防和{1}%体力 ──
EffectResult effect_2548_dual_dispel(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int true_pct = args.int_args[2];
    const int drain_pct = args.int_args[3];
    int dispelled = 0;
    if (g_core->dispel_active_anomalies) {
        dispelled += g_core->dispel_active_anomalies(ctx, owner);
        dispelled += g_core->dispel_active_anomalies(ctx, target);
    }
    ElfPet& opp = ctx->getPet(target);
    const int def_sum = opp.numericalBase[NumericalPropertyIndex::DEFENSE]
                      + opp.numericalBase[NumericalPropertyIndex::SPECIAL_DEFENSE];
    if (dispelled > 0 && true_pct > 0 && g_core->deal_true_damage) {
        g_core->deal_true_damage(ctx, target, def_sum * true_pct / 100, owner);
    } else if (dispelled == 0 && drain_pct > 0) {
        // 吸取 = 固伤 + 按实际回（2264 同款）
        if (g_core->fixed_damage) {
            ElfPet& tgt = ctx->getPet(target);
            const int before = tgt.hp;
            g_core->fixed_damage(ctx, target, def_sum * drain_pct / 100);
            const int dealt = before - tgt.hp;
            if (dealt > 0 && g_core->heal_amount) {
                g_core->heal_amount(ctx, owner, dealt);
            }
        }
    }
    return EffectResult::kOk;
}

// ── 2551 —— 击败对手 → +1 霆（镜像 +1 裁）──
EffectResult effect_2551_kill_ting(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (ctx->getPet(1 - owner).hp > 0) {
        return EffectResult::kOk;   // 未击败
    }
    // 复用魂印侧发放（发布槽直接写，镜像逻辑一致）
    int ting = gla_pub_get(ctx->plugin_storage, kGlaPubTing + owner) + 1;
    if (ting > 6) {
        ting = 6;
    }
    gla_pub_set(ctx->plugin_storage, kGlaPubTing + owner, ting);
    const int opp = 1 - owner;
    int cai = gla_cai_get(g_core, ctx, opp) + 1;
    if (cai > 6) {
        cai = 6;
    }
    gla_cai_set(g_core, ctx, opp, cai);
    // 后半句：直到霆减少，对方下只精灵属性技命中失效——武装旗 + 快照霆层
    //（魂印 ROUND_START 见旗重授命中失效票；霆跌破快照 → 清旗解除）。
    gla_pub_set(ctx->plugin_storage, kGlaPubBlind + opp, 1);
    gla_pub_set(ctx->plugin_storage, kGlaPubBlindTing + owner, ting);
    return EffectResult::kOk;
}

// ── 2552 —— 未击败 → 记旗；下次属性技消费 1 层裁附加速度值{0}%电·圣灵系伤害 ──
EffectResult effect_2552_arm_next_attribute(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int pct = args.int_args[2];
    if (ctx->getPet(1 - owner).hp > 0) {
        auto& store = ctx->getPet(owner).soulmark_storage;
        store[2552000 + owner] = pct;   // 记旗（下次属性技消费）
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(760, effect_760_no_weak);
SKILL_EFFECT(1551, effect_1551_self_destruct);
SKILL_EFFECT(967, effect_967_def_pink);
SKILL_EFFECT(968, effect_968_chance_anomaly);
SKILL_EFFECT(2548, effect_2548_dual_dispel);
SKILL_EFFECT(2551, effect_2551_kill_ting);
SKILL_EFFECT(2552, effect_2552_arm_next_attribute);
