// G 组批产 · G5 延迟固伤族 —— 571×29 / 611×28 / 192×28（2026-09-29，worktree ../agents/g5）
//
// 家族语义："伤害**晚一步到账**"——571 是"N 回合后定时引爆"、611 是"未来 N 回合内
// 打人时捎带一笔"、192 是"当次攻击按当前体力折算附加"。三模板全部走 deal_pink_damage
// 现代粉伤通道（FIXED 点数档 / PERCENT_VALUE 折算档），不直写 hp。
//
// 571（args_num=2）"{0}回合后对对手造成{1}点固定伤害 重复使用无法叠加"：
//   - 载体：劫数·万物哀鸣 16869（[143, 571]|[1, 300]，咤克斯第五技）、不祥之兆 23050
//     （[5, 571]|[5, 100, -2, 2, 300]，奈法里安——5 号切片 [5,100,-2]="100% 改变对手
//     命中(槽5)等级-2"、571 切片 [2,300]）。全库 29 载体。
//   - 4399 原文：咤克斯"反转对手能力提升效果；**1回合后**使对手受到300点固定伤害
//     **无法叠加**"、奈法里安"使对手命中-2,**2回合后**对对手造成300点固定伤害,无法叠加"
//     ——两载体切片与文本一一对应（N=1/N=2 各一），语义 = 施放后**过了 N 个完整回合**
//     才引爆一次。引爆时点 = **回合扣减点**（BATTLE_ROUND_REDUCTION_ALL_ROUND_MINUS，
//     F7 76 的 dot 同点——官方"每回合结束后"结算位，在 cleanup 之前跑）：
//     R 回合施放（roundCount=R-1）→ 第 R+N 个日历回合的收尾点引爆，此时 roundCount
//     == (R-1)+N，tick 以 detonate_round=cast_roundCount+N 精确闸门（窗口内其余回合的
//     扣减点静默空跑）。
//   - 窗口 = **NextRounds**（register_next_rounds_effect，F8 1004 同款显式声明——
//     custom_effect_overrides 未给 571 声明 window 家族，默认 InRounds 会把施放回合的
//     收尾点也算进窗口，"N回合**后**"语义错误）：duration=N 仅作窗口寿命，真正的
//     引爆闸门是 args 里的 detonate_round（InRounds/NextRounds 的过期计法在"施放回合
//     收尾点是否执行"上差一拍，闸门值不受影响、场景 750 T1/T2 双向钉死）。
//   - "重复使用无法叠加"（⚠️ 与工单提示的 plugin_storage 计时键有偏差，按引擎语义落并
//     记档）：TimedBucket::register_effect 的**同源去重**（timed_effect_key =
//     (source_id<<32)|effect_id，per trigger×owner）天然给出"同侧至多一条未引爆计时，
//     重复注册**覆盖刷新**"——比 plugin_storage 手工键少一份要自己跟生命周期的状态，
//     且断回合/离场作废自动同步。4399 两载体均只说"无法叠加"、从未说"保持旧计时"，
//     取覆盖刷新语义（刷新 vs 忽略的分歧点由场景 750 T3 钉死：R3 末不引爆、R4 末引爆
//     恰一次 300）。两载体不同宠同侧复用 = 同 key 覆盖，同样不叠加。
//   - 持有者死亡/离场（⚠️ 记档：官方无文本）：窗口走 ContinuousEffect 默认 **ON_STAGE**
//     作用域——换宠/阵亡补位时 perform_switch → invalidate_on_stage_effects 递增 epoch，
//     引爆窗口随之下场失效（"离场即失"保守取态）。选 ON_STAGE 而非 TEAM 的理由：
//     ① moves_lib 全部回合窗（915/979/1219、F7 76 记档）同口径；② 该炸弹是**施放者
//     施加的威胁**，施放者离场威胁取消与"窗口类效果挂宠"的引擎统一行为一致；
//     ③ 断回合（消回合类效果）同口径可清——ON_STAGE 回合类条目在断回合面上无豁免。
//   - 实例：16869 [1,300]、23050 [2,300]。
//
// 611（args_num=2）"下{0}回合自身使用攻击技能则附加{1}点固定伤害"：
//   - 载体：战将之气 23640（[611, 408]|[2, 100, 2, 100]，四九战将/四九战王/四九圣尊/
//     萨尔蒙系共用）、星辰剑意 23655（[585, 611]|[1×6, 2, 100]，绝影·孤星）。全库 28 载体。
//   - 4399 原文：四九战将"下两回合附加100伤害；下两回合受到伤害减100"、灵兽·萨尔蒙
//     "必中，**下2回合自身直接攻击技能附加100点固定伤害**，2回合内每回合所受的伤害减少
//     100"、绝影·孤星"全属性+1；**下2回合自身直接攻击技能附加100固伤**"——"下{0}回合"
//     = NextRounds（本回合不算，先/后手都从 R+1 起）。
//   - 消费 = 每回合 ROUND_START tick 武装一条**当回合**的 EVENT_HIT watcher（duration=1，
//     F8 1004 同款；WatcherScope::TEAM——跨回合 watcher 必须 TEAM，常识速查 §五），
//     回调过滤 ev.actor==self（对手出手不算）+ **攻击技能限定**：common.h 的
//     selected_skill_is_attack（481/681 攻击限定同款判据，读 roundChoice 里本回合
//     选中技能、非 Attribute 即攻击）——EVENT_HIT 对属性技命中也发（1004 头记档），
//     不加过滤会把战将之气这类属性技自己算进去。附加 = deal_pink_damage(FIXED, {1})，
//     每次命中各附一笔（多段攻多笔——1004 家族口径）。
//   - 窗口挂宠：武装时快照 on_stage 槽位，各回合 tick 复核（槽位不符不武装，换回则续）
//     ——1004/68 同思路；换宠当回合新宠出手不附加（工单 611 行"NextRounds+F6 817
//     受击发放同构"的挂宠忠实度口径）。
//   - 实例：23640 [2,100]、23655 [2,100]。
//
// 192（args_num=1）"附加{0}%当前体力值的伤害"：
//   - 载体：狂沙掠地击 13150（[192]|[10]，卡梅隆 150 物攻）、碎地强袭斩 14943
//     （[192]|[15]，沃夫莱德 150 物攻）。
//   - 4399 原文：两者均为"附加10%/15%**当前体力值**的伤害"——**无限定词**（说"对手"
//     时官方模板必带"对手/对方"字样，如 593 的{0}取值方）→ "当前体力值"取**施放者
//     自身**当前体力（技能描述默认主语 = 使用者；看板 192 行同口径），记档。
//   - 落点 = deal_pink_damage(PERCENT_VALUE, self.hp × {0}/100)（F8 593 同款：PERCENT_
//     VALUE 的 amount 传折算后的具体值）；结算时点 = SKILL_EFFECT 桶（当次攻击效果
//     结算期、ATTACK_DAMAGE 红伤之前——593/60 同桶先例，粉在红前是本家族既有时序）。
//   - 折算用整型除法（hp×pct/100 向下取整，引擎固伤一贯口径），折算 ≤0（空血异常态）
//     无伤可附。
//
// 前置三查（2026-09-29）：①内核 grep 无 571/611/192 硬编码（唯一命中是
// battle_primitives.cpp:1897 注释里"571×1.01"的数值演算示例，无关）②custom_effect_
// overrides 无这三个 effect_id 的行（无 window 家族声明——571/611 显式走 NextRounds
// 帮助函数，与 F8 1004 同因）③moves_lib/soul_lib 无既有注册（SKILL_EFFECT(571|611|192)
// 零命中；1925/1922/1926 是别的 id）。链上伙伴 143/5/408/585 均已实现（stat_dispel/
// round_window），载体链完整可用。
//
// 场景回归：test/scenario/scenario_750_delayed_damage.cpp（750-759 号段，一文件覆盖
// 三效果；571 用 23050 [2,300] 钉引爆时点/防叠加刷新/换宠即失、16869 [1,300] 钉 N=1；
// 611 用 23640/23655 [2,100] 钉窗口两跳/属性技不触发/对手出手不触发；192 用 13150/
// 14943 钉折算整除）。

#include "common.h"

namespace {

// ── 571 tick —— 回合扣减点：到引爆回合对对手结算 {1} 点固定伤害（粉伤·固定档）──
// args = [owner, target, amount, detonate_round]。
EffectResult effect_571_detonate_tick(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 571, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_count >= 4, 571, args, "需要 [owner,target,amount,detonate_round]");
    EFFECT_ARG_GUARD(g_core && g_core->deal_pink_damage != nullptr, 571, args,
                     "需要 deal_pink_damage 原语");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int amount = args.int_args[2];
    const int detonate_round = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1, 571, args,
                     "owner/target 越界");
    if (ctx->roundCount != detonate_round) {
        return EffectResult::kOk;   // "N回合后"：窗口内其余回合的收尾点静默空跑
    }
    if (ctx->getPet(target).hp <= 0) {
        return EffectResult::kOk;   // 对手当前宠已倒：不结算（F7 76 / 837 同款早退）
    }
    (void)g_core->deal_pink_damage(ctx, target, amount, DamageKind::FIXED, owner);
    return EffectResult::kOk;
}

// ── 571 —— {0}回合后对对手造成{1}点固定伤害 重复使用无法叠加 ──
// Args: [owner, target, {0}=延迟回合, {1}=固伤点数]。
EffectResult effect_571_delayed_fixed(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 571, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_count >= 4, 571, args, "需要 [owner,target,延迟回合,固伤点数]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int rounds = args.int_args[2];
    const int amount = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1, 571, args,
                     "owner/target 越界");
    EFFECT_ARG_GUARD(rounds > 0 && amount > 0, 571, args, "需要 延迟回合>0 且 固伤点数>0");
    // NextRounds 窗口（"N回合后"本回合收尾点不算数）挂**回合扣减点**；duration=N 只是
    // 窗口寿命，引爆由 detonate_round = cast_roundCount+N 精确闸门（本族记档）。
    // 同源去重 = "重复使用无法叠加"：同侧重复注册覆盖旧计时（刷新语义，见文件头）。
    const int detonate_round = ctx->roundCount + rounds;
    register_next_rounds_effect(ctx, State::BATTLE_ROUND_REDUCTION_ALL_ROUND_MINUS, owner, 571,
                                rounds, {owner, target, amount, detonate_round},
                                &effect_571_detonate_tick);
    return EffectResult::kOk;
}

// ── 611 tick —— 回合首：武装一条**当回合**的"攻击命中附加固伤"watcher ──
// args = [owner, target, amount, 武装时快照的 on_stage 槽位]。
EffectResult effect_611_arm_hit_watcher(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int armed_slot = args.int_args[3];
    const int amount = args.int_args[2];
    if (self < 0 || self > 1 || armed_slot < 0 || amount <= 0) {
        return EffectResult::kOk;
    }
    if (ctx->on_stage[self] != armed_slot) {
        return EffectResult::kOk;   // 窗口随挂宠走：换宠当回合不武装，换回则续（1004 同款）
    }
    // 当回合 watcher：duration=1 → 本回合后由 cleanup 收走；TEAM 作用域活过回合交界。
    ctx->event_center_.register_watcher(
        EventType::EVENT_HIT, self, ctx->roundCount, /*duration_rounds=*/1, /*once=*/false,
        [self, amount](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.actor != self) {
                return;   // 只认"自己使用技能命中"（对手出手不算，EVENT_HIT actor 过滤）
            }
            if (!executed_skill_is_attack(c, self)) {
                return;   // "攻击技能"限定：物理/特殊才算，属性技命中不发附加（481/681 判据）
            }
            if (!g_core || !g_core->deal_pink_damage) {
                return;
            }
            g_core->deal_pink_damage(c, 1 - self, amount, DamageKind::FIXED, self);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 611 —— 下{0}回合自身使用攻击技能则附加{1}点固定伤害 ──
// Args: [owner, target, {0}=回合数, {1}=附加点数]。
EffectResult effect_611_next_rounds_attack_bonus(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 611, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_count >= 4, 611, args, "需要 [owner,target,回合数,附加点数]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int amount = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner, 611,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && amount > 0, 611, args, "需要 回合数>0 且 附加点数>0");
    // NextRounds 窗口（"下{0}回合"：起点天然 R+1）→ ROUND_START 桶在 [R+1, R+{0}]
    // 每回合武装一条当回合 EVENT_HIT watcher。窗口挂宠：武装时快照槽位。
    const int armed_slot = ctx->on_stage[owner];
    register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, owner, 611, rounds,
                                {owner, opp, amount, armed_slot}, &effect_611_arm_hit_watcher);
    return EffectResult::kOk;
}

// ── 192 —— 附加{0}%当前体力值的伤害（"当前体力值" = 施放者自身，见文件头记档）──
// Args: [owner, target, {0}=百分比]。
EffectResult effect_192_percent_current_hp(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 192, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_count >= 3, 192, args, "需要 [owner,target,百分比]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int pct = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner, 192,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(pct > 0, 192, args, "需要 pct > 0");
    if (!g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    // 施放者**当前**体力（hp = numericalProperties[HP] 的引用别名，即当前值口径）。
    const int amount = ctx->getPet(owner).hp * pct / 100;
    if (amount <= 0) {
        return EffectResult::kOk;   // 折算为 0：无伤可附
    }
    g_core->deal_pink_damage(ctx, opp, amount, DamageKind::PERCENT_VALUE, owner);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(571, effect_571_delayed_fixed)
SKILL_EFFECT(611, effect_611_next_rounds_attack_bonus)
SKILL_EFFECT(192, effect_192_percent_current_hp)
