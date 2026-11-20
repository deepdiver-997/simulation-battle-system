// K431 · 对手能力提升条件族两宏 —— 184「若对手处于能力提升状态，则{1}%自身{0}等级+{2}」×6
// + 418「若对手处于能力提升状态则对方{0}等级{1}」×6
//（场景 test/scenario/scenario_8160_opp_boost_cond_stat.cpp；号段 8160-8169，
//  2026-11-07，基线 feat/k431 起点 21dc3af8（hub registry moves=1475 / soul=411））
//
// ══ 现查（2026-11-07，hub DB 快照 scripts/data/processed/seer_unity.sqlite）══
// 官方模板（effect_info，side_effect.arg_count 与之相等：184=3 / 418=2）：
//   184（args_num=3）「若对手处于能力提升状态，则{1}%自身{0}等级+{2}」
//   418（args_num=2）「若对手处于能力提升状态则对方{0}等级{1}」
// 「对手处于能力提升状态」条件读法（同门先例统一判据）：对手**本体**六槽
//   （BattleContext::kAbilityLevelSlotCount=6，含命中，battleContext.h:449）任一
//   **正等级**（负等级/零不算提升）——779（j_gaia_stance.cpp:251）/2298（:234）/
//   1361/1897（j_purge_success2.cpp:80-87）/1158/1178（d5_D6.cpp:27，962 同判据，
//   用户 2026-09-18 口径）逐字同判，读 ctx->ability_levels[opp][i] > 0。
// 载体 12 条全量切片（side_effect/friend_side_effect 全 27307 行 JSON 解析、按
//   side_effect.arg_count 平面切片，12/12 sum(arg_count)==len(arg) 零剩余零串位）：
//   184×6：
//     13022 蓝光海啸   [184]           [2,100,1]  特攻+1（特殊150）
//     13265 白蝠飞天弑 [184]           [4,100,1]  速度+1（物理150）
//     15495 创世之神谕 [184]           [0,100,1]  攻击+1（物理150）
//     16620 耀武诛邪破 [674,184,184]   [100, 0,100,1, 4,100,1] 攻+1 与 速+1 双腿
//                   （**混链**：674「攻击结束时恢复{0}点体力」未注册、保持 no-op——
//                     J334 17603 / K430 36351 混链同口径记档，不越族代做）
//     38404 胡涂       [1465,184]      [1, 0,60,1] 60% 攻+1（物理85；1465 已注册
//                   j_crit_gates.cpp:144，先手 ACTION_START 掷 50% 必暴）
//   418×6（{1} 全为负 → 全是弱化腿）：
//     13421 瞬域影杀   [418] [5,-1]  命中-1（特殊75）
//     13444 金刚一指   [418] [1,-1]  防御-1（物理70）
//     13480 异水之环   [418] [4,-2]  速度-2（特殊130）
//     15236 流星一闪   [418] [4,-2]  速度-2（物理80）
//     33126 魔影袭     [418] [5,-2]  命中-2（物理120）
//     33914 冰晶干扰   [418] [5,-2]  命中-2（特殊130）
// 纯链 11/12：唯一混链 = 16620（674 未注册，见上）。
//
// ══ {0} 能力码序取证 ══
// **官方码序**（0攻 1防 2特攻 3特防 4速 5命中）→ 引擎槽 {0,2,1,3,4,5}（防特攻互换
// 是映射的全部意义；常识速查"属性排列双序"坑）：
//   · 184 与 182「若对手处于{0}状态，{2}%自身{1}等级+{3}」（j_anomaly_gate_self_boost.cpp
//     直系同门，已实现）模板句式逐字同构，码序必须同读；
//   · 正面证据：蓝光海啸（特殊150）{0}=2 读**特攻**+1 才自洽（引擎序直读会是防+1）；
//     418 的 {0}=5 → 命中-1/-2 三载体（瞬域影杀/魔影袭/冰晶干扰）语义直证；
//   · 对手弱化侧同序先例 = 501（j_low_damage_stat_drop.cpp:13「{1} 是官方码序」、
//     :42 k501OfficialCodeToSlot 同表）。
//
// ══ 四查（2026-11-07）══
// ① 内核硬编码：`(^|[^0-9.])184([^0-9.]|$)` 对 src/ include/ **0 命中**；
//   418 同款 3 命中全为注释/异常码空间巧合（battle_primitives.cpp:304 与
//   rule_center.h:522 引官方机制文档 idx=418、abnormal-types.h:208 星赎(418)——
//   异常状态 id 418 ≠ 技能效果 418，两套 id 空间），无行为硬编码。
// ② effect_info 184/418 在列（args_num=3/2）= side_effect.arg_count 一致；
//   custom_effect_programs（6 行）/ custom_effect_overrides（17 行）按 effect_id
//   查 184/418 **零命中**。
// ③ 全库并集无注册：resources/ 内 SKILL_EFFECT(184,/SKILL_EFFECT(418,（精确逗号
//   边界）与 kMark184/kMark418 零命中；兄弟 worktree（agents / endgame-walk / k432）
//   git grep 同款零命中——真缺口。
// ④ 废弃 parser 覆盖面（冻结不扩，只记录）：两模板对
//   src/effects/effect_unit_parser.cpp **均全惰性**，无"半解析静默误执行"：
//   - 184「…则{1}%自身{0}等级+{2}」：主句四型 {"令对手{","令自身{","使对手{",
//     "使自身{"}（effect_unit_parser.cpp:75）要求"令/使+自身+{"，本模板是"{1}%自身{"
//     （百分号紧邻，无动词）→ 不中；第二刀 5 类条件模板（179/700/1083/1257/456，
//     :152-190）也不中 → parse_effect_unit 返回 -1，现状全惰性。
//   - 418「…则对方{0}等级{1}」：无令/使动词、"对方{"非 pattern、5 类条件模板
//     均不中 → 同为全惰性。
//   （两宏注册落地后 clone_effect 命中插件函数，parser 路径不再驱动
//     ——src/entities/skills.cpp:675 只在 effect.logic 为空时走 parse_effect_unit。）
//
// ══ 实现形态（同形套路照抄，通道零新增）══
// 184 直系同形先例 = **J240 182**（j_anomaly_gate_self_boost.cpp：「若对手处于{0}状态，
//   {2}%自身{1}等级+{3}」，仅条件词不同）：
//   · 时点 = SKILL_EFFECT（effect_register_state default = BATTLE_FIRST_SKILL_EFFECT，
//     src/entities/skills.cpp:184；miss/被盔桶不跑）；stat_change 同步 ws.view_levels，
//     本刀 ATTACK_DAMAGE 读视图 → 强化在本刀伤害前生效（J240 19682 直读口径记档）。
//   · 概率 = {1} 在位即消费：显式 0 关闸（不回落默认值）、100 直通（不耗抽取）、
//     其余 roll_percent 本地掷点（j_anomaly_gate_self_boost.cpp:82-86 同款）。
//     **不走 rewrite_anomaly_chance 申报**：亮节闸门只管"异常状态附加"概率
//     （rule_center.h:95 ChanceTag::AnomalyAttach 注释原文"概率强化、概率弱化不在
//     此列"）——概率强化类本地掷点是定版口径，不是漏迁移。
//   · 落点 = stat_change(自身, 槽, +{2}) 真原语（本体/视图同步；自身增益不吃免弱，
//     battle_primitives.h:483 契约；J240 :91 同款）。
// 条件判据 = 779/2298 同款本体六槽正等级（见现查节）。
// 418 直系同形先例 = **J238 501**（j_low_damage_stat_drop.cpp：「则对手{1}等级-{2}」
//   走 stat_drop(对手, 槽, 量, actor=owner) 弱化真原语，:82）：查免弱 STAT_DROP、
//   穿强化保护、钳 -6（battle_primitives.h:508 契约）。{1} 为**有符号**增量：
//   <0 = 弱化（stat_drop，全 6 载体此形态）；>0 = "对方{0}等级{1}"字面提升
//   （stat_change 原始通道，无免弱概念；现库无此形态载体，纯数据驱动备位）；
//   ==0 无语义 → EFFECT_ARG_GUARD 响亮报错（数据漂移口径，K430 同）。
//   时点同 184（SKILL_EFFECT、本刀伤害前生效——同门 182 通道语义一致）。
// 生存门（K430 1079 / K422 2295 口径）：目标已倒 → 整条跳过（184 条件体/418 落点
//   都挂在对手侧）；登记前时点窗口场景不可构造，同 J333/410 记档。
//
// ══ 审计槽（无随机精确断言口径，F9 782 首创 / K430 同式）══
// 184 键段 = 1,001,840,000（10^9 + 184×10^4）+ owner×16 + slot（新增前已跑
//   tools/check_storage_keys.py 全库无撞）：
//   slot 0  申报官方码（{0}）            slot 1  申报概率（{1}）
//   slot 2  申报增量（{2}）              slot 3  映射后引擎槽
//   slot 4  条件观测（1=对手有正等级 / 0=无）
//   slot 5  概率门结果（1=过 / 0=拒；100 直通也记 1）
//   slot 6  stat_change 返回码（StatChangeResult int，447 行枚举）
//   slot 7  成功变更次数累计（SUCCESS 才 +1）
//   slot 8  最近拒绝原因（0 无 / 1 条件假 / 2 概率拒 / 3 显式 0 关闸 / 4 目标已倒）
// 418 键段 = 1,004,180,000（10^9 + 418×10^4）+ owner×16 + slot（同上无撞）：
//   slot 0  申报官方码（{0}）            slot 1  申报有符号增量（{1}）
//   slot 2  映射后引擎槽                  slot 3  条件观测（1/0）
//   slot 4  原语返回码（StatDropResult/StatChangeResult int，均 0 起 SUCCESS）
//   slot 5  成功变更次数累计（SUCCESS 才 +1）
//   slot 6  最近拒绝原因（0 无 / 1 条件假 / 2 目标已倒）

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int k184AuditBase = 1001840000;   // 10^9 + 184×10^4
constexpr int k418AuditBase = 1004180000;   // 10^9 + 418×10^4

inline int key184(int owner, int slot) { return k184AuditBase + owner * 16 + slot; }
inline int key418(int owner, int slot) { return k418AuditBase + owner * 16 + slot; }

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

// 官方码序（0攻 1防 2特攻 3特防 4速 5命中）→ 引擎能力槽
// （0攻 1特攻 2防 3特防 4速 5命中）。防与特攻互换是本映射的全部意义
// （J240 :48 / J238 :42 同表）。
constexpr int kOfficialStatToSlot[6] = {0, 2, 1, 3, 4, 5};

// "对手处于能力提升状态" = 对手本体六槽（含命中）任一**正**等级
// （779 j_gaia_stance.cpp:251 / 1361/1897 j_purge_success2.cpp:80-87 同判据；
// 负等级不算提升，用户 2026-09-18 口径）。
bool has_any_positive_level(const BattleContext* ctx, int side) {
    if (!ctx || side < 0 || side > 1) {
        return false;
    }
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[side][i] > 0) {
            return true;
        }
    }
    return false;
}

}  // namespace

// ═══════════════════════════════════════════════════════════════════════════
// 184 —— 若对手处于能力提升状态，则{1}%自身{0}等级+{2}
// 行参布局 [owner, opp, 官方码, 概率, 增量]（DB 三参 {0}..{2}；skills.cpp 加 [0,1] 前缀）。
// ═══════════════════════════════════════════════════════════════════════════
EffectResult effect_184_opp_boost_cond_self_up(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 184, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 184, args,
                     "184 布局=[owner,opp,官方码,概率,增量]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int code = args.int_args[2];
    const int chance = args.int_args[3];
    const int levels = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     184, args, "需要 owner/opp ∈ {0,1}");
    EFFECT_ARG_GUARD(code >= 0 && code < 6, 184, args,
                     "官方码 " + std::to_string(code) + " 越界（0..5）");
    EFFECT_ARG_GUARD(chance >= 0 && chance <= 100, 184, args, "需要 概率 ∈ [0,100]");
    EFFECT_ARG_GUARD(levels > 0, 184, args, "需要 增量 > 0");

    const int slot = kOfficialStatToSlot[code];
    set_storage_int_local(ctx, key184(owner, 0), code);
    set_storage_int_local(ctx, key184(owner, 1), chance);
    set_storage_int_local(ctx, key184(owner, 2), levels);
    set_storage_int_local(ctx, key184(owner, 3), slot);
    set_storage_int_local(ctx, key184(owner, 8), 0);

    // 生存门（1079/2295 口径）：目标已倒 → 整条跳过。
    if (ctx->getPet(opp).hp <= 0) {
        set_storage_int_local(ctx, key184(owner, 8), 4);
        return EffectResult::kOk;
    }

    // 条件门：对手处于能力提升状态（本体六槽任一正等级）。
    const bool cond = has_any_positive_level(ctx, opp);
    set_storage_int_local(ctx, key184(owner, 4), cond ? 1 : 0);
    if (!cond) {
        set_storage_int_local(ctx, key184(owner, 8), 1);
        return EffectResult::kOk;
    }

    // 概率门：在位即消费——显式 0 关闸；100 直通不耗抽取；其余本地掷点
    // （概率强化不在亮节管辖面，rule_center.h:95；J240 182 同款）。
    if (chance <= 0) {
        set_storage_int_local(ctx, key184(owner, 5), 0);
        set_storage_int_local(ctx, key184(owner, 8), 3);
        return EffectResult::kOk;
    }
    if (!roll_percent(chance)) {
        set_storage_int_local(ctx, key184(owner, 5), 0);
        set_storage_int_local(ctx, key184(owner, 8), 2);
        return EffectResult::kOk;
    }
    set_storage_int_local(ctx, key184(owner, 5), 1);

    if (!g_core || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    const StatChangeResult r = g_core->stat_change(ctx, owner, slot, levels);
    set_storage_int_local(ctx, key184(owner, 6), static_cast<int>(r));
    if (r == StatChangeResult::SUCCESS) {
        set_storage_int_local(ctx, key184(owner, 7),
                              storage_int_local(ctx, key184(owner, 7), 0) + 1);
    }
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// 418 —— 若对手处于能力提升状态则对方{0}等级{1}
// 行参布局 [owner, opp, 官方码, 有符号增量]（DB 两参 {0}..{1}；skills.cpp 加 [0,1] 前缀）。
// ═══════════════════════════════════════════════════════════════════════════
EffectResult effect_418_opp_boost_cond_drop(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 418, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 418, args,
                     "418 布局=[owner,opp,官方码,有符号增量]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int code = args.int_args[2];
    const int delta = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     418, args, "需要 owner/opp ∈ {0,1}");
    EFFECT_ARG_GUARD(code >= 0 && code < 6, 418, args,
                     "官方码 " + std::to_string(code) + " 越界（0..5）");
    EFFECT_ARG_GUARD(delta != 0, 418, args, "需要 增量 ≠ 0（0 无语义，数据漂移响亮报错）");

    const int slot = kOfficialStatToSlot[code];
    set_storage_int_local(ctx, key418(owner, 0), code);
    set_storage_int_local(ctx, key418(owner, 1), delta);
    set_storage_int_local(ctx, key418(owner, 2), slot);
    set_storage_int_local(ctx, key418(owner, 6), 0);

    // 生存门（1079/2295 口径）：目标已倒 → 整条跳过。
    if (ctx->getPet(opp).hp <= 0) {
        set_storage_int_local(ctx, key418(owner, 6), 2);
        return EffectResult::kOk;
    }

    // 条件门：对手处于能力提升状态（与 184 同判据）。
    const bool cond = has_any_positive_level(ctx, opp);
    set_storage_int_local(ctx, key418(owner, 3), cond ? 1 : 0);
    if (!cond) {
        set_storage_int_local(ctx, key418(owner, 6), 1);
        return EffectResult::kOk;
    }

    // 有符号增量：{1}<0 = 弱化（stat_drop 真原语：查免弱 STAT_DROP、穿强化保护、
    // 钳 -6，actor=owner 如实声明——J238 501 同款）；{1}>0 = 字面提升（stat_change
    // 原始通道；现库无此形态载体，纯数据驱动备位）。
    if (delta < 0) {
        if (!g_core || !g_core->stat_drop) {
            return EffectResult::kOk;
        }
        const StatDropResult r = g_core->stat_drop(ctx, opp, slot, -delta, /*actor=*/owner);
        set_storage_int_local(ctx, key418(owner, 4), static_cast<int>(r));
        if (r == StatDropResult::SUCCESS) {
            set_storage_int_local(ctx, key418(owner, 5),
                                  storage_int_local(ctx, key418(owner, 5), 0) + 1);
        }
        return EffectResult::kOk;
    }
    if (!g_core || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    const StatChangeResult r = g_core->stat_change(ctx, opp, slot, delta);
    set_storage_int_local(ctx, key418(owner, 4), static_cast<int>(r));
    if (r == StatChangeResult::SUCCESS) {
        set_storage_int_local(ctx, key418(owner, 5),
                              storage_int_local(ctx, key418(owner, 5), 0) + 1);
    }
    return EffectResult::kOk;
}

SKILL_EFFECT(184, effect_184_opp_boost_cond_self_up)
SKILL_EFFECT(418, effect_418_opp_boost_cond_drop)
