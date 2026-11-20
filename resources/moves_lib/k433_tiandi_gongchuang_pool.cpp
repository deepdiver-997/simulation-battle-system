// K433 · 天敌概率翻倍 + 条件攻击窗两宏 —— 640「命中后{0}%使对手{1}{2}回合,
// 遇到天敌概率翻倍」×7 + 1339「若对手不处于异常状态则下{0}回合攻击{1}%令对手{2}」×7
//（场景 test/scenario/scenario_8190_tiandi_gongchuang.cpp；号段 8190-8199，
//  2026-11-07，基线 main-local e8bf5597，registry moves=1478 / soul=411）
//
// ══ 撞族协调记档（派发单红线：K434 并行同清单）══
// 开工时（2026-11-07 上午）跨 worktree grep：/Users/zhuhongrui/.agent-workspaces/
//   workspaces/sbs-local/k434 目录**不存在**、无 feat/k434 分支（tasklog 2633 行
//   K434 立项于 10:44:29，状态待领取）——对方占用面不可 grep，按"K430 头注公开
//   推介过的候选让给对方、自取头注未推介族"避让：本单取 640+1339（K430 头注
//   排除清单未提及），**让出 1325×7**（K430 头注明文"后续低垂果实候选"，K434 若
//   读头注大概率取它）；收单前复查 k434 worktree 是否已开工有占（若有占且同族，
//   以先 commit 者为准记档）。兄弟 worktree（agents/night-fuzz / endgame-walk / r8）
//   git grep SKILL_EFFECT(640,/SKILL_EFFECT(1339,/kMark640/kMark1339 全零命中。
//
// ══ 重新对账（author 协议探索二批，方法 = K430 同款）══
// registry moves=1478（本 worktree build/registry.json，基线 main-local e8bf5597）对
//   scripts/data/processed/seer_unity.sqlite moves 表 side_effect / friend_side_effect
//   两列 id 列表并集盘点（27307 行；args 在并列 side_effect_arg 列、按
//   side_effect.arg_count 平面切片；friend-only 独有 id 5 个 = 14/15/33/186/402，
//   全为 side_effect 已引用空间子集口径）。结果：**被引用效果 id 2294 个**（与 K430
//   记档 2294 完全一致），已注册 1478，**未实现 818 族 / 1997 条引用**（全部 ≤12
//   载体，最大族仍 2190×13）。
// 与 K430 盘点（registry 1470，未实现 824 族 / 2031 引用）对账：
//   新引用 +1（2273，J335 expansion 补丁，已注册不入未实现）；中间消化 6 族 35 引用
//   = 2132(K429) + 1079/1722(K430) + 184/418(K431) + 42(K432)；2031 +1 -35 = 1997 ✓、
//   824 - 6 = 818 ✓；registry 1470→1478 = +8（其中 2 个为 moves 表零引用 id，
//   不动未实现族数）。
//
// ══ 选族理由（候选池 = 派发 7 载体清单 16 id + 5-6 载体池）══
// 排除判据沿 K430 头注（随机 N 选族/经济类/无模板行/选技期/状态机依赖/1538）：
//   - 445×7 经济类（K430 已排除）；1146×7 计数语义歧义（同）；2028×7 天敌+必中跨
//     两通道（同，且本单 640 已占天敌判据通道）；1565×7 随机 PP 选族（同）。
//   - 1325×7 通道全备但 K430 头注公开推介 → 让 K434（见协调记档）。
//   - 130×7「对方为{0}则附加{1}点伤害」：{0}=属性码需属性码→系别权威映射考证 +
//     2 混链（898 未注册），留下一批；其余 7 载体候选（1339/1459/1575/1627/1645/
//     1799/1855/1863/2075）逐一体量评估：1627 随机攻击次数 + 秒杀通道、1799/1863
//     纯链不足（2/7、1/7）、1645/1855 双窗复合语义待拆、1459/1575 先后手门+技能无效
//     通道体量大——留后续批次。
// 选中 640+1339（"攻击附加异常"同域双宏，14 载体全实现即两族清账）：
//   ① **纯链 14/14**（勘误记档 2026-11-11：首查把 chain 串 "R" 误读作未注册——671/677
//     实际均在册，f_misc_family.cpp:517 / round_window.cpp:489）：仅有的两处"跨族
//     同链"为良性交互——33372 的 671「若对手处于异常状态则恢复造成伤害的{0}%体力」
//     条件恰由 640 的落地喂真（AFTER_ACTION 桥按 resolvedDamage 折算回血，场景 T4
//     直证 341×50/100=170）；37072 的 677「{0}回合内每回合使用技能吸取对手能力提升
//     状态」在 V5 盘面（对手无强化）为 no-op。
//   ② 通道零新增：is_predator（common.h 一等公民助手，428/1488/selection_seal
//     在册同款）+ apply_anomaly_checked 申报制（K422 917 / K430 1079 / J3 同款）+
//     has_any_active_abnormal_status 条件（188 在册同款）+ NextRounds 双探针
//     AFTER_ACTION 攻击窗（J22 592 直系同形）——四件全是既有设施，语义逐字可映射。
//   ③ args 自洽：side_effect.arg_count 640=3 / 1339=3 与 effect_info.args_num 一致，
//     14 载体平面切片 sum(arg_count)==len(arg) 零剩余零串位（脚本核算 2026-11-07）。
//
// ══ 现查（2026-11-07，本 worktree DB 快照）══
// 官方模板（effect_info，side_effect.arg_count 与之相等）：
//   640（args_num=3）「命中后{0}%使对手{1}{2}回合,遇到天敌概率翻倍」
//   1339（args_num=3）「若对手不处于异常状态则下{0}回合攻击{1}%令对手{2}」
// {1}（640）/{2}（1339）=battle_effects 异常码（0麻痹/6害怕/7疲惫/15冰封/27感染，
//   registry anomalies 44 码全覆盖；592 载体 16149 {2}=0 麻痹先例——0 是合法码位）。
// 天敌判据（common.h:114 is_predator，用户 2026-09-13 官方实测口径）：
//   对手**固有属性**克制自身固有属性 >1（不看技能系别/不看视图，双属性组合相乘）。
//   "遇到天敌"方向 = 技能使用者遇到（428「遇到天敌时附加固伤」j 组 e_predator_cond_amp
//   is_predator(ctx, owner) 在册同款；克制表 types_relation grass→water=2.0 直证）。
// 640 载体 7 条切片（json 解析、side_effect.arg_count 平面切片，7/7 自洽零剩余）：
//   17379 冰锥刺     [640]           [25,15,2]  P=25 冰封15 /2回合（物80）
//   23497 异次元战吼 [29,640]        [250, 40,7,2]  P=40 疲惫 /2（属性技；29 已注册）
//   28393 波光粼粼   [48,640,770,1046] [4, 30,7,3, 3]  P=30 疲惫 /**3 回合**（属性技；
//                                                      48/770/1046 全已注册）
//   33062 花狮奔     [766,640]       [3,300, 50,2,2]  P=50 烧伤 /2（物85；766 已注册）
//   33372 冥花绽放   [640,671]       [35,27,2, 50]  P=35 感染 /2（特150；671 已注册
//                     f_misc_family.cpp:517——条件由 640 落地喂真，良性同链）
//   35486 漫地扫射   [468,640]       [30,6,2]  P=30 害怕 /2（物85；468 已注册）
//   37646 永恒之铭   [640]           [50,7,2]  P=50 疲惫 /2（物140）
// 1339 载体 7 条切片（7/7 自洽零剩余）：
//   26910 可怖诅咒   [626,1339,58]   [3,1, 3,50,6, 2]  /3回合 50% 害怕（属性技；
//                     626/58 已注册）
//   28057 合万载     [191,792,1339]  [4, 2,100,6]  /2回合 100% 害怕（属性技）
//   28945 穷崖覆雪   [48,2068,1339,843] [4,100,5, 2,100,15, 3,1]  /2回合 100% 冰封
//                     （属性技）
//   31925 魅影无光破 [1099,752,1339] [1, 3, 1,100,7]  /1回合 100% 疲惫（物160）
//   35781 沧流唤海灵 [979,188,1339]  [2,1, 2,100,15]  /2回合 100% 冰封（特160）
//   36123 无生叹     [1339]          [3,100,6]  /3回合 100% 害怕（物130）
//   37072 维生反应   [1214,1339,677] [250, 2,50,0, 2]  /2回合 50% **麻痹0**（物150；
//                     677 已注册 round_window.cpp:489——盘面对手无强化时 no-op）
// 条件语义（1339「若对手不处于异常状态」）：激活时点求值（模板语序 = 条件从句在
//   "下{0}回合"之前，同 1079"命中后…未触发则…"整句激活形解析）；判据 =
//   has_any_active_abnormal_status(opp) 取反（e_predator_cond_amp 188 在册同款）。
//
// ══ 四查（2026-11-07）══
// ① 内核硬编码：`(^|[^0-9.])640([^0-9.]|$)` / 同款 1339 对 src/ include/ 均 **0 命中**
//   （grep rc=1 实测）；无行为硬编码。
// ② effect_info 640/1339 在列（args_num=3/3）= side_effect.arg_count 一致；
//   custom_effect_programs（6 行）/ custom_effect_overrides（17 行）按 effect_id 查
//   640/1339 **零命中**。
// ③ 全库并集无注册：resources/ test/ 内 SKILL_EFFECT(640,/SKILL_EFFECT(1339,（精确
//   逗号边界）与 kMark640/kMark1339 零命中；兄弟 worktree（agents/night-fuzz /
//   endgame-walk / r8）git grep 同款零命中；k434 worktree 不存在（见协调记档）——真缺口。
// ④ 废弃 parser 覆盖面（冻结不扩，只记录——**两模板均非全惰性，属"半解析"**，注册
//   落地后 clone_effect 命中插件函数、parser 路径不再驱动（src/entities/skills.cpp:675
//   只在 effect.logic 为空时走 parse_effect_unit），半解析行为随之消灭）：
//   - 640「命中后{0}%使对手{1}{2}回合,…」：主刀 pattern「使对手{」命中（status_ph={1}，
//     2321 堵煞守卫查 close 后紧跟"回合"——本模板 {1} 后是 {2}，守卫不拦）→ 现状产出
//     Anomaly(码={1}, duration=**硬编码 2**, chance={0}) 单元：缺天敌翻倍 + 28393 的
//     {2}=3 被吞。注册后改为精确语义（翻倍 + {2} 驱动时长）。
//   - 1339「若…则下{0}回合攻击{1}%令对手{2}」：主刀 pattern「令对手{」命中（status_ph=
//     {2}，{2} 位于句尾守卫不拦）→ 现状产出 Anomaly(**立即单次**, chance={1}, 码={2},
//     duration=2) 单元：把"下{0}回合攻击窗"误作立即附加、激活条件被吞。注册后改为
//     精确语义（条件 + NextRounds 窗）。
//
// ══ 实现形态（同形套路照抄，通道零新增）══
// 640 直系同形先例 = **968**（shengguang_gelaorui_skills.cpp:153「命中后{0}%使对手{1}，
//   先出手时概率翻倍」：条件翻倍本地折算 → 施加），仅条件词不同（先出手→天敌）与
//   时长参数化（968 固定 1 回合 → 本族 {2}）：
//   · 时点 = SKILL_EFFECT（effect_register_state default = BATTLE_FIRST_SKILL_EFFECT，
//     miss/被盔桶不跑）；"命中后"字面。
//   · 概率 = 翻倍后**申报制**（K430 1079 同式）：declared = min(2×{0},100)（天敌）/
//     {0}（非天敌）申报给 apply_anomaly_checked——闸门改写在原语掷骰前、"以实际概率
//     为准"判申报值；0<final<100 恰耗一抽、<=0/>=100 不耗（battle_primitives.cpp:262
//     契约）。**不走本地 roll**：本族是"异常状态附加"概率，亮节闸门管辖面
//     （ChanceSource::Skill），与 K431 184"概率强化本地掷点"不同域。
//   · 时长 = {2} 显式驱动（apply_anomaly_checked duration_rounds 直传；28393 {2}=3
//     场景直证）。
//   · 生存门（K430 1079 / K422 2295 口径）：目标已倒 → 整条跳过（audit reason=1）。
// 1339 直系同形先例 = **592**（j_attr_response_window.cpp:82「下{0}回合每回合使用
//   攻击技能{1}%令对手{2}」：FIRST/SECOND_AFTER_ACTION 双探针 NextRounds 窗 +
//   executed_skill_this_round/executed_skill_is_attack 门 + 申报制施加），前置条件
//   「对手不处于异常状态」= 188 判据取反（has_any_active_abnormal_status）。
//   · 窗界 = "下{0}回合" NextRounds（本回合不算，起点 R+1；592 同款）。
//     ⚠️ tick 侧 start 下界守卫（J171 j_window_fixed:40 同款坑记档）：NextRounds 起点
//     只挪过期锚、不挡注册当回合——AFTER_ACTION 桶晚于 SKILL_EFFECT，攻击技开窗
//     当回合 tick 会先跳一次；payload 带 start、tick 判 roundCount >= start 才生效。
//   · 窗内施加时长 = random_anomaly_duration()（=2，battle_primitives.h:67 常量；
//     592 同款——模板无显式时长参数）。
//   · 掷骰位置差记档：592 窗内 tick 每跳一抽（官方逐回合逐攻击判定）；激活侧不掷
//     （激活只判条件、不开掷）。
//   · 生存门：激活时目标已倒 → 整条跳过（reason=2）；窗内 tick 由原语 TARGET_DEFEATED
//     自拦（[2] 检查先于掷骰、零抽取消耗，audit 照记）。
// 事件中心不防未来锚点（J25 记档）：NextRounds 注册面自带窗界，无需自卡区间。
//
// ══ 审计槽（无随机精确断言口径，F9 782 首创 / K430 同式）══
// 640 键段 = 1,000,640,000（10^9 + 640×10^4）+ owner×16 + slot（新增前已跑
//   tools/check_storage_keys.py 全库无撞）：
//   slot 0  申报 P（{0} 原始）           slot 1  申报异常码（{1}）
//   slot 2  申报回合数（{2}）            slot 3  天敌观测（1=遇到天敌 / 0=否）
//   slot 4  翻倍后申报概率（declared）   slot 5  闸门改写后实际概率（final）
//   slot 6  原语返回码（ApplyAnomalyResult int）
//   slot 7  落地计数（SUCCESS/REPLACED_EXISTING/DURATION_EXTENDED 才 +1）
//   slot 8  最近拒绝原因（0 无 / 1 目标已倒）
// 1339 键段 = 1,003,390,000（10^9 + 1339×10^4）+ owner×16 + slot（同上无撞）：
//   slot 0  申报回合数（{0}）            slot 1  申报概率（{1}）
//   slot 2  申报异常码（{2}）            slot 3  条件观测（1=对手无异常 / 0=有）
//   slot 4  窗口注册起点（NextRounds start；条件假/目标已倒=0）
//   slot 5  窗内到达施加点次数（tick 过攻击技门才 +1）
//   slot 6  窗内落地计数（landed 才 +1）
//   slot 7  最近拒绝原因（0 无 / 1 激活条件假 / 2 激活时目标已倒 / 3 原语 TARGET_DEFEATED）

#include "common.h"

#include <algorithm>

#include <primitives/battle_primitives.h>

namespace {

constexpr int k640AuditBase = 1000640000;   // 10^9 + 640×10^4
constexpr int k1339AuditBase = 1003390000;  // 10^9 + 1339×10^4

inline int key640(int owner, int slot) { return k640AuditBase + owner * 16 + slot; }
inline int key1339(int owner, int slot) { return k1339AuditBase + owner * 16 + slot; }

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

inline bool landed(ApplyAnomalyResult r) {
    return r == ApplyAnomalyResult::SUCCESS
        || r == ApplyAnomalyResult::REPLACED_EXISTING
        || r == ApplyAnomalyResult::DURATION_EXTENDED;
}

// ═══════════════════════════════════════════════════════════════════════════
// 640 —— 命中后{0}%使对手{1}{2}回合,遇到天敌概率翻倍
// 行参布局 [owner, opp, P, 异常码, 回合]（DB 三参 {0}{1}{2}；skills.cpp 加 [0,1] 前缀）。
// ═══════════════════════════════════════════════════════════════════════════
EffectResult effect_640_hit_predator_double_anomaly(BattleContext* ctx,
                                                    const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 640, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 640, args,
                     "640 布局=[owner,opp,概率,异常码,回合]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int chance = args.int_args[2];
    const int anomaly = args.int_args[3];
    const int rounds = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     640, args, "需要 owner/opp ∈ {0,1}");
    EFFECT_ARG_GUARD(chance >= 0 && chance <= 100, 640, args, "需要 概率 ∈ [0,100]");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 640, args, "需要 异常码 ∈ [0,43]");
    EFFECT_ARG_GUARD(rounds > 0, 640, args, "需要 回合数 > 0");
    if (!g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }

    // 生存门（K430 1079 口径）：目标已倒 → 整条跳过。
    if (ctx->getPet(opp).hp <= 0) {
        set_storage_int_local(ctx, key640(owner, 8), 1);
        return EffectResult::kOk;
    }

    set_storage_int_local(ctx, key640(owner, 0), chance);
    set_storage_int_local(ctx, key640(owner, 1), anomaly);
    set_storage_int_local(ctx, key640(owner, 2), rounds);

    // 天敌概率翻倍（968 同款条件翻倍形，条件换 is_predator）：翻倍后申报。
    const bool predator = is_predator(ctx, owner);
    const int declared = predator ? std::min(chance * 2, 100) : chance;
    set_storage_int_local(ctx, key640(owner, 3), predator ? 1 : 0);
    set_storage_int_local(ctx, key640(owner, 4), declared);
    // 闸门改写后实际概率（纯查询、幂等——原语 [2.2] 掷骰前问的是同一个值）。
    const int final_pct = ctx->rewrite_anomaly_chance(opp, owner, declared,
                                                      ChanceSource::Skill);
    set_storage_int_local(ctx, key640(owner, 5), final_pct);

    // 申报制掷点（原语内：0<final<100 恰耗一抽；<=0 / >=100 不耗）。时长 = {2} 显式。
    const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
        ctx, opp, anomaly, rounds, owner, declared, ChanceSource::Skill);
    set_storage_int_local(ctx, key640(owner, 6), static_cast<int>(r));
    if (landed(r)) {
        set_storage_int_local(ctx, key640(owner, 7),
                              storage_int_local(ctx, key640(owner, 7), 0) + 1);
        set_storage_int_local(ctx, key640(owner, 8), 0);
    } else {
        set_storage_int_local(ctx, key640(owner, 8), 0);
    }
    return EffectResult::kOk;
}

// ── 1339 的 AFTER_ACTION 窗 tick —— payload={owner, opp, chance, anomaly, start}
// （592 j_attr_response_window 同款双探针形：桶按行动方执行，双侧注册每回合恰一次）。
// ⚠️ start 守卫（J171 j_window_fixed 同款坑）：ContinuousEffect 的 NextRounds 起点只
//   挪**过期锚**（isExpired 按 registered+duration），不挡"注册当回合"的执行——
//   AFTER_ACTION 桶在 SKILL_EFFECT **之后**跑，攻击技开窗当回合 tick 就会先跳一次。
//   "下{0}回合"语义 = tick 只在 roundCount >= start（= 武装回合+1）才生效。
EffectResult k433_1339_attack_window_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int chance = args.int_args[2];
    const int anomaly = args.int_args[3];
    const int start = args.int_args[4];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    if (ctx->roundCount < start) {
        return EffectResult::kOk;   // 武装当回合不算（"下{0}回合"字面）
    }
    // "每回合使用攻击技能"：本回合真用了技能且是攻击技（物理/特殊）——592 同门判据。
    if (!executed_skill_this_round(ctx, owner) || !executed_skill_is_attack(ctx, owner)) {
        return EffectResult::kOk;
    }
    if (!g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    set_storage_int_local(ctx, key1339(owner, 5),
                          storage_int_local(ctx, key1339(owner, 5), 0) + 1);
    const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
        ctx, opp, anomaly, random_anomaly_duration(), owner, chance, ChanceSource::Skill);
    if (landed(r)) {
        set_storage_int_local(ctx, key1339(owner, 6),
                              storage_int_local(ctx, key1339(owner, 6), 0) + 1);
        set_storage_int_local(ctx, key1339(owner, 7), 0);
    } else if (r == ApplyAnomalyResult::TARGET_DEFEATED) {
        set_storage_int_local(ctx, key1339(owner, 7), 3);
    }
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// 1339 —— 若对手不处于异常状态则下{0}回合攻击{1}%令对手{2}
// 行参布局 [owner, opp, 回合, 概率, 异常码]（DB 三参 {0}{1}{2}；skills.cpp 加 [0,1] 前缀）。
// ═══════════════════════════════════════════════════════════════════════════
EffectResult effect_1339_noanomaly_attack_window(BattleContext* ctx,
                                                 const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1339, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1339, args,
                     "1339 布局=[owner,opp,回合,概率,异常码]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int chance = args.int_args[3];
    const int anomaly = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1339, args, "需要 owner/opp ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0, 1339, args, "需要 回合数 > 0");
    EFFECT_ARG_GUARD(chance >= 0 && chance <= 100, 1339, args, "需要 概率 ∈ [0,100]");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 1339, args,
                     "需要 异常码 ∈ [0,43]");
    if (!g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }

    set_storage_int_local(ctx, key1339(owner, 0), rounds);
    set_storage_int_local(ctx, key1339(owner, 1), chance);
    set_storage_int_local(ctx, key1339(owner, 2), anomaly);

    // 生存门（K430 1079 口径）：目标已倒 → 整条跳过。
    if (ctx->getPet(opp).hp <= 0) {
        set_storage_int_local(ctx, key1339(owner, 3), 0);
        set_storage_int_local(ctx, key1339(owner, 4), 0);
        set_storage_int_local(ctx, key1339(owner, 7), 2);
        return EffectResult::kOk;
    }

    // 激活条件 = 对手不处于异常状态（188 判据取反；激活时点求值——模板语序整句激活形）。
    const bool cond = !ctx->has_any_active_abnormal_status(opp);
    set_storage_int_local(ctx, key1339(owner, 3), cond ? 1 : 0);
    if (!cond) {
        set_storage_int_local(ctx, key1339(owner, 4), 0);
        set_storage_int_local(ctx, key1339(owner, 7), 1);
        return EffectResult::kOk;
    }

    // "下{0}回合" NextRounds 窗（本回合不算）：AFTER_ACTION 双探针，592 同构；
    // payload 带 start 供 tick 做下界守卫（见 tick 注释）。start 与注册面同式预算
    // （NextRounds = roundCount+1，battleContext.h:992）。
    const int start = ctx->round_effect_start_round(owner, rounds,
                                                    EffectWindowKind::NextRounds);
    register_next_rounds_effect(
        ctx, State::BATTLE_FIRST_AFTER_ACTION, owner, 1339, rounds,
        {owner, opp, chance, anomaly, start}, &k433_1339_attack_window_tick);
    register_next_rounds_effect(ctx, State::BATTLE_SECOND_AFTER_ACTION, owner, 1339,
                                rounds, {owner, opp, chance, anomaly, start},
                                &k433_1339_attack_window_tick);
    set_storage_int_local(ctx, key1339(owner, 4), start);
    set_storage_int_local(ctx, key1339(owner, 7), 0);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(640, effect_640_hit_predator_double_anomaly)
SKILL_EFFECT(1339, effect_1339_noanomaly_attack_window)
