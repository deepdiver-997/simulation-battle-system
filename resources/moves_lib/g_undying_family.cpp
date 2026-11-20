// G 组 · G7 免死反扣族 —— 692×30（2026-09-29，worktree ../agents/g7）
//
// 692（args_num=1）"{0}回合内免疫受到的致命一击伤害并直接扣除对手等量体力"：
//   - ★ 68 不死族**姊妹档**（F6 DeathInterceptor 同通道，f_onhit_family.cpp）：
//     68 = 拦下致死后"余下1点"（残血档）；692 = 拦下致死后"完全免死（不掉血）"
//     + **反扣**。同一死亡拦截器挂点、同一边界口径，只差写回值与反扣子句。
//   - "免疫致命一击伤害" = 本次致命伤**不掉血**：拦截时 hp 已被 deal_damage 扣到 0
//    （契约：拦下必须写回 >0），写回值 = **受击前体力**。来源：致死一击的
//     EVENT_TAKE_DAMAGE 事件带 actual_damage = hp_before − hp_after，致死（钳 0）时
//     hp_after==0 → actual_damage **恰等于受击前体力**——由同窗口的受击 watcher 快照
//    （handler 一次创建 shared_ptr，watcher 写 / 拦截器读，811 std::function 可拷贝先例）。
//   - "直接扣除对手等量体力" = 原伤量**真伤直通**（deal_true_damage：穿护盾/护罩/
//     抗性/免疫，不进伤害管线——圣甲·盖亚 3056 死亡判定弹伤同款原语与读数）：
//     数值读 core 落血记录 last_landed_hit[side][slot].amount（护盾/锁伤后全额、
//     过量击杀不缩水——弹伤族数据源口径）。
//   - ⚠️ "致命一击"判据（与 68 的取宽记档差异，故意收窄并记档）：拦截器签名不带伤害
//     种类，但 692 的"等量"数值语义**必须**读落血记录 → 顺势按记录 kind 收窄到
//     **NORMAL（攻击红伤）/ ATTRIBUTE（属性直伤）** 两档（盖亚弹伤同判据）；粉伤/
//     真伤斩杀不是"一击"→ 不拦不扣（死亡照常成立）。68 无数值子句故取宽（粉/真致死
//     同拦）；692 有 → 收窄。HP_CONSUME（自爆族，sees_hp_consume=false）/ 秒杀
//    （INSTANT_KILL 票体系，非 DAMAGE 因素）/ EFFECT 直写归零（漏斗外无凭）不拦——
//     与 68 逐字同口径。
//   - 窗口 = {0} 回合 InRounds（round_effect_start_round 折算，后手 N≥2 顺延——68 同款），
//     拦截器 fn 内做回合闭环（过期自然哑火）；register_death_interceptor 同源幂等重注册
//    （复用即重新武装）；slot 用武装时快照——换宠即失保护。窗口期**多次**致死攻击逐次
//     拦截（{0} 回合是时限不是次数；每次拦截的受击前体力由 watcher 在该次致死一击落地时
//     刷新）。反伤/无归因致死照保（actor 不限，68 同款），只是"对手"无从扣时不反扣。
//
// 载体切片（DB moves.side_effect/side_effect_arg，side_effect.arg_count 切分，
// 2026-09-29 查库；全 30 条切片精确耗尽 arg 数组——单参数即 {0}=回合数，4399 原文
// 交叉验证逐字吻合，如 23769 无尽入侵"2回合内反弹对手致命一击的伤害"、
// 23920 紫电狂龙"2回合内反弹对手的致命一击"）：
//   23769 无尽入侵 [2]、23920 紫电狂龙 [2]、24136 无尽摧残 [3]、24168 异能暴涨 [2]、
//   24426 缥缈之羽 [2]、24668 长牙舞爪 [2]、24737 不败之地 [2]、24819 晓命之行 [2]、
//   26014 灵风惊梦 [3]、26718 寒潮退散 [2]、27106 神谕之证 [3]、27210 冰雪之力 [2]、
//   27518 胸有成竹 [3]、27567 万古图腾 [3]、27693 赤武之心 [3]、27824 绿荫牢笼 [3]、
//   27894 焚身火盾 [2]、27922 镇秽龙脉 [3]、28444 赤炎之盾 [3]、29025 意志模组·怒 [3]、
//   29147 幻动激涌 [4]、31710 圣战封印 [2]、31850 次元反转 [2]、32310 九幽天蛇 [3]、
//   33047 哀伤冰斩 [2]、36100 破灭之夜 [2]、36246 真实之影 [2]、37268 爆迫 [3]、
//   38250 肃正 [3]、38482 狂沙行者破 [3]
//   （35086 核·文明裁决是 LIKE '%692%' 子串误报，实际 1692——非载体）
//
// 动工前三查（2026-09-29）：①内核 grep 无 692 硬编码（src/ include/ 零命中）
// ②effect_meta 无牵连分类 ③moves_lib 无既有注册（SKILL_EFFECT(692) 零命中）——真缺口。
//
// 场景回归：test/scenario/scenario_770_undying_family.cpp（窗口内免死不掉血 + 反扣
// 精确等式、多次致死逐次拦、窗口外正常阵亡、双命名载体 23769/23920、反扣致死反杀）。

#include "common.h"

#include <memory>

namespace {

// ════════════════════════════════════════════════════════════════════
// 692 —— {0}回合内免疫受到的致命一击伤害并直接扣除对手等量体力
// 挂靠：死亡拦截器（DeathInterceptor，68 姊妹档）+ 同窗口受击 watcher（快照受击前体力）。
// ════════════════════════════════════════════════════════════════════
EffectResult effect_692_undying_counter_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 692, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 692, args,
                     "需要 [owner,target,rounds]");
    if (!g_core || !g_core->register_death_interceptor) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && rounds > 0, 692, args, "需要 rounds>0");
    // 窗口 = {0} 回合 InRounds（含当回合；round_effect_start_round 折算后手顺延，68 同款）。
    const int start = ctx->round_effect_start_round(self, rounds, EffectWindowKind::InRounds);
    const int expire = start + rounds;
    const int slot = ctx->on_stage[self];   // 换宠即失保护（武装时快照，68 同款）
    // 受击前体力快照：handler 一次创建，watcher 写 / 拦截器读同一份（std::function 要求
    // 可拷贝，811 先例；每次用技新快照——旧 watcher 写旧份，与重武装后的新份互不串扰）。
    auto prehit_hp = std::make_shared<int>(0);
    // 受击 watcher：致死一击在 deal_damage 里 emit 的 actual_damage，致死（落地后 hp 钳 0）
    // 时恰等于受击前体力 → 只在"这一击把我打到 0"时刷新快照；非致死一击不记。
    // 事件在原语里只入队、State 桶后统一 drain（event_center.h）——攻击致死路径的 drain
    // 发生在行动时点，早于死亡结算时点（FIRST/SECOND_MOVER_DEATH）的漏斗 → 拦截器读到
    // 的一定是本次致死一击的快照。
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, self, start, rounds, /*once=*/false,
        [self, prehit_hp](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != self || ev.amount <= 0) {
                return;
            }
            if (c->getPet(self).hp > 0) {
                return;   // 没被打到 0：非致死一击，不动快照
            }
            *prehit_hp = ev.amount;   // 钳 0 后 actual_damage = 受击前体力
        },
        WatcherScope::TEAM);   // 跨回合 watcher 必须 TEAM（常识速查 §五）；宠物身份由
                               // 拦截器的 slot 快照把关，watcher 只按方记数
    DeathInterceptor ic;
    ic.source_effect_id = 692;
    ic.owner = self;
    ic.sees_hp_consume = false;   // 残留体力免死档：消耗体力（自爆族）穿免死，不拦（68 同款）
    ic.fn = [self, slot, start, expire,
             prehit_hp](BattleContext* c, int side, int die_slot, int actor,
                        DefeatCause cause) -> bool {
        if (!c || side != self || die_slot != slot) {
            return false;
        }
        if (cause != DefeatCause::DAMAGE) {
            return false;   // 秒杀（INSTANT_KILL 票）/消耗体力/直写归零不在本档（68 边界）
        }
        if (c->roundCount < start || c->roundCount >= expire) {
            return false;   // {0} 回合窗口外（过期拦截器自然哑火，561 同款）
        }
        // "致命一击" = 攻击红伤/属性直伤的致死一击（core 落血记录 kind 判定，盖亚 3056
        // 弹伤同判据；粉伤/真伤斩杀不是"一击"→ 不拦不扣——比 68 取宽收窄，头注记档）。
        // hit.actor == actor 是一致性守卫（落血记录与死亡归因同一次 deal_damage 写入）。
        const BattleContext::LastLandedHit& hit = c->last_landed_hit[side][die_slot];
        if (!hit.valid || hit.amount <= 0 || hit.actor != actor
            || (hit.kind != static_cast<int>(DamageKind::NORMAL)
                && hit.kind != static_cast<int>(DamageKind::ATTRIBUTE))) {
            return false;
        }
        if (*prehit_hp <= 0) {
            return false;   // 没看到致死一击的受击事件（无凭写回）→ 不假拦（契约：写回 >0）
        }
        // 免疫致命一击伤害：写回受击前体力（"不掉血"；契约：拦下必须写回 >0）
        c->seerRobot[side].elfPets[slot].hp = *prehit_hp;
        // 直接扣除对手等量体力：原伤量真伤直通（deal_true_damage，盖亚弹伤同款）。
        // actor 无归因/自伤（反伤致死也保，68 同款）时没有"对手"可扣 → 只免死不反扣。
        if (actor >= 0 && actor <= 1 && actor != side && g_core->deal_true_damage) {
            g_core->deal_true_damage(c, actor, hit.amount, side);
        }
        return true;
    };
    g_core->register_death_interceptor(ctx, ic);   // 同源幂等重注册（复用即重新武装）
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(692, effect_692_undying_counter_window);
