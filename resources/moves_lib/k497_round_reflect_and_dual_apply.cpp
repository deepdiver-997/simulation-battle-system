// K497 · 回合制免疫反弹 + 双向施加未触发链两族 —— effect 2104（1 宏 / 4 载体）
// + effect 2200（1 宏 / 4 载体），2026-11-23 夜跑 a01-k497。
// 权威口径 〔源: docs_local/docs/05-任务清单/已收官/E组批量效果工单.md:102〕
// 数据来源 https://github.com/SeerAPI/seer-unity-assets （本区 SQLite 只读现查 2026-11-23）。
//
// ── 载体全链切片（side_effect.arg_count 优先、加载器实切；4399 文本逐条交叉验证）──
//  2104（{0}=回合数）：
//   29209 蘑蘑蜗·毒株散播   [483, 2104]      |[-1,-1,-1,-1,-1,-1,3] → 483[六槽-1]+2104[3]
//     （4399：「对手攻击-1…命中-1,后出手时弱化效果翻倍；对手不处于能力提升则3回合内免疫并反弹」）
//   29357 铭记片像          [1851, 2104, 1898]|[70,1,4,2] → 1851[70,1]+2104[4]+1898[2]
//     （K495 场景 8830 A 腿已锁 2104[4] 位，与本切一致）
//   37058 妖锋映黄泉        [1568, 2104, 1159, 1241, 1286]|[2,1,250,2] → 2104[2]
//   38444 深渊十字          [2083, 2104, 1012, 1679]|[2,300,300,100,16,3] → 2104[2]
//  2200（{0}/{1}=异常码）：
//   37438 化·血浴魂靡  [699, 1850, 2200]|[1,100,16,23]     → 2200[16,23]（流血→未触发补诅咒）
//   37793 灼锋破晦     [2220, 706, 2200]|[10,2,20]        → 2200[2,20]（烧伤→未触发补失明）
//   37853 爆炎浴火击   [40, 1549, 2200]|[1,2,7,2]         → 2200[2,7]（烧伤→未触发补疲惫）
//     （37853 平铺 4 参、链计 3 参：尾参 [2] 为加载器容忍的未消费余量，实测 load_skill_effects
//      只发 3 条链参数——场景 A 腿锁实切面）
//   37928 无言之浅眠   [856, 1974, 2200]|[3,30,8]         → 2200[30,8]（沉默→未触发补睡眠）
//  4399 交叉验证（reference/scrape_4399/out/pets_full.jsonl，2026-11-23 现查）：
//   4726「令双方烧伤，任意一方未触发则额外令对手失明」/ 4742「…疲惫」/ 4761「令双方沉默，
//   任意一方未触发则额外令对手睡眠」/ 4658「令双方流血，任意一方未触发则额外令对手诅咒」
//   ——{0}/{1} 均为官方异常码，无概率子句 → 申报 chance=100（j_purge_round 1370 同款）。
//
// ── 三查（2026-11-23 现查）──
//  ①内核硬编码：grep src/ include/ 2104/2200 零命中（battle_primitives 反射路径为通用
//    通道，非 id 硬编码）；②effect_meta 无两 id 行（effect_des 也无行——模板权威=
//    effect_info）；③全库无既有注册（SKILL_EFFECT(2104/2200) 零命中）。effect_icon 2104
//    行 = 魂印空间异 id（icon_id 2104 / effect_id 2501 / 宠 4894 黄金圣徒伙伴，K466 同款
//    甄别）：soul_lib 无 2104/2200 运行时实现 → 无键段交叠、免子块拆分（K481 728 判据）。
//    无 900000 拆分迁移、无退役记录（tasklog/工单现查）。
//
// ── 2104 通道甄别（K489 1935 blocked 分析勘明）──
//  回合制免疫反弹 = 内核原生在位通道，非 K489 的次数制缺口：
//    · 免疫面 = Modern 层 ANOMALY 回合票（grant_immunity counts=0 窗口条目，1381/1091 直系）；
//    · 反弹面 = rule_center_.grant_reflect 回合票（REFLECT 类，remaining_rounds 随回合
//      tick 递减）；apply_anomaly_checked [3] 免疫命中分支内原生 depth=1 反弹——携带
//      原异常码+原时长（K489 duration-native 探针 rc=0 实证）、镜像双方互弹不乒乓
//      （K489 mirror-native 探针 rc=0 实证：被弹回异常仍被对侧免疫挡下、不消耗）。
//      〔源: include/effects/rule_center.h:1073〕〔源: src/primitives/battle_primitives.cpp:287〕
//    · K496 1981 的 ATTACK_NULLIFY 类型门+双重计时缺口属攻击无效化通道，与本通道无关。
//  对手提升门 = **存储面**任一正槽（ability_levels[opp][slot] > 0 扫描，K490 2171 / 779
//    j_gaia_stance 同面直系；读存储不读视图——1767 视面族反例记档见 K490 头注）。
//  "{0}回合内" = InRounds 含施放当回合（1381 直系：immunity 窗 register_round=r0、
//    reflect 票 tick 到期，两者覆盖窗 r0..r0+{0}-1 对齐；"下{0}回合"才用 d5_D7 起算偏移）。
//  离场清理 = ON_STAGE 票随换宠 epoch 作废（K457/K461 同口径），场景 J 腿实证。
//
// ── 2200 语义甄别 ──
//  "令双方{0}"：对自身与对手**各独立申报一次** apply_anomaly_checked（2226/1850 双侧
//    独立判定的施加版）。施加顺序 = 先自身后对手（建模选择记档：模板无序语；反射交互下
//    两侧终态相同，仅 DURATION_EXTENDED 归属差异，无官方实测面）。
//  "任意一方未触发"：申报制返回面甄别 = j_purge_round anomaly_landed 同表——SUCCESS/
//    REPLACED_EXISTING/DURATION_EXTENDED/CONVERTED = 落地；免疫/被挡/抗性/弹回/掷歪 =
//    未触发（REFLECTED 保守档=未触发，1367 landed==0 同款取向）。chance=100 申报下
//    ROLL_FAILED 结构性不可达，与 K457"宽口径"张力不显化。
//  "额外令对手{1}"：任意一侧未落地即对**对手**补 {1} 一次（K457 条件追加直系）；
//    双侧都未触发也只补一次（"任意一方"=或门、非逐侧补）。
//  时长：模板无时长子句 → random_anomaly_duration()（家族口径，常量 2）。
//
// ── 显示面 ──
//  两 id 均为一次性 SKILL_EFFECT（无 duration 桶）；驻留态 = 2104 的 RuleCenter
//  IMMUNE/REFLECT 票（fullstate ruleTickets sourceEffectId=2104 可导出可解析，G2 断言
//  在场景）；2200 无驻留显示态（异常态走 abnormalStates 固定表）。审计槽为战斗账本
//  （随 ctx 析构，非效果状态，K490 同款）：
//    2104（1021040000）：slot 0 = 门面（1=开门已授票 / 0=提升门闭 / 2=守卫早退）；
//    2200（1022000000）：slot 0 = 自身落地（0/1）、1 = 对手落地（0/1）、2 = 补施加（0/1）、
//      3/4 = {0}/{1} 行参回显、5 = {0} 申报侧次序标记（0=先自身后对手）。
//  新增键 1021040000/1022000000 全库 grep 零命中（2026-11-23 现查）；soul 空间无运行时
//  实现 → 无键段交叠（K481 728 判据）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

namespace {

constexpr int k2104AuditBase = 1021040000;  // 10^9 + 2104×10^4
constexpr int k2200AuditBase = 1022000000;  // 10^9 + 2200×10^4

inline void set_audit(BattleContext* ctx, int base, int owner, int slot, int value) {
    ctx->plugin_storage[base + owner * 16 + slot] = value;
}

// "未触发"判定：异常**落地**才算触发（j_purge_round anomaly_landed 同表直系）。
inline bool anomaly_landed(ApplyAnomalyResult r) {
    switch (r) {
        case ApplyAnomalyResult::SUCCESS:
        case ApplyAnomalyResult::REPLACED_EXISTING:
        case ApplyAnomalyResult::DURATION_EXTENDED:
        case ApplyAnomalyResult::CONVERTED:
            return true;
        default:
            return false;
    }
}

// ── 2104 —— 对手不处于能力提升则{0}回合内免疫并反弹受到的异常状态 ──
// 行参布局 [owner, opp, rounds]（skills.cpp 加 [owner,target] 前缀）。
// 门 = 对手 ability_levels 存储面任一正槽（K490 2171 翻倍门同面）；开门 = 1381/1091
// 同款双授（Modern 层 ANOMALY 回合票 + grant_reflect 回合票），InRounds 含当回合。
EffectResult effect_2104_unboosted_reflect_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2104, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 2104, args,
                     "2104 布局=[owner,opp,回合]（int_count>=3）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp == 1 - owner,
                     2104, args, "需要 owner/opp 为对向双方");
    EFFECT_ARG_GUARD(rounds > 0, 2104, args, "需要 回合 > 0（在位即消费，禁缺省改默认）");

    // 生存门：对手已倒 → 不授窗（K490 178 口径）。
    if (ctx->getPet(opp).hp <= 0) {
        set_audit(ctx, k2104AuditBase, owner, 0, 2);
        return EffectResult::kOk;
    }
    // 提升门：对手存储面任一正槽 → 闭门不授（K490 2171/779 同面）。
    for (int slot = 0; slot < BattleContext::kAbilityLevelSlotCount; ++slot) {
        if (ctx->ability_levels[opp][slot] > 0) {
            set_audit(ctx, k2104AuditBase, owner, 0, 0);
            return EffectResult::kOk;
        }
    }
    // ⚠️ source_id 传 0（匿名句柄）：grant_immune 路径①按 source_id 匹配**不校验
    // source_owner**——两侧同传 2104 会在镜像局互相吞票（侧 1 授予被侧 0 既有票刷新
    // 吞掉，E 镜像腿实证）。匿名句柄走路径②覆盖键 (source_owner, source_effect_id)
    // → 双侧各占一条、同侧重施放=刷新不叠加（K489 1935 探针 native 同形）。
    ctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                        /*duration_rounds=*/rounds, /*source_id=*/0,
                        /*soul_immunity=*/false, EffectScope::ON_STAGE,
                        /*counts=*/0, /*source_effect_id=*/2104,
                        /*tier=*/ImmunityTier::Modern);
    ctx->rule_center_.grant_reflect(owner, /*source_slot=*/-1, /*source_effect_id=*/2104,
                                    /*target=*/owner, /*rounds=*/rounds);
    set_audit(ctx, k2104AuditBase, owner, 0, 1);
    return EffectResult::kOk;
}

// ── 2200 —— 令双方{0}，任意一方未触发则额外令对手{1} ──
// 行参布局 [owner, opp, 异常码{0}, 异常码{1}]。双侧独立申报（先自身后对手，建模选择见
// 文件头）；申报制返回面甄别未触发 → 对手补 {1} 一次（K457 条件追加直系）。
EffectResult effect_2200_dual_apply_then_fallback(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2200, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 2200, args,
                     "2200 布局=[owner,opp,异常码0,异常码1]（int_count>=4）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int code0 = args.int_args[2];
    const int code1 = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp == 1 - owner,
                     2200, args, "需要 owner/opp 为对向双方");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(code0) && is_valid_abnormal_status_id(code1),
                     2200, args, "异常码非法");

    set_audit(ctx, k2200AuditBase, owner, 3, code0);
    set_audit(ctx, k2200AuditBase, owner, 4, code1);
    set_audit(ctx, k2200AuditBase, owner, 5, 0);
    // 生存门：对手已倒 → 整条跳过（K490 口径；对亡者施加/补偿均无意义）。
    if (ctx->getPet(opp).hp <= 0) {
        set_audit(ctx, k2200AuditBase, owner, 0, -1);
        set_audit(ctx, k2200AuditBase, owner, 1, -1);
        set_audit(ctx, k2200AuditBase, owner, 2, -1);
        return EffectResult::kOk;
    }
    if (!g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }

    // 双侧独立申报（先自身后对手）。chance=100：模板无概率子句，闸门管道统一管辖
    // （j_purge_round 1370 同款口径）。
    const int duration = random_anomaly_duration();
    const ApplyAnomalyResult r_self = g_core->apply_anomaly_checked(
        ctx, owner, code0, duration, /*actor=*/owner, /*chance_pct=*/100, ChanceSource::Skill);
    const ApplyAnomalyResult r_opp = g_core->apply_anomaly_checked(
        ctx, opp, code0, duration, /*actor=*/owner, /*chance_pct=*/100, ChanceSource::Skill);
    const bool self_landed = anomaly_landed(r_self);
    const bool opp_landed = anomaly_landed(r_opp);
    set_audit(ctx, k2200AuditBase, owner, 0, self_landed ? 1 : 0);
    set_audit(ctx, k2200AuditBase, owner, 1, opp_landed ? 1 : 0);

    // 任意一方未触发 → 额外令对手{1}（或门，只补一次；K457 条件追加直系）。
    const bool fallback = !self_landed || !opp_landed;
    set_audit(ctx, k2200AuditBase, owner, 2, fallback ? 1 : 0);
    if (fallback) {
        (void)g_core->apply_anomaly_checked(ctx, opp, code1, random_anomaly_duration(),
                                            /*actor=*/owner, /*chance_pct=*/100,
                                            ChanceSource::Skill);
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 本单注册（2 实现；无记档跳过）──
SKILL_EFFECT(2104, effect_2104_unboosted_reflect_window)
SKILL_EFFECT(2200, effect_2200_dual_apply_then_fallback)
