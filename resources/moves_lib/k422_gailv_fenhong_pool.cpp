// K422 · 效果 917「命中后{0}%使对手{1}，未触发则附加自身最大体力值1/{2}的百分比伤害」
// ——概率参数化施加 + 未触发粉伤兜底（场景 test/scenario/scenario_8050_prob_anomaly_pink.cpp；
//  号段 8050-8059，2026-11-07，基线 main-local cc5f6ab1）
//
// ══ 现查（2026-11-07，本 worktree DB 快照）══
// 官方模板（effect_info 917，args_num=3，param=[1,1,1]）：
//   「命中后{0}%使对手{1}，未触发则附加自身最大体力值1/{2}的百分比伤害」
// side_effect.arg_count=3 与 args_num 一致。{1}=battle_effects 异常码（0麻痹/2烧伤/
// 4寄生/6害怕/7疲惫——本族出现的全部码位）。载体 9 条，切片（json_each 顺序切分）
// sum(arg_count)==len(arg) 全自洽零剩余：
//   19513 炼神乾坤灾   [917]            [50,7,3]     P=50 疲惫 /3
//   26443 深渊之炎     [917,693,843]    [100,2,3,…]  P=100 烧伤/3
//   29084 巨兽蔑视     [191,917,1236]   [4, 50,6,2,…] P=50 害怕/2
//   29171 卡布卡布！   [1932,917,1984]  [4,0, 80,0,3,…] P=80 麻痹/3
//   30712 异域次元斩   [913,917]        [50, 50,7,3]  P=50 疲惫/3
//   31240 魔法仆从     [1055,917]       [1, 35,0,4]   P=35 麻痹/4
//   31934 爆裂篝火     [917]            [50,2,3]      P=50 烧伤/3
//   37377 星光·粉末击打 [917,1000]      [100,4,4, …]  P=100 寄生/4
//   37789 天灯何其乐   [917,1821,1072]  [100,2,3, …]  P=100 烧伤/3
// sp_hide_moves(_bisaifu) 的 %917% 命中为 move id 子串噪声（29170 含"917"），非效果引用。
//
// ⚠️ 立板行参翻案记档：盘点行写「附加自身1/{2}体力固定伤害」，官方模板原文是
//   「**百分比伤害**」→ 兜底走 deal_pink_damage(PERCENT_VALUE)（粉伤管线：吃百分比
//   抗性/护罩，1236/1072/1574 同通道），不是 fixed_damage。以 effect_info 为权威。
//
// ══ 四查（2026-11-07）══
// ① 内核硬编码：`(^|[^0-9.])917([^0-9.]|$)` 对 src/ include/ 0 命中；
// ② effect_info 917 在列 args_num=3 = side_effect.arg_count=3；custom_effect_programs /
//    custom_effect_overrides 零命中；
// ③ 全库并集无注册：resources/（moves_lib+soul_lib）grep SKILL_EFFECT(917/SOUL_PROGRAM(917
//    零命中；全分支（main-local/feat/k422/feat/k423/night-fuzz/sbs-win*）git grep 零命中；
//    boss_invalid_effects.json 含 917 但该文件 namespace=soul（effect 证据索引，
//    granularity=effect_id_evidence_index_not_whole_soul_disable），与 moves 空间
//    SKILL_EFFECT 同号不相干；
// ④ 废弃 parser 覆盖面（冻结不扩，只记录）：917 模板的 StatusInflict 主句
//    「命中后{0}%使对手{1}」命中 parse_status_inflict_primary 的 "使对手{" pattern
//    （src/effects/effect_unit_parser.cpp:59），兜底子句「附加…百分比伤害」不在
//    parse_fallback_unit 三型内 → parser 半解析（只施加异常、无兜底）——J129 实测
//    37377 "917 被 parser 解析活、100% 上寄生"（j_hit_roll_fallback.cpp:12-16）。
//    本注册落地后 clone_effect 命中插件函数、parser 路径不再驱动（skills.cpp:671
//    只在 effect.logic 为空时走 parse_effect_unit），9 条载体的半解析静默误执行一并清账。
//
// ══ 实现形态（同形套路照抄 + 通道翻案记档）══
// 直系同形先例 = 1000「命中则{0}%使对手{1}，未触发则附加{2}点固定伤害」双件：
//   j_prob_fallback4.cpp:60-64（SKILL_EFFECT 桶申报制 + tri-state landed 取补 + 兜底）、
//   j_hit_roll_fallback.cpp:14（同口径 AFTER_ACTION 变体）。派发文本字面写
//   「roll_percent 闸（申报 r）」，本号按 **申报制落地不本地自掷骰**，翻案记档三条依据：
//   ① 施加腿有一等公民申报通道 apply_anomaly_checked（battle_primitives.h:82-96：
//     ">=0 = 申报：原语掷骰前先问闸门改写再掷一次；>=100 必定触发不耗 rand"）——
//     套路目录 §零「禁止私造等价机制（…自掷骰等）」；j410 的本地掷点是因为
//     heal_amount 没有申报参数（被迫），本号原语可申报、不重蹈；
//   ② [2.2] 概率闸门管道（battle_primitives.cpp:244-268）：申报制才受亮节族闸门管辖、
//     才发 EVENT_ANOMALY_ATTACH_LOW_CHANCE（≤50% 控场曝线，梅赫维特永沐线 2026-11-06）；
//     自掷 + 未申报(chance=-1) 应用会结构性漏管该事件面；
//   ③ 100% 短路不耗抽取性质在原语内成立（>=100 直接过门不掷，与 roll_percent 契约
//     一致）——派发要求的性质不因走申报制而丢失。
// 边界可观测性：原语掷骰恰耗一抽（0<final<100），场景按 MINSTD 闭式钉种子断言
//   r=P-1 拒 / r=P 过 / r=0 过 / P=100 不掷（scenario_8050 T1-T5、T10）。
// 兜底腿：deal_pink_damage(ctx, opp, max_hp_of(owner)/denom, PERCENT_VALUE, owner)
//   （core_api.h:113「插件造粉伤的唯一入口」；值=自身最大体力整除分母，1236
//   selection_seal.cpp:349-352 / 1072 batch_c_skills.cpp:893 同式）。
// landed 判据（J165 j165_anomaly_landed / J129 j129_landed 同三档）：
//   SUCCESS / REPLACED_EXISTING / DURATION_EXTENDED = 触发；其余（ROLL_FAILED 掷歪 /
//   TARGET_IMMUNE 免疫 / RESISTED_BY_RESISTANCE 抗性 / CONVERTED 转化 / REFLECTED 弹回 /
//   BLOCKED_BY_EFFECT / TARGET_DEFEATED）一律=未触发 → 走兜底。弹回（REFLECTED）按族
//   口径不特判（弹控局 917 视为未触发照常兜底，记档待实战对账）。
// 生存门（opp.hp<=0 整条跳过，J129 2230 探针 2295 口径"对尸体施加/补刀无意义"
//   j_hit_roll_fallback.cpp:64；与 1296/410 的 hp<=0 门同形——作用对象换成本效果
//   全链的两个动作：对尸不施异常、对尸不补粉伤）。
// 守卫（2026-09-28 EFFECT_ARG_GUARD 定稿，J165 1000 同款）：布局 [owner,opp,P,码,denom]
//   （int_count>=5）、owner/opp ∈{0,1}、P∈[0,100]、denom>0、码 is_valid_abnormal_status_id。
//   P=0 合法（永不触发 → 恒兜底，410「P=0 永不触发」族口径；与 J165 1000 的
//   chance>0 守卫不同——本号派发场景规范把 0 列为边界，按 410 口径收）。
// 时点：默认路由 SKILL_EFFECT 桶（skills.cpp effect_register_state/default_branch_for_effect
//   对未列 id 的 default）= HIT 分支、伤害管线前——「命中后」族标准桶（miss/被盔
//   execute() 提前 return，桶不跑，J129 记档同）。
//
// ══ 审计槽（无随机精确断言口径，F9 782 首创 / J334 410 同式）══
// 键段 = 1,009,170,000（10^9 + 917×10^4）+ owner×16 + slot（新增前已跑
// tools/check_storage_keys.py 全库无撞）：
//   slot 0  申报 P（{0} 原值）
//   slot 1  申报异常码（{1}）
//   slot 2  申报分母（{2}）
//   slot 3  闸门改写后实际概率（rewrite_anomaly_chance 纯查询，无票时==P；
//           原语掷骰前问的是同一个值，battleContext.h:1358「两边共用这一道闸门」）
//   slot 4  最近施加结果码（ApplyAnomalyResult int；-1 = 本次未施加——目标已倒/守卫）
//   slot 5  最近 landed（1/0；三档判据见上）
//   slot 6  最近兜底粉伤量（名义 max_hp/denom；0 = 未兜底）
//   slot 7  landed 次数累计
//   slot 8  兜底次数累计
//   slot 9  最近拒绝原因（0 无 / 1 目标已倒整条跳过）
#include "common.h"

#include <abnormal-system/abnormal-types.h>
#include <utils/sbs_rand.h>

namespace {

constexpr int k917AuditBase = 1009170000;

inline int key917(int owner, int slot) {
    return k917AuditBase + owner * 16 + slot;
}

int storage_int_local(BattleContext* ctx, int key, int def = 0) {
    const auto it = ctx->plugin_storage.find(key);
    if (it == ctx->plugin_storage.end()) {
        return def;
    }
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : def;
}

void set_storage_int_local(BattleContext* ctx, int key, int value) {
    ctx->plugin_storage[key] = value;
}

// 申报制"落地"三档（j165_anomaly_landed / j129_landed 同口径；匿名空间不跨 TU，就地重写）。
bool k422_landed(ApplyAnomalyResult r) {
    return r == ApplyAnomalyResult::SUCCESS
        || r == ApplyAnomalyResult::REPLACED_EXISTING
        || r == ApplyAnomalyResult::DURATION_EXTENDED;
}

}  // namespace

// ═══════════════════════════════════════════════════════════════════════════
// 917 —— 命中后{0}%使对手{1}，未触发则附加自身最大体力值1/{2}的百分比伤害
// 行参布局 [owner, opp, P, anomaly, denom]（DB 三参 {0}=P {1}=码 {2}=denom；
// build_effect_args_for_skill 加 [0,1] 前缀，skills.cpp:608-615）。
// ═══════════════════════════════════════════════════════════════════════════
EffectResult effect_917_prob_anomaly_pink(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 917, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 917, args,
                     "917 布局=[owner,opp,概率,异常码,分母]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int chance = args.int_args[2];
    const int anomaly = args.int_args[3];
    const int denom = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     917, args, "需要 owner/opp ∈ {0,1}");
    EFFECT_ARG_GUARD(chance >= 0 && chance <= 100, 917, args, "需要 概率 ∈ [0,100]");
    EFFECT_ARG_GUARD(denom > 0, 917, args, "需要 分母 > 0");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 917, args, "需要 异常码 ∈ [0,43]");
    if (!g_core || !g_core->apply_anomaly_checked || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }

    // 生存门（2295 口径）：目标已倒 → 整条跳过（不施异常、不补粉伤）。
    if (ctx->getPet(opp).hp <= 0) {
        set_storage_int_local(ctx, key917(owner, 4), -1);
        set_storage_int_local(ctx, key917(owner, 5), 0);
        set_storage_int_local(ctx, key917(owner, 6), 0);
        set_storage_int_local(ctx, key917(owner, 9), 1);
        return EffectResult::kOk;
    }

    set_storage_int_local(ctx, key917(owner, 0), chance);
    set_storage_int_local(ctx, key917(owner, 1), anomaly);
    set_storage_int_local(ctx, key917(owner, 2), denom);
    // 闸门改写后实际概率（纯查询、幂等——原语 [2.2] 掷骰前问的是同一个值）。
    const int final_pct = ctx->rewrite_anomaly_chance(opp, owner, chance,
                                                      ChanceSource::Skill);
    set_storage_int_local(ctx, key917(owner, 3), final_pct);
    set_storage_int_local(ctx, key917(owner, 9), 0);

    // 申报制掷点（原语内：0<final<100 恰耗一抽；<=0 / >=100 不耗）。
    const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
        ctx, opp, anomaly, random_anomaly_duration(), owner, chance, ChanceSource::Skill);
    set_storage_int_local(ctx, key917(owner, 4), static_cast<int>(r));
    if (k422_landed(r)) {
        set_storage_int_local(ctx, key917(owner, 5), 1);
        set_storage_int_local(ctx, key917(owner, 6), 0);   // 互斥：触发侧兜底量置 0
        set_storage_int_local(ctx, key917(owner, 7),
                              storage_int_local(ctx, key917(owner, 7), 0) + 1);
        return EffectResult::kOk;
    }
    set_storage_int_local(ctx, key917(owner, 5), 0);
    // 未触发兜底：附加自身最大体力值 1/{2} 的百分比伤害（粉伤管线，PERCENT_VALUE）。
    const int nominal = max_hp_of(ctx, owner) / denom;
    set_storage_int_local(ctx, key917(owner, 6), nominal);
    set_storage_int_local(ctx, key917(owner, 8),
                          storage_int_local(ctx, key917(owner, 8), 0) + 1);
    if (nominal > 0) {
        (void)g_core->deal_pink_damage(ctx, opp, nominal, DamageKind::PERCENT_VALUE, owner);
    }
    return EffectResult::kOk;
}

SKILL_EFFECT(917, effect_917_prob_anomaly_pink)
