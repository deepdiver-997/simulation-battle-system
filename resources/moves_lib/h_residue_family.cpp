// H 组 H4 · 高低伤/残留族 —— 506×26 / 508×26 / 135×22 / 545×21（2026-09-30，worktree ../agents/h4）
//
// 家族语义（effect_info 模板即权威；4399 双源逐字验证）：
//   506（args_num=1）"下回合受到致命伤害时残留{0}点体力"
//     —— 灵龙息 17351 [1]="下回合受到致命伤害时残留1点体力"。死亡拦截器
//     （DeathInterceptor，68 直系范式 f_onhit_family.cpp）：sees_hp_consume=false
//     残留体力免死档 + DefeatCause::DAMAGE 过滤；窗口 = **NextRounds 单回合**
//     （start=roundCount+1、expire=start+1——官方"下回合"明确不含当回合，与
//     68"{0}回合内"InRounds 含当回合的口径差一字半句，取字面并记档；后手当回合
//     被打死不保，官方文本字面口径）。全库 26 载体 {0} 均为 1。
//   508（args_num=1）"减少{0}点下回合所受的伤害"
//     —— 流石护体 14872 [100]="减少100点下回合所受到的伤害"。811 直系同款
//     （f_mitigate_cap.cpp）：一次性 REDUCE_FLAT（MITIGATE 类别——不可被挡伤失效
//     无视），金额固定 {0}（811 的 n=本回合伤害累计，本条是定点数，无需 watcher）；
//     "下次"= 一次性别忘消费（工单口径"508=MITIGATE 一次性（811 同款）"；官方
//     "下回合"字样与 811"下次"同族泛指，消费无回合窗——记档）；打盔管线不跑
//     天然不消费（811 同款官方口径）。
//   135（args_num=1）"造成的伤害不会低于{0}"
//     —— 光箭攻击 12546 [80]="造成的伤害不会低于80"。ws.damage_floor[owner]
//     FLOOR 阶段槽直写（447 族生产者先例，battleWorkspace.h:279——"攻击技能的
//     效果体在 SKILL_EFFECT 时点赋值，覆盖语义；管线跑完即无人再读；打盔/miss
//     不跑管线天然不触发"）。FLOOR 在 CAP 之前（damage_pipeline.h kOrder）。
//   545（args_num=3）"{0}回合内若受到伤害高于{1}则对手{2}"
//     —— 铁壁之护 22920 [5,200,7]="5回合内若受到伤害高于200则对手疲惫"、
//     光之屏障 23139 [5,200,2]=烧伤、灭天神火弹 16682 [2,240,2]="2回合内受到
//     伤害超过240，对手烧伤"——{2}=官方异常码（0麻痹…2烧伤…7疲惫，速查 §三）。
//     受击侧监听 = EVENT_TAKE_DAMAGE watcher（duration={0}——register 起算
//     current-register>=duration 撤，恰好 [R, R+{0}-1] 含当回合 InRounds 口径）；
//     每次受伤判一次（窗口内反复触发，原语幂等由异常系统处理）；判据 = 实际扣血
//     ev.amount > {1}（红/粉/真/属性四条 emit 路径都发，811 头注同源，取宽记档）；
//     异常施加 = apply_anomaly_checked（chance=100 申报，1106 同管道——模板无
//     概率字样，申报制与闸门一致性照 1106 先例）。
//
// 动工前置三查（2026-09-30）：①内核 grep 506/508/135/545 无硬编码（135 命中均为
// 1351/1355/1356 等 id 噪声）②custom_effect_seed.sql 零命中 ③moves_lib 无既有注册
// （SKILL_EFFECT(135 命中 1356 是别的 id）——真缺口。范式先例：68（DeathInterceptor）、
// 811（一次性 REDUCE_FLAT）、447（damage_floor，内核注释锚点）、1106（AFTER_ACTION
// 桥+apply_anomaly_checked——545 因判据在"受伤时"可当场知悉，直接 watcher 不需桥）。
//
// 场景回归：test/scenario/scenario_820_residue_family.cpp（820-829 号段，一文件覆盖
// 四效果六载体）。

#include "common.h"

#include <memory>

#include <primitives/battle_primitives.h>

namespace {

// ── 506 —— 下回合受到致命伤害时残留{0}点体力（死亡拦截器，68 直系）──
// int_args = [owner, target, {0}=残留点数]。
EffectResult effect_506_lethal_residue_next_round(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 506, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 506, args,
                     "506 布局=[owner,target,残留点数]（int_count>=3）");
    const int self = args.int_args[0];
    const int residue = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 506, args, "owner 越界");
    EFFECT_ARG_GUARD(residue > 0, 506, args, "需要 残留点数 > 0");
    if (!g_core || !g_core->register_death_interceptor) {
        return EffectResult::kOk;
    }
    // 窗口 = NextRounds 单回合：本回合不算，恰覆盖 roundCount+1（官方"下回合"字面）。
    const int start = ctx->roundCount + 1;
    const int expire = start + 1;
    const int slot = ctx->on_stage[self];   // 换宠即失保护（68 同款武装时快照）
    DeathInterceptor ic;
    ic.source_effect_id = 506;
    ic.owner = self;
    ic.sees_hp_consume = false;   // 残留体力免死档：消耗体力（自爆族）穿免死，不拦
    ic.fn = [self, slot, start, expire, residue](BattleContext* c, int side, int die_slot,
                                                 int actor, DefeatCause cause) -> bool {
        (void)actor;   // 不限来源（68 同款：反伤致死也保）
        if (!c || side != self || die_slot != slot) {
            return false;
        }
        if (cause != DefeatCause::DAMAGE) {
            return false;   // 只拦伤害致死（秒杀/消耗体力/直写归零不在本档）
        }
        if (c->roundCount != start) {
            return false;   // 窗口=恰"下回合"一回合（其余回合自然哑火）
        }
        c->seerRobot[side].elfPets[slot].hp = residue;   // "残留{0}点体力"（契约：写回 >0）
        return true;
    };
    g_core->register_death_interceptor(ctx, ic);
    return EffectResult::kOk;
}

// ── 508 —— 减少{0}点下回合所受的伤害（一次性 REDUCE_FLAT，811 直系定点版）──
// int_args = [owner, target, {0}=减免点数]。
EffectResult effect_508_next_hit_flat_reduce(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 508, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 508, args,
                     "508 布局=[owner,target,减免点数]（int_count>=3）");
    const int self = args.int_args[0];
    const int amount = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 508, args, "owner 越界");
    EFFECT_ARG_GUARD(amount > 0, 508, args, "需要 减免点数 > 0");
    // "下次"=一次性别忘消费（811 同款结构；n>0 固定，无累计 watcher）。
    auto consumed = std::make_shared<bool>(false);
    ctx->register_damage_effect(
        DamagePhase::REDUCE_FLAT, self, DamageEffectCategory::MITIGATE,
        [self, amount, consumed](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != self || *consumed) {
                return;
            }
            DamageSnapshot& d = c->resolvedDamage;
            if (d.defenderId != self || !d.isRed || d.final <= 0) {
                return;
            }
            *consumed = true;   // "下次"=一次性
            d.final = std::max(0, d.final - amount);   // 按点数扣减（下限 0）
        });
    return EffectResult::kOk;
}

// ── 135 —— 造成的伤害不会低于{0}（FLOOR 阶段槽直写，447 族生产者）──
// int_args = [owner, target, {0}=下限点数]。
EffectResult effect_135_damage_floor(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 135, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 135, args,
                     "135 布局=[owner,target,下限点数]（int_count>=3）");
    const int owner = args.int_args[0];
    const int floor_value = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 135, args, "owner 越界");
    EFFECT_ARG_GUARD(floor_value > 0, 135, args, "需要 下限点数 > 0");
    ctx->ws.damage_floor[owner] = floor_value;   // 覆盖语义（battleWorkspace.h:280）
    return EffectResult::kOk;
}

// ── 545 —— {0}回合内若受到伤害高于{1}则对手{2}（受击侧 watcher 窗口）──
// int_args = [owner, target, {0}=回合数, {1}=伤害阈值, {2}=官方异常码]。
EffectResult effect_545_high_damage_anomaly_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 545, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 545, args,
                     "545 布局=[owner,target,回合数,阈值,官方异常码]（int_count>=5）");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int threshold = args.int_args[3];
    const int anomaly = args.int_args[4];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && opp != self, 545, args,
                     "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && threshold > 0, 545, args, "需要 回合数>0 且 阈值>0");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 545, args, "官方异常码越界");
    if (!g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    // duration={0}：register 起算 current-register>=duration 撤 → 恰 [R, R+{0}-1]
    // 含当回合（InRounds 口径与 766/850 一致）。
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, self, ctx->roundCount, rounds, /*once=*/false,
        [self, opp, threshold, anomaly](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != self || ev.amount <= threshold) {
                return;   // 只认自己挨的打且高于阈值（每次受伤各判一次）
            }
            if (!g_core || !g_core->apply_anomaly_checked) {
                return;
            }
            (void)g_core->apply_anomaly_checked(c, opp, anomaly, random_anomaly_duration(),
                                                self, /*chance_pct=*/100, ChanceSource::Skill);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(506, effect_506_lethal_residue_next_round);
SKILL_EFFECT(508, effect_508_next_hit_flat_reduce);
SKILL_EFFECT(135, effect_135_damage_floor);
SKILL_EFFECT(545, effect_545_high_damage_anomaly_window);
