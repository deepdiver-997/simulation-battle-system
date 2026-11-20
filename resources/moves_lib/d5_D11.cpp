// D11 · 混沌魔尊 3683 双第五技「魔使碎灭令 32396 / 混沌君临诏 38526」缺失效果
// 1038、2561、2352、2496（夜跑 night-2，2026-09-28）
//
// 32396 链 = [1038, 1048, 1257]、args [300, 100, 3]：1038=[300]（本文件），
//   1048=[100]（effect_batch8.cpp 已接）、1257=[3]（非本单）。
// 38526 链 = [2561, 1038, 2352, 1257, 2496]、args [400, 100, 3, 100]：
//   2561=[]（本文件·CARRY）、1038=[400]、2352=[100]、1257=[3]、2496=[]（尾部 100 无主——
//   装载切片按 args_num 顺切，2496 args_num=0 不消费，多出位自然落空，记档）。
// 4399 图鉴（混沌君临诏）："必中；战斗开始时剥离自身的混沌系并转化为堕灵神谕，之后本场
//   战斗自身每次登场时若对手不处于控制类异常状态则令对手下1次属性技能无效；双倍反转自身
//   能力下降状态，反转成功则获得护盾；造成的攻击伤害提升100%，对手处于异常状态时效果翻倍；
//   对手不处于异常状态则吸取对手最大体力1/3；造成的攻击伤害为0时，若自身体力低于对手则
//   与对手交换体力"。
//
// 1038 "双倍反转自身能力下降状态，反转成功则获得{0}点护盾"：
//   - stat_reversal 原语是 1:1 无倍率通道 → 双倍语义逐槽 stat_change(+2×|下降|)
//     （-N→+2N，走引擎等级漏斗）；无下降槽 = 反转失败 → 不给盾；
//   - 盾 = shield_bank_.add（mistike/1754 同款；{0}=盾值，两技切片 300/400）。
// 2352 "造成的攻击伤害提升{0}%，对手处于异常状态时效果翻倍"：
//   - 通用增伤加法通道 ws.damage_add_pct（"提升X%"无"额外"字样，1178/1959 同判）；
//   - 翻倍 = 计入前 pct×2（+100% → +200%）；异常判定 has_any_active_abnormal_status
//     （1048 同款 inline）。
// 2496 "造成的攻击伤害为0时，若自身体力低于对手则与对手交换体力"：
//   - "伤害为0"须在攻击结算后判 → AFTER_ACTION 桥（582 同款，FIRST/SECOND 双侧 rounds=1）；
//     判据 = ws.resolvedDamage.final == 0（管线后最终伤害，1356 桥读同字段——盾吸满/
//     减到 0 都算；miss/被无效的技能不进 SKILL_EFFECT → 桥不挂，天然不触发）；
//   - 交换 = **绝对体力值**互换（4399 字面"交换体力"；灼烬 4696 用百分比互换是其文本
//     明写"体力值百分比"——两处口径不同，本条按字面），各自钳到自身最大体力；
//     换到 0 走 defeat_pet 死亡漏斗（zhiyan_swap_hp 同款）。
// 2561 "战斗开始时，剥离自身的混沌系并转化为堕灵神谕，之后本场战斗自身每次登场时若对手
//   不处于控制类异常状态则令对手下1次属性技能无效"（CARRY_EFFECT 携带通道，战斗开始扫描）：
//   - 剥离混沌系：elementalAttributes 中 222（混沌）分量置空——堕灵神谕无系别数据
//     （skill_types 全表无此系），机械效果 = 失去混沌分量；"转化为堕灵神谕"本体记档待拍板；
//   - 登场监听：EVENT_ENTER_STAGE（perform_switch 统一漏斗，战斗开始布阵不发 →
//     "之后每次登场"= 中途登场，含主动中切与死亡换宠）；对手控制类异常判定 =
//     abnormal_status_end_round + is_control_abnormal_status（空元 2087 同款内联）；
//   - 封技 = seal_skill 属性单类、count=1（976/1056 次数型同款，可被 699 穿）。
//
// 场景回归：test/scenario/scenario_270_hundunmozun_3683.cpp。

#include "common.h"

namespace {

constexpr int kChaosComponent = 222;            // skill_types: 222 = 混沌（单组件叶子）
constexpr int k2561WatcherKeyBase = 990125610;  // 登场监听已挂旗标（owner 侧）

// 目标是否处于控制类异常（kongyuan_zhishi 的 target_has_control_anomaly 同款内联；
// 自然过期不回写槽 → 按 roundCount < end 判"还在"）。
bool target_has_control_anomaly(BattleContext* ctx, int target) {
    if (!ctx || target < 0 || target > 1) {
        return false;
    }
    const auto& ends = ctx->abnormal_status_end_round[target];
    for (std::size_t id = 0; id < ends.size(); ++id) {
        if (ctx->roundCount < ends[id]
            && is_control_abnormal_status(static_cast<AbnormalStatusId>(id))) {
            return true;
        }
    }
    return false;
}

// ── 1038 —— 双倍反转自身能力下降，反转成功则获得{0}点护盾 ──
EffectResult effect_1038_double_reversal_shield(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int shield_value = args.int_args[2];
    if (owner < 0 || owner > 1 || shield_value <= 0) {
        return EffectResult::kOk;
    }
    bool reversed = false;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        const int lv = ctx->ability_levels[owner][i];
        if (lv >= 0) {
            continue;
        }
        reversed = true;
        // 双倍反转：目标等级 = +2N（N=|lv|，钳到上限 6），delta = 目标 - 现值 = 3N。
        const int target_lv = std::min(2 * (-lv), 6);
        g_core->stat_change(ctx, owner, i, target_lv - lv);
    }
    if (!reversed) {
        return EffectResult::kOk;   // 反转失败（无下降）：不给盾
    }
    ctx->getPet(owner).shield_bank_.add(/*priority=*/0, shield_value, /*source_id=*/1038,
                                        /*duration=*/0);
    return EffectResult::kOk;
}

// ── 2352 —— 造成的攻击伤害提升{0}%，对手处于异常状态时效果翻倍 ──
EffectResult effect_2352_amp_anomaly_double(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int pct = args.int_args[2];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || pct <= 0) {
        return EffectResult::kOk;
    }
    int amp = pct;
    if (ctx->has_any_active_abnormal_status(target)) {
        amp *= 2;   // 对手处于异常状态：效果翻倍
    }
    ctx->ws.damage_add_pct[owner] += static_cast<float>(amp);
    return EffectResult::kOk;
}

// ── 2496 的 AFTER_ACTION 桥：伤害为 0 且体力低于对手 → 绝对值换血 ──
EffectResult effect_2496_swap_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = 1 - owner;
    if (owner < 0 || owner > 1 || ctx->ws.resolvedDamage.final != 0) {
        return EffectResult::kOk;   // 本击造成了攻击伤害 → 不换
    }
    ElfPet& self = ctx->getPet(owner);
    ElfPet& foe = ctx->getPet(opp);
    if (self.hp >= foe.hp) {
        return EffectResult::kOk;   // "自身体力低于对手"
    }
    const long long self_max = std::max<long long>(
        1, self.numericalBase[NumericalPropertyIndex::HP]);
    const long long foe_max = std::max<long long>(
        1, foe.numericalBase[NumericalPropertyIndex::HP]);
    // 绝对值互换，各自钳到自身最大体力（两个新值都先算后写）。
    const long long new_self = std::min<long long>(foe.hp, self_max);
    const long long new_foe = std::min<long long>(self.hp, foe_max);
    self.hp = static_cast<int>(std::max<long long>(new_self, 0));
    foe.hp = static_cast<int>(std::max<long long>(new_foe, 0));
    // 换到 0 血 → 走死亡漏斗（EFFECT 归因；zhiyan_swap_hp 同款）。
    if (g_core && g_core->defeat_pet) {
        if (self.hp <= 0) {
            self.hp = 0;
            g_core->defeat_pet(ctx, owner, ctx->on_stage[owner], /*actor=*/opp,
                               DefeatCause::EFFECT);
        }
        if (foe.hp <= 0) {
            foe.hp = 0;
            g_core->defeat_pet(ctx, opp, ctx->on_stage[opp], /*actor=*/owner,
                               DefeatCause::EFFECT);
        }
    }
    return EffectResult::kOk;
}

// ── 2496 —— 造成的攻击伤害为0时，若自身体力低于对手则与对手交换体力 ──
EffectResult effect_2496_zero_damage_swap(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    // AFTER_ACTION 桥（582 同款）："伤害为0"在攻击结算后才可知。FIRST/SECOND 双侧都挂，
    // 桶按 (时点, 行动方) 执行，非行动方一侧自然空跑；rounds=1 只活在用技当回合。
    for (State st : {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_round_effect(ctx, st, owner, 2496, 1, {owner}, &effect_2496_swap_tick);
    }
    return EffectResult::kOk;
}

// ── 2561（CARRY，战斗开始扫描）：剥离混沌系 + 登场封属性技 ──
void carry_2561_strip_and_entry_seal(BattleContext* ctx, int owner, int skill_slot,
                                     int effect_id) {
    (void)skill_slot;
    (void)effect_id;
    if (!ctx || owner < 0 || owner > 1) {
        return;
    }
    // ① 剥离混沌系（堕灵神谕无系别数据 → 该分量置空，记档待拍板）。
    ElfPet& self = ctx->getPet(owner);
    for (std::size_t i = 0; i < self.elementalAttributes.size() && i < 2; ++i) {
        if (self.elementalAttributes[i] == kChaosComponent) {
            self.elementalAttributes[i] = 0;
        }
    }
    // ② 登场监听（本场只挂一次；duration_rounds=0 不随回合过期）。
    const int key = k2561WatcherKeyBase + owner;
    if (ctx->plugin_storage.count(key)) {
        return;
    }
    ctx->plugin_storage[key] = 1;
    // scope=TEAM：监视器要活过自身的下场/回归（ON_STAGE 作用域在携带方切换时即作废）。
    ctx->event_center_.register_watcher(
        EventType::EVENT_ENTER_STAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.actor != owner) {
                return;   // 只认自己登场
            }
            const int opp = 1 - owner;
            if (target_has_control_anomaly(c, opp)) {
                return;   // "对手不处于控制类异常状态"
            }
            if (g_core && g_core->seal_skill) {
                g_core->seal_skill(c, /*source=*/owner, /*target=*/opp, /*effect_id=*/2561,
                                   /*attribute=*/true, /*attack=*/false, /*count=*/1,
                                   /*duration_rounds=*/0, /*penetrable=*/true,
                                   /*source_slot=*/-1, EffectScope::ON_STAGE,
                                   /*hit_invalid=*/false, /*chance_pct=*/100,
                                   /*consumed_when_pierced=*/false);
            }
        },
        WatcherScope::TEAM);
}

}  // namespace

SKILL_EFFECT(1038, effect_1038_double_reversal_shield);
SKILL_EFFECT(2352, effect_2352_amp_anomaly_double);
SKILL_EFFECT(2496, effect_2496_zero_damage_swap);
CARRY_EFFECT(2561, carry_2561_strip_and_entry_seal);
