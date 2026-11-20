// K430 · 先制兜底族两宏 —— 1079「命中后{0}%令对手{1}，未触发则下{2}回合自身攻击技能
// 先制+{3}」×7 + 1722「未打出致命一击则自身下{0}回合攻击技能额外先制+{1}」×6
//（场景 test/scenario/scenario_8150_xianzhi_fallback.cpp；号段 8150-8159，
//  2026-11-07，基线 main-local 38fb4e99，registry moves=1472 / soul=411）
//
// ══ 重新对账（author 协议探索，2026-11-07，对账先例 = J334 qburn 盘点）══
// 方法：registry moves=1472（build/registry.json，hub main-local 38fb4e99）对
//   scripts/data/processed/seer_unity.sqlite moves 表 side_effect / friend_side_effect
//   全量 27307 行 JSON 解析（含 404 行"单效果双参段"历史数据形态，参数按
//   side_effect.arg_count 平面切片）。结果：**被引用效果 id 2294 个**（friend 列 48 个
//   全为 side_effect 子集、零新增），已注册 1470，**未实现 824 族 / 2031 条引用**。
//   ≤12 载体小族 823 个；唯一 >12 载体族 = 2190×13。
// 与 qburn 2026-11-06 盘点（registry 1451，未实现 844 族 / 引用 2293 个）对账：
//   **中间恰好消化 20 族**——917(K422)、72/1946/1947(K423)、1141(K424)、2344+4 散号
//   (K426)、517/574/1625/2205(K427)、1561/1908/2121/2459(K428)、1774+2273(J335)，
//   两清单差 = 844-824 = 20 ✓（registry 1472-1451 = +21，其中 2273 系 J335 expansion
//   补丁新引用、不在本盘点差集内）；引用差 2293→2294 = +1（2273 进入 moves 引用空间）。
//   qburn 晨报"843 个 ≤12 载体小族"口径 = 844-2190×13，与本次 823+1 结构一致。
//
// ══ 选族理由（供后续批次复用）══
// 选中 1079+1722（"先制兜底"同通道双宏，13 载体全实现即两族清账）：
//   ① 纯链：1079 的 7 载体链上兄弟（1065/726/191/47/693/843/766/828/604/1244/717）
//     **全部已注册**；1722 的 6 载体 5 条纯链 + 36351 混链（1748 未实现、保持 no-op，
//     J334 17603 混链同口径记档）。
//   ② 通道零新增：apply_anomaly_checked 申报制（K422 917 直系）+ NextRounds MOVE_RIGHT
//     先制 tick（J226 1230 / J41 1549 / J61 1029 三先例同形）+ crit_happened 旗读
//     （E 组 844/553 直系）——三件都是既有一等公民，语义逐字可映射模板。
//   ③ args 自洽：side_effect.arg_count 1079=4 / 1722=2 与 effect_info.args_num 一致，
//     13 载体平面切片 sum(arg_count)==len(arg) 零剩余。
// 排除清单（≥5 载体可做而未做，理由记档防复踏）：
//   - 2190×13：effect_info 模板即裸"{0}"，语义不可得（盘点最大族，留内核/语义侧补证）。
//   - 1538×8：内核缺口（多班派发红线，勿立）。
//   - 445×7：赛尔豆战斗外经济，未建模概念。
//   - 41×7：effect_info **无行**（无模板权威），四查②不过。
//   - 2028×7：3/7 纯链不足 + 天敌判定+必中凭证横跨两通道，留后续。
//   - 1325×7：6/7 纯链、通道全备（must_crit），零参无载体差异——后续低垂果实候选。
//   - 75/846/851/1116/1498/1624/2074 系：「随机附加 N 种异常」需**随机选族原语**，
//     无一等公民（私造 = 套路目录红线）。
//   - 1565/1557：「随机 N 个技能 PP 归零」同上随机选族缺口。
//   - 1134/1359/1771/2226：能力下降反馈/转移/反转封锁通道无先例，留通道设计单。
//   - 616×6：3/6 纯链不足；634×6：4/6 纯链不足；1146×7：「每处于{0}种」计数语义歧义。
//   - 184×6+418×6：同门"对手处于能力提升状态"条件能力族，通道全备纯链 11/12——
//     **后续批次首选候选**（本号 2 宏已足，留作下一班低垂果实）。
//
// ══ 现查（2026-11-07，本 worktree DB 快照）══
// 官方模板（effect_info，side_effect.arg_count 与之相等）：
//   1079（args_num=4）「命中后{0}%令对手{1}，未触发则下{2}回合自身攻击技能先制+{3}」
//   1722（args_num=2）「未打出致命一击则自身下{0}回合攻击技能额外先制+{1}」
// {1}=battle_effects 异常码（本族全部码位 6害怕/7疲惫/8睡眠/16流血，registry anomalies
// 44 码全覆盖）。1079 载体 7 条切片（json_each 顺序切分）：
//   25239 任天驰骋   [1079,1065,726]  [80,7,2,1, …]   P=80 疲惫 /2回合/+1（属性技）
//   25967 仙尘旧念   [191,47,1079]    [4,4, 70,8,1,2] P=70 睡眠 /1回合/+2（属性技）
//   26755 邪魅一笑   [1079,693,843]   [80,7,2,2, …]   P=80 疲惫 /2/+2（属性技；
//                                                      链兄弟 843 自带先制窗——场景
//                                                      预防性回避，插件语义不受影响）
//   32836 霹雳破     [766,1079]       [3,200, 40,16,1,1] P=40 流血 /1/+1（物85）
//   33297 销魂神火印 [828,604,1079]   [150,300,30,300, 50,7,2,1] P=50 疲惫 /2/+1（物150）
//   34784 入梦铃铛   [1079,1244]      [40,8,2,1, …]   P=40 睡眠 /2/+1（物150）
//   37216 冥冥灭影阵 [1079,717]       [80,6,2,2, …]   P=80 害怕 /2/+2（物150）
// 1722 载体 6 条切片：
//   35326 掣空奔雷陨 [1381,1356,1722] [2, 3, 1,1]  N=1/+1（特160）
//   35484 炽光炎锋刃 [1484,1356,1722] [3,1, 1,1]   N=1/+1（物150）
//   36011 飒飒驰夜光 [879,553,1722]   [100,0, 1,1] N=1/+1（物160）
//   36351 白金碎星弹 [1164,1748,1722] [1, 1, 1,3]  N=1/+3（物160；1748 未实现混链）
//   36440 启·辉霆璨世[1840,1722,1928] [100, 2,1, 3] N=2/+1（物160）
//   36517 泯辉极明破 [1689,1722,933]  [1,1, 2,2, 1] N=2/+2（特150）
//
// ══ 四查（2026-11-07）══
// ① 内核硬编码：`(^|[^0-9.])1079([^0-9.]|$)` / 同款 1722 对 src/ include/ 均 0 命中；
// ② effect_info 1079/1722 在列（args_num=4/2）= side_effect.arg_count 一致；
//    custom_effect_programs / custom_effect_overrides 零命中（17/6 行全查）；
// ③ 全库并集无注册：resources/ grep SKILL_EFFECT(1079/1722、SOUL_PROGRAM( 同款零命中；
//    全 worktree（hub main-local / feat/k429 / night-fuzz / endgame-walk / sbs-win /
//    sbs-win-net）git grep 零命中；
// ④ 废弃 parser 覆盖面（冻结不扩，只记录）：
//    - 1079 主句「命中后{0}%令对手{1}」命中 parse_status_inflict_primary 的 "令对手{"
//      pattern（src/effects/effect_unit_parser.cpp:59）→ parser 半解析：只施加异常
//      （固定 duration=2）、先制兜底子句不在 parse_fallback_unit 三型内被静默丢弃
//      （effect_unit_parser.cpp:110-137 递归三型：递归异常/恢复/附加固定伤害）。
//      本注册落地后 clone_effect 命中插件函数、parser 路径不再驱动
//      （src/entities/skills.cpp:671 只在 effect.logic 为空时走 parse_effect_unit），
//      7 载体的"有异常无先制"半解析静默误执行一并清账。
//    - 1722 全文无 "令/使对手{"/"令/使自身{" pattern、也不在第二刀 5 类条件模板内
//      （179/700/1083/1257/456）→ parse_effect_unit 返回 -1，现状**全惰性**（无半解析）。
//
// ══ 实现形态（同形套路照抄 + 申报制裁定）══
// 1079 直系同形先例 = K422 917（申报制异常 + 未触发兜底，k422_gailv_fenhong_pool.cpp）：
//   SKILL_EFFECT 桶（effect_register_state default = BATTLE_FIRST_SKILL_EFFECT，
//   src/entities/skills.cpp:181-182；miss/被盔 execute() 提前 return 桶不跑，"命中后"
//   语义天然成立）→ apply_anomaly_checked 申报 {0}%（亮节闸门管辖、100% 短路不耗
//   抽取、0<final<100 恰耗一抽）→ landed 三档判据（SUCCESS/REPLACED_EXISTING/
//   DURATION_EXTENDED；ROLL_FAILED/TARGET_IMMUNE/RESISTED/CONVERTED/REFLECTED/
//   BLOCKED/TARGET_DEFEATED 一律未触发，K422 记档同口径）→ 未触发走 NextRounds 兜底。
// 兜底腿直系先例 = J226 1230 / J61 1029（NextRounds({N}) 的 MOVE_RIGHT tick 写
//   ws.preemptive_level，common.h register_next_rounds_effect；窗口起点 NextRounds 折
//   算，先/后手都从下一回合起算）+ J41 1549（"**攻击技能**"限定 = tick 内
//   selected_skill_is_attack 门，681 同款；1079/1722 模板均带"攻击技能"字样 → 必须带门，
//   1230 无"攻击"字样不带门是它的模板差异，非口径分歧）。
// 1722 直系同形先例 = E 组 844/553（crit_happened 在 query_usage ①.5 ON_SKILL_HIT 掷、
//   SKILL_EFFECT 时已可见；miss/打盔不进 SKILL_EFFECT → "打出"语义天然成立，
//   e_crit_trigger.cpp:8-11 定口径）→ 未打出（旗 false）→ NextRounds 先制窗。
// 异常时长：模板无时长字样 → random_anomaly_duration()（917/553/878 族约定同）。
// 生存门（K422 2295 口径）：目标已倒 → 整条跳过（不施异常、不注册先制窗）——登记前
//   时点窗口场景不可构造，同 J333/410 记档（只证"已登记死亡 no-op"侧）。
// 守卫（J165/K422 同款）：布局 1079=[owner,opp,P,码,回合,先制]（int_count>=6）、
//   1722=[owner,opp,回合,先制]（int_count>=4）、owner/opp ∈{0,1} 且不等、P∈[0,100]、
//   码 is_valid_abnormal_status_id、回合>0、先制量>0（13 载体全满足，数据漂移响亮报错）。
//   P=0 合法（永不触发 → 恒先制窗，410「P=0 永不触发」族口径；与 J165 1000 的
//   chance>0 守卫不同，按 410/K422 口径收）。
//
// ══ 审计槽（无随机精确断言口径，F9 782 首创 / K422 917 同式）══
// 1079 键段 = 1,010,790,000（10^9 + 1079×10^4）+ owner×16 + slot（新增前已跑
//   tools/check_storage_keys.py 全库无撞）：
//   slot 0  申报 P（{0}）           slot 1  申报异常码（{1}）
//   slot 2  申报回合数（{2}）       slot 3  申报先制量（{3}）
//   slot 4  闸门改写后实际概率（rewrite_anomaly_chance 纯查询，无票时==P）
//   slot 5  最近施加结果码（ApplyAnomalyResult int；-1 = 本次未施加——目标已倒/守卫）
//   slot 6  最近 landed（1/0）
//   slot 7  最近 NextRounds 先制窗起点回合（register_next_rounds_effect 返回值；0=未注册）
//   slot 8  landed 次数累计
//   slot 9  先制窗注册次数累计
//   slot 10 最近拒绝原因（0 无 / 1 目标已倒整条跳过）
// 1722 键段 = 1,017,220,000（10^9 + 1722×10^4）+ owner×16 + slot（同上无撞）：
//   slot 0  申报回合数（{0}）       slot 1  申报先制量（{1}）
//   slot 2  门观测 crit_happened（1=已暴击门闭 / 0=未暴击）
//   slot 3  NextRounds 先制窗起点回合（0=未注册——门闭/守卫/目标已倒）
//   slot 4  先制窗注册次数累计
//   slot 5  最近拒绝原因（0 无 / 1 已暴击门闭 / 2 目标已倒整条跳过）
#include "common.h"

#include <abnormal-system/abnormal-types.h>
#include <utils/sbs_rand.h>

namespace {

constexpr int k1079AuditBase = 1010790000;
constexpr int k1722AuditBase = 1017220000;

inline int key1079(int owner, int slot) { return k1079AuditBase + owner * 16 + slot; }
inline int key1722(int owner, int slot) { return k1722AuditBase + owner * 16 + slot; }

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

// 申报制"落地"三档（K422 k422_landed 同口径；匿名空间不跨 TU，就地重写）。
bool k430_landed(ApplyAnomalyResult r) {
    return r == ApplyAnomalyResult::SUCCESS
        || r == ApplyAnomalyResult::REPLACED_EXISTING
        || r == ApplyAnomalyResult::DURATION_EXTENDED;
}

// ── 1079/1722 共用先制 tick：MOVE_RIGHT 写"攻击技能"先制 +bonus ──
// 载荷 [owner, opp, bonus]（[0]=owner / [2]=bonus，1726/1549 载荷同形）。
// "攻击技能"限定 = selected_skill_is_attack 门（J41 1549 同款；模板字样强制）。
EffectResult k430_preempt_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int bonus = args.int_args[2];
    if (self < 0 || self > 1 || bonus <= 0) {
        return EffectResult::kOk;
    }
    if (!selected_skill_is_attack(ctx, self)) {
        return EffectResult::kOk;   // "攻击技能"限定（681 同款）
    }
    ctx->ws.preemptive_level[self] += bonus;
    return EffectResult::kOk;
}

}  // namespace

// ═══════════════════════════════════════════════════════════════════════════
// 1079 —— 命中后{0}%令对手{1}，未触发则下{2}回合自身攻击技能先制+{3}
// 行参布局 [owner, opp, P, 异常码, 回合, 先制]（DB 四参 {0}..{3}；skills.cpp 加 [0,1] 前缀）。
// ═══════════════════════════════════════════════════════════════════════════
EffectResult effect_1079_hit_prob_preempt_fallback(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1079, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 1079, args,
                     "1079 布局=[owner,opp,概率,异常码,回合,先制]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int chance = args.int_args[2];
    const int anomaly = args.int_args[3];
    const int rounds = args.int_args[4];
    const int bonus = args.int_args[5];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1079, args, "需要 owner/opp ∈ {0,1}");
    EFFECT_ARG_GUARD(chance >= 0 && chance <= 100, 1079, args, "需要 概率 ∈ [0,100]");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 1079, args, "需要 异常码 ∈ [0,43]");
    EFFECT_ARG_GUARD(rounds > 0, 1079, args, "需要 回合数 > 0");
    EFFECT_ARG_GUARD(bonus > 0, 1079, args, "需要 先制量 > 0");
    if (!g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }

    // 生存门（2295 口径）：目标已倒 → 整条跳过（不施异常、不注册先制窗）。
    if (ctx->getPet(opp).hp <= 0) {
        set_storage_int_local(ctx, key1079(owner, 5), -1);
        set_storage_int_local(ctx, key1079(owner, 6), 0);
        set_storage_int_local(ctx, key1079(owner, 7), 0);
        set_storage_int_local(ctx, key1079(owner, 10), 1);
        return EffectResult::kOk;
    }

    set_storage_int_local(ctx, key1079(owner, 0), chance);
    set_storage_int_local(ctx, key1079(owner, 1), anomaly);
    set_storage_int_local(ctx, key1079(owner, 2), rounds);
    set_storage_int_local(ctx, key1079(owner, 3), bonus);
    // 闸门改写后实际概率（纯查询、幂等——原语 [2.2] 掷骰前问的是同一个值）。
    const int final_pct = ctx->rewrite_anomaly_chance(opp, owner, chance,
                                                      ChanceSource::Skill);
    set_storage_int_local(ctx, key1079(owner, 4), final_pct);
    set_storage_int_local(ctx, key1079(owner, 10), 0);

    // 申报制掷点（原语内：0<final<100 恰耗一抽；<=0 / >=100 不耗）。
    const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
        ctx, opp, anomaly, random_anomaly_duration(), owner, chance, ChanceSource::Skill);
    set_storage_int_local(ctx, key1079(owner, 5), static_cast<int>(r));
    if (k430_landed(r)) {
        set_storage_int_local(ctx, key1079(owner, 6), 1);
        set_storage_int_local(ctx, key1079(owner, 7), 0);   // 互斥：触发侧先制窗清 0
        set_storage_int_local(ctx, key1079(owner, 8),
                              storage_int_local(ctx, key1079(owner, 8), 0) + 1);
        return EffectResult::kOk;
    }
    set_storage_int_local(ctx, key1079(owner, 6), 0);
    // 未触发兜底：下{2}回合自身攻击技能先制+{3}（NextRounds 窗，MOVE_RIGHT tick）。
    const int start = register_next_rounds_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, owner,
                                                  1079, rounds, {owner, opp, bonus},
                                                  &k430_preempt_tick);
    set_storage_int_local(ctx, key1079(owner, 7), start);
    set_storage_int_local(ctx, key1079(owner, 9),
                          storage_int_local(ctx, key1079(owner, 9), 0) + 1);
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// 1722 —— 未打出致命一击则自身下{0}回合攻击技能额外先制+{1}
// 行参布局 [owner, opp, 回合, 先制]（DB 两参 {0}{1}；skills.cpp 加 [0,1] 前缀）。
// ═══════════════════════════════════════════════════════════════════════════
EffectResult effect_1722_no_crit_preempt(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1722, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1722, args,
                     "1722 布局=[owner,opp,回合,先制]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int bonus = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1722, args, "需要 owner/opp ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0, 1722, args, "需要 回合数 > 0");
    EFFECT_ARG_GUARD(bonus > 0, 1722, args, "需要 先制量 > 0");

    // 生存门（2295 口径）：目标已倒 → 整条跳过。
    if (ctx->getPet(opp).hp <= 0) {
        set_storage_int_local(ctx, key1722(owner, 2), ctx->crit_happened[owner] ? 1 : 0);
        set_storage_int_local(ctx, key1722(owner, 3), 0);
        set_storage_int_local(ctx, key1722(owner, 5), 2);
        return EffectResult::kOk;
    }

    set_storage_int_local(ctx, key1722(owner, 0), rounds);
    set_storage_int_local(ctx, key1722(owner, 1), bonus);
    // crit_happened 在 query_usage ①.5（ON_SKILL_HIT）掷、SKILL_EFFECT 时已可见
    // （E 组 844/553 定口径）；属性技能不掷暴击 → 旗保持上一次清零态 = false = 门开。
    const int crit = ctx->crit_happened[owner] ? 1 : 0;
    set_storage_int_local(ctx, key1722(owner, 2), crit);
    set_storage_int_local(ctx, key1722(owner, 5), 0);
    if (crit) {
        set_storage_int_local(ctx, key1722(owner, 3), 0);   // 门闭：已暴击 → 无先制窗
        set_storage_int_local(ctx, key1722(owner, 5), 1);
        return EffectResult::kOk;
    }
    // 未打出致命一击 → 下{0}回合自身攻击技能先制+{1}（NextRounds 窗）。
    const int start = register_next_rounds_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, owner,
                                                  1722, rounds, {owner, opp, bonus},
                                                  &k430_preempt_tick);
    set_storage_int_local(ctx, key1722(owner, 3), start);
    set_storage_int_local(ctx, key1722(owner, 4),
                          storage_int_local(ctx, key1722(owner, 4), 0) + 1);
    return EffectResult::kOk;
}

SKILL_EFFECT(1079, effect_1079_hit_prob_preempt_fallback);
SKILL_EFFECT(1722, effect_1722_no_crit_preempt);
