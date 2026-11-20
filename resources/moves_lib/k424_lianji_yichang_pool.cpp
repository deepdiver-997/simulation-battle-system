// K424 · 效果 1141「1回合做{0}~{1}次攻击，每次攻击{2}%令对手{3}，
// 攻击低于{4}次则每次攻击{5}%令对手{6}」——连击逐段异常 + 低击数概率补偿分支
// （场景 test/scenario/scenario_8070_combo_perhit_anomaly.cpp；号段 8070-8079，
//  2026-11-07，基线 main-local 33b83e8a）
//
// ══ 现查（2026-11-07，本 worktree DB 快照）══
// 官方模板（effect_info 1141，args_num=7，param=[1,3,3,1,6,6]）：
//   「1回合做{0}~{1}次攻击，每次攻击{2}%令对手{3}，攻击低于{4}次则每次攻击{5}%令对手{6}」
// side_effect.arg_count=7 与 args_num 一致。{3}/{6}=battle_effects 异常码
// （本族出现的全部码位：0麻痹/1中毒/2烧伤/6害怕/7疲惫/16流血/20失明/22焚烬/23诅咒，
//  全部 ∈ [0,43] 合法段）。载体 11 条，切片（json_each 顺序切分）sum(arg_count)
//  ==len(arg) 全自洽零剩余：
//   30502 青鸾炽羽阵   [48,1141]        [3, | 2,6,20,22,4,40,22]   2~6段 20%/40% 焚烬
//   32274 异次元剑阵   [1141,897,67]    [| 1,6,25,7,4,50,7 | 1,50 | 3]   1~6段 25%/50% 疲惫
//   32401 年少万兜鍪   [1292,1141]      [2, | 2,6,20,7,4,40,7]     2~6段 20%/40% 疲惫
//   32685 神佑破光斩   [1224,195,1141]  [2, 2, | 2,6,20,20,4,40,20] 2~6段 20%/40% 失明
//   33637 鸢·冥天惊神  [1141,835,672]   [| 2,6,20,6,4,40,6]        2~6段 20%/40% 害怕
//   34484 极夜幻象光   [1141,582,826]   [| 2,6,20,23,4,40,23 | 1 | 1] 2~6段 20%/40% 诅咒
//   34746 鳗鳗冲锋     [1141,918]       [| 3,6,20,1,5,30,1 | 300,1,50] 3~6段 20%/30% 中毒
//   36342 火花闪电     [1141]           [| 30,40,1,0,35,2,0]       30~40段 1%/2% 麻痹
//   36592 庆典礼炮     [1141]           [| 50,100,1,2,75,2,2]      50~100段 1%/2% 烧伤
//   37668 晶爆         [1141,412]       [| 5,10,30,16,7,60,16 | 2] 5~10段 30%/60% 流血
//   38439 幽影连击     [1141,1290]      [| 10,20,5,6,15,5,16 | 2,2] 10~20段 5%/5% 害怕/流血
//
// ══ 语义裁定（分支选读，2026-11-07）══
// 「攻击低于{4}次则每次攻击{5}%令对手{6}」= **分支替换**，不是叠加：
//   每次落地的攻击恰好掷一次，实际连击数 N < {4} → 用 ({5}%,{6})；N ≥ {4} → 用
//   ({2}%,{3})。三条依据：
//   ① 11 载体里 10 条 {3}=={6} 且 {5}>{2}（同异常概率补偿 20→40/25→50/1→2…），
//     叠加读法会得到复合概率（20%+40%-8%=52%）——文案写的是"则每次攻击40%"，
//     单值条件概率语义，不是双掷；
//   ② 38439 幽影连击（异双条 5%害怕/5%流血）官方 4399 图鉴文案与模板逐字同形：
//     「1回合做10~20次攻击，每次攻击5%令对手害怕；若攻击低于15次，则每次攻击5%
//     令对手流血」（news.4399.com/seer/jinglingdaquan/zhandouanying/202606-30-
//     1013595.htm）——"若…则…"的条件句式下两支互斥，N≥15 害怕、N<15 流血；
//   ③ 1141 在内核已按连击登记（custom_skill_combo_rules 行 1141|0|1「1回合做
//     {0}~{1}次攻击（原 case 1141）」，J326 数据化）：N = 每次使用掷一次
//     （skills.h roll_combo_count），伤害=一次公式×N（battleFsm.cpp:745-748）——
//     "攻击低于{4}次"读的正是这个 N（resolvedDamage.hitCount）。
//
// ══ 四查（2026-11-07）══
// ① 内核硬编码：`(^|[^0-9.])1141([^0-9.]|$)` 对 src/ include/ 仅 1 处——
//    effect_meta.cpp:42 纯元数据标注（EffectCategory::Combo，"次数已实现"），
//    无行为硬编码；连击次数本身走 DB custom_skill_combo_rules（J326 数据化，
//    内核无 id 开关表）；
// ② effect_info 1141 在列 args_num=7 = side_effect.arg_count=7；custom_effect_
//    programs / custom_effect_overrides 零命中；
// ③ 全库并集无注册：resources/（moves_lib+soul_lib）grep SKILL_EFFECT(1141 /
//    SOUL_PROGRAM(1141 零命中；registry.json moves=1455/soul=411 无 1141；
//    全分支（main-local/feat/k424/feat/k425/origin/main）git grep 零命中；
//    boss_invalid_effects.json ids 含 1141（line 391）——该文件 namespace=soul
//    （effect 证据索引，granularity=effect_id_evidence_index_not_whole_soul_disable，
//    consumer_migration=not_implemented），与 moves 空间 SKILL_EFFECT 同号不相干，
//    且场景框架不走 boss 预填路径，记档不处理；
// ④ 废弃 parser 覆盖面（冻结不扩，只记录）：1141 模板的子句「每次攻击{2}%令对手{3}」
//    命中 parse_status_inflict_primary 的 "令对手{" pattern（effect_unit_parser.cpp:70
//    起，find_last_percent_placeholder 回溯到 {2}%）——parser 半解析成"每次使用
//    一次性 chance=args[2] 施加 anomaly=args[3]"（忽略多段与低击数分支，K422
//    同款"半解析静默误执行"形态）；兜底子句无"未触发则"→ 无 fallback 挂链。
//    本注册落地后 clone_effect 命中插件函数、parser 路径不再驱动（skills.cpp:671
//    只在 effect.logic 为空时走 parse_effect_unit），11 条载体的半解析静默误执行
//    一并清账。
//
// ══ 实现形态（同形套路照抄：j_combo_face.cpp 1172 直系 + K422 申报制口径）══
// - 次数腿：**不实现**——内核连击通道已有（custom_skill_combo_rules 1141 行 +
//   roll_combo_count + 一次公式×N），插件不重复挂 combo_count_resolver（静态区间
//   roll 由 resolve_skill_execution 物化，battleFsm.cpp:517）。
// - 逐段腿：ws.combo_hit_hook[owner]（battleWorkspace.h:478-482：红伤落地后按
//   resolvedDamage.hitCount 循环调用、用后即焚；final<=0 整段被挡不触发——
//   "每次攻击"判据=实际落地的攻击段，1454/1172 同款）。钩子内按 N<thr 分支选
//   (chance, anomaly)，走 apply_anomaly_checked 申报制（ChanceSource::Skill）：
//   掷骰在原语内、受概率闸门管辖（K422 翻案三条依据照抄，不本地自掷骰）；
//   duration=random_anomaly_duration()（=2，族常量，1172 同款）。
// - target=resolvedDamage.defenderId（钩子现场读，钩子只在攻击方槽上、defender
//   即本刀防守方）；生存门不另加：TARGET_DEFEATED 由原语挡（dead 目标 apply
//   返回 TARGET_DEFEATED），钩子仅在 final>0 时被内核调用、对尸体无攻击段可言。
// - 守卫（EFFECT_ARG_GUARD，K422/J165 同款）：布局 [owner,opp,下限,上限,概率1,
//   异常码1,阈值,概率2,异常码2]（int_count>=9）、owner/opp ∈{0,1} 且不等、
//   概率 ∈[0,100]、异常码 is_valid_abnormal_status_id。**阈值不守卫**——
//   "在位即消费"：thr<=0 时 (N<0) 恒假 → 恒走第一支，即文案的退化语义本体
//   （K314/K340 pct 关闸档教训：>0 守卫会改写显式数据的可观察语义）。
// - 双段概率独立申报：两支都是"每次攻击掷一次"，由 apply_anomaly_checked
//   按次申报（无 100% 载体；0<ch<100 每段恰耗一抽——场景钉种子可复核）。
//
// ══ 审计槽（无随机精确断言口径，K422 917 / J334 410 同式）══
// 键段 = 1,011,410,000（10^9 + 1141×10^4）+ owner×16 + slot（新增前已跑
// tools/check_storage_keys.py 全库无撞）：
//   slot 0  申报概率1（{2}）        slot 1  申报异常码1（{3}）
//   slot 2  申报阈值（{4}）        slot 3  申报概率2（{5}）
//   slot 4  申报异常码2（{6}）
//   slot 5  最近一次有落地攻击使用的 N（钩子视角 resolvedDamage.hitCount；
//           -1 = 本次使用无落地攻击——handler 进场即重置）
//   slot 6  最近一次使用的分支（0=N>=thr 走第一支 / 1=N<thr 走第二支 /
//           -1 = 无落地攻击）
//   slot 7  逐段施加尝试次数累计（=落地段总数）
//   slot 8  landed 次数累计（SUCCESS/REPLACED_EXISTING/DURATION_EXTENDED 三档，
//           K422 同判据）
//   slot 9  最近一次施加结果码（ApplyAnomalyResult int；-1 = 本次使用尚未施加）
//   slot 10 最近一次传给原语的异常码（分支选择后的实际值——38439 异双条
//           害怕/流血选择的直接观测位）
//   slot 11 最近一次传给原语的概率（分支选择后的实际值）
//   slot 12 TARGET_IMMUNE 结果计数累计（掷点过门才到免疫层——ROLL_FAILED 不到，
//           与 landed 互补的可观测位：免疫局 immune==过门掷数）
#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int k1141AuditBase = 1011410000;

inline int key1141(int owner, int slot) {
    return k1141AuditBase + owner * 16 + slot;
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

// 申报制"落地"三档（k422_landed 同口径；匿名空间不跨 TU，就地重写）。
bool k424_landed(ApplyAnomalyResult r) {
    return r == ApplyAnomalyResult::SUCCESS
        || r == ApplyAnomalyResult::REPLACED_EXISTING
        || r == ApplyAnomalyResult::DURATION_EXTENDED;
}

}  // namespace

// ═══════════════════════════════════════════════════════════════════════════
// 1141 —— 1回合做{0}~{1}次攻击，每次攻击{2}%令对手{3}，攻击低于{4}次则每次
//        攻击{5}%令对手{6}
// 行参布局 [owner, opp, 下限, 上限, 概率1, 异常码1, 阈值, 概率2, 异常码2]
// （DB 七参 {0}~{6}；build_effect_args_for_skill 加 [0,1] 前缀）。
// 次数腿在内核连击通道；本插件只挂逐段异常钩子（分支替换语义，见头注裁定）。
// ═══════════════════════════════════════════════════════════════════════════
EffectResult effect_1141_combo_perhit_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1141, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 9, 1141, args,
                     "1141 布局=[owner,opp,下限,上限,概率1,异常码1,阈值,概率2,异常码2]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int chance1 = args.int_args[4];
    const int anomaly1 = args.int_args[5];
    const int threshold = args.int_args[6];
    const int chance2 = args.int_args[7];
    const int anomaly2 = args.int_args[8];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1141, args, "需要 owner/opp ∈ {0,1}");
    EFFECT_ARG_GUARD(chance1 >= 0 && chance1 <= 100 && chance2 >= 0 && chance2 <= 100,
                     1141, args, "需要 两支概率 ∈ [0,100]");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly1)
                         && is_valid_abnormal_status_id(anomaly2),
                     1141, args, "需要 两支异常码 ∈ [0,43]");

    // 申报位写入 + 本次使用钩子观测位重置（handler 在 SKILL_EFFECT 桶、伤害前）。
    set_storage_int_local(ctx, key1141(owner, 0), chance1);
    set_storage_int_local(ctx, key1141(owner, 1), anomaly1);
    set_storage_int_local(ctx, key1141(owner, 2), threshold);
    set_storage_int_local(ctx, key1141(owner, 3), chance2);
    set_storage_int_local(ctx, key1141(owner, 4), anomaly2);
    set_storage_int_local(ctx, key1141(owner, 5), -1);
    set_storage_int_local(ctx, key1141(owner, 6), -1);
    set_storage_int_local(ctx, key1141(owner, 9), -1);
    set_storage_int_local(ctx, key1141(owner, 10), -1);
    set_storage_int_local(ctx, key1141(owner, 11), -1);

    if (!g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    // 逐段钩子：红伤落地后按 N 循环调用（battleFsm.cpp:917-928），用后即焚。
    // 每段恰好申报一次：N<thr → (概率2,异常码2)；否则 (概率1,异常码1)。
    ctx->ws.combo_hit_hook[owner] = [owner, chance1, anomaly1, threshold, chance2,
                                     anomaly2](BattleContext* c, int) {
        const int defender = c->resolvedDamage.defenderId;
        if (defender < 0 || defender > 1) {
            return;
        }
        const int hits = c->resolvedDamage.hitCount;
        const bool low = hits < threshold;
        const int chance = low ? chance2 : chance1;
        const int anomaly = low ? anomaly2 : anomaly1;
        set_storage_int_local(c, key1141(owner, 5), hits);
        set_storage_int_local(c, key1141(owner, 6), low ? 1 : 0);
        set_storage_int_local(c, key1141(owner, 10), anomaly);
        set_storage_int_local(c, key1141(owner, 11), chance);
        set_storage_int_local(c, key1141(owner, 7),
                              storage_int_local(c, key1141(owner, 7), 0) + 1);
        const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
            c, defender, anomaly, random_anomaly_duration(), owner, chance,
            ChanceSource::Skill);
        set_storage_int_local(c, key1141(owner, 9), static_cast<int>(r));
        if (r == ApplyAnomalyResult::TARGET_IMMUNE) {
            set_storage_int_local(c, key1141(owner, 12),
                                  storage_int_local(c, key1141(owner, 12), 0) + 1);
        }
        if (k424_landed(r)) {
            set_storage_int_local(c, key1141(owner, 8),
                                  storage_int_local(c, key1141(owner, 8), 0) + 1);
        }
    };
    return EffectResult::kOk;
}

SKILL_EFFECT(1141, effect_1141_combo_perhit_anomaly)

// ══ 留续单（2026-11-07 本单实测记档，R 单素材）══
// 内核勘误候选：连击技 miss 后仍结算伤害。query_usage ① 命中 roll 正常判 MISS，
// 但 handle_*_AttackDamage 无效出口 run_attack_damage_from_invalid_skill（battleFsm.cpp:991）
// 被 variable_power_requested（battleFsm.cpp:296-300）的 `combo_view > 1` 项放行——
// **静态连击基数**（resolve_skill_execution 引擎打底物化，非效果改写）也命中该条件，
// 导致 miss 的多段技仍走 stage+finish 全额结算（单击技 N==1 不受影响；拍打 0/20 vs
// 手工 combo 界 20/20 实测隔离）。修复方向 = 条件排除"未改写的静态连击"（rewritten
// 项语义）。src/ 本单不动（派发纪律），1141 逐段钩子在 miss 路径本就不触发（本单
// 场景 8070 T7 已锁"钩子不触发"口径），不影响本效果正确性。
