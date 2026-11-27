// J 组 J116 · 保命窗族 —— 4 宏/56 载体（2026-10-03，worktree ../agents/j116）
//
// 家族语义（effect_info 模板即权威；4399 双源逐字验证）：
//   1068 [rounds,残留]   "下{0}回合受到致命伤害时残留{1}点体力"
//     —— 506（H4 h_residue_family.cpp）多回合直系：DeathInterceptor
//     sees_hp_consume=false + DefeatCause::DAMAGE 过滤 + 换宠即失槽快照（68 同款）；
//     窗口 = NextRounds 字面（"下{0}回合"不含当回合，506 单回合版口径）：恰
//     [R+1, R+{0}]。载体 {1} 全为 1（4399：琉璃心言"下2回合…残留1点体力"、
//     魔磁障壁"下3回合…"、密计脱身"下1回合…"）。
//   1259 [rounds,概率,六槽×6] "{0}回合内有{1}%概率免疫对手攻击伤害，未触发则对手{2}"
//     —— 1024（J14 j_prob_fallback3.cpp）同构去粉伤兜底：每回合掷点（4399 鬼步魅影
//     明写"每回合50%几率"；1024 直系节奏）——首回合用技即掷（850 首跳：ROUND_START
//     本回合已跑过）、2..{0} 走 NextRounds 的 ROUND_START tick；掷中 → DAMAGE 免票
//     duration=1（"攻击伤害"限定 → 只 DAMAGE 不 PINK，850/1024 同款）；掷歪 → 当场
//     对手 stat_drop（⚠️ 时点记档：官方无时点字样，1024 的兜底要量受伤故压 ROUND_END，
//     本族无金额需求取掷点即施加）。尾段六槽 = **语义序**（攻,防,特攻,特防,速,命中）
//     → 引擎槽 {0,2,1,3,4,5}（477/158 同款映射），4399 三载体钉死：沉灵之海
//     [0,0,0,-2,-2,0]="特防-2，速度-2"、冥界之火 [0,-2,0,0,-2,0]="防御-2、速度-2"、
//     冥魂之躯 [0,-2,0,0,0,-2]="防御-2、命中-2"（命中槽仅语义序才有）。⚠️ 工单"降2项"
//     不精确：双刃分合/固化石躯切片为 3 项降低（"防御-2、特防-2、速度-2"），按切片
//     逐槽实现（非零槽各降 |v| 级）。数值施加走 stat_drop（吃免弱/禁降/钳-6，
//     actor=owner——1259 是"未触发"罚则，属对手施加的弱化，不吃免弱就名不副实）。
//   874  [rounds,阈值]   "{0}回合内若对手造成的伤害低于{1}则免疫该伤害"
//     —— 量判免疫（4399 福禄富贵/蝎甲鳞鳞"3回合内若对手造成的伤害低于300则免疫该
//     伤害"）：受击侧 DETECT 桶读 final（1236 圣光吟诵对称位），final < 阈值（严格
//     低于）→ final=0。⚠️ BLOCK 票无金额钩位，DETECT 量判补票是唯一通道（工单口径，
//     记档）；红伤限定（DETECT 相位只在红伤管线——粉伤走 IMMUNE 阶段、阈值 200~500
//     段下小粉伤 tick 本就够不着；官方例句均指攻击伤害）；窗口 InRounds 含当回合
//     （729/1236 同款 round_effect_start_round 折算，后手不少算）。等值边界（伤害恰
//     ==阈值）不可稳定构造（85%~100% 浮动），按字面"低于"实现不设断言。
//   62   [rounds]        "{0}回合后若对方没有将自己击败，则对方死亡"
//     —— 镇魂歌直系（4399："3回合后若对手没有将自己击败，则对方死亡"）：InRounds
//     {0} 回合窗 + **窗末 ROUND_END 判定**（⚠️ 口径记档："N回合后"取 InRounds 字面
//     ——窗 [R, R+{0}-1]（后手经 round_effect_start_round 顺延为 [R+1, R+{0}]）、
//     末回合 ROUND_END 判死）。承诺检验 = 武装槽仍存活且仍在场（被击败/换下 =
//     承诺已破不判死——官方镇魂歌换人失效同源）。判死 = force_hp_to_zero（J23 495
//     秒杀统一落点：免疫秒杀票短路/转化/EVENT_HP_TO_ZERO）。⚠️ 与残留免死交互
//     （实测记档 2026-10-03）：force_hp_to_zero 本体直写 hp=0、不走拦截器，但其后
//     ROUND_END 死亡漏斗（battleFsm funnel_on_stage_deaths）对在场 hp≤0 者一律以
//     DefeatCause::DAMAGE 重判 → 506/1068 式残留拦截器（只拦 DAMAGE）**会把判死
//     拦下**（T2109 交互块实证 hp=0→漏斗→残留 1）。引擎现状 = 镇魂歌判死可被残留
//     免死救下；官方无实证、引擎为准、留 R 组台账。
//
// 载体切片（unity 库 side_effect.arg_count 切分全载体自洽、零剩余，脚本核算 2026-10-03；
// ⚠️ 记档：62 实测 12 载体（工单写 ×10）、总数 56（工单写 54），切片核算为准）：
//   1068 [rounds,残留] ×16：[2,1]×10、[1,1]×2、[3,1]×4——{1} 恒 1
//     纯链：魔磁障壁 28402 [3,1]、密计脱身 38344 [1,1]（伴生 1281/1782 n=0 no-op）
//   1259 [rounds,概率,六槽] ×16：[3,50,…]×12、[4,50,…]×2、[3,60,…]×1——概率 50/60，
//     降槽组合：防速×9、特防速×6、防特防速×2（双刃分合/固化石躯）、防命中×2
//     纯链：盛夏相逢 27995 [3,50,0,0,0,-2,-2,0]（特防速）
//   874  [rounds,阈值] ×12：阈值 200/250/280/300×8/350/500，回合 2~4
//     纯链：芸生长啸 37086 [3,350]（物攻140 载体）
//   62   [rounds] ×12：[2]×1、[3]×3、[4]×2、[5]×5、[7]×1
//     纯链：镇魂歌 20215 [3]、忍辱负重 23344 [5]、龙契·逝 37740 [4]（物攻120 载体）、
//     绝命镇魂歌 28140 [2]（29032 的 62 后有 43/843 已实现伴生）
//
// 动工前置三查（2026-10-03）：①内核 grep 1068/1259/874/62 无硬编码（62 命中 case 628、
//   135 系 1351/1355 均近邻噪声）②custom_effect_overrides 4 id 零行、custom_effect_seed
//   零命中 ③moves_lib 无既有注册——真缺口。范式先例：506（DeathInterceptor 多回合窗）、
//   1024（概率免票+每回合 tick）、1236（受击侧 DETECT 桶）、495（force_hp_to_zero
//   秒杀原语）、170（免票 source_id=0 路径②）。
//
// 场景回归：test/scenario/scenario_2100_lifeguard_window.cpp（号段 2100-2109，一文件
//   覆盖四效果六载体；纯链载体做精确断言，概率块走种子校准）。

#include "common.h"

#include <memory>

#include <primitives/battle_primitives.h>

namespace {

// ── 语义序 → 引擎槽映射（477/158 同款：语义序 攻,防,特攻,特防,速,命中）──
int j116_semantic_to_engine(int semantic) {
    switch (semantic) {
        case 0: return 0;   // 攻
        case 1: return 2;   // 防
        case 2: return 1;   // 特攻
        case 3: return 3;   // 特防
        case 4: return 4;   // 速
        case 5: return 5;   // 命中
        default: return -1;
    }
}

// ── 1068 —— 下{0}回合受到致命伤害时残留{1}点体力（506 多回合直系）──
// int_args = [owner, target, {0}=回合数, {1}=残留点数]。
EffectResult effect_1068_lethal_residue_next_rounds(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1068, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1068, args,
                     "1068 布局=[owner,target,回合数,残留点数]（int_count>=4）");
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    const int residue = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 1068, args, "owner 越界");
    EFFECT_ARG_GUARD(rounds > 0 && residue > 0, 1068, args, "需要 回合数>0 且 残留>0");
    if (!g_core || !g_core->register_death_interceptor) {
        return EffectResult::kOk;
    }
    // 窗口 = NextRounds {0} 回合：本回合不算，恰 [R+1, R+{0}]（"下{0}回合"字面，
    // 506 单回合口径的多回合推广）。
    const int start = ctx->roundCount + 1;
    const int expire = start + rounds;
    const int slot = ctx->on_stage[self];   // 换宠即失保护（68/506 同款武装时快照）
    DeathInterceptor ic;
    ic.source_effect_id = 1068;
    ic.owner = self;
    ic.sees_hp_consume = false;   // 残留体力免死档：消耗体力（自爆族）穿免死，不拦
    ic.fn = [self, slot, start, expire, residue](BattleContext* c, int side, int die_slot,
                                                 int actor, DefeatCause cause) -> bool {
        (void)actor;   // 不限来源（506 同款：反伤致死也保）
        if (!c || side != self || die_slot != slot) {
            return false;
        }
        if (cause != DefeatCause::DAMAGE) {
            return false;   // 只拦伤害致死（秒杀/消耗体力/直写归零不在本档）
        }
        if (c->roundCount < start || c->roundCount >= expire) {
            return false;   // 窗口=恰"下{0}回合"（其余回合自然哑火）
        }
        c->seerRobot[side].elfPets[slot].hp = residue;   // "残留{1}点体力"（契约：写回 >0）
        return true;
    };
    g_core->register_death_interceptor(ctx, ic);
    return EffectResult::kOk;
}

// ── 1259 每回合掷点（1024 roll 骨架去粉伤兜底）：掷中挂当回合免攻票，
//    掷歪当场按语义序六槽 stat_drop 对手 ──
void j116_1259_roll_round(BattleContext* ctx, int owner, int opp, int chance,
                          const int (&drops)[6]) {
    if (!ctx || owner < 0 || owner > 1 || opp < 0 || opp > 1 || chance <= 0) {
        return;
    }
    if (roll_skill_percent(*ctx, 1259, 0, owner, owner, chance)) {
        ctx->rule_center_.grant_immune(
            owner, static_cast<int>(ImmunityType::DAMAGE), ~0ULL, /*anomaly_mask=*/0,
            /*duration_rounds=*/1, ctx->roundCount, /*source_id=*/0, /*soul=*/false,
            EffectScope::ON_STAGE, /*source_slot=*/-1, /*counts=*/0,
            /*source_effect_id=*/1259);
        return;
    }
    if (!g_core || !g_core->stat_drop) {
        return;
    }
    for (int i = 0; i < 6; ++i) {
        if (drops[i] != 0) {   // 切片负值 = 降 |v| 级；stat_drop 传正级数（内部 level-amount）
            (void)g_core->stat_drop(ctx, opp, j116_semantic_to_engine(i), -drops[i], owner);
        }
    }
}

// ── 1259 第 2..{0} 回合的 ROUND_START tick（payload 拷贝自注册参数）──
EffectResult j116_1259_round_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 8) {
        return EffectResult::kOk;
    }
    const int drops[6] = {args.int_args[4], args.int_args[5], args.int_args[6],
                          args.int_args[7], args.int_args[8], args.int_args[9]};
    j116_1259_roll_round(ctx, args.int_args[0], args.int_args[1], args.int_args[3], drops);
    return EffectResult::kOk;
}

// ── 1259 —— {0}回合内有{1}%概率免疫对手攻击伤害，未触发则对手{六槽降} ──
// int_args = [owner, target, {0}回合, {1}概率, 语义序六槽×6]（int_count>=8）。
EffectResult effect_1259_chance_immune_or_drop(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1259, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 10, 1259, args,
                     "1259 布局=[owner,target,回合,概率,语义序六槽×6]（int_count>=10）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int chance = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1, 1259, args,
                     "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && chance > 0, 1259, args, "需要 回合/概率 > 0");
    const int drops[6] = {args.int_args[4], args.int_args[5], args.int_args[6],
                          args.int_args[7], args.int_args[8], args.int_args[9]};
    // 首回合立即掷（850 首跳先例：ROUND_START 桶本回合已跑过）。
    j116_1259_roll_round(ctx, owner, opp, chance, drops);
    if (rounds > 1) {
        register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, owner, 1259, rounds - 1,
                                    {owner, opp, 0, chance, drops[0], drops[1], drops[2],
                                     drops[3], drops[4], drops[5]},
                                    &j116_1259_round_tick);
    }
    return EffectResult::kOk;
}

// ── 874 —— {0}回合内若对手造成的伤害低于{1}则免疫该伤害（DETECT 量判补票）──
// int_args = [owner, target, {0}=回合数, {1}=阈值]。
EffectResult effect_874_low_damage_immune_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 874, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 874, args,
                     "874 布局=[owner,target,回合数,阈值]（int_count>=4）");
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    const int threshold = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 874, args, "owner 越界");
    EFFECT_ARG_GUARD(rounds > 0 && threshold > 0, 874, args, "需要 回合数>0 且 阈值>0");
    // 窗口 InRounds 含当回合（729/1236 同款折算，后手不少算）。
    const int start = ctx->round_effect_start_round(self, rounds, EffectWindowKind::InRounds);
    const int expire = start + rounds;
    ctx->register_damage_effect(
        DamagePhase::DETECT, self, DamageEffectCategory::DETECT,
        [self, start, expire, threshold](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != self) {
                return;
            }
            if (c->roundCount < start || c->roundCount >= expire) {
                return;   // "{0}回合内"窗自检（InRounds）
            }
            DamageSnapshot& d = c->resolvedDamage;
            if (d.defenderId != self || d.attackerId < 0 || d.attackerId > 1) {
                return;   // 只认"对手造成的伤害"
            }
            if (!d.isRed || d.final <= 0) {
                return;   // 红伤限定（文件头记档）；final<=0 无需免
            }
            if (d.final < threshold) {
                d.final = 0;   // 严格低于阈值 → 免疫该伤害（量判补票：final=0 不扣血）
            }
        });
    return EffectResult::kOk;
}

// ── 62 窗末判定 tick（末回合 ROUND_END 才行动，前 {0}-1 个回合末自然哑火）──
EffectResult j116_62_judge_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core
        || !g_core->force_hp_to_zero) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int slot = args.int_args[2];
    const int last_round = args.int_args[3];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || ctx->roundCount != last_round) {
        return EffectResult::kOk;   // 窗末才判（"{0}回合后"）
    }
    // 承诺检验：武装槽仍存活且仍在场（被击败/换下 = 承诺已破）。
    if (slot != ctx->on_stage[owner]) {
        return EffectResult::kOk;
    }
    if (ctx->seerRobot[owner].elfPets[slot].hp <= 0) {
        return EffectResult::kOk;   // "对方没有将自己击败"不成立
    }
    const int opp_slot = ctx->on_stage[opp];
    if (ctx->seerRobot[opp].elfPets[opp_slot].hp <= 0) {
        return EffectResult::kOk;   // 对方在场宠已倒：无需再判死（495 存活门）
    }
    (void)g_core->force_hp_to_zero(ctx, opp, owner);   // 秒杀统一落点（免疫秒杀票可短路）
    return EffectResult::kOk;
}

// ── 62 —— {0}回合后若对方没有将自己击败，则对方死亡（镇魂歌）──
// int_args = [owner, target, {0}=回合数]。
EffectResult effect_62_defeat_promise_kill(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 62, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 62, args,
                     "62 布局=[owner,target,回合数]（int_count>=3）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner, 62,
                     args, "需要 owner/target ∈ {0,1} 且对立");
    EFFECT_ARG_GUARD(rounds > 0, 62, args, "需要 回合数 > 0");
    // InRounds {0} 回合窗（含当回合），末回合 ROUND_END 判死（口径记档见文件头）。
    const int start = ctx->round_effect_start_round(owner, rounds, EffectWindowKind::InRounds);
    register_round_effect(ctx, State::BATTLE_ROUND_END, owner, 62, rounds,
                          {owner, opp, ctx->on_stage[owner], start + rounds - 1},
                          &j116_62_judge_tick);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1068, effect_1068_lethal_residue_next_rounds);
SKILL_EFFECT(1259, effect_1259_chance_immune_or_drop);
SKILL_EFFECT(874, effect_874_low_damage_immune_window);
SKILL_EFFECT(62, effect_62_defeat_promise_kill);
