#include <utils/sbs_rand.h>
// 混沌地王莱茵哈特（神谕 3516）技能组（精灵王线批 A，2026-09-24）
//
// 官方效果文本 = effect_info 表。本组效果 id：
//   605  地面撕裂：自身体力{0}对手时附加{1}值{2}%的百分比伤害
//   1052 大地腐蚀：消除对手能力提升状态，消除成功则令自身全属性+{0}
//   1087 无间地狱：反转自身能力下降状态，反转成功则对手下{0}回合技能无效
//   1128 王·昏天灭地：击败对手则令对手下{0}次使用的攻击技能无效
//   1280 混沌焦土断：附加自身当前体力值{0}%的百分比伤害，每次使用增加{1}%，最高{2}%
//   1281 王·昏天灭地：消耗自身所有护盾值并造成等量固定伤害，>300 则额外附加自身最大体力
//        1/3 百分比伤害，≤300 且 >0 则恢复自身最大体力 1/3 并造成等量百分比伤害
//   1626 王·伏尸沓岳：后出手时将当回合护盾所承受的伤害值以百分比伤害{0}%反弹给对手
//   1752 无间地狱：获得{0}点护盾，护盾消失时吸取对手{1}点体力且令对手随机{2}个技能 PP 归零
//   1803 溃灭坤舆：技能无效时，恢复自身最大体力的 1/3 且对手 2 回合内属性技能命中效果失效，
//        自身体力低于 1/2 时恢复效果翻倍，自身体力高于 1/2 时失效回合+1
//   2318 王·伏尸沓岳：消耗自身的全部护盾并附加消耗量 100% 的百分比伤害，每消耗 400 点附加的
//        伤害额外提升 40 点，以此法消耗的护盾视为被击破
//   2319 王·天乖陆离：附加自身双防之和{0}%的百分比伤害，对手每有1只留魂之蛊额外附加{1}%，
//        对手未受到百分比伤害时自身获得1层封邪之嶂且每有1层封邪之嶂为对手种下1只留魂之蛊
//   2320 冥府真诰：{0}回合内每回合使用技能令对手{1}，未触发则对手下{7}回合攻击技能无效
//
// 时点：605/1082 型伤害后判定的走 AFTER_ACTION（skills.cpp 已收录 605 的 case）；
// 1087/1221 反转族按内核口径挂伤害结算后（521/1221 同款——本组统一 AFTER_ACTION）。
#include "common.h"
#include "../diwang_runtime.h"

#include <any>

namespace {

// 最大体力（oracle_skills.cpp 同款就地实现）
int diwang_max_hp(const ElfPet& pet) {
    int max_hp = pet.numericalBase[NumericalPropertyIndex::HP];
    if (max_hp <= 0) {
        max_hp = pet.numericalProperties[NumericalPropertyIndex::HP];
    }
    if (max_hp <= 0) {
        max_hp = pet.hp;
    }
    return max_hp;
}

// moves 侧在场守卫（不能引 soul 内部件，就地实现）
inline bool is_diwang_onstage_pub(BattleContext* ctx, int owner) {
    return ctx->on_stage[owner] >= 0 && ctx->on_stage[owner] < 6
        && ctx->getPet(owner).soulMark.id == 2411;
}

// moves 侧加嶂（与 soul 侧同规则就地实现：cap4 + ws 重写）
inline void add_zhang_pub(BattleContext* ctx, int owner, int n) {
    int layers = diwang_pub_get(ctx->plugin_storage, kDiwangPubZhang + owner) + n;
    if (layers > 4) {
        layers = 4;
    }
    diwang_pub_set(ctx->plugin_storage, kDiwangPubZhang + owner, layers);
    ctx->ws.shield_absorb_bonus_pct[owner] = 10 * layers;
}

// ── 605 —— 自身体力{0}(1=低于 2=高于)对手时附加{1}值{2}%百分比伤害（伤害落地后）──
EffectResult effect_605_hp_compare_pink(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int cmp = args.int_args[2];      // 1=低于 2=高于
    const int base = args.int_args[3];     // {1}值：1=最大体力
    const int pct = args.int_args[4];
    ElfPet& me = ctx->getPet(owner);
    ElfPet& opp = ctx->getPet(1 - owner);
    const bool lower = me.hp < opp.hp;
    if ((cmp == 1 && !lower) || (cmp == 2 && lower)) {
        return EffectResult::kOk;
    }
    if (g_core && g_core->deal_pink_damage) {
        const int amount = diwang_max_hp(me) * pct / 100;
        g_core->deal_pink_damage(ctx, target, amount, DamageKind::PERCENT_VALUE, owner);
    }
    (void)base;
    return EffectResult::kOk;
}

// ── 1052 —— 消除对手能力提升，成功则自身全属性+{0} ──
EffectResult effect_1052_clear_then_buff(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int delta = args.int_args[2];
    if (!g_core->clear_stat_boosts) {
        return EffectResult::kOk;
    }
    // 带归因变体（K4）：混地魂印"能力提升被消除、吸取"监听按 target 归边。
    const int cleared = g_core->clear_stat_boosts_as
                            ? g_core->clear_stat_boosts_as(ctx, target, owner)
                            : g_core->clear_stat_boosts(ctx, target);
    if (cleared > 0 && g_core->stat_change) {
        for (int stat = 0; stat < 5; ++stat) {
            g_core->stat_change(ctx, owner, stat, delta);
        }
    }
    return EffectResult::kOk;
}

// ── 1087 —— 反转自身能力下降，成功则对手下{0}回合技能无效（1221 反转族，伤害结算后）──
EffectResult effect_1087_reverse_then_invalid(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int rounds = args.int_args[2];
    if (!g_core->stat_reversal
        || g_core->stat_reversal(ctx, owner) != StatReversalResult::REVERSED) {
        return EffectResult::kOk;
    }
    // "下N回合技能无效"：全技能命中失效票（窗口型，攻击+属性两条都注册）。
    // ⚠️ 同 2320：SEAL tick 模型下"下 N 回合"传 N+1 抵消当回合 tick（否则活不过当回合末）。
    if (g_core->seal_skill) {
        for (const bool attr : {true, false}) {
            g_core->seal_skill(ctx, owner, target, /*effect_id=*/1087, attr, !attr,
                               /*count=*/0, rounds + 1, /*penetrable=*/false,
                               /*source_slot=*/ctx->on_stage[owner], EffectScope::ON_STAGE,
                               /*hit_invalid=*/true, /*chance_pct=*/100,
                               /*consumed_when_pierced=*/false);
        }
    }
    return EffectResult::kOk;
}

// ── 1128 —— 击败对手则令对手下{0}次使用的攻击技能无效（击败后时点）──
EffectResult effect_1128_defeat_seal(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int counts = args.int_args[2];
    if (ctx->getPet(target).hp > 0) {
        return EffectResult::kOk;   // 未击败
    }
    if (g_core->seal_skill) {
        g_core->seal_skill(ctx, owner, target, /*effect_id=*/1128, /*attribute=*/false,
                           /*attack=*/true, counts, /*duration_rounds=*/0,
                           /*penetrable=*/false, /*source_slot=*/ctx->on_stage[owner],
                           EffectScope::ON_STAGE, /*hit_invalid=*/true, /*chance_pct=*/100,
                           /*consumed_when_pierced=*/false);
    }
    return EffectResult::kOk;
}

// ── 1280 —— 当前体力{0}%百分比伤害，每次+{1}%，最高{2}%（连用，伤害落地后）──
EffectResult effect_1280_scaling_current_hp(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int base_pct = args.int_args[2];
    const int step = args.int_args[3];
    const int cap_pct = args.int_args[4];
    auto& store = ctx->getPet(owner).soulmark_storage;
    const int key = 1280000 + owner;
    const int uses = store.count(key) ? any_int_or(store[key]) : 0;
    store[key] = uses + 1;
    int pct = base_pct + step * uses;
    if (pct > cap_pct) {
        pct = cap_pct;
    }
    if (g_core && g_core->deal_pink_damage) {
        ElfPet& me = ctx->getPet(owner);
        g_core->deal_pink_damage(ctx, target, me.hp * pct / 100, DamageKind::PERCENT_VALUE,
                                 owner);
    }
    return EffectResult::kOk;
}

// ── 1281 —— 消耗所有护盾并造成等量固伤；>300 追加 max/3 百分比；≤300>0 回 1/3 + 等量百分比 ──
EffectResult effect_1281_explode_shield(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    ElfPet& me = ctx->getPet(owner);
    const int total = me.shield_bank_.total();
    if (total <= 0 || !g_core->consume_shield) {
        return EffectResult::kOk;
    }
    int broken = 0;
    g_core->consume_shield(ctx, owner, total, &broken);   // "视为被击破"→破盾事件喂魂印①
    if (g_core->fixed_damage) {
        g_core->fixed_damage(ctx, target, total);
    }
    const int max_hp = diwang_max_hp(me);
    if (total > 300) {
        if (g_core->deal_pink_damage) {
            g_core->deal_pink_damage(ctx, target, max_hp / 3, DamageKind::PERCENT_VALUE, owner);
        }
    } else if (total > 0) {
        if (g_core->heal_amount) {
            g_core->heal_amount(ctx, owner, max_hp / 3);
        }
        if (g_core->deal_pink_damage) {
            g_core->deal_pink_damage(ctx, target, max_hp / 3, DamageKind::PERCENT_VALUE, owner);
        }
    }
    return EffectResult::kOk;
}

// ── 1626 —— 后出手时把当回合护盾承受的伤害按{0}%以百分比伤害反弹（伤害落地后）──
EffectResult effect_1626_reflect_absorbed(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int pct = args.int_args[2];
    if (!ctx->is_second_mover(owner)) {
        return EffectResult::kOk;   // 后出手限定
    }
    // 当回合护盾承受量：护盾吸收不直接记账——用"本次攻击 final 前后差"不可得，
    // 以 resolvedDamage 的 raw 与落血差近似（受盾完全挡下时 raw>0 落血=0 的差额即吸收量）。
    const int absorbed = ctx->ws.raw_attack_damage[1 - owner] > 0
                             ? ctx->ws.raw_attack_damage[1 - owner]
                             : 0;
    (void)absorbed;
    // ⚠️ 精确的"护盾吸收台账"待内核补（交接文档待办）；当前实现以攻击方裸伤为基数近似。
    if (g_core && g_core->deal_pink_damage) {
        const int base = ctx->resolvedDamage.final > 0 ? ctx->resolvedDamage.final : 0;
        g_core->deal_pink_damage(ctx, target, base * pct / 100, DamageKind::PERCENT_VALUE, owner);
    }
    return EffectResult::kOk;
}

// ── 1752 —— 获得护盾；消失时吸取对手{1}点体力 + 随机{2}个技能 PP 归零 ──
EffectResult effect_1752_shield_drain_deathrattle(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int shield = args.int_args[2];
    const int drain = args.int_args[3];
    const int slots = args.int_args[4];
    if (shield <= 0) {
        return EffectResult::kOk;
    }
    const int source = 1752000 + owner;
    ctx->getPet(owner).shield_bank_.add(/*priority=*/0, shield, source, /*duration=*/0);
    ctx->event_center_.register_watcher(
        EventType::EVENT_SHIELD_BROKEN, owner, ctx->roundCount, 0, /*once=*/false,
        [owner, drain, slots, source](BattleContext* wctx, const BattleEvent& ev) {
            (void)ev;
            if (!wctx || !g_core || !g_core->drain_hp || !g_core->pp_zero_slot) {
                return;
            }
            ElfPet& me = wctx->getPet(owner);
            if (me.shield_bank_.quantity_of(source) > 0 && me.shield_bank_.total() > 0) {
                return;   // 还有盾在（含这笔没破）——粗守卫，见交接文档
            }
            ElfPet& tgt = wctx->getPet(1 - owner);
            const int before = tgt.hp;
            g_core->fixed_damage(wctx, 1 - owner, drain);
            const int dealt = before - tgt.hp;
            if (dealt > 0 && g_core->heal_amount) {
                g_core->heal_amount(wctx, owner, dealt);
            }
            // 随机 {slots} 个技能 PP 归零（打乱槽序取前 slots 个）
            int idx[5] = {0, 1, 2, 3, 4};
            for (int i = 4; i > 0; --i) {
                const int j = choose_skill_range(*wctx, 1752, 0, owner, 1 - owner, 0, i);
                const int t = idx[i];
                idx[i] = idx[j];
                idx[j] = t;
            }
            for (int i = 0; i < slots && i < 5; ++i) {
                g_core->pp_zero_slot(wctx, 1 - owner, idx[i], /*actor=*/owner);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 1803 —— 技能无效时恢复/封属（SKILL_INVALID 路径：命中失效时技能效果桶不跑，
// 走事件 watcher——K 阶段 EVENT_SKILL_INVALID 已有）──
EffectResult effect_1803_on_invalid(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    auto& storage = ctx->getPet(owner).soulmark_storage;
    const int key = 1803000 + owner;
    if (storage.count(key)) {
        return EffectResult::kOk;
    }
    storage[key] = true;
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_INVALID, owner, ctx->roundCount, 0, /*once=*/false,
        [owner](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || ev.actor != owner || !is_diwang_onstage_pub(wctx, owner)) {
                return;
            }
            if (!g_core || !g_core->heal_amount || !g_core->seal_skill) {
                return;
            }
            ElfPet& me = wctx->getPet(owner);
            const int max_hp = diwang_max_hp(me);
            int heal = max_hp / 3;
            int rounds = 2;
            if (me.hp < max_hp / 2) {
                heal *= 2;          // 低于 1/2：恢复翻倍
            } else if (me.hp > max_hp / 2) {
                rounds += 1;        // 高于 1/2：失效回合 +1
            }
            g_core->heal_amount(wctx, owner, heal);
            g_core->seal_skill(wctx, owner, 1 - owner, /*effect_id=*/1803,
                               /*attribute=*/true, /*attack=*/false, /*count=*/0, rounds,
                               /*penetrable=*/false, /*source_slot=*/wctx->on_stage[owner],
                               EffectScope::ON_STAGE, /*hit_invalid=*/true,
                               /*chance_pct=*/100, /*consumed_when_pierced=*/false);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 2318 —— 消耗全部护盾 → 消耗量 100% 百分比伤害，每 400 点额外 +40（视为被击破）──
EffectResult effect_2318_explode_shield(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    ElfPet& me = ctx->getPet(owner);
    const int total = me.shield_bank_.total();
    if (total <= 0 || !g_core->consume_shield || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    int broken = 0;
    g_core->consume_shield(ctx, owner, total, &broken);
    const int bonus = total / 400 * 40;
    g_core->deal_pink_damage(ctx, target, total + bonus, DamageKind::PERCENT_VALUE, owner);
    return EffectResult::kOk;
}

// ── 2319 —— 第五：双防和{0}%百分比伤害；每蛊+{1}%；对手未受百分比伤害→+1嶂、每嶂种1蛊 ──
EffectResult effect_2319_fifth(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int pct = args.int_args[2];
    const int per_gu = args.int_args[3];
    ElfPet& me = ctx->getPet(owner);
    const int def_sum = me.numericalBase[NumericalPropertyIndex::DEFENSE]
                      + me.numericalBase[NumericalPropertyIndex::SPECIAL_DEFENSE];
    const int opp = 1 - owner;
    const int gu = diwang_pub_get(ctx->plugin_storage, kDiwangPubGu + opp);
    int amount = def_sum * pct / 100 + gu * per_gu;
    if (g_core->deal_pink_damage) {
        g_core->deal_pink_damage(ctx, target, amount, DamageKind::PERCENT_VALUE, owner);
    }
    // "对手未受到百分比伤害时"：本次之后仍未受 → +1 嶂；每层嶂种 1 蛊。
    if (ctx->resolvedPink.final <= 0) {
        add_zhang_pub(ctx, owner, 1);
        const int layers = diwang_pub_get(ctx->plugin_storage, kDiwangPubZhang + owner);
        for (int i = 0; i < layers; ++i) {
            const int cur = diwang_pub_get(ctx->plugin_storage, kDiwangPubGu + opp);
            if (cur < 4) {
                diwang_pub_set(ctx->plugin_storage, kDiwangPubGu + opp, cur + 1);
            }
        }
    }
    return EffectResult::kOk;
}

// ── 2320 窗口载荷：owner 行动后弱化尝试；未触发（免弱挡/已 -6 钳底）→ 补偿攻击无效 ──
// 口径（用户 2026-09-26 拍板；2026-10-25 按官方实测修订）：
//   · "未触发" = 弱化未落地 = 原语返回 **AT_FLOOR**（已 -6 钳底）或 **IMMUNE**（免弱挡）
//     ——2026-10-25 官战实测修订（灼眼焰华 2161 同族"未触发则补偿"，补偿以状态栏图标
//     呈现，用户实测对手 -6 也触发补偿）：AT_FLOOR 不再算触发；
//   · SUCCESS（含压到 -6 为止的部分生效）= 触发，不补偿；
//   · INVALID_PARAM（stat 越界等装配问题）仍不补；
//   · "每回合使用技能"是补偿的前置触发条件——没用技（嗑药/换宠/被控跳过）连弱化
//     都不发起，补偿自然不会触发。
// 弱化时点近似：官方"使用技能时"，实现为行动后桶统一结算（免弱票均为常驻，无回合内
// 时效场景；回合末判定对落地结果等价），记偏差。
EffectResult effect_2320_after_action(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int stat = args.int_args[2];
    const int invalid_rounds = args.int_args[3];
    // 只认"使用技能"的行动（1=SELECT_SKILL；嗑药/换宠不触发弱化）。
    if (ctx->roundChoice[owner][0] != 1 /*SELECT_SKILL*/) {
        return EffectResult::kOk;
    }
    if (!g_core->stat_drop) {
        return EffectResult::kOk;
    }
    // 未落地（AT_FLOOR 钳底 / IMMUNE 免弱挡）补偿；SUCCESS=触发；
    // INVALID_PARAM（stat 越界等装配问题）不应发生于正常切片，同样不补。
    const StatDropResult r = g_core->stat_drop(ctx, target, stat, /*amount=*/1, owner);
    if (r == StatDropResult::SUCCESS || r == StatDropResult::INVALID_PARAM) {
        return EffectResult::kOk;
    }
    // 补偿：对手下 {7} 回合攻击技无效。⚠️ SEAL 票是回合边界 tick 递减模型（tick_rounds
    // 每回合 -1、归零删除）——"下 N 回合"必须传 N+1 抵消授票当回合的那次 tick，
    // 否则票活不过当回合末、下回合查询时已不存在（2026-09-26 用户实测发现）。
    // 已知近似：授票当回合的剩余时段（后手方行动）也在封禁内，待实测翻案则去 +1。
    if (g_core->seal_skill) {
        g_core->seal_skill(ctx, owner, target, /*effect_id=*/2320, /*attribute=*/false,
                           /*attack=*/true, /*count=*/0, invalid_rounds + 1,
                           /*penetrable=*/false, /*source_slot=*/ctx->on_stage[owner],
                           EffectScope::ON_STAGE, /*hit_invalid=*/false, /*chance_pct=*/100,
                           /*consumed_when_pierced=*/false);
    }
    return EffectResult::kOk;
}

// ── 2320 —— 弹控：{0}回合内每回合使用技能令对手{1}，未触发→对手下{7}回合攻击无效 ──
// ⚠️ 参数为 8 参 union 布局（全库审计 2026-09-26：{0}/{1}/{7} 跳号是官方模板族设计，
//    2161 同构互证；{2}~{6} 为 0/-1 填充）。{1}=弱化的能力槽下标（0=物攻，走能力下降
//    通道查免弱 STAT_DROP——谱尼圣洁可挡，idx=190"弱化"+4399"附加能力下降状态"）。
EffectResult effect_2320_pill(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 10) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int rounds = args.int_args[2];          // {0}
    const int stat = args.int_args[3];            // {1}
    const int invalid_rounds = args.int_args[9];  // {7}
    if (owner < 0 || owner > 1 || target < 0 || target > 1
        || rounds <= 0 || invalid_rounds <= 0 || stat < 0
        || stat >= BattleContext::kAbilityLevelSlotCount) {
        return EffectResult::kOk;
    }
    // 窗口条目挂 owner 侧 FIRST/SECOND 两个 AFTER_ACTION 桶（桶只在 mover 侧执行 →
    // 回调天然只在 owner 行动后触发；被控跳过时整桶跳过，天然不算"使用技能"）。
    // 施放当回合即首回合（施放本身就是用技 → 当回合 AFTER_ACTION 完成首次弱化尝试）。
    const std::vector<int> payload{owner, target, stat, invalid_rounds};
    register_round_effect(ctx, State::BATTLE_FIRST_AFTER_ACTION, owner, 2320, rounds,
                          payload, &effect_2320_after_action);
    register_round_effect(ctx, State::BATTLE_SECOND_AFTER_ACTION, owner, 2320, rounds,
                          payload, &effect_2320_after_action);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(605, effect_605_hp_compare_pink);
SKILL_EFFECT(1052, effect_1052_clear_then_buff);
SKILL_EFFECT(1087, effect_1087_reverse_then_invalid);
SKILL_EFFECT(1128, effect_1128_defeat_seal);
SKILL_EFFECT(1280, effect_1280_scaling_current_hp);
SKILL_EFFECT(1281, effect_1281_explode_shield);
SKILL_EFFECT(1626, effect_1626_reflect_absorbed);
SKILL_EFFECT(1752, effect_1752_shield_drain_deathrattle);
SKILL_EFFECT(1803, effect_1803_on_invalid);
SKILL_EFFECT(2318, effect_2318_explode_shield);
SKILL_EFFECT(2319, effect_2319_fifth);
SKILL_EFFECT(2320, effect_2320_pill);
