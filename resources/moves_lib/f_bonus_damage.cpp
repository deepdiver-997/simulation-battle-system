#include <utils/sbs_rand.h>
// F 组批产 · F8 —— 固伤附加族：593×35 / 455×33 / 1004×32 / 1244×31（2026-09-29，worktree ../agents/f8）
//
// 家族语义："在攻击之外**附加**一笔伤害"——四条模板全部走 deal_pink_damage 原语
// （插件造粉伤唯一入口，不直写 hp）：593 = PERCENT_VALUE 档（数值按面板取值折算），
// 455 / 1004 / 1244 = FIXED 档（点数）。
//
// 593（args_num=3）"附加{0}的{1}值的{2}%的百分比伤害"：
//   - 载体：光之翼 16115（物攻 80 先制+2，[0,0,15]）、圣熄之力 16134（物攻 0，
//     [1,3,100]）。全库 35 载体（16115/16134/16197/16333/17045/…）。
//   - ⚠️ {1} 的槽序是**官方属性码序**，不是引擎 NumericalPropertyIndex 序！
//     4399 图鉴六处交叉验证：光之翼[0,0,15]="附加自身**攻击**15%"、圣光创世盾
//     [0,1,25]="附加自身**防御值**25%"、真龙千言诀/摄魂耀芒[0,2,*]="附加自身
//     **特攻**值*"、圣熄之力/秘法射击[1,3,*]="附加**对方**的**特防**值*"、
//     光羽脉冲波[0,4,15]="附加自身**速度**15%" → 官方码 {0攻 1防 2特攻 3特防 4速}
//     → 引擎槽映射 {0,2,1,3,4}（常识速查 477 六槽同款官方序，与
//     stat_dispel.cpp 的 kStatMap={0,2,1,3,4,5} 一致——按引擎序直读会把
//     "防御"读成"特攻"、"特攻"读成"防御"）。
//   - {0} = 取值方（0=自身、1=对方），{2} = 百分比。取值口径 = **当前面板值**
//     `pet.numericalProperties[idx]`（含套装加成的面板口径；hp 别名同源，
//     numericalBase 是底板、套装线 suit.cpp 只改 numericalProperties）。
//   - 落点 = deal_pink_damage(PERCENT_VALUE, 面板值×{2}/100)（d5_D15 的 1327
//     同款用法：amount 传折算后的具体值）；结算时点 = SKILL_EFFECT 桶（当次
//     攻击的效果结算期，ATTACK_DAMAGE 红伤之前——effect 60 "每回合附加固伤"
//     同桶先例，粉在红前是本家族既有时序）。
// 455（args_num=2）"每损失{0}点体力则额外附加{1}点固定伤害"：
//   - 载体：创世弹 13552（特攻 90，[1,1]）。全库 33 载体，{0}∈{1,2}、{1}=1。
//   - 损失 = 自身 max_hp − 当前 hp（numericalBase[HP] 为最大值口径，
//     common.h max_hp_of 同源）→ 附加 (损失/{0})*{1} 点 FIXED。创世弹 [1,1]
//     = 每损失 1 点体力附加 1 点（阿瑞斯"体力越低打越疼"名技）。
// 1004（args_num=2）"{0}回合内使用技能附加{1}点固定伤害，若自身体力高于对手则效果翻倍"：
//   - 载体：狂龙啸傲 19814（特攻 0，链 [63,1003,1004]/args[80,6,1,5,150]，
//     1004 切片 [5,150]）。4399（天邪龙皇）："5回合内每回合附加150点固定伤害，
//     使用时若自身体力高于对手则造成的固定伤害翻倍"——effect_info 模板即权威
//     （"使用技能附加"，按次结算；4399 的"每回合"是图鉴转述口径）。
//   - ⚠️ **撞号防混淆**：SKILL_EFFECT 1004（本条，技能效果注册空间）≠ 沧岚挂泪
//     SOUL_MARK 1004（soul_lib/canglan_tears.cpp，魂印注册空间）——两表互不相干
//     （常识速查 §二有记档），检索/翻账时别串门。
//   - 窗口 = **NextRounds**（register_next_rounds_effect，d5_D15/F3 范式）：
//     起点天然 R+1，窗口恰 [R+1, R+{0}]。为什么不用 InRounds（字面"{0}回合内"）：
//     消费通道是 EVENT_HIT watcher（F6 817 同款注册法），而**用技当回合 R 的
//     EVENT_HIT 在 ON_SKILL_HIT 桶后已 drain**（Skills::execute emit → 本桶收尾
//     投递，早于 SKILL_EFFECT 桶跑 1004 的注册）——R 当回合的消费天生观察不到，
//     NextRounds 让 {0} 个可观察回合一格不少。
//   - 消费 = 每回合 ROUND_START tick 武装一条**当回合**的 EVENT_HIT watcher
//     （duration=1，WatcherScope::TEAM——跨回合 watcher 必须 TEAM，常识速查 §五）；
//     回调过滤 ev.actor==self → 附加 {1} 点 FIXED（自身体力**严格高于**对手时
//     ×2，"使用时"= 命中结算点现读双方 hp）。
//   - 换宠口径：窗口挂**宠物**（武装时快照 on_stage 槽位，各回合 tick 复核——
//     槽位不符不武装；换回则续）——比纯 TEAM 多一层"换宠即失"的忠实度，
//     又避开 ON_STAGE watcher 活不过回合交界的坑（68 拦截器 slot 快照同思路）。
// 1244（args_num=4）"随机附加{0}-{1}点固伤，对手处于能力下降状态时固伤变为{2}-{3}点"：
//   - 载体：荒芜之爪 31164（物攻 85 先制+3，链 [889,1244]/args[6,100,300,200,400]，
//     1244 切片 [100,300,200,400]）。
//   - "处于能力下降状态" = 对手 ability_levels 六槽（0攻..5命中）任一 **< 0**
//     （权威态在 BattleContext::ability_levels，stat_drop 落点）→ 区间取
//     [{2},{3}]，否则 [{0},{1}]。随机 roll = e_power_crit_ramp 同款
//     lo + rng.next()%(hi-lo+1)。两档都是 FIXED 点数。
//
// 前置三查（2026-09-29）：①内核无 593/455/1004/1244 硬编码（grep src/ include/
// 仅 plugin_interface.h 注释里 SOUL_MARK 1004 示例，非实现）②custom_effect_overrides
// 无这四个 effect_id 的行（默认窗口族判定不牵连；1004 显式走 NextRounds 帮助函数）
// ③moves_lib 无既有注册（SKILL_EFFECT(593/455/1004/1244) 零命中；63/889 已在
// batch_b_skills/selection_seal 实现，1003 全库未注册=狂龙啸傲链上它 no-op，不碍事）。
//
// 场景回归：test/scenario/scenario_680_bonus_damage.cpp（680-689 号段，一文件覆盖四效果）。

#include "common.h"

namespace {

// ── 593：官方属性码（{0}攻 {1}防 {2}特攻 {3}特防 {4}速）→ 引擎数值属性槽 ──
// 与 stat_dispel.cpp 的 kStatMap={0,2,1,3,4,5} 前五位一致（官方第六位是命中，
// 本模板的"值"只观测到 0..4；命中无"值"面板，出现即按参数故障记档跳过）。
NumericalPropertyIndex stat_593_engine_slot(int official_code) {
    switch (official_code) {
        case 0: return NumericalPropertyIndex::PHYSICAL_ATTACK;
        case 1: return NumericalPropertyIndex::DEFENSE;
        case 2: return NumericalPropertyIndex::SPECIAL_ATTACK;
        case 3: return NumericalPropertyIndex::SPECIAL_DEFENSE;
        case 4: return NumericalPropertyIndex::SPEED;
        default: return NumericalPropertyIndex::HP;   // 不可达（调用方已 guard 0..4）
    }
}

// ── 593 —— 附加{0}的{1}值的{2}%的百分比伤害 ──
// int_args = [owner, target, 取值方, 官方属性码, 百分比]（build_effect_args_for_skill
// 固定前置 [owner,target]，bind_participants 填实；模板切片从 [2] 起）。
EffectResult effect_593_percent_of_stat(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 593, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 593, args,
                     "需要 [owner,target,取值方,官方属性码,百分比]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int side_code = args.int_args[2];
    const int stat_code = args.int_args[3];
    const int pct = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner,
                     593, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(side_code == 0 || side_code == 1, 593, args,
                     "取值方 {0}=自身 / {1}=对方（4399 口径）");
    EFFECT_ARG_GUARD(stat_code >= 0 && stat_code <= 4, 593, args,
                     "官方属性码 0攻/1防/2特攻/3特防/4速（4399 六载体交叉验证）");
    EFFECT_ARG_GUARD(pct > 0, 593, args, "需要 pct > 0");
    if (!g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int stat_side = (side_code == 0) ? owner : opp;
    const int stat_value = ctx->getPet(stat_side)
        .numericalProperties[stat_593_engine_slot(stat_code)];
    const int amount = stat_value * pct / 100;
    if (amount <= 0) {
        return EffectResult::kOk;   // 面板值为 0 / 折算为 0：无伤可附
    }
    g_core->deal_pink_damage(ctx, opp, amount, DamageKind::PERCENT_VALUE, owner);
    return EffectResult::kOk;
}

// ── 455 —— 每损失{0}点体力则额外附加{1}点固定伤害 ──
// int_args = [owner, target, {0}=每损失体力档, {1}=附加点数]。
EffectResult effect_455_per_hp_lost(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 455, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 455, args,
                     "需要 [owner,target,损失档,附加点数]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int step = args.int_args[2];
    const int per = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner,
                     455, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(step > 0 && per > 0, 455, args, "需要 损失档>0 且 附加点数>0");
    if (!g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int lost = max_hp_of(ctx, owner) - ctx->getPet(owner).hp;
    if (lost <= 0) {
        return EffectResult::kOk;   // 满血（或异常态）：无损失可折算
    }
    const int amount = lost / step * per;
    if (amount <= 0) {
        return EffectResult::kOk;   // 损失不足一档
    }
    g_core->deal_pink_damage(ctx, opp, amount, DamageKind::FIXED, owner);
    return EffectResult::kOk;
}

// ── 1004 的回合 tick：武装**当回合**的"使用技能附加固伤"watcher（F6 EVENT_HIT 同款）──
// args = [owner, target, amount, 武装时快照的 on_stage 槽位]。
EffectResult effect_1004_round_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int amount = args.int_args[2];
    const int armed_slot = args.int_args[3];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || amount <= 0) {
        return EffectResult::kOk;
    }
    if (ctx->on_stage[self] != armed_slot) {
        return EffectResult::kOk;   // 窗口随挂宠走：换宠/阵亡当回合不再武装（68 快照同思路）
    }
    // 当回合 watcher：duration=1 → 本回合后由 cleanup 收走；TEAM 作用域活过回合交界。
    ctx->event_center_.register_watcher(
        EventType::EVENT_HIT, self, ctx->roundCount, /*duration_rounds=*/1, /*once=*/false,
        [self, opp, amount](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.actor != self) {
                return;   // 只认"自己使用技能命中"（EVENT_HIT：miss/被封不发，属性技命中也发）
            }
            if (!g_core || !g_core->deal_pink_damage) {
                return;
            }
            // "使用时若自身体力高于对手则效果翻倍"：命中结算点现读双方当前体力。
            const int gain = (c->getPet(self).hp > c->getPet(opp).hp) ? amount * 2 : amount;
            g_core->deal_pink_damage(c, opp, gain, DamageKind::FIXED, self);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 1004 —— {0}回合内使用技能附加{1}点固定伤害，若自身体力高于对手则效果翻倍 ──
// int_args = [owner, target, {0}=回合数, {1}=附加点数]。
// ⚠️ SKILL_EFFECT 1004 ≠ SOUL_MARK 1004（沧岚挂泪，soul_lib/canglan_tears.cpp）——
// 两个注册空间，本 handler 只进技能效果表。
EffectResult effect_1004_skill_bonus_fixed(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1004, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1004, args,
                     "需要 [owner,target,回合数,附加点数]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int amount = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner,
                     1004, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && amount > 0, 1004, args, "需要 回合数>0 且 附加点数>0");
    // NextRounds 窗口（d5_D15 范式）：起点天然 R+1 → ROUND_START 桶在 [R+1, R+{0}]
    // 每回合武装一条当回合 EVENT_HIT watcher。窗口挂宠：武装时快照槽位。
    const int armed_slot = ctx->on_stage[owner];
    register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, owner, 1004, rounds,
                                {owner, opp, amount, armed_slot}, &effect_1004_round_tick);
    return EffectResult::kOk;
}

// ── 1244 —— 随机附加{0}-{1}点固伤，对手处于能力下降状态时固伤变为{2}-{3}点 ──
// int_args = [owner, target, 下限, 上限, 下降档下限, 下降档上限]。
EffectResult effect_1244_random_fixed(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1244, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 1244, args,
                     "需要 [owner,target,下限,上限,下降档下限,下降档上限]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int lo = args.int_args[2];
    const int hi = args.int_args[3];
    const int lo_drop = args.int_args[4];
    const int hi_drop = args.int_args[5];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner,
                     1244, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(lo >= 0 && hi >= lo && lo_drop >= 0 && hi_drop >= lo_drop, 1244, args,
                     "需要 0 ≤ 下限 ≤ 上限（两档各自成立）");
    if (!g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    // "对手处于能力下降状态"：六槽（0攻..5命中）任一负等级（权威态 ability_levels）。
    bool foe_dropped = false;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[opp][i] < 0) {
            foe_dropped = true;
            break;
        }
    }
    const int lo_use = foe_dropped ? lo_drop : lo;
    const int hi_use = foe_dropped ? hi_drop : hi;
    const int amount = lo_use + (hi_use > lo_use ? ctx->rng().next() % (hi_use - lo_use + 1) : 0);
    if (amount <= 0) {
        return EffectResult::kOk;
    }
    g_core->deal_pink_damage(ctx, opp, amount, DamageKind::FIXED, owner);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(593, effect_593_percent_of_stat)
SKILL_EFFECT(455, effect_455_per_hp_lost)
SKILL_EFFECT(1004, effect_1004_skill_bonus_fixed)
SKILL_EFFECT(1244, effect_1244_random_fixed)
